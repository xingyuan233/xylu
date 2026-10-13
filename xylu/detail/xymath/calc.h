#pragma once

/**
 * @file    xymath/calc.h
 * @brief   基础数值计算
 * @date    2025-12-25
 */

#include "./bit.h"
#include "../xycore/error.h"
#include "../xycore/config.h"
#include "../xytraits/numeric.h"

/// 常用计算
namespace xylu::xymath
{
    /**
     * @brief 计算 x 的绝对值
     * @attention 对于有符号整数，abs(MIN) 为 ub
     */
    template <typename T>
    constexpr decltype(auto) calc_abs(T x) noexcept
    {
        static_assert(xyu::t_is_number<T>);
        if constexpr (!XY_RELEASE && xyu::t_number<T>::is_signed)
        {
            if (XY_UNLIKELY(x == xyu::t_number<T>::min))
                xyu::E_Logic::abort_invalid_value(XY_CurPos(), x);
        }
        if (xyu::t_constant()) return x < 0 ? -x : x;
        else if constexpr (xyu::t_number<T>::is_unsigned)   return x;
        else if constexpr (xyu::t_is_same<T, int>)          return __builtin_abs(x);
        else if constexpr (xyu::t_is_same<T, long>)         return __builtin_labs(x);
        else if constexpr (xyu::t_is_same<T, long long>)    return __builtin_llabs(x);
        else if constexpr (xyu::t_is_same<T, float>)        return __builtin_fabsf(x);
        else if constexpr (xyu::t_is_same<T, double>)       return __builtin_fabs(x);
        else if constexpr (xyu::t_is_same<T, long double>)  return __builtin_fabsl(x);
        else return x < 0 ? -x : x;
    }

    /// 获取 x 和 y 中的较小值
    template <typename T, typename U>
    constexpr auto calc_min(T x, U y) noexcept
    {
        static_assert(xyu::t_is_number<T> && xyu::t_is_number<U>);
        // 类型不同则使用更宽的类型
        using W = xyu::t_wide<T, U>;
        if constexpr (!xyu::t_is_same<T, U>) return calc_min<W,W>(x, y);
        // 相同类型
        else if (xyu::t_constant()) return x <= y ? x : y;
        else if constexpr (xyu::t_is_same<T, float>)        return __builtin_fminf(x, y);
        else if constexpr (xyu::t_is_same<T, double>)       return __builtin_fmin(x, y);
        else if constexpr (xyu::t_is_same<T, long double>)  return __builtin_fminl(x, y);
        else return x <= y ? x : y;
    }

    /// 获取 x 和 y 中的较大值
    template <typename T, typename U>
    constexpr auto calc_max(T x, U y) noexcept
    {
        static_assert(xyu::t_is_number<T> && xyu::t_is_number<U>);
        // 类型不同则使用更宽的类型
        using W = xyu::t_wide<T, U>;
        if constexpr (!xyu::t_is_same<T, U>) return calc_max<W,W>(x, y);
        else if (xyu::t_constant()) return x >= y ? x : y;
        else if constexpr (xyu::t_is_same<T, float>)        return __builtin_fmaxf(x, y);
        else if constexpr (xyu::t_is_same<T, double>)       return __builtin_fmax(x, y);
        else if constexpr (xyu::t_is_same<T, long double>)  return __builtin_fmaxl(x, y);
        else return x >= y ? x : y;
    }

    /// 获取最小值
    template <typename T>
    constexpr auto min(const T& x) noexcept { return x; }
    /// 获取最小值
    template <typename T, typename U, typename... Args>
    constexpr auto min(const T& x, const U& y, const Args&... args) noexcept
    {
        if constexpr (xyu::t_is_number<T> && xyu::t_is_number<U>) return min(calc_min(x, y), args...);
        else return min(x <= y ? x : y, args...);
    }

    /// 获取最大值
    template <typename T>
    constexpr auto max(const T& x) noexcept { return x; }
    /// 获取最大值
    template <typename T, typename U, typename... Args>
    constexpr auto max(const T& x, const U& y, const Args&... args) noexcept
    {
        if constexpr (xyu::t_is_number<T> && xyu::t_is_number<U>) return max(calc_max(x, y), args...);
        else return max(x >= y ? x : y, args...);
    }
}

