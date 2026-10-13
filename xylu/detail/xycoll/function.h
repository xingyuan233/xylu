#pragma once

/**
 * @file    xycoll/function.h
 * @brief   可调用对象包装器
 * @date    2026-06-15
 */

#include "./__/tool.h"

namespace xylu::xycoll
{
    namespace __
    {
        /// 可调用对象
        class XY_MAY_ALIAS Functor
        {
            union Data
            {
                struct Class;

                void* p;
                void(*fp)();
                void(Class::*mp)();
            }
            data;

        public:
            // 判断能否直接存储
            template <typename T>
            static constexpr bool can_direct_store() noexcept
            {
                return (sizeof(T) <= sizeof(Data) && alignof(Data) % alignof(T) == 0);
            }

            // 获取对象
            template <typename T> T& get() noexcept { return *reinterpret_cast<T*>(&data); }
            // 获取对象 (const)
            template <typename T> const T& get() const noexcept { return *reinterpret_cast<const T*>(&data); }
        };

        /// 可调用对象管理器
        struct FuncManager
        {
            // 调用
            template <typename Func, typename Ret, typename... Args>
            static Ret call(const Functor& functor, bool is_const, Args... args) noexcept(xyu::t_can_nothrow_call<Func, Args...>)
            {
                if (!is_const) return get<Func>(const_cast<Functor&>(functor))(xyu::forward<Args>(args)...);
                if constexpr(xyu::t_can_call<const Func&, Args...>) return get<Func>(functor)(xyu::forward<Args>(args)...);
                else abort_unsupport_call(__LINE__);
            }

            // 创建
            template <typename Func, bool cross_thread, typename Fn>
            static void create(Functor& functor, Fn&& fn) noexcept(xyu::t_can_nothrow_init<Func, Fn>)
            {
                if constexpr (Functor::can_direct_store<Func>())
                    ::new (&functor.get<Func>()) Func{xyu::forward<Fn>(fn)};
                else
                {
                    Func*& fp = functor.get<Func*>();
                    fp = Common::alloc<Func, cross_thread>();
                    if constexpr (xyu::t_can_nothrow_init<Func, Fn>)
                        ::new (fp) Func{xyu::forward<Fn>(fn)};
                    else try { ::new (fp) Func{xyu::forward<Fn>(fn)}; }
                        catch (...) { Common::dealloc<cross_thread>(fp); throw; }
                }
            }

            // 操作类型
            enum CommandType { Destroy, Copy, Move };
            // 管理
            template <typename Func, bool cross_thread>
            static void manage(Functor& dst, const Functor& src, int command)
            {
                switch (command)
                {
                    case Destroy: return destroy<Func, cross_thread>(dst);
                    case Copy: return copy<Func, cross_thread>(dst, src);
                    case Move: return move<Func, cross_thread>(dst, const_cast<Functor&>(src));
                    default:;
                }
            }

        private:
            // 获取
            template <typename Func>
            static Func& get(Functor& functor) noexcept
            {
                if constexpr (Functor::can_direct_store<Func>()) return functor.get<Func>();
                else return *functor.get<Func*>();
            }
            // 获取 (const)
            template <typename Func>
            static const Func& get(const Functor& functor) noexcept
            {
                if constexpr (Functor::can_direct_store<Func>()) return functor.get<Func>();
                else return *functor.get<const Func*>();
            }

            // 复制
            template <typename Func, bool cross_thread>
            static void copy(Functor& dst, const Functor& src) noexcept(xyu::t_can_nothrow_cpconstr<Func>)
            {
                create<Func, cross_thread>(dst, get<Func>(src));
            }

            // 移动
            template <typename Func, bool cross_thread>
            static void move(Functor& dst, Functor& src) noexcept
            {
                create<Func, cross_thread>(dst, xyu::move(get<Func>(src)));
            }

            // 释放
            template <typename Func, bool cross_thread>
            static void destroy(Functor& functor) noexcept
            {
                if constexpr (Functor::can_direct_store<Func>()) functor.get<Func>().~Func();
                else
                {
                    Func* fp = functor.get<Func*>();
                    fp->~Func();
                    Common::dealloc<cross_thread>(fp);
                }
            }

            [[noreturn]] XY_NOINLINE XY_COLD
            static void abort_unsupport_call(int line) noexcept
            {
                xyloge2("functor can't support const-call", line, "Function::call");
                xyu::abort();
            }
        };
    }

    /**
     * @brief 可调用对象包装器
     * @tparam FuncDefine 函数签名，形如 `Ret(Args...)`
     * @tparam cross_thread 是否允许跨线程内存管理。
     *         - false（默认）: 使用线程局部内存池，性能更好；
     *         - true : 使用全局内存分配，对象可跨线程重分配和释放。
     *
     * @details
     *   存储方式：
     *   - 小对象优化：若可调用对象大小不超过内联存储容量且对齐兼容，则直接存储在 `Functor` 联合体内；
     *               否则在堆上分配（依据 `cross_thread` 选择内存池）
     *   - 通过函数指针管理生命周期（复制/移动/销毁）和调用，实现类型擦除
     *
     *   方法简述：
     *   - 支持 `op bool` 判断是否已经绑定，`release` 释放对象
     *   - 支持任意符合 `FuncDefine` 签名的可调用对象进行 构造 或 赋值
     *   - 通过 `operator()` 进行调用，允许 非const 与 const 版本
     *
     *   注意事项：
     *   - 空指针：若传入函数指针为空，在 !XY_RELEASE 下检查并触发 abort
     *   - 空调用：未绑定函数时调用 `operator()` !XY_RELEASE 下检查并触发 abort
     *   - 非法调用：若被包装对象不支持 const 调用却进行 const 调用会记录错误并 abort
     *
     *   异常安全：
     *   - 复制赋值：先释放目标再复制源；若复制抛出异常，对象变为空
     *   - 调用：异常保证完全取决于原 func
     */
    template <typename FuncDefine, bool cross_thread = false>
    class Function;

