#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wdeprecated-builtins"
#pragma ide diagnostic ignored "OCUnusedMacroInspection"
#pragma ide diagnostic ignored "OCUnusedGlobalDeclarationInspection"
#pragma once

#include "./kind.h"
#include "./layout.h"

/* 类|结构体属性 */

/** 继承 */
namespace xylu::xytraits
{
    /**
     * @brief 检查类型 Base 是否为类型 Derived 的基类
     * @tparam Base      可能为基类的类型。
     * @tparam Derived   可能为派生类的类型。
     */
    template <typename Base, typename Derived>
    constexpr bool t_is_base_of = __is_base_of(Base, Derived);
}

/** 构造、赋值、拷贝、移动 */
namespace xylu::xytraits
{
    /**
     * @brief 检查类型 T 能否以 T v(args...) 的方式使用 Args... 进行构造。
     * @details 此检查允许发生隐式类型转换。若 Args... 为空，则检查默认构造。
     */
    template <typename T, typename... Args>
    constexpr bool t_can_construct = __is_constructible(T, Args...);

    namespace __
    {
        template <typename T, typename... Args, typename = decltype(T{t_val<Args>()...})>
        true_c can_listinit_test(int);
        template <typename, typename...>
        false_c can_listinit_test(...);
    }
    /**
     * @brief 检查类型 T 能否以 T v{args...} 的方式使用 Args... 进行列表初始化。
     * @details 列表初始化是一种更严格的初始化形式，通常不允许窄化转换。
     */
    template <typename T, typename... Args>
    constexpr bool t_can_listinit = decltype(__::can_listinit_test<T, Args...>(0))::value;

    namespace __
    {
        template <typename T, typename... Args, typename = decltype(T(t_val<Args>()...))>
        true_c can_init_test(int);
        template <typename T, typename... Args, typename = decltype(T{t_val<Args>()...})>
        true_c can_init_test(long);
        template <typename, typename...>
        false_c can_init_test(...);
    }
    /**
     * @brief 检查类型 T 能否以 T(args...) 或 T{args...} 的方式使用 Args... 进行初始化。
     * @details 先检测 T(args...)，再检测 T{args...}。
     */
    template <typename T, typename... Args>
    constexpr bool t_can_init = decltype(__::can_init_test<T, Args...>(0))::value;

    /// 检查类型 T 的实例能否被 V... 中的所有类型进行赋值。
    template <typename T, typename... V>
    constexpr bool t_can_assign = (... && __is_assignable(T, V));

    /// 检查所有给定类型能否进行默认构造。
    template <typename... T>
    constexpr bool t_can_dfconstr = (... && t_can_construct<T>);

    /// 检查所有给定类型能否进行拷贝构造。
    template <typename... T>
    constexpr bool t_can_cpconstr = (... && (t_can_refer<T> && t_can_construct<T, const T&>));

    /// 检查所有给定类型能否进行移动构造。
    template <typename... T>
    constexpr bool t_can_mvconstr = (... && (t_can_refer<T> && t_can_construct<T, T&&>));

    /// 检查所有给定类型能否进行拷贝赋值。
    template <typename... T>
    constexpr bool t_can_cpassign = (... && t_can_assign<T&, const T&>);

    /// 检查所有给定类型能否进行移动赋值。
    template <typename... T>
    constexpr bool t_can_mvassign = (... && t_can_assign<T&, T&&>);
}

/** 异常 */
namespace xylu::xytraits
{
    /// 检查类型 T 能否使用 Args... 进行无异常构造。
    template <typename T, typename... Args>
    constexpr bool t_can_nothrow_construct = __is_nothrow_constructible(T, Args...);

    namespace __
    {
        template <typename T, typename... Args>
        bool_c<noexcept(T{t_val<Args>()...})> can_nothrow_listinit_test(int);
        template <typename, typename...>
        false_c can_nothrow_listinit_test(...);
    }
    /// 检查类型 T 能否使用 Args... 进行无异常的列表初始化。
    template <typename T, typename... Args>
    constexpr bool t_can_nothrow_listinit = decltype(__::can_nothrow_listinit_test<T, Args...>(0))::value;

