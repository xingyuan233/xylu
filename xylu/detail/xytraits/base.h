#pragma once

/**
 * @file    xytraits/base.h
 * @brief   元编程基础设施：编译期常量封装与 SFINAE 核心工具
 * @date    2025-12-19
 */

#include "./_.h"
#include "../xycore/type.h"

/// 编译期常量
namespace xylu::xytraits
{
    /**
     * @brief 编译期常量类型
     * @tparam T 常量类型
     * @tparam v 常量值
     */
    template <typename T, T v>
    struct constant_c
    {
        using type = T;
        constexpr static T value = v;
    };

    // 编译期 bool 常量
    template <bool v>
    using bool_c = constant_c<bool, v>;

    using true_c = bool_c<true>;
    using false_c = bool_c<false>;

    // 编译期 size_t 常量
    template <xyu::size_t v>
    using size_c = constant_c<xyu::size_t, v>;
}

/// 编译期基础工具
namespace xylu::xytraits
{
    namespace __
    {
        // 默认的右值引用适合很多转发场景的需求
        template <typename T, typename U = T&&>
        U declval(int);
        // 当T不适合被转发为右值引用时匹配
        template <typename T>
        T declval(void*);
    }
    /**
     * @brief 在不求值语境中创建一个类型 T 的 “虚假” 实例
     * @attention 仅用于编译期类型推导，绝不能在运行时调用或求值
     * @tparam T “虚假”实例化的类型
     */
    template <typename T>
    decltype(__::declval<T>(0)) t_val() noexcept { return __::declval<T>(0); }

    namespace __
    {
        template <bool cond, typename if_true_t, typename if_false_t>
        struct condition { using type = if_false_t; };
        template <typename if_true_t, typename if_false_t>
        struct condition<true, if_true_t, if_false_t> { using type = if_true_t; };
    }
    /**
     * @brief 根据编译期布尔条件返回类型 (condition ? if_true_t : if_false_t)
     * @tparam condition 编译期条件
     * @tparam if_true_t 条件为 true 时选择的类型
     * @tparam if_false_t 条件为 false 时选择的类型
     */
    template <auto condition, typename if_true_t, typename if_false_t>
    using t_cond = typename __::condition<static_cast<bool>(condition), if_true_t, if_false_t>::type;

    namespace __
    {
        template <bool condition, typename if_true_t>
        struct enable_if {};
        template <typename if_true_t>
        struct enable_if<true, if_true_t> { using type = if_true_t; };
    }
    /**
     * @brief 用于 SFINAE 条件编译，仅条件为 true 时推导成功
     * @tparam condition 编译期条件
     * @tparam if_true_t 当条件为 true 时得到的类型，默认为 void
     */
    template <auto condition, typename if_true_t = void>
    using t_enable = typename __::enable_if<static_cast<bool>(condition), if_true_t>::type;

    namespace __
    {
        template <typename... T>
        struct make_void { using type = void; };
    }
    /**
     * @brief 用于 SFINAE 条件编译，仅所有的模板参数有效时推导成功
     * @bar 示例
     * @code
     * // 检测 T 是否有成员函数 .begin()
     * template ˂typename T, typename = void˃
     * struct test_begin : false_c {};
     * template ˂typename T˃
     * struct test_begin˂T, t_void˂decltype(t_val˂T˃().begin())˃˃ : true_c {};
     * @endcode
     */
    template <typename... T>
    using t_void = typename __::make_void<T...>::type;

    /**
     * @brief 编译期判断当前代码是否在常量表达式求值上下文中执行
     * @bar 示例
     * @code
     * void fun()
     * {
     *     // 编译期判断，不会因为 else 需要运行时执行而编译失败
     *     if (t_constant()) { ... }
     *     else { ... }
     * }
     * @endcode
     */
    constexpr bool t_constant() noexcept { return __builtin_is_constant_evaluated(); }
}
