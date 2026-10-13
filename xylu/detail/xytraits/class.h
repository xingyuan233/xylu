#pragma once

/**
 * @file    xytraits/class.h
 * @brief   类属性与布局：继承关系、构造可行性与平凡性检测
 * @date    2025-12-19
 */

#include "./kind.h"
#include "./arrptr.h"

/* 继承、结构、布局 */
namespace xylu::xytraits
{
    /// 检查类型 Base 是否为类型 Derived 的基类(或本身)
    template <typename Base, typename... Derived>
    constexpr bool t_is_base = (... && __is_base_of(t_rem_refer<Base>, t_rem_refer<Derived>));

    namespace __
    {
        template <typename T, typename = decltype(sizeof(T))>
        true_c is_complete_test(int);
        template <typename...>
        false_c is_complete_test(...);
    }
    /**
     * @brief 是否为完整类型
     * @details 可以使用 sizeof(T)
     */
    template <typename... T>
    constexpr bool t_is_complete = (... && decltype(__::is_complete_test<T>(0))::value);

    namespace __
    {
        template <typename T>
        struct check_complete_test
        {
            static_assert(t_is_complete<T> || t_is_void<T>, "type is not complete");
            using type = T;
        };
        template <typename T>
        using check_complete = typename check_complete_test<T>::type;
    }
    /**
     * @brief 是否为 final 类型
     * @details 不可被继承
     */
    template<typename... T>
    constexpr bool t_is_final = (... && __is_final(__::check_complete<T>));

    /**
     * @brief 是否为空类型
     * @details 不包含任何非静态数据成员、虚函数或虚基类
     * @note 理论上不需要存储空间，但编译器为区分实例仍会分配至少1字节
     */
    template<typename... T>
    constexpr bool t_is_empty = (... && __is_empty(__::check_complete<T>));

    /**
     * @brief 是否为聚合类型
     * @details 可以直接通过大括号{}进行初始化
     * 1.无用户提供的构造函数
     * 2.无私有或受保护的非静态数据成员
     * 3.无基类
     * 4.无虚函数
     */
    template<typename... T>
    constexpr bool t_is_aggregate = (... && __is_aggregate(__::check_complete<T>));

    /// 是否含有虚函数
    template <typename... T>
    constexpr bool t_is_polymorphic = (... && __is_polymorphic(__::check_complete<T>));

    /// 是否为抽象类
    template <typename... T>
    constexpr bool t_is_abstract = (... && __is_abstract(__::check_complete<T>));

    /**
     * @brief 是否为平凡类型
     * @details 拥有默认的构造、析构、拷贝、赋值函数
     */
    template <typename... T>
    constexpr bool t_is_trivial = (... && __is_trivial(__::check_complete<T>));

    /**
     * @brief 是否为可平凡复制的类型
     * @details 拥有默认的拷贝、移动函数
     */
    template <typename... T>
    constexpr bool t_is_trivial_copyable = (... && __is_trivially_copyable(__::check_complete<T>));

    /**
     * @brief 是否为标准布局类型
     * @details 所有非静态成员的访问控制权限相同，无虚函数虚基类，所有基类也为标准布局类型
     */
    template <typename... T>
    constexpr bool t_is_std_layout = (... && __is_standard_layout(__::check_complete<T>));
}

/* 构造、赋值、拷贝、移动 */
namespace xylu::xytraits
{
    /// 检查类型能否通过 T v(args...) 进行构造
    template <typename T, typename... Args>
    constexpr bool t_can_construct = __is_constructible(__::check_complete<T>, Args...);

    namespace __
    {
        template <typename T, typename... Args, typename = decltype(T{t_val<Args>()...})>
        true_c can_listinit_test(int);
        template <typename...>
        false_c can_listinit_test(...);
    }
    /// 检查类型能否通过 T v{args...} 进行列表初始化
    template <typename T, typename... Args>
    constexpr bool t_can_listinit = decltype(__::can_listinit_test<__::check_complete<T>, Args...>(0))::value;

