#pragma once

/**
 * @file    xymath/bit.h
 * @brief   底层位运算工具库：基于编译器内置指令的高效位计算实现
 * @date    2025-12-25
 */

#include "./_.h"
#include "../xycore/error.h"
#include "../xycore/config.h"
#include "../xytraits/numeric.h"

namespace xylu::xymath
{
    namespace __
    {
        using uint = unsigned int;
        using ulong = unsigned long;
        using ullong = unsigned long long;
        constexpr auto Su = sizeof(uint);
        constexpr auto Sul = sizeof(ulong);
        constexpr auto Sull = sizeof(ullong);
        constexpr auto Mull = xyu::t_number<ullong>::max;
    }

    /// 计算整数的二进制表示中 1 的总数（population count）
    template <typename T>
    constexpr xyu::uint bit_count_1_total(T v) noexcept
    {
        static_assert(xyu::t_is_integral<T>);

        xyu::t_chg_sign2un<T> u = v;
        constexpr auto St = xyu::t_number<T>::size;
        
        if constexpr (xyu::t_is_bool<T>)       return v == true;
        else if constexpr (St <= __::Su)       return __builtin_popcount(u);
        else if constexpr (St <= __::Sul)      return __builtin_popcountl(u);
        else if constexpr (St <= __::Sull)     return __builtin_popcountll(u);
        else if constexpr (St <= __::Sull*2)   return __builtin_popcountll(u & __::Mull)
                                                      + __builtin_popcountll(u >> __::Sull*8);
        else {
            int count = 0;
            while (u) count += __builtin_popcountll(u & __::Mull), u >>= __::Sull*8;
            return count;
        }
    }

    /// 计算整数的二进制表示中 0 的总数
    template <typename T>
    constexpr xyu::uint bit_count_0_total(T v) noexcept 
    {
        if constexpr (xyu::t_is_bool<T>) return v == false;
        else return sizeof(T)*8 - bit_count_1_total(v); 
    }

    /**
     * @brief 计算整数的二进制表示中前导 0 的数量（clz）
     * @details 负数前导零的数量总是 0
     * @note 若 v 为 0，则为该类型的总位数
     */
    template <typename T>
    constexpr xyu::uint bit_count_0_high(T v) noexcept
    {
        static_assert(xyu::t_is_integer<T>);

        xyu::t_chg_sign2un<T> u = v;
        constexpr auto St = xyu::t_number<T>::size;

        if (XY_UNLIKELY(v == 0)) return St * 8;
        if constexpr (St <= __::Su)          return __builtin_clz(u)   - 8*(__::Su-St);
        else if constexpr (St <= __::Sul)    return __builtin_clzl(u)  - 8*(__::Sul-St);
        else if constexpr (St <= __::Sull)   return __builtin_clzll(u) - 8*(__::Sull-St);
        else {
            //前置处理
            int last = St & (__::Sull - 1);
            int parts = St / __::Sull + (last > 0);
            //分组处理
            int count = 0;
            //-第一块 (考虑填充)
            auto pu = u >> --parts * 8*__::Sull;
            count += (pu == 0 ? 8*__::Sull : __builtin_clzll(pu)) - (last ? 8*int(__::Sull-last) : 0);
            //-后续块
            while (parts > 0 && pu == 0) {
                pu = (u >> --parts * 8*__::Sull) & __::Mull;
                count += (pu == 0 ? 8*__::Sull : __builtin_clzll(pu));
            }
            return count;
        }
    }

    /**
     * @brief 计算整数二进制表示中前导 1 的数量
     * @details 正数前导一的数量总是 0
     * @note 若 v 为 -1，则为该类型的总位数
     */
    template <typename T>
    constexpr xyu::uint bit_count_1_high(T v) noexcept { return bit_count_0_high(T(~v)); }

    /**
     * @brief 计算整数二进制表示中尾随 0 的数量（ctz）
     * @note 若 v 为 0，则返回 0
     */
    template <typename T>
    constexpr xyu::uint bit_count_0_low(T v) noexcept
    {
        static_assert(xyu::t_is_integer<T>);

        xyu::t_chg_sign2un<T> u = v;
        constexpr auto St = xyu::t_number<T>::size;

        if (XY_UNLIKELY(v == 0)) return 0;
        else if constexpr (St <= __::Su)     return __builtin_ctz(u);
        else if constexpr (St <= __::Sul)    return __builtin_ctzl(u);
        else if constexpr (St <= __::Sull)   return __builtin_ctzll(u);
        else {
            //分组处理
            int count = 0, SS = 8*__::Sull;
            do {
                auto tmp = u & __::Mull;
                if (tmp == 0) count += SS;
                else { count += __builtin_ctzll(tmp); break; }
                u >>= SS;
            } while (u > 0);
            return count;
        }
    }

    /**
     * @brief 计算整数二进制表示中尾随 1 的数量
     * @note 若 v 为 -1，则返回 0
     */
    template <typename T>
    constexpr xyu::uint bit_count_1_low(T v) noexcept { return bit_count_0_low(T(~v)); }

