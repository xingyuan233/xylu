#pragma clang diagnostic push
#pragma ide diagnostic ignored "OCUnusedGlobalDeclarationInspection"
#pragma once

#include "../../link/range"

/**
 * @file
 * @brief 提供了全局的、可扩展的比较运算符 (`==`, `!=`, `<`, `>`, `>=`, `<=`) 和 `equals`/`compare` 函数。
 * @note 仅影响到本文件导入之后的使用
 * @details
 *   本文件旨在为 C++ 提供一个强大的、统一的比较框架。它通过重载全局的比较运算符，
 *   使其能够自动地、按优先级顺序地为任意类型 `T` 和 `U` 寻找最合适的比较方法。
 *
 *   ### 比较策略的优先级顺序:
 *
 *   1. **`equals()` / `compare()` 成员函数**:
 *      - `t.equals(u)` 或 `u.equals(t)`
 *      - `t.compare(u)` 或 `u.compare(t)` (返回 `int`)
 *
 *   2. **`equals()` / `compare()` ADL (Argument-Dependent Lookup) 函数**:
 *      - `equals(t, u)` 或 `equals(u, t)`
 *      - `compare(t, u)` 或 `compare(u, t)` (返回 `int`)
 *
 *   3. **范围比较 (Range Comparison)**:
 *      - 如果 `t` 和 `u` 都可以通过 `t.range()` 或 `make_range(t)` 转换为 `xylu::xyrange::Range`，
 *        则会对两个范围进行逐元素的字典序比较。
 *
 *   4. **内置运算符**:
 *      - 如果上述方法均不适用，则回退到使用语言内置的比较运算符 (`==`, `<`, etc.)。
 *
 *   这个框架使得任何类型，只要满足上述接口之一（例如，提供 `equals` 成员函数，或者使其可转换为 `Range`），
 *   就能自动获得全套的 `==`, `!=`, `<`, `<=`, `>`, `>=` 比较能力。
 *
 *   本框架的全局运算符重载会自动避开对内置整数和枚举类型的比较，以防止与编译器内置的高效比较产生歧义或冲突。
 */

/* 比较运算符 */

/** 全局运算符 */

// 模板检测
namespace xylu::xyrange::__
{
    template <typename R1, typename R2>
    constexpr bool range_impl_noexcept() noexcept
    {
        return noexcept(xyu::t_val<R1>() == xyu::t_val<R2>(), ++xyu::t_val<R1>(), ++xyu::t_val<R2>(), *xyu::t_val<R1>(), *xyu::t_val<R2>());
    }

    #define XY_RANGE_IMPL_NOEXCEPT( op ) \
                    noexcept(range_impl_noexcept<R1, R2>() && noexcept(xyu::t_val<R1>() op xyu::t_val<R2>()))
    // 迭代器范围比较
    template <typename R1, typename R2>
    constexpr bool range_impl_equal(R1 r1, R2 r2) XY_RANGE_IMPL_NOEXCEPT(==);
    template <typename R1, typename R2>
    constexpr bool range_impl_less(R1 r1, R2 r2) XY_RANGE_IMPL_NOEXCEPT(<);
    template <typename R1, typename R2>
    constexpr bool range_impl_greater(R1 r1, R2 r2) XY_RANGE_IMPL_NOEXCEPT(>);
    template <typename R1, typename R2>
    constexpr bool range_impl_less_equal(R1 r1, R2 r2) XY_RANGE_IMPL_NOEXCEPT(<=);
    template <typename R1, typename R2>
    constexpr bool range_impl_greater_equal(R1 r1, R2 r2) XY_RANGE_IMPL_NOEXCEPT(>=);

    // equals 模板匹配失败
    constexpr void equals_impl_test(...) noexcept {}
    // 调用 t.equals(u)
    template <typename T, typename U, typename = decltype(xyu::t_val<const T&>().equals(xyu::t_val<const U&>()))>
    constexpr bool equals_impl_test(const T& t, const U& u, char&&) noexcept(noexcept(t.equals(u))) { return t.equals(u); }
    // 调用 u.equals(t)
    template <typename T, typename U, typename = decltype(xyu::t_val<const U&>().equals(xyu::t_val<const T&>()))>
    constexpr bool equals_impl_test(const T& t, const U& u, const char&&) noexcept(noexcept(u.equals(t))) { return u.equals(t); }
    // 调用 equals(t,u)
    template <typename T, typename U, typename = decltype(equals(xyu::t_val<const T&>(), xyu::t_val<const U&>()))>
    constexpr bool equals_impl_test(const T& t, const U& u, const char&) noexcept(noexcept(equals(t, u))) { return equals(t, u); }
    // 调用 equals(u,t)
    template <typename T, typename U, typename = decltype(equals(xyu::t_val<const U&>(), xyu::t_val<const T&>()))>
    constexpr bool equals_impl_test(const T& t, const U& u, int) noexcept(noexcept(equals(u, t))) { return equals(u, t); }

