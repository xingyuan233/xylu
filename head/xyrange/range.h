#pragma clang diagnostic push
#pragma ide diagnostic ignored "google-explicit-constructor"
#pragma ide diagnostic ignored "modernize-use-nodiscard"
#pragma ide diagnostic ignored "OCUnusedGlobalDeclarationInspection"
#pragma ide diagnostic ignored "ConstantParameter"
#pragma ide diagnostic ignored "UnreachableCallsOfFunction"
#pragma once

#include "./gen.h"
#include "../../link/initlist"

/**
 * @file
 * @brief 定义了 `Range` 类和一系列 `make_range` 工厂函数。
 * @details
 *   `Range` 是一个封装了“可遍历序列”概念的核心类，它可以被用于范围 for 循环。
 *   `make_range` 函数提供了从各种数据源（如 C 数组、STL 风格容器、指针、生成器等）便捷地创建 `Range` 对象的统一接口。
 */

/** 核心范围类 */
namespace xylu::xyrange
{
    namespace __
    {
        template <typename T>
        constexpr bool is_iter_generate = [](){
            if constexpr (!xyu::t_is_iterator<T>) return false;
            else return xyu::t_is_same<typename T::Equal_t, RangeIter_None> &&
                        xyu::t_is_same<typename T::Compare_t, RangeIter_None>;
        }();
    }

    /**
     * @brief 表示一个可遍历的序列，由一对迭代器定义。
     * @details
     *   这是一个核心的范围类，封装了序列的起点、终点和大小。
     *   它提供了 `begin()` 和 `end()` 方法，使其可以被用于 C++ 的范围 for 循环。
     *   同时，它也提供了 `crange`, `mrange`, `rrange` 等方法来方便地获取常量、移动或反向的范围视图。
     *
     * @tparam Iter 构成此范围的迭代器类型，必须是 `RangeIter` 的特化。
     */
    template <typename Iter, bool = __::is_iter_generate<xyu::t_remove_cv<Iter>>>
    struct Range
    {
        static_assert(xyu::t_is_iter_single<Iter>);

        using cIter = RangeIter_Const<Iter>;
        using mIter = RangeIter_Move<Iter>;
        using rIter = RangeIter_Reverse<Iter>;

    private:
        xyu::size_t n;      // 元素个数
        Iter bg, ed;        // 起点终点迭代器

    public:
        /**
         * @brief 构造函数
         * @note 提供完整的参数进行构造，保证性能最优
         */
        constexpr Range(xyu::size_t count, Iter bg, Iter ed) noexcept(noexcept(Iter{bg}))
            : n{count}, bg{bg}, ed{ed} {}
        /**
         * @brief 构造函数
         * @note 提供起点迭代器和元素个数进行构造，自动计算终点迭代器
         * @note 如果是非随机迭代器，计算的代价请自己负责
         */
        constexpr Range(xyu::size_t count, Iter bg) noexcept(noexcept(Iter{nextiter(bg, count)}))
            : n{count}, bg{bg}, ed{nextiter(bg, count)} {}
        /**
         * @brief 构造函数
         * @note 提供起点和终点迭代器进行构造，自动计算元素个数
         * @note 如果是非随机迭代器，计算的代价请自己负责
         */
        constexpr Range(Iter bg, Iter ed) noexcept(noexcept(Iter{bg}, distance(bg, ed)))
            : n{distance(bg, ed)}, bg{bg}, ed{ed} {}

        // 获取元素个数
        constexpr xyu::size_t count() const noexcept { return n; }
        // 判断是否为空
        constexpr bool empty() const noexcept { return n == 0; }
        // 获取范围起点迭代器
        constexpr Iter begin() const noexcept { return bg; }
        // 获取范围终点迭代器
        constexpr Iter end() const noexcept { return ed; }

        // 获取常量范围
        constexpr Range<cIter> crange() const noexcept(noexcept(cIter{bg})) { return {n, bg, ed}; }
        // 获取移动范围
        constexpr Range<mIter> mrange() const noexcept(noexcept(mIter{bg})) { return {n, bg, ed}; }
        // 获取反向范围
        constexpr Range<rIter> rrange() const noexcept(noexcept(rIter{bg})) { return {n, ed, bg}; }

