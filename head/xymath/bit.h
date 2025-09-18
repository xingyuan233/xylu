#pragma clang diagnostic push
#pragma ide diagnostic ignored "OCUnusedGlobalDeclarationInspection"
#pragma ide diagnostic ignored "ConstantConditionsOC"
#pragma once

#include "../../link/attr"
#include "../../link/traits"

/**
 * @file
 * @brief 提供了一组高效、类型通用的位运算函数。
 * @details
 *   本文件中的函数支持对任意大小的整数类型（包括自定义的大整数类型）进行位操作。
 *   它们通过 `if constexpr` 自动为标准整数宽度选择最优的编译器内建实现，
 *   并为超大整数类型提供了分块处理的回退方案。
 * @warning
 *   **整数提升陷阱 (Integer Promotion)**:
 *   在 C++ 中，比 `int` 小的整数类型（如 `char`, `short`）在参与大多数表达式运算时，
 *   其值会被自动提升（promote）为 `int` 或 `unsigned int`。这可能导致位运算
 *   （特别是涉及移位和取反的）产生非预期的结果。本文件中的函数已通过 `static_cast`
 *   等方式处理此问题，但使用者在库外进行位运算时仍需特别注意。
 */

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
        constexpr auto Mu = xyu::number_traits<uint>::max;
        constexpr auto Mul = xyu::number_traits<ulong>::max;
        constexpr auto Mull = xyu::number_traits<ullong>::max;
    }

    /**
     * @brief 检查一个整数的二进制表示中 1 的个数的奇偶性（奇偶校验）。
     * @param v 要检查的整数。
     * @return 若 1 的个数为奇数，返回 true；若为偶数，返回 false。
     * @note 对于有符号负数，其补码表示中的所有 1（包括符号位）都会被计数。
     */
    template <typename T>
    XY_CONST constexpr bool bit_check_parity(T v) noexcept
    {
        static_assert(xyu::t_is_mathint<T>);

        using vt = xyu::number_traits<T>;
        constexpr auto St = vt::size;

        constexpr auto inner = [](T u) noexcept
        {
            if constexpr      (St <= __::Su)       return __builtin_parity(u);
            else if constexpr (St <= __::Sul)      return __builtin_parityl(u);
            else if constexpr (St <= __::Sull)     return __builtin_parityll(u);
            else if constexpr (St <= __::Sull*2)   return __builtin_parityll(u & __::Mull)
                                                   ^ __builtin_parityll(u >> __::Sull*8);
            else {
                bool parity = false;
                while (u) parity ^= __builtin_popcountll(u & __::Mull), u >>= __::Sull*8;
                return parity;
            }
        };

        //无符号整数 或 与内置函数位数相同的有符号整数
        if constexpr (!vt::is_signed || St == __::Su || St == __::Sul || St == __::Sull) return inner(v);
        //有符号整数 (其余的) ((T)防止short等小类型被整型提升为int)
        else return v >= 0 ? inner(v) : !inner(static_cast<T>(v & vt::max));
    }

    /**
     * @brief 计算一个整数的二进制表示中 1 的总数（population count）。
     * @param v 要计算的整数。
     * @return 整数 v 的二进制表示中 1 的数量。
     * @note 对于有符号负数，其补码表示中的所有 1（包括符号位）都会被计数。
     */
    template <typename T>
    XY_CONST constexpr xyu::uint bit_count_1_total(T v) noexcept
    {
        static_assert(xyu::t_is_mathint<T>);

        using vt = xyu::number_traits<T>;
        constexpr auto St = vt::size;

        constexpr auto inner = [](T u) noexcept
        {
            if constexpr      (St <= __::Su)       return __builtin_popcount(u);
            else if constexpr (St <= __::Sul)      return __builtin_popcountl(u);
            else if constexpr (St <= __::Sull)     return __builtin_popcountll(u);
            else if constexpr (St <= __::Sull*2)   return __builtin_popcountll(u & __::Mull)
                                                   + __builtin_popcountll(u >> __::Sull*8);
            else {
                int count = 0;
                while (u) count += __builtin_popcountll(u & __::Mull), u >>= __::Sull*8;
                return count;
            }
        };

        //无符号整数 或 与内置函数位数相同的有符号整数
        if constexpr (!vt::is_signed || St == __::Su || St == __::Sul || St == __::Sull) return inner(v);
        //有符号整数 (其余的) ((T)防止short等小类型被整型提升为int)
        else return v >= 0 ? inner(v) : inner(static_cast<T>(v & vt::max)) + 1;
    }

    /**
     * @brief 计算一个整数的二进制表示中 0 的总数。
     * @param v 要计算的整数。
     * @return 整数 v 的二进制表示中 0 的数量。
     * @note 结果等于 (sizeof(T) * 8 - bit_count_1_total(v))。
     */
    template <typename T>
    XY_CONST constexpr xyu::uint bit_count_0_total(T v) noexcept { return sizeof(T)*8 - bit_count_1_total(v); }

    /**
     * @brief 计算整数二进制表示中前导 0 的数量（count leading zeros, clz）。
     * @details 此函数计算从最高有效位（MSB）开始的连续 0 的数量。
     * @param v 要计算的整数。
     * @return 前导 0 的数量。
     * @note
     *   - 若 v 为 0，则返回该类型的总位数 (sizeof(T) * 8)。
     *   - 对于负数（其 MSB 为 1），前导零的数量总是 0。
     */
    template <typename T>
    XY_CONST constexpr xyu::uint bit_count_0_front(T v) noexcept
    {
        static_assert(xyu::t_is_mathint<T>);

        using vt = xyu::number_traits<T>;
        constexpr auto St = vt::size;

        constexpr auto inner = [](T u) noexcept
        {
            if constexpr      (St <= __::Su)     return __builtin_clz(u)   - 8*(__::Su-St);
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
                while (parts > 0 && pu == 0)
                    pu = (u >> --parts * 8*__::Sull) & __::Mull,
                    count += (pu == 0 ? 8*__::Sull : __builtin_clzll(pu));
                return count;
            }
        };

        //无符号整数
        if constexpr (!vt::is_signed) return v == 0 ? St*8 : inner(v);
        //有符号整数
        else return v < 0 ? 0 : (v == 0 ? St*8 : inner(v));
    }

    /**
     * @brief 计算整数二进制表示中前导 1 的数量（count leading ones）。
     * @details 此函数计算从最高有效位（MSB）开始的连续 1 的数量。
     * @param v 要计算的整数。
     * @return 前导 1 的数量。
     * @note
     *   - 若 v 的所有位均为 1，则返回该类型的总位数。
     *   - 对于正数（其 MSB 为 0），前导一的数量总是 0。
     */
    template <typename T>
    XY_CONST constexpr xyu::uint bit_count_1_front(T v) noexcept { return bit_count_0_front(static_cast<T>(~v)); }

    /**
     * @brief 计算整数二进制表示中尾随 0 的数量（count trailing zeros, ctz）。
     * @details 此函数计算从最低有效位（LSB）开始的连续 0 的数量。
     * @param v 要计算的整数。
     * @return 尾随 0 的数量。
     * @note 若 v 为 0，则返回该类型的总位数 (sizeof(T) * 8)。
     */
    template <typename T>
    XY_CONST constexpr xyu::uint bit_count_0_back(T v) noexcept
    {
        static_assert(xyu::t_is_mathint<T>);

        using vt = xyu::number_traits<T>;
        constexpr auto St = vt::size;

        constexpr auto inner = [](T u) noexcept
        {
            if constexpr      (St <= __::Su)     return __builtin_ctz(u);
            else if constexpr (St <= __::Sul)    return __builtin_ctzl(u);
            else if constexpr (St <= __::Sull)   return __builtin_ctzll(u);
            else {
                //如果为负数，转换为非负数 (使算数位移填充0)
                if (vt::is_signed && u < 0) u &= vt::max;
                //分组处理
                int count = 0, tmp = 8*__::Sull;
                while (u && tmp == 8*__::Sull)
                    tmp = __builtin_ctzll(u & __::Mull),
                    count += tmp,
                    u >>= 8*__::Sull;
                return count;
            }
        };

        return v == 0 ? St*8 : inner(v);
    }

    /**
     * @brief 计算整数二进制表示中尾随 1 的数量（count trailing ones）。
     * @details 此函数计算从最低有效位（LSB）开始的连续 1 的数量。
     * @param v 要计算的整数。
     * @return 尾随 1 的数量。
     * @note 若 v 的所有位均为 1，则返回该类型的总位数。
     */
    template <typename T>
    XY_CONST constexpr xyu::uint bit_count_1_back(T v) noexcept { return bit_count_0_back(static_cast<T>(~v)); }

    /**
     * @brief 计算表示一个数值所需要的最小位数（bit length）。
     * @param v 要计算的整数。
     * @return
     *   - 对于 0，返回 1。
     *   - 对于正数，返回其不包含前导零的位数。
     *   - 对于负数，返回其补码表示中不包含多余前导一（保留一个符号位）的位数。
     */
    template <typename T>
    XY_CONST constexpr xyu::uint bit_count_effect(T v) noexcept
    {
        using vt = xyu::number_traits<T>;
        constexpr auto St = vt::size;

        if (v == 0) return 1;
        if constexpr (!vt::is_signed) return 8*St - bit_count_0_front(v);
        else return v >= 0 ? 8*St - bit_count_0_front(v) :
                             8*St - bit_count_1_front(v) + 1;
    }

    /**
     * @brief 查找不小于 v 的最小的 2 的幂。
     * @param v 输入值。
     * @return
     *   - 若 v 为非正数，返回 1。
     *   - 若 v 已经是 2 的幂，返回 v。
     *   - 否则，返回大于 v 的最小的 2 的幂。
     *   - 如果计算结果溢出，返回 0。
     */
    template <typename T>
    XY_CONST constexpr T bit_get_2ceil(T v) noexcept
    {
        using vt = xyu::number_traits<T>;
        if (v <= 1) return 1;
        //防止整型提升 ((T)防止short等小类型被整型提升为int)
        return 1 << (vt::digits - bit_count_0_front(static_cast<T>(v-1)));
    }

    /**
     * @brief 查找不大于 v 的最大的 2 的幂。
     * @param v 输入值。
     * @return
     *   - 若 v 为非正数，返回 0。
     *   - 否则，返回小于或等于 v 的最大的 2 的幂。
     */
    template <typename T>
    XY_CONST constexpr T bit_get_2floor(T v) noexcept
    {
        using vt = xyu::number_traits<T>;
        if (v <= 0) return 0;
        return 1 << (vt::digits - bit_count_0_front(v));
    }

    /**
     * @brief 交换整数的字节序（endianness swap）。
     * @param v 要交换字节序的整数。
     * @return 字节序反转后的整数。
     * @note
     *   主要为无符号整数设计。对有符号数应用此函数会改变其算术值，
     *   但可用于底层数据表示的字节序转换。
     */
    template <typename T>
    XY_CONST constexpr T bit_swap_byte(T v) noexcept
    {
        static_assert(xyu::t_is_mathint<T>);

        using vt = xyu::number_traits<T>;
        constexpr auto St = vt::size;

        if constexpr (St <= 1) return v;
        else if constexpr (St == 2) return __builtin_bswap16(v);
        else if constexpr (St == 4) return __builtin_bswap32(v);
        else if constexpr (St == 8) return __builtin_bswap64(v);
        else {
            T result = 0;
            for (xyu::uint i = 0; i < St; i++)
                result |= (((v >> (i*8)) & 0xff) << ((St-1-i)*8));
            return result;
        }
    }

    /**
     * @brief 对整数 v 执行循环左移 bits 位。
     * @param v    要操作的整数。
     * @param bits 左移的位数。
     * @return 循环左移后的结果。
     * @note 有符号整数必须采用补码表示法才能保证行为正确。
     */
    template <typename T>
    XY_CONST constexpr T bit_rotate_left(T v, xyu::size_t bits) noexcept
    {
        static_assert(xyu::t_is_mathint<T>);

        using vt = xyu::number_traits<T>;
        constexpr auto St = vt::size;

        if constexpr      (St == 16) return __builtin_rotateleft16(v, bits);
        else if constexpr (St == 32) return __builtin_rotateleft32(v, bits);
        else if constexpr (St == 64) return __builtin_rotateleft64(v, bits);
        else {
            if (XY_UNLIKELY(bits >= St*8)) bits %= St*8;
            return (v << bits) | ((v >> (St*8 - bits)) & ((1 << bits) - 1));
        }
    }

    /**
     * @brief 对整数 v 执行循环右移 bits 位。
     * @param v    要操作的整数。
     * @param bits 右移的位数。
     * @return 循环右移后的结果。
     * @note 有符号整数必须采用补码表示法才能保证行为正确。
     */
    template <typename T>
    XY_CONST constexpr T bit_rotate_right(T v, xyu::size_t bits) noexcept
    {
        static_assert(xyu::t_is_mathint<T>);

        using vt = xyu::number_traits<T>;
        constexpr auto St = vt::size;

        if constexpr      (St == 16) return __builtin_rotateright16(v, bits);
        else if constexpr (St == 32) return __builtin_rotateright32(v, bits);
        else if constexpr (St == 64) return __builtin_rotateright64(v, bits);
        else {
            if (XY_UNLIKELY(bits >= St*8)) bits %= St*8;
            return (((v >> bits) & (1 << (St*8 - bits)) - 1) | (v << (St*8 - bits)));
        }
    }
}

#pragma clang diagnostic pop