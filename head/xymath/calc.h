#pragma clang diagnostic push
#pragma ide diagnostic ignored "OCUnusedGlobalDeclarationInspection"
#pragma once

#include "../../link/attr"
#include "../../link/traits"

/**
 * @file
 * @brief 提供了一组类型安全的、自动选择精度的数学计算函数。
 * @details
 *   本文件中的 `calc_*` 系列函数是对编译器内置数学函数（`__builtin_*`）的封装。
 *   它们的核心特性是 **自动精度选择**：
 *
 *   1. **自动类型提升**:
 *      - 当传入整数类型时，函数会根据整数的位数自动将其转换为一个能够无损或以最小精度损失
 *        表示它的浮点类型 (`float`, `double`, `long double`) 来进行计算。
 *      - 例如，`int16` (15位有效数字) 会提升为 `float` (24位)，`int32` (31位) 会提升为 `double` (53位)。
 *      - 最终的返回类型也是提升后的浮点类型。
 *
 *   2. **多参数类型取宽**:
 *      - 对于接受多个参数的函数（如 `calc_pow(x, y)`），如果参数类型不同，
 *        函数会使用 `t_wider` 来确定一个能同时容纳所有参数的“更宽”的类型， 然后以该类型进行计算。
 *
 *   3. **手动精度控制**:
 *      - 用户可以通过显式指定模板参数（如 `calc_sqrt˂double˃(1.0f)`）或
 *        对输入参数进行 `static_cast` 来强制使用特定的浮点精度。
 *
 *   这些设计旨在提供一个既易于使用又能在大多数情况下保持足够精度的数学函数库。
 */
 
namespace xylu::xymath::__
{
    constexpr auto Df = xyu::number_traits<double>::digits;
    constexpr auto Dd = xyu::number_traits<double>::digits;
}

/** 次方与开方函数 */
namespace xylu::xymath
{
    /**
     * @brief 计算 x 的 y 次幂 (x^y)。
     * @param x 底数。
     * @param y 指数。
     * @note 若 T 和 U 类型不同，会自动提升到更宽的数值类型进行计算。
     */
    template <typename T, typename U>
    XY_CONST constexpr auto calc_pow(T x, U y) noexcept
    {
        static_assert(xyu::t_is_number<T> && xyu::t_is_number<U>);
        constexpr auto Dt = xyu::number_traits<T>::digits;
        // 类型不同则使用更宽的类型
        using W = xyu::t_wider<T, U>;
        if constexpr (!xyu::t_is_same<T, U>) return calc_pow<W,W>(x, y);
        // 同类型处理
        else if constexpr (Dt <= __::Df) return __builtin_powf(x, y);
        else if constexpr (Dt <= __::Dd) return __builtin_pow(x, y);
        else                             return __builtin_powl(x, y);
    }

    /**
     * @brief 计算 x 的平方根 (√x)。
     * @param x 被开方数。
     * @note 若 x 为负数，结果为 NaN (Not-a-Number)。
     */
    template <typename T>
    XY_CONST constexpr auto calc_sqrt(T x) noexcept
    {
        static_assert(xyu::t_is_number<T>);
        constexpr auto Dt = xyu::number_traits<T>::digits;
        if constexpr      (Dt <= __::Df) return __builtin_sqrtf(x);
        else if constexpr (Dt <= __::Dd) return __builtin_sqrt(x);
        else                             return __builtin_sqrtl(x);
    }

    /**
     * @brief 计算 x 的立方根 (∛x)。
     * @param x 被开方数。
     */
    template <typename T>
    XY_CONST constexpr auto calc_cbrt(T x) noexcept
    {
        static_assert(xyu::t_is_number<T>);
        constexpr auto Dt = xyu::number_traits<T>::digits;
        if constexpr      (Dt <= __::Df) return __builtin_cbrtf(x);
        else if constexpr (Dt <= __::Dd) return __builtin_cbrt(x);
        else                             return __builtin_cbrtl(x);
    }