    private:
        // 计算下一个迭代器
        static constexpr Iter nextiter(Iter it, xyu::size_t step) noexcept(check_nextiter(Iter{}, xyu::size_t{}))
        {
            if constexpr (xyu::t_is_iter_random<Iter>) return it + step;
            else { while (step--) ++it; return it; }
        }
        // 计算两个迭代器之间的距离
        static constexpr xyu::size_t distance(Iter s, Iter o) noexcept(check_distance(Iter{}, Iter{}))
        {
            if constexpr (xyu::t_is_iter_random<Iter>) return s - o;
            else {
                xyu::size_t dist = 0;
                while (s != o) ++s, ++dist;
                return dist;
            }
        }

    private:
        // 异常检测
        static constexpr bool check_nextiter(Iter it, xyu::size_t step) noexcept
        {
            if constexpr (xyu::t_is_iter_random<Iter>) return noexcept(it + step);
            else return noexcept(++it);
        }
        static constexpr bool check_distance(Iter s, Iter o) noexcept
        {
            if constexpr (xyu::t_is_iter_random<Iter>) return noexcept(s - o);
            else return noexcept(++s, s != o);
        }
    };

    /**
     * @brief `Range` 的一个特化版本，用于封装“生成器”。
     * @details
     *   当 `Range` 持有一个生成器而非一对迭代器时，它的 `begin()` 和 `end()` 都返回基于生成器的迭代器。
     *   迭代器通过其 `operator==` 来检查生成器的 `valid()` 状态，从而决定循环是否结束。
     *
     * @tparam Generator 生成器类型。
     * @note 对于无限序列，`count()` 的返回值是未定义的，不应使用。
     */
    template <typename Generator>
    struct Range<Generator, true>
    {
    private:
        // 比较转发
        struct RangeIter_Equal_Generator
        {
            template <typename Iter>
            constexpr bool operator()(Iter& i1, Iter&) noexcept(noexcept(static_cast<bool>(i1.i)))
            { return !static_cast<bool>(i1.i); }
        };
        // 生成器封装
        using Iter = RangeIter<RangeIter_Storage_Iter<Generator>,
                RangeIter_Valid_Native, RangeIter_None, RangeIter_Dereference_Native,
                RangeIter_Increment_Native, RangeIter_None, RangeIter_None, RangeIter_None,
                RangeIter_None, RangeIter_Equal_Generator, RangeIter_None>;

        using cIter = RangeIter_Const<Iter>;
        using mIter = RangeIter_Move<Iter>;

    private:
        xyu::size_t n;    // 元素个数 (可能无效或不准确,如对于无限序列)
        Generator gen;    // 生成器

    public:
        /**
         * @brief 构造函数
         * @note 提供生成器进行构造，元素个数默认为 -1
         */
        constexpr explicit Range(Generator g) noexcept(xyu::t_can_nothrow_construct<Generator>)
            : n{static_cast<xyu::size_t>(-1)}, gen(g) {}

        /**
         * @brief 构造函数
         * @note 提供生成器和元素个数进行构造
         */
        constexpr Range(xyu::size_t count, Generator g) noexcept(xyu::t_can_nothrow_construct<Generator>)
            : n{count}, gen(g) {}

        // 获取元素个数
        constexpr xyu::size_t count() const noexcept { return n; }
        // 判断是否为空
        constexpr bool empty() const noexcept { return !static_cast<bool>(gen); }
        // 获取范围起点迭代器
        constexpr Iter begin() const noexcept { return gen; }
        // 获取范围终点迭代器
        constexpr Iter end() const noexcept { return gen; }

        // 获取常量范围
        constexpr Range<cIter> crange() const noexcept(noexcept(cIter{begin()})) { return {n, begin(), begin()}; }
        // 获取移动范围
        constexpr Range<mIter> mrange() const noexcept(noexcept(mIter{begin()})) { return {n, begin(), begin()}; }
    };
}

/// 范围属性
namespace xylu::xytraits
{
    namespace __
    {
        template <typename T>
        struct is_range : false_c {};
        template <typename Iter>
        struct is_range<xylu::xyrange::Range<Iter>> : true_c {};
    }
    /// 判断是否为范围类型
    template <typename... T>
    constexpr bool t_is_range = (... && __::is_range<t_remove_cvref<T>>::value);
}

/** 迭代器范围创建 */
namespace xylu::xyrange
{
    /// 从 C 风格数组创建一个 `Range`。
    template <typename T, xyu::size_t N>
    constexpr auto make_range(T(&arr)[N]) noexcept { return Range<RangeIter_Ptr<T>>{N, arr, arr + N}; }

