#pragma clang diagnostic push
#pragma ide diagnostic ignored "google-explicit-constructor"
#pragma ide diagnostic ignored "OCUnusedGlobalDeclarationInspection"
#pragma ide diagnostic ignored "modernize-use-nodiscard"
#pragma once

#include "../../link/format"

/// 原子变量
namespace xylu::xyconc
{
    /**
     * @brief 原子变量类
     * @note 支持 整型及指针类型
     * @note XY_UNTHREAD 下直接作为普通变量操作
     */
    /**
     * @brief 为类型 T 提供原子操作的封装类。
     * @details
     *   `Atomic<T>` 保证了对共享变量 T 的所有操作都是原子的，即这些操作在
     *   多线程环境中不会被中断，从而避免了数据竞争。它封装了底层的原子指令，
     *   并提供了与普通变量类似的接口（如赋值、隐式转换、算术运算符等）。
     *
     *   所有原子操作都接受一个可选的内存序（memory order）参数，用于控制
     *   编译器和 CPU 对内存操作的重排行为，以实现更精细的同步。若不指定，
     *   则使用全局默认的内存序 xyu::K_ATOMIC_ORDER。
     *
     * @tparam T 被封装的类型。必须是可平凡复制的（trivially copyable），
     *           且对于算术和位运算，通常要求是整型或指针类型。
     * @note 在单线程编译环境 (XY_UNTHREAD 宏定义) 下，`Atomic` 的行为会退化为
     *       对普通变量的非原子操作，以消除不必要的性能开销。
     */
    template <typename T>
    class Atomic
    {
        static_assert(!xyu::t_is_empty<T>, "Atomic<T> must not be an empty type");
        static_assert(xyu::t_can_trivial_copy<T>, "Atomic<T> must be a trivially copyable type");

    private:
        // 对齐要求: 当 sizeof(T)<=K_DEFAULT_ALIGN 且 sizeof(T)是2的幂时，取 alignof(T)与sizof(T)的较大值
        // 如当 K_DEFAULT_ALIGN为16, sizeof(T)=16, alignof(T)=8 时，会将 align 设置为 16
        constexpr static xyu::size_t align =
                xyu::max(alignof(T), (sizeof(T) > xyu::K_DEFAULT_ALIGN || (sizeof(T) & (sizeof(T)-1))) ? 0 : sizeof(T));
        // 存储
        alignas(align) T v;

    public:
        /* 实例 */

        /// 默认构造
        Atomic() = default;
        /// 析构
        ~Atomic() noexcept = default;
        /// 参数构造 (聚合类型)
        template <typename... Args, typename Test = T, xyu::t_enable<xyu::t_is_aggregate<Test>, bool> = true>
        Atomic(Args&&... args) noexcept(xyu::t_can_nothrow_listinit<T, Args...>) : v{xyu::forward<Args>(args)...} {}
        /// 参数构造 (非聚合类型)
        template <typename... Args, typename Test = T, xyu::t_enable<!xyu::t_is_aggregate<Test>, bool> = false>
        Atomic(Args&&... args) noexcept(xyu::t_can_nothrow_construct<T, Args...>) : v(xyu::forward<Args>(args)...) {}
        /// 参数赋值
        Atomic& operator=(T value) noexcept { store(value); return *this; }

        /// 复制构造
        Atomic(const Atomic& other) noexcept : v{other.load(xyu::K_ATOMIC_ORDER)} {}
        /// 移动构造
        Atomic(Atomic&& other) noexcept : v{other.load(xyu::K_ATOMIC_ORDER)} {}
        /// 复制赋值
        Atomic& operator=(const Atomic& other) noexcept { store(other.load(xyu::K_ATOMIC_ORDER)); return *this; }
        /// 移动赋值
        Atomic& operator=(Atomic&& other) noexcept { store(other.load(xyu::K_ATOMIC_ORDER)); return *this; }

        /// 隐式转换
        operator T() const noexcept { return load(xyu::K_ATOMIC_ORDER); }
        /// 隐式转换
        operator T() const volatile noexcept { return load(xyu::K_ATOMIC_ORDER); }

        /* 锁判断 */
#if XY_UNTHREAD
        /// 是否为无锁操作
        static constexpr bool is_always_lock_free = true;
        /// 是否为无锁操作
        bool is_lock_free() const noexcept { return true; }
        /// 是否为无锁操作
        bool is_lock_free() const volatile noexcept { return __atomic_is_lock_free(sizeof(T), &v); }
#else
        /**
         * @brief 是否为无锁操作
         * @note 编译期判断，根据平台、编译选项、类型大小等因素来决定是否为无锁操作
         */
        static constexpr bool is_always_lock_free = __atomic_always_lock_free(sizeof(T), nullptr);

        /**
         * @brief 是否为无锁操作
         * @note 运行期判断，根据平台、类型大小、指针是否内存对齐等因素来决定是否为无锁操作
         */
        bool is_lock_free() const noexcept { return __atomic_is_lock_free(sizeof(T), &v); }
        /**
         * @brief 是否为无锁操作
         * @note 运行期判断，根据平台、类型大小、指针是否内存对齐等因素来决定是否为无锁操作
         */
        bool is_lock_free() const volatile noexcept { return __atomic_is_lock_free(sizeof(T), &v); }
#endif

