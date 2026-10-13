#pragma once

/**
 * @file    xycoll/array.h
 * @brief   原生数组封装
 * @date    2026-04-22
 */

#include "./__/tool.h"
#include "../xytraits/seq.h"
#include "../xystd/structbind.h"

/// 原生数组
namespace xylu::xycoll
{
    /**
     * @brief 原生数组
     * @tparam T  元素类型
     * @tparam N  数组元素个数
     *
     * @details
     *   存储方式：栈数组，编译期固定容量
     *
     *   方法简述：
     *   - 支持运行期索引 `get(index)` 与 编译期索引 `get˂index˃()` 访问，
     *     其中 `get˂index˃()` 会自动转发 && 属性 (为支持结构化绑定)
     *   - 支持批量操作 `set` 通过 多个值/范围 更新，`fill` 单元素数组填充
     *   - 支持 结构化绑定 及 隐式转换为 数组引用
     *
     *   注意事项：
     *   - 索引越界：`get`、`op[]`、`set`更新多个值 会在 !XY_RELEASE 下检查并触发 abort，其余方法无害
     *
     *   异常安全：
     *   - 复制赋值、`update`、`set`：若元素赋值抛出异常，数组将部分更新，无回滚
     */
    template <typename T, xyu::size_t N>
    class Array
    {
        // 原生数组
        T arr[N];

    public:
        /* 构造 */

        /// 任意构造
        template <typename... Args, typename = xyu::t_enable<sizeof...(Args) <= N && (... && xyu::t_can_init<T, Args>)>>
        constexpr explicit Array(Args&&... args) noexcept((... && xyu::t_can_nothrow_init<T, Args>) && (sizeof...(Args) == N || xyu::t_can_nothrow_dfconstr<T>))
        : arr{xyu::forward<Args>(args)...} {}

        /// 值填充构造
        constexpr Array(xyu::special_t, const T& val) noexcept(xyu::t_can_nothrow_cpconstr<T>)
        : Array(xyu::make_isequence<N>(), val) {}

        /* 读取 */

        /// 获取数组大小
        static constexpr xyu::size_t count() noexcept { return N; }

        /// 获取数组指针
        constexpr auto data() noexcept -> T(&)[N] { return arr; }
        /// 获取数组指针
        constexpr auto data() const noexcept -> xyu::t_add_const<T>(&)[N] { return arr; }

        /// 获取元素 (左值)
        template <xyu::size_t index>
        constexpr T& get() & noexcept
        {
            static_assert(index < N);
            return arr[index];
        }
        /// 获取元素 (右值)
        template <xyu::size_t index>
        constexpr T&& get() && noexcept
        {
            static_assert(index < N);
            return xyu::move(arr[index]);
        }
        /// 获取元素 (const)
        template <xyu::size_t index>
        constexpr xyu::t_add_const<T>& get() const& noexcept
        {
            static_assert(index < N);
            return arr[index];
        }

        /// 获取元素
        constexpr T& get(xyu::size_t index) noexcept
        {
            __::Checker::check_index(index, N, XY_CurPos("Array::get"));
            return arr[index];
        }
        /// 获取元素 (const)
        constexpr xyu::t_add_const<T>& get(xyu::size_t index) const noexcept
        {
            __::Checker::check_index(index, N, XY_CurPos("Array::get"));
            return arr[index];
        }

        /// 索引访问
        constexpr T& operator[](xyu::size_t index) noexcept { return get(index); }
        /// 索引访问
        constexpr xyu::t_add_const<T>& operator[](xyu::size_t index) const noexcept { return get(index); }

        /* 修改 */

        /// 直接修改
        template <typename... Args, xyu::t_enable<xyu::t_can_assign<T, Args...>, int> = 0>
        constexpr Array& update(xyu::size_t index, Args&&... args) noexcept(xyu::t_can_nothrow_assign<T, Args...>)
        {
            __::Checker::check_index(index, N, XY_CurPos("Array::update"));
            if constexpr (!XY_RELEASE) if (XY_UNLIKELY(N - index < sizeof...(args)))
            {
                if (!xyu::t_constant()) xyloge3("too many arguments for update");
                xyu::abort(xyu::native_v);
            }
            (..., (arr[index++] = xyu::forward<Args>(args)));
            return *this;
        }

