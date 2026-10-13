#pragma once

/**
 * @file    xycoll/refval.h
 * @brief   引用对象
 * @date    2026-06-16
 */

#include "./__/tool.h"
#include "../xystd/structbind.h"

namespace xylu::xycoll
{
    namespace __
    {
        template <typename T>
        struct RefValData
        {
            T* p;

            constexpr RefValData(T* p = nullptr) noexcept : p(p) {}
            constexpr RefValData(const RefValData& other) noexcept = default;
            constexpr RefValData(RefValData&& other) noexcept = default;
            constexpr RefValData& operator=(const RefValData&) noexcept {}
            constexpr RefValData& operator=(RefValData&&) noexcept {}
        };
    }

    /**
     * @brief 引用容器
     * @tparam T  引用对象的类型
     *
     * @details
     *   存储方式：通过指针模拟引用语义
     *
     *   方法简述：
     *   - 构造与赋值原生引用完全相同
     *   - 可以隐式转换为引用，或者用 `get` 显式获取底层引用
     *   - 可以通过 `xyu::native_v` 进行延迟构造，并通过 `rebind` 进行重绑定
     *
     *   注意事项：
     *   - 空指针：延迟构造后未绑定直接调用 `get` 会在 !XY_RELEASE 下检查并触发 abort
     *   - 拷贝/移动赋值：完全模拟原生引用，不会进行指针赋值，而是原对象赋值
     *   - 无法转发const：与原生引用相同，会阻断 const 传播
     */
    template <typename T>
    struct RefVal : __::RefValData<T>,
            xyu::class_auto_copy_assign_t<RefVal<T>, xyu::t_can_cpassign<T>>,
            xyu::class_auto_move_assign_t<RefVal<T>, xyu::t_can_mvassign<T>>
    {
        static_assert(!xyu::t_is_refer<T>, "can't refer to a reference");

        /* 引用操作 */

        /// 构造
        constexpr RefVal(T& val) noexcept : __::RefValData<T>(&val) {}

        /// 赋值
        template <typename U, typename = xyu::t_enable<xyu::t_can_assign<T&, U>>>
        constexpr RefVal& operator=(U&& v) noexcept
        {
            *__::Checker::check_ptr(this->p, XY_CurPos("RefVal::operator=")) = xyu::forward<U>(v);
            return *this;
        }

        /// 隐式转换
        constexpr operator T&() const noexcept { return get(); }

        /// 复制构造
        constexpr RefVal(const RefVal& val) noexcept : __::RefValData<T>(val.p) {}
        /// 移动构造
        constexpr RefVal(RefVal&& val) noexcept : __::RefValData<T>(val.p) {}

        /* 包装器操作 */

        /// 延迟构造
        constexpr explicit RefVal(xyu::native_t) noexcept : __::RefValData<T>{} {}

        /// 检查并获取对象
        constexpr T& get() const noexcept { return *__::Checker::check_ptr(this->p, XY_CurPos("RefVal::get")); }

        /// 是否已绑定对象
        constexpr bool valid() const noexcept { return this->p != nullptr; }
        /// 重绑定对象
        constexpr RefVal& rebind(T& val) noexcept { this->p = &val; return *this; }

    private:
        friend struct xyu::class_auto_copy_assign_t<RefVal, xyu::t_can_cpassign<T>>;
        friend struct xyu::class_auto_move_assign_t<RefVal, xyu::t_can_mvassign<T>>;
        // 复制赋值
        constexpr void do_cpassign(const RefVal& other) noexcept(xyu::t_can_nothrow_cpassign<T>)
        {
            *__::Checker::check_ptr(XY_CurPos("RefVal::operator="), this->p, other.p);
            *this->p = *other.p;
        }
        // 移动赋值
        constexpr void do_mvassign(RefVal&& other) noexcept(xyu::t_can_nothrow_mvassign<T>)
        {
            *__::Checker::check_ptr(XY_CurPos("RefVal::operator="), this->p, other.p);
            get() = xyu::move(other.get());
        }
    };

}

/// const 属性
namespace xylu::xytraits::__
{
    // 移除 const
    template <typename T>
    struct remove_const<xyu::RefVal<T>> { using type = xyu::RefVal<t_rem_const<T>>; };

    // 添加 const
    template <typename T>
    struct add_const<xyu::RefVal<T>> { using type = xyu::RefVal<t_add_const<T>>; };
}

/// 格式化
namespace xylu::xystring
{
    // 隐式转换为原类型
    template <typename T>
    struct FormatImpl<xyu::RefVal<T>> : FormatImpl<T> {};
}

/// 结构化绑定
namespace std
{
    //元素容量
    template <typename T>
    struct tuple_size<xyu::RefVal<T>> : xyu::size_c<tuple_size<T>::value> {};

    //元素类型
    template <xyu::size_t index, typename T>
    struct tuple_element<index, xyu::RefVal<T>> { using type = typename tuple_element<index, T>::type; };
}
