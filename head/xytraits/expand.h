#pragma clang diagnostic push
#pragma ide diagnostic ignored "modernize-use-nodiscard"
#pragma ide diagnostic ignored "OCUnusedGlobalDeclarationInspection"
#pragma ide diagnostic ignored "google-explicit-constructor"
#pragma once

#include "./class.h"
#include "./layout.h"

/* 拓展属性 */

/** 通用函数 */
namespace xylu::xytraits
{
    /**
     * @brief 根据 T 的类型，有条件地将一个左值引用 v 转发为左值或右值引用。
     * @details 这是“完美转发”的核心组件之一。
     *          - 若 T 是左值引用类型 (如 int&)，v 被转发为左值引用 (int&)。
     *          - 若 T 是非引用类型 (如 int)，v 被转发为右值引用 (int&&)。
     */
    template <typename T>
    [[nodiscard]] constexpr T&& forward(t_remove_refer<T>& v) noexcept { return static_cast<T&&>(v); }

    /**
     * @brief 将一个右值引用 v 转发为右值引用。
     * @details 这是“完美转发”的另一个核心组件，用于处理右值实参。
     * @note 禁止将右值转发为左值引用，因为这会危险地延长临时对象的生命周期。
     */
    template<typename T>
    [[nodiscard]] constexpr T&& forward(t_remove_refer<T>&& v) noexcept
    {
        static_assert(!t_is_lrefer<T>, "forward must not be used to convert an rvalue to an lvalue");
        return static_cast<T&&>(v);
    }

    /// 无条件地将参数 v 转换为其类型的右值引用，表示该对象可以被“移走”。
    template <typename T>
    [[nodiscard]] constexpr t_remove_refer<T>&& move(T&& v) noexcept
    { return static_cast<t_remove_refer<T>&&>(v); }
    namespace __
    {
        template <typename T>
        decltype(t_val<T&>().swap(t_val<T&>()), true_c{}) has_member_swap_test(int);
        template <typename>
        false_c has_member_swap_test(void*);
        template <typename T>
        constexpr bool has_member_swap = decltype(__::has_member_swap_test<T>(0))::value;
    }

    /**
     * @brief 通用的交换函数，优先使用成员 swap 方法。
     * @details 若类型 T 提供了 `a.swap(b)` 成员函数，则调用它。
     *          这允许类型自定义其交换逻辑，通常可以实现比通用版本更高的效率。
     */
    template <typename T, t_enable<__::has_member_swap<T>, bool> = true>
    constexpr void swap(T& a, T& b) noexcept(noexcept(a.swap(b))) { a.swap(b); }
    /**
     * @brief 通用的交换函数，在没有成员 swap 时回退到基于移动语义的实现。
     * @details 此版本要求类型 T 是可移动构造和移动赋值的 (或退化为复制)。
     */
    template <typename T, t_enable<!__::has_member_swap<T>, bool> = false>
    constexpr void swap(T& a, T &b) noexcept(t_can_nothrow_mvconstr<T> && t_can_nothrow_mvassign<T>)
    {
        T tmp(move(a));
        a = move(b);
        b = move(tmp);
    }

    /**
     * @brief 为传入的参数 v 添加顶层 const 限定符，同时保持其值类别 (左值或右值)。
     * @return 若 v 是 T&，则返回 const T&；若 v 是 T&&，则返回 const T&&。
     */
    template <typename T>
    [[nodiscard]] constexpr t_add_const_inref<T>&& make_const(T&& v) noexcept
    { return static_cast<t_add_const_inref<T>&&>(v); }
}

