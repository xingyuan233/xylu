#pragma once

/**
 * @file    xyrange/cmp.h
 * @brief   通用比较
 * @date    2026-04-26
 */

#include "./range.h"

/// 通用比较函数
namespace xylu::xyrange
{
    // 比较函数检测
    namespace __
    {
        // 无 == / !=
        constexpr void common_e(...);
        // t == u
        template <typename T, typename U, typename = decltype(xyu::t_val<const T&>() == xyu::t_val<const U&>())>
        constexpr bool common_e(const T& t, const U& u, char&&) noexcept(noexcept(t == u)) { return t == u; }
        // u == t
        template <typename T, typename U, typename = decltype(xyu::t_val<const U&>() == xyu::t_val<const T&>())>
        constexpr bool common_e(const T& t, const U& u, const char&&) noexcept(noexcept(u == t)) { return u == t; }
        // t != u
        template <typename T, typename U, typename = decltype(xyu::t_val<const T&>() != xyu::t_val<const U&>())>
        constexpr bool common_e(const T& t, const U& u, const char&) noexcept(noexcept(t != u)) { return !(t != u); }
        // u != t
        template <typename T, typename U, typename = decltype(xyu::t_val<const U&>() != xyu::t_val<const T&>())>
        constexpr bool common_e(const T& t, const U& u, int&&) noexcept(noexcept(u != t)) { return !(u != t); }
        // 能否 ==
        template <typename T, typename U>
        constexpr bool can_common_e = xyu::t_is_bool<decltype(common_e(xyu::t_val<const T&>(), xyu::t_val<const U&>(), '0'))>;

        // 能否 < > <= >=
    #define XY_common_op_gen( name, op, op2 ) \
        constexpr void common_##name(...); \
        template <typename T, typename U, typename = decltype(xyu::t_val<const T&>() op xyu::t_val<const U&>())> \
        constexpr bool common_##name(const T& t, const U& u, char&&) noexcept(noexcept(t op u)) { return t op u; } \
        template <typename T, typename U, typename = decltype(xyu::t_val<const U&>() op2 xyu::t_val<const T&>())> \
        constexpr bool common_##name(const T& t, const U& u, const char&&) noexcept(noexcept(u op2 t)) { return !(u op2 t); } \
        template <typename T, typename U> \
        constexpr bool can_common_##name = xyu::t_is_bool<decltype(common_##name(xyu::t_val<const T&>(), xyu::t_val<const U&>(), '0'))>;
    XY_PP_FORWARD(XY_common_op_gen, 3, l,<,>=, g,>,<=, le,<=,>, ge,>=,<)
    #undef XY_common_op_gen

        // 无成员 equals
        constexpr void member_equals(...);
        // t.equals(u)
        template <typename T, typename U, typename = decltype(xyu::t_val<const T&>().equals(xyu::t_val<const U&>()))>
        constexpr bool member_equals(const T& t, const U& u, char) noexcept(noexcept(t.equals(u))) { return t.equals(u); }
        // u.equals(t)
        template <typename T, typename U, typename = decltype(xyu::t_val<const U&>().equals(xyu::t_val<const T&>()))>
        constexpr bool member_equals(const T& t, const U& u, int) noexcept(noexcept(u.equals(t))) { return u.equals(t); }
        // 是否支持成员 equals
        template <typename T, typename U>
        constexpr static bool can_member_equals = xyu::t_is_bool<decltype(member_equals(xyu::t_val<const T&>(), xyu::t_val<const U&>(), '0'))>;

        // 无成员 compare
        constexpr void member_compare(...);
        // t.compare(u)
        template <typename T, typename U, typename = decltype(xyu::t_val<const T&>().compare(xyu::t_val<const U&>()))>
        constexpr int member_compare(const T& t, const U& u, char&&) noexcept(noexcept(t.compare(u))) { return t.compare(u); }
        // u.compare(t)
        template <typename T, typename U, typename = decltype(xyu::t_val<const U&>().compare(xyu::t_val<const T&>()))>
        constexpr int member_compare(const T& t, const U& u, const char&&) noexcept(noexcept(u.compare(t))) { return -u.compare(t); }
        // 是否支持成员 compare
        template <typename T, typename U>
        constexpr static bool can_member_compare = xyu::t_is_same<int, decltype(member_compare(xyu::t_val<const T&>(), xyu::t_val<const U&>(), '0'))>;
    }

    // 范围比较函数声明
    namespace __
    {
        // 范围比较异常检测
    #define XY_range_common_noexcept_gen( name ) \
        template <typename T, typename U> \
        constexpr bool range_common_##name##_noexcept() noexcept;
    XY_PP_FORWARD(XY_range_common_noexcept_gen, 1, equals, compare )
    #undef XY_range_common_noexcept_gen

