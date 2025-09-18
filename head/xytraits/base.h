#pragma clang diagnostic push
#pragma ide diagnostic ignored "OCUnusedGlobalDeclarationInspection"
#pragma once

/* 类型属性基础 */

#include "../../link/type"

namespace xylu::xytraits
{
    /**
     * @brief 将一个编译期常量值封装成一个类型。
     * @details 这是模板元编程的基础组件，允许将常量值作为类型进行传递和操作。
     * @tparam T 常量的类型。
     * @tparam v 常量的值。
     */
    template <typename T, T v>
    struct constant_c
    {
        using type = T;                     // 常量本身的类型。
        constexpr static T value = v;       // 封装的常量值。
    };

    /**
     * @brief bool 类型编译期常量的别名。
     * @see constant_c
     */
    template <bool v>
    using bool_c = constant_c<bool, v>;

    using true_c = bool_c<true>;            // 代表编译期常量 true 的类型。
    using false_c = bool_c<false>;          // 代表编译期常量 false 的类型。

    /**
     * @brief xyu::size_t 类型编译期常量的别名。
     * @see constant_c
     */
    template <xyu::size_t v>
    using size_c = constant_c<xyu::size_t, v>;

    /**
     * @brief int 类型编译期常量的别名。
     * @see constant_c
     */
    template <int v>
    using int_c = constant_c<int, v>;

    namespace __
    {
        //[默认的右值引用符合很多转发场景的需求]
        template <typename T, typename U = T&&>
        U declval(int);
        //[当T不适合被转发为右值引用时匹配]
        template <typename T>
        T declval(void*);
    }
    /**
     * @brief 在不求值语境中（如 decltype）创建一个类型 T 的 “虚假” 实例。
     * @details 它允许你在不实际构造对象的情况下，获取一个假设存在的对象，并对其进行类型运算。
     * @attention 仅可用于编译期类型推导，绝不能在运行时调用或求值。
     * @attention 仅在编译期执行的函数不应使用任何非 t_val 构造的变量。
     * @tparam T 要“实例化”的类型。
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
     * @brief 根据编译期布尔条件，从两个类型中选择一个。
     * @details 可以看作是作用于类型的三元运算符 (condition ? if_true_t : if_false_t)。
     * @tparam condition 编译期布尔条件。
     * @tparam if_true_t 如果条件为 true，则选择此类型。
     * @tparam if_false_t 如果条件为 false，则选择此类型。
     */
    template <bool condition, typename if_true_t, typename if_false_t>
    using t_cond = typename __::condition<condition, if_true_t, if_false_t>::type;

    namespace __
    {
        template <bool condition, typename if_true_t>
        struct enable_if {};
        template <typename if_true_t>
        struct enable_if<true, if_true_t> { using type = if_true_t; };
    }
    /**
     * @brief 用于条件编译的 SFINAE 核心工具。
     * @details 当条件为 true 时，它解析为指定的类型， 从而使模板重载有效。
     *          当条件为 false 时，类型推导失败，该模板重载会被编译器安静地忽略，而不是报错。
     * @tparam condition 编译期布尔条件。
     * @tparam if_true_t 当条件为 true 时得到的类型，默认为 void。
     */
    template <bool condition, typename if_true_t = void>
    using t_enable = typename __::enable_if<condition, if_true_t>::type;

    /**
     * @brief 一个将任意模板参数列表映射到 void 类型的工具。
     * @details 主要用于 SFINAE 检测，以判断一个或多个类型表达式是否有效。
     * @example
     * // 检测 T 是否有成员函数 .begin()
     * template <typename T, typename = void>
     * struct test_begin : false_c {};
     * template <typename T>
     * struct test_begin<T, t_void<decltype(t_val<T>().begin())>> : true_c {};
     */
    template <typename...>
    using t_void = void;

    /**
     * @brief 在编译期判断当前代码是否在常量表达式求值上下文中执行。
     * @details 它允许一个 constexpr 函数在编译期和运行时有不同的行为。
     * @return 如果在编译期求值，返回 true；否则返回 false。
     */
    constexpr bool t_constant() noexcept { return __builtin_is_constant_evaluated(); }
}

#pragma clang diagnostic pop