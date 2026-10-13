#pragma once

/**
 * @file    xyrange/range.h
 * @brief   范围抽象层：支持多源数据适配的统一视图封装
 * @date    2025-12-24
 */

#include "./iter.h"
#include "./gen.h"
#include "../xystd/initlist.h"
#include "../xymath/calc.h"
#include "../xymath/hash.h"

/// 范围模板类
namespace xylu::xyrange
{
    namespace __
    {
        // 向前移动迭代器
        template <typename Iter, xyu::t_enable<xyu::t_is_riter<Iter>, bool> = true>
        constexpr Iter nextiter(Iter it, xyu::size_t step) noexcept(noexcept(it + step)) { return it + step; }
        template <typename Iter, xyu::t_enable<!xyu::t_is_riter<Iter>, bool> = false>
        constexpr Iter nextiter(Iter it, xyu::size_t step) noexcept(noexcept(++it))
        {
            static_assert(xyu::t_is_siter<Iter>, "cannot move non-single iterator");
            while (step) { ++it; --step; }
            return it;
        }

        // 向后移动迭代器
        template <typename Iter, xyu::t_enable<xyu::t_is_riter<Iter>, bool> = true>
        constexpr Iter previter(Iter it, xyu::size_t step) noexcept(noexcept(it - step)) { return it - step; }
        template <typename Iter, xyu::t_enable<!xyu::t_is_riter<Iter>, bool> = false>
        constexpr Iter previter(Iter it, xyu::size_t step) noexcept(noexcept(--it))
        {
            static_assert(xyu::t_is_diter<Iter>, "cannot move non-double iterator");
            while (step) { --it; --step; }
            return it;
        }

        // 计算迭代器差值
        template <typename Iter, xyu::t_enable<xyu::t_is_riter<Iter>, bool> = true>
        constexpr xyu::size_t distance(Iter s, Iter o) noexcept(noexcept(o - s)) { return o - s; }
        template <typename Iter, xyu::t_enable<!xyu::t_is_riter<Iter>, bool> = false>
        constexpr xyu::size_t distance(Iter s, Iter o) noexcept(noexcept(++s, s != o))
        {
            static_assert(xyu::t_is_siter<Iter>, "cannot calculate distance for non-single iterator");
            xyu::size_t dist = 0;
            while (s != o) { ++s; ++dist; }
            return dist;
        }

        /// 范围基类
        // 提供统一的使用函数
        template <typename Derived>
        struct RangeBase
        {
            /// 获取元素个数或状态标识
            constexpr xyu::size_t count() const noexcept { return derived().n; }
            /// 是否为空
            constexpr bool empty() const noexcept { return count() == 0; }

            /// 是否为 已知且有限 的范围
            constexpr bool definite() const noexcept { return count() <= static_cast<xyu::size_t>(-1) / 2; }
            /// 是否为 未知但有限 的范围
            constexpr bool indefinite() const noexcept { return count() + 1 > static_cast<xyu::size_t>(-1) / 2 + 1; }

            /// 是否为有限范围
            constexpr bool limited() const noexcept { return count() != -1; }
            /// 是否为无限范围
            constexpr bool unlimited() const noexcept { return count() == -1; }

        private:
            // 获取子类
            constexpr const Derived& derived() const noexcept { return static_cast<const Derived&>(*this); }
        };
    }

    /**
     * @brief 范围类型
     * @tparam Iter 构成此范围的迭代器或生成器类型，必须是 `Iterator` 或 `Generator` 的特化
     * @details
     *   封装了序列的起点、终点和大小；
     *   提供了 `begin()` 和 `end()` 方法，使其可以被直接用于 范围 for 循环。
     *
     *   由于某些特殊范围的长度在运行时可能是动态、无限或未知的，
     *   因此 `count` 使用 `size_t` 的魔法数字段来标识范围的不同状态：
     *   - [0, max/2] : 已知大小的有限范围，limited && definite
     *   - (max/2, max): 未知大小的有限范围，limited && indefinite
     *   - max (-1)   : 无限范围，unlimted
     *
     * @attention
     *   当作为生成器范围时，`end()` 会返回一个空的“哨兵”节点
     */
    template <typename Iter, bool = xyu::t_is_gen<Iter>> struct Range;