        // 范围比较
    #define XY_range_common_gen( name ) \
        template <typename T, typename U> \
        constexpr int range_common_##name(const T& t, const U& u) noexcept(range_common_##name##_noexcept<T,U>());
    XY_PP_FORWARD(XY_range_common_gen, 1, equals, compare )
    #undef XY_range_common_gen
    }

    // 比较函数组合
    namespace __
    {
        // 不支持通用 equals
        constexpr void common_equals(...);
        // == | !=
        template <typename T, typename U, typename = xyu::t_enable<can_common_e<T,U>>>
        constexpr bool common_equals(const T& t, const U& u, char&&) noexcept(noexcept(common_e(t,u,'0'))) { return common_e(t,u,'0'); }
        // .equals()
        template <typename T, typename U, typename = xyu::t_enable<can_member_equals<T,U>>>
        constexpr bool common_equals(const T& t, const U& u, const char&&) noexcept(noexcept(member_equals(t,u,'0'))) { return member_equals(t,u,'0'); }
        // .compare()
        template <typename T, typename U, typename = xyu::t_enable<can_member_compare<T,U>>>
        constexpr bool common_equals(const T& t, const U& u, const char&) noexcept(noexcept(member_compare(t,u,'0'))) { return member_compare(t,u,'0') == 0; }
        // < && <=
        template <typename T, typename U, typename = xyu::t_enable<can_common_l<T,U> && can_common_le<T,U>>>
        constexpr bool common_equals(const T& t, const U& u, int&&) noexcept(noexcept(common_l(t,u,'0'), common_le(t,u,'0')))
        { return !common_l(t,u,'0') && common_le(t,u,'0'); }
        // > && >=
        template <typename T, typename U, typename = xyu::t_enable<can_common_g<T,U> && can_common_ge<T,U>>>
        constexpr bool common_equals(const T& t, const U& u, const int&&) noexcept(noexcept(common_g(t,u,'0'), common_ge(t,u,'0')))
        { return !common_g(t,u,'0') && common_ge(t,u,'0'); }
        // > && <
        template <typename T, typename U, typename = xyu::t_enable<can_common_l<T,U> && can_common_g<T,U>>>
        constexpr bool common_equals(const T& t, const U& u, const int&) noexcept(noexcept(common_l(t,u,'0'), common_g(t,u,'0')))
        { return !common_l(t,u,'0') && !common_g(t,u,'0'); }
        // >= && <=
        template <typename T, typename U, typename = xyu::t_enable<can_common_le<T,U> && can_common_ge<T,U>>>
        constexpr bool common_equals(const T& t, const U& u, long&&) noexcept(noexcept(common_le(t,u,'0'), common_ge(t,u,'0')))
        { return common_le(t,u,'0') && common_ge(t,u,'0'); }
        // range
        template <typename T, typename U, typename = void>
        struct CommonEqualsEnable;
        template <typename T, typename U, typename = void>
        struct CommonEqualsEnableRange : xyu::false_c {};
        template <typename T, typename U>
        struct CommonEqualsEnableRange<T, U, xyu::t_enable<xyu::t_can_range<const T&, const U&>>> : CommonEqualsEnable<xyu::t_access<const T&>, xyu::t_access<const U&>> {};
        template <typename T, typename U, typename>
        struct CommonEqualsEnable : CommonEqualsEnableRange<T, U> {};
        template <typename T, typename U>
        struct CommonEqualsEnable<T, U, xyu::t_enable<can_common_e<T,U> || can_member_equals<T,U> || can_member_compare<T,U> || (can_common_l<T,U> && (can_common_le<T,U> || can_common_g<T,U>)) || (can_common_ge<T,U> && (can_common_g<T,U> || can_common_le<T,U>))>> : xyu::true_c {};
        template <typename T, typename U, typename = xyu::t_enable<xyu::t_can_range<const T&, const U&> && CommonEqualsEnable<T,U>::value>>
        constexpr bool common_equals(const T& t, const U& u, const long&&) noexcept(range_common_equals_noexcept<T,U>())
        { return range_common_equals(t,u); }
        // 能否通用 equals
        template <typename T, typename U>
        constexpr bool can_common_equals = xyu::t_is_bool<decltype(common_equals(xyu::t_val<const T&>(), xyu::t_val<const U&>(), '0'))>;


