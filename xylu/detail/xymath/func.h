#pragma once

/**
 * @file    xymath/func.h
 * @brief   常用数学函数：统一的高性能数学接口
 * @date    2025-12-25
 */

#include "./_.h"
#include "../xytraits/numeric.h"

#define XY_calc_func_gen( name, func ) \
        static_assert(xyu::t_is_number<T>); \
        if constexpr (!xyu::t_is_floating<T>) return calc_##name<xyu::t_wide<T,float>>(x); \
        else if constexpr (xyu::t_is_same<T, float>)    return __builtin_##func##f(x); \
        else if constexpr (xyu::t_is_same<T, double>)   return __builtin_##func(x); \
        else                                            return __builtin_##func##l(x);

#define XY_calc_func2_gen( name, func ) \
        static_assert(xyu::t_is_number<T> && xyu::t_is_number<U>); \
        using W = xyu::t_wide<T, U>; \
        if constexpr (!xyu::t_is_same<T, U>) return calc_##name<W,W>(x, y); \
        else if constexpr (!xyu::t_is_floating<T>) return calc_##name<xyu::t_wide<T,float>>(x, y); \
        else if constexpr (xyu::t_is_same<T, float>)    return __builtin_##func##f(x, y); \
        else if constexpr (xyu::t_is_same<T, double>)   return __builtin_##func(x, y); \
        else                                            return __builtin_##func##l(x, y);

/// 次方与开方函数
namespace xylu::xymath
{
    /// 幂函数 x^y
    template <typename T, typename U>
    auto calc_pow(T x, U y) noexcept { XY_calc_func2_gen( pow, pow ) }

    /**
     * @brief 平方根 √x
     * @note 若 x 为负数，结果为 NaN
     */
    template <typename T>
    auto calc_sqrt(T x) noexcept { XY_calc_func_gen( sqrt, sqrt ) }

    /// 立方根 ∛x
    template <typename T>
    auto calc_cbrt(T x) noexcept { XY_calc_func_gen( cbrt, cbrt ) }

    /**
     * @brief 计算直角三角形的斜边长度 sqrt(x*x + y*y)
     * @details 此函数在计算上比直接实现更精确，且能更好地避免中间结果溢出
     */
    template <typename T, typename U>
    auto calc_hypot(T x, U y) noexcept { XY_calc_func2_gen( hypot, hypot ) }
}

/// 指数与对数函数
namespace xylu::xymath
{
    /// 自然指数 e^x
    template <typename T>
    auto calc_exp(T x) noexcept { XY_calc_func_gen( exp, exp ) }

    /// 2^x
    template <typename T>
    auto calc_exp2(T x) noexcept { XY_calc_func_gen( exp2, exp2 ) }

    /**
     * @brief e^x - 1
     * @details 当 x 接近 0 时，此函数的结果比 `calc_exp(x) - 1` 更精确
     */
    template <typename T>
    auto calc_exp_m1(T x) noexcept { XY_calc_func_gen( exp_m1, expm1 ) }

    /**
     * @brief 自然对数 lnx
     * @details 若 x 为 0，结果为 -Inf；若 x 为负数，结果为 NaN
     */
    template <typename T>
    auto calc_ln(T x) noexcept { XY_calc_func_gen( ln, log ) }

    /// 常用对数 lgx (以 10 为底)
    template <typename T>
    auto calc_lg(T x) noexcept { XY_calc_func_gen( lg, log10 ) }

    /// 以2为底的对数 log₂x
    template <typename T>
    auto calc_log2(T x) noexcept { XY_calc_func_gen( log2, log2 ) }

    /// 以 base 为底的对数 log_base(x)
    template <typename T, typename U>
    auto calc_log(U base, T x) noexcept { return calc_ln(x) / calc_ln(base); }

    /**
     * @brief ln(1 + x)
     * @details 当 x 接近 0 时，此函数的结果比 `calc_ln(1 + x)` 更精确
     */
    template <typename T>
    auto calc_ln_1p(T x) noexcept { XY_calc_func_gen( ln_1p, log1p ) }
}

