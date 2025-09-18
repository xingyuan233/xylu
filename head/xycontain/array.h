#pragma clang diagnostic push
#pragma ide diagnostic ignored "OCUnusedGlobalDeclarationInspection"
#pragma ide diagnostic ignored "cert-dcl58-cpp"
#pragma ide diagnostic ignored "hicpp-exception-baseclass"
#pragma once

#include "../../link/format"

/// 固定数组
namespace xylu::xycontain
{
    /**
     * @brief 一个功能增强的、零开销的固定大小数组容器。
     *
     * @tparam T 存储的元素类型。
     * @tparam N 数组的大小，必须是一个编译期常量。
     *
     * @details
     * `xyu::Array` 是对 C-style 数组和 `std::array` 的一次概念性升级。它在保持聚合体
     * (Aggregate) 特性、与C语言兼容以及零运行时开销的核心优势的同时，深度集成了 `xylu`
     * 库的现代化特性，提供了更强大、更安全、更便捷的接口。
     *
     * ### 核心亮点 (Key Features):
     *
     * 1.  **零开销抽象 (Zero-Cost Abstraction):**
     *     - 内部仅包含一个原生数组 `T arr[N]`，`sizeof(xyu::Array˂T, N˃)` 等同于
     *       `sizeof(T[N])`。所有操作都尽可能在编译期解析，确保了与原生数组相同的性能。
     *
     * 2.  **与范围生态无缝融合 (Seamless Range Integration):**
     *     - `range()` 方法返回一个 `xyu::Range` 对象，使 `Array` 可以直接、高效地用于
     *       范围for循环 (`for (auto& item : my_array.range())`)，并与所有 `xylu`
     *       范围算法和适配器无缝协作。
     *
     * 3.  **强大的泛型接口 (Powerful Generic API):**
     *     - **结构化绑定:** 原生支持结构化绑定 (`auto [a, b, c] = my_array;`)，
     *       简化了对元素的解构访问。
     *     - **格式化支持:** 通过 `Formatter` 特化，可以直接用于 `xyfmt` 和 `xylog`，
     *       并能通过格式说明符精确控制每个元素的输出格式和分隔符。
     *
     * 4.  **更友好的用户体验 (Enhanced Ergonomics):**
     *     - **负索引访问:** `operator[]` 支持类似 Python 的负索引（例如 `arr[-1]`
     *       访问最后一个元素），并对越界索引提供了安全的“夹逼(clamp)”行为，
     *       使其比原生数组和 `std::array` 更易用、更安全。
     *     - **灵活的赋值:** `assign` 方法被高度重载，支持通过参数包、`std::initializer_list`
     *       或任何可转换为 `xyu::Range` 的对象进行部分或全部赋值。
     *
     * 5.  **现代C++特性:**
     *     - 提供了类模板参数推导指引 (CTAD)，允许通过 `xyu::Array arr = {1, 2, 3};`
     *       进行简洁的初始化。
     *     - 广泛使用 `constexpr`，使得 `Array` 的创建和访问可以在编译期进行。
     *
     * @note `xyu::Array` 是一个聚合类型，可以使用聚合初始化 `xyu::Array˂int, 3˃ arr = {1, 2, 3};`。
     */
    template <typename T, xyu::size_t N>
    struct Array
    {
        // 原生数组
        T arr[N];

    public:
        /* 元素访问 */

        /// 获取数组大小
        static constexpr xyu::size_t count() noexcept { return N; }
        /// 获取数组指针
        constexpr T* data() noexcept { return arr; }
        /// 获取数组指针
        constexpr const T* data() const noexcept { return arr; }

        /// 获取元素 (左值)
        template <xyu::size_t index>
        constexpr T& get() & noexcept
        {
            static_assert(index < N);
            return arr[index];
        }
        /// 获取元素 (const左值)
        template <xyu::size_t index>
        constexpr const T& get() const& noexcept
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
        /// 获取元素 (const右值)
        template <xyu::size_t index>
        constexpr const T&& get() const&& noexcept
        {
            static_assert(index < N);
            return xyu::move(arr[index]);
        }

        /// 获取元素
        constexpr T& get(xyu::size_t index) XY_NOEXCEPT_NDEBUG
        {
#if XY_DEBUG
            return at(index);
#else
            return arr[index];
#endif
        }
        /// 获取元素 (const)
        constexpr const T& get(xyu::size_t index) const XY_NOEXCEPT_NDEBUG
        { return const_cast<Array*>(this)->get(index); }

        /// 获取元素 [检测范围]
        T& at(xyu::size_t index)
        {
            if (XY_UNLIKELY(index >= N)) {
                xyloge(false, "E_Logic_Out_Of_Range: index {} out of range [0, {})", index, N);
                throw xyu::E_Logic_Out_Of_Range{};
            }
            return arr[index];
        }
        /// 获取元素 (const) [检测范围]
        const T& at(xyu::size_t index) const { return const_cast<Array*>(this)->at(index); }

        /**
         * @brief 索引访问 (可修改)
         * @note 支持负索引，即 [-N,-1] 映射到 [0,N-1]
         * @note (-∞,-N) 与 [N,+∞) 则 映射到 两端 即 0 和 N-1
         */
        T& operator[](xyu::diff_t index) XY_NOEXCEPT_NDEBUG
        {
            auto i = static_cast<xyu::size_t>(index);
            if (index >= 0) return get(i >= N ? N - 1 : i);
            else return get(XY_UNLIKELY((i += N) >= N) ? 0 : i);
        }
        /**
         * @brief 索引访问 (不可修改)
         * @note 支持负索引，即 [-N,-1] 映射到 [0,N-1]
         * @note (-∞,-N) 与 [N,+∞) 则 映射到 两端 即 0 和 N-1
         */
        const T& operator[](xyu::diff_t index) const XY_NOEXCEPT_NDEBUG
        { return const_cast<Array*>(this)->operator[](index); }