    /// 从 `std::initializer_list` 创建一个常量 `Range`。
    template <typename T>
    constexpr auto make_range(std::initializer_list<T> il) noexcept
    {
        using iter = RangeIter_Ptr<const T>;
        return Range<iter>{il.size(), il.begin(), il.end()};
    }

    namespace __
    {
        // 测试 range()
        template <typename T, typename = xyu::t_enable<xyu::t_is_range<decltype(xyu::t_val<T>().range())>>>
        xyu::true_c has_range_test(int);
        template <typename T>
        xyu::false_c has_range_test(...);
        template <typename T>
        constexpr bool has_range = decltype(has_range_test<T>(0))::value;
    }
    /**
     * @brief 从一个自定义容器创建一个 `Range` (通过 `range()` 成员函数)。
     * @details 若容器 T 提供了 `range()` 成员函数且其返回 `xylu::xyrange::Range`，则优先调用此重载。
     */
    template <typename T, xyu::t_enable<xyu::t_is_class<T> && __::has_range<T>, bool> = true>
    constexpr auto make_range(T& contain) noexcept(noexcept(contain.range())) { return contain.range(); }

    namespace __
    {
        // 测试 begin(), end()
        template <typename T>
        decltype(xyu::t_val<T>().begin(), xyu::t_val<T>().end(), xyu::true_c{}) hasn_begin_end_test(int);
        template <typename T>
        xyu::false_c hasn_begin_end_test(...);
        template <typename T>
        constexpr bool has_be = decltype(hasn_begin_end_test<T>(0))::value;

        // 测试获取元素个数函数 count()/length()/size()
        template <typename T, typename = decltype(xyu::t_val<T>().count())>
        xyu::int_c<1> has_count_test(char);
        template <typename T, typename = decltype(xyu::t_val<T>().length())>
        xyu::int_c<2> has_count_test(int);
        template <typename T, typename = decltype(xyu::t_val<T>().size())>
        xyu::int_c<3> has_count_test(long);
        template <typename T>
        xyu::int_c<0> has_count_test(...);
        template <typename T>
        constexpr int has_count = decltype(has_count_test<T>('0'))::value;

        // 异常检测
        template <typename T>
        constexpr bool make_range_contain_noexcept() noexcept
        {
            using iter_ord = decltype(xyu::t_val<T&>().begin());
            using iter_new = RangeIter_Iter<iter_ord>;
            if constexpr (__::has_count<T> == 1) return noexcept(Range<iter_new>{xyu::t_val<T&>().count(), xyu::t_val<T&>().begin(), xyu::t_val<T&>().end()});
            else if constexpr (__::has_count<T> == 2) return noexcept(Range<iter_new>{xyu::t_val<T&>().length(), xyu::t_val<T&>().begin(), xyu::t_val<T&>().end()});
            else if constexpr (__::has_count<T> == 3) return noexcept(Range<iter_new>{xyu::t_val<T&>().size(), xyu::t_val<T&>().begin(), xyu::t_val<T&>().end()});
            else return noexcept(Range<iter_new>{xyu::t_val<T&>().begin(), xyu::t_val<T&>().end()});
        }
    }
    /**
     * @brief 从一个 STL 风格的容器创建一个 `Range` (通过 `begin()`/`end()`)。
     * @details
     *   此重载会自动适配任何提供了 `begin()` 和 `end()` 成员函数的容器。
     *   它会使用 `RangeIter_Iter` 将容器的原生迭代器封装为 `RangeIter`。
     * @note 若容器还提供了 `count()`, `length()` 或 `size()`，则会用其优化范围大小的计算。
     */
    template <typename T, xyu::t_enable<xyu::t_is_class<T> && !__::has_range<T> && __::has_be<T>, bool> = false>
    constexpr auto make_range(T& contain) noexcept(__::make_range_contain_noexcept<T>())
    {
        using iter_ord = decltype(contain.begin());
        using iter_new = RangeIter_Iter<iter_ord>;
        if constexpr (__::has_count<T> == 1) return Range<iter_new>{contain.count(), contain.begin(), contain.end()};
        else if constexpr (__::has_count<T> == 2) return Range<iter_new>{contain.length(), contain.begin(), contain.end()};
        else if constexpr (__::has_count<T> == 3) return Range<iter_new>{contain.size(), contain.begin(), contain.end()};
        else return Range<iter_new>{contain.begin(), contain.end()};
    }