    /**
     * @brief 计算直角三角形的斜边长度 (sqrt(x*x + y*y))。
     * @param x 直角边 a 的长度。
     * @param y 直角边 b 的长度。
     * @note 此函数在计算上比直接实现更精确，且能更好地避免中间结果溢出。
     */
    template <typename T, typename U>
    XY_CONST constexpr auto calc_hypot(T x, U y) noexcept
    {
        static_assert(xyu::t_is_number<T> && xyu::t_is_number<U>);
        constexpr auto Dt = xyu::number_traits<T>::digits;
        using W = xyu::t_wider<T, U>;
        // 类型不同则使用更宽的类型
        if constexpr (!xyu::t_is_same<T, U>) return calc_hypot<W,W>(x, y);
        // 同类型处理
        else if constexpr (Dt <= __::Df) return __builtin_hypotf(x, y);
        else if constexpr (Dt <= __::Dd) return __builtin_hypot(x, y);
        else                             return __builtin_hypotl(x, y);
    }
}

/** 指数与对数函数 */
namespace xylu::xymath
{
    /// 计算自然指数 e^x。
    template <typename T>
    XY_CONST constexpr auto calc_exp(T x) noexcept
    {
        static_assert(xyu::t_is_number<T>);
        constexpr auto Dt = xyu::number_traits<T>::digits;
        if constexpr      (Dt <= __::Df) return __builtin_expf(x);
        else if constexpr (Dt <= __::Dd) return __builtin_exp(x);
        else                             return __builtin_expl(x);
    }

    /// 计算 2^x。
    template <typename T>
    XY_CONST constexpr auto calc_exp2(T x) noexcept
    {
        static_assert(xyu::t_is_number<T>);
        constexpr auto Dt = xyu::number_traits<T>::digits;
        if constexpr      (Dt <= __::Df) return __builtin_exp2f(x);
        else if constexpr (Dt <= __::Dd) return __builtin_exp2(x);
        else                             return __builtin_exp2l(x);
    }

    /**
     * @brief 计算 e^x - 1。
     * @note 当 x 接近 0 时，此函数的结果比 `calc_exp(x) - 1` 更精确。
     */
    template <typename T>
    XY_CONST constexpr auto calc_exp_m1(T x) noexcept
    {
        static_assert(xyu::t_is_number<T>);
        constexpr auto Dt = xyu::number_traits<T>::digits;
        if constexpr      (Dt <= __::Df) return __builtin_expm1f(x);
        else if constexpr (Dt <= __::Dd) return __builtin_expm1(x);
        else                             return __builtin_expm1l(x);
    }

    /**
     * @brief 计算 x 的自然对数 (ln x)。
     * @note 若 x 为 0，结果为负无穷大；若 x 为负数，结果为 NaN。
     */
    template <typename T>
    XY_CONST constexpr auto calc_ln(T x) noexcept
    {
        static_assert(xyu::t_is_number<T>);
        constexpr auto Dt = xyu::number_traits<T>::digits;
        if constexpr      (Dt <= __::Df) return __builtin_logf(x);
        else if constexpr (Dt <= __::Dd) return __builtin_log(x);
        else                             return __builtin_logl(x);
    }

    /// 计算 x 的常用对数 (以 10 为底, lg x)。
    template <typename T>
    XY_CONST constexpr auto calc_lg(T x) noexcept
    {
        static_assert(xyu::t_is_number<T>);
        constexpr auto Dt = xyu::number_traits<T>::digits;
        if constexpr      (Dt <= __::Df) return __builtin_log10f(x);
        else if constexpr (Dt <= __::Dd) return __builtin_log10(x);
        else                             return __builtin_log10l(x);
    }

    /// 计算 x 的二进制对数 (以 2 为底, log₂x)。
    template <typename T>
    XY_CONST constexpr auto calc_log2(T x) noexcept
    {
        static_assert(xyu::t_is_number<T>);
        constexpr auto Dt = xyu::number_traits<T>::digits;
        if constexpr      (Dt <= __::Df) return __builtin_log2f(x);
        else if constexpr (Dt <= __::Dd) return __builtin_log2(x);
        else                             return __builtin_log2l(x);
    }

