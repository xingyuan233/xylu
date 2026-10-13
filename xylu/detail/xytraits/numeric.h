#pragma once

/**
 * @file    xytraits/numeric.h
 * @brief   编译期数值极限：摆脱标准库依赖的跨平台数值属性萃取
 * @date    2025-12-22
 */

#include "./kind.h"
#include "../xycore/attr.h"

/// 数值属性
namespace xylu::xytraits
{
    // 数值类型基类，便于拓展复用
    template <typename T>
    struct t_number_base : xyu::class_no_object_t
    {
        static_assert(t_is_number<T>, "T must be a number type");

        /* 通用属性 */

        // 类型
        using type = T;
        // 大小
        static constexpr xyu::size_t size = sizeof(T);

        // 是否为有符号数
        static constexpr bool is_signed = T(-1) < T(0);
        // 是否为无符号数
        static constexpr bool is_unsigned = T(-1) > T(0);

        // 是否为纯整数
        static constexpr bool is_integer = t_is_integer<T>;
        // 是否为浮点数
        static constexpr bool is_floating = t_is_floating<T>;

        // 最大值
        static constexpr T max = []
        {
            if constexpr (!is_floating) return is_signed ? (((((T)1<<(size*8-2))-1)<<1)+1) : ~T(0);
            else if constexpr (t_is_same<T, float>) return __FLT_MAX__;
            else if constexpr (t_is_same<T, double>) return __DBL_MAX__;
            else if constexpr (t_is_same<T, long double>) return __LDBL_MAX__;
            else return T();
        }();
        // 最小值
        static constexpr T min = []
        {
            if constexpr (is_integer) return is_signed ? ((T)-max-1) : T(0);
            else if constexpr (is_floating) return -max;
            else return T();
        }();

        // 二进制下的有效位数 (不含符号位，浮点数包含隐含的整数位)
        static constexpr xyu::uint digits = []
        {
            if constexpr (t_is_bool<T>) return 1;
            else if constexpr (is_integer) return size*8 - is_signed;
            else if constexpr (t_is_same<T, float>) return __FLT_MANT_DIG__;
            else if constexpr (t_is_same<T, double>) return __DBL_MANT_DIG__;
            else if constexpr (t_is_same<T, long double>) return __LDBL_MANT_DIG__;
            else return 0;
        }();
        // 十进制下能被精确表示的有效数字位数 (不含符号位)
        static constexpr xyu::uint digits10 = []
        {
            if constexpr (t_is_bool<T>) return 1;
            else return (digits - is_floating) * 643L / 2136; // (log10(2) -> 643/2136)
        }();

        /* 浮点数属性 */

        // 最小的正规范化值
        static constexpr T norm_min = []
        {
            if constexpr (t_is_same<T, float>) return __FLT_MIN__;
            else if constexpr (t_is_same<T, double>) return __DBL_MIN__;
            else if constexpr (t_is_same<T, long double>) return __LDBL_MIN__;
            else return T();
        }();
        // 最小的正非规范化值 (若支持)
        static constexpr T denorm_min = []
        {
            if constexpr (t_is_same<T, float>) return __FLT_DENORM_MIN__;
            else if constexpr (t_is_same<T, double>) return __DBL_DENORM_MIN__;
            else if constexpr (t_is_same<T, long double>) return __LDBL_DENORM_MIN__;
            else return T();
        }();

        // 机器精度：1.0 与下一个可表示的浮点数之间的差值
        static constexpr T epsilon = []
        {
            if constexpr (t_is_same<T, float>) return __FLT_EPSILON__;
            else if constexpr (t_is_same<T, double>) return __DBL_EPSILON__;
            else if constexpr (t_is_same<T, long double>) return __LDBL_EPSILON__;
            else return T();
        }();

