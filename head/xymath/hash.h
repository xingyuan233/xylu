#pragma clang diagnostic push
#pragma ide diagnostic ignored "Simplify"
#pragma ide diagnostic ignored "OCUnusedGlobalDeclarationInspection"
#pragma ide diagnostic ignored "UnreachableCallsOfFunction"
#pragma once

#include "../../link/traits"
#include "../../link/memfun"
#include "../../link/calcfun"

/// 哈希函数
namespace xylu::xymath
{
    // 64位整数
    template <typename T, xyu::t_enable<xyu::t_is_integral<T> && sizeof(T) == 8, int> = 8>
    constexpr xyu::size_t make_hash(T value) noexcept
    {
        if constexpr (sizeof(xyu::size_t) == 8)
        {
            constexpr xyu::uint64 SECRET1 = 0x2d358dccaa6c78a5ull;
            constexpr xyu::uint64 SECRET2 = 0x8bb84b93962eacc9ull;
            __uint128_t result = static_cast<__uint128_t>(value ^ SECRET1) * (value ^ SECRET2);
            return static_cast<xyu::uint64>(result) ^ static_cast<xyu::uint64>(result >> 64);
        }
        else
        {
            value = (value ^ (value >> 30)) * 0xbf58476d1ce4e5b9;
            value = (value ^ (value >> 27)) * 0x94d049bb133111eb;
            value = value ^ (value >> 31);
            return value;
        }
    }

    // 32位整数
    template <typename T, xyu::t_enable<xyu::t_is_integral<T> && sizeof(T) == 4, int> = 4>
    constexpr xyu::size_t make_hash(T value) noexcept
    {
        constexpr xyu::uint32 M1 = 0x85ebca6bu;
        constexpr xyu::uint32 M2 = 0xc2b2ae35u;
        xyu::uint64 result = static_cast<xyu::uint64>(value ^ M1) * (value ^ M2);
        if constexpr (sizeof(xyu::size_t) == 8) return result ^ (result >> 32);
        else return static_cast<xyu::size_t>(result) ^ static_cast<xyu::size_t>(result >> 32);
    }

    // 16位整数
    template <typename T, xyu::t_enable<xyu::t_is_integral<T> && sizeof(T) == 2, int> = 2>
    constexpr xyu::size_t make_hash(T value) noexcept
    { return make_hash(value | (value << 16)); }

    // 8位整数
    template <typename T, xyu::t_enable<xyu::t_is_integral<T> && sizeof(T) == 1, int> = 1>
    constexpr xyu::size_t make_hash(T value) noexcept
    {
        xyu::uint32 v{};
        xyu::mem_set(&v, 4, value);
        return make_hash(v);
    }

    // 枚举类型
    template <typename T, xyu::t_enable<xyu::t_is_enum<T>, int> = 0>
    constexpr xyu::size_t make_hash(T value) noexcept
    { return make_hash(static_cast<xyu::t_get_enum_under<T>>(value)); }

    // nullptr
    constexpr xyu::size_t make_hash(xyu::nullptr_t) noexcept { return 0; }

    // 指针类型
    template <typename T>
    constexpr xyu::size_t make_hash(const T* const p) noexcept
    { return make_hash(static_cast<xyu::diff_t>(p)); }

    // 浮点数
    constexpr xyu::size_t make_hash(float value) noexcept
    {
        if (value == 0.0f) return 0;
        return make_hash(*reinterpret_cast<xyu::uint32*>(&value));
    }

    // 双精度浮点数
    constexpr xyu::size_t make_hash(double value) noexcept
    {
        if (value == 0.0) return 0;
        return make_hash(*reinterpret_cast<xyu::uint64*>(&value));
    }

    // 长双精度浮点数
    constexpr xyu::size_t make_hash(long double value) noexcept
    {
        //0
        if (value == 0.0l) return 0;
        //指数部分 2^m
        int exponent{};
        //尾数部分 0.xxxx
        value = xyu::calc_resolve(value, &exponent);
        value = value < 0.0l ? -(value + 0.5l) : value;
        //尽可能利用所有尾数
        constexpr long double mult = xyu::number_traits<xyu::size_t>::max + 1.0l;
        value *= mult;
        //尽可能利用剩余尾数
        auto hibits = static_cast<xyu::size_t>(value);
        value = (value - static_cast<long double>(hibits)) * mult;
        //利用指数
        xyu::size_t coeff = xyu::number_traits<xyu::size_t>::max / __LDBL_MAX_EXP__;
        return hibits + static_cast<xyu::size_t>(value) + coeff * exponent;
    }
}

#pragma clang diagnostic pop