    /**
     * @brief 计算以 `base` 为底 x 的对数 (log_base(x))。
     * @note 效率通常低于固定底数的对数函数。
     */
    template <typename T, typename U>
    XY_CONST constexpr auto calc_log(U base, T x) noexcept { return calc_ln(x) / calc_ln(base); }

    /**
     * @brief 计算 ln(1 + x)。
     * @note 当 x 接近 0 时，此函数的结果比 `calc_ln(1 + x)` 更精确。
     */
    template <typename T>
    XY_CONST constexpr auto calc_ln_1p(T x) noexcept
    {
        static_assert(xyu::t_is_number<T>);
        constexpr auto Dt = xyu::number_traits<T>::digits;
        if constexpr      (Dt <= __::Df) return __builtin_log1pf(x);
        else if constexpr (Dt <= __::Dd) return __builtin_log1p(x);
        else                             return __builtin_log1pl(x);
    }
}

/** 三角函数 */
namespace xylu::xymath
{
    /// 计算 弧度x 的正弦值 (sin x)。
    template <typename T>
    XY_CONST constexpr auto calc_sin(T x) noexcept
    {
        static_assert(xyu::t_is_number<T>);
        constexpr auto Dt = xyu::number_traits<T>::digits;
        if constexpr      (Dt <= __::Df) return __builtin_sinf(x);
        else if constexpr (Dt <= __::Dd) return __builtin_sin(x);
        else                             return __builtin_sinl(x);
    }

    /// 计算 弧度x 的余弦值 (cos x)。
    template <typename T>
    XY_CONST constexpr auto calc_cos(T x) noexcept
    {
        static_assert(xyu::t_is_number<T>);
        constexpr auto Dt = xyu::number_traits<T>::digits;
        if constexpr      (Dt <= __::Df) return __builtin_cosf(x);
        else if constexpr (Dt <= __::Dd) return __builtin_cos(x);
        else                             return __builtin_cosl(x);
    }

    /// 计算 弧度x 的正切值 (tan x)。
    template <typename T>
    XY_CONST constexpr auto calc_tan(T x) noexcept
    {
        static_assert(xyu::t_is_number<T>);
        constexpr auto Dt = xyu::number_traits<T>::digits;
        if constexpr      (Dt <= __::Df) return __builtin_tanf(x);
        else if constexpr (Dt <= __::Dd) return __builtin_tan(x);
        else                             return __builtin_tanl(x);
    }

    /// 计算 x 的反正弦值 (arcsin x)。
    template <typename T>
    XY_CONST constexpr auto calc_asin(T x) noexcept
    {
        static_assert(xyu::t_is_number<T>);
        constexpr auto Dt = xyu::number_traits<T>::digits;
        if constexpr      (Dt <= __::Df) return __builtin_asinf(x);
        else if constexpr (Dt <= __::Dd) return __builtin_asin(x);
        else                             return __builtin_asinl(x);
    }

    /// 计算 x 的反余弦值 (arccos x)。
    template <typename T>
    XY_CONST constexpr auto calc_acos(T x) noexcept
    {
        static_assert(xyu::t_is_number<T>);
        constexpr auto Dt = xyu::number_traits<T>::digits;
        if constexpr      (Dt <= __::Df) return __builtin_acosf(x);
        else if constexpr (Dt <= __::Dd) return __builtin_acos(x);
        else                             return __builtin_acosl(x);
    }

    /// 计算 x 的反正切值 (arctan x)。
    template <typename T>
    XY_CONST constexpr auto calc_atan(T x) noexcept
    {
        static_assert(xyu::t_is_number<T>);
        constexpr auto Dt = xyu::number_traits<T>::digits;
        if constexpr      (Dt <= __::Df) return __builtin_atanf(x);
        else if constexpr (Dt <= __::Dd) return __builtin_atan(x);
        else                             return __builtin_atanl(x);
    }