    // compare 模板匹配失败
    constexpr void compare_impl_test(...) noexcept {}
    // 调用 t.compare(u)
    template <typename T, typename U, typename = decltype(xyu::t_val<const T&>().compare(xyu::t_val<const U&>()))>
    constexpr int compare_impl_test(const T& t, const U& u, char&&) noexcept(noexcept(t.compare(u))) { return t.compare(u); }
    // 调用 u.compare(t)
    template <typename T, typename U, typename = decltype(xyu::t_val<const U&>().compare(xyu::t_val<const T&>()))>
    constexpr int compare_impl_test(const T& t, const U& u, const char&&) noexcept(noexcept(u.compare(t))) { return -u.compare(t); }
    // 调用 compare(t,u)
    template <typename T, typename U, typename = decltype(compare(xyu::t_val<const T&>(), xyu::t_val<const U&>()))>
    constexpr int compare_impl_test(const T& t, const U& u, const char&) noexcept(noexcept(compare(t, u))) { return compare(t, u); }
    // 调用 compare(u,t)
    template <typename T, typename U, typename = decltype(compare(xyu::t_val<const U&>(), xyu::t_val<const T&>()))>
    constexpr int compare_impl_test(const T& t, const U& u, int) noexcept(noexcept(compare(u, t))) { return -compare(u, t); }

    // range 模板匹配失败
    xyu::int_c<-1> range_impl_test(...);
    // 调用 t.range() 和 u.range()
    template <typename T, typename U, typename = xyu::t_enable<xyu::t_is_range<decltype(xyu::t_val<const T&>().range())> && xyu::t_is_range<decltype(xyu::t_val<const U&>().range())>>>
    xyu::int_c<1> range_impl_test(const T& t, const U& u, char&&);
    // 调用 t.range() 和 make_range(u)
    template <typename T, typename U, typename = xyu::t_enable<xyu::t_is_range<decltype(xyu::t_val<const T&>().range())> && xyu::t_is_range<decltype(make_range(xyu::t_val<const U&>()))>>>
    xyu::int_c<2> range_impl_test(const T& t, const U& u, const char&&);
    // 调用 make_range(t) 和 u.range()
    template <typename T, typename U, typename = xyu::t_enable<xyu::t_is_range<decltype(make_range(xyu::t_val<const T&>()))> && xyu::t_is_range<decltype(xyu::t_val<const U&>().range())>>>
    xyu::int_c<3> range_impl_test(const T& t, const U& u, const char&);
    // 调用 make_range(t) 和 make_range(u)
    template <typename T, typename U, typename = xyu::t_enable<xyu::t_is_range<decltype(make_range(xyu::t_val<const T&>()))> && xyu::t_is_range<decltype(make_range(xyu::t_val<const U&>()))>>>
    xyu::int_c<4> range_impl_test(const T& t, const U& u, int);

    // 防止影响隐式转换为 int 比较
    template <typename T, typename U>
    constexpr bool is_enum_or_int = (xyu::t_is_enum<T> || xyu::t_is_integral<T> || xyu::t_can_icast<T, int>) &&
                                    (xyu::t_is_enum<U> || xyu::t_is_integral<U> || xyu::t_can_icast<U, int>);
}

// 异常检测
namespace xylu::xyrange::__
{
    template <typename T, typename U>
    constexpr bool global_operator_equal_noexcept() noexcept
    {
        using namespace xylu::xyrange::__;
        constexpr bool t1 = xyu::t_is_same<bool, decltype(equals_impl_test(xyu::t_val<const T&>(), xyu::t_val<const U&>(), '1'))>;
        if constexpr (t1) return noexcept(equals_impl_test(xyu::t_val<const T&>(), xyu::t_val<const U&>(), '1'));
        else {
            constexpr bool t2 = xyu::t_is_same<int, decltype(compare_impl_test(xyu::t_val<const T&>(), xyu::t_val<const U&>(), '2'))>;
            if constexpr (t2) return noexcept(compare_impl_test(xyu::t_val<const T&>(), xyu::t_val<const U&>(), '2'));
            else {
                using namespace xylu::xyrange;
                constexpr int t3 = decltype(range_impl_test(xyu::t_val<const T&>(), xyu::t_val<const U&>(), '3'))::value;
                if constexpr (t3 == 1) return noexcept(range_impl_equal(xyu::t_val<const T&>().range(), xyu::t_val<const U&>().range()));
                else if constexpr (t3 == 2) return noexcept(range_impl_equal(xyu::t_val<const T&>().range(), make_range(xyu::t_val<const U&>())));
                else if constexpr (t3 == 3) return noexcept(range_impl_equal(make_range(xyu::t_val<const T&>()), xyu::t_val<const U&>().range()));
                else return noexcept(range_impl_equal(make_range(xyu::t_val<const T&>()), make_range(xyu::t_val<const U&>())));
                return false;
            }
        }
    }

