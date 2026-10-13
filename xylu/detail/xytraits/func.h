#pragma once

/**
 * @file    xytraits/func.h
 * @brief   可调用实体内省：通用返回类型推导与调用可行性分析
 * @date    2025-12-20
 */

#include "./class.h"

namespace xylu::xytraits
{
    // 内部实现细节：
    // 通过 SFINAE 和重载决议，为不同类型的可调用对象（成员函数、成员变量、其他）提供不同的测试函数
    // 成功的匹配会返回一个包含 `ret` 类型和 `nothrow` 值的结构体，失败的匹配则会回退到返回 `xyu::fail_t`
    // 主分派器 `call_ret_of_fun_test` 根据 Func 的类型（是否为成员函数/变量指针）转发到正确的测试实现
    namespace __
    {
        // 成员函数指针 (提供成员实例)
        template <typename Func, typename Class, typename... Args>
        decltype((t_val<Class>().*t_val<Func>())(t_val<Args>()...)) test_member_fun_ref(int) noexcept(noexcept((t_val<Class>().*t_val<Func>())(t_val<Args>()...)));
        template <typename...>
        xyu::special_t test_member_fun_ref(void*);

        template <typename Func, typename Class, typename... Args>
        struct member_fun_ref
        {
            using ret = decltype(test_member_fun_ref<Func, Class, Args...>(0));
            constexpr static bool nothrow = noexcept(test_member_fun_ref<Func, Class, Args...>(0));
        };

        // 成员函数指针 (提供成员指针)
        template <typename Func, typename ClassPtr, typename... Args>
        decltype((t_val<ClassPtr>()->*t_val<Func>())(t_val<Args>()...)) test_member_fun_ptr(int) noexcept(noexcept((t_val<ClassPtr>()->*t_val<Func>())(t_val<Args>()...)));
        template <typename...>
        xyu::special_t test_member_fun_ptr(void*);

        template <typename Func, typename ClassPtr, typename... Args>
        struct member_fun_ptr
        {
            using ret = decltype(test_member_fun_ptr<Func, ClassPtr, Args...>(0));
            constexpr static bool nothrow = noexcept(test_member_fun_ptr<Func, ClassPtr, Args...>(0));
        };

        // 成员变量指针 (提供成员实例)
        template <typename Val, typename Class>
        decltype(t_val<Class>().*t_val<Val>()) test_member_val_ref(int) noexcept(noexcept(t_val<Class>().*t_val<Val>()));
        template <typename...>
        xyu::special_t test_member_val_ref(void*);

        template <typename Val, typename Class>
        struct member_val_ref
        {
            using ret = decltype(test_member_val_ref<Val, Class>(0));
            constexpr static bool nothrow = noexcept(test_member_val_ref<Val, Class>(0));
        };

        // 成员变量指针 (提供成员指针)
        template <typename Val, typename ClassPtr>
        decltype(t_val<ClassPtr>()->*t_val<Val>()) test_member_val_ptr(int) noexcept(noexcept(t_val<ClassPtr>()->*t_val<Val>()));
        template <typename...>
        xyu::special_t test_member_val_ptr(void*);

        template <typename Val, typename ClassPtr>
        struct member_val_ptr
        {
            using ret = decltype(test_member_val_ptr<Val, ClassPtr>(0));
            constexpr static bool nothrow = noexcept(test_member_val_ptr<Val, ClassPtr>(0));
        };

        // 其他函数
        template <typename Func, typename... Args>
        decltype(t_val<Func>()(t_val<Args>()...)) test_other_fun(int) noexcept(noexcept(t_val<Func>()(t_val<Args>()...)));
        template <typename...>
        xyu::special_t test_other_fun(void*);

        template <typename Func, typename... Args>
        struct other_fun
        {
            using ret = decltype(test_other_fun<Func, Args...>(0));
            constexpr static bool nothrow = noexcept(test_other_fun<Func, Args...>(0));
        };

        // 非法
        struct invalid_fun
        {
            using ret = xyu::special_t;
            constexpr static bool nothrow = false;
        };

