#pragma once

/**
 * @file    xytraits/kind.h
 * @brief   核心类型分类系统：基于编译器内置指令的原始属性判定
 * @date    2025-12-19
 *
 * @attention
 *   此处的分类不包含修饰(const或引用)，默认情况下忽略，除非明确说明。
 *   仅将 char 视为 字符类型，其余视为整型 (如 signed char, unsigned char)。
 */

#include "./crts.h"
#include "../xycore/macro.h"

/** 基础类型 */
namespace xylu::xytraits
{
    namespace __
    {
        template <typename T>
        struct is_integer : false_c {};

    #define XY_tmpl_instance_gen_part( tmpl, param ) \
        template <> struct tmpl<param> : true_c {};

    #ifdef __SIZEOF_INT128__
        XY_PP_FORWARD( XY_tmpl_instance_gen_part, 2, XY_PP_PARAM_LINS( is_integer, 1,
                unsigned char, signed char, short, unsigned short, int, unsigned int,
                long, unsigned long, long long, unsigned long long, xyu::int128, xyu::uint128))

    #else
        XY_PP_FORWARD( XY_tmpl_instance_gen_part, 2, XY_PP_PARAM_LINS( is_integer, 1,
                unsigned char, signed char, short, unsigned short, int, unsigned int,
                long, unsigned long, long long, unsigned long long))
    #endif
    }
    /// 是否为纯整数 (short,int等、[u/s]char)
    template <typename... T>
    constexpr bool t_is_integer = (... && __::is_integer<t_rem_cr<T>>::value);

    /// 是否为 bool 类型
    template <typename... T>
    constexpr bool t_is_bool = (... && t_is_same_nocr<T, bool>);

    /// 是否为 char 类型
    template <typename... T>
    constexpr bool t_is_char = (... && t_is_same_nocr<T, char>);

    /// 是否为整型 (整数类型、bool类型、char类型)
    template <typename... T>
    constexpr bool t_is_integral = (... && (t_is_integer<T> || t_is_bool<T> || t_is_char<T>));

    namespace __
    {
        template <typename T>
        struct is_floating : false_c {};

    XY_PP_FORWARD( XY_tmpl_instance_gen_part, 2, XY_PP_PARAM_LINS( is_floating, 1,
                   float, double, long double))
    #undef XY_tmpl_instance_gen_part
    }
    /// 是否为浮点型
    template <typename... T>
    constexpr bool t_is_floating = (... && __::is_floating<t_rem_cr<T>>::value);

    /// 是否为算术类型 (纯整数、浮点数)
    template <typename... T>
    constexpr bool t_is_arithmetic = (... && (t_is_integer<T> || t_is_floating<T>));

    /// 是否为数值类型 (整型、浮点型)
    template <typename... T>
    constexpr bool t_is_number = (... && (t_is_integral<T> || t_is_floating<T>));

    /// 是否为 void
    template <typename... T>
    constexpr bool t_is_void = t_is_same_noconst<void, t_rem_const<T>...>;
}

/** 复合类型 */
namespace xylu::xytraits
{
    /// 是否为枚举类型
    template <typename... T>
    constexpr bool t_is_enum = (... && __is_enum(t_rem_refer<T>));

    /// 是否为强枚举类型 (不可隐式转换为整数)
    template <typename... T>
    constexpr bool t_is_senum = (... && (t_is_enum<T> && !t_can_icast<T, int>));

    /// 是否为弱枚举类型 (可隐式转换为整数)
    template <typename... T>
    constexpr bool t_is_uenum = (... && (t_is_enum<T> && t_can_icast<T, int>));

    /**
     * @brief 是否为函数类型
     * @details 仅 引用类型 与 函数类型 添加 浅const 后仍为 非const类型(即原类型)
     * @note 不包含函数引用、函数指针、成员函数指针
     */
    namespace __
    {
        template <typename T>
        struct is_const_top : false_c {};
        template <typename T>
        struct is_const_top<T const> : true_c {};
    }
    template <typename... T>
    constexpr bool t_is_function = (... && (!__::is_const_top<T const>::value && !t_is_refer<T>));