        /// 修改 (范围)
        template <typename Rg, typename = xyu::t_enable<xyu::t_can_range<Rg> && xyu::t_can_assign<T, xyu::t_access<Rg>>>>
        constexpr Array& update(xyu::size_t index, Rg&& rg) noexcept(xyu::t_can_nothrow_assign<T, xyu::t_noexcept_access<Rg>>)
        {
            __::Checker::check_index(index, N, XY_CurPos("Array::update"));
            auto range = xyu::make_range(xyu::forward<Rg>(rg));
            auto b = range.begin(), e = range.end();
            for (xyu::size_t i = index; i < N && b != e; ++i, ++b) arr[i] = *b;
            return *this;
        }

        /// 修改 (初始化列表)
        constexpr Array& update(xyu::size_t index, std::initializer_list<T> il) noexcept(xyu::t_can_nothrow_cpassign<T>)
        { return update(index, xyu::make_range(il)); }

        /// 填充
        template <typename Arg>
        constexpr Array& set(const Arg& arg) noexcept(xyu::t_can_nothrow_assign<T, const Arg&>) { return set(0, N, arg); }

        /// 部分填充
        template <typename Arg>
        constexpr Array& set(const Arg& arg, xyu::size_t start) noexcept(xyu::t_can_nothrow_assign<T, const Arg&>) { return set(start, N, arg); }

        /// 部分填充
        template <typename Arg>
        constexpr Array& set(xyu::size_t start, xyu::size_t end, const Arg& arg) noexcept(xyu::t_can_nothrow_assign<T, const Arg&>)
        {
            if constexpr (!XY_RELEASE) if (XY_UNLIKELY(end > N && start < end))
                xyu::E_Logic::abort_out_range(XY_CurPos("Array::set"), end - 1, 0, N);
            for (xyu::size_t i = start; i < end; ++i) arr[i] = arg;
            return *this;
        }

        /* 转换 */

    private:
        using ArrRef = T(&)[N];
        using ArrCRef = xyu::t_add_const<T>(&)[N];
    public:
        /// 转换为数组引用
        constexpr operator ArrRef() noexcept { return arr; }
        /// 转换为数组引用 (const)
        constexpr operator ArrCRef() const noexcept { return arr; }

        /* 范围 */

        /// 创建范围
        constexpr auto range() noexcept { return xyu::Range<xyu::Iterator_Ptr<T,Array>>(N, arr, arr + N); }
        /// 创建范围 (const)
        constexpr auto range() const noexcept { return const_cast<Array*>(this)->range().crange(); }

    private:
        // 值填充构造辅助
        template <xyu::size_t... indexs>
        constexpr Array(xyu::isequence_c<indexs...>, const T& val) : arr{((void)indexs, val)...} {}
    };

    // 推导指引
    template <typename U, typename... T>
    Array(U, T...) -> Array<U, sizeof...(T) + 1>;
}

/// 格式化
namespace xylu::xystring
{
    // 转发给数组格式化
    template <typename T, xyu::size_t N>
    struct FormatImpl<xyu::Array<T,N>> : FormatImpl<xyu::typelist_c<xyu::Array<T,N>, T[N]>>
    {
        static auto transform(const xyu::Array<T,N>& array) noexcept -> xyu::t_add_const<T>(&)[N] { return array.data(); }
    };
}

/// 结构化绑定
namespace std
{
    //元素容量
    template <typename T, xyu::size_t N>
    struct tuple_size<xyu::Array<T,N>> : xyu::size_c<N> {};

    //元素类型
    template <xyu::size_t index, typename T, xyu::size_t N>
    struct tuple_element<index, xyu::Array<T,N>> { using type = T; };
}