/// 空基类优化
namespace xylu::xytraits
{
    /**
     * @brief 利用空基类优化（EBO）来存储一个可能为空类型的成员。
     * @details
     *   当类型 T 是一个空类时，t_ebo 会通过私有继承 T来持有它，从而使 t_ebo 的实例大小不因持有 T 而增加。
     *   如果 T 不是空类或被 final 修饰，则 t_ebo 会回退到常规的成员变量方式持有它。
     * @tparam T     要被持有的类型。
     * @tparam Tag   一个唯一的 xyu::size_t 标签，用于在同一个类中区分多个 t_ebo 实例。
     * @tparam Owner 所有者类型 (默认 void，通常不使用，直接用 private/protected 继承)。
     *
     * @note 通过 base() 成员函数来访问被持有的 T 的实例 (包含 非const 与 const 两个版本)。
     */
    template <typename T, xyu::size_t Tag, typename Owner = void, bool need_extend = t_is_empty<T> && !t_is_final<T>>
    struct t_ebo;

    // 继承
    template <typename T, xyu::size_t Tag, typename Owner>
    struct t_ebo<T, Tag, Owner, true> : private T
    {
        // 默认构造函数
        template <typename Test = T, t_enable<t_can_dfconstr<Test>, bool> = true>
        constexpr t_ebo() noexcept(t_can_nothrow_dfconstr<T>) : T{} {}
        // 泛型构造函数
        template <typename... Args, typename Test = T, t_enable<!t_is_aggregate<Test> && sizeof...(Args), bool> = true>
        constexpr t_ebo(Args&&... args) noexcept(t_can_nothrow_construct<T, Args...>) : T(forward<Args>(args)...) {}
        // 聚合类型初始化
        template <typename... Args, typename Test = T, t_enable<t_is_aggregate<Test> && sizeof...(Args), bool> = false>
        constexpr t_ebo(Args&&... args) noexcept(t_can_nothrow_listinit<T, Args...>) : T{forward<Args>(args)...} {}
        // 获取基类
        constexpr T& base() noexcept { return *this; }
        constexpr const T& base() const noexcept { return *this; }
    };

    // 组合
    template <typename T, xyu::size_t Tag, typename Owner>
    struct t_ebo<T, Tag, Owner, false>
    {
    private:
        T v;
    public:
        // 默认构造函数
        template <typename Test = T, t_enable<t_can_dfconstr<Test>, bool> = true>
        constexpr t_ebo() noexcept(t_can_nothrow_dfconstr<T>) : v{} {}
        // 泛型构造函数
        template <typename... Args, typename Test = T, t_enable<!t_is_aggregate<Test> && sizeof...(Args), bool> = true>
        constexpr t_ebo(Args&&... args) noexcept(t_can_nothrow_construct<T, Args...>) : v(forward<Args>(args)...) {}
        // 聚合类型初始化
        template <typename... Args, typename Test = T, t_enable<t_is_aggregate<Test> && sizeof...(Args), bool> = false>
        constexpr t_ebo(Args&&... args) noexcept(t_can_nothrow_listinit<T, Args...>) : v{forward<Args>(args)...} {}
        // 获取基类
        constexpr T& base() noexcept { return v; }
        constexpr const T& base() const noexcept { return v; }
    };
}

/// 引用封装
namespace xylu::xytraits
{
    /**
     * @brief 一个轻量级的引用包装器，使其可以像对象一样被存储和传递。
     * @details
     *   t_refer 内部通过指针持有一个对象的引用，解决了 C++ 引用不能被重新绑定、
     *   不能作为容器元素等限制。它提供了与原生引用几乎相同的语法（如赋值、隐式转换）。
     */
    template <typename T>
    struct t_refer
    {
        static_assert(!t_is_rrefer<T>, "rvalue reference can not bind to lvalue reference");
    private:
        // 存储指针
        t_remove_refer<T>* p;

    public:
        // 默认构造函数
        constexpr t_refer() = default;
        // 绑定到左值
        constexpr t_refer(T& v) noexcept : p{&v} {}
        // 重新绑定
        constexpr t_refer& rebind(T& v) noexcept { p = &v; return *this; }

        // 检查是否有效
        constexpr bool valid() const noexcept { return p != nullptr; }
        // 隐式转换为引用
        constexpr operator T&() const noexcept { return *p; }
        constexpr T& get() const noexcept { return *p; }