    /// 检查类型能否通过 非聚合类型:T(args...) | 聚合类型:T{args...} 进行初始化
    template <typename T, typename... Args>
    constexpr bool t_can_init = t_is_aggregate<T> ? t_can_listinit<T, Args...> : t_can_construct<T, Args...>;

    /// 检查类型的实例能否通过 V... 中的所有类型进行赋值
    template <typename T, typename... V>
    constexpr bool t_can_assign = (... && __is_assignable(t_add_lrefer<__::check_complete<T>>, V));

    /// 能否进行默认构造
    template <typename... T>
    constexpr bool t_can_dfconstr = (... && t_can_construct<T>);
    
    /// 能否进行拷贝构造
    template <typename... T>
    constexpr bool t_can_cpconstr = (... && t_can_construct<T, t_add_lrefer<const T>>);
    
    /// 能否进行移动构造
    template <typename... T>
    constexpr bool t_can_mvconstr = (... && t_can_construct<T, t_add_rrefer<T>>);

    /// 能否进行拷贝赋值
    template <typename... T>
    constexpr bool t_can_cpassign = (... && t_can_assign<T, t_add_lrefer<const T>>);

    /// 能否进行移动赋值
    template <typename... T>
    constexpr bool t_can_mvassign = (... && t_can_assign<T, t_add_rrefer<T>>);
}

/* 异常 */
namespace xylu::xytraits
{
    /// 检查类型能否通过 T v(args...) 进行无异常构造
    template <typename T, typename... Args>
    constexpr bool t_can_nothrow_construct = __is_nothrow_constructible(__::check_complete<T>, Args...);

    namespace __
    {
        template <typename T, typename... Args>
        bool_c<noexcept(T{t_val<Args>()...})> can_nothrow_listinit_test(int);
        template <typename, typename...>
        false_c can_nothrow_listinit_test(...);
    }
    /// 检查类型能否通过 T v{args...} 进行无异常列表初始化
    template <typename T, typename... Args>
    constexpr bool t_can_nothrow_listinit = decltype(__::can_nothrow_listinit_test<__::check_complete<T>, Args...>(0))::value;

    /// 检查类型能否通过 非聚合类型:T(args...) | 聚合类型:T{args...} 进行无异常初始化
    template <typename T, typename... Args>
    constexpr bool t_can_nothrow_init = t_is_aggregate<T> ? t_can_nothrow_listinit<T, Args...> : t_can_nothrow_construct<T, Args...>;

    /// 检查类型的实例能否通过 V... 中的所有类型进行无异常的赋值
    template <typename T, typename... V>
    constexpr bool t_can_nothrow_assign = (... && __is_nothrow_assignable(t_add_lrefer<__::check_complete<T>>, V));

    namespace __
    {
        template <typename T>
        bool_c<noexcept(t_val<T&>().~T())> can_nothrow_destruct_test(int);
        template <typename>
        false_c can_nothrow_destruct_test(...);

        template <typename T>
        constexpr bool can_nothrow_destruct = !(t_is_uarray<T> || t_is_function<T> || t_is_void<T>) &&
                            (t_is_refer<T> || decltype(__::can_nothrow_destruct_test<t_rem_array<T>>(0))::value);
    }
    /**
     * @brief 检查类型能否无异常地析构
     * @details 对于指针、引用、析构函数为noexcept的类均满足；但对未知边界数组、函数(无析构函数)等不满足
     */
    template <typename... T>
    constexpr bool t_can_nothrow_destruct = (... && __::can_nothrow_destruct<T>);

    /// 能否无异常地默认构造
    template <typename... T>
    constexpr bool t_can_nothrow_dfconstr = (... && __is_nothrow_constructible(__::check_complete<T>));

    /// 能否无异常地拷贝构造
    template <typename... T>
    constexpr bool t_can_nothrow_cpconstr = (... && t_can_nothrow_construct<T, t_add_lrefer<const T>>);

