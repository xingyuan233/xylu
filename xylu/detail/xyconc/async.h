#pragma once

/**
 * @file    async.h
 * @brief   异步调用
 * @date    2026-08-04
 */

#include "./atomic.h"
#include "../xycoll/tuple.h"
#include "../xycoll/refval.h"

namespace xylu::xyconc
{
    namespace __
    {
#if defined _WIN32
        using async_func_t = xyu::uint32(__stdcall*)(void*);
        using async_func_r = xyu::uint32;
#else
        using async_func_t = void*(*)(void*);
        using async_func_r = void*;
#endif
        void async_create(void*& h, async_func_t func, void* param);
        void async_join(void* h) noexcept;
        void async_detach(void* h) noexcept;
    }

    /**
     * @brief 轻量级异步任务封装，用于
     * @tparam T  任务的返回值类型 (不需要则为void)
     * @details
     *   - 在独立线程中执行可调用对象，并支持获取返回值或异常
     *   - 析构或新建任务时若旧任务仍在运行，则自动分离线程
     */
    template <typename T>
    struct Async : xyu::class_no_copy_t
    {
        /// 异步状态
        enum _Status { Uninit, Running, Failed, Finished, };

    private:
        struct alignas(xyu::K_Cache_Line_Size) Area
        {
            Atomic<int> s = Uninit;     // 线程状态
            void* h = nullptr;          // 线程句柄

            union Ret
            {
                // 返回值存储
                using V = xyu::t_cond<xyu::t_is_void<T>, int[0],
                          xyu::t_cond<xyu::t_is_lrefer<T>, xyu::RefVal<xyu::t_rem_lrefer<T>>, xyu::TagVal<T>>>;
                V v;

                // 异常
                xyu::ErrorPtr e;

                // 创建返回值
                template <typename U, typename Test = T, typename = xyu::t_enable<!xyu::t_is_void<Test>>>
                void create(U&& u) noexcept(xyu::t_can_nothrow_init<T, U>) { ::new (&v) V{xyu::forward<U>(u)}; }
                // 创建异常对象
                void create() noexcept { e = xyu::ErrorPtr::current(); }
                // 析构
                void release(int state) noexcept
                {
                    if (state == Failed) e.~ErrorPtr();
                    else if (state == Finished) xyu::object_destroy(&v);
                }

                Ret() noexcept {}
                ~Ret() noexcept {}
            } r;
        };

        Area* area;
        bool joined;

    public:
        /* 构造析构 */

        /// 默认构造
        Async() noexcept : area(xyu::alloc<Area>(xyu::native_v, 1)), joined(true) { ::new (area) Area{}; }

        /**
          * @brief 析构函数
          * @details 如果任务未完成则分离线程
          */
        ~Async() noexcept
        {
            auto& s = area->s;
            if (!joined) __::async_detach(area->h);
            if (int old = s.exchange(Uninit); old != Running)
            {
                area->r.release(old);
                xyu::dealloc(xyu::native_v, area);
            }
        }

        /// 构造并创建任务
        template <typename Func, typename... Args>
        Async(Func&& func, Args&&... args) noexcept(noexcept(create(xyu::forward<Func>(func), xyu::forward<Args>(args)...)))
        : Async() { create(xyu::forward<Func>(func), xyu::forward<Args>(args)...); }

        /* 移动 */

        /// 移动构造
        Async(Async&& other) noexcept : area{other.area}, joined{other.joined}
        {
            other.area = xyu::alloc<Area>(xyu::native_v, 1);
            other.joined = true;
        }

        /**
         * @brief 移动赋值运
         * @details 如果旧任务未完成则分离线程
         */
        Async& operator=(Async&& other) noexcept
        {
            if (XY_UNLIKELY(this == &other)) return *this;
            Async{xyu::move(other)}.swap(*this);
            return *this;
        }

        /// 交换
        Async& swap(Async& other) noexcept
        {
            xyu::swap(area, other.area);
            xyu::swap(joined, other.joined);
            return *this;
        }

        /* 任务创建 */

