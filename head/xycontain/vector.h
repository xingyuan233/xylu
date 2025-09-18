#pragma clang diagnostic push
#pragma ide diagnostic ignored "UnusedParameter"
#pragma ide diagnostic ignored "misc-unconventional-assign-operator"
#pragma ide diagnostic ignored "OCUnusedStructInspection"
#pragma ide diagnostic ignored "UnreachableCode"
#pragma ide diagnostic ignored "LoopDoesntUseConditionVariableInspection"
#pragma ide diagnostic ignored "EndlessLoop"
#pragma ide diagnostic ignored "UnreachableCallsOfFunction"
#pragma ide diagnostic ignored "OCUnusedGlobalDeclarationInspection"
#pragma ide diagnostic ignored "hicpp-exception-baseclass"
#pragma ide diagnostic ignored "modernize-use-nodiscard"
#pragma once

#include "../../link/log"

namespace xylu::xycontain
{
    /**
     * @brief 一个功能强大、高度优化且异常安全的动态数组容器。
     *
     * @tparam T 存储的元素类型。T 必须满足可无异常析构的要求 (is_nothrow_destructible)。
     *
     * @details
     * `xyu::Vector` 是一个旨在超越 `std::vector` 的动态数组实现，它深度集成了 xylu 库的
     * 核心组件，以提供卓越的性能、更强的健壮性和更丰富的API。
     *
     * ### 核心亮点 (Key Features):
     *
     * 1.  **高级内存管理 (Advanced Memory Management):**
     *     - 所有动态内存都通过 `xylu` 的线程局部内存池 (`thread_local` pool) 进行分配。
     *       这带来了极高的分配/释放性能，并在多线程环境中避免了全局堆锁的竞争。
     *     - 线程退出时，由该线程的 `Vector` 分配的内存会被自动回收，增强了程序的健壮性。
     *
     * 2.  **极致的异常安全 (Unparalleled Exception Safety):**
     *     - 在需要重新分配内存的操作（如扩容）中，提供了**强大的异常安全保证**。
     *     - 采用了“提交或回滚 (Commit or Rollback)”策略：在将旧元素移动到新内存块的过程中，
     *       如果任何元素的移动构造函数抛出异常，所有在新内存上已构造的对象都将被销毁，
     *       新内存被释放，而原始的 `Vector` 状态保持完全不变，杜绝了内存泄露和状态损坏。
     *
     * 3.  **统一且强大的修改接口 (Unified & Powerful Modification API):**
     *     - `append()` 和 `insert()` 方法通过模板元编程被极大地增强，能够智能地处理多种输入：
     *       - **多元素:** `vec.append(1, 2, 3);`
     *       - **范围对象:** `vec.append(another_vec.range());`
     *       - **任何可转换为范围的对象:** `vec.append(std::initializer_list<T>{...});`
     *       - **原地构造 (Emplace-like):** `vec.append<xyu::special_t>(arg1, arg2);`
     *     - 这种设计统一了 `push_back`, `emplace_back`, `insert` 等多种操作，使API更简洁、更强大。
     *
     * 4.  **与范围生态无缝融合 (Seamless Range Integration):**
     *     - `range()` 方法返回一个 `xyu::Range` 对象，使 `Vector` 成为 `xylu` 范围生态的
     *       一等公民。它可以直接用于范围for循环，并与所有范围算法和适配器无缝协作。
     *
     * 5.  **更友好的用户体验 (Enhanced Ergonomics):**
     *     - `operator[]` 支持类似 Python 的**负索引**，例如 `vec[-1]` 可以安全地访问
     *       最后一个元素，极大地简化了对尾部数据的操作。
     *     - 对聚合类型 (`Aggregate Types`) 的构造提供了原生支持，确保了 `{...}` 语法的正确使用。
     */
    template <typename T>
    class Vector
    {
        static_assert(xyu::t_can_nothrow_destruct<T>);

    public:
        /* 静态常量 */
        /// 释放空间条件比例
        static constexpr double K_shrink_factor = 0.5;
        /// 扩容比例
        static constexpr double K_grow_factor = 2;

