#pragma once

/**
 * @file    xymath/hash.h
 * @brief   哈希函数
 * @date    2026-06-20
 */

#include "../xytraits/numeric.h"
#include "../xymemory/func.h"

/// 哈希函数
namespace xylu::xymath
{
    /**
     * @brief 哈希基类
     * @tparam T 特化类型
     * @tparam SFINAE 支持的条件，使用 `xyu::t_enable`
     * @tparam priority 优先级，数值越小越先匹配，最大值为 9，超过后不会被匹配
     * @details
     *   - 对于单元素，提供 `xyu::size_t operator()(const T&) noexcept;`
     *   - 对于混合哈希，T 指定为 void，并提供 `xyu::size_t operator()(xyu::size_t hash1, xyu::size_t hash2) noexcept;`
     */
    template <typename T, typename SFINAE = void, xyu::size_t priority = 0>
    struct Hasher;

    namespace __
    {
        // 不存在
        template <typename T>
        constexpr xyu::size_t make_hash_impl(const T&, xyu::size_c<10>, int)
        {
            static_assert(xyu::t_is_void<T>, "can't find any Hasher for the type");
            return 0;
        }

        // 存在直接调用
        template <typename T, xyu::size_t priority, typename H = Hasher<T, void, priority>,
                  typename = decltype(xyu::t_val<H>()(xyu::t_val<const T&>())), typename = xyu::t_enable<(priority <= 9)>>
        constexpr xyu::size_t make_hash_impl(const T& value, xyu::size_c<priority>, int) noexcept { return H{}(value); }

        // 不存在查找下一级
        template <typename T, xyu::size_t priority, typename = xyu::t_enable<(priority <= 9)>>
        constexpr xyu::size_t make_hash_impl(const T& value, xyu::size_c<priority>, long) noexcept
        { return make_hash_impl(value, xyu::size_c<priority + 1>{}, 0); }
    }
    /**
     * @brief 哈希函数
     * @details 直接转发给 `Hasher`，可通过重载其进行自 定义类型支持 或 优先级覆盖
     */
    template <typename T>
    constexpr xyu::size_t make_hash(const T& value) noexcept { return __::make_hash_impl(value, xyu::size_c<0>{}, 0); }

    // 整型
    template <typename T>
    struct Hasher<T, xyu::t_enable<xyu::t_is_integral<T>>, 6>
    {
        // 8字节
        template <typename Test = T, xyu::t_enable<sizeof(Test) == 8, int> = 8>
        constexpr xyu::size_t operator()(T value) noexcept
        {
#ifdef __SIZEOF_INT128__
            constexpr xyu::uint64 SECRET1 = 0x2d358dccaa6c78a5ull;
            constexpr xyu::uint64 SECRET2 = 0x8bb84b93962eacc9ull;
            __uint128_t result = __uint128_t(value ^ SECRET1) * (value ^ SECRET2);
            return xyu::uint64(result) ^ xyu::uint64(result >> 64);
#else
            xyu::size_t result = value;
            result = (result ^ (result >> 30)) * 0xbf58476d1ce4e5b9;
            result = (result ^ (result >> 27)) * 0x94d049bb133111eb;
            result = result ^ (result >> 31);
            return result;
#endif
        }

        // 4字节
        template <typename Test = T, xyu::t_enable<sizeof(Test) == 4, int> = 4>
        constexpr xyu::size_t operator()(T value) noexcept
        {
            constexpr xyu::uint32 M1 = 0x85ebca6bu;
            constexpr xyu::uint32 M2 = 0xc2b2ae35u;
            xyu::uint64 result = xyu::uint64(value ^ M1) * (value ^ M2);
            if constexpr (sizeof(xyu::size_t) == 8) return result ^ (result >> 32);
            else return xyu::size_t(result) ^ xyu::size_t(result >> 32);
        }

        // 2字节
        template <typename Test = T, xyu::t_enable<sizeof(Test) == 2, int> = 2>
        constexpr xyu::size_t operator()(T value) noexcept
        {
            return make_hash(xyu::uint32(value) | (xyu::uint32(value) << 16));
        }

        // 1字节
        template <typename Test = T, xyu::t_enable<sizeof(Test) == 1, int> = 1>
        constexpr xyu::size_t operator()(T value) noexcept
        {
            xyu::uint32 v{};
            xyu::mem_set(&v, value, 1);
            return make_hash(v);
        }
    };

    // 浮点型
    template <typename T>
    struct Hasher<T, xyu::t_enable<xyu::t_is_floating<T>>, 6>
    {
        // 浮点数
        template <typename Test, xyu::t_enable<xyu::t_is_same<Test, float>, char> = 'f'>
        constexpr xyu::size_t operator()(T value) noexcept
        {
            if (value == 0.0f) return 0;
            return make_hash(xyu::mem_cast<xyu::uint32>(value));
        }

        // 双精度浮点数
        constexpr xyu::size_t operator()(double value) noexcept
        {
            if (value == 0.0f) return 0;
            return make_hash(xyu::mem_cast<xyu::uint64>(value));
        }