    namespace __
    {
        // 测试 Node 类型
        template <typename T, typename = decltype(xyu::t_val<T>().val, xyu::t_val<T>().next, xyu::t_val<T>().prev)>
        xyu::constant_c<int,1> is_node_test(int);
        template <typename T, typename = decltype(xyu::t_val<T>().val, xyu::t_val<T>().next)>
        xyu::constant_c<int,2> is_node_test(long);
        template <typename T>
        xyu::constant_c<int,0> is_node_test(...);
        template <typename T>
        constexpr int is_node = decltype(is_node_test<T>(0))::value;
    }
    /**
     * @brief 创建节点指针范围
     * @tparam Node 节点类型
     * @param count 节点个数
     * @param begin 起点节点指针
     * @param end 终点节点指针 (可选)
     * @note 节点必须包含 v[value], n[next], b[back,可选] 成员
     */
    template <typename Node, typename = xyu::t_enable<__::is_node<Node>>>
    constexpr auto make_range(xyu::size_t count, Node* begin, Node* end = nullptr) noexcept
    {
        if (end == nullptr) return Range<RangeIter_NodePtr<Node, __::is_node<Node> == 1>>{count, begin};
        else return Range<RangeIter_NodePtr<Node, __::is_node<Node> == 1>>{count, begin, end};
    }
    /**
     * @brief 创建节点指针范围
     * @tparam Node 节点类型
     * @param begin 起点节点指针
     * @param end 终点节点指针
     * @note 节点必须包含 v[value], n[next], b[back,可选] 成员
     */
    template <typename Node, typename = xyu::t_enable<static_cast<bool>(__::is_node<Node>)>>
    constexpr auto make_range(Node* begin, Node* end) noexcept
    { return Range<RangeIter_NodePtr<Node, __::is_node<Node> == 1>>{begin, end}; }

    namespace __
    {
        // 测试 Tree 类型
        template <typename T, typename = decltype(xyu::t_val<T>().val, xyu::t_val<T>().left, xyu::t_val<T>().right, xyu::t_val<T>().up)>
        xyu::true_c is_tree_test(int);
        template <typename T>
        xyu::false_c is_tree_test(long);
        template <typename T>
        constexpr bool is_tree = decltype(is_tree_test<T>(0))::value;
    }
    /**
     * @brief 创建树指针范围
     * @tparam Tree 树类型
     * @param count 树节点个数
     * @param begin 起点树指针
     * @param end 终点树指针 (可选)
     * @note 树必须包含 v[value], l[left], r[right], u[parent/up] 成员
     * @note 其存储模式应包含一个哨兵节点，其 u指向根节点，l指向最左节点，r指向最右节点
     */
    template <typename Tree, xyu::t_enable<__::is_tree<Tree>, bool> = true>
    constexpr auto make_range(xyu::size_t count, Tree* begin, Tree* end = nullptr) noexcept
    {
        if (end == nullptr) return Range<RangeIter_TreePtr<Tree>>{count, begin};
        else return Range<RangeIter_TreePtr<Tree>>{count, begin, end};
    }
    /**
     * @brief 创建树指针范围
     * @tparam Tree 树类型
     * @param begin 起点树指针
     * @param end 终点树指针
     * @note 树必须包含 v[value], l[left], r[right], u[parent/up] 成员
     * @note 其存储模式应包含一个哨兵节点，其 u指向根节点，l指向最左节点，r指向最右节点
     */
    template <typename Tree, xyu::t_enable<__::is_tree<Tree>, bool> = true>
    constexpr auto make_range(Tree* begin, Tree* end) noexcept
    { return Range<RangeIter_TreePtr<Tree>>{iter(begin), iter(end)}; }

    /**
     * @brief 创建指针范围
     * @tparam T 元素类型
     * @param count 元素个数
     * @param begin 起点指针
     * @param end 终点指针 (可选)
     */
    template <typename T, xyu::t_enable<!__::is_node<T> && !__::is_tree<T>, bool> = false>
    constexpr auto make_range(xyu::size_t count, T* begin, T* end = nullptr) noexcept
    { return Range<RangeIter_Ptr<T>>{count, begin, end ? end : begin + count}; }
    /**
     * @brief 创建指针范围
     * @tparam T 元素类型
     * @param begin 起点指针
     * @param end 终点指针
     */
    template <typename T, xyu::t_enable<!__::is_node<T> && !__::is_tree<T>, bool> = false>
    constexpr auto make_range(T* begin, T* end) noexcept
    { return Range<RangeIter_Ptr<T>>{begin, end}; }

