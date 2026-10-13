#pragma once

/**
 * @file    xytraits/crts.h
 * @brief   类型修饰符管理：const 限定符、引用属性的探测、剥离与同步
 * @date    2025-12-19
 *
 * @details
 *   所有泛型中忽略 volatile，它本就不应该出现在泛型中。
 *   指针与引用是访问对象的工具，它们不应该阻止底层 const 的传播，就像数组一样。
 */

#include "./base.h"

/* const 修饰符 */
namespace xylu::xytraits
{
    namespace __
    {
        template <typename T>
        struct is_const : false_c {};
        template <typename T>
        struct is_const<T const> : true_c {};
        template <typename T>
        struct is_const<T&> : bool_c<is_const<T>::value> {};
        template <typename T>
        struct is_const<T&&> : bool_c<is_const<T>::value> {};
        template <typename T>
        struct is_const<T*> : bool_c<is_const<T>::value> {};
        template <typename T>
        struct is_const<T* const> : bool_c<is_const<T>::value> {};
    }
    /// 是否有 深const 修饰
    template <typename... T>
    constexpr bool t_is_const = (... && __::is_const<T>::value);

    namespace __
    {
        template <typename T>
        struct remove_const { using type = T; };
        template <typename T>
        struct remove_const<T const> { using type = T; };
        template <typename T>
        struct remove_const<T&> { using type = typename remove_const<T>::type&; };
        template <typename T>
        struct remove_const<T&&> { using type = typename remove_const<T>::type&&; };
        template <typename T>
        struct remove_const<T*> { using type = typename remove_const<T>::type*; };
        template <typename T>
        struct remove_const<T* const> { using type = typename remove_const<T>::type*; };
    }
    /**
     * @brief 移除 深const 修饰
     * @bar 示例
     * @code
     * t_rem_const˂const T*˃ -˃ T*
     * t_rem_const˂const T&˃ -˃ T&
     * @endcode
     */
    template <typename T>
    using t_rem_const = typename __::remove_const<T>::type;

    namespace __
    {
        template <typename T>
        struct add_const { using type = T const; };
        template <>
        struct add_const<void> { using type = void; };
        template <typename T>
        struct add_const<T&> { using type = typename add_const<T>::type&; };
        template <typename T>
        struct add_const<T&&> { using type = typename add_const<T>::type&&; };
        template <typename T>
        struct add_const<T*> { using type = typename add_const<T>::type* const; };
        template <typename T>
        struct add_const<T* const> { using type = typename add_const<T>::type* const; };
    }
    /**
     * @brief 添加 深const 修饰
     * @bar 示例
     * @code
     * t_add_const˂T*˃ -˃ const T*
     * t_add_const˂T&˃ -˃ const T&
     * @endcode
     */
    template <typename T>
    using t_add_const = typename __::add_const<T>::type;

    /**
     * @brief 为参数添加 深const 限定符
     * @example int& -˃ const int&
     */
    template <typename T>
    constexpr t_add_const<T> make_const(T&& v) noexcept { return static_cast<t_add_const<T>>(v); }
}

/* 引用 */
namespace xylu::xytraits
{
    namespace __
    {
        template <typename T>
        struct is_lvalue_refer : false_c {};
        template <typename T>
        struct is_lvalue_refer<T&> : true_c {};
    }
    /// 是否为 左值引用
    template <typename... T>
    constexpr bool t_is_lrefer = (... && __::is_lvalue_refer<T>::value);

    namespace __
    {
        template <typename T>
        struct is_rvalue_refer : false_c {};
        template <typename T>
        struct is_rvalue_refer<T&&> : true_c {};
    }
    /// 是否为 右值引用
    template <typename... T>
    constexpr bool t_is_rrefer = (... && __::is_rvalue_refer<T>::value);

    /// 是否为 左值或右值引用
    template <typename... T>
    constexpr bool t_is_refer = (... && (t_is_lrefer<T> || t_is_rrefer<T>));

    namespace __
    {
        template <typename T, typename = decltype(t_val<T&>())>
        true_c can_refer_test(int);
        template <typename T>
        false_c can_refer_test(...);
    }
    /**
     * @brief 能否被引用
     * @details 主要用于排除 (const) void
     */
    template <typename... T>
    constexpr bool t_can_refer = (... && decltype(__::can_refer_test<T>(0))::value);

    namespace __
    {
        template <typename T>
        struct remove_lvalue_refer { using type = T; };
        template <typename T>
        struct remove_lvalue_refer<T&> { using type = T; };
    }
    /// 移除 左值引用 修饰
    template <typename T>
    using t_rem_lrefer = typename __::remove_lvalue_refer<T>::type;

    namespace __
    {
        template <typename T>
        struct remove_rvalue_refer { using type = T; };
        template <typename T>
        struct remove_rvalue_refer<T&&> { using type = T; };
    }
    /// 移除 右值引用 修饰
    template <typename T>
    using t_rem_rrefer = typename __::remove_rvalue_refer<T>::type;

    /// 移除 左值或右值引用 修饰
    template <typename T>
    using t_rem_refer = t_rem_lrefer<t_rem_rrefer<T>>;

    namespace __
    {
        template <typename T, bool = t_can_refer<T>>
        struct add_lvalue_refer { using type = T&; };
        template <typename T>
        struct add_lvalue_refer<T, false> { using type = T; };
    }
    /**
     * @brief 添加 左值引用 修饰
     * @note 对于 void 类型，返回 void
     */
    template <typename T>
    using t_add_lrefer = typename __::add_lvalue_refer<T>::type;