        // 正无穷大（+∞）
        static constexpr T inf_plus = []
        {
            using namespace xyu;
            if constexpr (size == 4) return __builtin_bit_cast(T, 0x7F800000u);
            else if constexpr (size == 8) return __builtin_bit_cast(T, 0x7FF0000000000000ULL);
#if defined(__i386__) || defined(__x86_64__)
            else if constexpr (size > 8)
            {
                struct XY_PACKED LongDouble
                {
                    uint64 mant;
                    uint16 exp_and_sign;
                    uint8 pad[size - 10];
                };
                LongDouble ld{};
                ld.mant = 0x8000000000000000ULL;
                ld.exp_and_sign = 0x7fff;
                return __builtin_bit_cast(T, ld);
            }
#elif defined(__aarch64__) || defined(__arm64__) || defined(__riscv)
            else if constexpr (size == 16)
            {
                struct { uint64 low, high; } ld{};
                LongDouble ld{};
                ld.high = 0x7FFF << 48;
                return __builtin_bit_cast(T, ld);
            }
#endif
        }();

        // 负无穷大（-∞）
        static constexpr T inf_minus = []
        {
            using namespace xyu;
            if constexpr (t_is_same<T, float>) return __builtin_bit_cast(T, 0xFF800000u);
            else if constexpr (t_is_same<T, double>) return __builtin_bit_cast(T, 0xFFF0000000000000ULL);
#if defined(__i386__) || defined(__x86_64__)
            else if constexpr (size > 8)
            {
                struct XY_PACKED LongDouble
                {
                    uint64 mant;
                    uint16 exp_and_sign;
                    uint8 pad[size - 10];
                };
                LongDouble ld{};
                ld.mant = 0x8000000000000000ULL;
                ld.exp_and_sign = 0xffff;
                return __builtin_bit_cast(T, ld);
            }
#elif defined(__aarch64__) || defined(__arm64__) || defined(__riscv)
            else if constexpr (size == 16)
            {
                struct { uint64 low, high; } ld{};
                ld.high = 0xFFFF << 48;
                return __builtin_bit_cast(T, ld);
            }
#endif
        }();

        // 静默 NAN
        static constexpr T nan_quiet = []
        {
            if constexpr (t_is_same<T, float>) return __builtin_nanf("");
            else if constexpr (t_is_same<T, double>) return __builtin_nan("");
            else if constexpr (t_is_same<T, long double>) return __builtin_nanl("");
            else return T();
        }();
        // 信号 NAN
        static constexpr T nan_signal = []
        {
            if constexpr (t_is_same<T, float>) return __builtin_nansf("");
            else if constexpr (t_is_same<T, double>) return __builtin_nans("");
            else if constexpr (t_is_same<T, long double>) return __builtin_nansl("");
            else return T();
        }();

        // 尾数位数 (不含隐含位)
        static constexpr xyu::uint mantissas = []
        {
            if constexpr (is_floating) return digits - 1;
            else return 0;
        }();
        // 指数位数
        static constexpr xyu::uint exponents = []
        {
            if constexpr (t_is_same<T, float>) return __builtin_ctz(__FLT_MAX_EXP__) + 1;
            else if constexpr (t_is_same<T, double>) return __builtin_ctz(__DBL_MAX_EXP__) + 1;
            else if constexpr (t_is_same<T, long double>) return __builtin_ctz(__LDBL_MAX_EXP__) + 1;
            else return 0;
        }();

        // 重解释
        template <typename Test = T, typename = t_enable<t_is_floating<Test>>>
        static constexpr decltype(auto) to_bits(T val) noexcept
        {
            using namespace xyu;            
            
            if constexpr (size == 4) return __builtin_bit_cast(uint32, val);
            else if constexpr (size == 8) return __builtin_bit_cast(uint64, val);
#if defined(__i386__) || defined(__x86_64__)
            else if constexpr (size > 8)
            {
                struct XY_PACKED LongDouble
                {
                    uint64 mant;
                    uint16 exp_and_sign;
                    uint8 pad[size - 10];
                };
                auto ld = __builtin_bit_cast(LongDouble, val);
                struct FixLongDouble { uint64 mant; uint16 exp; bool sign; };
                return FixLongDouble{ld.mant, uint16(ld.exp_and_sign & 0x7fff), bool(ld.exp_and_sign >> 15)};
            }
#elif defined(__aarch64__) || defined(__arm64__) || defined(__riscv)
            else if constexpr (size == 16)
            {
                struct LongDouble { uint64 low, high; };
                auto ld = __builtin_bit_cast(LongDouble, val);
                struct FixLongDouble { uint64 mant_low, mant_high; uint16 exp; bool sign; };
                return FixLongDouble{ld.low, ld.high & 0xffffffffffffull, uint16((ld.high >> 48) & 0x7fff), bool(ld.high >> 63)};
            }
#endif
        }