/// 因数倍数
namespace xylu::xymath
{
    /**
     * @brief 计算两个整数的最大公约数 (GCD)
     * @details 采用二进制 Stein 算法实现
     *    - 若 a 或 b 中有一个为 0，则返回另一个非零值。若都为 0，返回 0。
     *    - 输入的 a 和 b 会被视为它们的绝对值进行计算，结果总是非负数。
     * @attention 对于有符号整数，a 或 b 是 INT_MIN 为 ub
     */
    template <typename T, typename U>
    constexpr auto calc_gcd(T a, U b) noexcept
    {
        static_assert(xyu::t_is_integer<T> && xyu::t_is_integer<U>);
        // 类型不同则使用更宽的类型
        using W = xyu::t_wide<T, U>;
        using uT = xyu::t_chg_sign2un<T>;
        if constexpr (!xyu::t_is_same<T, U>) return calc_gcd<W,W>(a, b);
        // 有符号转为无符号
        else if constexpr (xyu::t_number<T>::is_signed)
        {
            if constexpr (!XY_RELEASE)
            {
                if (XY_UNLIKELY(a == xyu::t_number<T>::min))
                    xyu::E_Logic::abort_invalid_value(XY_CurPos(), a, "a");
                if (XY_UNLIKELY(b == xyu::t_number<T>::min))
                    xyu::E_Logic::abort_invalid_value(XY_CurPos(), b, "b");
            }

            if (XY_UNLIKELY(a < 0)) a = -a;
            if (XY_UNLIKELY(b < 0)) b = -b;
            return calc_gcd<uT,uT>(a, b);
        }
        // 无符号同类型
        else
        {
            // 处理 0
            if (XY_UNLIKELY(a == 0)) return b;
            if (XY_UNLIKELY(b == 0)) return a;
            // 消去并获取2的最大幂
            const T i = bit_count_0_low(a);  a >>= i;
            const T j = bit_count_0_low(b);  b >>= j;
            const T k = i <= j ? i : j;
            // 辗转相减
            for (;;)
            {
                if (a < b) { T t = a; a = b; b = t; }
                a -= b;
                if (a == 0) return (T)(b << k);
                a >>= bit_count_0_low(a);
            }
        }
    }

    /**
     * @brief 计算两个整数的最小公倍数 (LCM)
     * @details 通过公式 lcm(a, b) = (|a * b|) / gcd(a, b) 进行计算
     *   - 若 a 或 b 中有一个为 0，则返回 0。
     *   - 结果的正负与 a*b 同号。
     * @attention 对于有符号整数，a 或 b 是 INT_MIN 为 ub
     */
    template <typename T, typename U>
    constexpr auto calc_lcm(T a, U b) noexcept
    {
        if (XY_UNLIKELY(a == 0 || b == 0)) return static_cast<xyu::t_wide<T,U>>(0);
        return a / calc_gcd(a, b) * b;
    }
}

/// 舍入函数
namespace xylu::xymath
{
    /**
     * @brief 四舍五入到最近的整数
     * @code calc_round(1.5) == 2.0, calc_round(-1.5) == -2.0
     */
    template <typename T>
    constexpr decltype(auto) calc_round(T x) noexcept
    {
        static_assert(xyu::t_is_number<T>);
        if constexpr (!xyu::t_number<T>::is_floating) return x;
        else if (xyu::t_constant())
        {
            constexpr T threshold = (sizeof(T) > 4) ? static_cast<T>(4.5e15) : static_cast<T>(1e7);
            if (x >= 0) return x >= threshold ? x : static_cast<T>(static_cast<unsigned long long>(x + 0.5));
            return x <= -threshold ? x : static_cast<T>(static_cast<long long>(x - 0.5));
        }
        else if constexpr (xyu::t_is_same<T, float>)    return __builtin_roundf(x);
        else if constexpr (xyu::t_is_same<T, double>)   return __builtin_round(x);
        else                                            return __builtin_roundl(x);
    }

