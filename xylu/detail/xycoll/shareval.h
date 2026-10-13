#pragma once

/**
 * @file    xylu/shareval.h
 * @brief   共享对象
 * @date    2026-10-02
 */

#include "./__/tool.h"
#include "../xyconc/atomic.h"

namespace xylu::xycoll
{
    namespace __
    {
        // 共享块
        template <typename T>
        struct ShareData
        {
            T data;

            template <typename... Args, xyu::t_enable<xyu::t_can_init<T, Args...> && !xyu::t_is_aggregate<T>, bool> = false>
            ShareData(Args&&... args) noexcept(xyu::t_can_nothrow_init<T, Args...>)
            : data(xyu::forward<Args>(args)...) {}

            template <typename... Args, xyu::t_enable<xyu::t_can_init<T, Args...> && xyu::t_is_aggregate<T>, bool> = true>
            ShareData(Args&&... args) noexcept(xyu::t_can_nothrow_init<T, Args...>)
            : data{xyu::forward<Args>(args)...} {}
        };

        // 控制块
        struct ShareRef
        {
            using CT = xyu::t_cond<xyu::Atomic<xyu::size_t>::is_always_lock_free, xyu::size_t, xyu::uint>;
            static constexpr xyu::size_t HalfBits = xyu::t_number<xyu::size_t>::digits / 2;
            static constexpr CT Mask = (CT(1) << HalfBits) - 1;

            xyu::Atomic<CT> counts;     // high:视图数量 | low:持有者数量+1 (为1表示正在析构对象)

            ShareRef() noexcept : counts(2) {}

            // 增加持有者数量
            CT inc_holds() noexcept
            {
                auto tmp = counts.fetch_add(1, xyu::N_Atomic_Order::Relaxed);
                if constexpr (!XY_RELEASE) if (XY_UNLIKELY((tmp & Mask) == Mask)) out_limit_error(true);
                return tmp;
            }
            // 减少持有者数量
            CT dec_holds() noexcept
            {
                // 保证对数据的写入可见，析构函数基于最终状态
                return counts.fetch_sub(1);
            }

            // 增加视图数量
            CT inc_views() noexcept
            {
                auto tmp = counts.fetch_add(CT(1) << HalfBits, xyu::N_Atomic_Order::Relaxed);
                if constexpr (!XY_RELEASE) if (XY_UNLIKELY((tmp >> HalfBits) == Mask)) out_limit_error(false);
                return tmp;
            }
            // 减少视图数量
            CT dec_views() noexcept
            {
                // 保证与 holds 同步
                return counts.fetch_sub(CT(1) << HalfBits);
            }

            // 获取持有者数量
            CT share_count() const noexcept { return xyu::max(counts.load(xyu::N_Atomic_Order::Relaxed) & Mask, 1) - 1; }
            // 获取视图数量
            CT view_count() const noexcept { return counts.load(xyu::N_Atomic_Order::Relaxed) >> HalfBits; }

            [[noreturn]] XY_NOINLINE XY_COLD
            static void out_limit_error(bool hold) noexcept
            {
                xyloge3("maximum the same data {} count exceeded", hold ? "holders" : "views");
                xyu::abort();
            }
        };

        // 共享数据 - 组合
        template <typename T, bool combine, bool cross_thread>
        struct ShareValData
        {
        protected:
            struct CombineData : ShareData<T>, ShareRef
            {
                using ShareData<T>::ShareData;
            }* data;

            // 默认构造
            ShareValData() noexcept : data(nullptr) {}
            // 拷贝构造
            ShareValData(const ShareValData& other) noexcept : data(other.check_ptr()) {}
            // 移动构造
            ShareValData(ShareValData&& other) noexcept : data(other.check_ptr()) { other.data = nullptr; }
            // 拷贝赋值
            void operator=(const ShareValData& other) noexcept { data = other.check_ptr(); }
            // 交换
            void swap(ShareValData& other) noexcept { xyu::swap(data, other.data); }

            // 创建
            void do_create() noexcept { data = Common::alloc<CombineData, cross_thread>(); }
            // 释放
            void do_destroy() noexcept { Common::dealloc<cross_thread>(data); }

            // 释放数据
            void release_data() noexcept { ((ShareData<T>*)data)->~ShareData(); }
            // 释放控制块
            void release_ctrl() noexcept { do_destroy(); data = nullptr; }

            // 获取指针
            CombineData* check_ptr() const noexcept { return Checker::check_ptr(data, XY_CurPos("ShareValData::get")); }
            // 获取数据块
            ShareData<T>& get_data() const noexcept { return (CombineData&)*data; }
            // 获取控制块
            ShareRef& get_ctrl() const noexcept { return (CombineData&)*data; }
        };