    private:
        T* data;            // 数组指针
        xyu::size_t n;      // 使用的元素数量
        xyu::size_t capa;   // 数组容量

    public:
        /* 构造析构 */

        /// 默认构造
        Vector() noexcept : data{nullptr}, n{0}, capa{0} {}
        /// 移动构造
        Vector(Vector&& other) noexcept : data{other.data}, n{other.n}, capa{other.capa}
        {
            other.data = nullptr;
            other.n = other.capa = 0;
        }
        /// 复制构造
        Vector(const Vector& other) : Vector{} { append(other.range()); }

        /// 预分配空间构造
        explicit Vector(xyu::size_t capa) : data{xyu::alloc<T>(capa)}, n{0}, capa{capa} {}
        /// 分配空间并初始化
        Vector(xyu::size_t n, const T& value) : Vector{} { append(xyu::make_range_repeat(value, n)); }

        /// 初始化列表构造
        Vector(std::initializer_list<T> il) : Vector{} { append(il); }

        /// 析构
        ~Vector() noexcept { release(); }

        /* 赋值交换 */

        /// 移动赋值
        Vector& operator=(Vector&& other) noexcept { return swap(other); }
        /// 复制赋值
        Vector& operator=(const Vector& other)
        {
            if (XY_UNLIKELY(this == &other)) return *this;
            // 需要扩容
            if (capa < other.n)
            {
                clear();
                append(other.range());
            }
            // 容量足够
            else
            {
                bool app = n < other.n;
                for (xyu::size_t i = 0, j = app ? n : other.n; i < j; ++i) data[i] = other.data[i];
                if (app) for (;n < other.n; ++n) place_init(data + n, other.data[n]);
                else for (; n > other.n;) data[--n].~T();
            }
            return *this;
        }

        /// 交换
        Vector& swap(Vector&& other) noexcept { return swap(other); }
        /// 交换
        Vector& swap(Vector& other) noexcept
        {
            xyu::swap(data, other.data);
            xyu::swap(n, other.n);
            xyu::swap(capa, other.capa);
            return *this;
        }

        /* 数据容量 */

        /// 获取最大容量
        constexpr static xyu::size_t limit() noexcept { return xyu::number_traits<xyu::size_t>::max / 2; }
        /// 获取当前容量
        xyu::size_t capacity() const noexcept { return capa; }

        /// 获取元素数量
        xyu::size_t count() const noexcept { return n; }
        /// 是否为空
        bool empty() const noexcept { return n == 0; }

        /* 元素获取 */

        /// 获取元素
        T& get(xyu::size_t index) XY_NOEXCEPT_NDEBUG
        {
#if XY_DEBUG
            return at(index);
#else
            return data[index];
#endif
        }
        /// 获取元素 (const)
        const T& get(xyu::size_t index) const XY_NOEXCEPT_NDEBUG { return const_cast<Vector*>(this)->at(index); }

        /// 获取元素 [检测范围]
        T& at(xyu::size_t index)
        {
            if (XY_UNLIKELY(index >= n)) {
                xyloge(false, "E_Logic_Out_Of_Range: index {} out of range [0, {})", index, n);
                throw xyu::E_Logic_Out_Of_Range{};
            }
            return data[index];
        }
        /// 获取元素 (const) [检测范围]
        const T& at(xyu::size_t index) const { return const_cast<Vector*>(this)->at(index); }

        /* 数据管理 */

        /// 预分配空间
        void reserve(xyu::size_t mincapa)
        {
            if (mincapa > capa) realloc_capa(calc_new_capa(mincapa));
        }
        /// 缩减容量 (仅当 元素数量 < 容量 * K_shrink_factor)
        void reduce()
        {
            if (n < capa * K_shrink_factor) realloc_capa(n);
        }

        /// 调整元素数量 (可以缩减或扩容)
        void resize(xyu::size_t newsize, const T& value = T{})
        {
            if (newsize <= n) while (n > newsize) data[--n].~T();
            else {
                reserve(newsize);
                while (n < newsize) data[n++] = value;
            }
        }