        /* 原子操作 */
#if XY_UNTHREAD
        /// 读取
        T load(xyu::N_ATOMIC_ORDER = xyu::K_ATOMIC_ORDER) const noexcept { return v; }
        /// 读取
        T load(xyu::N_ATOMIC_ORDER = xyu::K_ATOMIC_ORDER) const volatile noexcept { return v; }

        /// 写入
        void store(T value, xyu::N_ATOMIC_ORDER = xyu::K_ATOMIC_ORDER) noexcept { v = value; }
        /// 写入
        void store(T value, xyu::N_ATOMIC_ORDER = xyu::K_ATOMIC_ORDER) volatile noexcept { v = value; }

        /// 交换，读取旧值返回并写入新值
        T exchange(T value, xyu::N_ATOMIC_ORDER = xyu::K_ATOMIC_ORDER) noexcept { T tmp = v; v = value; return tmp; }
        /// 交换，读取旧值返回并写入新值
        T exchange(T value, xyu::N_ATOMIC_ORDER = xyu::K_ATOMIC_ORDER) volatile noexcept { T tmp = v; v = value; return tmp; }

        /// 比较，如果当前值等于预期值，则写入新值并返回true，否则返回false
        bool compare_exchange_weak(T expected, T value, xyu::N_ATOMIC_ORDER = xyu::K_ATOMIC_ORDER) noexcept
        { if (v != expected) return false; v = value; return true; }
        /// 比较，如果当前值等于预期值，则写入新值并返回true，否则返回false
        bool compare_exchange_weak(T expected, T value, xyu::N_ATOMIC_ORDER = xyu::K_ATOMIC_ORDER) volatile noexcept
        { if (v != expected) return false; v = value; return true; }
        /// 比较，如果当前值等于预期值，则写入新值并返回true，否则返回false
        bool compare_exchange_strong(T expected, T value, xyu::N_ATOMIC_ORDER = xyu::K_ATOMIC_ORDER) noexcept
        { return compare_exchange_weak(expected, value); }
        /// 比较，如果当前值等于预期值，则写入新值并返回true，否则返回false
        bool compare_exchange_strong(T expected, T value, xyu::N_ATOMIC_ORDER = xyu::K_ATOMIC_ORDER) volatile noexcept
        { return compare_exchange_weak(expected, value); }

        /// 读取旧值返回并写入增加后的新值
        template <typename Test = T, typename = xyu::t_enable<xyu::t_is_mathint<Test> || xyu::t_is_pointer<Test>>>
        T fetch_add(T value, xyu::N_ATOMIC_ORDER = xyu::K_ATOMIC_ORDER) noexcept { T tmp = v; v += value; return tmp; }
        /// 读取旧值返回并写入增加后的新值
        template <typename Test = T, typename = xyu::t_enable<xyu::t_is_mathint<Test> || xyu::t_is_pointer<Test>>>
        T fetch_add(T value, xyu::N_ATOMIC_ORDER = xyu::K_ATOMIC_ORDER) volatile noexcept { T tmp = v; v += value; return tmp; }
        /// 读取旧值增加后写入并返回
        template <typename Test = T, typename = xyu::t_enable<xyu::t_is_mathint<Test> || xyu::t_is_pointer<Test>>>
        T add_fetch(T value, xyu::N_ATOMIC_ORDER = xyu::K_ATOMIC_ORDER) noexcept { return v += value; }
        /// 读取旧值增加后写入并返回
        template <typename Test = T, typename = xyu::t_enable<xyu::t_is_mathint<Test> || xyu::t_is_pointer<Test>>>
        T add_fetch(T value, xyu::N_ATOMIC_ORDER = xyu::K_ATOMIC_ORDER) volatile noexcept { return v += value; }

        /// 读取旧值返回并写入减少后的新值
        template <typename Test = T, typename = xyu::t_enable<xyu::t_is_mathint<Test> || xyu::t_is_pointer<Test>>>
        T fetch_sub(T value, xyu::N_ATOMIC_ORDER = xyu::K_ATOMIC_ORDER) noexcept { T tmp = v; v -= value; return tmp; }
        /// 读取旧值返回并写入减少后的新值
        template <typename Test = T, typename = xyu::t_enable<xyu::t_is_mathint<Test> || xyu::t_is_pointer<Test>>>
        T fetch_sub(T value, xyu::N_ATOMIC_ORDER = xyu::K_ATOMIC_ORDER) volatile noexcept { T tmp = v; v -= value; return tmp; }
        /// 读取旧值减少后写入并返回
        template <typename Test = T, typename = xyu::t_enable<xyu::t_is_mathint<Test> || xyu::t_is_pointer<Test>>>
        T sub_fetch(T value, xyu::N_ATOMIC_ORDER = xyu::K_ATOMIC_ORDER) noexcept { return v -= value; }
        /// 读取旧值减少后写入并返回
        template <typename Test = T, typename = xyu::t_enable<xyu::t_is_mathint<Test> || xyu::t_is_pointer<Test>>>
        T sub_fetch(T value, xyu::N_ATOMIC_ORDER = xyu::K_ATOMIC_ORDER) volatile noexcept { return v -= value; }