    #define XY_OPERATOR_NOEXCEPT(op) \
        noexcept([](){           \
            using namespace xylu::xyrange::__; \
            constexpr bool t1 = xyu::t_is_same<int, decltype(compare_impl_test(xyu::t_val<const T&>(), xyu::t_val<const U&>(), '1'))>; \
            if constexpr (t1) return noexcept(compare_impl_test(xyu::t_val<const T&>(), xyu::t_val<const U&>(), '1')); \
            else { \
                    using namespace xylu::xyrange; \
                    constexpr int t2 = decltype(range_impl_test(xyu::t_val<const T&>(), xyu::t_val<const U&>(), '2'))::value; \
                    if constexpr (t2 == 1) return noexcept(range_impl_op(xyu::t_val<const T&>().range(), xyu::t_val<const U&>().range())); \
                    else if constexpr (t2 == 2) return noexcept(range_impl_op(xyu::t_val<const T&>().range(), make_range(xyu::t_val<const U&>()))); \
                    else if constexpr (t2 == 3) return noexcept(range_impl_op(make_range(xyu::t_val<const T&>()), xyu::t_val<const U&>().range())); \
                    else return noexcept(range_impl_op(make_range(xyu::t_val<const T&>()), make_range(xyu::t_val<const U&>()))); \
                    return false;              \
            } \
        }())
}

#define XY_OPERATOR_TMPL , typename = xyu::t_enable<!xylu::xyrange::__::is_enum_or_int<T, U>>

// 全局运算符 == !=  <  <=  >  >=
template <typename T, typename U XY_OPERATOR_TMPL>
constexpr bool operator==(const T& t, const U& u) noexcept(xylu::xyrange::__::global_operator_equal_noexcept<T, U>())
{
    using namespace xylu::xyrange::__;
    constexpr bool t1 = xyu::t_is_same<bool, decltype(equals_impl_test(t, u, '1'))>;
    if constexpr (t1) return equals_impl_test(t, u, '1');
    else {
        constexpr bool t2 = xyu::t_is_same<int, decltype(compare_impl_test(t, u, '2'))>;
        if constexpr (t2) return compare_impl_test(t, u, '2') == 0;
        else {
            using namespace xylu::xyrange;
            constexpr int t3 = decltype(range_impl_test(t, u, '3'))::value;
            static_assert(t3 != -1, "failed to find a valid comparison method");
            if constexpr (t3 == 1) return range_impl_equal(t.range(), u.range());
            else if constexpr (t3 == 2) return range_impl_equal(t.range(), make_range(u));
            else if constexpr (t3 == 3) return range_impl_equal(make_range(t), u.range());
            else return range_impl_equal(make_range(t), make_range(u));
        }
    }
}

template <typename T, typename U XY_OPERATOR_TMPL>
constexpr bool operator!=(const T& t, const U& u) noexcept(xylu::xyrange::__::global_operator_equal_noexcept<T, U>())
{
    using namespace xylu::xyrange::__;
    constexpr bool t1 = xyu::t_is_same<bool, decltype(equals_impl_test(t, u, '1'))>;
    if constexpr (t1) return !equals_impl_test(t, u, '1');
    else {
        constexpr bool t2 = xyu::t_is_same<int, decltype(compare_impl_test(t, u, '2'))>;
        if constexpr (t2) return compare_impl_test(t, u, '2') != 0;
        else {
            using namespace xylu::xyrange;
            constexpr int t3 = decltype(range_impl_test(t, u, '3'))::value;
            static_assert(t3 != -1, "failed to find a valid comparison method");
            if constexpr (t3 == 1) return !range_impl_equal(t.range(), u.range());
            else if constexpr (t3 == 2) return !range_impl_equal(t.range(), make_range(u));
            else if constexpr (t3 == 3) return !range_impl_equal(make_range(t), u.range());
            else return !range_impl_equal(make_range(t), make_range(u));
        }
    }
}