        /* 修改元素 */

        /// 直接赋值
        template <typename Arg, typename... Args, xyu::t_enable<xyu::t_can_assign<T&, Arg>, int> = 0>
        Array& assign(xyu::size_t index, Arg&& arg, Args&&... args) noexcept(xyu::t_can_nothrow_assign<T&, Arg, Args...>)
        {
#if XY_DEBUG
            if (XY_UNLIKELY(index >= N)) {
                xyloge(false, "E_Logic_Out_Of_Range: index {} out of range [0, {})", index, N);
                throw xyu::E_Logic_Out_Of_Range{};
            }
            if (XY_UNLIKELY(N - index <= sizeof...(args))) {
                xyloge(false, "E_Logic_Invalid_Argument: too many arguments for assign");
                throw xyu::E_Logic_Invalid_Argument{};
            }
#endif
            arr[index] = xyu::forward<Arg>(arg);
            (..., (arr[++index] = xyu::forward<Args>(args)));
            return *this;
        }
        /// 赋值 (范围)
        template <typename Rg, xyu::t_enable<!xyu::t_can_assign<T&, Rg> && xyu::t_is_range<Rg>, int> = 1>
        Array& assign(xyu::size_t index, Rg&& range) noexcept(xyu::t_can_nothrow_assign<T&, decltype(*(xyu::t_val<Rg>().begin()))>)
        {
#if XY_DEBUG
            if (XY_UNLIKELY(index >= N)) {
                xyloge(false, "E_Logic_Out_Of_Range: index {} out of range [0, {})", index, N);
                throw xyu::E_Logic_Out_Of_Range{};
            }
#endif
            xyu::size_t s = N - index >= range.count() ? index + range.count() : N;
            auto i = range.begin();
            for (xyu::size_t j = index; j < s; ++j, ++i) arr[j] = *i;
            return *this;
        }
        /// 赋值 (构造范围)
        template <typename Arg, xyu::t_enable<!xyu::t_can_assign<T&, Arg> && !xyu::t_is_range<Arg>, int> = 2>
        Array& assign(xyu::size_t index, Arg&& arg) noexcept(noexcept(assign(index, xyu::make_range(arg))))
        { return assign(index, xyu::make_range(arg)); }
        /// 赋值 (初始化列表)
        Array& assign(xyu::size_t index, std::initializer_list<T> il) noexcept(xyu::t_can_nothrow_cpassign<T>)
        { return assign(index, xyu::make_range(il)); }

        /// 填充
        template <typename Arg>
        Array& fill(const Arg& arg) noexcept(xyu::t_can_nothrow_assign<T&, const Arg&>) { return fill(0, arg); }
        /// 填充
        template <typename Arg>
        Array& fill(xyu::size_t index, const Arg& arg) noexcept(xyu::t_can_nothrow_assign<T&, const Arg&>)
        {
            for (xyu::size_t i = index; i < N; ++i) arr[i] = arg;
            return *this;
        }

        /* 范围 */

        /// 创建范围
        constexpr auto range() noexcept { return xyu::make_range(arr); }
        /// 创建范围 (const)
        constexpr auto range() const noexcept { return xyu::make_range(arr); }
    };

    // 推导指引
    template <typename U, typename... T>
    Array(U, T...) -> Array<xyu::t_enable<xyu::t_is_same<U, T...>, U>, sizeof...(T) + 1>;
}

/// 其他属性
namespace xylu::xytraits::__
{
    // 判断是否为数组类型
    template <typename T, xyu::size_t N>
    struct is_array<xylu::xycontain::Array<T, N>> : true_c {};
}

/// 格式化
namespace xylu::xystring
{
    // 作为数组格式化
    template <typename T, xyu::size_t N>
    struct Formatter<xylu::xycontain::Array<T, N>>
    {
        using U = xylu::xycontain::Array<T, N>;

        // 运行时解析
        static constexpr bool runtime = __::is_formatter_runtime<T>;

        /// 预解析数组格式串
        constexpr static xyu::size_t prepare(const StringView& pattern, const StringView& expand) noexcept
        { return Formatter<T[N]>::prepare(pattern, expand); }

        /// 解析数组格式串
        static xyu::size_t parse(const U& arr, const StringView& pattern, const StringView& expand)
        { return Formatter<T[N]>::parse(arr.data(), pattern, expand); }

        /// 运行期预解析数组格式串
        static xyu::size_t preparse(const U& arr, const StringView& pattern, const StringView& expand)
        { return Formatter<T[N]>::preparse(arr.data(), pattern, expand); }

        /// 格式化数组
        template <typename Stream>
        static void format(Stream& out, const U& arr, const StringView& pattern, const StringView& expand)
        { Formatter<T[N]>::format(out, arr.data(), pattern, expand); }
    };
}

/// 结构化绑定
namespace std
{
    template <typename T> struct tuple_size;
    template <xyu::size_t index, typename T> struct tuple_element;

    //元素容量
    template <typename T, xyu::size_t N>
    struct tuple_size<xylu::xycontain::Array<T, N>> : xyu::size_c<N> {};
    //元素类型
    template <xyu::size_t index, typename T, xyu::size_t N>
    struct tuple_element<index, xylu::xycontain::Array<T, N>> { using type = T; };
}

#pragma clang diagnostic pop