    /// 能否无异常地移动构造
    template <typename... T>
    constexpr bool t_can_nothrow_mvconstr = (... && t_can_nothrow_construct<T, t_add_rrefer<T>>);

    /// 能否无异常地拷贝赋值
    template <typename... T>
    constexpr bool t_can_nothrow_cpassign = (... && t_can_nothrow_assign<T, t_add_lrefer<const T>>);

    /// 能否无异常地移动赋值
    template <typename... T>
    constexpr bool t_can_nothrow_mvassign = (... && t_can_nothrow_assign<T, t_add_rrefer<T>>);
}

/* 平凡 */
// (平凡一定是 noexcept)
namespace xylu::xytraits
{
    /// 检查类型能否使用 Args... 进行平凡构造
    template<typename T, typename... Args>
    constexpr bool t_can_trivial_construct = __is_trivially_constructible(__::check_complete<T>, Args...);

    /// 检查类型的实例能否通过 V... 中的所有类型进行平凡赋值
    template<typename T, typename... V>
    constexpr bool t_can_trivial_assign = (... && __is_trivially_assignable(t_add_lrefer<__::check_complete<T>>, V));

    /// 能否平凡地析构
    template <typename... T>
    constexpr bool t_can_trivial_destruct = (... &&
#if __has_builtin(__is_trivially_destructible)
        __is_trivially_destructible(__::check_complete<T>)      // 某些编译器不支持
#else
        __has_trivial_destructor(__::check_complete<T>)
#endif
    );

    /// 能否平凡地默认构造
    template <typename... T>
    constexpr bool t_can_trivial_dfconstr = (... && __is_trivially_constructible(__::check_complete<T>));

    /// 能否平凡地拷贝构造
    template <typename... T>
    constexpr bool t_can_trivial_cpconstr = (... && t_can_trivial_construct<T, t_add_lrefer<const T>>);

    /// 能否平凡地移动构造
    template <typename... T>
    constexpr bool t_can_trivial_mvconstr = (... && t_can_trivial_construct<T, t_add_rrefer<T>>);

    /// 能否平凡地拷贝赋值
    template <typename... T>
    constexpr bool t_can_trivial_cpassign = (... && t_can_trivial_assign<T, t_add_lrefer<const T>>);

    /// 能否平凡地移动赋值
    template <typename... T>
    constexpr bool t_can_trivial_mvassign = (... && t_can_trivial_assign<T, t_add_rrefer<T>>);
}

/// 实例
namespace xylu::xytraits
{
    /**
     * @brief 就地初始化
     * @details 对于非聚类类型使用 `()` 初始化，聚合类型使用 `{}` 初始化
     */
    template <typename T, typename... Args>
    void place_init(T* p, Args&&... args) noexcept(sizeof(T) == 0 || t_can_nothrow_init<T, Args...>)
    {
        if constexpr (sizeof(T) == 0);
        else if constexpr (t_is_aggregate<T>) ::new (p) T{xyu::forward<Args>(args)...};
        else ::new (p) T(xyu::forward<Args>(args)...);
    }

    /// 创建对象
    template <typename T, typename... Args>
    T object_create(Args&&... args) noexcept(t_can_nothrow_init<T, Args...>)
    {
        if constexpr (t_is_aggregate<T>) return T{xyu::forward<Args>(args)...};
        else return T(xyu::forward<Args>(args)...);
    }