template <typename T, typename U XY_OPERATOR_TMPL>
constexpr bool operator<(const T& t, const U& u) XY_OPERATOR_NOEXCEPT(less)
{
    using namespace xylu::xyrange::__;
    constexpr bool t1 = xyu::t_is_same<int, decltype(compare_impl_test(t, u, '1'))>;
    if constexpr (t1) return compare_impl_test(t, u, '1') < 0;
    else {
        using namespace xylu::xyrange;
        constexpr int t2 = decltype(range_impl_test(t, u, '2'))::value;
        static_assert(t2 != -1, "failed to find a valid comparison method");
        if constexpr (t2 == 1) return range_impl_less(t.range(), u.range());
        else if constexpr (t2 == 2) return range_impl_less(t.range(), make_range(u));
        else if constexpr (t2 == 3) return range_impl_less(make_range(t), u.range());
        else return range_impl_less(make_range(t), make_range(u));
    }
}

template <typename T, typename U XY_OPERATOR_TMPL>
constexpr bool operator<=(const T& t, const U& u) XY_OPERATOR_NOEXCEPT(less_equal)
{
    using namespace xylu::xyrange::__;
    constexpr bool t1 = xyu::t_is_same<int, decltype(compare_impl_test(t, u, '1'))>;
    if constexpr (t1) return compare_impl_test(t, u, '1') <= 0;
    else {
        using namespace xylu::xyrange;
        constexpr bool t2 = decltype(range_impl_test(t, u, '2'))::value;
        static_assert(t2 != -1, "failed to find a valid comparison method");
        if constexpr (t2 == 1) return range_impl_less_equal(t.range(), u.range());
        else if constexpr (t2 == 2) return range_impl_less_equal(t.range(), make_range(u));
        else if constexpr (t2 == 3) return range_impl_less_equal(make_range(t), u.range());
        else return range_impl_less_equal(make_range(t), make_range(u));
    }
}

template <typename T, typename U XY_OPERATOR_TMPL>
constexpr bool operator>(const T& t, const U& u) XY_OPERATOR_NOEXCEPT(greater)
{
    using namespace xylu::xyrange::__;
    constexpr bool t1 = xyu::t_is_same<int, decltype(compare_impl_test(t, u, '1'))>;
    if constexpr (t1) return compare_impl_test(t, u, '1') > 0;
    else {
        using namespace xylu::xyrange;
        constexpr int t2 = decltype(range_impl_test(t, u, '2'))::value;
        static_assert(t2 != -1, "failed to find a valid comparison method");
        if constexpr (t2 == 1) return range_impl_greater(t.range(), u.range());
        else if constexpr (t2 == 2) return range_impl_greater(t.range(), make_range(u));
        else if constexpr (t2 == 3) return range_impl_greater(make_range(t), u.range());
        else return range_impl_greater(make_range(t), make_range(u));
    }
}

template <typename T, typename U XY_OPERATOR_TMPL>
constexpr bool operator>=(const T& t, const U& u) XY_OPERATOR_NOEXCEPT(greater_equal)
{
    using namespace xylu::xyrange::__;
    constexpr bool t1 = xyu::t_is_same<int, decltype(compare_impl_test(t, u, '1'))>;
    if constexpr (t1) return compare_impl_test(t, u, '1') >= 0;
    else {
        using namespace xylu::xyrange;
        constexpr int t2 = decltype(range_impl_test(t, u, '2'))::value;
        static_assert(t2 != -1, "failed to find a valid comparison method");
        if constexpr (t2 == 1) return range_impl_greater_equal(t.range(), u.range());
        else if constexpr (t2 == 2) return range_impl_greater_equal(t.range(), make_range(u));
        else if constexpr (t2 == 3) return range_impl_greater_equal(make_range(t), u.range());
        else return range_impl_greater_equal(make_range(t), make_range(u));
    }
}

#undef XY_OPERATOR_TMPL

// 后置实现 (以确保支持递归调用上方的比较运算符)
namespace xylu::xyrange::__
{
    #undef XY_OPERATOR_NOEXCEPT

    template <typename R1, typename R2>
    constexpr bool range_impl_equal(R1 r1, R2 r2) XY_RANGE_IMPL_NOEXCEPT(==)
    {
        if (r1.count() != r2.count()) return false;
        auto b1 = r1.begin(), e1 = r1.end();
        auto b2 = r2.begin();
        while (b1 != e1)
        {
            if (*b1 != *b2) return false;
            ++b1, ++b2;
        }
        return true;
    }