        // 判断是否为有限值
        template <typename Test = T, typename = t_enable<t_is_floating<Test>>>
        static constexpr bool is_finity(T val) noexcept
        {
            if (t_constant())
            {
                if constexpr (size == 4) return (to_bits(val) & 0x7F800000u) != 0x7F800000u;
                else if constexpr (size == 8) return (to_bits(val) & 0x7FF0000000000000ULL) != 0x7FF0000000000000ULL;
                else return to_bits(val).exp != 0x7FFF;
            }
            return __builtin_isfinite(val);
        }

        // 判断是否为 INF
        template <typename Test = T, typename = t_enable<t_is_floating<Test>>>
        static constexpr bool is_inf(T val) noexcept
        {
            if (t_constant())
            {
                auto bits = to_bits(val);
                if constexpr (size == 4)
                    return (bits & 0x7F800000u) == 0x7F800000u && (bits & 0x007FFFFFu) == 0u;
                else if constexpr (size == 8)
                    return (bits & 0x7FF0000000000000ULL) == 0x7FF0000000000000ULL &&
                            (bits & 0x000FFFFFFFFFFFFFULL) == 0ULL;
#if defined(__i386__) || defined(__x86_64__)
                else if constexpr (size > 8) return bits.exp == 0x7FFF && bits.mant == 0x8000000000000000ULL;
#endif
                else if constexpr (size == 16)
                    return bits.exp == 0x7FFF && bits.mantissa_low == 0 && bits.mantissa_high == 0;
            }
            return __builtin_isinf(val);
        }
        // 判断是否为 +INF
        template <typename Test = T, typename = t_enable<t_is_floating<Test>>>
        static constexpr bool is_inf_plus(T val) noexcept
        {
            if (t_constant())
            {
                auto bits = to_bits(val);
                if constexpr (size == 4)
                    return (bits & 0x7F800000u) == 0x7F800000u && (bits & 0x007FFFFFu) == 0u && (bits & 0x80000000u) == 0u;
                else if constexpr (size <= 8)
                    return (bits & 0x7FF0000000000000ULL) == 0x7FF0000000000000ULL &&
                           (bits & 0x000FFFFFFFFFFFFFULL) == 0ULL &&
                           (bits & 0x8000000000000000ULL) == 0ULL;
#if defined(__i386__) || defined(__x86_64__)
                else if constexpr (size > 8) return bits.exp == 0x7FFF && bits.mant == 0x8000000000000000ULL && bits.sign == 0;
#endif
                else if constexpr (size == 16)
                    return bits.exp == 0x7FFF && bits.mantissa_low == 0 && bits.mantissa_high == 0 && bits.sign == 0;
            }
            return __builtin_isinf_sign(val) == 1;
        }
        // 判断是否为 -INF
        template <typename Test = T, typename = t_enable<t_is_floating<Test>>>
        static constexpr bool is_inf_minus(T val) noexcept
        {
            if (t_constant())
            {
                auto bits = to_bits(val);
                if constexpr (size == 4)
                    return (bits & 0x7F800000u) == 0x7F800000u && (bits & 0x007FFFFFu) == 0u && (bits & 0x80000000u) != 0u;
                else if constexpr (size <= 8)
                    return (bits & 0x7FF0000000000000ULL) == 0x7FF0000000000000ULL &&
                           (bits & 0x000FFFFFFFFFFFFFULL) == 0ULL &&
                           (bits & 0x8000000000000000ULL) != 0ULL;
#if defined(__i386__) || defined(__x86_64__)
                else if constexpr (size > 8) return bits.exp == 0x7FFF && bits.mant == 0x8000000000000000ULL && bits.sign != 0;
#endif
                else if constexpr (size == 16)
                    return bits.exp == 0x7FFF && bits.mantissa_low == 0 && bits.mantissa_high == 0 && bits.sign != 0;
            }
            return __builtin_isinf_sign(val) == -1;
        }