    /// 迭代器
    template <typename Iter>
    struct Range<Iter, false> : __::RangeBase<Range<Iter, false>>
    {
        static_assert(xyu::t_is_siter<Iter>);
        friend class __::RangeBase<Range>;

        using cIter = Iterator_Const<Iter>;
        using mIter = Iterator_Move<Iter>;
        using rIter = Iterator_Reverse<Iter>;

    private:
        xyu::size_t n;      // 元素个数 (可能为特殊状态)
        Iter bg, ed;        // 起点终点迭代器

    public:
        /**
         * @brief 构造函数
         * @details 提供完整的参数进行构造，保证性能最优。如果范围是无限或未知的，请传入特殊值
         */
        constexpr Range(xyu::size_t count, Iter bg, Iter ed) noexcept(noexcept(Iter{bg}))
                : n{count}, bg{bg}, ed{ed} {}
        /**
         * @brief 构造函数
         * @details
         *   提供起点迭代器和元素个数进行构造，自动计算终点迭代器。
         *   如果是非随机迭代器，需要支付遍历计算终点的代价
         */
        constexpr Range(xyu::size_t count, Iter bg) noexcept(noexcept(Iter{__::nextiter(bg, count)}))
                : n{count}, bg{bg}, ed{__::nextiter(bg, count)} {}
        /**
         * @brief 构造函数
         * @details
         *   提供起点和终点迭代器进行构造，自动计算元素个数。
         *   调用者自行保证其为有限，如果是非随机迭代器，需要支付遍历计算元素个数的代价
         */
        constexpr Range(Iter bg, Iter ed) noexcept(noexcept(Iter{bg}, __::distance(bg, ed)))
                : n{__::distance(bg, ed)}, bg{bg}, ed{ed} {}

        /// 获取范围起点迭代器
        constexpr Iter begin() const noexcept { return bg; }
        /// 获取范围终点迭代器
        constexpr Iter end() const noexcept { return ed; }

        /// 获取常量范围
        constexpr Range<cIter> crange() const noexcept(noexcept(cIter{bg})) { return {n, bg, ed}; }
        /// 获取移动范围
        constexpr Range<mIter> mrange() const noexcept(noexcept(mIter{bg})) { return {n, bg, ed}; }
        /// 获取反向范围
        template <typename Test = Iter, typename = xyu::t_enable<xyu::t_is_diter<Test>>>
        constexpr Range<rIter> rrange() const noexcept(noexcept(rIter{bg})) { return {n, ed, bg}; }
    };
            
    /// 生成器
    template <typename Gen>
    struct Range<Gen, true> : __::RangeBase<Range<Gen, true>>
    {
        friend class __::RangeBase<Range>;
    private:
        // 生成器封装
        using bIter = Generator_Iter<Gen>;
        using eIter = struct {};

        using cGen = Iterator_Const<Gen>;
        using mGen = Iterator_Move<Gen>;

    private:
        xyu::size_t n;  // 元素个数 (可能包含状态标识)
        Gen gen;        // 生成器

    public:
        /**
         * @brief 构造函数
         * @details 提供生成器和明确的元素个数(或状态标志)进行构造
         */
        constexpr Range(xyu::size_t count, Gen g) noexcept(xyu::t_can_nothrow_construct<Gen>)
                : n{count}, gen(g) {}

        /**
         * @brief 构造函数
         * @details 通过修改后的迭代器构造和明确的元素个数(或状态标志)进行构造
         */
        template <typename It = int>
        constexpr Range(xyu::size_t count, bIter it, It/*占位*/ = 0) noexcept(xyu::t_can_nothrow_construct<Gen>)
                : n{count}, gen(it.p) {}