        /// 清空元素
        void clear() noexcept
        {
            for (xyu::size_t i = 0; i < n; ++i) data[i].~T();
            n = 0;
        }
        /// 释放内存
        void release() noexcept
        {
            if (XY_UNLIKELY(capa == 0)) return;
            clear();
            xyu::dealloc<T>(data, capa);
            data = nullptr;
            capa = 0;
        }

        /* 尾增 */

        /**
         * @brief 尾部增加元素 (多元素)
         * @note 将每一个 args 作为一个元素添加到尾部
         */
        template <typename Tag = void, typename... Args, xyu::t_enable<!xyu::t_is_same<Tag, xyu::special_t> && xyu::t_can_init<T, xyu::t_args_get<xyu::typelist_c<Args..., void>, 0>>, bool> = true>
        Vector& append(Args&&... args)
        {
            static_assert(xyu::t_is_same<Tag, void>);
            static_assert((... && xyu::t_can_init<T, Args>));
            static_assert(sizeof...(args) <= limit());
            append_help<(... && xyu::t_can_nothrow_init<T, Args>)>(n + sizeof...(args),
                         [&]{ (..., place_init(data + n++, xyu::forward<Args>(args))); },
                         [&](T* newdata, xyu::size_t& i){ (..., place_init(newdata + i++, xyu::forward<Args>(args))); });
            return *this;
        }

        /**
         * @brief 尾部增加元素 (范围)
         * @note 将 range 中的每一个元素添加到尾部
         */
        template <typename Tag = void, typename Rg, xyu::t_enable<!xyu::t_is_same<Tag, xyu::special_t> && !xyu::t_can_init<T, Rg> && xyu::t_is_range<Rg>, bool> = true>
        Vector& append(Rg&& range)
        {
            static_assert(xyu::t_is_same<Tag, void>);
            check_add(range.count());
            append_help<xyu::t_can_nothrow_init<T, decltype(*range.begin())>>(n + range.count(),
                         [&]{ for (auto&& v : range) place_init(data + n++, xyu::forward<decltype(v)>(v)); },
                         [&](T* newdata, xyu::size_t& i){ for (auto&& v : range) place_init(newdata + i++, xyu::forward<decltype(v)>(v)); });
            return *this;
        }

        /**
         * @brief 尾部增加元素 (构造范围)
         * @note 将 args 构造为一个 xyu::range 后，逐元素添加到尾部
         */
        template <typename Tag = void, typename Arg, xyu::t_enable<!xyu::t_is_same<Tag, xyu::special_t> && !xyu::t_can_init<T, Arg> && !xyu::t_is_range<Arg>, bool> = true>
        Vector& append(Arg&& arg)
        {
            static_assert(xyu::t_is_same<Tag, void>);
            static_assert(xyu::t_can_make_range<Arg>);
            return append<Tag>(xyu::make_range(arg));
        }

        /**
         * @brief 尾部增加元素 (单元素)
         * @tparam Tag 显式指定为 xyu::special_t
         * @note 将 args 作为一个元素的构造参数添加到尾部
         */
        template <typename Tag, typename... Args, xyu::t_enable<xyu::t_is_same<Tag, xyu::special_t>, bool> = false>
        Vector& append(Args&&... args)
        {
            static_assert(xyu::t_can_init<T, Args...>);
            append_help<xyu::t_can_nothrow_init<T, Args...>>(n + 1,
                                 [&]{ place_init(data + n++, xyu::forward<Args>(args)...); },
                                 [&](T* newdata, xyu::size_t& i){ place_init(newdata + i++, xyu::forward<Args>(args)...); });
            return *this;
        }

        /// 尾部增加元素 (初始化列表)
        Vector& append(std::initializer_list<T> il) { return append(xyu::make_range(il)); }

        /* 插入 */