        // 共享数据 - 分离
        template <typename T, bool cross_thread>
        struct ShareValData<T, false, cross_thread> : xyu::class_no_copy_move_t
        {
        protected:
            ShareRef* ctrl;
            ShareData<T>* data;

            // 默认构造
            ShareValData() noexcept : ctrl(nullptr), data(nullptr) {}
            // 拷贝构造
            ShareValData(const ShareValData& other) noexcept : ctrl(other.check_ptr()), data(other.data) {}
            // 移动构造
            ShareValData(ShareValData&& other) noexcept : ctrl(other.check_ptr()), data(other.data) { other.ctrl = other.data = nullptr; }
            // 拷贝赋值
            void operator=(const ShareValData& other) noexcept { ctrl = other.check_ptr(); data = other.data; }
            // 交换
            void swap(ShareValData& other) noexcept { xyu::swap(ctrl, other.ctrl); xyu::swap(data, other.data); }

            // 创建
            void do_create() noexcept
            {
                ctrl = Common::alloc<ShareRef, cross_thread>();
                data = Common::alloc<ShareData<T>, cross_thread>();
                ::new (ctrl) ShareRef{};
            }
            // 释放所有
            void destroy_data() noexcept { Common::dealloc<cross_thread>(data); }
            void destroy_ctrl() noexcept { Common::dealloc<cross_thread>(ctrl); }
            void do_destroy() noexcept { destroy_data(); destroy_ctrl(); }

            // 释放数据
            void release_data() noexcept { data->~ShareData(); destroy_data(); data = nullptr; }
            // 释放控制块
            void release_ctrl() noexcept { destroy_ctrl(); ctrl = nullptr; }

            // 获取指针
            ShareRef* check_ptr() const noexcept { return Checker::check_ptr(ctrl, XY_CurPos("ShareValData::get")); }
            // 获取数据块
            ShareData<T>& get_data() const noexcept { return (ShareData<T>&)*data; }
            // 获取控制块
            ShareRef& get_ctrl() const noexcept { return (ShareRef&)*ctrl; }
        };
    }

    /**
     * @brief 共享值
     * @tparam T 值类型
     * @tparam combine 控制块与数据块是否放在一起。
     *         - false: 分离，需要两次分配与释放，缓存不如合并，但数据块可以单独转移；
     *         - true: 合并，仅需一次分配释放，缓存性能更好，但数据块无法转移，无法单独回收。
     * @tparam cross_thread 是否允许跨线程内存管理。
     *         - false: 使用线程局部内存池，性能更好，但需要确保创建与释放均在同一线程进行；
     *         - true（默认）: 使用全局内存分配，对象可跨线程重分配和释放。
     *
     * @details
     *   存储方式：堆上分配 控制块与数据块，通过 持有者和视图的原子计数 实现共享
     *
     *   方法简述：
     *    - 通过对 T 的任意构造函数分配一个新共享值
     *    - 通过原子计数的修改，支持对象的拷贝移动，任意一个持有者存活时共享值始终有效
     *    - 支持 `share_count`,`view_count`,`wait`,`notify` 查看当前的 持有者和视图数量
     *    - 支持 `sync_acquire`,`sync_release` 进行共享值的同步
     *    - 支持 `take` 转移所有权，`xyu::special_v` 通过 `take`转移的 或 同种分配模式的堆指针 构造 (仅限 !combine)
     *
     *   注意事项：
     *    - 空指针：空构造，移动构造或 `take` 后访问空指针，或 `xyu::special_v`
     *             传入空指针 会在 !XY_RELEASE 下 abort
     *    - 值同步：析构函数具有 `acq_rel`，take具有 `acquire`，其他均需要手动调用
     *             `sync_acquire` 和 `sync_release` 或 外部同步手段保证同步
     *
     *   异常安全：
     *    - 赋值完全转发底层类型，与底层类型相同
     *
     *   @bar 共享值同步
     *   @code
     *   ShareVal<Data> sv{0};
     *
     *   // 线程A
     *   auto sv1 = sv;
     *   auto& ref = sv1.get();
     *   ref.value = 200;
     *   sv1.sync_release();
     *
     *   // 线程B
     *   auto sv2 = sv;
     *   auto& ref = sv2.get();
     *   sv2.sync_acquire();
     *   auto value = ref.value;
     *   @endcode
     */
    template <typename T, bool combine = !xyu::t_is_array<T>, bool cross_thread = true>
    class ShareVal : __::ShareValData<T, combine, cross_thread>
    {
        using Ref = __::ShareRef;
        using Base = __::ShareValData<T, combine, cross_thread>;
    public:
        /* 对象操作 */