    /**
     * @brief 向零取整（截断小数部分）
     * @code calc_trunc(1.9) == 1.0, calc_trunc(-1.9) == -1.0
     */
    template <typename T>
    constexpr decltype(auto) calc_trunc(T x) noexcept
    {
        static_assert(xyu::t_is_number<T>);
        if constexpr (!xyu::t_number<T>::is_floating) return x;
        else if (xyu::t_constant())
        {
            constexpr T threshold = (sizeof(T) > 4) ? static_cast<T>(4.5e15) : static_cast<T>(1e7);
            if (x >= 0) return x >= threshold ? x : static_cast<T>(static_cast<unsigned long long>(x));
            return x <= -threshold ? x : static_cast<T>(static_cast<long long>(x));
        }
        else if constexpr (xyu::t_is_same<T, float>)    return __builtin_truncf(x);
        else if constexpr (xyu::t_is_same<T, double>)   return __builtin_trunc(x);
        else                                            return __builtin_truncl(x);
    }

    /**
     * @brief 向上取整（不小于 x 的最小整数）
     * @code calc_ceil(1.1) == 2.0, calc_ceil(-1.9) == -1.0
     */
    template <typename T>
    constexpr decltype(auto) calc_ceil(T x) noexcept
    {
        static_assert(xyu::t_is_number<T>);
        if constexpr (!xyu::t_number<T>::is_floating) return x;
        else if (xyu::t_constant())
        {
            if (x == 0) return x;
            constexpr T threshold = (sizeof(T) > 4) ? static_cast<T>(4.5e15) : static_cast<T>(1e7);
            if (x >= threshold || x <= -threshold) return x;
            auto truncated = static_cast<long long>(x);
            T r = static_cast<T>(truncated);
            if (x > r) return static_cast<T>(r + 1);
            return r;
        }
        else if constexpr (xyu::t_is_same<T, float>)    return __builtin_ceilf(x);
        else if constexpr (xyu::t_is_same<T, double>)   return __builtin_ceil(x);
        else                                            return __builtin_ceill(x);
    }

    /**
     * @brief 向下取整（不大于 x 的最大整数）
     * @code calc_floor(1.9) == 1.0, calc_floor(-1.1) == -2.0
     */
    template <typename T>
    constexpr decltype(auto) calc_floor(T x) noexcept
    {
        static_assert(xyu::t_is_number<T>);
        if constexpr (!xyu::t_number<T>::is_floating) return x;
        else if (xyu::t_constant())
        {
            if (x == 0) return x;
            constexpr T threshold = (sizeof(T) > 4) ? static_cast<T>(4.5e15) : static_cast<T>(1e7);
            if (x >= threshold || x <= -threshold) return x;
            auto truncated = static_cast<long long>(x);
            T r = static_cast<T>(truncated);
            if (x < r) return static_cast<T>(r - 1);
            return r;
        }
        else if constexpr (xyu::t_is_same<T, float>)    return __builtin_floorf(x);
        else if constexpr (xyu::t_is_same<T, double>)   return __builtin_floor(x);
        else                                            return __builtin_floorl(x);
    }
}

/// 数值状态
namespace xylu::xymath
{
    /// 检查是否为 NaN
    template <typename T>
    constexpr bool calc_is_nan(T v) noexcept
    {
        static_assert(xyu::t_is_number<T>);
        if constexpr (!xyu::t_is_floating<T>) return false;
        else return __builtin_isnan(v);
    }

    /// 检查是否为 Inf
    template <typename T>
    constexpr bool calc_is_inf(T v) noexcept
    {
        static_assert(xyu::t_is_number<T>);
        if constexpr (!xyu::t_is_floating<T>) return false;
        else return __builtin_isinf(v);
    }

    /// 检查是否为有限值 (既非 NaN 也非无穷大)
    template <typename T>
    constexpr bool calc_is_finite(T v) noexcept
    {
        static_assert(xyu::t_is_number<T>);
        if constexpr (!xyu::t_is_floating<T>) return true;
        else return __builtin_isfinite(v);
    }