/// 三角函数 
namespace xylu::xymath
{
    /// 正弦函数 sinx
    template <typename T>
    auto calc_sin(T x) noexcept { XY_calc_func_gen( sin, sin ) }

    /// 余弦函数 cosx
    template <typename T>
    auto calc_cos(T x) noexcept { XY_calc_func_gen( cos, cos ) }

    /// sinx + cosx
    template <typename T>
    auto calc_sincos(T x) noexcept
    {
        static_assert(xyu::t_is_number<T>);
        struct { T sin, cos; } ret;
        if constexpr (!xyu::t_is_floating<T>) return calc_sincos<xyu::t_wide<T,float>>(x);
        else if constexpr (xyu::t_is_same<T, float>)    return __builtin_sincosf(x, &ret.sin, &ret.cos), ret;
        else if constexpr (xyu::t_is_same<T, double>)   return __builtin_sincos (x, &ret.sin, &ret.cos), ret;
        else                                            return __builtin_sincosl(x, &ret.sin, &ret.cos), ret;
    }

    /// 正切函数 tanx
    template <typename T>
    auto calc_tan(T x) noexcept { XY_calc_func_gen( tan, tan ) }

    /// 反正弦函数 arcsinx
    template <typename T>
    auto calc_asin(T x) noexcept { XY_calc_func_gen( asin, asin ) }

    /// 反余弦函数 arccosx
    template <typename T>
    auto calc_acos(T x) noexcept { XY_calc_func_gen( acos, acos ) }

    /// 反正切函数 arctanx
    template <typename T>
    auto calc_atan(T x) noexcept { XY_calc_func_gen( atan, atan ) }

    /// 反正切函数 arctan(y/x)
    template <typename T, typename U> 
    auto calc_atan(T y, U x) noexcept { return [](auto x,auto y){XY_calc_func2_gen( atan, atan2 )}(y,x); }
}

/// 双曲函数 
namespace xylu::xymath
{
    /// 双曲正弦函数 sinh(x)
    template <typename T>
    auto calc_sinh(T x) noexcept { XY_calc_func_gen( sinh, sinh ) }

    /// 双曲余弦函数 cosh(x)
    template <typename T>
    auto calc_cosh(T x) noexcept { XY_calc_func_gen( cosh, cosh ) }

    /// 双曲正切函数 tanh(x)
    template <typename T>
    auto calc_tanh(T x) noexcept { XY_calc_func_gen( tanh, tanh ) }

    /// 双曲反正弦函数 arcsinh(x)
    template <typename T>
    auto calc_asinh(T x) noexcept { XY_calc_func_gen( asinh, asinh ) }

    /// 双曲反余弦函数 arccosh(x)
    template <typename T>
    auto calc_acosh(T x) noexcept { XY_calc_func_gen( acosh, acosh ) }

    /// 双曲反正切函数 arctanh(x)
    template <typename T>
    auto calc_atanh(T x) noexcept { XY_calc_func_gen( atanh, atanh ) }
}

/// 误差函数 
namespace xylu::xymath
{
    /// 误差函数 erf(x)
    template <typename T>
    auto calc_erf(T x) noexcept { XY_calc_func_gen( erf, erf ) }

    /// 补充误差函数 erfc(x) (即 1 - erf(x))
    template <typename T>
    auto calc_erfc(T x) noexcept { XY_calc_func_gen( erfc, erfc ) }
}

/// 伽马函数
namespace xylu::xymath
{
    /// 伽马函数 gamma(x)
    template <typename T>
    auto calc_gamma(T x) noexcept { XY_calc_func_gen( gamma, tgamma ) }

    /// 伽马函数的自然对数 lgamma(x) (即 ln(gamma(x)))
    template <typename T>
    auto calc_lgamma(T x) noexcept { XY_calc_func_gen( lgamma, lgamma ) }
}

#undef XY_calc_func_gen
#undef XY_calc_func2_gen