    namespace __
    {
        // 测试能否解引用
        template <typename T, typename = xyu::t_enable<!xyu::t_is_pointer<T>, decltype(*xyu::t_val<T>())>>
        xyu::true_c is_iter_test(int);
        template <typename T>
        xyu::false_c is_iter_test(long);
        template <typename T>
        constexpr bool maybe_iter = decltype(is_iter_test<T>(0))::value;
    }
    /**
     * @brief 创建迭代器范围
     * @tparam Iter 迭代器类型
     * @param count 元素个数
     * @param begin 起点迭代器
     * @note 通过不是指针但能解引用近似判断
     */
    template <typename Iter, typename = xyu::t_enable<__::maybe_iter<Iter>>>
    constexpr auto make_range(xyu::size_t count, Iter begin, Iter end) noexcept(noexcept(Range<RangeIter_Iter<Iter>>{xyu::size_t{}, Iter{}, Iter{}}))
    { return Range<RangeIter_Iter<Iter>>{count, begin, end}; }
    /**
     * @brief 创建迭代器范围
     * @tparam Iter 迭代器类型
     * @param count 元素个数
     * @param begin 起点迭代器
     * @param end 终点迭代器
     * @note 通过不是指针但能解引用近似判断
     */
    template <typename Iter, typename = xyu::t_enable<__::maybe_iter<Iter>>>
    constexpr auto make_range(xyu::size_t count, Iter begin) noexcept(noexcept(Range<RangeIter_Iter<Iter>>{xyu::size_t{}, Iter{}}))
    { return Range<RangeIter_Iter<Iter>>{count, begin}; }
    /**
     * @brief 创建迭代器范围
     * @tparam Iter 迭代器类型
     * @param begin 起点迭代器
     * @param end 终点迭代器
     * @note 通过不是指针但能解引用近似判断
     */
    template <typename Iter, typename = xyu::t_enable<__::maybe_iter<Iter>>>
    constexpr auto make_range(Iter begin, Iter end) noexcept(noexcept(Range<RangeIter_Iter<Iter>>{Iter{}, Iter{}}))
    { return Range<RangeIter_Iter<Iter>>{begin, end}; }

    namespace __
    {
        // 返回值接受测试
        template <typename It, typename T, typename U, typename = decltype(xyu::t_val<T&>()(xyu::t_val<U&>()(xyu::t_val<It&>())))>
        xyu::int_c<1> part_call_test(char);
        template <typename It, typename T, typename U, typename = decltype(xyu::t_val<T&>()(xyu::t_val<U&>()(xyu::t_val<It&>())), xyu::t_val<It&>())>
        xyu::int_c<2> part_call_test(int);
        template <typename It, typename T, typename U, typename = decltype(xyu::t_val<T&>()(xyu::t_val<U&>()(xyu::t_val<It&>(), xyu::t_val<It&>())))>
        xyu::int_c<3> part_call_test(double);
        template <typename...>
        xyu::int_c<0> part_call_test(...);

        // 组件继承
        template <typename NewPart>
        struct change_extend
        {
            // 新组件继承模板
            template <typename OldPart>
            struct RangeIter_Change_Extend : xyu::t_ebo<OldPart, 1>, xyu::t_ebo<NewPart, 2>
            {
            public:
                template <typename Iter>
                constexpr decltype(auto) operator()(Iter& it) noexcept
                {
                    // 如果原组件有返回值
                    constexpr int kind = decltype(part_call_test<Iter, NewPart, OldPart>('0'))::value;
                    if constexpr (!xyu::t_is_void<decltype(op()(it))> && kind)
                    {
                        if constexpr (kind == 1) return np()(op()(it));
                        else if constexpr (kind == 2) return np()(op()(it), it);
                        else if constexpr (kind == 3) return np()(it, op()(it));
                    }
                    // 原组件无返回值 或 新组件无法接受原组件的返回值
                    else { op()(it); return (np()(it)); }
                }
            private:
                constexpr OldPart& op() noexcept { return static_cast<xyu::t_ebo<OldPart, 1>&>(*this).base(); }
                constexpr NewPart& np() noexcept { return static_cast<xyu::t_ebo<NewPart, 2>&>(*this).base(); }
            };