    /// 计算 y/x 的反正切值 (arctan(y/x))。
    template <typename T, typename U>
    XY_CONST constexpr auto calc_atan(T y, U x) noexcept
    {
        static_assert(xyu::t_is_number<T> && xyu::t_is_number<U>);
        constexpr auto Dt = xyu::number_traits<T>::digits;
        // 类型不同则使用更宽的类型
        using W = xyu::t_wider<T, U>;
        if constexpr (!xyu::t_is_same<T, U>) return calc_atan<W,W>(y, x);
        // 同类型处理
        else if constexpr (Dt <= __::Df) return __builtin_atan2f(y, x);
        else if constexpr (Dt <= __::Dd) return __builtin_atan2(y, x);
        else                             return __builtin_atan2l(y, x);
    }
}

/** 双曲函数 */
namespace xylu::xymath
{
    /// 计算 sinh(x) (双曲正弦)
    template <typename T>
    XY_CONST constexpr auto calc_sinh(T x) noexcept
    {
        static_assert(xyu::t_is_number<T>);
        constexpr auto Dt = xyu::number_traits<T>::digits;
        if constexpr      (Dt <= __::Df) return __builtin_sinhf(x);
        else if constexpr (Dt <= __::Dd) return __builtin_sinh(x);
        else                             return __builtin_sinhl(x);
    }

    /// 计算 cosh(x) (双曲余弦)
    template <typename T>
    XY_CONST constexpr auto calc_cosh(T x) noexcept
    {
        static_assert(xyu::t_is_number<T>);
        constexpr auto Dt = xyu::number_traits<T>::digits;
        if constexpr      (Dt <= __::Df) return __builtin_coshf(x);
        else if constexpr (Dt <= __::Dd) return __builtin_cosh(x);
        else                             return __builtin_coshl(x);
    }

    /// 计算 tanh(x) (双曲正切)
    template <typename T>
    XY_CONST constexpr auto calc_tanh(T x) noexcept
    {
        static_assert(xyu::t_is_number<T>);
        constexpr auto Dt = xyu::number_traits<T>::digits;
        if constexpr      (Dt <= __::Df) return __builtin_tanhf(x);
        else if constexpr (Dt <= __::Dd) return __builtin_tanh(x);
        else                             return __builtin_tanhl(x);
    }

    /// 计算 arcsinh(x) (双曲反正弦)
    template <typename T>
    XY_CONST constexpr auto calc_asinh(T x) noexcept
    {
        static_assert(xyu::t_is_number<T>);
        constexpr auto Dt = xyu::number_traits<T>::digits;
        if constexpr      (Dt <= __::Df) return __builtin_asinhf(x);
        else if constexpr (Dt <= __::Dd) return __builtin_asinh(x);
        else                             return __builtin_asinhl(x);
    }

    /// 计算 arccosh(x) (双曲反余弦)
    template <typename T>
    XY_CONST constexpr auto calc_acosh(T x) noexcept
    {
        static_assert(xyu::t_is_number<T>);
        constexpr auto Dt = xyu::number_traits<T>::digits;
        if constexpr      (Dt <= __::Df) return __builtin_acoshf(x);
        else if constexpr (Dt <= __::Dd) return __builtin_acosh(x);
        else                             return __builtin_acoshl(x);
    }

    /// 计算 arctanh(x) (双曲反正切)
    template <typename T>
    XY_CONST constexpr auto calc_atanh(T x) noexcept
    {
        static_assert(xyu::t_is_number<T>);
        constexpr auto Dt = xyu::number_traits<T>::digits;
        if constexpr      (Dt <= __::Df) return __builtin_atanhf(x);
        else if constexpr (Dt <= __::Dd) return __builtin_atanh(x);
        else                             return __builtin_atanhl(x);
    }
}