/// 静态数组
namespace xylu::xycoll
{
    /**
     * @brief 静态数组
     * @tparam T    元素类型
     * @tparam Capa 最大容量
     *
     * @details
     *   存储方式：栈数组，编译期固定容量，连续存储
     *
     *   方法简述：
     *   - 支持 `reduce`、`resize` 整体 缩减 或 扩展，`clear` 清空元素
     *   - 支持 `append`O(1)、`insert`O(n) 插入 多元素/范围/单元素，`erase`O(n) 删除元素
     *
     *   注意事项：
     *   - 容量限制：增加元素超过 Capa 时会触发 abort
     *   - 索引越界：`get`、`op[]` 会在 !XY_RELEASE 下检查并触发 abort，其余方法无害
     *   - 自引用：`insert` 待插入元素为插入点之后的元素引用时 XY_DEBUG 下会检查并触发 abort
     *   - 迭代器失效：`insert` 插入后会使 插入位置及之后的迭代器失效
     *
     *   异常安全：
     *   - 复制/移动赋值：若元素赋值可能抛出异常，先清空再插入，异常时容器为空；否则就地移动或构造
     *   - `append`、`resize`扩展：若元素构造抛出异常，已成功构造的元素保留，需要手动回滚或继续尝试
     *   - `insert`：抛出异常时尝试回滚至操作前状态，若元素移动本身可能抛出异常，回滚可能失败，容器状态无法确定
     */
    template <typename T, xyu::size_t Capa>
    struct ArrayList
    {
        static_assert(Capa <= xyu::t_number<xyu::size_t>::max / xyu::max(2, sizeof(T)));
        static_assert(xyu::t_can_nothrow_destruct<T>, "ArrayList is cowardly");

    private:
        xyu::size_t n = 0;                                  // 元素数量
        alignas(alignof(T)) char buf[sizeof(T) * Capa];     // 数据

    public:
        /* 构造赋值 */

        /// 析构
        ~ArrayList() noexcept { clear(); }

        /// 默认构造
        ArrayList() noexcept {}

        /// 范围构造
        template <typename Rg, typename = xyu::t_enable<xyu::t_can_init<T, xyu::t_access<Rg>> && !xyu::t_can_init<T, Rg>>>
        explicit ArrayList(Rg&& range) noexcept(noexcept(append(xyu::forward<Rg>(range)))) { insert(0, xyu::forward<Rg>(range)); }

        /* 复制移动 */

        /// 类复制构造
        template <xyu::size_t Capa2>
        ArrayList(const ArrayList<T,Capa2>& other) noexcept(noexcept(append(other))) { insert(0, other.range()); }

        /// 类移动构造
        template <xyu::size_t Capa2>
        ArrayList(ArrayList<T,Capa2>&& other) noexcept(xyu::t_can_nothrow_mvconstr<T>) { insert(0, other.range().mrange()); }

        /// 类复制赋值
        template <xyu::size_t Capa2>
        ArrayList& operator=(const ArrayList<T,Capa2>& other) noexcept(xyu::t_can_nothrow_cpconstr<T>)
        {
            if (XY_UNLIKELY(this == &other)) return *this;
            if constexpr (xyu::t_can_nothrow_cpassign<T>)
            {
                bool app = n < other.n;
                for (xyu::size_t i = 0, j = app ? n : other.n; i < j; ++i) data()[i] = other.data()[i];
                if (app) for (;n < other.n; ++n) xyu::place_init(data() + n, other.data()[n]);
                else { for (auto i = other.n; i < n; ++i) data()[i].~T(); n = other.n; }
            }
            else
            {
                clear();
                insert(0, other.range());
            }
            return *this;
        }

        /// 类移动赋值
        template <xyu::size_t Capa2>
        ArrayList& operator=(ArrayList<T,Capa2>&& other) noexcept(xyu::t_can_nothrow_mvconstr<T>)
        {
            if (XY_UNLIKELY(this == &other)) return *this;
            if constexpr (xyu::t_can_nothrow_mvassign<T>)
            {
                bool app = n < other.n;
                for (xyu::size_t i = 0, j = app ? n : other.n; i < j; ++i) data()[i] = xyu::move(other.data()[i]);
                if (app) for (;n < other.n; ++n) xyu::place_init(data() + n, xyu::move(other.data()[n]));
                else { for (auto i = other.n; i < n; ++i) data()[i].~T(); n = other.n; }
            }
            else
            {
                clear();
                insert(0, other.range().mrange());
            }
            return *this;
        }

        /* 数据容量 */

        /// 获取当前容量
        static constexpr xyu::size_t capacity() noexcept { return Capa; }

        /// 获取元素数量
        xyu::size_t count() const noexcept { return n; }
        /// 是否为空
        bool empty() const noexcept { return n == 0; }

        /* 数据获取 */