            using type = xyu::t_park_tmpl<RangeIter_Change_Extend>;
        };

        template <template<typename>class NewTT>
        struct change_extend<xyu::t_park_tmpl<NewTT>>
        {
            // 新组件继承模板
            template <typename OldPart>
            struct RangeIter_Change_Extend : protected NewTT<OldPart>
            {
            public:
                template <typename Iter>
                constexpr decltype(auto) operator()(Iter& it) noexcept
                {
                    // 如果原组件有返回值
                    constexpr int kind = decltype(part_call_test<Iter, NewTT<OldPart>, OldPart>('0'))::value;
                    if constexpr (!xyu::t_is_void<decltype(op()(it))> && kind)
                    {
                        if constexpr (kind == 1) return np()(op()(it));
                        else if constexpr (kind == 2) return np()(op()(it), it);
                        else if constexpr (kind == 3) return np()(it, op()(it));
                    }
                    // 原组件无返回值 或 新组件无法接受原组件的返回值
                    else { op()(it); return (np()(it)); }
                }
            private:
                constexpr OldPart& op() noexcept { return static_cast<OldPart&>(*this); }
                constexpr NewTT<OldPart>& np() noexcept { return static_cast<NewTT<OldPart>&>(*this); }
            };

            using type = xyu::t_park_tmpl<RangeIter_Change_Extend>;
        };