        /// 析构
        ~ShareVal() noexcept
        {
            if (!this->data) return;
            auto tmp = this->get_ctrl().dec_holds();
            if ((tmp & Ref::Mask) != 2) return;     // 仍有其他持有者
            this->release_data();
            if (tmp >> Ref::HalfBits)               // 仍有其他视图
            {
                tmp = this->get_ctrl().dec_holds(); // 对象析构完毕，low 变为 0，内存可以安全释放
                if (tmp >> Ref::HalfBits) return;
            }
            this->release_ctrl();
        }

        /// 空构造
        ShareVal(xyu::native_t) noexcept {}
        /// 拷贝构造
        ShareVal(const ShareVal& other) noexcept : Base(other) { this->get_ctrl().inc_holds(); }
        /// 移动构造
        ShareVal(ShareVal&& other) noexcept :Base(xyu::move(other)) {}

        /// 拷贝赋值
        ShareVal& operator=(const ShareVal& other) noexcept
        {
            if (XY_UNLIKELY(this == &other)) return *this;
            this->~ShareVal();
            Base::operator=(other);
            this->get_ctrl().inc_holds();
            return *this;
        }
        /// 移动赋值
        ShareVal& operator=(ShareVal&& other) noexcept { return swap(other); }
        /// 交换
        ShareVal& swap(ShareVal& other) noexcept { Base::swap(other); return *this; }

        /// 是否有效
        explicit operator bool() const noexcept { return this->data; }
        /* 数据操作 */

        /// 任意构造
        template <typename... Args, typename = xyu::t_enable<xyu::t_can_init<T, Args...> && (sizeof...(Args) != 1 || !xyu::t_is_same_nocr<ShareVal, Args...>)>>
        ShareVal(Args&&... args) noexcept(xyu::t_can_nothrow_init<T, Args...>)
        {
            this->do_create();
            if constexpr (xyu::t_can_nothrow_init<T, Args...>)
                xyu::place_init(this->data, xyu::forward<Args>(args)...);
            else try { xyu::place_init(this->data, xyu::forward<Args>(args)...); }
                catch (...) { this->do_destroy(); throw; }
        }

        /// 任意赋值
        template <typename Arg, typename = xyu::t_enable<xyu::t_can_assign<T, Arg> && !xyu::t_is_same_nocr<Arg, ShareVal>>, typename = void>
        ShareVal& operator=(Arg&& arg) noexcept(xyu::t_can_nothrow_assign<T, Arg>)
        {
            get() = xyu::forward<Arg>(arg);
            return *this;
        }

        /// 获取对象 &
        T& get() & noexcept { this->check_ptr(); return this->get_data().data; }
        /// 获取对象 &&
        T&& get() && noexcept { return xyu::move(((ShareVal&)(*this))->get()); }
        /// 获取对象 const
        xyu::t_add_const<T>& get() const& noexcept { return ((ShareVal&)(*this))->get(); }

        /* 数据转移 */

        /**
         * @brief 转移对象构造
         * @param p 必须为 xyu::alloc 或 take() 获取的对象
         */
        template <bool Test = combine, typename = xyu::t_enable<!Test>>
        ShareVal(xyu::special_t, T* p) noexcept
        {
            this->data = p;
            this->check_ptr();
            this->ctrl = __::Common::alloc<__::ShareRef, cross_thread>();
            new (this->ctrl) __::ShareRef{};
        }

        /**
         * @brief 转移对象
         * @details 仅当存在一个持有者时可以成功转移，否则返回 nullptr
         */
        T* take() noexcept
        {
            if (XY_UNLIKELY(!this->data)) return nullptr;
            auto tmp = this->get_ctrl().counts.load(xyu::N_Atomic_Order::Relaxed);
            for (;;)
            {
                if ((tmp & Ref::Mask) != 2) return nullptr;
                if (this->get_ctrl().counts.compare_exchange_weak(tmp, tmp & ~Ref::Mask, xyu::N_Atomic_Order::Acquire))
                {
                    T* p = this->data;
                    this->data = nullptr;
                    if ((tmp >> Ref::HalfBits) == 0) this->release_ctrl();
                    return p;
                }
            }
        }

        /* 计数与同步 */

        /// 获取共享者数量
        auto share_count() const noexcept { this->check_ptr(); return this->get_ctrl().share_count(); }
        /// 获取视图数量
        auto view_count() const noexcept { this->check_ptr(); return this->get_ctrl().view_count(); }