        /// 获取范围起点迭代器 (生成器)
        constexpr bIter begin() const noexcept { return {gen, n}; }
        /// 获取范围终点迭代器 (哨兵)
        constexpr eIter end() const noexcept { return {}; }

        /// 获取常量生成器
        constexpr Range<cGen> crange() const noexcept(noexcept(cGen{gen})) { return {n, gen}; }
        /// 获取移动生成器
        constexpr Range<mGen> mrange() const noexcept(noexcept(mGen{gen})) { return {n, gen}; }
    };

    // 推导指引
    template <typename Iter, typename = xyu::t_enable<!xyu::t_is_gen<Iter>>>
    Range(xyu::size_t, Iter, Iter) -> Range<Iter>;
    template <typename Iter>
    Range(xyu::size_t, Iter) -> Range<Iter>;
    template <typename Iter, typename = xyu::t_enable<!xyu::t_is_gen<Iter>>>
    Range(Iter, Iter) -> Range<Iter>;
    template <typename Gen, typename = xyu::t_enable<xyu::t_is_gen<Gen>>>
    Range(Gen) -> Range<Gen>;
}

/// 范围属性
namespace xylu::xytraits
{
    namespace __
    {
        template <typename T>
        struct is_range : false_c {};
        template <typename Iter, bool b>
        struct is_range<xyu::Range<Iter, b>> : true_c {};
    }
    /// 判断是否为范围
    template <typename... T>
    constexpr bool t_is_range = (... && __::is_range<xyu::t_rem_cr<T>>::value);

    namespace __
    {
        template <typename T>
        struct is_gen_range : false_c {};
        template <typename Iter>
        struct is_gen_range<xyu::Range<Iter, true>> : true_c {};
    }
    /// 判断是否为生成器范围
    template <typename... T>
    constexpr bool t_is_gen_range = (... && __::is_gen_range<xyu::t_rem_cr<T>>::value);
}

/// 迭代器范围创建
namespace xylu::xyrange
{
    /* 内置类型 */

    /// 通过 C风格数组 创建范围
    template <typename T, xyu::size_t N>
    constexpr auto make_range(T(&arr)[N]) noexcept { return Range<Iterator_Ptr<T>>{N, arr, arr + N}; }

    /**
     * @brief 通过 C风格字符串 创建范围
     * @note 忽略末尾的 '\0' 字符
     */
    template <xyu::size_t N>
    constexpr auto make_range(const char(&arr)[N]) noexcept { return Range<Iterator_Ptr<const char>>{N - 1, arr, arr + N - 1}; }

    /**
     * @brief 通过 初始化列表 `std::initializer_list` 创建常量范围
     * @attention 初始化列表的生命周期很短，确保创建的范围在其生命周期内（即同一条语句中）
     */
    template <typename T>
    constexpr auto make_range(std::initializer_list<T> il) noexcept { return Range<Iterator_Ptr<const T>>(il.size(), il.begin(), il.end()); }

    /* 容器 */

    namespace __
    {
        // 测试 range()
        template <typename T, typename = xyu::t_enable<xyu::t_is_range<decltype(xyu::t_val<T>().range())>>>
        xyu::true_c has_method_range_test(int);
        template <typename T>
        xyu::false_c has_method_range_test(...);
        template <typename T>
        constexpr bool has_method_range = decltype(has_method_range_test<xyu::t_rem_cr<T>>(0))::value;
    }
    /**
     * @brief 通过 自定义容器 获取范围
     * @details 容器提供了 `range()` 成员函数且返回有效的 Range 对象
     */
    template <typename T, xyu::t_enable<__::has_method_range<T>, bool> = true>
    constexpr auto make_range(T&& contain) noexcept(noexcept(contain.range())) { return contain.range(); }

    namespace __
    {
        // 测试 begin(), end()
        template <typename T, typename = decltype(xyu::t_val<T>().begin(), xyu::t_val<T>().end())>
        xyu::true_c has_method_begin_end_test(int);
        template <typename T>
        xyu::false_c has_method_begin_end_test(...);
        template <typename T>
        constexpr bool has_method_be = decltype(has_method_begin_end_test<xyu::t_rem_cr<T>>(0))::value;