        /**
         * @brief 启动一个异步任务
         * @param func 要在新线程中执行的函数，禁止为 nullptr
         * @param args 传递给任务的参数
         * @details 如果旧任务未完成则分离旧线程
         */
        template <typename Func, typename... Args>
        void create(Func&& func, Args&&... args) noexcept(xyu::t_can_nothrow_init<xyu::t_decay<Func>, Func> && (... && xyu::t_can_nothrow_init<xyu::t_decay<Args>, Args>))
        {
            static_assert(xyu::t_can_call_to<T, Func, xyu::t_decay<Args>...>);
            if constexpr (!XY_RELEASE && xyu::t_is_pointer<Func>)
            {
                if (XY_UNLIKELY(!func))
                {
                    xyloge3("can't start task with null function");
                    xyu::abort();
                }
            }

            // 分离旧线程
            auto& s = area->s;
            if (!joined) __::async_detach(area->h), joined = true;
            if (int old = s.exchange(Uninit); old == Running)
            {
                area = xyu::alloc<Area>(xyu::native_v, 1);
                ::new (area) Area{};
            }
            else area->r.release(old);

            // 分配参数
            struct Param
            {
                Area* area;
                xyu::t_decay<Func> fun;
                xyu::Tuple<xyu::t_decay<Args>...> args;
            };
            Param* param = xyu::alloc<Param>(xyu::native_v, 1);
            ::new (param) Param{area, xyu::forward<Func>(func), xyu::forward<Args>(args)...};

            // 任务
            __::async_func_t task = [](void* p) -> __::async_func_r
            {
                Param& param = *(Param*)p;
                Atomic<int>& s = param.area->s;
                if constexpr (xyu::t_can_nothrow_call_to<T, Func, xyu::t_decay<Args>...>)
                {
                    if constexpr (xyu::t_is_void<T>) param.args.apply(param.fun);
                    else param.area->r.create(param.args.apply(param.fun));
                    if (s.exchange(Finished) == Uninit)
                    {
                        if constexpr (!xyu::t_is_void<T>) param.area->r.release(Finished);
                        xyu::dealloc(xyu::native_v, param.area);
                    }
                }
                else
                {
                    try
                    {
                        if constexpr (xyu::t_is_void<T>) param.args.apply(param.fun);
                        else param.area->r.create(param.args.apply(param.fun));
                        if (s.exchange(Finished) == Uninit)
                        {
                            if constexpr (!xyu::t_is_void<T>) param.area->r.release(Finished);
                            xyu::dealloc(xyu::native_v, param.area);
                        }
                    }
                    catch (...)
                    {
                        param.area->r.create();
                        if (s.exchange(Failed) == Uninit)
                        {
                            param.area->r.release(Failed);
                            xyu::dealloc(xyu::native_v, param.area);
                        }
                    }
                }
                param.~Param();
                xyu::dealloc(xyu::native_v, &param);
                return 0;
            };

            // 创建线程
            try
            {
                area->s = Running;
                __::async_create(area->h, task, param);
                joined = false;
            }
            catch (...)
            {
                area->s = Uninit;
                joined = true;
                param->~Param();
                xyu::dealloc(xyu::native_v, param);
            }
        }

        /* 任务管理 */

        /// 获取任务状态
        _Status status() const noexcept { return area->s; }

        /// 阻塞等待任务完成
        void wait() noexcept
        {
            if (joined) return;
            if (area->s != Running) return;
            joined = true;
            __::async_join(area->h);
        }

        /**
         * @brief 阻塞等待任务完成，并获取返回值
         * @details 如果任务抛出异常则原样抛出
         */
        T get()
        {
            wait();
            if (area->s == Failed) area->r.e.rethrow();
            if constexpr (!xyu::t_is_void<T>) return area->r.v.get();
        }

        /**
         * @brief 阻塞等待任务完成，并获取返回值
         * @details 如果任务抛出异常则存储到指针中并返回默认构造值
         */
        T get(xyu::ErrorPtr* ep) noexcept(xyu::t_can_cpconstr<T> && xyu::t_can_dfconstr<T>)
        {
            wait();
            bool failed = area->s == Failed;
            if (failed && ep) *ep = area->r.e;
            if constexpr (!xyu::t_is_void<T>) return failed ? T{} : area->r.v.get();
        }

        /**
         * @brief 阻塞等待任务完成，并获取返回值 (一次性移动)
         * @details 如果任务抛出异常则原样抛出
         */
        T take()
        {
            wait();
            if (area->s == Failed) area->r.e.rethrow();
            if constexpr (!xyu::t_is_void<T>) return xyu::move(area->r.v.get());
        }

        /**
         * @brief 阻塞等待任务完成，并获取返回值 (一次性移动)
         * @details 如果任务抛出异常则存储到指针中并返回默认构造值
         */
        T take(xyu::ErrorPtr* ep) noexcept(xyu::t_can_cpconstr<T> && xyu::t_can_dfconstr<T>)
        {
            wait();
            bool failed = area->s == Failed;
            if (failed && ep) *ep = xyu::move(area->r.e);
            if constexpr (!xyu::t_is_void<T>) return failed ? T{} : xyu::move(area->r.v.get());
        }
    };

    // 推导指引
    template <typename Func, typename... Args, typename = xyu::t_enable<xyu::t_can_call<Func, Args...>>>
    Async(Func&&, Args&&...) -> Async<xyu::t_call<Func, Args...>>;
}