        // 判断是否为 NAN
        template <typename Test = T, typename = t_enable<t_is_floating<Test>>>
        static constexpr bool is_nan(T val) noexcept
        {
            if (t_constant())
            {
                auto bits = to_bits(val);
                if constexpr (size == 4)
                    return (bits & 0x7F800000u) == 0x7F800000u && (bits & 0x007FFFFFu) != 0u;
                else if constexpr (size <= 8)
                    return (bits & 0x7FF0000000000000ULL) == 0x7FF0000000000000ULL &&
                           (bits & 0x000FFFFFFFFFFFFFULL) != 0ULL;
#if defined(__i386__) || defined(__x86_64__)
                else if constexpr (size > 8) return bits.exp == 0x7FFF && bits.mant != 0x8000000000000000ULL;
#endif
                else if constexpr (size == 16)
                    return bits.exp == 0x7FFF && (bits.mantissa_low != 0 || bits.mantissa_high != 0);
            }
            return __builtin_isnan(val);
        }

        // 计算 ulp
        template <typename Test = T, typename = t_enable<t_is_floating<Test> && size <= 8>>
        static constexpr T ulp(T val) noexcept
        {
            using U = t_cond<size == 4, xyu::uint32, xyu::uint64>;
            constexpr int mant_bits = digits - 1;
            constexpr int exp_shift = mant_bits;
            constexpr U exp_mask = (size == 4) ? 0x7F800000 : 0x7FF0000000000000ULL;
            constexpr U mant_mask = (size == 4) ? 0x007FFFFF : 0x000FFFFFFFFFFFFFULL;
            constexpr int exp_all_ones = (size == 4) ? 0xFF : 0x7FF;

            U bits = __builtin_bit_cast(U, val);
            int exp = (bits & exp_mask) >> exp_shift;

            // 1. 处理 无穷大 和 NaN
            if (XY_UNLIKELY(exp == exp_all_ones)) return val;

            // 2. 处理 0 和 非规格化数
            if (exp == 0) return denorm_min;

            // 3. 处理规格化数
            //    对于规格化数，ULP = 2^(指数 - 偏置 - 尾数位数)
            //    即新指数的偏置值 = exp - mant_bits
            int new_exp = exp - mant_bits;

            if (new_exp > 0)
            {
                // 若 ULP 是指数 > 0 的规格化数，直接构造一个浮点数
                U ulp_bits = (static_cast<U>(new_exp) << exp_shift);
                return __builtin_bit_cast(T, ulp_bits);
            }
            else
            {
                // 若 ULP 是指数 <= 0 的非规格化数
                // 指数位全为0，尾数 = 2^(new_exp + mant_bits - 1)
                int shift = new_exp + mant_bits - 1;
                U mant = 1ULL << shift;
                return __builtin_bit_cast(T, mant);
            }
        }

        /* 浮点数特殊值 */

        // pi
        static constexpr T pi = []
        {
            if constexpr (!is_floating) return 3;
            else if constexpr (size <= 8) return 3.141592653589793;
#if defined(__i386__) || defined(__x86_64__)
            else if constexpr (size > 8) return 3.14159265358979323846L;
#elif defined(__aarch64__) || defined(__arm64__) || defined(__riscv)
            else if constexpr (size == 16) return 3.14159265358979323846264338327950288L;
#endif
        }();