        /// 等待计数变化，阻塞直到 Func(share_count, view_count) 返回 true
        template <typename Func, typename = xyu::t_enable<xyu::t_can_call<Func, Ref::CT, Ref::CT>>>
        void wait(Func&& func) noexcept(xyu::t_can_nothrow_call<Func, Ref::CT, Ref::CT>)
        {
            this->check_ptr();
            this->get_ctrl().counts.wait([&](auto tmp)
            {
                return xyu::forward<Func>(func)(tmp & Ref::Mask, tmp >> Ref::HalfBits);
            });
        }
        /// 计数变化后唤醒等待线程
        void notify_one() noexcept { this->get_ctrl().counts.notify_one(); }
        void notify_all() noexcept { this->get_ctrl().counts.notify_all(); }

        /// 同步其他线程的写入让本线程可见
        void sync_acquire() noexcept { this->check_ptr(); this->get_ctrl().counts.load(xyu::N_Atomic_Order::Acquire); }
        /// 同步本线程的写入让其他线程可见
        void sync_release() noexcept { this->check_ptr(); this->get_ctrl().counts.fetch_add(0, xyu::N_Atomic_Order::Release); }
    };

    /**
     * @brief 共享值视图
     * @tparam T 值类型
     * @tparam combine 控制块与数据块是否放在一起。
     *         - false: 分离，需要两次分配与释放，缓存不如合并，但数据块可以单独转移；
     *         - true: 合并，仅需一次分配释放，缓存性能更好，但数据块无法转移，无法单独回收。
     * @tparam cross_thread 是否允许跨线程内存管理。
     *         - false: 使用线程局部内存池，性能更好，但需要确保创建与释放均在同一线程进行；
     *         - true（默认）: 使用全局内存分配，对象可跨线程重分配和释放。
     *
     * @details
     *   存储方式：堆上分配 控制块与数据块，通过 持有者和视图的原子计数 实现共享
     *
     *   方法简述：
     *    - 通过对 共享值 或 其他视图 构造新的视图
     *    - 通过原子计数的修改，支持对象的拷贝移动，任意视图存活则控制块一定有效
     *    - 支持 `share_count`,`view_count`,`wait`,`notify` 查看当前的 持有者和视图数量
     *    - 支持 `sync_acquire`,`sync_release` 进行共享值的同步
     *    - 支持 `hold`,`apply` 在共享值有效时执行操作
     *
     *   注意事项：
     *    - 空指针：空构造，移动构造后访问空指针会在 !XY_RELEASE 下 abort
     *    - 弱引用：视图存活不能保证共享值有效
     *    - 值同步：析构函数具有 `acq_rel`，其他均需要手动调用 `sync_acquire` 和 `sync_release` 或 外部同步手段保证同步
     *
     *   @bar 循环引用
     *   @code
     *   struct Son;
     *   struct Parent { ShareVal<Son> son; }
     *   struct Son { ShareValView<Parent> pa; }
     *
     *   ShareVal<Parent> pa(native_v);
     *   ShareVal<Son> son(pa);
     *   pa.son = son;
     *   // parent-(1 holder, 1 view) son-(2 holders, 0 view)
     *
     *   // son析构 parent-(1 holder, 0 view) son-(1 holders, 0 view)
     *   // pa析构  parent-(0 holder, 0 view) son-(0 holders, 0 view)
     *   @endcode
     */
    template <typename T, bool combine = !xyu::t_is_array<T>, bool cross_thread = true>
    class ShareValView : __::ShareValData<T, combine, cross_thread>
    {
        using Ref = __::ShareRef;
        using Base = __::ShareValData<T, combine, cross_thread>;
        using ShareVal = ShareVal<T, combine, cross_thread>;
    public:
        /* 对象操作 */

        /// 析构
        ~ShareValView() noexcept
        {
            if (!this->data) return;
            auto tmp = this->get_ctrl().dec_views();
            if (tmp != Ref::Mask + 1) return;
            this->release_ctrl();
        }

        /// 空构造
        ShareValView(xyu::native_t) noexcept {}
        /// 从 ShareVal 构造
        ShareValView(const ShareVal& val) noexcept : ShareValView((ShareValView&)(Base&)val) {}

        /// 拷贝构造
        ShareValView(const ShareValView& other) noexcept : Base(other) { this->get_ctrl().inc_views(); }
        /// 移动构造
        ShareValView(ShareValView&& other) noexcept : Base(xyu::move(other)) {}

