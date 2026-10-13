#pragma once

/**
 * @file    xyconc/atomic.h
 * @brief   原子变量
 * @date    2026-03-07
 */

#include "./_.h"
#include "../xycore/config.h"
#include "../xytraits/func.h"
#include "../xymath/calc.h"

/// 原子变量
namespace xylu::xyconc
{
    /**
     * @brief 原子变量类
     * @tparam T 被封装的类型。必须是可平凡复制的，并对整型提供算术及位运算。
     * @details
     *   `Atomic<T>` 保证了对共享变量 T 的所有操作都是原子的。
     *   它封装了底层的原子指令，并提供了与普通变量类似的接口（如赋值、隐式转换、算术运算符等）。
     *
     *   所有原子操作都接受一个可选的内存序参数，用于控制编译器和 CPU 对内存操作的重排行为，以实现更精细的同步。
     *   若不指定，则使用默认的 `ACQUIRE`(读取) , `RELEASE`(写入) 及 `ACQ_REL`(读写) 内存序。
     *
     *   单线程下所有操作退化为普通操作。
     *
     * @attention 使用时需注意伪共享的问题，可搭配 `xyu::K_CACHE_LINE_SIZE` 使用
     */
    template <typename T>
    class Atomic : xyu::class_no_copy_move_t
    {
        static_assert(!xyu::t_is_empty<T>, "Atomic<T> can't be an empty type");
        static_assert(xyu::t_is_trivial_copyable<T>, "Atomic<T> must be a trivially copyable type");

        using Order = xyu::N_Atomic_Order;
        static constexpr auto align =
                xyu::max((sizeof(T) & (sizeof(T) - 1)) || sizeof(T) > 16 ? 0 : sizeof(T), alignof(T));

    private:
        // 存储
        alignas(align) T v;

    public:
        /* 实例 */

        /// 默认构造
        Atomic() = default;

        /// 值构造 (聚合类型)
        template <typename... Args, typename Test = T, xyu::t_enable<xyu::t_is_aggregate<Test>, bool> = true>
        Atomic(Args&&... args) noexcept(xyu::t_can_nothrow_listinit<T, Args...>) : v{xyu::forward<Args>(args)...} {}
        /// 值构造 (非聚合类型)
        template <typename... Args, typename Test = T, xyu::t_enable<!xyu::t_is_aggregate<Test>, bool> = false>
        Atomic(Args&&... args) noexcept(xyu::t_can_nothrow_construct<T, Args...>) : v(xyu::forward<Args>(args)...) {}

        /// 值赋值
        template <typename U>
        Atomic& operator=(U&& value) noexcept(xyu::t_can_nothrow_icast<T,U>) { store(value); return *this; }

        /// 隐式转换
        operator T() const noexcept { return load(); }

#if XY_THREAD
        /* 锁判断 */

        /**
         * @brief 是否为无锁操作
         * @details 编译期判断，根据平台、编译选项、类型大小等因素来决定是否为无锁操作
         */
        static constexpr bool is_always_lock_free = __atomic_always_lock_free(sizeof(T), nullptr);

        /**
         * @brief 是否为无锁操作
         * @details 运行期判断，根据平台、类型大小、指针是否内存对齐等因素来决定是否为无锁操作
         */
        bool is_lock_free() const noexcept { return __atomic_is_lock_free(sizeof(T), &v); }

        /* 原子操作 */

        /// 读取 (默认为 ACQUIRE)
        T load(Order order = Order::Acquire) const noexcept
        { return __atomic_load_n(&v, (int)order); }

        /// 写入 (默认为 RELEASE)
        void store(T value, Order order = Order::Release) noexcept
        { __atomic_store(&v, &value, (int)order); }

        /**
         * @brief 交换，读取旧值返回并写入新值
         * @param value 新的值
         * @param order 内存序 (默认为 ACQ_REL)
         */
        T exchange(T value, Order order = Order::Acq_Rel) noexcept
        { return __atomic_exchange_n(&v, value, (int)order); }

        /**
         * @brief 比较，如果当前值等于预期值，则写入新值并返回true，否则返回false
         * @param expected 预期值
         * @param value 新的值
         * @param order 内存序 (默认为 ACQ_REL)
         * @details 弱比较，可能出现虚假失败，失败时修改 expected 为当前值 (如果传递引用)
         */
        template <typename U, typename = xyu::t_enable<xyu::t_is_same_nocr<U,T>>>
        bool compare_exchange_weak(U&& expected, T value, Order order = Order::Acq_Rel) noexcept
        { return __atomic_compare_exchange_n(&v, &expected, value, true, (int)order, get_failed_order(order)); }