    /// 计算表示一个数值所需要的最小二进制位数
    template <typename T>
    constexpr xyu::uint bit_count_effect(T v) noexcept
    {
        static_assert(xyu::t_is_integer<T>);
        using vt = xyu::t_number<T>;
        constexpr auto St = vt::size;

        if (v == 0) return 1;
        if constexpr (!vt::is_signed) return 8*St - bit_count_0_high(v);
        else return v >= 0 ? 8*St - bit_count_0_high(v) :
                             8*St - bit_count_1_high(v) + 1;
    }

    /**
     * @brief 获取不大于 v 的 最大的 2的幂次，最小为 0
     */
    template <typename T>
    constexpr T bit_floor(T v) noexcept
    {
        using vt = xyu::t_number<T>;
        if (v <= 0) return 0;
        return (T)1 << (vt::size*8 - bit_count_0_high(v) - 1);
    }

    /**
     * @brief 获取不小于 v 的 最小的 2的幂次，最小为 1
     * @attention 结果溢出 ub
     */
    template <typename T>
    constexpr T bit_ceil(T v) noexcept
    {
        static_assert(xyu::t_is_integer<T>);
        using vt = xyu::t_number<T>;
        constexpr auto limit = bit_floor(vt::max);
        if constexpr (!XY_RELEASE) if (XY_UNLIKELY(v > limit))
            xyu::E_Logic::abort_over_limit(XY_CurPos(), v, 0, limit);
        if (v <= 1) return 1;
        //防止整型提升 ((T)防止short等小类型被整型提升为int)
        return (T)1 << (vt::size*8 - bit_count_0_high(T(v-1)));
    }

    /**
     * @brief 检查整数的二进制表示中 1 的个数的奇偶性（奇偶校验）
     * @details 若 1 的个数为奇数，返回 true；若为偶数，返回 false
     */
    template <typename T>
    constexpr bool bit_parity(T v) noexcept
    {
        static_assert(xyu::t_is_integer<T>);

        xyu::t_chg_sign2un<T> u = v;
        constexpr auto St = xyu::t_number<T>::size;

        if constexpr (St <= __::Su)            return __builtin_parity(u);
        else if constexpr (St <= __::Sul)      return __builtin_parityl(u);
        else if constexpr (St <= __::Sull)     return __builtin_parityll(u);
        else if constexpr (St <= __::Sull*2)   return __builtin_parityll(u & __::Mull)
                                                      ^ __builtin_parityll(u >> __::Sull*8);
        else {
            bool parity = false;
            while (u) parity ^= __builtin_popcountll(u & __::Mull), u >>= __::Sull*8;
            return parity;
        }
    }

    /**
     * @brief 交换整数的字节序
     * @code bit_swap(0x12345678) == 0x78563412
     */
    template <typename T>
    constexpr T bit_swap(T v) noexcept
    {
        static_assert(xyu::t_is_integer<T>);

        using vt = xyu::t_number<T>;
        constexpr auto St = vt::size;

        if      constexpr (St <= 1) return v;
        else if constexpr (St == 2) return __builtin_bswap16(v);
        else if constexpr (St == 4) return __builtin_bswap32(v);
        else if constexpr (St == 8) return __builtin_bswap64(v);
        else if constexpr (vt::is_signed) return T(bit_swap<xyu::t_chg_sign2un<T>>(v));
        else {
            T result = 0;
            for (xyu::uint i = 0; i < St; i++) result |= (((v >> (i*8)) & 0xff) << ((St-1-i)*8));
            return result;
        }
    }

    /**
     * @brief 将整数 v 循环左移 bits 位
     * @code bit_rotate_left(0x12345678, 4) == 0x23456781
     */
    template <typename T>
    constexpr T bit_rotate_left(T v, xyu::uint bits) noexcept
    {
        static_assert(xyu::t_is_integer<T>);

        using vt = xyu::t_number<T>;
        constexpr auto St = vt::size;

        if      constexpr (St <= 1) return v;
        else if constexpr (St == 2) return __builtin_rotateleft16(v, bits);
        else if constexpr (St == 4) return __builtin_rotateleft32(v, bits);
        else if constexpr (St == 8) return __builtin_rotateleft64(v, bits);
        else if constexpr (vt::is_signed)
             return T(bit_rotate_left<xyu::t_chg_sign2un<T>>(v, bits));
        else return (v << (bits & (St*8 - 1))) | (v >> ((-bits) & (St*8 - 1)));
    }

    /**
     * @brief 将整数 v 循环右移 bits 位
     * @code bit_rotate_right(0x12345678, 4) == 0x81234567
     */
    template <typename T>
    constexpr T bit_rotate_right(T v, xyu::uint bits) noexcept
    {
        static_assert(xyu::t_is_integer<T>);

        using vt = xyu::t_number<T>;
        constexpr auto St = vt::size;

        if      constexpr (St <= 1) return v;
        else if constexpr (St == 2) return __builtin_rotateright16(v, bits);
        else if constexpr (St == 4) return __builtin_rotateright32(v, bits);
        else if constexpr (St == 8) return __builtin_rotateright64(v, bits);
        else if constexpr (vt::is_signed)
             return T(bit_rotate_right<xyu::t_chg_sign2un<T>>(v, bits));
        else return (v >> (bits & (St*8 - 1))) | (v << ((-bits) & (St*8 - 1)));
    }
}