        // 测试获取元素个数函数 count()/length()/size()
        template <typename T, typename = decltype(xyu::t_val<T>().count())>
        xyu::size_c<1> has_method_count_test(char);
        template <typename T, typename = decltype(xyu::t_val<T>().length())>
        xyu::size_c<2> has_method_count_test(int);
        template <typename T, typename = decltype(xyu::t_val<T>().size())>
        xyu::size_c<3> has_method_count_test(long);
        template <typename T>
        xyu::size_c<0> has_method_count_test(...);
        template <typename T>
        constexpr int has_method_count = decltype(has_method_count_test<xyu::t_rem_cr<T>>('0'))::value;

        // 异常检测
        template <typename T>
        constexpr bool make_range_contain_noexcept() noexcept
        {
            using Iter = Iterator_iter<xyu::t_rem_cr<decltype(xyu::t_val<T&>().begin())>>;
            if constexpr (has_method_count<T> == 1) return noexcept(Range<Iter>{xyu::t_val<T&>().count(), xyu::t_val<T&>().begin(), xyu::t_val<T&>().end()});
            else if constexpr (has_method_count<T> == 2) return noexcept(Range<Iter>{xyu::t_val<T&>().length(), xyu::t_val<T&>().begin(), xyu::t_val<T&>().end()});
            else if constexpr (has_method_count<T> == 3) return noexcept(Range<Iter>{xyu::t_val<T&>().size(), xyu::t_val<T&>().begin(), xyu::t_val<T&>().end()});
            else return noexcept(Range<Iter>{xyu::t_val<T&>().begin(), xyu::t_val<T&>().end()});
        }
    }
    /**
     * @brief 通过 STL风格 的容器 创建范围
     * @details
     *   适配任何提供了 `begin()` 和 `end()` 成员函数的容器，
     *   若容器提供了 `count()`, `length()` 或 `size()`，则直接调用获取大小
     * @attention 请确保容器的生命周期比范围等长或更长
     */
    template <typename T, xyu::t_enable<!__::has_method_range<T> && __::has_method_be<T> && !xyu::t_is_range<T>, bool> = true>
    constexpr auto make_range(T&& contain) noexcept(__::make_range_contain_noexcept<T>())
    {
        using Iter = Iterator_iter<xyu::t_rem_cr<decltype(contain.begin())>>;
        if constexpr (__::has_method_count<T> == 1) return Range<Iter>{contain.count(), contain.begin(), contain.end()};
        else if constexpr (__::has_method_count<T> == 2) return Range<Iter>{contain.length(), contain.begin(), contain.end()};
        else if constexpr (__::has_method_count<T> == 3) return Range<Iter>{contain.size(), contain.begin(), contain.end()};
        else return Range<Iter>{contain.begin(), contain.end()};
    }

    /* 范围 */

    /// 返回范围自身
    template <typename Rg, typename = xyu::t_enable<xyu::t_is_range<Rg>>>
    constexpr Rg make_range(Rg&& range) noexcept { return xyu::forward<Rg>(range); }

    /* 指针 */

    /**
     * @brief 创建指针范围
     * @tparam T 元素类型
     * @param count 元素个数
     * @param begin 起点指针
     * @param end 终点指针 (可选)
     */
    template <typename T>
    constexpr auto make_range(xyu::size_t count, T* begin, T* end = nullptr) noexcept
    {
        if (end == nullptr) return Range<Iterator_Ptr<T>>{count, begin};
        return Range<Iterator_Ptr<T>>{count, begin, end};
    }
    /**
     * @brief 创建指针范围
     * @tparam T 元素类型
     * @param begin 起点指针
     * @param end 终点指针
     * @see make_range(xyu::size_t, T*, T*) 迭代器类型详见
     */
    template <typename T>
    constexpr auto make_range(T* begin, T* end) noexcept { return Range<Iterator_Ptr<T>>{begin, end}; }

