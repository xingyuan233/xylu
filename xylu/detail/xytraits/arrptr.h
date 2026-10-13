#pragma once

/**
 * @file    xytraits/arrptr.h
 * @brief   高级指针与数组变换：Decay 规则实现与成员指针拆解
 * @date    2025-12-19
 */

#include "./kind.h"

/* 指针|数组增删 */
namespace xylu::xytraits
{
    namespace __
    {
        template <typename T, xyu::size_t deep>
        struct remove_pointer { using type = T; };
        template <typename T>
        struct remove_pointer<T, 0> { using type = T; };
        template <typename T, xyu::size_t deep>
        struct remove_pointer<T*, deep> { using type = typename remove_pointer<T, deep - 1>::type; };
        template <typename T, xyu::size_t deep>
        struct remove_pointer<T* const, deep> { using type = typename remove_pointer<T, deep - 1>::type; };
    }
    /// 移除指针，最多移除 deep (默认为1) 层
    template <typename T, xyu::size_t deep = 1>
    using t_rem_pointer = typename __::remove_pointer<T, deep>::type;

    namespace __
    {
        template <typename T, bool = t_can_refer<T> || t_is_void<T>>
        struct add_pointer { using type = T; };
        template <typename T>
        struct add_pointer<T, true> { using type = t_rem_refer<T>*; };
    }
    /**
     * @brief 添加一层指针
     * @details 对于引用类型，去除引用后添加指针 (T& -˃ T*)
     */
    template <typename T>
    using t_add_pointer = typename __::add_pointer<T>::type;

    namespace __
    {
        template <typename T, xyu::size_t deep>
        struct remove_array { using type = T; };
        template <typename T>
        struct remove_array<T, 0> { using type = T; };
        template <typename T, xyu::size_t deep>
        struct remove_array<T[], deep> { using type = typename remove_array<T, deep - 1>::type; };
        template <typename T, xyu::size_t N, xyu::size_t deep>
        struct remove_array<T[N], deep> { using type = typename remove_array<T, deep - 1>::type; };
    }
    /// 删除数组，最多移除 deep (默认为1) 层
    template <typename T, xyu::size_t deep = 1>
    using t_rem_array = typename __::remove_array<T, deep>::type;
}

/* 指针转换 */
namespace xylu::xytraits
{
    namespace __
    {
        // 对成员函数指针 T S::* 获取 T 会保留 const & && 的修饰
        template <typename T>
        struct change_memptr_to_vfptr_native { using type = T; };
        template <typename S, typename T>
        struct change_memptr_to_vfptr_native<T S::*> { using type = T; };
        template <typename S, typename T>
        struct change_memptr_to_vfptr_native<T S::* const> { using type = T; };

        template <typename T>
        struct change_memptr_to_vfptr
        { using type = typename change_memptr_to_vfptr_native<T>::type; };

    #define XY_REMOVE_MEMPTR( MODIFIER ) \
        template <typename S, typename T, typename... Args, bool b> \
        struct change_memptr_to_vfptr< T(S::*)(Args...) MODIFIER noexcept(b) > \
        { using type = T(Args...) noexcept(b); }; \
        template <typename S, typename T, typename... Args, bool b> \
        struct change_memptr_to_vfptr< T(S::*)(Args..., ...) MODIFIER noexcept(b) > \
        { using type = T(Args...) noexcept(b); }; \
        template <typename S, typename T, typename... Args, bool b> \
        struct change_memptr_to_vfptr< T(S::* const)(Args...) MODIFIER noexcept(b) > \
        { using type = T(Args...) noexcept(b); }; \
        template <typename S, typename T, typename... Args, bool b> \
        struct change_memptr_to_vfptr< T(S::* const)(Args..., ...) MODIFIER noexcept(b) > \
        { using type = T(Args...) noexcept(b); };
        XY_PP_FORWARD( XY_REMOVE_MEMPTR, 1, , &, &&, const, const&, const&&)
    #undef XY_REMOVE_MEMPTR
    }
    /**
     * @brief 移除成员指针，获取其指向的成员类型
     * @details 对成员变量指针，返回变量的类型；对成员函数指针，返回函数类型；对于普通指针或其他类型返回本身
     * @bar 示例
     * @code
     * t_chg_mp2vf˂int SomeClass::*˃ -˃ int
     * t_chg_mp2vf˂int(SomeClass::*)(bool) const˃ -˃ int(bool)
     * @endcode
     */
    template <typename T>
    using t_chg_mp2vf = typename __::change_memptr_to_vfptr<T>::type;