        /**
         * @brief 中间插入元素 (多元素)
         * @note 将每一个 args 作为一个元素依次插入到 index 位置
         * @note 若 index 越界，则在尾部插入
         */
        template <typename Tag = void, typename... Args, xyu::t_enable<sizeof...(Args) && !xyu::t_is_same<Tag, xyu::special_t> && xyu::t_can_init<T, xyu::t_args_get<xyu::typelist_c<Args..., void>, 0>>, bool> = true>
        Vector& insert(xyu::size_t index, Args&&... args)
        {
            static_assert(xyu::t_is_same<Tag, void>);
            static_assert((... && xyu::t_can_init<T, Args>));
            static_assert(sizeof...(args) <= limit());
            insert_help<(... && xyu::t_can_nothrow_init<T, Args>)>(index, sizeof...(Args),
                                 [&]{ (..., place_init(data + index++, xyu::forward<Args>(args))); },
                                 [&](T* newdata, xyu::size_t& i){ (..., place_init(newdata + i++, xyu::forward<Args>(args))); });
            return *this;
        }

        /**
         * @brief 中间插入元素 (范围)
         * @note 将 range 中的每一个元素依次插入到 index 位置
         * @note 若 index 越界，则在尾部插入
         */
        template <typename Tag = void, typename Rg, xyu::t_enable<!xyu::t_is_same<Tag, xyu::special_t> && !xyu::t_can_init<T, Rg> && xyu::t_is_range<Rg>, bool> = true>
        Vector& insert(xyu::size_t index, Rg&& range)
        {
            static_assert(xyu::t_is_same<Tag, void>);
            check_add(range.count());
            insert_help<xyu::t_can_nothrow_init<T, decltype(*range.begin())>>(index, range.count(),
                         [&]{ for (auto&& v : range) place_init(data + index++, xyu::forward<decltype(v)>(v)); },
                         [&](T* newdata, xyu::size_t& i){ for (auto&& v : range) place_init(newdata + i++, xyu::forward<decltype(v)>(v)); });
            return *this;
        }

        /**
         * @brief 中间插入元素 (构造范围)
         * @note 将 args 构造为一个 xyu::range 后，逐元素插入到 index 位置
         * @note 若 index 越界，则在尾部插入
         */
        template <typename Tag = void, typename Arg, xyu::t_enable<!xyu::t_is_same<Tag, xyu::special_t> && !xyu::t_can_init<T, Arg> && !xyu::t_is_range<Arg>, bool> = true>
        Vector& insert(xyu::size_t index, Arg&& arg)
        {
            static_assert(xyu::t_is_same<Tag, void>);
            static_assert(xyu::t_can_make_range<Arg>);
            return insert<Tag>(index, xyu::make_range(arg));
        }

        /**
         * @brief 中间插入元素 (单元素)
         * @tparam Tag 显式指定为 xyu::special_t
         * @note 将 args 作为一个元素的构造参数依次插入到 index 位置
         * @note 若 index 越界，则在尾部插入
         */
        template <typename Tag, typename... Args, xyu::t_enable<xyu::t_is_same<Tag, xyu::special_t>, bool> = false>
        Vector& insert(xyu::size_t index, Args&&... args)
        {
            static_assert(xyu::t_can_init<T, Args...>);
            insert_help<xyu::t_can_nothrow_init<T, Args...>>(index, 1,
                    [&]{ place_init(data + index, xyu::forward<Args>(args)...); },
                    [&](T* newdata, xyu::size_t& i){ place_init(newdata + i++, xyu::forward<Args>(args)...); });
            return *this;
        }

        /// 中间插入元素 (初始化列表)
        Vector& insert(xyu::size_t index, std::initializer_list<T> il) { return insert(index, xyu::make_range(il)); }

        /* 删除 */

        /// 删除索引开始的 count(默认为1) 个元素
        Vector& erase(xyu::size_t index, xyu::size_t count = 1)
        {
            if (XY_UNLIKELY(index >= n)) return *this;
            if (n - index <= count) while (n > index) data[--n].~T();
            else {
                xyu::size_t i;
                for (i = index; i < n - count; ++i) data[i] = xyu::move(data[i + count]);
                for (; i < n; ++i) data[i].~T();
                n -= count;
            }
            return *this;
        }

        /* 范围 */