    namespace __
    {
        template <typename T, bool = t_can_refer<T>>
        struct add_rvalue_refer { using type = T&&; };
        template <typename T>
        struct add_rvalue_refer<T, false> { using type = T; };
    }
    /**
     * @brief 添加 右值引用 修饰
     * @note 对于 void 类型，返回 void
     */
    template <typename T>
    using t_add_rrefer = typename __::add_rvalue_refer<T>::type;

    /// 移除类型的 深const和引用修饰
    template <typename T>
    using t_rem_cr = t_rem_const<t_rem_refer<T>>;

    /**
     * @brief 将参数转发为右值引用
     * @tparam move_allowed 为 false 时对左值依旧转发为左值
     */
    template <bool move_allowed = true, typename T, typename Ret = t_cond<move_allowed || !t_is_lrefer<T>, t_rem_refer<T>&&, t_rem_refer<T>&>>
    constexpr Ret move(T&& v) noexcept { return static_cast<Ret>(v); }

    /**
     * @brief 完美转发的左值处理部分，将左值引用转发为左值或右值引用
     * @tparam move_allowed 为 false 时对于左值依旧转发为左值
     */
    template <typename T, bool move_allowed = true, typename Ret = t_cond<move_allowed || t_is_lrefer<T>, T&&, T&>>
    constexpr Ret forward(t_rem_refer<T>& v) noexcept { return static_cast<Ret>(v); }
    /**
     * @brief 完美转发的右值处理部分，将右值引用 v 转发为右值引用
     * @note 禁止将右值转发为左值引用，可能会危险地延长临时对象的生命周期
     */
    template<typename T, bool = true>
    constexpr T&& forward(t_rem_refer<T>&& v) noexcept
    {
        static_assert(!t_is_lrefer<T>, "forward must not be used to convert an rvalue to an lvalue");
        return static_cast<T&&>(v);
    }
}

/* 修饰同步 */
namespace xylu::xytraits
{
    namespace __
    {
        template <typename Standard, typename T>
        struct sync_const { using type = t_cond<t_is_const<Standard>, t_add_const<T>, T>; };
    }
    /// 将 Standard 的 深const修饰 同步到 T
    template <typename Standard, typename T>
    using t_sync_const = typename __::sync_const<Standard, T>::type;

    namespace __
    {
        template <typename Standard, typename T>
        struct sync_refer { using type = T; };
        template <typename S, typename T>
        struct sync_refer<S&, T> { using type = T&; };
        template <typename S, typename T>
        struct sync_refer<S&&, T> { using type = T&&; };
    }
    /// 将 Standard 的 引用修饰 同步到 T
    template <typename Standard, typename T>
    using t_sync_refer = typename __::sync_refer<Standard, t_rem_refer<T>>::type;

    /// 将 Standard 的 深const及引用修饰 同步到 T
    template <typename Standard, typename T>
    using t_sync_cr = t_sync_refer<Standard, t_sync_const<t_rem_refer<Standard>, t_rem_refer<T>>>;
}

/* 类型比较 */
namespace xylu::xytraits
{
    /// 检查所有给定类型是否均相同
    template <typename T, typename... U>
    constexpr bool t_is_same = (... && __is_same(T, U));

    /// 检查所有给定类型移除 深const修饰 是否相同
    template <typename T, typename... U>
    constexpr bool t_is_same_noconst = (... && __is_same(t_rem_const<T>, t_rem_const<U>));

    /// 检查所有给定类型移除 引用修饰 是否相同
    template <typename T, typename... U>
    constexpr bool t_is_same_norefer = (... && __is_same(t_rem_refer<T>, t_rem_refer<U>));

    /// 检查所有给定类型移除 深const和引用修饰 是否相同
    template <typename T, typename... U>
    constexpr bool t_is_same_nocr = (... && __is_same(t_rem_cr<T>, t_rem_cr<U>));

    /// 检查给定类型 U 中是否有与 T 相同的类型
    template <typename T, typename... U>
    constexpr bool t_is_same_one = (... || __is_same(T, U));

    /// 检查给定类型 U 移除 深const和引用修饰 是否有与 T 移除同等修饰后相同的类型
    template <typename T, typename... U>
    constexpr bool t_is_same_nocr_one = (... || __is_same(t_rem_cr<T>, t_rem_cr<U>));
}

/* 类型转换 */
namespace xylu::xytraits
{
    namespace __
    {
        template <typename To>
        void can_implicit_cast_test_fun(const To&) noexcept;
        template <typename From, typename To, typename = decltype(can_implicit_cast_test_fun<To>(t_val<From>()))>
        true_c can_implicit_cast_test(int) noexcept(noexcept(can_implicit_cast_test_fun<To>(t_val<From>())));
        template <typename...>
        false_c can_implicit_cast_test(...);
    }
    /// 检查类型 From 能否隐式转换为类型 To
    template <typename From, typename To>
    constexpr bool t_can_icast = decltype(__::can_implicit_cast_test<From, To>(0))::value;

    /// 检查类型 From 能否 无异常地 隐式转换为类型 To
    template <typename From, typename To>
    constexpr bool t_can_nothrow_icast = noexcept(__::can_implicit_cast_test<From, To>(0));

    namespace __
    {
        template <typename From, typename To, typename = decltype(static_cast<To>(xyu::t_val<From>()))>
        true_c can_cast_test(int) noexcept(noexcept(static_cast<To>(xyu::t_val<From>())));
        template <typename...>
        false_c can_cast_test(...);
    }
    /// 检查类型 From 能否转换为类型 To
    template <typename From, typename To>
    constexpr bool t_can_cast = decltype(__::can_cast_test<From, To>(0))::value;

    /// 检查类型 From 能否 无异常地 转换为类型 To
    template <typename From, typename To>
    constexpr bool t_can_nothrow_cast = noexcept(__::can_cast_test<From, To>(0));
}