        /// 获取元素
        T& get(xyu::size_t index) noexcept
        {
            __::Checker::check_index(index, n, XY_CurPos("ArrayList::get"));
            return data()[index];
        }
        /// 获取元素 (const)
        xyu::t_add_const<T>& get(xyu::size_t index) const noexcept
        {
            __::Checker::check_index(index, n, XY_CurPos("ArrayList::get"));
            return data()[index];
        }

        /* 数据管理 */

        /// 缩减元素数量
        void reduce(xyu::size_t newn) noexcept
        {
            if (XY_UNLIKELY(newn >= n)) return;
            xyu::object_destroy(data() + newn, data() + n);
            n = newn;
        }

        /// 调整元素数量
        void resize(xyu::size_t newn, const T& value) noexcept(xyu::t_can_nothrow_cpconstr<T>)
        {
            if (newn <= n) reduce(newn);
            else
            {
                if (XY_UNLIKELY(newn > capacity()))
                    xyu::E_Logic::abort_over_limit(XY_CurPos("ArrayList::resize"), n, newn - n, capacity(), "newcapa");
                for (; n<newn; ++n) ::new (data() + n) T(value);
            }
        }

        /// 清空元素
        void clear() noexcept
        {
            xyu::object_destroy(data(), n);
            n = 0;
        }

        /* 尾增 */

        /**
         * @brief 尾部增加元素 (多元素)
         * @details 将每一个 args 作为一个元素添加到尾部
         */
        template <typename... Args, typename = xyu::t_enable<sizeof...(Args) && (... && xyu::t_can_init<T, Args>)>>
        ArrayList& append(Args&&... args) noexcept((... && xyu::t_can_nothrow_init<T, Args>))
        {
            static_assert(sizeof...(args) <= capacity());
            __::Checker::check_capa<Capa>(n, sizeof...(args), XY_CurPos("ArrayList::append"));
            (..., (xyu::place_init(data() + n, xyu::forward<Args, xyu::t_can_nothrow_init<T, Args>>(args)), ++n));
            return *this;
        }

        /**
         * @brief 尾部增加元素 (范围)
         * @details 将 range 中的每一个元素添加到尾部
         */
        template <typename Rg, typename = xyu::t_enable<xyu::t_can_init<T, xyu::t_access<Rg>> && !xyu::t_can_init<T, Rg>>,
                  bool nothrow_init = xyu::t_can_nothrow_init<T, xyu::t_noexcept_access<Rg>>>
        ArrayList& append(Rg&& rg) noexcept(nothrow_init)
        {
            auto range = xyu::make_range(xyu::forward<Rg>(rg));
            __::Checker::check_range(range, XY_CurPos("ArrayList::append"));
            __::Checker::check_capa<Capa>(n, range.count(), XY_CurPos("ArrayList::append"));
            for (auto&& v : range) xyu::place_init(data() + n, xyu::forward<decltype(v), nothrow_init>(v)), ++n;
            return *this;
        }

        /**
         * @brief 尾部增加元素 (单元素)
         * @details 将 args 作为一个元素的构造参数添加到尾部
         */
        template <typename... Args, typename = xyu::t_enable<xyu::t_can_init<T, Args...>>>
        ArrayList& append(xyu::special_t = {}, Args&&... args) noexcept(xyu::t_can_nothrow_init<T, Args...>)
        {
            __::Checker::check_capa<Capa>(n, 1, XY_CurPos("ArrayList::append"));
            xyu::place_init(data() + n, xyu::forward<Args, xyu::t_can_nothrow_init<T, Args...>>(args)...);
            ++n;
            return *this;
        }

        /// 尾部增加元素 (初始化列表)
        ArrayList& append(std::initializer_list<T> il) noexcept(noexcept(append(xyu::make_range(il))))
        { return append(xyu::make_range(il)); }

        /* 插入 */