    template <typename R1, typename R2>
    constexpr bool range_impl_less(R1 r1, R2 r2) XY_RANGE_IMPL_NOEXCEPT(<)
    {
        auto b1 = r1.begin(), e1 = r1.end();
        auto b2 = r2.begin(), e2 = r2.end();
        while (b1 != e1 && b2 != e2)
        {
            if (*b1 < *b2) return true;
            if (*b2 < *b1) return false;
            ++b1, ++b2;
        }
        return b1 == e1 && b2 != e2;
    }

    template <typename R1, typename R2>
    constexpr bool range_impl_greater(R1 r1, R2 r2) XY_RANGE_IMPL_NOEXCEPT(>)
    {
        auto b1 = r1.begin(), e1 = r1.end();
        auto b2 = r2.begin(), e2 = r2.end();
        while (b1 != e1 && b2 != e2)
        {
            if (*b1 > *b2) return true;
            if (*b2 > *b1) return false;
            ++b1, ++b2;
        }
        return b1 != e1 && b2 == e2;
    }

    template <typename R1, typename R2>
    constexpr bool range_impl_less_equal(R1 r1, R2 r2) XY_RANGE_IMPL_NOEXCEPT(<=)
    {
        auto b1 = r1.begin(), e1 = r1.end();
        auto b2 = r2.begin(), e2 = r2.end();
        while (b1 != e1 && b2 != e2)
        {
            if (*b1 < *b2) return true;
            if (*b2 < *b1) return false;
            ++b1, ++b2;
        }
        return b1 == e1;
    }

    template <typename R1, typename R2>
    constexpr bool range_impl_greater_equal(R1 r1, R2 r2) XY_RANGE_IMPL_NOEXCEPT(>=)
    {
        auto b1 = r1.begin(), e1 = r1.end();
        auto b2 = r2.begin(), e2 = r2.end();
        while (b1 != e1 && b2 != e2)
        {
            if (*b1 > *b2) return true;
            if (*b2 > *b1) return false;
            ++b1, ++b2;
        }
        return b2 == e2;
    }

    #undef XY_RANGE_IMPL_NOEXCEPT
}


/** equals & compare */

// 模板检测
namespace xylu::xyrange::__
{
    template <typename R1, typename R2>
    constexpr bool range_impl_equals_noexcept() noexcept;
    template <typename R1, typename R2>
    constexpr bool range_impl_compare_noexcept() noexcept;

    template <typename R1, typename R2>
    constexpr bool range_impl_equals(R1 r1, R2 r2) noexcept(range_impl_equals_noexcept<R1, R2>());
    template <typename R1, typename R2>
    constexpr int range_impl_compare(R1 r1, R2 r2) noexcept(range_impl_compare_noexcept<R1, R2>());

    // 测试 == / !=
    xyu::int_c<-1> operator_equal_impl_test(...);
    template <typename T, typename U, typename = decltype(xyu::t_val<T>() == xyu::t_val<U>())>
    xyu::int_c<0> operator_equal_impl_test(const T& t, const U& u, char);
    template <typename T, typename U, typename = decltype(xyu::t_val<T>() != xyu::t_val<U>())>
    xyu::int_c<1> operator_equal_impl_test(const T& t, const U& u, int);

    // 测试 < / >=
    xyu::int_c<-1> operator_less_impl_test(...);
    template <typename T, typename U, typename = decltype(xyu::t_val<T>() <  xyu::t_val<U>())>
    xyu::int_c<0> operator_less_impl_test(const T& t, const U& u, char);
    template <typename T, typename U, typename = decltype(xyu::t_val<T>() >= xyu::t_val<U>())>
    xyu::int_c<1> operator_less_impl_test(const T& t, const U& u, int);

    // 测试 > / <=
    xyu::int_c<-1> operator_greater_impl_test(...);
    template <typename T, typename U, typename = decltype(xyu::t_val<T>() >  xyu::t_val<U>())>
    xyu::int_c<0> operator_greater_impl_test(const T& t, const U& u, char);
    template <typename T, typename U, typename = decltype(xyu::t_val<T>() <= xyu::t_val<U>())>
    xyu::int_c<1> operator_greater_impl_test(const T& t, const U& u, int);
}