    /**
     * @brief 检查浮点数是否为规格化数 (normalized)
     * @noto 整型也算规格化数
     */
    template <typename T>
    constexpr bool calc_is_normalized(T v) noexcept
    {
        static_assert(xyu::t_is_number<T>);
        if constexpr (!xyu::t_is_floating<T>) return true;
        else return __builtin_isnormal(v);
    }
}

/// 其他内置计算
namespace xylu::xymath
{
    /**
     * @brief 将浮点数 x 分解为 m * 2^exp，并返回整数指数 exp
     * @details 1 ≤ |m| < 2
     */
    template <typename T>
    decltype(auto) calc_decompose_exp(T x) noexcept
    {
        static_assert(xyu::t_is_number<T>);
        if constexpr (!xyu::t_is_floating<T>) return calc_decompose_exp<xyu::t_wide<T,float>>(x);
        else if constexpr (xyu::t_is_same<T, float>)    return __builtin_ilogbf(x);
        else if constexpr (xyu::t_is_same<T, double>)   return __builtin_ilogb(x);
        else                                            return __builtin_ilogbl(x);
    }

    /**
     * @brief 将浮点数 x 分解为 m * 2^exp，并返回 m
     * @details 1 ≤ |m| < 2
     * @attention exp 溢出为 ub
     */
    template <typename T, typename U = int>
    decltype(auto) calc_decompose(T x, U* exp = nullptr) noexcept
    {
        static_assert(xyu::t_is_number<T> && xyu::t_is_number<U>);

        int tmp;
        auto inner = [&](auto y) noexcept
        {
            if (exp)
            {
                if constexpr (!XY_RELEASE && xyu::t_is_integer<U>)
                {
                    if (XY_UNLIKELY(__builtin_sub_overflow(tmp, 1, exp)))
                        xyu::E_Logic::abort_overflow(XY_CurPos("calc_decompose"), tmp, 1, "sub");
                }
                else *exp = tmp-1;
            }
            return 2 * y;
        };

        if constexpr (!xyu::t_is_floating<T>) return calc_decompose<xyu::t_wide<T,float>>(x, exp);
        else if constexpr (xyu::t_is_same<T, float>)    return inner(__builtin_frexpf(x, &tmp));
        else if constexpr (xyu::t_is_same<T, double>)   return inner(__builtin_frexp(x, &tmp));
        else                                            return inner(__builtin_frexpl(x, &tmp));
    }

    /**
     * @brief 将浮点数 x 分解为 整数部分 i 和 小数部分 f，并返回 f
     * @details x = i + f，且 i 和 f 的符号与 x 相同
     * @attention i 溢出为 ub
     */
    template <typename T, typename U = T>
    decltype(auto) calc_extract(T x, U* i = nullptr) noexcept
    {
        static_assert(xyu::t_is_number<T> && xyu::t_is_number<U>);
        if constexpr (!xyu::t_is_floating<T>) return calc_extract<xyu::t_wide<T,float>>(x, i);

        xyu::t_rem_cr<T> tmp;
        auto inner = [&](auto y) noexcept
        {
            if (i)
            {
                if constexpr (!XY_RELEASE && xyu::t_is_integer<U>)
                {
                    if (XY_UNLIKELY(tmp < xyu::t_number<U>::min))
                        xyu::E_Logic::abort_under_limit(XY_CurPos("calc_extract"), tmp, 0, xyu::t_number<U>::min);
                    if (XY_UNLIKELY(tmp > xyu::t_number<U>::max))
                        xyu::E_Logic::abort_over_limit(XY_CurPos("calc_extract"), tmp, 0, xyu::t_number<U>::max);
                }
                *i = tmp;
            }
            return y;
        };

        if      constexpr (xyu::t_is_same<T, float>)    return inner(__builtin_modff(x, &tmp));
        else if constexpr (xyu::t_is_same<T, double>)   return inner(__builtin_modf(x, &tmp));
        else                                            return inner(__builtin_modfl(x, &tmp));
    }