        /// 读取旧值返回并写入按位交与后的新值
        template <typename U, typename Test = T, typename = xyu::t_enable<xyu::t_is_mathint<Test> && xyu::t_is_mathint<U>>>
        T fetch_and(const U& value, xyu::N_ATOMIC_ORDER = xyu::K_ATOMIC_ORDER) noexcept { T tmp = v; v &= value; return tmp; }
        /// 读取旧值返回并写入按位交与后的新值
        template <typename U, typename Test = T, typename = xyu::t_enable<xyu::t_is_mathint<Test> && xyu::t_is_mathint<U>>>
        T fetch_and(const U& value, xyu::N_ATOMIC_ORDER = xyu::K_ATOMIC_ORDER) volatile noexcept { T tmp = v; v &= value; return tmp; }
        /// 读取旧值按位与后写入并返回
        template <typename U, typename Test = T, typename = xyu::t_enable<xyu::t_is_mathint<Test> && xyu::t_is_mathint<U>>>
        T and_fetch(const U& value, xyu::N_ATOMIC_ORDER = xyu::K_ATOMIC_ORDER) noexcept { return v &= value; }
        /// 读取旧值按位与后写入并返回
        template <typename U, typename Test = T, typename = xyu::t_enable<xyu::t_is_mathint<Test> && xyu::t_is_mathint<U>>>
        T and_fetch(const U& value, xyu::N_ATOMIC_ORDER = xyu::K_ATOMIC_ORDER) volatile noexcept { return v &= value; }

        /// 读取旧值返回并写入按位或后的新值
        template <typename U, typename Test = T, typename = xyu::t_enable<xyu::t_is_mathint<Test> && xyu::t_is_mathint<U>>>
        T fetch_or(const U& value, xyu::N_ATOMIC_ORDER = xyu::K_ATOMIC_ORDER) noexcept { T tmp = v; v |= value; return tmp; }
        /// 读取旧值返回并写入按位或后的新值
        template <typename U, typename Test = T, typename = xyu::t_enable<xyu::t_is_mathint<Test> && xyu::t_is_mathint<U>>>
        T fetch_or(const U& value, xyu::N_ATOMIC_ORDER = xyu::K_ATOMIC_ORDER) volatile noexcept { T tmp = v; v |= value; return tmp; }
        /// 读取旧值按位或后写入并返回
        template <typename U, typename Test = T, typename = xyu::t_enable<xyu::t_is_mathint<Test> && xyu::t_is_mathint<U>>>
        T or_fetch(const U& value, xyu::N_ATOMIC_ORDER = xyu::K_ATOMIC_ORDER) noexcept { return v |= value; }
        /// 读取旧值按位或后写入并返回
        template <typename U, typename Test = T, typename = xyu::t_enable<xyu::t_is_mathint<Test> && xyu::t_is_mathint<U>>>
        T or_fetch(const U& value, xyu::N_ATOMIC_ORDER = xyu::K_ATOMIC_ORDER) volatile noexcept { return v |= value; }

        /// 读取旧值返回并写入按位异或后的新值
        template <typename U, typename Test = T, typename = xyu::t_enable<xyu::t_is_mathint<Test> && xyu::t_is_mathint<U>>>
        T fetch_xor(const U& value, xyu::N_ATOMIC_ORDER = xyu::K_ATOMIC_ORDER) noexcept { T tmp = v; v ^= value; return tmp; }
        /// 读取旧值返回并写入按位异或后的新值
        template <typename U, typename Test = T, typename = xyu::t_enable<xyu::t_is_mathint<Test> && xyu::t_is_mathint<U>>>
        T fetch_xor(const U& value, xyu::N_ATOMIC_ORDER = xyu::K_ATOMIC_ORDER) volatile noexcept { T tmp = v; v ^= value; return tmp; }
        /// 读取旧值按位异或后写入并返回
        template <typename U, typename Test = T, typename = xyu::t_enable<xyu::t_is_mathint<Test> && xyu::t_is_mathint<U>>>
        T xor_fetch(const U& value, xyu::N_ATOMIC_ORDER = xyu::K_ATOMIC_ORDER) noexcept { return v ^= value; }
        /// 读取旧值按位异或后写入并返回
        template <typename U, typename Test = T, typename = xyu::t_enable<xyu::t_is_mathint<Test> && xyu::t_is_mathint<U>>>
        T xor_fetch(const U& value, xyu::N_ATOMIC_ORDER = xyu::K_ATOMIC_ORDER) volatile noexcept { return v ^= value; }

        /// 读取旧值返回并写入按位与非后的新值
        template <typename U, typename Test = T, typename = xyu::t_enable<xyu::t_is_mathint<Test> && xyu::t_is_mathint<U>>>
        T fetch_nand(const U& value, xyu::N_ATOMIC_ORDER = xyu::K_ATOMIC_ORDER) noexcept { T tmp = v; v = ~(v & value); return tmp; }
        /// 读取旧值返回并写入按位与非后的新值
        template <typename U, typename Test = T, typename = xyu::t_enable<xyu::t_is_mathint<Test> && xyu::t_is_mathint<U>>>
        T fetch_nand(const U& value, xyu::N_ATOMIC_ORDER = xyu::K_ATOMIC_ORDER) volatile noexcept { T tmp = v; v = ~(v & value); return tmp; }
        /// 读取旧值按位与非后写入并返回
        template <typename U, typename Test = T, typename = xyu::t_enable<xyu::t_is_mathint<Test> && xyu::t_is_mathint<U>>>
        T nand_fetch(const U& value, xyu::N_ATOMIC_ORDER = xyu::K_ATOMIC_ORDER) noexcept { return (v = ~v & value); }
        /// 读取旧值按位与非后写入并返回
        template <typename U, typename Test = T, typename = xyu::t_enable<xyu::t_is_mathint<Test> && xyu::t_is_mathint<U>>>
        T nand_fetch(const U& value, xyu::N_ATOMIC_ORDER = xyu::K_ATOMIC_ORDER) volatile noexcept { return (v = ~v & value); }
#else
        /// 读取
        T load(xyu::N_ATOMIC_ORDER order = xyu::K_ATOMIC_ORDER) const noexcept
        {
            alignas(align) xyu::uint8 buf[sizeof(T)];
            auto p = reinterpret_cast<T*>(buf);
            __atomic_load(&v, p, order);
            return *p;
        }
        /// 读取
        T load(xyu::N_ATOMIC_ORDER order = xyu::K_ATOMIC_ORDER) const volatile noexcept
        {
            alignas(align) xyu::uint8 buf[sizeof(T)];
            auto p = reinterpret_cast<T*>(buf);
            __atomic_load(&v, p, order);
            return *p;
        }

