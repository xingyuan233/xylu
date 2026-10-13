#pragma once

/**
 * @file    xycoll/tagval.h
 * @brief   标签类型
 * @date    2026-05-08
 */

#include "./_.h"
#include "../xytraits/class.h"

namespace xylu::xycoll
{
    /**
     * @brief 标签容器
     * @tparam T     原始类型
     * @tparam Tag   索引标签，通常用于区分多个可能为同类型的参数
     * @tparam Owner 所有者类型，用于进一步区分类型来源
     *
     * @details
     *   当 T 为 空类且可继承时，TagVal 会私有继承它，否则作为私有成员。
     *   通过 val() [const] 来访问 T 的实例。
     *   常用于以下情况：
     *   - 空基类优化
     *   - 多重继承中区分多个同类型基类
     *   - 自动转发 () 构造与 {} 构造
     *   - 泛型中用于条件构造 (Tag < 0)
     */
    template <typename T, auto Tag = 0, typename Owner = void,
              bool extend = xyu::t_is_empty<T> && !xyu::t_is_final<T>>
    struct TagVal;

    /// 继承
    template <typename T, auto Tag, typename Owner>
    struct TagVal<T, Tag, Owner, true> : private T
    {
        /// 任意构造函数
        template <typename... Args, typename Test = T, xyu::t_enable<!xyu::t_is_aggregate<Test> && xyu::t_can_construct<Test, Args...>, bool> = false>
        explicit constexpr TagVal(Args&&... args) noexcept(xyu::t_can_nothrow_construct<T, Args...>) : T(xyu::forward<Args>(args)...) {}

        /// 聚合类型构造函数
        template <typename... Args, typename Test = T, xyu::t_enable<xyu::t_is_aggregate<Test> && xyu::t_can_listinit<Test, Args...>, bool> = true>
        explicit constexpr TagVal(Args&&... args) noexcept(xyu::t_can_nothrow_listinit<T, Args...>) : T{xyu::forward<Args>(args)...} {}

        /// 自适应构造函数
        template <typename... Args, typename Test = T, auto param = Tag, xyu::t_enable<param < 0 && xyu::t_can_init<Test, Args...>, bool> = true>
        explicit constexpr TagVal(xyu::special_t, Args&&... args) noexcept(xyu::t_can_nothrow_init<T, Args...>) : TagVal(xyu::forward<Args>(args)...) {}

        /// 自适应构造函数
        template <typename... Args, typename Test = T, auto param = Tag, xyu::t_enable<param < 0 && !xyu::t_can_init<Test, Args...>, bool> = false>
        explicit constexpr TagVal(xyu::special_t, Args&&...) noexcept(xyu::t_can_nothrow_dfconstr<T>) : T{} {}

        /// 获取值 &
        constexpr T& get() & noexcept { return *this; }
        /// 获取值 &&
        constexpr T&& get() && noexcept { return move(*this); }
        /// 获取值 const
        constexpr xyu::t_add_const<T>& get() const& noexcept { return *this; }
    };

    /// 组合
    template <typename T, auto Tag, typename Owner>
    struct TagVal<T, Tag, Owner, false>
    {
    private:
        T v;

    public:
        /// 任意构造函数
        template <typename... Args, typename Test = T, xyu::t_enable<!xyu::t_is_aggregate<Test> && xyu::t_can_construct<Test, Args...>, bool> = false>
        explicit constexpr TagVal(Args&&... args) noexcept(xyu::t_can_nothrow_construct<T, Args...>) : v(xyu::forward<Args>(args)...) {}

        /// 聚合类型构造函数
        template <typename... Args, typename Test = T, xyu::t_enable<xyu::t_is_aggregate<Test> && xyu::t_can_listinit<Test, Args...>, bool> = true>
        explicit constexpr TagVal(Args&&... args) noexcept(xyu::t_can_nothrow_listinit<T, Args...>) : v{xyu::forward<Args>(args)...} {}

        /// 自适应构造函数
        template <typename... Args, typename Test = T, auto param = Tag, xyu::t_enable<param < 0 && xyu::t_can_init<Test, Args...>, bool> = true>
        explicit constexpr TagVal(xyu::special_t, Args&&... args) noexcept(xyu::t_can_nothrow_init<T, Args...>) : TagVal(xyu::forward<Args>(args)...) {}

        /// 自适应构造函数
        template <typename... Args, typename Test = T, auto param = Tag, xyu::t_enable<param < 0 && !xyu::t_can_init<Test, Args...>, bool> = false>
        explicit constexpr TagVal(xyu::special_t, Args&&...) noexcept(xyu::t_can_nothrow_dfconstr<T>) : v{} {}

        /// 获取值
        constexpr T& get() & noexcept { return v; }
        /// 获取值 &&
        constexpr T&& get() && noexcept { return move(v); }
        /// 获取值 const
        constexpr xyu::t_add_const<T>& get() const& noexcept { return v; }
    };

    /// void 特化
    // 作为空对象
    template <auto Tag, typename Owner>
    struct TagVal<void, Tag, Owner, false>
    {
        constexpr TagVal() = default;

        /// 自适应构造函数
        template <typename... Args, auto param = Tag, typename = xyu::t_enable<param < 0>>
        explicit constexpr TagVal(xyu::special_t, Args&&...) noexcept {}
    };

}