    /**
     * @brief 计算浮点余数 (x % y)
     * @details 结果的符号与 x 相同
     * @attention y == 0 为 ub
     */
    template <typename T, typename U>
    constexpr decltype(auto) calc_modulo(T x, U y) noexcept
    {
        static_assert(xyu::t_is_number<T> && xyu::t_is_number<U>);
        if constexpr (!XY_RELEASE) if (XY_UNLIKELY(y == 0))
            xyu::E_Logic::abort_invalid_value(XY_CurPos(), 0, "divisor");
        // 类型不同则使用更宽的类型
        using W = xyu::t_wide<T, U>;
        if constexpr (!xyu::t_is_same<T, U>) return calc_modulo<W,W>(x, y);
        // 同类型处理
        else if constexpr (!xyu::t_is_floating<T>) return calc_modulo<xyu::t_wide<T,float>>(x, y);
        else if constexpr (xyu::t_is_same<T, float>)    return __builtin_fmodf(x, y);
        else if constexpr (xyu::t_is_same<T, double>)   return __builtin_fmod(x, y);
        else                                            return __builtin_fmodl(x, y);
    }

    /**
     * @brief 计算 IEEE 754 标准的浮点余数
     * @details 结果是 (x - n*y)，其中 n 是 x/y 四舍五入后的整数
     * @attention y == 0 为 ub
     */
    template <typename T, typename U>
    constexpr decltype(auto) calc_remain(T x, U y) noexcept
    {
        static_assert(xyu::t_is_number<T> && xyu::t_is_number<U>);
        if constexpr (!XY_RELEASE) if (XY_UNLIKELY(y == 0))
            xyu::E_Logic::abort_invalid_value(XY_CurPos(), 0, "divisor");
        // 类型不同则使用更宽的类型
        using W = xyu::t_wide<T, U>;
        if constexpr (!xyu::t_is_same<T, U>) return calc_remain<W,W>(x, y);
        // 同类型处理
        else if constexpr (!xyu::t_is_floating<T>) return calc_remain<xyu::t_wide<T,float>>(x, y);
        else if constexpr (xyu::t_is_same<T, float>)    return __builtin_remainderf(x, y);
        else if constexpr (xyu::t_is_same<T, double>)   return __builtin_remainder(x, y);
        else                                            return __builtin_remainderl(x, y);
    }

    /**
     * @brief 计算融合乘加 (x * y + z)
     * @details 在支持的硬件上，此操作通常作为单条指令执行，可以提供更高的精度和性能
     */
    template <typename T, typename U, typename V>
    constexpr decltype(auto) calc_mul_add(T x, U y, V z) noexcept
    {
        static_assert(xyu::t_is_number<T> && xyu::t_is_number<U> && xyu::t_is_number<V>);
        // 类型不同则使用更宽的类型
        using W = xyu::t_wide<T,U,V>;
        if constexpr (!xyu::t_is_same<T,U,V>) return calc_mul_add<W,W,W>(x, y, z);
        // 同类型处理
        else if constexpr (!xyu::t_is_floating<T>) return calc_mul_add<xyu::t_wide<T,float>>(x, y, z);
        else if constexpr (xyu::t_is_same<T, float>)    return __builtin_fmaf(x, y, z);
        else if constexpr (xyu::t_is_same<T, double>)   return __builtin_fma(x, y, z);
        else                                            return __builtin_fmal(x, y, z);
    }

    /**
     * @brief 计算 (x * 2^exp)
     * @details 这是一个高效的、通过直接操作浮点数指数部分来计算乘以2的幂的方法
     */
    template <typename T>
    constexpr decltype(auto) calc_mul_exp2(T x, int e2) noexcept
    {
        static_assert(xyu::t_is_number<T>);

        if constexpr (!xyu::t_is_floating<T>) return calc_mul_exp2<xyu::t_wide<T,float>>(x, e2);
        else if constexpr (xyu::t_is_same<T, float>)    return __builtin_ldexpf(x, e2);
        else if constexpr (xyu::t_is_same<T, double>)   return __builtin_ldexp(x, e2);
        else                                            return __builtin_ldexpl(x, e2);
    }
}