        /**
         * @brief 中间插入元素 (多元素)
         * @details 将每一个 args 作为一个元素依次插入到 index 位置 (若 index 越界，则在尾部插入)
         * @attention 禁止插入 index 位置后的自引用
         */
        template <typename... Args, typename = xyu::t_enable<sizeof...(Args) && (... && xyu::t_can_init<T, Args>)>,
                  bool nothrow_init = (... && xyu::t_can_nothrow_init<T, Args>), bool nothrow_assign = (... && (xyu::t_can_nothrow_assign<T, Args>))>
        ArrayList& insert(xyu::size_t index, Args&&... args) noexcept(nothrow_init && nothrow_assign && Nmc() && Nma())
        {
            static_assert(sizeof...(args) <= capacity());
            if (XY_UNLIKELY(index >= n)) return append_help<nothrow_init>(xyu::forward<Args, nothrow_init>(args)...);
            __::Checker::check_capa<Capa>(n, sizeof...(args), XY_CurPos("ArrayList::insert"));
            auto ordi = index;
            insert_help<nothrow_assign>(
                    index, sizeof...(args),
                    [&]{ (..., (data()[index++] = __::Checker::check_alias(xyu::forward<Args, nothrow_assign>(args), ordi, data(), n, XY_CurPos("ArrayList::insert")))); });
            return *this;
        }

        /**
         * @brief 中间插入元素 (范围)
         * @details 将 range 中的每一个元素依次插入到 index 位置 (若 index 越界，则在尾部插入)
         * @attention 禁止插入 index 位置后的自引用
         */
        template <typename Rg, typename = xyu::t_enable<xyu::t_can_init<T, xyu::t_access<Rg>> && !xyu::t_can_init<T, Rg>>,
                  bool nothrow_init = xyu::t_can_nothrow_init<T, xyu::t_noexcept_access<Rg>>, bool nothrow_assign = xyu::t_can_nothrow_assign<T&, xyu::t_noexcept_access<Rg>>>
        ArrayList& insert(xyu::size_t index, Rg&& rg) noexcept(nothrow_init && nothrow_assign && Nmc() && Nma())
        {
            if (XY_UNLIKELY(index >= n)) return append_help<nothrow_init>(xyu::forward<Rg, nothrow_init>(rg));
            auto ordi = index;
            auto range = xyu::make_range(xyu::forward<Rg>(rg));
            __::Checker::check_range(range, XY_CurPos("ArrayList::insert"));
            __::Checker::check_capa<Capa>(n, range.count(), __FILE__, __LINE__, "ArrayList::insert");
            insert_help<nothrow_assign>(
                    index, range.count(),
                    [&]{ for (auto&& v : range) data()[index++] = __::Checker::check_alias(xyu::forward<decltype(v), nothrow_assign>(v), ordi, data(), n, XY_CurPos("ArrayList::insert")); });
            return *this;
        }

        /**
         * @brief 中间插入元素 (单元素)
         * @details 将 args 作为一个元素的构造参数依次插入到 index 位置 (若 index 越界，则在尾部插入)
         * @attention 禁止插入 index 位置后的自引用
         */
        template <typename... Args, typename = xyu::t_enable<xyu::t_can_init<T, Args...>>,
                  bool nothrow_init = xyu::t_can_nothrow_init<T, Args...>, bool nothrow_assign = (sizeof...(Args) == 1 && xyu::t_can_assign<T, Args...>) ? xyu::t_can_nothrow_assign<T, Args...> : (xyu::t_can_nothrow_init<T, Args...> && xyu::t_can_nothrow_mvassign<T>)>
        ArrayList& insert(xyu::size_t index, xyu::special_t = {}, Args&&... args) noexcept(nothrow_init && nothrow_assign && Nmc() && Nma())
        {
            if (XY_UNLIKELY(index >= n)) return append_help<true>(xyu::special_v, xyu::forward<Args, nothrow_init>(args)...);
            auto ordi = index;
            __::Checker::check_capa<Capa>(n, sizeof...(args), XY_CurPos("ArrayList::insert"));
            insert_help<nothrow_assign>(
                    index, 1,
                    [&]{
                        if constexpr (sizeof...(Args) == 1 && xyu::t_can_assign<T, Args...>) data()[index] = (..., __::Checker::check_alias(xyu::forward<Args, nothrow_assign>(args), ordi, data(), n, XY_CurPos("ArrayList::insert")));
                        else data()[index] = xyu::object_create<T>(__::Checker::check_alias(xyu::forward<Args, nothrow_assign>(args), ordi, data(), n, XY_CurPos("ArrayList::insert"))...);
                    });
            return *this;
        }

        /**
         * @brief 中间插入元素 (初始化列表)
         * @attention 禁止插入 index 位置后的自引用
         */
        ArrayList& insert(xyu::size_t index, std::initializer_list<T> il) noexcept(noexcept(insert(index, xyu::make_range(il))))
        { return insert(index, xyu::make_range(il)); }