// 异常检测
namespace xylu::xyrange::__
{
    template <typename T, typename U>
    constexpr bool global_operator_equals_noexcept() noexcept
    {
        using namespace xylu::xyrange::__;
        constexpr bool t1 = []{
            if constexpr (xyu::t_is_class<T, U>) return xyu::t_is_same<bool, decltype(equals_impl_test(xyu::t_val<T>(), xyu::t_val<U>(), '1'))>;
            else return false;
        }();
        if constexpr (t1) return noexcept(equals_impl_test(xyu::t_val<T>(), xyu::t_val<U>(), '1'));
        else {
            constexpr bool t2 = xyu::t_is_same<int, decltype(compare_impl_test(xyu::t_val<T>(), xyu::t_val<U>(), '3'))>;
            if constexpr (t2) return noexcept(compare_impl_test(xyu::t_val<T>(), xyu::t_val<U>(), '2'));
            else {
                using namespace xylu::xyrange;
                constexpr int t3 = decltype(range_impl_test(xyu::t_val<T>(), xyu::t_val<U>(), '3'))::value;
                if constexpr (t3 == 1) return noexcept(range_impl_equals(xyu::t_val<T>().range(), xyu::t_val<U>().range()));
                else if constexpr (t3 == 2) return noexcept(range_impl_equals(xyu::t_val<T>().range(), make_range(xyu::t_val<U>())));
                else if constexpr (t3 == 3) return noexcept(range_impl_equals(make_range(xyu::t_val<T>()), xyu::t_val<U>().range()));
                else if constexpr (t3 == 4) return noexcept(range_impl_equals(make_range(xyu::t_val<T>()), make_range(xyu::t_val<U>())));
                else {
                    constexpr int t4 = decltype(operator_equal_impl_test(xyu::t_val<T>(), xyu::t_val<U>(), '4'))::value;
                    if constexpr (t4 == 0) return noexcept(xyu::t_val<T>() == xyu::t_val<U>());
                    else if constexpr (t4 == 1) return noexcept(xyu::t_val<T>() != xyu::t_val<U>());
                    else {
                        constexpr int t5_1 = decltype(operator_less_impl_test(xyu::t_val<T>(), xyu::t_val<U>(), '5'))::value;
                        constexpr int t5_2 = decltype(operator_greater_impl_test(xyu::t_val<T>(), xyu::t_val<U>(), '5'))::value;
                        if constexpr (t5_1 == 0 && t5_2 == 0) return noexcept(!(xyu::t_val<T>() < xyu::t_val<U>()), xyu::t_val<T>() > xyu::t_val<U>());
                        else if constexpr (t5_1 == 0 && t5_2 == 1) return noexcept(xyu::t_val<T>() < xyu::t_val<U>(), xyu::t_val<T>() <= xyu::t_val<U>());
                        else if constexpr (t5_1 == 1 && t5_2 == 0) return noexcept(xyu::t_val<T>() >= xyu::t_val<U>() && xyu::t_val<T>() > xyu::t_val<U>());
                        else if constexpr (t5_1 == 1 && t5_2 == 1) return noexcept(xyu::t_val<T>() >= xyu::t_val<U>() && xyu::t_val<T>() <= xyu::t_val<U>());
                        else return false;
                    }
                }
            }
        }
    }

