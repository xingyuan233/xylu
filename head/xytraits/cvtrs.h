#pragma clang diagnostic push
#pragma ide diagnostic ignored "OCUnusedTypeAliasInspection"
#pragma ide diagnostic ignored "OCUnusedGlobalDeclarationInspection"
#pragma once

#include "./base.h"

/* 类型 转换 引用 修饰 比较 相关 */

/** 类型转换 */
namespace xylu::xytraits
{
    namespace __
    {
        template <typename To>
        void can_implicit_cast_test_fun(To from) noexcept;
        template <typename From, typename To, typename = decltype(can_implicit_cast_test_fun<To>(t_val<From>()))>
        true_c can_implicit_cast_test(int) noexcept(noexcept(can_implicit_cast_test_fun<To>(t_val<From>())));
        template <typename, typename>
        false_c can_implicit_cast_test(...);
    }
    /**
     * @brief 编译期检查类型 From 能否隐式转换为类型 To。
     * @tparam From 源类型。
     * @tparam To   目标类型。
     */
    template <typename From, typename To>
    constexpr bool t_can_icast = decltype(__::can_implicit_cast_test<From, To>(0))::value;

    /**
     * @brief 编译期检查类型 From 能否 无异常地 隐式转换为类型 To。
     * @details 建立在 t_can_icast 的基础上，并使用 `noexcept` 操作符检查转换过程是否标记为 noexcept。
     * @see t_can_icast
     * @tparam From 源类型。
     * @tparam To   目标类型。
     */
    template <typename From, typename To>
    constexpr bool t_can_nothrow_icast = noexcept(__::can_implicit_cast_test<From, To>(0));
}

/** 引用 */
namespace xylu::xytraits
{
    namespace __
    {
        template <typename T>
        struct remove_lvalue_refer { using type = T; };
        template <typename T>
        struct remove_lvalue_refer<T&> { using type = T; };
    }
    /// 移除类型的 左值引用 修饰，若存在。
    template <typename T>
    using t_remove_lrefer = typename __::remove_lvalue_refer<T>::type;

    namespace __
    {
        template <typename T>
        struct remove_rvalue_refer { using type = T; };
        template <typename T>
        struct remove_rvalue_refer<T&&> { using type = T; };
    }
    /// 移除类型的 右值引用 修饰，若存在。
    template <typename T>
    using t_remove_rrefer = typename __::remove_rvalue_refer<T>::type;

    namespace __
    {
        template <typename T>
        struct remove_refer { using type = T; };
        template <typename T>
        struct remove_refer<T&> { using type = T; };
        template <typename T>
        struct remove_refer<T&&> { using type = T; };
    }
    /// 移除类型的 左值或右值引用 饰词，若存在。
    template <typename T>
    using t_remove_refer = typename __::remove_refer<T>::type;

    namespace __
    {
        template <typename T>
        struct is_lvalue_refer : false_c {};
        template <typename T>
        struct is_lvalue_refer<T&> : true_c {};
    }
    /// 检查所有给定类型是否均为 左值引用。
    template <typename... T>
    constexpr bool t_is_lrefer = (... && __::is_lvalue_refer<T>::value);

    namespace __
    {
        template <typename T>
        struct is_rvalue_refer : false_c {};
        template <typename T>
        struct is_rvalue_refer<T&&> : true_c {};
    }
    /// 检查所有给定类型是否均为 右值引用。
    template <typename... T>
    constexpr bool t_is_rrefer = (... && __::is_rvalue_refer<T>::value);

    namespace __
    {
        template <typename T>
        struct is_refer : false_c {};
        template <typename T>
        struct is_refer<T&> : true_c {};
        template <typename T>
        struct is_refer<T&&> : true_c {};
    }
    /// 检查所有给定类型是否均为 引用类型 (左值或右值)。
    template <typename... T>
    constexpr bool t_is_refer = (... && __::is_refer<T>::value);

    namespace __
    {
        template <typename T>
        decltype(t_val<T&>(), true_c{}) can_refer_test(int);
        template <typename T>
        false_c can_refer_test(...);
    }
    /**
     * @brief 检查一个类型 T 能否被引用 (T&)。
     * @note 主要用于排除 void类型 以及 xyu::nullptr_t。
     */
    template <typename... T>
    constexpr bool t_can_refer = (... && decltype(__::can_refer_test<T>(0))::value);
}

/** cv修饰符 */
namespace xylu::xytraits
{
    namespace __
    {
        template <typename T>
        struct remove_const { using type = T; };
        template <typename T>
        struct remove_const<T const> { using type = T; };
    }
    /// 移除类型的顶层 const 修饰。
    template <typename T>
    using t_remove_const = typename __::remove_const<T>::type;