        /// 写入
        void store(T value, xyu::N_ATOMIC_ORDER order = xyu::K_ATOMIC_ORDER) noexcept
        { __atomic_store(&v, &value, order); }
        /// 写入
        void store(T value, xyu::N_ATOMIC_ORDER order = xyu::K_ATOMIC_ORDER) volatile noexcept
        { __atomic_store(&v, &value, order); }

        /**
         * @brief 交换，读取旧值返回并写入新值
         * @param value 新的值
         * @param order 内存序 (默认为 K_ATOMIC_ORDER)
         */
        T exchange(T value, xyu::N_ATOMIC_ORDER order = xyu::K_ATOMIC_ORDER) noexcept
        {
            alignas(align) xyu::uint8 buf[sizeof(T)];
            auto p = reinterpret_cast<T*>(buf);
            __atomic_exchange(&v, &value, p, order);
            return *p;
        }
        /**
         * @brief 交换，读取旧值返回并写入新值
         * @param value 新的值
         * @param order 内存序 (默认为 K_ATOMIC_ORDER)
         */
        T exchange(T value, xyu::N_ATOMIC_ORDER order = xyu::K_ATOMIC_ORDER) volatile noexcept
        {
            alignas(align) xyu::uint8 buf[sizeof(T)];
            auto p = reinterpret_cast<T*>(buf);
            __atomic_exchange(&v, &value, p, order);
            return *p;
        }

        /**
         * @brief 比较，如果当前值等于预期值，则写入新值并返回true，否则返回false
         * @param expected 预期值
         * @param value 新的值
         * @param order 内存序 (默认为 K_ATOMIC_ORDER)
         * @note 弱比较，可能出现虚假失败
         */
        bool compare_exchange_weak(T expected, T value, xyu::N_ATOMIC_ORDER order = xyu::K_ATOMIC_ORDER) noexcept
        { return __atomic_compare_exchange_n(&v, &expected, value, true, order, get_failed_order(order)); }
        /**
         * @brief 比较，如果当前值等于预期值，则写入新值并返回true，否则返回false
         * @param expected 预期值
         * @param value 新的值
         * @param order 内存序 (默认为 K_ATOMIC_ORDER)
         * @note 弱比较，可能出现虚假失败
         */
        bool compare_exchange_weak(T expected, T value, xyu::N_ATOMIC_ORDER order = xyu::K_ATOMIC_ORDER) volatile noexcept
        { return __atomic_compare_exchange_n(&v, &expected, value, true, order, get_failed_order(order)); }

        /**
        * @brief 比较，如果当前值等于预期值，则写入新值并返回true，否则返回false
        * @param expected 预期值
        * @param value 新的值
        * @param order 内存序 (默认为 K_ATOMIC_ORDER)
        * @note 强比较，仅值不匹配时返回 false
        */
        bool compare_exchange_strong(T expected, T value, xyu::N_ATOMIC_ORDER order = xyu::K_ATOMIC_ORDER) noexcept
        { return __atomic_compare_exchange_n(&v, &expected, value, false, order, get_failed_order(order)); }
        /**
         * @brief 比较，如果当前值等于预期值，则写入新值并返回true，否则返回false
         * @param expected 预期值
         * @param value 新的值
         * @param order 内存序 (默认为 K_ATOMIC_ORDER)
         * @note 强比较，仅值不匹配时返回 false
         */
        bool compare_exchange_strong(T expected, T value, xyu::N_ATOMIC_ORDER order = xyu::K_ATOMIC_ORDER) volatile noexcept
        { return __atomic_compare_exchange_n(&v, &expected, value, false, order, get_failed_order(order)); }

        /**
         * @brief 读取旧值返回并写入增加后的新值
         * @param value 增加的值
         * @param order 内存序 (默认为 K_ATOMIC_ORDER)
         */
        template <typename Test = T, typename = xyu::t_enable<xyu::t_is_mathint<Test> || xyu::t_is_pointer<Test>>>
        T fetch_add(T value, xyu::N_ATOMIC_ORDER order = xyu::K_ATOMIC_ORDER) noexcept
        { return __atomic_fetch_add(&v, value, order); }
        /**
         * @brief 读取旧值返回并写入增加后的新值
         * @param value 增加的值
         * @param order 内存序 (默认为 K_ATOMIC_ORDER)
         */
        template <typename Test = T, typename = xyu::t_enable<xyu::t_is_mathint<Test> || xyu::t_is_pointer<Test>>>
        T fetch_add(T value, xyu::N_ATOMIC_ORDER order = xyu::K_ATOMIC_ORDER) volatile noexcept
        { return __atomic_fetch_add(&v, value, order); }