        // 不支持通用 compare
        constexpr void common_compare(...);
        // .compare()
        template <typename T, typename U, typename = xyu::t_enable<can_member_compare<T,U>>>
        constexpr int common_compare(const T& t, const U& u, char&&) noexcept(noexcept(member_compare(t,u,'0'))) { return member_compare(t,u,'0'); }
        // < && >
        template <typename T, typename U, typename = xyu::t_enable<can_common_l<T,U> && can_common_g<T,U>>>
        constexpr int common_compare(const T& t, const U& u, const char&&) noexcept(noexcept(common_l(t,u,'0'), common_g(t,u,'0')))
        { return common_l(t,u,'0') ? -1 : (common_g(t,u,'0') ? 1 : 0); }
        // < && ==
        template <typename T, typename U, typename = xyu::t_enable<can_common_l<T, U> && can_common_e<T, U>>>
        constexpr int common_compare(const T& t, const U& u, char&) noexcept(noexcept(common_l(t,u,'0'), common_e(t,u,'0')))
        { return common_e(t,u,'0') ? 0 : (common_l(t,u,'0') ? -1 : 1); }
        // > && ==
        template <typename T, typename U, typename = xyu::t_enable<can_common_g<T, U> && can_common_e<T, U>>>
        constexpr int common_compare(const T& t, const U& u, int&&) noexcept(noexcept(common_g(t,u,'0'), common_e(t,u,'0')))
        { return common_e(t,u,'0') ? 0 : (common_g(t,u,'0') ? 1 : -1); }
        // <= && >=
        template <typename T, typename U, typename = xyu::t_enable<can_common_le<T, U> && can_common_ge<T, U>>>
        constexpr int common_compare(const T& t, const U& u, const int&&) noexcept(noexcept(common_le(t,u,'0'), common_ge(t,u,'0')))
        { return !common_le(t,u,'0') ? 1 : (common_ge(t,u,'0') ? 0 : -1); }
        // < && <=
        template <typename T, typename U, typename = xyu::t_enable<can_common_l<T, U> && can_common_le<T, U>>>
        constexpr int common_compare(const T& t, const U& u, int&) noexcept(noexcept(common_l(t,u,'0'), common_le(t,u,'0')))
        { return common_l(t,u,'0') ? -1 : (common_le(t,u,'0') ? 0 : 1); }
        // > && >=
        template <typename T, typename U, typename = xyu::t_enable<can_common_g<T, U> && can_common_ge<T, U>>>
        constexpr int common_compare(const T& t, const U& u, long&&) noexcept(noexcept(common_g(t,u,'0'), common_ge(t,u,'0')))
        { return common_g(t,u,'0') ? 1 : (common_ge(t,u,'0') ? 0 : -1); }
        // range
        template <typename T, typename U, typename = void>
        struct CommonCmpareEnable;
        template <typename T, typename U, typename = void>
        struct CommonCmpareEnableRange : xyu::false_c {};
        template <typename T, typename U>
        struct CommonCmpareEnableRange<T, U, xyu::t_enable<xyu::t_can_range<const T&, const U&>>> : CommonCmpareEnable<xyu::t_access<const T&>, xyu::t_access<const U&>> {};
        template <typename T, typename U, typename>
        struct CommonCmpareEnable : CommonCmpareEnableRange<T, U> {};
        template <typename T, typename U>
        struct CommonCmpareEnable<T, U, xyu::t_enable<can_member_compare<T,U> || (can_common_l<T,U> && (can_common_g<T,U> || can_common_e<T,U> || can_common_le<T,U>)) || (can_common_g<T,U> && (can_common_e<T,U> || can_common_ge<T,U>)) || (can_common_le<T,U> || can_common_ge<T,U>)>> : xyu::true_c {};  template <typename T, typename U, typename = xyu::t_enable<xyu::t_can_range<const T&, const U&> && CommonCmpareEnable<T,U>::value>>
        constexpr int common_compare(const T& t, const U& u, const long&&) noexcept(range_common_compare_noexcept<T,U>())
        { return range_common_compare(t,u); }
        // 能否通用 compare
        template <typename T, typename U>
        constexpr bool can_common_compare = xyu::t_is_same<int, decltype(common_compare(xyu::t_val<const T&>(), xyu::t_val<const U&>(), '0'))>;
    }

    /**
     * @brief 通用相等比较
     *
     * @details
     * 根据 T 与 U 实际支持的操作，自动选择最合适的相等性判断方式。
     * 支持内置类型的内建运算符，也支持用户定义类型的重载运算符或成员函数。
     *
     * 选择优先级（从上到下依次尝试）：
     * - 直接使用 == 运算符；
     * - 使用 != 运算符；
     * - 调用成员函数 `.equals()`；
     * - 调用成员函数 `.compare()`；
     * - 通过 < 和 <= 运算符推导；
     * - 通过 > 和 >= 运算符推导；
     * - 通过 > 和 < 运算符推导；
     * - 通过 >= 和 <= 运算符推导；
     * - 如果 T 和 U 都是范围（range）类型，则进行逐元素比较。
     * - 若均不支持，则模板不会被选择
     */
    template <typename T, typename U, typename = xyu::t_enable<__::can_common_equals<T,U>>>
    constexpr bool equals(const T& t, const U& u) noexcept(noexcept(__::common_equals(t,u,'0')))
    { return __::common_equals(t,u,'0'); }