        // 综合
        template <typename Func, typename = void, typename... Args>
        struct call_ret_of_fun_impl { using base = invalid_fun; };
        template <typename Func, typename... Args>
        struct call_ret_of_fun_impl<Func, t_enable<!t_is_mfptr<Func> && !t_is_mvptr<Func>>, Args...>
        { using base = other_fun<Func, Args...>; };
        template <typename Ord, typename Class, typename CP, typename... Args>
        struct call_ret_of_fun_impl<Ord Class::*, t_enable<t_is_mfptr<Ord Class::*> && t_is_base<Class, CP>>, CP, Args...>
        { using base = member_fun_ref<Ord Class::*, CP, Args...>; };
        template <typename Ord, typename Class, typename CP, typename... Args>
        struct call_ret_of_fun_impl<Ord Class::*, t_enable<t_is_mfptr<Ord Class::*> && !t_is_base<Class, CP>>, CP, Args...>
        { using base = member_fun_ptr<Ord Class::*, CP, Args...>; };
        template <typename Ord, typename Class, typename CP>
        struct call_ret_of_fun_impl<Ord Class::*, t_enable<t_is_mvptr<Ord Class::*> && t_is_base<Class, CP>>, CP>
        { using base = member_val_ref<Ord Class::*, CP>; };
        template <typename Ord, typename Class, typename CP>
        struct call_ret_of_fun_impl<Ord Class::*, t_enable<t_is_mvptr<Ord Class::*> && !t_is_base<Class, CP>>, CP>
        { using base = member_val_ptr<Ord Class::*, CP>; };

        template <typename Func, typename... Args>
        struct call_ret_of_fun
        {
            using base = typename call_ret_of_fun_impl<Func, void, Args...>::base;
            using ret = typename base::ret;
            constexpr static bool nothrow = base::nothrow;
        };
    }

    /**
     * @brief 获取可调用表达式的返回类型
     * @tparam Func 可调用实体类型，如函数类型、成员指针类型等
     * @tparam Args 调用参数包。对于成员调用，第一个参数是类实例或指针
     * @return 若调用合法，则返回其返回类型；否则返回 special_t
     * @bar 支持多种调用形式：
     * @code
     * - 普通调用: `t_call˂Func, Args...˃` (Func 是函数、函数指针、lambda 或函数对象)
     * - 成员函数调用: `t_call˂MemberFuncPtr, ClassObj, Args...˃` (ClassObj 是对象实例、引用或指针)
     * - 成员变量访问: `t_call˂MemberVarPtr, ClassObj˃` (ClassObj 是对象实例、引用或指针)
     * @endcode
     */
    template <typename Func, typename... Args>
    using t_call = typename __::call_ret_of_fun<Func, Args...>::ret;

    /// 检查 Func 是否能通过 Args 成功调用
    template <typename Func, typename... Args>
    constexpr bool t_can_call = !t_is_same<t_call<Func, Args...>, xyu::special_t>;

    /// 检查 Func 是否能通过 Args 无异常成功调用
    template <typename Func, typename... Args>
    constexpr bool t_can_nothrow_call = __::call_ret_of_fun<Func, Args...>::nothrow;

    /**
     * @brief 检查 Func 是否能通过 Args 成功调用，并将返回值隐式转换为 To
     * @details 若 To 为 void，则仅检查可调用性
     */
    template <typename To, typename Func, typename... Args>
    constexpr bool t_can_call_to = t_can_call<Func, Args...> &&
                                   (t_is_void<To> || t_can_icast<t_call<Func, Args...>, To>);

    /**
     * @brief 检查 Func 是否能通过 Args 无异常成功调用，并将返回值无异常隐式转换为 To
     * @details 若 To 为 void，则仅检查能否无异常调用
     */
    template <typename To, typename Func, typename... Args>
    constexpr bool t_can_nothrow_call_to = t_can_nothrow_call<Func, Args...> &&
                                           (t_is_void<To> || t_can_nothrow_icast<t_call<Func, Args...>, To>);
    
}