        /**
         * @brief 读取旧值增加后写入并返回
         * @param value 增加的值
         * @param order 内存序 (默认为 K_ATOMIC_ORDER)
         */
        template <typename Test = T, typename = xyu::t_enable<xyu::t_is_mathint<Test> || xyu::t_is_pointer<Test>>>
        T add_fetch(T value, xyu::N_ATOMIC_ORDER order = xyu::K_ATOMIC_ORDER) noexcept
        { return __atomic_add_fetch(&v, value, order); }
        /**
         * @brief 读取旧值增加后写入并返回
         * @param value 增加的值
         * @param order 内存序 (默认为 K_ATOMIC_ORDER)
         */
        template <typename Test = T, typename = xyu::t_enable<xyu::t_is_mathint<Test> || xyu::t_is_pointer<Test>>>
        T add_fetch(T value, xyu::N_ATOMIC_ORDER order = xyu::K_ATOMIC_ORDER) volatile noexcept
        { return __atomic_add_fetch(&v, value, order); }

        /**
         * @brief 读取旧值返回并写入减少后的新值
         * @param value 减少的值
         * @param order 内存序 (默认为 K_ATOMIC_ORDER)
         */
        template <typename Test = T, typename = xyu::t_enable<xyu::t_is_mathint<Test> || xyu::t_is_pointer<Test>>>
        T fetch_sub(T value, xyu::N_ATOMIC_ORDER order = xyu::K_ATOMIC_ORDER) noexcept
        { return __atomic_fetch_sub(&v, value, order); }
        /**
         * @brief 读取旧值返回并写入减少后的新值
         * @param value 减少的值
         * @param order 内存序 (默认为 K_ATOMIC_ORDER)
         */
        template <typename Test = T, typename = xyu::t_enable<xyu::t_is_mathint<Test> || xyu::t_is_pointer<Test>>>
        T fetch_sub(T value, xyu::N_ATOMIC_ORDER order = xyu::K_ATOMIC_ORDER) volatile noexcept
        { return __atomic_fetch_sub(&v, value, order); }

        /**
         * @brief 读取旧值减少后写入并返回
         * @param value 减少的值
         * @param order 内存序 (默认为 K_ATOMIC_ORDER)
         */
        template <typename Test = T, typename = xyu::t_enable<xyu::t_is_mathint<Test> || xyu::t_is_pointer<Test>>>
        T sub_fetch(T value, xyu::N_ATOMIC_ORDER order = xyu::K_ATOMIC_ORDER) noexcept
        { return __atomic_sub_fetch(&v, value, order); }
        /**
         * @brief 读取旧值减少后写入并返回
         * @param value 减少的值
         * @param order 内存序 (默认为 K_ATOMIC_ORDER)
         */
        template <typename Test = T, typename = xyu::t_enable<xyu::t_is_mathint<Test> || xyu::t_is_pointer<Test>>>
        T sub_fetch(T value, xyu::N_ATOMIC_ORDER order = xyu::K_ATOMIC_ORDER) volatile noexcept
        { return __atomic_sub_fetch(&v, value, order); }

        /**
         * @brief 读取旧值返回并写入按位与后的新值
         * @param value 按位交换的值
         * @param order 内存序 (默认为 K_ATOMIC_ORDER)
         */
        template <typename U, typename Test = T, typename = xyu::t_enable<xyu::t_is_mathint<Test> && xyu::t_is_mathint<U>>>
        T fetch_and(const U& value, xyu::N_ATOMIC_ORDER order = xyu::K_ATOMIC_ORDER) noexcept
        { return __atomic_fetch_and(&v, value, order); }
        /**
         * @brief 读取旧值返回并写入按位与后的新值
         * @param value 按位与的值
         * @param order 内存序 (默认为 K_ATOMIC_ORDER)
         */
        template <typename U, typename Test = T, typename = xyu::t_enable<xyu::t_is_mathint<Test> && xyu::t_is_mathint<U>>>
        T fetch_and(const U& value, xyu::N_ATOMIC_ORDER order = xyu::K_ATOMIC_ORDER) volatile noexcept
        { return __atomic_fetch_and(&v, value, order); }

        /**
         * @brief 读取旧值按位与后写入并返回
         * @param value 按位与的值
         * @param order 内存序 (默认为 K_ATOMIC_ORDER)
         */
        template <typename U, typename Test = T, typename = xyu::t_enable<xyu::t_is_mathint<Test> && xyu::t_is_mathint<U>>>
        T and_fetch(const U& value, xyu::N_ATOMIC_ORDER order = xyu::K_ATOMIC_ORDER) noexcept
        { return __atomic_and_fetch(&v, value, order); }
        /**
         * @brief 读取旧值按位与后写入并返回
         * @param value 按位与的值
         * @param order 内存序 (默认为 K_ATOMIC_ORDER)
         */
        template <typename U, typename Test = T, typename = xyu::t_enable<xyu::t_is_mathint<Test> && xyu::t_is_mathint<U>>>
        T and_fetch(const U& value, xyu::N_ATOMIC_ORDER order = xyu::K_ATOMIC_ORDER) volatile noexcept
        { return __atomic_and_fetch(&v, value, order); }