    /// 是否为联合体
    template <typename... T>
    constexpr bool t_is_union = (... && __is_union(t_rem_refer<T>));

    /// 是否为类或结构体
    template <typename... T>
    constexpr bool t_is_class = (... && __is_class(t_rem_refer<T>));
    /// 是否为类或结构体
    template <typename... T>
    constexpr bool t_is_struct = (... && __is_class(t_rem_refer<T>));
}

/** 数组类型 */
namespace xylu::xytraits
{
    namespace __
    {
        template <typename T>
        struct is_bounded_array : false_c {};
        template <typename T, xyu::size_t size>
        struct is_bounded_array<T[size]> : true_c {};
    }
    /// 是否为有界数组 (T[N])
    template <typename... T>
    constexpr bool t_is_barray = (... && __::is_bounded_array<t_rem_refer<T>>::value);

    namespace __
    {
        template <typename T>
        struct is_unbounded_array : false_c {};
        template <typename T>
        struct is_unbounded_array<T[]> : true_c {};
    }
    /// 是否为无界数组 (T[])
    template <typename... T>
    constexpr bool t_is_uarray = (... && __::is_unbounded_array<t_rem_refer<T>>::value);

    /// 是否为数组类型 (有界、无界)
    template <typename... T>
    constexpr bool t_is_array = (... && (t_is_barray<T> || t_is_uarray<T>));
}

/** 指针类型 */
namespace xylu::xytraits
{
    /// 是否为 nullptr
    template <typename... T>
    constexpr bool t_is_nullptr = t_is_same<xyu::nullptr_t, T...>;

    namespace __
    {
        template <typename T>
        struct is_normal_pointer : false_c {};
        template <typename T>
        struct is_normal_pointer<T*> : true_c {};
    }
    /**
     * @brief 是否为普通指针 (指向变量或函数)
     * @note 不包含指向类成员的指针
     */
    template <typename... T>
    constexpr bool t_is_vfptr = (... && __::is_normal_pointer<t_rem_cr<T>>::value);

    namespace __
    {
        template <typename T>
        struct is_function_pointer_base : false_c {};
        template <typename T>
        struct is_function_pointer_base<T*> : bool_c<t_is_function<T>> {};
    }
    /**
     * @brief 是否为函数指针
     * @note 不包含成员函数指针
     */
    template <typename... T>
    constexpr bool t_is_fptr = (... && __::is_function_pointer_base<t_rem_cr<T>>::value);

    /**
     * @brief 是否为变量指针
     * @note 不包含成员变量指针
     */
    template <typename... T>
    constexpr bool t_is_vptr = (... && (t_is_vfptr<T> && !t_is_fptr<T>));

    namespace __
    {
        template <typename T>
        struct is_member_pointer_base : false_c {};
        template <typename S, typename T>
        struct is_member_pointer_base<T S::*> : true_c {};
    }
    /// 是否为指向类成员的指针 (成员变量或成员函数)
    template <typename... T>
    constexpr bool t_is_mptr = (... && __::is_member_pointer_base<t_rem_cr<T>>::value);

    namespace __
    {
        template <typename T>
        struct is_member_function_pointer_base : false_c {};
        template <typename S, typename T>
        struct is_member_function_pointer_base<T S::*> : bool_c<t_is_function<T>> {};
    }
    /// 是否为成员函数指针
    template <typename... T>
    constexpr bool t_is_mfptr = (... && __::is_member_function_pointer_base<t_rem_cr<T>>::value);

    /// 是否为成员变量指针
    template <typename... T>
    constexpr bool t_is_mvptr = (... && (t_is_mptr<T> && !t_is_mfptr<T>));

    /// 是否为任意类型的指针 (普通指针、成员指针、nullptr)
    template <typename... T>
    constexpr bool t_is_pointer = (... && (t_is_nullptr<T> || t_is_vfptr<T> || t_is_mptr<T>));
}