    namespace __
    {
        template <typename T>
        struct change_array_to_pointer { using type = T; };
        template <typename T, xyu::size_t size>
        struct change_array_to_pointer<T[size]> { using type = T*; };
        template <typename T>
        struct change_array_to_pointer<T[]> { using type = T*; };
    }
    /// 将数组转换为指针 (T[N] -˃ T*)
    template <typename T>
    using t_chg_arr2ptr = typename __::change_array_to_pointer<T>::type;

    namespace __
    {
        template <typename T>
        struct change_refer_to_pointer { using type = T; };
        template <typename T>
        struct change_refer_to_pointer<T&> { using type = T*; };
    }
    /// 将引用转换为指针 (T& -˃ T*)
    template <typename T>
    using t_chg_ref2ptr = typename __::change_refer_to_pointer<T>::type;
}

/* 综合功能 */
namespace xylu::xytraits
{
    namespace __
    {
        template <typename T, bool = t_is_array<T>, bool = t_is_function<T>>
        struct get_decay { using type = T; };
        template <typename Arr>
        struct get_decay<Arr, true, false> { using type = t_chg_arr2ptr<Arr>; };
        template <typename Fun>
        struct get_decay<Fun, false, true> { using type = Fun*; };
    }
    /**
     * @brief 对类型进行 退化，模拟按值传参时的类型转换
     * @details 此转换规则如下：
     *   1.移除顶层引用；
     *   2.将数组类型转换为指针类型；
     *   3.将函数类型转换为函数指针类型；
     *   4.移除底层 const
     */
    template <typename T>
    using t_decay = t_rem_const<typename __::get_decay<t_rem_refer<T>>::type>;


    /// 获取访问类型 - 基类
    namespace __
    {
        template <typename T, typename = void>
        struct t_access_impl { using type = void; };
        template <typename T>
        struct t_access_impl<T, t_enable<t_is_vptr<T> && !t_is_void<t_rem_pointer<T>>>> { using type = t_rem_pointer<T>&; };
        template <typename T>
        struct t_access_impl<T, t_enable<t_is_mptr<T>>> { using type = t_chg_mp2vf<T>&; };
        template <typename T>
        struct t_access_impl<T, t_enable<t_is_enum<T>>> { using type = t_sync_cr<T, __underlying_type(t_rem_cr<T>)>; };
    }
    template <typename T, typename = void>
    struct t_access_base { using type = typename __::t_access_impl<T>::type; };
    template <typename T>
    struct t_access_base<T, t_enable<t_is_array<T>>> { using type = t_rem_array<t_rem_refer<T>>&; };

    /**
     * @brief 获取访问类型
     * @details 规则如下：
     *   1.对于数组，T[N] 得到 T&，const T[N] 得到 const T&
     *   2.对于指针，T* 得到 T&，const T* 得到 const T&
     *   3.对于枚举返回底层整型，E 得到 I，E& 得到 I&
     *   4.迭代器得到解引用后的类型 (通常为 T&)
     *   5.范围得到其存储迭代器解引用后的类型 (通常为 T&)
     *   6.不可访问返回 void
     *
     * @note 重载自定义时需特化 t_access_base
     * @attention const或引用修饰视为不同类型
     */
    template <typename T>
    using t_access = typename t_access_base<T>::type;

    /// 能否无异常访问 - 基类
    namespace __
    {
        template <typename T, typename = void>
        struct t_noexcept_access_impl { using type = void; };
        template <typename T>
        struct t_noexcept_access_impl<T, t_enable<(t_is_vptr<T> && !t_is_void<t_rem_pointer<T>>) ||
                                                   t_is_mptr<T> || t_is_enum<T>>>
        { using type = t_access<T>; };
    }
    template <typename T, typename = void>
    struct t_noexcept_access_base { using type = typename __::t_noexcept_access_impl<T>::type; };
    template <typename T>
    struct t_noexcept_access_base<T, t_enable<t_is_array<T>>> { using type = t_rem_array<T>&; };

    /**
     * @brief 能否无异常访问
     * @details 对于数组指针枚举直接返回访问类型，其他如果无法无异常访问返回 void
     * @note 重载自定义时需特化 t_noexcept_access
     * @attention const或引用修饰视为不同类型
     */
    template <typename T>
    using t_noexcept_access = typename t_noexcept_access_base<T>::type;
}