        /**
         * @brief 读取旧值返回并写入按位或后的新值
         * @param value 按位或的值
         * @param order 内存序 (默认为 K_ATOMIC_ORDER)
         */
        template <typename U, typename Test = T, typename = xyu::t_enable<xyu::t_is_mathint<Test> && xyu::t_is_mathint<U>>>
        T fetch_or(const U& value, xyu::N_ATOMIC_ORDER order = xyu::K_ATOMIC_ORDER) noexcept
        { return __atomic_fetch_or(&v, value, order); }
        /**
         * @brief 读取旧值返回并写入按位或后的新值
         * @param value 按位或的值
         * @param order 内存序 (默认为 K_ATOMIC_ORDER)
         */
        template <typename U, typename Test = T, typename = xyu::t_enable<xyu::t_is_mathint<Test> && xyu::t_is_mathint<U>>>
        T fetch_or(const U& value, xyu::N_ATOMIC_ORDER order = xyu::K_ATOMIC_ORDER) volatile noexcept
        { return __atomic_fetch_or(&v, value, order); }

        /**
         * @brief 读取旧值按位或后写入并返回
         * @param value 按位或的值
         * @param order 内存序 (默认为 K_ATOMIC_ORDER)
         */
        template <typename U, typename Test = T, typename = xyu::t_enable<xyu::t_is_mathint<Test> && xyu::t_is_mathint<U>>>
        T or_fetch(const U& value, xyu::N_ATOMIC_ORDER order = xyu::K_ATOMIC_ORDER) noexcept
        { return __atomic_or_fetch(&v, value, order); }
        /**
         * @brief 读取旧值按位或后写入并返回
         * @param value 按位或的值
         * @param order 内存序 (默认为 K_ATOMIC_ORDER)
         */
        template <typename U, typename Test = T, typename = xyu::t_enable<xyu::t_is_mathint<Test> && xyu::t_is_mathint<U>>>
        T or_fetch(const U& value, xyu::N_ATOMIC_ORDER order = xyu::K_ATOMIC_ORDER) volatile noexcept
        { return __atomic_or_fetch(&v, value, order); }

        /**
         * @brief 读取旧值返回并写入按位异或后的新值
         * @param value 按位异或的值
         * @param order 内存序 (默认为 K_ATOMIC_ORDER)
         */
        template <typename U, typename Test = T, typename = xyu::t_enable<xyu::t_is_mathint<Test> && xyu::t_is_mathint<U>>>
        T fetch_xor(const U& value, xyu::N_ATOMIC_ORDER order = xyu::K_ATOMIC_ORDER) noexcept
        { return __atomic_fetch_xor(&v, value, order); }
        /**
         * @brief 读取旧值返回并写入按位异或后的新值
         * @param value 按位异或的值
         * @param order 内存序 (默认为 K_ATOMIC_ORDER)
         */
        template <typename U, typename Test = T, typename = xyu::t_enable<xyu::t_is_mathint<Test> && xyu::t_is_mathint<U>>>
        T fetch_xor(const U& value, xyu::N_ATOMIC_ORDER order = xyu::K_ATOMIC_ORDER) volatile noexcept
        { return __atomic_fetch_xor(&v, value, order); }

        /**
         * @brief 读取旧值按位异或后写入并返回
         * @param value 按位异或的值
         * @param order 内存序 (默认为 K_ATOMIC_ORDER)
         */
        template <typename U, typename Test = T, typename = xyu::t_enable<xyu::t_is_mathint<Test> && xyu::t_is_mathint<U>>>
        T xor_fetch(const U& value, xyu::N_ATOMIC_ORDER order = xyu::K_ATOMIC_ORDER) noexcept
        { return __atomic_xor_fetch(&v, value, order); }
        /**
         * @brief 读取旧值按位异或后写入并返回
         * @param value 按位异或的值
         * @param order 内存序 (默认为 K_ATOMIC_ORDER)
         */
        template <typename U, typename Test = T, typename = xyu::t_enable<xyu::t_is_mathint<Test> && xyu::t_is_mathint<U>>>
        T xor_fetch(const U& value, xyu::N_ATOMIC_ORDER order = xyu::K_ATOMIC_ORDER) volatile noexcept
        { return __atomic_xor_fetch(&v, value, order); }

        /**
         * @brief 读取旧值返回并写入按位与非后的新值
         * @param value 按位与非的值
         * @param order 内存序 (默认为 K_ATOMIC_ORDER)
         */
        template <typename U, typename Test = T, typename = xyu::t_enable<xyu::t_is_mathint<Test> && xyu::t_is_mathint<U>>>
        T fetch_nand(const U& value, xyu::N_ATOMIC_ORDER order = xyu::K_ATOMIC_ORDER) noexcept
        { return __atomic_fetch_nand(&v, value, order); }
        /**
         * @brief 读取旧值返回并写入按位与非后的新值
         * @param value 按位与非的值
         * @param order 内存序 (默认为 K_ATOMIC_ORDER)
         */
        template <typename U, typename Test = T, typename = xyu::t_enable<xyu::t_is_mathint<Test> && xyu::t_is_mathint<U>>>
        T fetch_nand(const U& value, xyu::N_ATOMIC_ORDER order = xyu::K_ATOMIC_ORDER) volatile noexcept
        { return __atomic_fetch_nand(&v, value, order); }