    template <typename T, typename U>
    constexpr bool global_operator_compare_noexcept() noexcept
    {
        using namespace xylu::xyrange::__;
        constexpr bool t1 = []{
            if constexpr (xyu::t_is_class<T, U>) xyu::t_is_same<int, decltype(compare_impl_test(xyu::t_val<T>(), xyu::t_val<U>(), '1'))>;
            else return false;
        }();
        if constexpr (t1) return noexcept(compare_impl_test(xyu::t_val<T>(), xyu::t_val<U>(), '1'));
        else {
            using namespace xylu::xyrange;
            constexpr int t2 = decltype(range_impl_test(xyu::t_val<T>(), xyu::t_val<U>(), '2'))::value;
            if constexpr (t2 == 1) return noexcept(range_impl_compare(xyu::t_val<T>().range(), xyu::t_val<U>().range()));
            else if constexpr (t2 == 2) return noexcept(range_impl_compare(xyu::t_val<T>().range(), make_range(xyu::t_val<U>())));
            else if constexpr (t2 == 3) return noexcept(range_impl_compare(make_range(xyu::t_val<T>()), xyu::t_val<U>().range()));
            else if constexpr (t2 == 4) return noexcept(range_impl_compare(make_range(xyu::t_val<T>()), make_range(xyu::t_val<U>())));
            else {
                constexpr int t3_1 = decltype(operator_equal_impl_test(xyu::t_val<T>(), xyu::t_val<U>(), '3'))::value;
                constexpr int t3_2 = decltype(operator_less_impl_test(xyu::t_val<T>(), xyu::t_val<U>(), '3'))::value;
                constexpr int t3_3 = decltype(operator_greater_impl_test(xyu::t_val<T>(), xyu::t_val<U>(), '3'))::value;
                if constexpr (t3_1 == 0 && t3_2 == 0) return noexcept(xyu::t_val<T>() == xyu::t_val<U>(), xyu::t_val<T>() < xyu::t_val<U>());
                else if constexpr (t3_1 == 0 && t3_2 == 1) return noexcept(xyu::t_val<T>() == xyu::t_val<U>(), xyu::t_val<T>() >= xyu::t_val<U>());
                else if constexpr (t3_1 == 0 && t3_3 == 0) return noexcept(xyu::t_val<T>() == xyu::t_val<U>(), xyu::t_val<T>() > xyu::t_val<U>());
                else if constexpr (t3_1 == 0 && t3_3 == 1) return noexcept(xyu::t_val<T>() == xyu::t_val<U>(), xyu::t_val<T>() <= xyu::t_val<U>());
                else if constexpr (t3_1 == 1 && t3_2 == 0) return noexcept(xyu::t_val<T>() != xyu::t_val<U>(), xyu::t_val<T>() < xyu::t_val<U>());
                else if constexpr (t3_1 == 1 && t3_2 == 1) return noexcept(xyu::t_val<T>() != xyu::t_val<U>(), xyu::t_val<T>() >= xyu::t_val<U>());
                else if constexpr (t3_1 == 1 && t3_3 == 0) return noexcept(xyu::t_val<T>() != xyu::t_val<U>(), xyu::t_val<T>() > xyu::t_val<U>());
                else if constexpr (t3_1 == 1 && t3_3 == 1) return noexcept(xyu::t_val<T>() != xyu::t_val<U>(), xyu::t_val<T>() <= xyu::t_val<U>());
                else if constexpr (t3_2 == 0 && t3_3 == 0) return noexcept(xyu::t_val<T>() < xyu::t_val<U>(), xyu::t_val<T>() > xyu::t_val<U>());
                else if constexpr (t3_2 == 0 && t3_3 == 1) return noexcept(xyu::t_val<T>() < xyu::t_val<U>(), xyu::t_val<T>() <= xyu::t_val<U>());
                else if constexpr (t3_2 == 1 && t3_3 == 0) return noexcept(xyu::t_val<T>() >= xyu::t_val<U>(), xyu::t_val<T>() > xyu::t_val<U>());
                else if constexpr (t3_2 == 1 && t3_3 == 1) return noexcept(xyu::t_val<T>() >= xyu::t_val<U>(), xyu::t_val<T>() <= xyu::t_val<U>());
                else return false;
            }
    }
}
}

// equals & compare 实现
namespace xyu
{
    template <typename T, typename U>
    constexpr bool equals(const T& t, const U& u) noexcept(xylu::xyrange::__::global_operator_equals_noexcept<T, U>())
    {
        using namespace xylu::xyrange::__;
        constexpr bool t1 = []{
           if constexpr (xyu::t_is_class<T, U>) return xyu::t_is_same<bool, decltype(equals_impl_test(t, u, '1'))>;
           else return false;
        }();
        if constexpr (t1) return equals_impl_test(t, u, '1');
        else {
            constexpr bool t2 = xyu::t_is_same<int, decltype(compare_impl_test(t, u, '3'))>;
            if constexpr (t2) return compare_impl_test(t, u, '2') == 0;
            else {
                using namespace xylu::xyrange;
                constexpr int t3 = decltype(range_impl_test(t, u, '3'))::value;
                if constexpr (t3 == 1) return range_impl_equals(t.range(), u.range());
                else if constexpr (t3 == 2) return range_impl_equals(t.range(), make_range(u));
                else if constexpr (t3 == 3) return range_impl_equals(make_range(t), u.range());
                else if constexpr (t3 == 4) return range_impl_equals(make_range(t), make_range(u));
                else {
                    constexpr int t4 = decltype(operator_equal_impl_test(t, u, '4'))::value;
                    if constexpr (t4 == 0) return t == u;
                    else if constexpr (t4 == 1) return !(t != u);
                    else {
                        constexpr int t5_1 = decltype(operator_less_impl_test(t, u, '5'))::value;
                        constexpr int t5_2 = decltype(operator_greater_impl_test(t, u, '5'))::value;
                        if constexpr (t5_1 == 0 && t5_2 == 0) return !(t < u) && !(t > u);
                        else if constexpr (t5_1 == 0 && t5_2 == 1) return !(t < u) && (t <= u);
                        else if constexpr (t5_1 == 1 && t5_2 == 0) return (t >= u) && !(t > u);
                        else if constexpr (t5_1 == 1 && t5_2 == 1) return (t >= u) && (t <= u);
                        else static_assert(t5_1 == 404, "failed to find a valid comparison method");
                    }
                }
            }
        }
    }