/** 误差函数 */
namespace xylu::xymath
{
    /// 计算 erf(x) (误差函数)
    template <typename T>
    XY_CONST constexpr auto calc_erf(T x) noexcept
    {
        static_assert(xyu::t_is_number<T>);
        constexpr auto Dt = xyu::number_traits<T>::digits;
        if constexpr      (Dt <= __::Df) return __builtin_erff(x);
        else if constexpr (Dt <= __::Dd) return __builtin_erf(x);
        else                             return __builtin_erfl(x);
    }

    /// 计算 erfc(x) (补充误差函数，即 1 - erf(x))
    template <typename T>
    XY_CONST constexpr auto calc_erfc(T x) noexcept
    {
        static_assert(xyu::t_is_number<T>);
        constexpr auto Dt = xyu::number_traits<T>::digits;
        if constexpr      (Dt <= __::Df) return __builtin_erfcf(x);
        else if constexpr (Dt <= __::Dd) return __builtin_erfc(x);
        else                             return __builtin_erfcl(x);
    }
}

/** 伽马函数 */
namespace xylu::xymath
{
    /// 计算 gamma(x) (伽马函数)
    template <typename T>
    XY_CONST constexpr auto calc_gamma(T x) noexcept
    {
        static_assert(xyu::t_is_number<T>);
        constexpr auto Dt = xyu::number_traits<T>::digits;
        if constexpr      (Dt <= __::Df) return __builtin_tgammaf(x);
        else if constexpr (Dt <= __::Dd) return __builtin_tgamma(x);
        else                             return __builtin_tgammal(x);
    }

    /// 计算 lgamma(x) (伽马函数的自然对数，即 ln(gamma(x)))
    template <typename T>
    XY_CONST constexpr auto calc_gamma_ln(T x) noexcept
    {
        static_assert(xyu::t_is_number<T>);
        constexpr auto Dt = xyu::number_traits<T>::digits;
        if constexpr      (Dt <= __::Df) return __builtin_lgammaf(x);
        else if constexpr (Dt <= __::Dd) return __builtin_lgamma(x);
        else                             return __builtin_lgammal(x);
    }
}

/** 数值状态 */
namespace xylu::xymath
{
    /// 检查一个浮点数是否为 NaN (Not-a-Number)。对整数总是返回 false。
    template <typename T>
    XY_CONST constexpr bool calc_is_nan(T v) noexcept
    {
        static_assert(xyu::t_is_number<T>);
        if constexpr (!xyu::t_is_floating<T>) return false;
        else return __builtin_isnan(v);
    }

    /// 检查一个浮点数是否为无穷大 (infinity)。对整数总是返回 false。
    template <typename T>
    XY_CONST constexpr bool calc_is_inf(T v) noexcept
    {
        static_assert(xyu::t_is_number<T>);
        if constexpr (!xyu::t_is_floating<T>) return false;
        else return __builtin_isinf(v);
    }

    /// 检查一个浮点数是否为有限值 (既非 NaN 也非无穷大)。对整数总是返回 true。
    template <typename T>
    XY_CONST constexpr bool calc_is_finite(T v) noexcept
    {
        static_assert(xyu::t_is_number<T>);
        if constexpr (!xyu::t_is_floating<T>) return true;
        else return __builtin_isfinite(v);
    }

    /// 检查一个浮点数是否为规格化数 (normalized)。
    template <typename T>
    XY_CONST constexpr bool calc_is_normalized(T v) noexcept
    {
        static_assert(xyu::t_is_number<T>);
        if constexpr (!xyu::t_is_floating<T>) return false;
        else return __builtin_isnormal(v);
    }
}

/** 数值分解 */
namespace xylu::xymath
{
    /**
     * @brief 将浮点数 x 分解为 m * 2^exp，并返回整数指数 exp。
     * @details m 是 x 的有效数（significand），满足 1 ≤ |m| < 2。
     */
    template <typename T>
    XY_CONST constexpr auto calc_get_exp(T x) noexcept
    {
        static_assert(xyu::t_is_number<T>);
        constexpr auto Dt = xyu::number_traits<T>::digits;
        if constexpr      (Dt <= __::Df) return __builtin_ilogbf(x);
        else if constexpr (Dt <= __::Dd) return __builtin_ilogb(x);
        else                             return __builtin_ilogbl(x);
    }