        // 传递参数
        template <typename Iter, typename... Args>
        constexpr auto make_range_construct(Args&&... args) noexcept(noexcept(Range<Iter>{xyu::t_val<Args>()...}))
        { return Range<Iter>{xyu::forward<Args>(args)...}; }
        // 生成新迭代器
        template <typename Iter, typename Tag, typename Change>
        using get_new_extend_iter = xyu::t_change_iter<Iter, Tag, typename change_extend<xyu::t_remove_cvref<Change>>::type>;
        template <typename Iter, typename Tag, typename Change>
        using get_new_native_iter = xyu::t_change_iter<Iter, Tag, Change>;
        // 异常情况
        template <typename Iter, typename Tps, typename... Args>
        constexpr bool make_range_construct_noexcept() noexcept
        {
            using Tag = xyu::t_args_get<Tps, 0>;
            using V = xyu::t_args_get<Tps, 1>;
            if constexpr (!xyu::t_is_same<V, xyu::native_t>) {
                return noexcept(make_range_construct<get_new_extend_iter<Iter, Tag, V>>(xyu::t_val<Args>()...));
            } else {
                using Change = xyu::t_args_get<Tps, 2>;
                return noexcept(make_range_construct<get_new_native_iter<Iter, Tag, Change>>(xyu::t_val<Args>()...));
            }
        }
        // 递归处理参数
        template <typename Iter, typename Tps, typename... Args, typename = xyu::t_enable<xyu::t_is_typelist<Tps>>>
        constexpr auto make_range_construct(Tps, Args&&... args) noexcept(make_range_construct_noexcept<Iter, Tps, Args...>())
        {
            static_assert(xyu::t_args_count<Tps> >= 2, "Too few arguments for typelist");
            using Tag = xyu::t_args_get<Tps, 0>;
            static_assert(xyu::t_is_iter_tag<Tag>, "Invalid tag type");
            using V = xyu::t_args_get<Tps, 1>;
            if constexpr (!xyu::t_is_same<V, xyu::native_t>) {
                return make_range_construct<get_new_extend_iter<Iter, Tag, V>>(xyu::forward<Args>(args)...);
            } else {
                static_assert(xyu::t_args_count<Tps> >= 3, "Too few arguments for typelist");
                using Change = xyu::t_args_get<Tps, 2>;
                return make_range_construct<get_new_native_iter<Iter, Tag, Change>>(xyu::forward<Args>(args)...);
            }
        }
        template <typename Iter, typename Tag, typename Change, typename... Args, typename = xyu::t_enable<xyu::t_is_iter_tag<Tag> && !xyu::t_is_same_nocvref<Change, xyu::native_t>>>
        constexpr auto make_range_construct(Tag, Change, Args&&... args) noexcept(noexcept(make_range_construct<get_new_extend_iter<Iter, Tag, Change>>(xyu::t_val<Args>()...)))
        {
            static_assert(xyu::t_is_iter_tag<Tag>, "Invalid tag type");
            return make_range_construct<get_new_extend_iter<Iter, Tag, Change>>(xyu::forward<Args>(args)...);
        }
        template <typename Iter, typename Tag, typename Native, typename Change, typename... Args, typename = xyu::t_enable<xyu::t_is_iter_tag<Tag> && xyu::t_is_same_nocvref<Native, xyu::native_t>>>
        constexpr auto make_range_construct(Tag, Native, Change, Args&&... args) noexcept(noexcept(make_range_construct<get_new_native_iter<Iter, Tag, Change>>(xyu::t_val<Args>()...)))
        {
            static_assert(xyu::t_is_iter_tag<Tag>, "Invalid tag type");
            return make_range_construct<get_new_native_iter<Iter, Tag, Change>>(xyu::forward<Args>(args)...);
        }
    }
    /**
     * @brief 通过动态修改迭代器行为来创建一个新的 `Range`。
     * @details 这是一个高级的 `make_range` 版本，它允许在创建 `Range` 的同时， 即时地改变其底层迭代器的一个或多个策略。
     *
     * @tparam Iter 原始迭代器类型，将作为修改的基础。
     * @param args  一系列参数，分为“修改部分”和“构造部分”。
     *
     * ### 参数格式:
     * `make_range˂BaseIter˃({mod1}, {mod2}, ..., {ctor_args...})`
     *
     * - **修改部分 (`{mod}`)**:
     *   每个修改指令是一个 `{Tag, [native_v], Change}` 的参数序列 或 `typelist_c<Tag, [native_t], Change>{}`。
     *   - `Tag`: 迭代器部件标志，如 `RangeIter_Dereference_v`。
     *   - `native_v` (可选): 若提供，表示 `Change` 将完全覆盖旧策略。
     *   - `Change`: 新的策略逻辑，可以是一个函数对象（如 lambda）或一个打包后的模板。
     *
     * - **构造部分 (`{ctor_args...}`)**:
     *   紧跟在所有修改指令之后，是传递给 `Range` 构造函数的参数，
     *   如 `{count, begin, end}`, `{count, begin}` 或 `{begin, end}`。
     *
     * @note `Change` (无 `native_v`): 新策略**扩展**旧策略。新策略的调用包裹了旧策略的调用。
     * @note `Change` (有 `native_v`): 新策略**覆盖**旧策略。旧策略将不再被调用。
     *
     * @example
     *   // 扩展：解引用时对原值加一
     *   make_range˂RangeIter_Ptr˂int˃˃(RangeIter_Dereference_v, [](int& v){ return v + 1; }, arr, arr + 5);
     *
     *   // 覆盖：解引用时返回一个固定值 42
     *   make_range˂RangeIter_Ptr˂int˃˃(RangeIter_Dereference_v, native_v, [](auto&){ return 42; }, arr, arr + 5);
     */
    template <typename Iter, typename... Args, typename = xyu::t_enable<xyu::t_is_iterator<Iter> && sizeof...(Args)>>
    constexpr auto make_range(Args&&... args) noexcept(noexcept(__::make_range_construct<Iter>(xyu::forward<Args>(args)...)))
    { return __::make_range_construct<xyu::t_remove_cvref<Iter>>(xyu::forward<Args>(args)...); }