    /**
     * @brief 通用三路比较
     *
     * @details
     * 根据 T 与 U 实际支持的操作，自动生成一个符合三路比较语义的整数结果。
     * 支持内置类型的内建运算符，也支持用户定义类型的重载运算符或成员函数。
     *
     * 选择优先级（从上到下依次尝试）：
     * - 调用成员函数 `.compare()`；
     * - 通过 < 和 > 运算符推导；
     * - 通过 < 和 == 运算符推导；
     * - 通过 > 和 == 运算符推导；
     * - 通过 <= 和 >= 运算符推导；
     * - 通过 < 和 <= 运算符推导；
     * - 通过 > 和 >= 运算符推导；
     * - 如果 T 和 U 都是范围（range）类型，则进行逐元素比较。
     * - 若均不支持，则模板不会被选择
     */
    template <typename T, typename U, typename = xyu::t_enable<__::can_common_compare<T,U>>>
    constexpr int compare(const T& t, const U& u) noexcept(noexcept(__::common_compare(t,u,'0')))
    { return __::common_compare(t,u,'0'); }
    
    // 范围比较函数后置实现
    namespace __
    {
        // 范围比较异常检测
    #define XY_range_common_noexcept_gen( name ) \
        template <typename T, typename U> \
        constexpr bool range_common_##name##_noexcept() noexcept \
        { \
            using It = decltype(xyu::make_range(xyu::t_val<const T&>()).begin()); \
            using Iu = decltype(xyu::make_range(xyu::t_val<const U&>()).begin()); \
            if constexpr (!xyu::t_can_nothrow_range<const T&, const U&>) return false; \
            if constexpr (!noexcept(++xyu::t_val<It&>() != xyu::t_val<It&>())) return false; \
            if constexpr (!noexcept(++xyu::t_val<Iu&>() != xyu::t_val<Iu&>())) return false; \
            else return noexcept(name(*xyu::t_val<It&>(), *xyu::t_val<Iu&>())); \
        }
    XY_PP_FORWARD(XY_range_common_noexcept_gen, 1, equals, compare)
    #undef XY_range_common_noexcept_gen

        // 范围相等比较
        template <typename T, typename U>
        constexpr bool range_common_equals(const T& t, const U& u) noexcept(range_common_equals_noexcept<T,U>())
        {
            auto rt = xyu::make_range(t);
            auto ru = xyu::make_range(u);
            if (rt.count() != ru.count()) return false;
            auto bt = rt.begin(); auto et = rt.end();
            auto bu = ru.begin();
            while (bt != et)
            {
                if (!equals(*bt, *bu)) return false;
                ++bt; ++bu;
            }
            return true;
        }

        // 范围关系比较
        template <typename T, typename U>
        constexpr int range_common_compare(const T& t, const U& u) noexcept(range_common_compare_noexcept<T,U>())
        {
            auto rt = xyu::make_range(t);
            auto ru = xyu::make_range(u);
            auto bt = rt.begin(); auto et = rt.end();
            auto bu = ru.begin(); auto eu = ru.end();
            while (bt != et && bu != eu)
            {
                if (int r = compare(*bt, *bu); r != 0) return r;
                ++bt; ++bu;
            }
            return (bu == eu) - (bt == et);
        }
    }
}

/// 比较属性
namespace xylu::xytraits
{
    namespace __
    {
        template <typename T, typename U, typename = decltype(xyu::equals(xyu::t_val<const T&>(), xyu::t_val<const U&>()))>
        true_c can_equals_test(int);
        template <typename T, typename U>
        false_c can_equals_test(...);

        template <typename T, typename U, typename = decltype(xyu::compare(xyu::t_val<const T&>(), xyu::t_val<const U&>()))>
        true_c can_compare_test(int);
        template <typename T, typename U>
        false_c can_compare_test(...);
    }

    /// 判断是否支持 equals
    template <typename T, typename U>
    constexpr bool t_can_equals = decltype(__::can_equals_test<T,U>(0))::value;

    /// 判断是否支持 compare
    template <typename T, typename U>
    constexpr bool t_can_compare = decltype(__::can_compare_test<T,U>(0))::value;

}