    /* 迭代器 */

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
     * @param count 元素个数
     * @param begin 起点迭代器
     * @param end 终点迭代器
     */
    template <typename Iter, typename = xyu::t_enable<__::maybe_iter<Iter>>>
    constexpr auto make_range(xyu::size_t count, Iter begin, Iter end) noexcept(noexcept(Range<Iterator_iter<Iter>>{xyu::size_t{}, xyu::t_val<Iter>(), xyu::t_val<Iter>()}))
    { return Range<Iterator_iter<Iter>>{count, begin, end}; }
    /**
     * @brief 创建迭代器范围
     * @tparam Iter 迭代器类型
     * @param count 元素个数
     * @param begin 起点迭代器
     */
    template <typename Iter, typename = xyu::t_enable<__::maybe_iter<Iter>>>
    constexpr auto make_range(xyu::size_t count, Iter begin) noexcept(noexcept(Range<Iterator_iter<Iter>>{xyu::size_t{}, xyu::t_val<Iter>()}))
    { return Range<Iterator_iter<Iter>>{count, begin}; }
    /**
     * @brief 创建迭代器范围
     * @tparam Iter 迭代器类型
     * @param begin 起点迭代器
     * @param end 终点迭代器
     */
    template <typename Iter, typename = xyu::t_enable<__::maybe_iter<Iter>>>
    constexpr auto make_range(Iter begin, Iter end) noexcept(noexcept(Range<Iterator_iter<Iter>>{xyu::t_val<Iter>(), xyu::t_val<Iter>()}))
    { return Range<Iterator_iter<Iter>>{begin, end}; }

    /* 封装 */

    /**
     * @brief 创建封装迭代器范围
     * @tparam NewIter 封装迭代器
     * @details 通过传递的参数构造范围后，将迭代器类型替换为 `NewIter`
     */
    template <typename NewIter, typename... Args, typename Iter = decltype(make_range(xyu::t_val<Args>()...).begin())>
    constexpr auto make_range_pack(Args&&... args) noexcept(noexcept(make_range(xyu::t_val<Args>()...)) && xyu::t_can_nothrow_init<NewIter, Iter>)
    {
        auto range = make_range(xyu::forward<Args>(args)...);
        return Range<NewIter>{range.count(), range.begin(), range.end()};
    }

    /**
     * @brief 创建封装迭代器范围
     * @tparam IterPack 封装迭代器模板
     * @details 通过传递的参数构造范围后，将迭代器类型替换为 `IterPack˂Iter˃`
     */
    template <template<typename>class IterPack, typename... Args, typename Iter = decltype(make_range(xyu::t_val<Args>()...).begin())>
    constexpr auto make_range_pack(Args&&... args) noexcept(noexcept(make_range(xyu::t_val<Args>()...)) && xyu::t_can_nothrow_init<IterPack<Iter>, Iter>)
    {
        auto range = make_range(xyu::forward<Args>(args)...);
        return Range<IterPack<Iter>>{range.count(), range.begin(), range.end()};
    }

}

/// 生成器范围创建
namespace xylu::xyrange
{
    /**
     * @brief 创建一个连续序列范围 [0, end)
     * @attention 浮点数可能存在精度误差
     */
    template <typename T>
    constexpr auto make_range_sequence(T end) noexcept
    {
        static_assert(xyu::t_is_arithmetic<T>);
        using Gen = Generator_Sequence<T>;
        if constexpr (xyu::t_is_integral<T>) return Range(end > 0 ? end : 0, Gen{0, 1});
        else return Range(end > 0 ? xyu::calc_ceil(end - xyu::t_number<T>::epsilon) : 0, Gen{0, 1});
    }