    namespace __
    {
        // 根绝传递给 Range 构造函数的参数 推断出原 Iter 类型，然后生成新的迭代器并创建范围
        template <typename Arg, typename... Args, typename = xyu::t_enable<!xyu::t_is_iter_tag<Arg>>>
        constexpr auto make_range_check(Arg arg, Args&&... args) noexcept
        {
            if constexpr (xyu::t_is_typelist<Arg>)
            {
                static_assert(sizeof...(Args) > 0, "Too few arguments for constructor Range");
                return make_range_check(xyu::forward<Args>(args)...);
            }
            else return make_range(xyu::forward<Arg>(arg), xyu::forward<Args>(args)...).begin();
        }
        // 跳过 {tag, change}，找到最终传递给 Range 构造函数的参数
        template <typename Tag, typename Change, typename... Args, typename = xyu::t_enable<xyu::t_is_iter_tag<Tag> && !xyu::t_is_same_nocvref<Change, xyu::native_t>>>
        constexpr auto make_range_check(Tag, Change, Args&&... args) noexcept
        {
            return make_range_check(xyu::forward<Args>(args)...);
        }
        // 跳过 {tag, native_v, change}，找到最终传递给 Range 构造函数的参数
        template <typename Tag, typename Native, typename Change, typename... Args, typename = xyu::t_enable<xyu::t_is_iter_tag<Tag> && xyu::t_is_same_nocvref<Native, xyu::native_t>>>
        constexpr auto make_range_check(Tag, Native, Change, Args&&... args) noexcept
        {
            return make_range_check(xyu::forward<Args>(args)...);
        }
    }
    /**
     * @brief 自动推断迭代器类型，并动态修改其行为来创建一个新的 `Range`。
     * @details
     *   这是最高级的 `make_range` 版本。它会从“构造部分”的参数（如 C 数组、容器引用）
     *   自动推断出基础迭代器类型，然后应用“修改部分”的指令。
     * @see make_range˂Iter, Args...˃
     *
     * @example
     *   // 自动推断出 RangeIter_Ptr˂int˃，然后修改解引用行为
     *   make_range(RangeIter_Dereference_v, [](int& v){ return v + 1; }, arr, arr + 5);
     */
    template <typename Arg, typename... Args, typename = xyu::t_enable<xyu::t_is_iter_tag<Arg> || (xyu::t_is_typelist<Arg> && sizeof(Arg) == 0)>>
    constexpr auto make_range(Arg arg, Args&&... args) noexcept(noexcept(__::make_range_construct<decltype(__::make_range_check(xyu::t_val<Arg>(), xyu::t_val<Args>()...))>(xyu::t_val<Arg>(), xyu::t_val<Args>()...)))
    {
        using Iter = decltype(__::make_range_check(xyu::forward<Arg>(arg), xyu::forward<Args>(args)...));
        return __::make_range_construct<Iter>(xyu::forward<Arg>(arg), xyu::forward<Args>(args)...);
    }
}

/** 生成器范围创建 */
namespace xylu::xyrange
{
    /**
     * @brief 创建一个生成等差数列的 `Range`。
     * @param end   序列的结束值（不包含）。序列从 0 开始，步长为 1。
     */
    template <typename T, typename = xyu::t_enable<xyu::t_is_number<T>>>
    constexpr auto make_range_sequence(T end) noexcept
    {
        using Generator = RangeGenerator_Sequence<T>;
        if constexpr (xyu::t_is_integral<T>) return Range<Generator>(end - 0, Generator{end});
        else return Range<Generator>(__builtin_ceill(end), Generator{end});
    }

    /**
     * @brief 创建一个生成等差数列的 `Range`。
     * @param begin 序列的起始值（包含）。
     * @param end   序列的结束值（不包含）。
     * @param step  步长，默认为 1。
     */
    template <typename T, typename = xyu::t_enable<xyu::t_is_number<T>>>
    constexpr auto make_range_sequence(T begin, T end, T step = 1) noexcept
    {
        using Generator = RangeGenerator_Sequence<T>;
        if constexpr (xyu::t_is_integral<T>) return Range<Generator>((end - begin) / step, Generator{begin, end, step});
        else return Range<Generator>(__builtin_ceill((end - begin) / step), Generator{begin, end, step});
    }

    /**
     * @brief 创建一个重复单个值的 `Range`。
     * @param value 要重复的值。
     * @param n     重复的次数，默认为 1。
     */
    template <typename T>
    constexpr auto make_range_repeat(const T& value, xyu::size_t n = 1) noexcept(xyu::t_can_nothrow_cpconstr<T>)
    {
        using Generator = RangeGenerator_Repeat<T>;
        return Range<Generator>(n, Generator{value, n});
    }
}

/// 范围属性
namespace xylu::xytraits
{
    namespace __
    {
        template <typename T, typename = decltype(xylu::xyrange::make_range(t_val<T&>()))>
        true_c can_make_range_test(int);
        template <typename T>
        false_c can_make_range_test(...);
    }
    /// 判断能否生成范围
    template <typename... T>
    constexpr bool t_can_make_range = (... && decltype(__::can_make_range_test<T>(0))::value);
}

#pragma clang diagnostic pop