        /// 拷贝赋值
        ShareValView& operator=(const ShareValView& other) noexcept
        {
            if (XY_UNLIKELY(this == &other)) return *this;
            this->~ShareValView();
            Base::operator=(other);
            this->get_ctrl().inc_views();
            return *this;
        }
        /// 移动赋值
        ShareValView& operator=(ShareValView&& other) noexcept { return swap(other); }
        /// 交换
        ShareValView& swap(ShareValView& other) noexcept { Base::swap(other); return *this; }

        /// 是否有效
        explicit operator bool() const noexcept { return this->data; }

        /* 数据操作 */

        /// 获取对象 & (不检查是否存活)
        T& get() & noexcept { this->check_ptr(); return this->get_data().data; }
        /// 获取对象 && (不检查是否存活)
        T&& get() && noexcept { return xyu::move(((ShareValView&)(*this))->get()); }
        /// 获取对象 const (不检查是否存活)
        xyu::t_add_const<T>& get() const& noexcept { return ((ShareValView&)(*this))->get(); }

        /* 计数与同步 */

        /// 获取共享者数量
        auto share_count() const noexcept { this->check_ptr(); return this->get_ctrl().share_count(); }
        /// 获取视图数量
        auto view_count() const noexcept { this->check_ptr(); return this->get_ctrl().view_count(); }

        /// 等待计数变化，阻塞直到 Func(share_count, view_count) 返回 true
        template <typename Func, typename = xyu::t_enable<xyu::t_can_call<Func, Ref::CT, Ref::CT>>>
        void wait(Func&& func) noexcept(xyu::t_can_nothrow_call<Func, Ref::CT, Ref::CT>)
        {
            this->check_ptr();
            this->get_ctrl().counts.wait([&](auto tmp)
            {
                return xyu::forward<Func>(func)(tmp & Ref::Mask, tmp >> Ref::HalfBits);
            });
        }
        /// 计数变化后唤醒等待线程
        void notify_one() noexcept { this->get_ctrl().counts.notify_one(); }
        void notify_all() noexcept { this->get_ctrl().counts.notify_all(); }

        /// 同步其他线程的写入让本线程可见
        void sync_acquire() noexcept { this->check_ptr(); this->get_ctrl().counts.load(xyu::N_Atomic_Order::Acquire); }
        /// 同步本线程的写入让其他线程可见
        void sync_release() noexcept { this->check_ptr(); this->get_ctrl().counts.fetch_add(0, xyu::N_Atomic_Order::Release); }

        /* 存活操作 */

        /// 若共享值存活则返回一个新共享值持有对象，否则返回空对象
        ShareVal hold() noexcept
        {
            ShareVal ret{xyu::native_v};
            apply([&](ShareVal& tmp) { ret.swap(tmp); });
            return ret;
        }

        /// 若共享值存活执行操作 func() 并返回 true
        template <typename Func, xyu::t_enable<xyu::t_can_call<Func>, int> = 1>
        bool apply(Func&& func) noexcept(xyu::t_can_nothrow_call<Func>)
        { return apply([&](ShareVal&) noexcept(xyu::t_can_nothrow_call<Func>) { xyu::forward<Func>(func)(); }); }

        /// 若共享值存活执行操作 func(T&) 并返回 true
        template <typename Func, xyu::t_enable<xyu::t_can_call<Func, T&>, int> = 2>
        bool apply(Func&& func) noexcept(xyu::t_can_nothrow_call<Func, T&>)
        {
            return apply([&]() noexcept(xyu::t_can_nothrow_call<Func, T&>)
            {
                sync_acquire();
                xyu::forward<Func>(func)(get());
            });
        }

    private:
        /// 若共享值存活执行操作 func(ShareVal&) 并返回 true
        template <typename Func, xyu::t_enable<xyu::t_can_call<Func, ShareVal&>, int> = 0>
        bool apply(Func&& func) noexcept(xyu::t_can_nothrow_call<Func, ShareVal&>)
        {
            auto tmp = this->data->counts.load(xyu::N_Atomic_Order::Relaxed);
            for (;;)
            {
                if ((tmp & Ref::Mask) < 2) return false;
                if constexpr (!XY_RELEASE) if (XY_UNLIKELY((tmp & Ref::Mask) == Ref::Mask)) return false;
                if (this->data->counts.compare_exchange_weak(tmp, tmp + 1, xyu::N_Atomic_Order::Relaxed))
                {
                    // 利用 ShareVal 析构归还计数
                    ShareVal sv{xyu::native_v};
                    ((Base&)sv).data = this->data;
                    if constexpr (!combine) ((Base&)sv).ctrl = this->ctrl;
                    xyu::forward<Func>(func)(sv);
                    return true;
                }
            }
        }
    };

}