        /**
        * @brief 比较，如果当前值等于预期值，则写入新值并返回true，否则返回false
        * @param expected 预期值
        * @param value 新的值
        * @param order 内存序 (默认为 ACQ_REL)
        * @details 强比较，仅值不匹配时返回 false，失败时修改 expected 为当前值 (如果传递引用)
        */
        template <typename U, typename = xyu::t_enable<xyu::t_is_same_nocr<U,T>>>
        bool compare_exchange_strong(U&& expected, T value, Order order = Order::Acq_Rel) noexcept
        { return __atomic_compare_exchange_n(&v, &expected, value, false, (int)order, get_failed_order(order)); }

        /**
         * @brief 读取旧值返回并写入增加后的新值
         * @param value 增加的值
         * @param order 内存序 (默认为 ACQ_REL)
         */
        template <typename Test = T, typename = xyu::t_enable<xyu::t_is_integer<Test> || xyu::t_is_pointer<Test>>>
        T fetch_add(T value, Order order = Order::Acq_Rel) noexcept
        { return __atomic_fetch_add(&v, value, (int)order); }
        /**
         * @brief 读取旧值增加后写入并返回
         * @param value 增加的值
         * @param order 内存序 (默认为 ACQ_REL)
         */
        template <typename Test = T, typename = xyu::t_enable<xyu::t_is_integer<Test> || xyu::t_is_pointer<Test>>>
        T add_fetch(T value, Order order = Order::Acq_Rel) noexcept
        { return __atomic_add_fetch(&v, value, (int)order); }

        /**
         * @brief 读取旧值返回并写入减少后的新值
         * @param value 减少的值
         * @param order 内存序 (默认为 ACQ_REL)
         */
        template <typename Test = T, typename = xyu::t_enable<xyu::t_is_integer<Test> || xyu::t_is_pointer<Test>>>
        T fetch_sub(T value, Order order = Order::Acq_Rel) noexcept
        { return __atomic_fetch_sub(&v, value, (int)order); }
        /**
         * @brief 读取旧值减少后写入并返回
         * @param value 减少的值
         * @param order 内存序 (默认为 ACQ_REL)
         */
        template <typename Test = T, typename = xyu::t_enable<xyu::t_is_integer<Test> || xyu::t_is_pointer<Test>>>
        T sub_fetch(T value, Order order = Order::Acq_Rel) noexcept
        { return __atomic_sub_fetch(&v, value, (int)order); }

        /**
         * @brief 读取旧值返回并写入按位与后的新值
         * @param value 按位交换的值
         * @param order 内存序 (默认为 ACQ_REL)
         */
        template <typename U, typename Test = T, typename = xyu::t_enable<xyu::t_is_integer<Test> && (xyu::t_is_integer<U> || xyu::t_is_uenum<U>)>>
        T fetch_and(U value, Order order = Order::Acq_Rel) noexcept
        { return __atomic_fetch_and(&v, value, (int)order); }
        /**
         * @brief 读取旧值按位与后写入并返回
         * @param value 按位与的值
         * @param order 内存序 (默认为 ACQ_REL)
         */
        template <typename U, typename Test = T, typename = xyu::t_enable<xyu::t_is_integer<Test> && (xyu::t_is_integer<U> || xyu::t_is_uenum<U>)>>
        T and_fetch(U value, Order order = Order::Acq_Rel) noexcept
        { return __atomic_and_fetch(&v, value, (int)order); }

        /**
         * @brief 读取旧值返回并写入按位或后的新值
         * @param value 按位或的值
         * @param order 内存序 (默认为 ACQ_REL)
         */
        template <typename U, typename Test = T, typename = xyu::t_enable<xyu::t_is_integer<Test> && (xyu::t_is_integer<U> || xyu::t_is_uenum<U>)>>
        T fetch_or(U value, Order order = Order::Acq_Rel) noexcept
        { return __atomic_fetch_or(&v, value, (int)order); }
        /**
         * @brief 读取旧值按位或后写入并返回
         * @param value 按位或的值
         * @param order 内存序 (默认为 ACQ_REL)
         */
        template <typename U, typename Test = T, typename = xyu::t_enable<xyu::t_is_integer<Test> && (xyu::t_is_integer<U> || xyu::t_is_uenum<U>)>>
        T or_fetch(U value, Order order = Order::Acq_Rel) noexcept
        { return __atomic_or_fetch(&v, value, (int)order); }