    template <typename T, typename U>
    constexpr int compare(const T& t, const U& u) noexcept(xylu::xyrange::__::global_operator_compare_noexcept<T, U>())
    {
        using namespace xylu::xyrange::__;
        constexpr bool t1 = []{
            if constexpr (xyu::t_is_class<T, U>) return xyu::t_is_same<int, decltype(compare_impl_test(t, u, '1'))>;
            else return false;
        }();
        if constexpr (t1) return compare_impl_test(t, u, '1');
        else {
            using namespace xylu::xyrange;
            constexpr int t2 = decltype(range_impl_test(t, u, '2'))::value;
            if constexpr (t2 == 1) return range_impl_compare(t.range(), u.range());
            else if constexpr (t2 == 2) return range_impl_compare(t.range(), make_range(u));
            else if constexpr (t2 == 3) return range_impl_compare(make_range(t), u.range());
            else if constexpr (t2 == 4) return range_impl_compare(make_range(t), make_range(u));
            else {
                constexpr int t3_1 = decltype(operator_equal_impl_test(t, u, '3'))::value;
                constexpr int t3_2 = decltype(operator_less_impl_test(t, u, '3'))::value;
                constexpr int t3_3 = decltype(operator_greater_impl_test(t, u, '3'))::value;
                if constexpr (t3_1 == 0 && t3_2 == 0) return t == u ? 0 : (t < u ? -1 : 1);
                else if constexpr (t3_1 == 0 && t3_2 == 1) return t == u ? 0 : (t >= u ? 1 : -1);
                else if constexpr (t3_1 == 0 && t3_3 == 0) return t == u ? 0 : (t > u ? 1 : -1);
                else if constexpr (t3_1 == 0 && t3_3 == 1) return t == u ? 0 : (t <= u ? -1 : 1);
                else if constexpr (t3_1 == 1 && t3_2 == 0) return t != u ? (t < u ? -1 : 1) : 0;
                else if constexpr (t3_1 == 1 && t3_2 == 1) return t != u ? (t >= u ? 1 : -1) : 0;
                else if constexpr (t3_1 == 1 && t3_3 == 0) return t != u ? (t > u ? 1 : -1) : 0;
                else if constexpr (t3_1 == 1 && t3_3 == 1) return t != u ? (t <= u ? -1 : 1) : 0;
                else if constexpr (t3_2 == 0 && t3_3 == 0) return t < u ? -1 : (t > u ? 1 : 0);
                else if constexpr (t3_2 == 0 && t3_3 == 1) return t < u ? -1 : (t <= u ? 0 : 1);
                else if constexpr (t3_2 == 1 && t3_3 == 0) return t >= u ? (t > u ? 1 : 0) : -1;
                else if constexpr (t3_2 == 1 && t3_3 == 1) return t >= u ? (t <= u ? 0 : 1) : -1;
                else static_assert(t3_1 == 404, "failed to find a valid comparison method");
            }
        }
    }
}

// 后置实现 (以确保支持递归调用)
namespace xylu::xyrange::__
{
    template <typename R1, typename R2>
    constexpr bool range_impl_equals_noexcept() noexcept
    {
        return range_impl_noexcept<R1, R2>() && noexcept(xyu::equals(*xyu::t_val<R1>(), *xyu::t_val<R2>()));
    }
    template <typename R1, typename R2>
    constexpr bool range_impl_compare_noexcept() noexcept
    {
        return range_impl_noexcept<R1, R2>() && noexcept(xyu::compare(*xyu::t_val<R1>(), *xyu::t_val<R2>()));
    }
    
    template <typename R1, typename R2>
    constexpr bool range_impl_equals(R1 r1, R2 r2) noexcept(range_impl_equals_noexcept<R1, R2>())
    {
        if (r1.count() != r2.count()) return false;
        auto b1 = r1.begin(), e1 = r1.end();
        auto b2 = r2.begin();
        while (b1 != e1)
        {
            if (!xyu::equals(*b1, *b2)) return false;
            ++b1, ++b2;
        }
        return true;
    }

    template <typename R1, typename R2>
    constexpr int range_impl_compare(R1 r1, R2 r2) noexcept(range_impl_compare_noexcept<R1, R2>())
    {
        auto b1 = r1.begin(), e1 = r1.end();
        auto b2 = r2.begin(), e2 = r2.end();
        while (b1 != e1 && b2 != e2)
        {
            int cmp = xyu::compare(*b1, *b2);
            if (cmp != 0) return cmp;
            ++b1, ++b2;
        }
        return (b2 == e2) - (b1 == e1);
    }
}


#pragma clang diagnostic pop