    /// 析构对象
    template <typename T>
    void object_destroy(T* p) noexcept(t_can_nothrow_destruct<T>)
    { if constexpr (sizeof(T) && !t_can_trivial_destruct<T>) p->~T(); }
    /// 析构数组
    template <typename T, xyu::size_t N>
    void object_destroy(T(&arr)[N]) noexcept(t_can_nothrow_destruct<T>) { object_destroy(arr, arr + N); }
    /// 析构数组
    template <typename T>
    void object_destroy(T* begin, T* end) noexcept(t_can_nothrow_destruct<T>)
    { while (begin != end) object_destroy(begin++); }
    /// 析构数组
    template <typename T>
    void object_destroy(T* p, xyu::size_t count) noexcept(t_can_nothrow_destruct<T>)
    { if constexpr (sizeof(T)) for (xyu::size_t i = 0; i < count; ++i) p[i].~T(); }

    /**
     * @brief 对象赋值
     * @details 如果不能赋值则退化为析构+构造
     */
    template <typename T, typename Arg, bool ex = xyu::t_can_assign<T,Arg>>
    T& object_assign(T& t, Arg&& v) noexcept(sizeof(T) == 0 || (ex ? xyu::t_can_nothrow_assign<T,Arg> : (xyu::t_can_nothrow_destruct<T> && xyu::t_can_nothrow_init<T, Arg>)))
    {
        if constexpr (sizeof(T) == 0);
        else if constexpr (ex) t = forward<Arg>(v);
        else t.~T(), place_init(&t, forward<Arg>(v));
        return t;
    }
}

/// 交换
namespace xylu::xytraits
{
    namespace __
    {
        template <typename T, typename = decltype(t_val<T&>().swap(t_val<T&>()))>
        size_c<1> has_swap_test(int);
        template <typename T, typename = decltype(swap(t_val<T&>(), t_val<T&>()))>
        size_c<2> has_swap_test(long);
        template <typename T, bool valid = t_can_mvconstr<T> && t_can_mvassign<T>>
        size_c<valid ? 3 : 0> has_swap_test(...);
        template <typename T>
        constexpr xyu::size_t get_swap_type = decltype(has_swap_test<T>(0))::value;
    }
    /**
     * @brief 通用的交换函数
     * @details 此版本调用 类型本身提供的 a.swap(b) 成员函数
     */
    template <typename T, t_enable<__::get_swap_type<T> == 1, int> = 1>
    constexpr void swap(T& a, T& b) noexcept(noexcept(a.swap(b))) { a.swap(b); }
    /**
     * @brief 通用的交换函数
     * @details 此版本调用 特定名称空间下的 swap(a, b) 函数
     */
    template <typename T, t_enable<__::get_swap_type<T> == 2, int> = 2>
    constexpr void swap(T& a, T& b) noexcept(noexcept(swap(a, b))) { swap(a, b); }
    /**
     * @brief 通用的交换函数
     * @details 此版本基于移动构造与赋值的默认实现
     */
    template <typename T, t_enable<__::get_swap_type<T> == 3, int> = 3>
    constexpr void swap(T& a, T& b) noexcept(t_can_nothrow_mvconstr<T> && t_can_nothrow_mvassign<T>)
    {
        T tmp(move(a));
        a = move(b);
        b = move(tmp);
    }
}

/// 特殊函数控制生成
namespace xylu::xytraits
{
    /**
     * @brief 自动决定是否生成 复制构造函数
     * @tparam Derived 基类，需要继承该类并设为友元
     * @tparam can_cpconstr 生成条件
     * @attention 如有数据成员，为防止覆盖，应该在该类之前继承，并使其对应的特殊函数体为空
     */
    template <typename Derived, bool can_cpconstr>
    struct class_auto_copy_construct_t : xyu::class_no_copy_construct_t {};
    template <typename Derived>
    struct class_auto_copy_construct_t<Derived, true>
    {
    #define XY_SELF class_auto_copy_construct_t
        constexpr XY_SELF() = default;
        constexpr XY_SELF& operator=(const XY_SELF&) = default;
        constexpr XY_SELF(XY_SELF&&) = default;
        constexpr XY_SELF& operator=(XY_SELF&&) = default;