    namespace __
    {
        template <typename T>
        struct remove_volatile { using type = T; };
        template <typename T>
        struct remove_volatile<T volatile> { using type = T; };
    }
    /// 移除类型的顶层 volatile 修饰。
    template <typename T>
    using t_remove_volatile = typename __::remove_volatile<T>::type;

    namespace __
    {
        template <typename T>
        struct remove_cv { using type = T; };
        template <typename T>
        struct remove_cv<T const> { using type = T; };
        template <typename T>
        struct remove_cv<T volatile> { using type = T; };
        template <typename T>
        struct remove_cv<T const volatile> { using type = T; };
    }
    /// 移除类型的顶层 const 和 volatile 修饰。
    template <typename T>
    using t_remove_cv = typename __::remove_cv<T>::type;

    namespace __
    {
        template <typename T>
        struct is_const : false_c {};
        template <typename T>
        struct is_const<T const> : true_c {};
    }
    /// 检查所有给定类型是否有顶层 const 修饰。
    template <typename... T>
    constexpr bool t_is_const = (... && __::is_const<T>::value);

    namespace __
    {
        template <typename T>
        struct is_volatile : false_c {};
        template <typename T>
        struct is_volatile<T volatile> : true_c {};
    }
    /// 检查所有给定类型是否有顶层 volatile 修饰。
    template <typename... T>
    constexpr bool t_is_volatile = (... && __::is_volatile<T>::value);
}

/** 类型比较 */
namespace xylu::xytraits
{
    namespace __
    {
        template <typename T, typename U>
        struct is_same : false_c {};
        template <typename T>
        struct is_same<T, T> : true_c {};
    }
    /// 检查所有给定类型是否均相同
    template <typename T, typename... U>
    constexpr bool t_is_same = (... && __::is_same<T, U>::value);
}

/** 复合操作 */
namespace xylu::xytraits
{
    /// 移除类型的顶层 cv修饰和引用。
    template <typename T>
    using t_remove_cvref = t_remove_cv<t_remove_refer<T>>;

    namespace __
    {
        template <typename T>
        struct add_const_under_refer { using type = const T; };
        template <typename T>
        struct add_const_under_refer<T&> { using type = const T&; };
        template <typename T>
        struct add_const_under_refer<T&&> { using type = const T&&; };
    }
    /**
     * @brief 为引用所指向的类型添加 const。
     * @return 若 T = U&/U&&，返回 const U&/const U&&；若 T = U，返回 const U。
     */
    template <typename T>
    using t_add_const_inref = typename __::add_const_under_refer<T>::type;

    namespace __
    {
        template <typename T>
        struct remove_const_under_refer { using type = const T; };
        template <typename T>
        struct remove_const_under_refer<const T> { using type = T; };
        template <typename T>
        struct remove_const_under_refer<const T&> { using type = T&; };
        template <typename T>
        struct remove_const_under_refer<const T&&> { using type = T&&; };
    }
    /**
     * @brief 移除引用所指向类型的 const。
     * @return 若 T = const U&/const U&&，返回 U&/U&&；若 T = const U，返回 U。
     */
    template <typename T>
    using t_remove_const_inref = typename __::remove_const_under_refer<T>::type;

    namespace __
    {
        template <typename T>
        struct add_const_under_pointer_base { using type = const T; };
        template <typename T>
        struct add_const_under_pointer_base<T*> { using type = const T*; };
    }
    /**
     * @brief 为指针所指向的类型添加 const (变为指向常量的指针)。
     * @return 若 T = U*，返回 const U*；若 T = U，返回 const U。
     */
    template <typename T>
    using t_add_const_inptr = typename __::add_const_under_pointer_base<T>::type;

    namespace __
    {
        template <typename T>
        struct remove_const_under_pointer_base { using type = T; };
        template <typename T>
        struct remove_const_under_pointer_base<const T> { using type = T; };
        template <typename T>
        struct remove_const_under_pointer_base<const T*> { using type = T*; };
    }
    /**
     * @brief 移除指针所指向类型的 const。
     * @return 若 T = const U*，返回 U*；若 T = const U，返回 U。
     */
    template <typename T>
    using t_remove_const_inptr = typename __::remove_const_under_pointer_base<T>::type;

    /// 检查所有给定类型在移除 cv修饰 后是否相同。
    template <typename T, typename... U>
    constexpr bool t_is_same_nocv = t_is_same<t_remove_cv<T>, t_remove_cv<U>...>;

    /// 检查所有给定类型在移除 cv修饰和引用 后是否相同。
    template <typename T, typename... U>
    constexpr bool t_is_same_nocvref = t_is_same<t_remove_cvref<T>, t_remove_cvref<U>...>;
}

#pragma clang diagnostic pop