    /**
     * @brief 创建一个等差数列范围 [begin, end)，步长 step (默认为1)
     * @attention 浮点数可能存在精度误差
     */
    template <typename T>
    constexpr auto make_range_sequence(T begin, T end, T step = 1) noexcept
    {
        static_assert(xyu::t_is_arithmetic<T>);
        auto gen = Generator_Sequence<T>{begin ,step};
        auto epsilon = xyu::t_number<T>::epsilon;
        xyu::size_t n = 0;
        if (XY_UNLIKELY(step == 0)) n = begin == end ? 0 : -1;
        else if (step > 0)
        {
            if (end <= begin);
            else if constexpr (xyu::t_is_integral<T>) n = (end - begin + step - 1) / step;
            else n = xyu::calc_ceil((end - begin) / step - epsilon);
        }
        else
        {
            if (end >= begin);
            else if constexpr (xyu::t_is_integral<T>) n = (end - begin + step + 1) / step;
            else n = xyu::calc_ceil((begin - end) / -step - epsilon);
        }
        return Range(n, Generator_Sequence<T>{begin, step});
    }

    /**
     * @brief 创建一个重复单个值 n 次的范围
     * @param value 重复的值
     * @param n 重复次数 (-1为无限)
     */
    template <typename T>
    constexpr auto make_range_repeat(const T& value, xyu::size_t n) noexcept(xyu::t_can_nothrow_cpconstr<T>)
    {
        if constexpr (!XY_RELEASE) if (XY_UNLIKELY(n != -1 && n > xyu::size_t(-1) / 2))
            xyu::E_Logic::abort_over_limit(XY_CurPos("make_range_repeat"), n, 0, xyu::size_t(-1) / 2);
        return Range(n, Generator_Repeat<T>{value});
    }
}

/// 范围属性
namespace xylu::xytraits
{
    namespace __
    {
        template <typename T>
        decltype(xyu::make_range(xyu::t_val<T>())) get_range_type_test(int)
        noexcept(noexcept(xyu::make_range(xyu::t_val<T>())));

        template <typename T>
        void get_range_type_test(...);
    }
    /// 获取范围类型
    template <typename T>
    using t_range = decltype(__::get_range_type_test<T>(0));

    /// 能否变为范围
    template <typename... T>
    constexpr bool t_can_range = (... && !t_is_void<t_range<T>>);

    /// 能否无异常变为范围
    template <typename... T>
    constexpr bool t_can_nothrow_range = (... && noexcept(__::get_range_type_test<T>(0)));

    /// 获取范围的访问类型
    template <typename T>
    struct t_access_base<T, t_enable<t_is_range<T>>> { using type = t_access<decltype(t_val<T>().begin())>; };
    namespace __
    {
        /// 获取可构造范围类型的访问类型
        template <typename T>
        struct t_access_impl<T, t_enable<t_can_range<T>>>
        { using type = t_access<decltype(xyu::make_range(xyu::t_val<T>()))>; };
    }

    /// 能否无异常访问
    template <typename T>
    struct t_noexcept_access_base<T, t_enable<t_is_iter<T>>>
    { using type = t_cond<noexcept(*t_val<T>()), t_access<T>, void>; };
    template <typename T>
    struct t_noexcept_access_base<T, t_enable<t_is_range<T>>>
    { using type = typename t_noexcept_access_base<decltype(xyu::t_val<T>().begin())>::type; };
    namespace __
    {
        template <typename T>
        struct t_noexcept_access_impl<T, t_enable<t_can_nothrow_range<T>>>
        { using type = typename t_noexcept_access_base<decltype(xyu::make_range(xyu::t_val<T>()))>::type; };
    }
}

/// 范围哈希
namespace xylu::xymath
{
    template <typename Rg>
    struct Hasher<Rg, xyu::t_enable<xyu::t_can_range<const Rg&>>, 7>
    {
        constexpr xyu::size_t operator()(const Rg& rg) noexcept
        {
            auto range = xyu::make_range(rg);
            if constexpr (XY_DEBUG) if (range.unlimited())
                xyu::E_Logic::abort_range_unlimited(XY_CurPos("make_hash<Range>"));
            xyu::size_t hash = range.count();
            for (const auto& v : range) hash = xyu::make_hash(hash, v);
            return hash;
        }
    };
}