        constexpr XY_SELF(const XY_SELF& other) noexcept(noexcept(derived().do_cpconstr(other.derived())))
        { derived().do_cpconstr(other.derived()); }
    #undef XY_SELF
    private:
        constexpr Derived& derived() noexcept { return static_cast<Derived&>(*this); }
        constexpr const Derived& derived() const noexcept { return static_cast<const Derived&>(*this); }
    };

    /**
     * @brief 自动决定是否生成 复制赋值运算符
     * @tparam Derived 基类，需要继承该类并设为友元
     * @tparam can_cpassign 生成条件
     * @attention 如有数据成员，为防止覆盖，应该在该类之前继承，并使其对应的特殊函数体为空
     */
    template <typename Derived, bool can_cpassign>
    struct class_auto_copy_assign_t : xyu::class_no_copy_assign_t {};
    template <typename Derived>
    struct class_auto_copy_assign_t<Derived, true>
    {
    #define XY_SELF class_auto_copy_assign_t
        constexpr XY_SELF() = default;
        constexpr XY_SELF(const XY_SELF&) = default;
        constexpr XY_SELF(XY_SELF&&) = default;
        constexpr XY_SELF& operator=(XY_SELF&&) = default;

        constexpr XY_SELF& operator=(const XY_SELF& other) noexcept(noexcept(derived().do_cpassign(other.derived())))
        { derived().do_cpassign(other.derived()); return *this; }
    #undef XY_SELF
    private:
        constexpr Derived& derived() noexcept { return static_cast<Derived&>(*this); }
        constexpr const Derived& derived() const noexcept { return static_cast<const Derived&>(*this); }
    };

    /**
     * @brief 自动决定是否生成 移动构造函数
     * @tparam Derived 基类，需要继承该类并设为友元
     * @tparam can_mvconstr 生成条件
     * @attention 如有数据成员，为防止覆盖，应该在该类之前继承，并使其对应的特殊函数体为空
     */
    template <typename Derived, bool can_mvconstr>
    struct class_auto_move_construct_t : xyu::class_no_move_construct_t {};
    template <typename Derived>
    struct class_auto_move_construct_t<Derived, true>
    {
    #define XY_SELF class_auto_move_construct_t
        constexpr XY_SELF() = default;
        constexpr XY_SELF(const XY_SELF&) = default;
        constexpr XY_SELF& operator=(const XY_SELF&) = default;
        constexpr XY_SELF& operator=(XY_SELF&&) = default;

        constexpr XY_SELF(XY_SELF&& other) noexcept(noexcept(derived().do_mvconstr(static_cast<Derived&&>(other.derived()))))
        { derived().do_mvconstr(static_cast<Derived&&>(other.derived())); }
    #undef XY_SELF
    private:
        constexpr Derived& derived() noexcept { return static_cast<Derived&>(*this); }
    };

    /**
     * @brief 自动决定是否生成 移动赋值运算符
     * @tparam Derived 基类，需要继承该类并设为友元
     * @tparam can_mvassign 生成条件
     * @attention 如有数据成员，为防止覆盖，应该在该类之前继承，并使其对应的特殊函数体为空
     */
    template <typename Derived, bool can_mvassign>
    struct class_auto_move_assign_t : xyu::class_no_move_assign_t {};
    template <typename Derived>
    struct class_auto_move_assign_t<Derived, true>
    {
    #define XY_SELF class_auto_move_assign_t
        constexpr XY_SELF() = default;
        constexpr XY_SELF(const XY_SELF&) = default;
        constexpr XY_SELF& operator=(const XY_SELF&) = default;
        constexpr XY_SELF(XY_SELF&&) = default;

        constexpr XY_SELF& operator=(XY_SELF&& other) noexcept(noexcept(derived().do_mvassign(static_cast<Derived&&>(other.derived()))))
        { derived().do_mvassign(static_cast<Derived&&>(other.derived())); return *this; }
    #undef XY_SELF
    private:
        constexpr Derived& derived() noexcept { return static_cast<Derived&>(*this); }
    };
}