        /// 创建范围
        auto range() noexcept { return xyu::make_range(n, data, data + n); }
        /// 创建范围 (const)
        auto range() const noexcept { return xyu::make_range(n, data, data + n).crange(); }

        /* 运算符 */

        /**
         * @brief 索引访问 (可修改)
         * @note 支持负索引，即 [-count,-1] 映射到 [0,count-1]
         * @note (-∞,-count) 与 [count,+∞) 则 映射到 两端 即 0 和 count-1
         */
        T& operator[](xyu::diff_t index) XY_NOEXCEPT_NDEBUG
        {
            auto i = static_cast<xyu::size_t>(index);
            if (index >= 0) return get(i >= n ? n - 1 : i);
            else return get(XY_UNLIKELY((i += n) >= n) ? 0 : i);
        }
        /**
         * @brief 索引访问 (不可修改)
         * @note 支持负索引，即 [-count,-1] 映射到 [0,count-1]
         * @note (-∞,-count) 与 [count,+∞) 则 映射到 两端 即 0 和 count-1
         */
        const T& operator[](xyu::diff_t index) const XY_NOEXCEPT_NDEBUG
        { return const_cast<Vector*>(this)->operator[](index); }

        /// 尾部添加元素
        template <typename Arg>
        Vector& operator<<(Arg&& arg) { return append(xyu::forward<Arg>(arg)); }

    private:
        // 就地构造新元素
        template <typename... Args>
        void place_init(void* p, Args&&... args) noexcept(xyu::t_can_nothrow_init<T, Args...>)
        {
            if constexpr (xyu::t_is_aggregate<T>) ::new (p) T{xyu::forward<Args>(args)...};
            else ::new (p) T(xyu::forward<Args>(args)...);
        }

        // 检查增加的元素数量
        static void check_add(xyu::size_t count)
        {
            if (XY_UNLIKELY(count > limit())) {
                xyloge(false, "E_Memory_Capacity: add {} over limit {}", count, limit());
                throw xyu::E_Memory_Capacity{};
            }
        }
        // 计算新容量
        xyu::size_t calc_new_capa(xyu::size_t mincapa) const
        {
            if (XY_UNLIKELY(mincapa > limit())) {
                xyloge(false, "E_Memory_Capacity: capacity {} over limit {}", capa, limit());
                throw xyu::E_Memory_Capacity{};
            }
            return xyu::max(mincapa, static_cast<xyu::size_t>(capa * K_grow_factor));
        }
        // 重新分配内存
        void realloc_capa(xyu::size_t newcapa)
        {
            // 重新分配内存
            T* newdata = xyu::alloc<T>(newcapa);
            // 资源转移
            if constexpr (xyu::t_can_nothrow_mvconstr<T>) {
                for (xyu::size_t i = 0; i < n; ++i) {
                    ::new (newdata + i) T{xyu::move(data[i])};
                    data[i].~T();
                }
            } else {
                xyu::size_t i = 0;
                try { for (; i < n; ++i) ::new (newdata + i) T{xyu::move(data[i])}; }
                catch (...) {
                    for (; i > 0;) newdata[--i].~T();
                    xyu::dealloc<T>(newdata, newcapa);
                    throw;
                }
                for (i = 0; i < n; ++i) data[i].~T();
            }
            // 释放旧内存
            xyu::dealloc<T>(data, capa);
            // 更新状态
            data = newdata;
            capa = newcapa;
        }