    /**
     * @brief 将浮点数 x 分解为尾数 m 和指数 exp。
     * @param x   要分解的浮点数。
     * @param exp (输出) 用于存储指数 exp 的指针。
     * @return 返回尾数 m。
     * @note 满足 x = m * 2^exp，且 0.5 ≤ |m| < 1。
     */
    template <typename T, typename U>
    XY_CONST constexpr auto calc_resolve(T x, U* e2p = nullptr) noexcept
    {
        static_assert(xyu::t_is_number<T> && xyu::t_is_number<U>);
        constexpr auto Dt = xyu::number_traits<T>::digits;

        int tmp{};
        auto inner = [e2p, &tmp](auto y) noexcept -> decltype(y)
        { if (e2p) *e2p = tmp-1; return y*2; };

        if constexpr      (Dt <= __::Df) return inner(__builtin_frexpf(x, &tmp));
        else if constexpr (Dt <= __::Dd) return inner(__builtin_frexp(x, &tmp));
        else                             return inner(__builtin_frexpl(x, &tmp));
    }

    /**
     * @brief 将浮点数 x 分解为整数部分 i 和小数部分 f。
     * @param x  要分解的浮点数。
     * @param ip (输出) 用于存储整数部分 i 的指针。
     * @return 返回小数部分 f。
     * @note 满足 x = i + f，且 i 和 f 的符号与 x 相同。
     */
    template <typename T, typename U>
    XY_CONST constexpr auto calc_extract(T x, U* ip = nullptr) noexcept
    {
        static_assert(xyu::t_is_number<T> && xyu::t_is_number<U>);
        constexpr auto Dt = xyu::number_traits<T>::digits;
        using F = xyu::t_cond<Dt <= __::Df, float,
                xyu::t_cond<Dt <= __::Dd, double, long double>>;
        F tmp;
        auto inner = [ip, &tmp](F y) noexcept { if (ip) *ip = tmp; return y; };
        if constexpr      (Dt <= __::Df) return inner(__builtin_modff(x, &tmp));
        else if constexpr (Dt <= __::Dd) return inner(__builtin_modf(x, &tmp));
        else                             return inner(__builtin_modfl(x, &tmp));
    }
}

/** 常用计算 */
namespace xylu::xymath
{
    /// 计算 x 的绝对值。
    template <typename T>
    XY_CONST constexpr auto calc_abs(T x) noexcept
    {
        static_assert(xyu::t_is_number<T>);
        if (xyu::t_constant()) { return x < 0 ? -x : x; }
        if constexpr      (xyu::t_is_unsigned<T>)           return x;
        else if constexpr (xyu::t_is_same<T, int>)          return __builtin_abs(x);
        else if constexpr (xyu::t_is_same<T, long>)         return __builtin_labs(x);
        else if constexpr (xyu::t_is_same<T, long long>)    return __builtin_llabs(x);
        else if constexpr (xyu::t_is_same<T, float>)        return __builtin_fabsf(x);
        else if constexpr (xyu::t_is_same<T, double>)       return __builtin_fabs(x);
        else if constexpr (xyu::t_is_same<T, long double>)  return __builtin_fabsl(x);
        else return x < 0 ? -x : x;
    }

    /// 获取 x 和 y 中的较小值。
    template <typename T, typename U>
    XY_CONST constexpr auto calc_min(T x, U y) noexcept
    {
        static_assert(xyu::t_is_number<T> && xyu::t_is_number<U>);
        // 类型不同则使用更宽的类型
        using W = xyu::t_wider<T, U>;
        if constexpr (!xyu::t_is_same<T, U>) return calc_min<W,W>(x, y);
            // 相同类型
        else if constexpr (xyu::t_is_same<T, float>)        return __builtin_fminf(x, y);
        else if constexpr (xyu::t_is_same<T, double>)       return __builtin_fmin(x, y);
        else if constexpr (xyu::t_is_same<T, long double>)  return __builtin_fminl(x, y);
        else return x <= y ? x : y;
    }