        // pi ÷ 2
        static constexpr T pi_d2 = []
        {
            if constexpr (!is_floating) return 2;
            else if constexpr (size <= 8) return 1.5707963267948966;
#if defined(__i386__) || defined(__x86_64__)
            else if constexpr (size > 8) return 1.57079632679489661923L;
#elif defined(__aarch64__) || defined(__arm64__) || defined(__riscv)
            else if constexpr (size == 16) return 1.57079632679489661923132169163975144L;
#endif
        }();

        // pi * 2
        static constexpr T pi_m2 = []
        {
            if constexpr (!is_floating) return 6;
            else if constexpr (size <= 8) return 6.283185307179586;
#if defined(__i386__) || defined(__x86_64__)
            else if constexpr (size > 8) return 6.28318530717958647692L;
#elif defined(__aarch64__) || defined(__arm64__) || defined(__riscv)
            else if constexpr (size == 16) return 6.28318530717958647692528676655900576L;
#endif
        }();

        // e
        static constexpr T e = []
        {
            if constexpr (!is_floating) return 3;
            else if constexpr (size <= 8) return 2.718281828459045;
#if defined(__i386__) || defined(__x86_64__)
            else if constexpr (size > 8) return 2.71828182845904523536L;
#elif defined(__aarch64__) || defined(__arm64__) || defined(__riscv)
            else if constexpr (size == 16) return 2.71828182845904523536028747135266250L;
#endif
        }();
    };

    /**
     * @brief 为数值类型提供编译期属性的基类模板
     * @details
     * 包含了关于数值类型（整型、浮点型）的各种常量信息
     *
     * ### 通用属性:
     * - `is_signed`: 是否为有符号数
     * - `is_unsigned`: 是否为无符号数
     * - `is_integer`: 是否为整数 (不包括 bool)
     * - `is_floating`: 是否为浮点数
     * - `max`: 类型的最大可表示值
     * - `min`: 类型的最小可表示值
     * - `digits`: 二进制下的有效位数 (不含符号位)
     * - `digits10`: 十进制下能被精确表示的有效数字位数
     *
     * ### 浮点数专用属性:
     * - `norm_min`: 最小的正规范化值
     * - `denorm_min`: 最小的正非规范化值 (若支持)
     * - `epsilon`: 机器精度
     * - `infinity`: 正无穷大的表示
     * - `nan_quiet`: 静默 NaN 的表示
     * - `nan_signal`: 信号 NaN 的表示
     * - `mantissas`: 尾数位数 (不含隐含位)
     * - `exponents`: 指数位数
     */
    template <typename T, bool = t_is_enum<T>>
    struct t_number : t_number_base<t_rem_cr<T>> {};
    template <typename T>
    struct t_number<T, true> : t_number_base<__underlying_type(t_rem_cr<T>)> {};
}

/// 数值转换
namespace xylu::xytraits
{
    namespace __
    {
        template <typename T, typename = void>
        struct change_sign2un_impl { using type = T; };
        template <> struct change_sign2un_impl<char> { using type = unsigned char; };
        template <> struct change_sign2un_impl<signed char> { using type = unsigned char; };
        template <> struct change_sign2un_impl<short> { using type = unsigned short; };
        template <> struct change_sign2un_impl<int> { using type = unsigned int; };
        template <> struct change_sign2un_impl<long> { using type = unsigned long; };
        template <> struct change_sign2un_impl<long long> { using type = unsigned long long; };
#ifdef __SIZEOF_INT128__
        template <> struct change_sign2un_impl<xyu::int128> { using type = xyu::uint128; };
#endif
    }
    /// 获取数值对应的无符号类型
    template <typename T>
    using t_chg_sign2un = typename __::change_sign2un_impl<t_rem_cr<T>>::type;

    namespace __
    {
        template <typename T, typename = void>
        struct change_un2sign_impl { using type = T; };
        template <> struct change_un2sign_impl<unsigned char> { using type = char; };
        template <> struct change_un2sign_impl<unsigned short> { using type = short; };
        template <> struct change_un2sign_impl<unsigned int> { using type = int; };
        template <> struct change_un2sign_impl<unsigned long> { using type = long; };
        template <> struct change_un2sign_impl<unsigned long long> { using type = long long; };
#ifdef __SIZEOF_INT128__
        template <> struct change_un2sign_impl<xyu::uint128> { using type = xyu::int128; };
#endif
    }
    /// 获取数值对应的有符号类型
    template <typename T>
    using t_chg_un2sign = typename __::change_un2sign_impl<t_rem_cr<T>>::type;