        /**
         * @brief 读取旧值返回并写入按位异或后的新值
         * @param value 按位异或的值
         * @param order 内存序 (默认为 ACQ_REL)
         */
        template <typename U, typename Test = T, typename = xyu::t_enable<xyu::t_is_integer<Test> && (xyu::t_is_integer<U> || xyu::t_is_uenum<U>)>>
        T fetch_xor(U value, Order order = Order::Acq_Rel) noexcept
        { return __atomic_fetch_xor(&v, value, (int)order); }
        /**
         * @brief 读取旧值按位异或后写入并返回
         * @param value 按位异或的值
         * @param order 内存序 (默认为 ACQ_REL)
         */
        template <typename U, typename Test = T, typename = xyu::t_enable<xyu::t_is_integer<Test> && (xyu::t_is_integer<U> || xyu::t_is_uenum<U>)>>
        T xor_fetch(U value, Order order = Order::Acq_Rel) noexcept
        { return __atomic_xor_fetch(&v, value, (int)order); }

        /**
         * @brief 读取旧值返回并写入按位与非后的新值
         * @param value 按位与非的值
         * @param order 内存序 (默认为 ACQ_REL)
         */
        template <typename U, typename Test = T, typename = xyu::t_enable<xyu::t_is_integer<Test> && (xyu::t_is_integer<U> || xyu::t_is_uenum<U>)>>
        T fetch_nand(U value, Order order = Order::Acq_Rel) noexcept
        { return __atomic_fetch_nand(&v, value, (int)order); }
        /**
         * @brief 读取旧值按位与非后写入并返回
         * @param value 按位与非的值
         * @param order 内存序 (默认为 ACQ_REL)
         */
        template <typename U, typename Test = T, typename = xyu::t_enable<xyu::t_is_integer<Test> && (xyu::t_is_integer<U> || xyu::t_is_uenum<U>)>>
        T nand_fetch(U value, Order order = Order::Acq_Rel) noexcept
        { return __atomic_nand_fetch(&v, value, (int)order); }

        /* 等待与唤醒 */

        /// 至少唤醒一个等待线程
        void notify_one() noexcept;
        /// 唤醒所有等待线程
        void notify_all() noexcept;

        /**
         * @brief 等待原子变量转为指定值
         * @param expect 期望值
         */
        template <typename Value, xyu::t_enable<!xyu::t_can_call<Value, const T&>, bool> = false>
        void wait(Value&& expect) { wait([&](const T& val){ return expect == val; }); }

        /**
         * @brief 等待直到指定条件满足
         * @param condition (const T& value) -> bool 的可调用对象
         */
        template <typename Condition, xyu::t_enable<xyu::t_can_call<Condition, const T&>, bool> = true>
        void wait(Condition&& condition) noexcept;
#else
        /* 锁判断 */

        /// 单线程永远无锁
        static constexpr bool is_always_lock_free = true;

        /// 单线程永远无锁
        bool is_lock_free() const noexcept { return true; }

        /* 原子操作 */

        /// 读取
        T load(Order = {}) const noexcept { return v; }

        /// 写入
        void store(T value, Order = {}) noexcept { v = value; }

        /// 交换，读取旧值返回并写入新值
        T exchange(T value, Order = {}) noexcept { T tmp = v; v = value; return tmp; }

        /**
         * @brief 比较，如果当前值等于预期值，则写入新值并返回true，否则返回false
         * @param expected 预期值
         * @param value 新的值
         */
        template <typename U, typename = xyu::t_enable<xyu::t_is_same_nocr<U,T>>>
        bool compare_exchange_weak(U&& expected, T value, Order = {}) noexcept
        { return compare_exchange_strong(expected, value); }

        /**
        * @brief 比较，如果当前值等于预期值，则写入新值并返回true，否则返回false
        * @param expected 预期值
        * @param value 新的值
        */
        template <typename U, typename = xyu::t_enable<xyu::t_is_same_nocr<U,T>>>
        bool compare_exchange_strong(U&& expected, T value, Order = {}) noexcept
        {
            if (v == expected) { v = value; return true; }
            if constexpr (xyu::t_is_lrefer<U>) expected = v;
            return false;
        }