        /**
         * @brief 读取旧值按位与非后写入并返回
         * @param value 按位与非的值
         * @param order 内存序 (默认为 K_ATOMIC_ORDER)
         */
        template <typename U, typename Test = T, typename = xyu::t_enable<xyu::t_is_mathint<Test> && xyu::t_is_mathint<U>>>
        T nand_fetch(const U& value, xyu::N_ATOMIC_ORDER order = xyu::K_ATOMIC_ORDER) noexcept
        { return __atomic_nand_fetch(&v, value, order); }
        /**
         * @brief 读取旧值按位与非后写入并返回
         * @param value 按位与非的值
         * @param order 内存序 (默认为 K_ATOMIC_ORDER)
         */
        template <typename U, typename Test = T, typename = xyu::t_enable<xyu::t_is_mathint<Test> && xyu::t_is_mathint<U>>>
        T nand_fetch(const U& value, xyu::N_ATOMIC_ORDER order = xyu::K_ATOMIC_ORDER) volatile noexcept
        { return __atomic_nand_fetch(&v, value, order); }
#endif

        /* 运算符重载 */

        template <typename Test = T, typename = xyu::t_enable<xyu::t_is_mathint<Test> || xyu::t_is_pointer<Test>>>
        Atomic& operator++() noexcept { add_fetch(1); return *this; }
        template <typename Test = T, typename = xyu::t_enable<xyu::t_is_mathint<Test> || xyu::t_is_pointer<Test>>>
        Atomic& operator++() volatile noexcept { add_fetch(1); return *this; }

        template <typename Test = T, typename = xyu::t_enable<xyu::t_is_mathint<Test> || xyu::t_is_pointer<Test>>>
        Atomic operator++(int) noexcept { return {fetch_add(1)}; }
        template <typename Test = T, typename = xyu::t_enable<xyu::t_is_mathint<Test> || xyu::t_is_pointer<Test>>>
        Atomic operator++(int) volatile noexcept { return {fetch_add(1)}; }

        template <typename Test = T, typename = xyu::t_enable<xyu::t_is_mathint<Test> || xyu::t_is_pointer<Test>>>
        Atomic& operator--() noexcept { sub_fetch(1); return *this; }
        template <typename Test = T, typename = xyu::t_enable<xyu::t_is_mathint<Test> || xyu::t_is_pointer<Test>>>
        Atomic& operator--() volatile noexcept { sub_fetch(1); return *this; }

        template <typename Test = T, typename = xyu::t_enable<xyu::t_is_mathint<Test> || xyu::t_is_pointer<Test>>>
        Atomic operator--(int) noexcept { return {fetch_sub(1)}; }
        template <typename Test = T, typename = xyu::t_enable<xyu::t_is_mathint<Test> || xyu::t_is_pointer<Test>>>
        Atomic operator--(int) volatile noexcept { return {fetch_sub(1)}; }

        template <typename U, typename Test = T, typename = xyu::t_enable<xyu::t_is_mathint<Test> && xyu::t_is_mathint<U>>>
        Atomic& operator+=(const U& value) noexcept { add_fetch(value); return *this; }
        template <typename U, typename Test = T, typename = xyu::t_enable<xyu::t_is_mathint<Test> && xyu::t_is_mathint<U>>>
        Atomic& operator+=(const U& value) volatile noexcept { add_fetch(value); return *this; }

        template <typename U, typename Test = T, typename = xyu::t_enable<xyu::t_is_mathint<Test> && xyu::t_is_mathint<U>>>
        Atomic operator+(const U& value) const noexcept { return {load() + value}; }
        template <typename U, typename Test = T, typename = xyu::t_enable<xyu::t_is_mathint<Test> && xyu::t_is_mathint<U>>>
        Atomic operator+(const U& value) const volatile noexcept { return {load() + value}; }
        template <typename U, typename Test = T, typename = xyu::t_enable<xyu::t_is_mathint<Test> && xyu::t_is_mathint<U>>>
        friend Atomic operator+(const U& value, const Atomic& atom) noexcept { return {atom.load() + value}; }
        template <typename U, typename Test = T, typename = xyu::t_enable<xyu::t_is_mathint<Test> && xyu::t_is_mathint<U>>>
        friend Atomic operator+(const U& value, const volatile Atomic& atom) noexcept { return {atom.load() + value}; }

        template <typename U, typename Test = T, typename = xyu::t_enable<xyu::t_is_mathint<Test> && xyu::t_is_mathint<U>>>
        Atomic& operator-=(const U& value) noexcept { sub_fetch(value); return *this; }
        template <typename U, typename Test = T, typename = xyu::t_enable<xyu::t_is_mathint<Test> && xyu::t_is_mathint<U>>>
        Atomic& operator-=(const U& value) volatile noexcept { sub_fetch(value); return *this; }

        template <typename U, typename Test = T, typename = xyu::t_enable<xyu::t_is_mathint<Test> && xyu::t_is_mathint<U>>>
        Atomic operator-(const U& value) const noexcept { return {load() - value}; }
        template <typename U, typename Test = T, typename = xyu::t_enable<xyu::t_is_mathint<Test> && xyu::t_is_mathint<U>>>
        Atomic operator-(const U& value) const volatile noexcept { return {load() - value}; }
        template <typename U, typename Test = T, typename = xyu::t_enable<xyu::t_is_mathint<Test> && xyu::t_is_mathint<U>>>
        friend Atomic operator-(const U& value, const Atomic& atom) noexcept { return {atom.load() - value}; }
        template <typename U, typename Test = T, typename = xyu::t_enable<xyu::t_is_mathint<Test> && xyu::t_is_mathint<U>>>
        friend Atomic operator-(const U& value, const volatile Atomic& atom) noexcept { return {atom.load() - value}; }