    /// 将无符号数变为相应的有符号数
    template <typename T>
    constexpr auto make_signed(T num) { return static_cast<t_chg_un2sign<T>>(num); }

    /// 将有符号数变为相应的无符号数
    template <typename T>
    constexpr auto make_unsigned(T num) { return static_cast<t_chg_sign2un<T>>(num); }
}

/// 类型拓宽
namespace xylu::xytraits
{
    /// 获取更宽类型 - 基类
    template <typename T, typename U, typename = void>
    struct t_wide_base;
    template <typename T>
    struct t_wide_base<T, void> { using type = T; };
    template <typename T>
    struct t_wide_base<void, T> { using type = T; };
    template <typename T>
    struct t_wide_base<T, T> { using type = T; };
    template <typename T, typename U>
    struct t_wide_base<T, U, t_enable<t_is_number<T> && t_is_number<U> && !t_is_same<T, U>>>
    {
        constexpr static bool f1 = t_is_floating<T>;
        constexpr static bool f2 = t_is_floating<U>;
        constexpr static xyu::size_t s1 = sizeof(T);
        constexpr static xyu::size_t s2 = sizeof(U);
        constexpr static bool u1 = t_number<T>::is_unsigned;
        constexpr static bool u2 = t_number<U>::is_unsigned;
        constexpr static xyu::size_t s = (s1 >= s2 ? s1 : s2);

        using type =
            t_cond<f1 == f2 && u1 == u2,  t_cond<(s1 >= s2), T, U>,
            t_cond<f1 && !f2, t_cond<(s1 > s2), T, t_cond<(sizeof(double) > s2), double, long double>>,
            t_cond<!f1 && f2, t_cond<(s2 > s1), U, t_cond<(sizeof(double) > s1), double, long double>>,
            t_cond<(s < 2), xyu::int16, t_cond<(s < 4), xyu::int32, t_cond<(s < 8), xyu::int64,
#ifdef __SIZEOF_INT128__
                t_cond<s >= 16, xyu::uint128, xyu::uint64>
#else
                xyu::uint64
#endif
            >>>>>>;
    };

    namespace __
    {
        template <typename T, typename U, typename = void>
        struct get_wide_base
        { using type = typename t_wide_base<U, T>::type; };
        template <typename T, typename U>
        struct get_wide_base<T, U, t_void<typename t_wide_base<T,U>::type>>
        { using type = typename t_wide_base<T, U>::type; };

        template <typename... Args>
        struct get_wide;
        template <>
        struct get_wide<> { using type = void; };
        template <typename T>
        struct get_wide<T> { using type = T; };
        template <typename T, typename U, typename... Args>
        struct get_wide<T, U, Args...>
        { using type = typename get_wide<typename get_wide_base<T, U>::type, Args...>::type; };
    }
    /**
     * @brief 获取更宽(表示范围更大或精度更高)的类型
     * @details 其选择规则如下：
     *   1.忽略 const及引用修饰
     *   2.所有类型相同时，返回原类型
     *   3.任意类型均宽于 void
     *   4. 整型 vs 整型:
     *      - 若符号性不同，结果提升为能容纳两者的有符号类型 (无法提升则返回无符号类型)
     *      - 否则，返回字节数更大的类型
     *   5. 浮点型 vs 浮点型: 返回字节数更大的类型
     *   6. 浮点型 vs 整型: 返回能同时精确表示两者的浮点类型
     *
     * @note 未显式传入 128位拓展整型时，不会自动提升为拓展类型，以保证跨平台一致性
     * @note 若需支持自定义数值类型，应特化 t_wide_base
     */
    template <typename... Args>
    using t_wide = typename __::get_wide<t_rem_cr<Args>...>::type;
}