        /**
         * @brief 读取旧值返回并写入增加后的新值
         * @param value 增加的值
         */
        template <typename Test = T, typename = xyu::t_enable<xyu::t_is_integer<Test> || xyu::t_is_pointer<Test>>>
        T fetch_add(T value, Order = {}) noexcept { T tmp = v; v += value; return tmp; }
        /**
         * @brief 读取旧值增加后写入并返回
         * @param value 增加的值
         */
        template <typename Test = T, typename = xyu::t_enable<xyu::t_is_integer<Test> || xyu::t_is_pointer<Test>>>
        T add_fetch(T value, Order = {}) noexcept { return v += value; }

        /**
         * @brief 读取旧值返回并写入减少后的新值
         * @param value 减少的值
         */
        template <typename Test = T, typename = xyu::t_enable<xyu::t_is_integer<Test> || xyu::t_is_pointer<Test>>>
        T fetch_sub(T value, Order = {}) noexcept { T tmp = v; v -= value; return tmp; }
        /**
         * @brief 读取旧值减少后写入并返回
         * @param value 减少的值
         */
        template <typename Test = T, typename = xyu::t_enable<xyu::t_is_integer<Test> || xyu::t_is_pointer<Test>>>
        T sub_fetch(T value, Order = {}) noexcept { return v -= value; }

        /**
         * @brief 读取旧值返回并写入按位与后的新值
         * @param value 按位交换的值
         */
        template <typename U, typename Test = T, typename = xyu::t_enable<xyu::t_is_integer<Test> && (xyu::t_is_integer<U> || xyu::t_is_uenum<U>)>>
        T fetch_and(U value, Order = {}) noexcept { T tmp = v; v &= value; return tmp; }
        /**
         * @brief 读取旧值按位与后写入并返回
         * @param value 按位与的值
         */
        template <typename U, typename Test = T, typename = xyu::t_enable<xyu::t_is_integer<Test> && (xyu::t_is_integer<U> || xyu::t_is_uenum<U>)>>
        T and_fetch(U value, Order = {}) noexcept { return v &= value; }

        /**
         * @brief 读取旧值返回并写入按位或后的新值
         * @param value 按位或的值
         */
        template <typename U, typename Test = T, typename = xyu::t_enable<xyu::t_is_integer<Test> && (xyu::t_is_integer<U> || xyu::t_is_uenum<U>)>>
        T fetch_or(U value, Order = {}) noexcept { T tmp = v; v |= value; return tmp; }
        /**
         * @brief 读取旧值按位或后写入并返回
         * @param value 按位或的值
         */
        template <typename U, typename Test = T, typename = xyu::t_enable<xyu::t_is_integer<Test> && (xyu::t_is_integer<U> || xyu::t_is_uenum<U>)>>
        T or_fetch(U value, Order = {}) noexcept { return v |= value; }

        /**
         * @brief 读取旧值返回并写入按位异或后的新值
         * @param value 按位异或的值
         */
        template <typename U, typename Test = T, typename = xyu::t_enable<xyu::t_is_integer<Test> && (xyu::t_is_integer<U> || xyu::t_is_uenum<U>)>>
        T fetch_xor(U value, Order = {}) noexcept { T tmp = v; v ^= value; return tmp; }
        /**
         * @brief 读取旧值按位异或后写入并返回
         * @param value 按位异或的值
         */
        template <typename U, typename Test = T, typename = xyu::t_enable<xyu::t_is_integer<Test> && (xyu::t_is_integer<U> || xyu::t_is_uenum<U>)>>
        T xor_fetch(U value, Order = {}) noexcept { return v ^= value; }

        /**
         * @brief 读取旧值返回并写入按位与非后的新值
         * @param value 按位与非的值
         */
        template <typename U, typename Test = T, typename = xyu::t_enable<xyu::t_is_integer<Test> && (xyu::t_is_integer<U> || xyu::t_is_uenum<U>)>>
        T fetch_nand(U value, Order = {}) noexcept { T tmp = v; v = ~(v & value); return tmp; }
        /**
         * @brief 读取旧值按位与非后写入并返回
         * @param value 按位与非的值
         */
        template <typename U, typename Test = T, typename = xyu::t_enable<xyu::t_is_integer<Test> && (xyu::t_is_integer<U> || xyu::t_is_uenum<U>)>>
        T nand_fetch(U value, Order = {}) noexcept { return v = ~(v & value); }
#endif