    template <typename Ret, typename... Args, bool cross_thread>
    class Function<Ret(Args...), cross_thread>
    {
        __::Functor functor;                                            // 可调用对象
        Ret(*caller)(const __::Functor&, bool, Args...);                // 调用指针
        void(*manager)(__::Functor& dst, const __::Functor& src, int);  // 管理指针

    public:
        /* 构造赋值 */

        /// 默认构造
        Function() noexcept = default;

        /// 析构
        ~Function() noexcept { release(); }

        /// 空指针构造
        Function(xyu::nullptr_t) noexcept : Function() {}
        /// 空指针赋值
        Function& operator=(xyu::nullptr_t) noexcept { release(); return *this; }

        /**
         * @brief 函数构造
         * @param func 满足 `Ret(Args...)` 的函数，不能为 nullptr
         */
        template <typename Func>
        Function(Func&& func) noexcept(xyu::t_can_nothrow_init<xyu::t_decay<Func>, Func>) : Function()
        {
            static_assert(xyu::t_can_call_to<Ret, Func, Args...>);
            static_assert(xyu::t_can_nothrow_destruct<Func> && xyu::t_can_nothrow_mvconstr<Func>);
            __::Checker::check_ptr(func, XY_CurPos("Function::Function"));
            __::FuncManager::create<xyu::t_decay<Func>, cross_thread>(functor, xyu::forward<Func>(func));
            manager = __::FuncManager::manage<xyu::t_decay<Func>, cross_thread>;
            caller = __::FuncManager::call<xyu::t_decay<Func>, Ret, Args...>;
        }
        /**
         * @brief 函数赋值
         * @param func 满足 `Ret(Args...)` 的函数，不能为 nullptr
         */
        template <typename Func>
        Function& operator=(Func&& func) noexcept(xyu::t_can_nothrow_init<xyu::t_decay<Func>, Func>)
        {
            static_assert(xyu::t_can_call_to<Ret, Func, Args...>);
            static_assert(xyu::t_can_nothrow_destruct<Func> && xyu::t_can_nothrow_mvconstr<Func>);
            __::Checker::check_ptr(func, XY_CurPos("Function::="));
            release();
            __::FuncManager::create<xyu::t_decay<Func>, cross_thread>(functor, xyu::forward<Func>(func));
            manager = __::FuncManager::manage<xyu::t_decay<Func>, cross_thread>;
            caller = __::FuncManager::call<xyu::t_decay<Func>, Ret, Args...>;
            return *this;
        }

        /// 复制构造
        Function(const Function& other) : Function()
        {
            if (XY_UNLIKELY(!other.manager)) return;
            other.manager(functor, other.functor, __::FuncManager::Copy);
            manager = other.manager;
            caller = other.caller;
        }
        /// 复制赋值
        Function& operator=(const Function& other)
        {
            if (XY_UNLIKELY(this == &other)) return *this;
            release();
            if (XY_UNLIKELY(!other.manager)) return *this;
            other.manager(functor, other.functor, __::FuncManager::Copy);
            manager = other.manager;
            caller = other.caller;
            return *this;
        }

        /* 移动交换 */

        /// 移动构造
        Function(Function&& other) noexcept : caller(other.caller), manager(other.manager)
        {
            if (XY_UNLIKELY(!manager)) return;
            manager(functor, other.functor, __::FuncManager::Move);
        }
        /// 移动赋值
        Function& operator=(Function&& other) noexcept
        {
            if (XY_UNLIKELY(this == &other)) return *this;
            release();
            manager = other.manager;
            if (XY_UNLIKELY(!manager)) return *this;
            caller = other.caller;
            manager(functor, other.functor, __::FuncManager::Move);
            return *this;
        }

        /// 交换
        Function& swap(Function& other) noexcept
        {
            xyu::swap(functor, other.functor);
            xyu::swap(caller, other.caller);
            xyu::swap(manager, other.manager);
            return *this;
        }

        /* 对象 */

        /// 判断非空
        explicit operator bool() const noexcept { return manager; }

        /// 释放
        void release() noexcept
        {
            if (manager)
            {
                manager(functor, functor, __::FuncManager::Destroy);
                manager = nullptr;
            }
        }

        /**
         * @brief 函数调用
         * @param args 传递的参数，必须能够隐式转换为 Args
         * @attention 未绑定函数会崩溃
         */
        template <typename... Vargs>
        Ret operator()(Vargs&&... args)
        {
            static_assert(sizeof...(Args) == sizeof...(Vargs));
            static_assert((... && xyu::t_can_icast<Vargs, Args>));
            __::Checker::check_ptr(manager, XY_CurPos("Function::call"));
            return caller(functor, false, xyu::forward<Vargs>(args)...);
        }
        /**
         * @brief 函数调用 (const)
         * @param args 传递的参数，必须能够隐式转换为 Args
         * @attention 未绑定函数会崩溃
         */
        template <typename... Vargs>
        Ret operator()(Vargs&&... args) const
        {
            static_assert(sizeof...(Args) == sizeof...(Vargs));
            static_assert((... && xyu::t_can_icast<Vargs, Args>));
            __::Checker::check_ptr(manager, XY_CurPos("Function::call"));
            return caller(functor, true, xyu::forward<Vargs>(args)...);
        }
    };

    // 推导指引
    template <typename Func, typename... Args, typename = xyu::t_enable<xyu::t_can_call<Func, Args...>>>
    Function(Func&&, Args&&...) -> Function<xyu::t_call<Func, Args...>(xyu::t_decay<Args>...)>;
}