    /// 获取参数列表中的最小值。
    template <typename T, typename U, typename... Args>
    XY_CONST constexpr auto min(const T& x, const U& y, const Args&... args) noexcept
    {
        if constexpr (sizeof...(args) == 0) return calc_min(x, y);
        else return min(calc_min(x, y), args...);
    }

    /// 获取 x 和 y 中的较大值。
    template <typename T, typename U>
    XY_CONST constexpr auto calc_max(T x, U y) noexcept
    {
        static_assert(xyu::t_is_number<T> && xyu::t_is_number<U>);
        // 类型不同则使用更宽的类型
        using W = xyu::t_wider<T, U>;
        if constexpr (!xyu::t_is_same<T, U>) return calc_max<W,W>(x, y);
            // 相同类型
        else if constexpr (xyu::t_is_same<T, float>)        return __builtin_fmaxf(x, y);
        else if constexpr (xyu::t_is_same<T, double>)       return __builtin_fmax(x, y);
        else if constexpr (xyu::t_is_same<T, long double>)  return __builtin_fmaxl(x, y);
        else return x >= y ? x : y;
    }

    /// 获取参数列表中的最大值。
    template <typename T, typename U, typename... Args>
    XY_CONST constexpr auto max(const T& x, const U& y, const Args&... args) noexcept
    {
        if constexpr (xyu::t_is_number<T> && xyu::t_is_number<U>) {
            if constexpr (sizeof...(args) == 0) return calc_max(x, y);
            else return max(calc_max(x, y), args...);
        }
        else if constexpr (sizeof...(args) == 0) return x >= y ? x : y;
        else return max(x >= y ? x : y, args...);
    }
}

/** 舍入函数 */
namespace xylu::xymath
{
    /**
     * @brief 四舍五入到最近的整数。
     * @example calc_round(1.5) -> 2.0, calc_round(-1.5) -> -2.0
     */
    template <typename T>
    XY_CONST constexpr auto calc_round(T x) noexcept
    {
        static_assert(xyu::t_is_number<T>);
        using Nt = xyu::number_traits<T>;
        if constexpr      (!Nt::is_floating)     return x;
        else if constexpr (Nt::digits <= __::Df) return __builtin_roundf(x);
        else if constexpr (Nt::digits <= __::Dd) return __builtin_round(x);
        else                                     return __builtin_roundl(x);
    }

    /**
     * @brief 向零取整（截断小数部分）。
     * @example calc_trunc(1.9) -> 1.0, calc_trunc(-1.9) -> -1.0
     */
    template <typename T>
    XY_CONST constexpr auto calc_trunc(T x) noexcept
    {
        static_assert(xyu::t_is_number<T>);
        using Nt = xyu::number_traits<T>;
        if constexpr      (!Nt::is_floating)     return x;
        else if constexpr (Nt::digits <= __::Df) return __builtin_truncf(x);
        else if constexpr (Nt::digits <= __::Dd) return __builtin_trunc(x);
        else                                     return __builtin_truncl(x);
    }

    /**
     * @brief 向上取整（取不小于 x 的最小整数）。
     * @example calc_ceil(1.1) -> 2.0, calc_ceil(-1.9) -> -1.0
     */
    template <typename T>
    XY_CONST constexpr auto calc_ceil(T x) noexcept
    {
        static_assert(xyu::t_is_number<T>);
        using Nt = xyu::number_traits<T>;
        if constexpr      (!Nt::is_floating)     return x;
        else if constexpr (Nt::digits <= __::Df) return __builtin_ceilf(x);
        else if constexpr (Nt::digits <= __::Dd) return __builtin_ceil(x);
        else                                     return __builtin_ceill(x);
    }

