#pragma once

/**
 * @file    xymath/of.h
 * @brief   基础计算溢出检测
 * @date    2026-08-07
 */

#include "_.h"
#include "../xycore/error.h"
#include "../xytraits/numeric.h"

namespace xylu::xymath
{
    /// 计算 a + b 得到 ret 并检查是否溢出
    template <typename R, typename T, typename U, typename = xyu::t_enable<xyu::t_is_integer<T, U, R>>>
    constexpr bool calc_of_add(T a, U b, R& ret) noexcept { return __builtin_add_overflow(a, b, &ret); }

    /**
     * @brief 计算 a + b 得到 ret 并检查是否溢出
     * @tparam R 期望返回类型，不指定则为 decltype(a + b)
     * @tparam check 是否进行检查，默认为 true
     * @note 溢出会执行 abort_overflow
     */
    template <typename R = void, bool check = true, typename T, typename U, typename = xyu::t_enable<xyu::t_is_integer<T, U>>>
    constexpr auto calc_of_add(T a, U b, xyu::ErrorPos pos) noexcept
    {
        xyu::t_cond<xyu::t_is_void<R>, decltype(a + b), R> ret{};
        if constexpr (!check) ret = a + b;
        else if (XY_UNLIKELY(calc_of_add(a, b, ret)))
            xyu::E_Logic::abort_overflow(pos, a, b, "add");
        return ret;
    }


    /// 计算 a - b 得到 ret 并检查是否溢出
    template <typename R, typename T, typename U, typename = xyu::t_enable<xyu::t_is_integer<T, U, R>>>
    constexpr bool calc_of_sub(T a, U b, R& ret) noexcept { return __builtin_sub_overflow(a, b, &ret); }

    /**
     * @brief 计算 a - b 得到 ret 并检查是否溢出
     * @tparam R 期望返回类型，不指定则为 decltype(a - b)
     * @tparam check 是否进行检查，默认为 true
     * @note 溢出会执行 abort_overflow
     */
    template <typename R = void, bool check = true, typename T, typename U, typename = xyu::t_enable<xyu::t_is_integer<T, U>>>
    constexpr auto calc_of_sub(T a, U b, xyu::ErrorPos pos) noexcept
    {
        xyu::t_cond<xyu::t_is_void<R>, decltype(a - b), R> ret{};
        if constexpr (!check) ret = a - b;
        else if (XY_UNLIKELY(calc_of_sub(a, b, ret)))
            xyu::E_Logic::abort_overflow(pos, a, b, "sub");
        return ret;
    }


    /// 计算 a * b 得到 ret 并检查是否溢出
    template <typename R, typename T, typename U, typename = xyu::t_enable<xyu::t_is_integer<T, U, R>>>
    constexpr bool calc_of_mul(T a, U b, R& ret) noexcept { return __builtin_mul_overflow(a, b, &ret); }

    /**
     * @brief 计算 a * b 得到 ret 并检查是否溢出
     * @tparam R 期望返回类型，不指定则为 decltype(a * b)
     * @tparam check 是否进行检查，默认为 true
     * @note 溢出会执行 abort_overflow
     */
    template <typename R = void, bool check = true, typename T, typename U, typename = xyu::t_enable<xyu::t_is_integer<T, U>>>
    constexpr auto calc_of_mul(T a, U b, xyu::ErrorPos pos) noexcept
    {
        xyu::t_cond<xyu::t_is_void<R>, decltype(a * b), R> ret{};
        if constexpr (!check) ret = a * b;
        else if (XY_UNLIKELY(calc_of_mul(a, b, ret)))
            xyu::E_Logic::abort_overflow(pos, a, b, "mul");
        return ret;
    }