    namespace __
    {
        template <typename T, typename... Args>
        bool_c<noexcept(T(t_val<Args>()...))> can_nothrow_init_test(int);
        template <typename T, typename... Args>
        bool_c<noexcept(T{t_val<Args>()...})> can_nothrow_init_test(long);
        template <typename, typename...>
        false_c can_nothrow_init_test(...);
    }
    /// 检查类型 T 能否使用 Args... 进行无异常的初始化。
    template <typename T, typename... Args>
    constexpr bool t_can_nothrow_init = decltype(__::can_nothrow_init_test<T, Args...>(0))::value;

    /// 检查类型 T 的实例能否被 V... 中的所有类型进行无异常的赋值。
    template <typename T, typename... V>
    constexpr bool t_can_nothrow_assign = (... && __is_nothrow_assignable(T, V));

    namespace __
    {
        template <typename T>
        bool_c<noexcept(t_val<T&>().~T())> _can_nothrow_destruct_test(int);
        template<typename>
        false_c _can_nothrow_destruct_test(...);

        template <typename T>
        constexpr bool can_nothrow_destruct = t_is_refer<T> || t_is_barray<T> || t_is_function<T> 
                || decltype(__::_can_nothrow_destruct_test<T>(0))::value;
    }
    /**
     * @brief 检查类型能否无异常地析构。
     * @details 对于类类型，这会检查其析构函数是否被标记为 `noexcept`。
     *          对于引用、函数等没有析构行为的类型，此检查也为 `true`。
     */
    template <typename... T>
    constexpr bool t_can_nothrow_destruct = (... && __::can_nothrow_destruct<T>);

    /// 检查所有给定类型能否无异常地默认构造。
    template <typename... T>
    constexpr bool t_can_nothrow_dfconstr = (... && __is_nothrow_constructible(T));

    /// 检查所有给定类型能否无异常地拷贝构造。
    template <typename... T>
    constexpr bool t_can_nothrow_cpconstr = (... && (t_can_refer<T> && t_can_nothrow_construct<T, const T&>));

    /// 检查所有给定类型能否无异常地移动构造。
    template <typename... T>
    constexpr bool t_can_nothrow_mvconstr = (... && (t_can_refer<T> && t_can_nothrow_construct<T, T&&>));

    /// 检查所有给定类型能否无异常地拷贝赋值。
    template <typename... T>
    constexpr bool t_can_nothrow_cpassign = (... && t_can_nothrow_assign<T&, const T&>);

    /// 检查所有给定类型能否无异常地移动赋值。
    template <typename... T>
    constexpr bool t_can_nothrow_mvassign = (... && t_can_nothrow_assign<T&, T&&>);
}

/** 平凡性 */
// (注: 平凡的操作保证是 noexcept 的)
namespace xylu::xytraits
{
    /// 检查类型 T 能否使用 Args... 进行平凡构造。
    template<typename T, typename... Args>
    constexpr bool t_can_trivial_construct = __is_trivially_constructible(T, Args...);

    /// 检查类型 T 的实例能否被 V... 中的所有类型进行平凡的赋值。
    template<typename T, typename... V>
    constexpr bool t_can_trivial_assign = (... && __is_trivially_assignable(T, V));

    /// 检查所有给定类型能否平凡地析构。
    template <typename... T>
    constexpr bool t_can_trivial_destruct = (... && __has_trivial_destructor(T));

    /// 检查所有给定类型能否平凡地默认构造。
    template <typename... T>
    constexpr bool t_can_trivial_dfconstr = (... && __is_trivially_constructible(T));

    /// 检查所有给定类型能否平凡地拷贝构造。
    template <typename... T>
    constexpr bool t_can_trivial_cpconstr = (... && (t_can_refer<T> && t_can_trivial_construct<T, const T&>));

    /// 检查所有给定类型能否平凡地移动构造。
    template <typename... T>
    constexpr bool t_can_trivial_mvconstr = (... && (t_can_refer<T> && t_can_trivial_construct<T, T&&>));

    /// 检查所有给定类型能否平凡地拷贝赋值。
    template <typename... T>
    constexpr bool t_can_trivial_cpassign = (... && t_can_trivial_assign<T&, const T&>);

    /// 检查所有给定类型能否平凡地移动赋值。
    template <typename... T>
    constexpr bool t_can_trivial_mvassign = (... && t_can_trivial_assign<T&, T&&>);
}

#pragma clang diagnostic pop