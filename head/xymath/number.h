#pragma clang diagnostic push
#pragma ide diagnostic ignored "OCUnusedGlobalDeclarationInspection"
#pragma once

#include "../../link/bitfun"
#include "../../link/calcfun"

/* 初等数论相关的函数 */

namespace xylu::xymath
{

    /**
     * @brief 计算两个整数 a 和 b 的最大公约数 (Greatest Common Divisor, GCD)。
     * @details
     *   此函数采用二进制 GCD 算法（Stein 算法）实现，该算法通过移位和减法代替
     *   了传统欧几里得算法中的取模运算，在许多平台上效率更高。
     *
     * @param a 第一个整数。
     * @param b 第二个整数。
     * @return 返回 a 和 b 的最大公约数。
     *
     * @note
     *   - 若 a 或 b 中有一个为 0，则返回另一个非零值。若都为 0，返回 0。
     *   - 输入的 a 和 b 会被视为它们的绝对值进行计算，结果总是非负数。
     *   - 若 T 和 U 类型不同，会自动提升到更宽的整数类型进行计算。
     */
    template <typename T, typename U>
    XY_CONST constexpr auto calc_gcd(T a, U b) noexcept
    {
        static_assert(xyu::t_is_mathint<T> && xyu::t_is_mathint<U>);
        // 类型不同则使用更宽的类型
        using W = xyu::t_wider<T, U>;
        using uT = xyu::t_change_sign2un<T>;
        if constexpr (!xyu::t_is_same<T, U>) return calc_gcd<W,W>(a, b);
        // 有符号转为无符号
        else if constexpr (xyu::t_is_signed<T>)
        {
            // 处理负数
            if (a < 0) a = -a;
            if (b < 0) b = -b;
            return calc_gcd<uT,uT>(a, b);
        }
        // 无符号同类型
        else
        {
            // 处理 0
            if (a == 0) return b;
            if (b == 0) return a;
            // 消去并获取2的最大幂
            const T i = bit_count_0_back(a);  a >>= i;
            const T j = bit_count_0_back(b);  b >>= j;
            const T k = min(i, j);
            // 辗转相减
            for (;;)
            {
                if (a < b) { T t = a; a = b; b = t; }
                a -= b;
                if (a == 0) return (T)(b << k);
                a >>= bit_count_0_back(a);
            }
        }
    }

    /**
     * @brief 计算两个整数 a 和 b 的最小公倍数 (Least Common Multiple, LCM)。
     * @details
     *   通过公式 lcm(a, b) = (|a * b|) / gcd(a, b) 进行计算。
     *
     * @param a 第一个整数。
     * @param b 第二个整数。
     * @return 返回 a 和 b 的最小公倍数。
     *
     * @note
     *   - 若 a 或 b 中有一个为 0，则返回 0。
     *   - 输入的 a 和 b 会被视为它们的绝对值进行计算，结果总是非负数。
     */
    template <typename T, typename U>
    XY_CONST constexpr auto calc_lcm(T a, U b) noexcept
    {
        if (a == 0 || b == 0) return decltype((calc_abs(a) / calc_gcd(a, b)) * calc_abs(b))(0);
        return (calc_abs(a) / calc_gcd(a, b)) * calc_abs(b);
    }

    /**
     * @brief 计算整数 a 在模 m 意义下的乘法逆元。
     * @details
     *   此函数使用扩展欧几里得算法来寻找一个整数 x，使得 (a * x) % m == 1。
     *
     * @param a 要求逆元的整数。
     * @param m 模数。
     * @return
     *   - 如果逆元存在（即 a 和 m 互质），则返回 [1, m-1] 范围内的最小正逆元。
     *   - 如果逆元不存在，则返回 0。
     *
     * @note 模数 m 会被视为其绝对值。
     */
    template <typename T, typename U>
    XY_CONST constexpr auto calc_mod_inverse(T a, U m) noexcept
    {
        static_assert(xyu::t_is_mathint<T> && xyu::t_is_mathint<U>);
        // 类型不同则使用更宽的类型
        using W = xyu::t_wider<T, U>;
        if constexpr (!xyu::t_is_same<T, U>) return calc_mod_inverse<W,W>(a, m);
        else
        {
            // 处理0
            if (a == 0 || m == 0) return (T)(0);
            // 处理负数
            if (m < 0) m = -m;
            // 循环迭代
            T x = 1, y = 0, x0 = 0, y0 = 1, q{}, t{}, m0 = m;
            do {
                q = a / m0;
                t = x;
                x = x0;
                x0 = t - q * x0;
                t = y;
                y = y0;
                y0 = t - q * y0;
                t = a;
                a = m0;
                m0 = t % m0;
            } while (m0 != 0);
            // 逆元不存在 (gcd=1)
            if (a != 1) return T{0};
            // 存在逆元
            return x < 0 ? x % m + m : x % m;
        }
    }
}

#pragma clang diagnostic pop