        /* 运算符重载 */

        template <typename Test = T, typename = xyu::t_enable<xyu::t_is_integer<Test> || xyu::t_is_pointer<Test>>>
        T operator++() noexcept { return add_fetch(1); }
        template <typename Test = T, typename = xyu::t_enable<xyu::t_is_integer<Test> || xyu::t_is_pointer<Test>>>
        T operator--() noexcept { return sub_fetch(1); }
        template <typename Test = T, typename = xyu::t_enable<xyu::t_is_integer<Test> || xyu::t_is_pointer<Test>>>
        T operator++(int) noexcept { return fetch_add(1); }
        template <typename Test = T, typename = xyu::t_enable<xyu::t_is_integer<Test> || xyu::t_is_pointer<Test>>>
        T operator--(int) noexcept { return fetch_sub(1); }

        template <typename U, typename Test = T, typename = xyu::t_enable<xyu::t_is_integer<Test> && (xyu::t_is_integer<U> || xyu::t_is_uenum<U>)>>
        T operator+=(U value) noexcept { return add_fetch(value); }
        template <typename U, typename Test = T, typename = xyu::t_enable<xyu::t_is_integer<Test> && (xyu::t_is_integer<U> || xyu::t_is_uenum<U>)>>
        T operator-=(U value) noexcept { return sub_fetch(value); }

        template <typename U, typename Test = T, typename = xyu::t_enable<xyu::t_is_integer<Test> && (xyu::t_is_integer<U> || xyu::t_is_uenum<U>)>>
        T operator&=(U value) noexcept { return and_fetch(value); }
        template <typename U, typename Test = T, typename = xyu::t_enable<xyu::t_is_integer<Test> && (xyu::t_is_integer<U> || xyu::t_is_uenum<U>)>>
        T operator|=(U value) noexcept { return or_fetch(value); }
        template <typename U, typename Test = T, typename = xyu::t_enable<xyu::t_is_integer<Test> && (xyu::t_is_integer<U> || xyu::t_is_uenum<U>)>>
        T operator^=(U value) noexcept { return xor_fetch(value); }

    private:
        // 操作失败时的内存序 (抛弃 释放操作，即不保证其他线程读取到的值为修改后的)
        constexpr static int get_failed_order(Order order) noexcept
        {
            if (order == Order::Acq_Rel) return (int)Order::Acquire;
            if (order == Order::Release) return (int)Order::Relaxed;
            return (int)order;
        }
    };
}

#if XY_THREAD
/// 等待与唤醒
namespace xylu::xyconc
{
    namespace __
    {
        // 唤醒辅助函数
        void notify_one_help(xyu::size_t size, void* p) noexcept;
        void notify_all_help(xyu::size_t size, void* p) noexcept;
        // 等待辅助函数
        void wait_enter(xyu::size_t size, void* p) noexcept;
        void wait_help(xyu::size_t size, void* p, const void* vp, bool(*eq)(void*, const void*)) noexcept;
        void wait_leave(xyu::size_t size, void* p) noexcept;
        // 比较函数
        template <typename T>
        bool wait_equals(void* p, const void* vp) noexcept
        { return __atomic_load_n((T*)p, (int)xyu::N_Atomic_Order::Relaxed) == *(const T*)vp; }
    }

    // 唤醒单个
    template <typename T>
    void Atomic<T>::notify_one() noexcept { __::notify_one_help(sizeof(T), &v); }
    // 唤醒全部
    template <typename T>
    void Atomic<T>::notify_all() noexcept { __::notify_all_help(sizeof(T), &v); }

    // 等待
    template <typename T>
    template <typename Condition, xyu::t_enable<xyu::t_can_call<Condition, const T&>, bool>>
    void Atomic<T>::wait(Condition&& condition) noexcept
    {
        if constexpr (!XY_RELEASE && xyu::t_is_pointer<Condition>)
            if (XY_UNLIKELY(condition == nullptr))
                xyu::E_Logic::abort_null_pointer(XY_CurPos("Atomic::wait"), "conditon");

        __::wait_enter(sizeof(T), &v);
        for(;;)
        {
            T old = load();
            if (condition(old)) break;
            __::wait_help(sizeof(T), &v, &old, __::wait_equals<T>);
        }
        __::wait_leave(sizeof(T), &v);
    }
}
#endif