        template <typename U, typename Test = T, typename = xyu::t_enable<xyu::t_is_mathint<Test> && xyu::t_is_mathint<U>>>
        Atomic& operator&=(const U& value) noexcept { and_fetch(value); return *this; }
        template <typename U, typename Test = T, typename = xyu::t_enable<xyu::t_is_mathint<Test> && xyu::t_is_mathint<U>>>
        Atomic& operator&=(const U& value) volatile noexcept { and_fetch(value); return *this; }

        template <typename U, typename Test = T, typename = xyu::t_enable<xyu::t_is_mathint<Test> && xyu::t_is_mathint<U>>>
        Atomic operator&(const U& value) const noexcept { return {load() & value}; }
        template <typename U, typename Test = T, typename = xyu::t_enable<xyu::t_is_mathint<Test> && xyu::t_is_mathint<U>>>
        Atomic operator&(const U& value) const volatile noexcept { return {load() & value}; }
        template <typename U, typename Test = T, typename = xyu::t_enable<xyu::t_is_mathint<Test> && xyu::t_is_mathint<U>>>
        friend Atomic operator&(const U& value, const Atomic& atom) noexcept { return {atom.load() & value}; }
        template <typename U, typename Test = T, typename = xyu::t_enable<xyu::t_is_mathint<Test> && xyu::t_is_mathint<U>>>
        friend Atomic operator&(const U& value, const volatile Atomic& atom) noexcept { return {atom.load() & value}; }

        template <typename U, typename Test = T, typename = xyu::t_enable<xyu::t_is_mathint<Test> && xyu::t_is_mathint<U>>>
        Atomic& operator|=(const U& value) noexcept { or_fetch(value); return *this; }
        template <typename U, typename Test = T, typename = xyu::t_enable<xyu::t_is_mathint<Test> && xyu::t_is_mathint<U>>>
        Atomic& operator|=(const U& value) volatile noexcept { or_fetch(value); return *this; }

        template <typename U, typename Test = T, typename = xyu::t_enable<xyu::t_is_mathint<Test> && xyu::t_is_mathint<U>>>
        Atomic operator|(const U& value) const noexcept { return {load() | value}; }
        template <typename U, typename Test = T, typename = xyu::t_enable<xyu::t_is_mathint<Test> && xyu::t_is_mathint<U>>>
        Atomic operator|(const U& value) const volatile noexcept { return {load() | value}; }
        template <typename U, typename Test = T, typename = xyu::t_enable<xyu::t_is_mathint<Test> && xyu::t_is_mathint<U>>>
        friend Atomic operator|(const U& value, const Atomic& atom) noexcept { return {atom.load() | value}; }
        template <typename U, typename Test = T, typename = xyu::t_enable<xyu::t_is_mathint<Test> && xyu::t_is_mathint<U>>>
        friend Atomic operator|(const U& value, const volatile Atomic& atom) noexcept { return {atom.load() | value}; }

        template <typename U, typename Test = T, typename = xyu::t_enable<xyu::t_is_mathint<Test> && xyu::t_is_mathint<U>>>
        Atomic& operator^=(const U& value) noexcept { xor_fetch(value); return *this; }
        template <typename U, typename Test = T, typename = xyu::t_enable<xyu::t_is_mathint<Test> && xyu::t_is_mathint<U>>>
        Atomic& operator^=(const U& value) volatile noexcept { xor_fetch(value); return *this; }

        template <typename U, typename Test = T, typename = xyu::t_enable<xyu::t_is_mathint<Test> && xyu::t_is_mathint<U>>>
        Atomic operator^(const U& value) const noexcept { return {load() ^ value}; }
        template <typename U, typename Test = T, typename = xyu::t_enable<xyu::t_is_mathint<Test> && xyu::t_is_mathint<U>>>
        Atomic operator^(const U& value) const volatile noexcept { return {load() ^ value}; }
        template <typename U, typename Test = T, typename = xyu::t_enable<xyu::t_is_mathint<Test> && xyu::t_is_mathint<U>>>
        friend Atomic operator^(const U& value, const Atomic& atom) noexcept { return {atom.load() ^ value}; }
        template <typename U, typename Test = T, typename = xyu::t_enable<xyu::t_is_mathint<Test> && xyu::t_is_mathint<U>>>
        friend Atomic operator^(const U& value, const volatile Atomic& atom) noexcept { return {atom.load() ^ value}; }

    private:
#if !XY_UNTHREAD
        // 操作失败时的内存序 (抛弃 释放操作，即不保证其他线程读取到的值为修改后的)
        constexpr static xyu::N_ATOMIC_ORDER get_failed_order(xyu::N_ATOMIC_ORDER order) noexcept
        {
            if (order == xyu::N_ATOMIC_ACQ_REL) return xyu::N_ATOMIC_ACQUIRE;
            if (order == xyu::N_ATOMIC_RELEASE) return xyu::N_ATOMIC_RELAXED;
            return order;
        }
#endif
    };
}

/// 格式化
namespace xylu::xystring
{
    // 原子变量格式化，直接作为 T 格式化
    template <typename T>
    struct Formatter<xylu::xyconc::Atomic<T>> : Formatter<T> {};
}

#pragma clang diagnostic pop