    /// 计算 a / b 得到 ret 并检查是否溢出
    template <typename R, typename T, typename U, typename = xyu::t_enable<xyu::t_is_integer<T, U, R>>>
    constexpr bool calc_of_div(T a, U b, R& ret) noexcept
    {
        bool ng1 = a < 0, ng2 = b < 0;
        // 除数为0
        if (XY_UNLIKELY(b == 0)) return true;
        // 同号
        if (ng1 == ng2)
        {
            using V = xyu::t_wide<T, U, decltype(a / b)>;
            if constexpr (sizeof(T) == sizeof(V))
                if (ng2 && XY_UNLIKELY(b == -1 && a == xyu::t_number<V>::min)) return true;
            V r = V(a) / V(b);
            if (r > xyu::t_number<R>::max) return true;
            ret = r;
        }
        // 异号
        else
        {
            using V = xyu::t_chg_sign2un<xyu::t_wide<T, U, decltype(a / b)>>;
            V r{};
            if (ng1) r = -V(a) / V(b);
            else r = V(a) / -V(b);
            if constexpr (xyu::t_number<R>::is_unsigned)
            {
                if (r != 0) return true;
                ret = 0;
            }
            else
            {
                constexpr auto limit = xyu::make_unsigned(xyu::t_number<R>::max) + 1;
                if (r > limit) return true;
                if (r == limit) ret = xyu::t_number<R>::min;
                else ret = -R(r);
            }
        }
        return false;
    }

    /**
     * @brief 计算 a / b 得到 ret 并检查是否溢出
     * @tparam R 期望返回类型，不指定则为 decltype(a / b)
     * @tparam check 是否进行检查，默认为 true
     * @note 溢出会执行 abort_overflow
     */
    template <typename R = void, bool check = true, typename T, typename U, typename = xyu::t_enable<xyu::t_is_integer<T, U>>>
    constexpr auto calc_of_div(T a, U b, xyu::ErrorPos pos) noexcept
    {
        xyu::t_cond<xyu::t_is_void<R>, decltype(a / b), R> ret{};
        if constexpr (!check) ret = a / b;
        else if (XY_UNLIKELY(calc_of_div(a, b, ret)))
            xyu::E_Logic::abort_overflow(pos, a, b, "div");
        return ret;
    }


    /// 计算 a % b 得到 ret 并检查是否溢出
    template <typename R, typename T, typename U, typename = xyu::t_enable<xyu::t_is_integer<T, U, R>>>
    constexpr bool calc_of_mod(T a, U b, R& ret) noexcept
    {
        bool ng1 = a < 0, ng2 = b < 0;
        // 除数为0
        if (XY_UNLIKELY(b == 0)) return true;
        // 同号
        if (ng1 == ng2)
        {
            using V = xyu::t_wide<T, U, decltype(a % b)>;
            if constexpr (sizeof(T) == sizeof(V))
                if (ng2 && XY_UNLIKELY(b == -1 && a == xyu::t_number<V>::min)) return true;
            V r = V(a) % V(b);
            if (r >= 0) { if (r > xyu::t_number<R>::max) return true; }
            else if constexpr (xyu::t_number<R>::is_unsigned) return true;
            else if (r < xyu::t_number<R>::min) return true;
            ret = r;
        }
        // 异号
        else
        {
            using V = xyu::t_chg_sign2un<xyu::t_wide<T, U, decltype(a % b)>>;
            if (ng1)
            {
                V r = -V(a) % V(b);
                if constexpr (xyu::t_number<R>::is_unsigned) { if (r != 0) return true; }
                constexpr auto limit = xyu::make_unsigned(xyu::t_number<R>::max) + 1;
                if (r > limit) return true;
                if (r == limit) ret = xyu::t_number<R>::min;
                else ret = -R(r);
            }
            else
            {
                V r = V(a) % -V(b);
                if (r > xyu::t_number<R>::max) return true;
                ret = r;
            }
        }
        return false;
    }

    /**
     * @brief 计算 a % b 得到 ret 并检查是否溢出
     * @tparam R 期望返回类型，不指定则为 decltype(a % b)
     * @tparam check 是否进行检查，默认为 true
     * @note 溢出会执行 abort_overflow
     */
    template <typename R = void, bool check = true, typename T, typename U, typename = xyu::t_enable<xyu::t_is_integer<T, U>>>
    constexpr auto calc_of_mod(T a, U b, xyu::ErrorPos pos) noexcept
    {
        xyu::t_cond<xyu::t_is_void<R>, decltype(a % b), R> ret{};
        if constexpr (!check) ret = a % b;
        else if (XY_UNLIKELY(calc_of_mod(a, b, ret)))
            xyu::E_Logic::abort_overflow(pos, a, b, "mod");
        return ret;
    }

}