        // 长双精度浮点数
        constexpr xyu::size_t operator()(long double value) noexcept
        {
            // +0.0 | -0.0  ->  0
            if (value == 0.0l) return 0;
            if (xyu::t_constant()) return operator()(double(value));
            if (XY_UNLIKELY(!xyu::t_number<long double>::is_finity(value))) return -1;
            // 指数部分 2^m
            int exponent{};
            // 尾数部分 [-1.0,-0.5)U[0.5,1.0) -> [0, 1.0)
            value = __builtin_frexpl(value, &exponent);
            value = value < 0.0l ? -(value + 0.5l) : value;
            // 将尾数映射到完整的 xyu::size_t
            constexpr long double mult = xyu::t_number<xyu::size_t>::max + 1.0l;
            value *= mult;
            return make_hash(xyu::size_t(value)) ^ make_hash(exponent);
        }
    };

    // 枚举类型
    template <typename T>
    struct Hasher<T, xyu::t_enable<xyu::t_is_enum<T>>, 7>
    {
        constexpr xyu::size_t operator()(T value) noexcept
        {
            return make_hash(typename xyu::t_number<T>::type(value));
        }
    };

    // nullptr
    template <>
    struct Hasher<xyu::nullptr_t, void, 5>
    {
        constexpr xyu::size_t operator()(xyu::nullptr_t) noexcept { return 0; }
    };

    // 指针类型
    template <typename T>
    struct Hasher<T, xyu::t_enable<xyu::t_is_pointer<T>>, 7>
    {
        constexpr xyu::size_t operator()(T p) noexcept { return make_hash(xyu::diff_t(p)); }
    };


    namespace __
    {
        // 不存在
        template <typename T = int>
        constexpr xyu::size_t mix_hash_impl(xyu::size_t, xyu::size_t, xyu::size_c<10>, int)
        {
            static_assert(xyu::t_is_void<T>, "can't find any Hasher for mix hash");
            return 0;
        }

        // 存在直接调用
        template <xyu::size_t priority, typename H = Hasher<void, void, priority>,
                  typename = decltype(xyu::t_val<H>()(xyu::size_t{}, xyu::size_t{})), typename = xyu::t_enable<(priority <= 9)>>
        constexpr xyu::size_t mix_hash_impl(xyu::size_t h1, xyu::size_t h2, xyu::size_c<priority>, int) noexcept  { return H{}(h1, h2); }

        // 不存在查找下一级
        template <xyu::size_t priority, typename = xyu::t_enable<(priority <= 9)>>
        constexpr xyu::size_t mix_hash_impl(xyu::size_t h1, xyu::size_t h2, xyu::size_c<priority>, long) noexcept
        { return mix_hash_impl(h1, h2, xyu::size_c<priority + 1>{}, 0); }
    }
    /**
     * @brief 多元素哈希函数
     * @details 转发给 `Hasher` 计算每个元素的哈希值并混合，可通过重载其进行自 定义类型支持 或 优先级覆盖
     */
    template <typename... U, typename = xyu::t_enable<sizeof...(U)>>
    constexpr xyu::size_t make_hash(xyu::size_t seed, const U&... us) noexcept
    {
        xyu::size_t hash = seed;
        (..., (hash = __::mix_hash_impl(hash, make_hash(us), xyu::size_c<0>{}, 0)));
        return hash;
    }

    // 混合哈希
    template <>
    struct Hasher<void, void, 6>
    {
        constexpr xyu::size_t operator()(xyu::size_t h1, xyu::size_t h2) noexcept
        {
            if constexpr (sizeof(xyu::size_t) == 4)
                 return h1 ^ (h2 + 0x9e3779b9u + (h1 << 6) + (h1 >> 2));
            else return h1 ^ (h2 + 0x9e3779b97f4a7c15ull + (h1 << 6) + (h1 >> 2));
        }
    };
}

/// 哈希属性
namespace xylu::xytraits
{
    namespace __
    {
        template <typename T, typename = decltype(xyu::make_hash(t_val<const T&>()))>
        true_c can_hash_test(int);
        template <typename T>
        false_c can_hash_test(...);
    }
    /// 判断是否支持哈希函数
    template <typename... T>
    constexpr bool t_can_hash = (... && decltype(__::can_hash_test<T>(0))::value);

    namespace __
    {
        template <typename T, typename U, typename = void>
        struct is_same_hash_impl : false_c {};
        // signed -> unsigned
        template <typename T, typename U>
        struct is_same_hash_impl<T, U, t_enable<t_is_same<t_chg_sign2un<U>, T>>> : true_c {};
        // unsigned -> signed
        template <typename T, typename U>
        struct is_same_hash_impl<T, U, t_enable<t_is_same<t_chg_un2sign<U>, T>>> : true_c {};
    }
    /// 判断 Check 是否与 Standatd 具有相同的哈希 - 基类
    template <typename Standard, typename Check, typename = void>
    struct t_is_same_hash_base : __::is_same_hash_impl<Standard, Check> {};
    template <typename Standatd>
    struct t_is_same_hash_base<Standatd, Standatd> : true_c {};

    /**
     * @brief 判断 Check 是否与 Standatd 具有相同的哈希
     * @details
     *   - 即 `make_hash(T(v)) == `make_hash(v)`
     *   - 当模板成立时，计算哈希值时不需要转换为相同类型
     *   - 自定义类型需要通过继承 `t_is_same_hash_base` 拓展
     *   - 默认情况下所有相同大小的整型等价，各种字符串等价，如须禁止需特化 `t_is_same_hash_base` 覆盖
     */
    template <typename Standatd, typename... Check>
    constexpr bool t_is_same_hash = (... && t_is_same_hash_base<t_rem_cr<Standatd>, t_rem_cr<Check>>::value);
}