        /* 删除 */

        /// 删除索引开始的 count(默认为1) 个元素
        ArrayList& erase(xyu::size_t index, xyu::size_t count = 1) noexcept(xyu::t_can_nothrow_mvassign<T>)
        {
            if (XY_UNLIKELY(index >= n)) return *this;
            if (n - index <= count)
            {
                xyu::object_destroy(data() + index, data() + n);
                n = index;
            }
            else if constexpr (xyu::t_can_nothrow_mvassign<T>)
            {
                xyu::size_t i;
                for (i = index; i < n - count; ++i) data()[i] = xyu::move(data()[i + count]);
                xyu::object_destroy(data() + i, data() + n);
                n -= count;
            }
            return *this;
        }

        /* 范围 */

        /// 创建范围
        auto range() noexcept { return xyu::Range<xyu::Iterator_Ptr<T,ArrayList>>(n, data(), data() + n); }
        /// 创建范围 (const)
        auto range() const noexcept { return const_cast<ArrayList*>(this)->range().crange(); }

        /* 运算符 */

        /// 索引访问
        T& operator[](xyu::size_t index) noexcept { return get(index); }
        /// 索引访问 (const)
        xyu::t_add_const<T>& operator[](xyu::size_t index) const noexcept { return get(index); }

        /// 尾部添加元素
        template <typename Arg>
        ArrayList& operator<<(Arg&& arg) noexcept(noexcept(append(xyu::forward<Arg>(arg))))
        { return append(xyu::forward<Arg>(arg)); }

    private:
        // 获取数组
        T* data() noexcept { return (T*)buf; }
        // 获取数组 (const)
        xyu::t_add_const<T>* data() const noexcept { return (T*)buf; }

        // 无异常移动构造
        static constexpr bool Nmc() noexcept { return xyu::t_can_nothrow_mvconstr<T>; }
        // 无异常移动赋值
        static constexpr bool Nma() noexcept { return xyu::t_can_nothrow_mvassign<T>; }

        // insert 转发 append 辅助
        template <bool safe_call, typename... Args>
        ArrayList& append_help(Args&&... args)
        {
            if constexpr (safe_call) return append(xyu::forward<Args>(args)...);
            else
            {
                xyu::size_t tmp = n;
                try { append(xyu::forward<Args>(args)...); }
                catch (...) { while (n > tmp) data()[--n].~T(); throw; }
                return *this;
            }
        }

        // insert 辅助
        template <bool nothrow_assign, typename InsFun>
        void insert_help(xyu::size_t index, xyu::size_t count, InsFun sf)
        {
            xyu::size_t newn = n + count;
            // 移动构造
            xyu::size_t i, l = n - xyu::min(count, n - index);
            if constexpr (xyu::t_can_nothrow_mvconstr<T>)
                for (i = l; i < n; ++i) ::new (data() + i + count) T{xyu::move(data()[i])};
            else try { for (i = l; i < n; ++i) ::new (data() + i + count) T{data()[i]}; }
                catch (...) { for (; l < i; ++l) data()[l + count].~T(); throw; }
            // 移动赋值
            if constexpr (xyu::t_can_nothrow_mvassign<T>)
                for (i = l; i > index;) --i, data()[i + count] = xyu::move(data()[i]);
            else try { for (i = l; i > index;) --i, data()[i + count] = xyu::move(data()[i]); }
                catch (...) {
                    try { for (++i; i < l; ++i) data()[i] = xyu::move(data()[i + count]);  }
                    catch (...) {}
                    for (; l < n; ++l) data()[l + count].~T();
                    throw;
                }
            n = newn;
            // 新元素赋值
            if constexpr (nothrow_assign) sf();
            else try { sf(); }
                catch (...) {
                    n -= count;
                    if constexpr (xyu::t_can_nothrow_mvassign<T>)
                        for (i = index; i < n; ++i) data()[i] = xyu::move(data()[i + count]);
                    else try { for (i = index; i < n; ++i) data()[i] = xyu::move(data()[i + count]); }
                        catch (...) {}
                    for (i = l; i < n; ++i) data()[i + count].~T();
                    throw;
                }
        }
    };

}