        // 赋值 (直接对引用赋值)
        template <typename U>
        constexpr t_refer& operator=(U&& v) noexcept { *p = forward<U>(v); return *this; }
    };

    // 推导指引
    template <typename T>
    t_refer(T) -> t_refer<T>;

    /// 创建 t_refer 实例的辅助函数。
    template <typename T>
    constexpr t_refer<T> make_refer(T& v) noexcept { return t_refer<T>(v); }
}

/// 根据 T,U 获取较宽的类型
namespace xylu::xytraits
{
    namespace __
    {
        template <typename T, typename U>
        constexpr decltype(auto) get_wider_test()
        {
            using namespace xyu;
            //存在非数值类型则返回void
            if constexpr (!t_is_number<T> || !t_is_number<U>) return;
            else
            {
                constexpr bool f1 = t_is_floating<T>;
                constexpr bool f2 = t_is_floating<U>;
                constexpr size_t s1 = sizeof(T);
                constexpr size_t s2 = sizeof(U);
                //[浮,浮]
                if constexpr (f1 && f2) return t_cond<s1 >= s2, T, U>{};
                    //[整,整]
                else if constexpr (!(f1 || f2))
                {
                    constexpr bool u1 = t_is_unsigned<T>;
                    constexpr bool u2 = t_is_unsigned<U>;
                    constexpr size_t s = (s1 >= s2 ? s1 : s2) + (u1 ^ u2);
                    if constexpr (s < sizeof(int16)) return int16{};
                    else if constexpr (s < sizeof(int32))  return int32{};
                    else if constexpr (s < sizeof(int64))  return int64{};
                    else return uint64{};
                }
                    //[浮,整]
                else if constexpr (f1)
                {
                    //浮点型能精确表示整型
                    if constexpr (s1 > s2) return T{};
                        //若不能
                    else if constexpr (sizeof(double) > s2) return double{};
                    else return (long double){};
                }
                    //[整,浮]
                else
                {
                    //整型能精确表示浮点型
                    if constexpr (s2 > s1) return U{};
                        //若不能
                    else if constexpr (sizeof(double) > s1) return double{};
                    else return (long double){};
                }
            }
        }
    }
    /**
     * @brief 找出两种数值类型中“更宽”（表示范围更大或精度更高）的一种。
     * @details
     *   这是 `t_wider` 的核心实现，用于两种类型间的比较。
     *   其选择规则如下：
     *   1. **整型 vs 整型**:
     *      - 若符号性不同（一个有符号，一个无符号），结果提升为能容纳两者的、更大的有符号类型。
     *      - 否则，返回字节数更大的类型。
     *   2. **浮点型 vs 浮点型**: 返回字节数更大的类型。
     *   3. **浮点型 vs 整型**: 返回能同时精确表示两者的浮点类型。
     *
     * @note 若需支持自定义数值类型，应特化此类模板。
     */
    template <typename T, typename U>
    struct t_wider_base { using type = decltype(__::get_wider_test<T,U>()); };
    template <typename T>
    struct t_wider_base<T,T> { using type = T; };
    template <typename T>
    struct t_wider_base<T,void> { using type = T; };
    template <typename T>
    struct t_wider_base<void,T> { using type = T; };

    namespace __
    {
        template <typename... Args>
        struct get_wider { using type = void; };
        template <typename T>
        struct get_wider<T> { using type = T; };
        template <typename T, typename U, typename... Args>
        struct get_wider<T, U, Args...>
        {
            using type = typename get_wider<typename t_wider_base<T, U>::type, Args...>::type;
        };
    }
    /**
     * @brief 在给定的类型参数包 Args... 中，递归地找出最宽的数值类型。
     * @details
     *   此 trait 通过连续调用 `t_wider_base` 来比较参数包中的所有类型。
     *   - 任何类型与 `void` 比较，结果为该类型本身。
     * @see t_wider_base
     * @example t_wider˂char, long, float˃ // 结果为 double
     */
    template <typename... Args>
    using t_wider = typename __::get_wider<Args...>::type;
}

#pragma clang diagnostic pop