    /**
     * @brief 向下取整（取不大于 x 的最大整数）。
     * @example calc_floor(1.9) -> 1.0, calc_floor(-1.1) -> -2.0
     */
    template <typename T>
    XY_CONST constexpr auto calc_floor(T x) noexcept
    {
        static_assert(xyu::t_is_number<T>);
        using Nt = xyu::number_traits<T>;
        if constexpr      (!Nt::is_floating)     return x;
        else if constexpr (Nt::digits <= __::Df) return __builtin_floorf(x);
        else if constexpr (Nt::digits <= __::Dd) return __builtin_floor(x);
        else                                     return __builtin_floorl(x);
    }
}

/** 其他运算 */
namespace xylu::xymath
{
    /**
     * @brief 计算浮点余数 (x % y)。
     * @details 结果的符号与 x 相同。
     */
    template <typename T, typename U>
    XY_CONST constexpr auto calc_mod(T x, U y) noexcept
    {
        static_assert(xyu::t_is_number<T> && xyu::t_is_number<U>);
        constexpr auto Dt = xyu::number_traits<T>::digits;
        // 类型不同则使用更宽的类型
        using W = xyu::t_wider<T, U>;
        if constexpr (!xyu::t_is_same<T, U>) return calc_mod<W,W>(x, y);
        // 同类型处理
        else if constexpr (Dt <= __::Df) return __builtin_fmodf(x, y);
        else if constexpr (Dt <= __::Dd) return __builtin_fmod(x, y);
        else                             return __builtin_fmodl(x, y);
    }

    /**
     * @brief 计算 IEEE 754 标准的浮点余数。
     * @details 结果是 (x - n*y)，其中 n 是 x/y 四舍五入后的整数。
     */
    template <typename T, typename U>
    XY_CONST constexpr auto calc_rem(T x, U y) noexcept
    {
        static_assert(xyu::t_is_number<T> && xyu::t_is_number<U>);
        constexpr auto Dt = xyu::number_traits<T>::digits;
        // 类型不同则使用更宽的类型
        using W = xyu::t_wider<T, U>;
        if constexpr (!xyu::t_is_same<T, U>) return calc_rem<W,W>(x, y);
        // 同类型处理
        else if constexpr (Dt <= __::Df) return __builtin_remainderf(x, y);
        else if constexpr (Dt <= __::Dd) return __builtin_remainder(x, y);
        else                             return __builtin_remainderl(x, y);
    }

    /**
     * @brief 计算融合乘加 (x * y + z)。
     * @details 在支持的硬件上，此操作通常作为单条指令执行，可以提供更高的精度和性能。
     */
    template <typename T, typename U, typename V>
    XY_CONST constexpr auto calc_mul_add(T x, U y, V z) noexcept
    {
        static_assert(xyu::t_is_number<T> && xyu::t_is_number<U> && xyu::t_is_number<V>);
        constexpr auto Dt = xyu::number_traits<T>::digits;
        // 类型不同则使用更宽的类型
        using W = xyu::t_wider<T,U,V>;
        if constexpr (!xyu::t_is_same<T,U,V>) return calc_mul_add<W,W,W>(x, y, z);
        // 同类型处理
        else if constexpr (Dt <= __::Df) return __builtin_fmaf(x, y, z);
        else if constexpr (Dt <= __::Dd) return __builtin_fma(x, y, z);
        else                             return __builtin_fmal(x, y, z);
    }

    /**
     * @brief 计算 (x * 2^exp)。
     * @details 这是一个高效的、通过直接操作浮点数指数部分来计算乘以2的幂的方法。
     */
    template <typename T, typename U>
    XY_CONST constexpr auto calc_mul_exp2(T x, U e2) noexcept
    {
        static_assert(xyu::t_is_number<T> && xyu::t_is_number<U>);
        constexpr auto Dt = xyu::number_traits<T>::digits;

        int e2i = e2;
        if constexpr      (Dt <= __::Df) return __builtin_ldexpf(x, e2i);
        else if constexpr (Dt <= __::Dd) return __builtin_ldexp(x, e2i);
        else                             return __builtin_ldexpl(x, e2i);
    }
}

#pragma clang diagnostic pop