        // append 辅助函数
        // 扩容时，先构造新元素，再释放旧元素，保证插入容器内元素时不会因扩容而失效
        template <bool nothrow_init, typename AppFun, typename ReFun>
        void append_help(xyu::size_t newn, AppFun af, ReFun rf)
        {
            if (newn <= capa) af();
            else {
                xyu::size_t newcapa = calc_new_capa(newn);
                T* newdata = xyu::alloc<T>(newcapa);
                xyu::size_t i = n;
                // 构造新元素
                if constexpr (nothrow_init) rf(newdata, i);
                else try { rf(newdata, i); }
                    catch (...) {
                        --i; while (i > n) newdata[--i].~T();
                        xyu::dealloc<T>(newdata, newcapa);
                        throw;
                    }
                // 移动旧元素
                if constexpr (xyu::t_can_nothrow_mvconstr<T>)
                    for (i = 0; i < n; ++i) {
                        ::new (newdata + i) T{xyu::move(data[i])};
                        data[i].~T();
                    }
                else {
                    try { for (i = 0; i < n; ++i) ::new (newdata + i) T{xyu::move(data[i])}; }
                    catch (...) {
                        for (; i > 0;) newdata[--i].~T();
                        for (i = newn; i > n;) newdata[--i].~T();
                        xyu::dealloc<T>(newdata, newcapa);
                        throw;
                    }
                    for (i = 0; i < n; ++i) data[i].~T();
                }
                // 释放旧内存
                xyu::dealloc<T>(data, capa);
                // 更新状态
                n = newn;
                data = newdata;
                capa = newcapa;
            }
        }

        // insert 辅助函数
        // 扩容时，先构造新元素，再释放旧元素，保证插入容器内元素时不会因扩容而失效
        template <bool nothrow_init, typename InsFun, typename ReFun>
        void insert_help(xyu::size_t index, xyu::size_t count, InsFun sf, ReFun rf)
        {
            if (XY_UNLIKELY(index >= n)) index = n;
            xyu::size_t newn = n + count;
            // 容量足够
            if (newn <= capa)
            {
                // 移动构造
                xyu::size_t i = n, j = n - xyu::min(count, n - index);
                if constexpr (xyu::t_can_nothrow_mvconstr<T>)
                    for (; i > j;) --i, ::new (data + i + count) T{xyu::move(data[i])};
                else try { for (; i > j;) --i, ::new (data + i + count) T{xyu::move(data[i])}; }
                    catch (...) {
                        for (++i; i < n; ++i) data[i].~T();
                        throw;
                    }
                // 移动赋值
                if constexpr (xyu::t_can_nothrow_mvassign<T>)
                    for (; i > index;) --i, data[i + count] = xyu::move(data[i]);
                else try { for (; i > index;) --i, data[i + count] = xyu::move(data[i]); }
                    catch (...) {
                        for (; j < n; ++j) data[j].~T();
                        throw;
                    }
                n = newn;
                // 新元素赋值
                sf();
            }
            // 容量不足
            else
            {
                // 重新分配内存
                xyu::size_t newcapa = calc_new_capa(newn);
                T* newdata = xyu::alloc<T>(newcapa);
                // 构造新元素
                xyu::size_t i = index;
                if constexpr (nothrow_init) rf(newdata, i);
                else try { rf(newdata, i); }
                    catch (...) {
                        --i; while (i > index) newdata[--i].~T();
                        xyu::dealloc<T>(newdata, newcapa);
                        throw;
                    }
                // 移动旧元素
                i = 0;
                if constexpr (xyu::t_can_nothrow_mvconstr<T>) {
                    for (; i < index; ++i) {
                        ::new (newdata + i) T{xyu::move(data[i])};
                        data[i].~T();
                    }
                    for (; i < n; ++i) {
                        ::new (newdata + i + count) T{xyu::move(data[i])};
                        data[i].~T();
                    }
                } else {
                    try {
                        for (; i < index; ++i) ::new (newdata + i) T{xyu::move(data[i])};
                        for (; i < n; ++i) ::new (newdata + i + count) T{xyu::move(data[i])};
                    } catch (...) {
                        for (; i > index;) newdata[--i + count].~T();
                        for (; i > 0;) newdata[--i].~T();
                        for (i = index + count; i > index;) newdata[--i].~T();
                        xyu::dealloc<T>(newdata, newcapa);
                        throw;
                    }
                    for (i = 0; i < n; ++i) data[i].~T();
                }
                // 释放旧内存
                xyu::dealloc<T>(data, capa);
                // 更新状态
                n = newn;
                data = newdata;
                capa = newcapa;
            }
        }

    private:
        static_assert(K_grow_factor >= 1);
        static_assert(K_shrink_factor <= 1 && K_shrink_factor > 0);
    };
}

#pragma clang diagnostic pop