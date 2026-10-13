#pragma once
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wignored-attributes"

/**
 * @file    xycore/__/simd.h
 * @brief   simd 操作
 * @date    2026-07-28
 */

#include "../log.h"
#include "../../xystring/fmt.h"

#if defined __i386__ || defined __x86_64__
#include <immintrin.h>
#elif defined __ARM_NEON__
# include <arm_neon.h>
#endif

// 对于通用方法:
// AVX2: 32字节
// SSE2/NEON: 16字节
// 退化: 16字节

/// 枚举
namespace xylu::xycore
{
    enum simd_Type : uint32
    {
        simd_None = 0,
        simd_Sse = 1 << 0,
        simd_Avx = 1 << 1,
        simd_Neon = 1 << 2,
        simd_Mask = -1u,
    };

    constexpr simd_Type operator|(simd_Type a, simd_Type b) noexcept
    {
        return static_cast<simd_Type>(static_cast<uint32>(a) | static_cast<uint32>(b));
    }
}

/// 宏生成
namespace
{
#if defined __i386__ || defined __x86_64__
    #define XY_AVX_OP_GEN(fun, ...) else if constexpr (options & simd_Avx) { return fun(__VA_ARGS__); }
    #define XY_SSE_OP_GEN(fun, ...) else if constexpr (options & simd_Sse) { return fun(__VA_ARGS__); }
    #define XY_AVX_T_GEN(fun, ...) else if constexpr (xyu::t_is_same<__m256i, M>) { return fun(__VA_ARGS__); }
    #define XY_SSE_T_GEN(fun, ...) else if constexpr (xyu::t_is_same<__m128i, M>) { return fun(__VA_ARGS__); }
    #define XY_AVX_T2_GEN(fun, ...) else if constexpr (xyu::t_is_same<__m256i, M1, M2>) { return fun(__VA_ARGS__); }
    #define XY_SSE_T2_GEN(fun, ...) else if constexpr (xyu::t_is_same<__m128i, M1, M2>) { return fun(__VA_ARGS__); }
#else
    #define XY_AVX_OP_GEN(fun, ...)
    #define XY_SSE_OP_GEN(fun, ...)
    #define XY_AVX_T_GEN(fun, ...)
    #define XY_SSE_T_GEN(fun, ...)
    #define XY_AVX_T2_GEN(fun, ...)
    #define XY_SSE_T2_GEN(fun, ...)
#endif

#if defined __ARM_NEON__
    #define XY_NEON_OP_GEN(fun, ...) else if constexpr (options & simd_Neon) { return fun(__VA_ARGS__); }
    #define XY_NEON_T_GEN(fun, ...) else if constexpr (xyu::t_is_same<uint8x16_t, M>) { return fun(__VA_ARGS__); }
    #define XY_NEON_T2_GEN(fun, ...) else if constexpr (xyu::t_is_same<uint8x16_t, M1, M2>) { return fun(__VA_ARGS__); }
#else
    #define XY_NEON_OP_GEN(fun, ...)
    #define XY_NEON_T_GEN(fun, ...)
    #define XY_NEON_T2_GEN(fun, ...)
#endif
}

/// 加载数据
namespace xylu::xycore
{
    // simd 长度
    template <simd_Type options>
    constexpr uint simd_size() noexcept
    {
        if constexpr (false) {}
        XY_AVX_OP_GEN(, 32)
        XY_SSE_OP_GEN(, 16)
        XY_NEON_OP_GEN(, 16)
        else return 0;
    }

    // 对齐加载
    template <simd_Type options>
    decltype(auto) simd_load(const void* data) noexcept
    {
        if constexpr (false) {}
        XY_AVX_OP_GEN(_mm256_load_si256, (const __m256i*)data)
        XY_SSE_OP_GEN(_mm_load_si128, (const __m128i*)data)
        XY_NEON_OP_GEN(vld1q_u8, (const xyu::uint8*)data)
    }

    // 非对齐加载
    template <simd_Type options>
    decltype(auto) simd_loadu(const void* data) noexcept
    {
        if constexpr (false) {}
        XY_AVX_OP_GEN(_mm256_loadu_si256, (const __m256i*)data)
        XY_SSE_OP_GEN(_mm_loadu_si128, (const __m128i*)data)
        XY_NEON_OP_GEN(vld1q_u8, (const xyu::uint8*)data)
    }

    // 字节填充
    template <simd_Type options>
    decltype(auto) simd_fill(char c) noexcept
    {
        if constexpr (false) {}
        XY_AVX_OP_GEN(_mm256_set1_epi8, c)
        XY_SSE_OP_GEN(_mm_set1_epi8, c)
        XY_NEON_OP_GEN(vdupq_n_u8, (xyu::uint8)c)
    }
    
    // uint16 填充
    template <simd_Type options>
    decltype(auto) simd_fill(uint16 v) noexcept
    {
        if constexpr (false) {}
        else if (XY_BIG_ENDIAN) {}
        XY_AVX_OP_GEN(_mm256_set1_epi16, v)
        XY_SSE_OP_GEN(_mm_set1_epi16, v)
        XY_NEON_OP_GEN(vdupq_n_u16, v)
    }

    // uint32 填充
    template <simd_Type options>
    decltype(auto) simd_fill(uint32 v) noexcept
    {
        if constexpr (false) {}
        else if (XY_BIG_ENDIAN) {}
        XY_AVX_OP_GEN(_mm256_set1_epi32, v)
        XY_SSE_OP_GEN(_mm_set1_epi32, v)
        XY_NEON_OP_GEN(vdupq_n_u32, v)
    }
}

/// 保存数据
namespace xylu::xycore
{
    // 对齐保存
    template <typename M>
    void simd_store(M m, void* dst) noexcept
    {
        if constexpr (false) {}
        XY_AVX_T_GEN(_mm256_store_si256, (__m256i*)dst, m)
        XY_SSE_T_GEN(_mm_store_si128, (__m128i*)dst, m)
        XY_NEON_T_GEN(vst1q_u8, (uint8*)dst, m)
        else static_assert(xyu::t_is_void<M>, "unsupport type");
    }

    // 非对齐保存
    template <typename M>
    void simd_storeu(M m, void* dst) noexcept
    {
        if constexpr (false) {}
        XY_AVX_T_GEN(_mm256_storeu_si256, (__m256i*)dst, m)
        XY_SSE_T_GEN(_mm_storeu_si128, (__m128i*)dst, m)
        XY_NEON_T_GEN(vst1q_u8, (uint8*)dst, m)
        else static_assert(xyu::t_is_void<M>, "unsupport type");
    }
}

/// 比较
namespace xylu::xycore
{
#define XY_SIMD_CMP_GEN( name, op ) \
    template <typename M1, typename M2> \
    decltype(auto) simd_##name(M1 m1, M2 m2) noexcept \
    { \
        if constexpr (false) {} \
        XY_AVX_T2_GEN(_mm256_cmp##name##_epi8, m1, m2) \
        XY_SSE_T2_GEN(_mm_cmp##name##_epi8, m1, m2) \
        XY_NEON_T2_GEN(vc##name##q_s8, vreinterpretq_s8_u8(m1), vreinterpretq_s8_u8(m2)) \
        else static_assert(xyu::t_is_void<M1, M2>, "unsupport type"); \
    }
    // 逐字节有符号比较
    XY_PP_FORWARD(XY_SIMD_CMP_GEN, 2, eq,==, gt,>)
#undef XY_SIMD_CMP_GEN
}

/// 位压缩
namespace xylu::xycore
{
    // 压缩最高有效位
    template <typename M>
    decltype(auto) simd_msb(M m) noexcept
    {
        if constexpr (false) {}
#if defined __i386__ || defined __x86_64__
        else if constexpr (xyu::t_is_same<__m256i, M>) return (uint32)_mm256_movemask_epi8(m);
        else if constexpr (xyu::t_is_same<__m128i, M>) return (uint16)_mm_movemask_epi8(m);
#elif defined __ARM_NEON__
        else if constexpr (xyu::t_is_same<uint8x16_t, M>)
        {
            // 1. 提取每字节最高位（MSB）到最低位
            uint16x8_t high_bits = vreinterpretq_u16_u8(vshrq_n_u8(m, 7));
            // 2. 三级移位累加，将分散的位压缩到特定字节
            uint32x4_t paired16 = vreinterpretq_u32_u16(vsraq_n_u16(high_bits, high_bits, 7));
            uint64x2_t paired32 = vreinterpretq_u64_u32(vsraq_n_u32(paired16, paired16, 14));
            uint8x16_t paired64 = vreinterpretq_u8_u64(vsraq_n_u64(paired32, paired32, 28));
            // 3. 从字节 0 和字节 8 拼出最终 16 位掩码
            return (uint16)(vgetq_lane_u8(paired64, 0) | ((uint)vgetq_lane_u8(paired64, 8) << 8));
        }
#endif
        else static_assert(xyu::t_is_void<M>, "unsupport type");
    }
}

/// 位运算
namespace xylu::xycore
{
#define XY_SIMD_BIT_GEN( name, name_neno, op ) \
    template <typename M1, typename M2> \
    decltype(auto) simd_##name(M1 m1, M2 m2) noexcept \
    { \
        if constexpr (false) {} \
        XY_AVX_T2_GEN(_mm256_##name##_si256, m1, m2) \
        XY_SSE_T2_GEN(_mm_##name##_si128, m1, m2) \
        XY_NEON_T2_GEN(v##name_neno##q_u8, m1, m2) \
        else static_assert(xyu::t_is_void<M1, M2>, "unsupport type"); \
    }
    // 按位 与或异或
    XY_PP_FORWARD(XY_SIMD_BIT_GEN, 3, and,and,&, or,orr,|, xor,eor,^)
#undef XY_SIMD_BIT_GEN

    // 按位取反
    template <typename M>
    decltype(auto) simd_not(M m) noexcept
    {
        if constexpr (false) {}
        XY_AVX_T_GEN(_mm256_xor_si256, m, _mm256_set1_epi8(-1))
        XY_SSE_T_GEN(_mm_xor_si128, m, _mm_set1_epi8(-1))
        XY_NEON_T_GEN(vmvnq_u8, m)
        else static_assert(xyu::t_is_void<M>, "unsupport type");
    }

#define XY_SIMD_RSHIFT_GEN( blocksize ) \
    template <typename M> \
    decltype(auto) simd_rshift##blocksize(M m, uint bits) noexcept \
    { \
        if constexpr (false) {} \
        XY_AVX_T_GEN(_mm256_srli_epi##blocksize, m, bits) \
        XY_SSE_T_GEN(_mm_srli_epi##blocksize, m, bits) \
        XY_NEON_T_GEN(vshrq_n_u##blocksize, m, bits) \
        else static_assert(xyu::t_is_void<M>, "unsupport type"); \
    }
    // uint16/32/64快内逻辑右移
    XY_PP_FORWARD(XY_SIMD_RSHIFT_GEN, 1, 16, 32, 64)
#undef XY_SIMD_RSHIFT_GEN

    // uint8快内逻辑右移
    template <typename M>
    decltype(auto) simd_rshift8(M m, uint bits) noexcept
    {
        if constexpr (false) {}
#if defined __i386__ || defined __x86_64__
        else if constexpr (xyu::t_is_same_one<M, __m256i, __m128i>)
        {
            constexpr simd_Type option = xyu::t_is_same<M, __m256i> ? simd_Avx : simd_Sse;
            if (XY_UNLIKELY(bits >= 8)) return simd_fill<option>('\0');
            return simd_and(simd_rshift16(m, bits), simd_fill<option>((char)(0xffu >> bits)));
        }
#endif
        XY_NEON_T_GEN(vshrq_n_u8, m, bits)
        else static_assert(xyu::t_is_void<M>, "unsupport type");
    }

#define XY_SIMD_LSHIFT_GEN( blocksize ) \
    template <typename M> \
    decltype(auto) simd_lshift##blocksize(M m, uint bits) noexcept \
    { \
        if constexpr (false) {} \
        XY_AVX_T_GEN(_mm256_slli_epi##blocksize, m, bits) \
        XY_SSE_T_GEN(_mm_slli_epi##blocksize, m, bits) \
        XY_NEON_T_GEN(vshlq_n_u##blocksize, m, bits) \
        else static_assert(xyu::t_is_void<M>, "unsupport type"); \
    }
    // uint16/32/64快内左移
    XY_PP_FORWARD(XY_SIMD_LSHIFT_GEN, 1, 16, 32, 64)
#undef XY_SIMD_LSHIFT_GEN

    // uint8快内左移
    template <typename M>
    decltype(auto) simd_lshift8(M m, uint bits) noexcept
    {
        if constexpr (false) {}
#if defined __i386__ || defined __x86_64__
        else if constexpr (xyu::t_is_same_one<M, __m256i, __m128i>)
        {
            constexpr simd_Type option = xyu::t_is_same<M, __m256i> ? simd_Avx : simd_Sse;
            if (XY_UNLIKELY(bits >= 8)) return simd_fill<option>>('\0');
            return simd_and(simd_lshift16(m, bits), simd_fill<option>((char)(0xffu << bits)));
        }
#endif
        XY_NEON_T_GEN(vshlq_n_u8, m, bits)
        else static_assert(xyu::t_is_void<M>, "unsupport type");
    }

}

/// 位顺序
namespace xylu::xycore
{
    // 全字节反转 B1..B16 -> B16..B1
    template <typename M>
    decltype(auto) simd_reverse1(M m) noexcept
    {
        if constexpr (false) {}
#if defined __i386__ || defined __x86_64__
        else if constexpr (xyu::t_is_same<__m256i, M>)
        {
            // _mm256_shuffle_epi8 不能跨 128 位通道
            const auto idx = _mm256_set_epi8(16,17,18,19,20,21,22,23,24,25,26,27,28,29,30,31, 0,1,2,3,4,5,6,7,8,9,10,11,12,13,14,15);
            return _mm256_permute4x64_epi64(_mm256_shuffle_epi8(m, idx), 0x4E);
        }
        else if constexpr (xyu::t_is_same<__m128i, M>)
        {
            const auto idx = _mm_set_epi8(0,1,2,3,4,5,6,7,8,9,10,11,12,13,14,15);
            return _mm_shuffle_epi8(m, idx);
        }
#elif defined __ARM_NEON__
        else if constexpr (xyu::t_is_same<uint8x16_t, M>)
        {
            uint8x16_t rev = vrev64q_u8(m);
            return vcombine_u8(vget_high_u8(rev), vget_low_u8(rev));
        }
#endif
        else static_assert(xyu::t_is_void<M>, "unsupport type");
    }

    // 反转uint32块顺序 B1..B4 ... B12..B16 -> B12..B16 ... B1..B4
    template <typename M>
    decltype(auto) simd_reverse4(M m) noexcept
    {
        if constexpr (false) {}
#if defined __i386__ || defined __x86_64__
        else if constexpr (xyu::t_is_same<__m256i, M>)
        {
            const __m256i idx = _mm256_set_epi32(0, 1, 2, 3, 4, 5, 6, 7);
            return _mm256_permutevar8x32_epi32(m, idx);
        }
        else if constexpr (xyu::t_is_same<__m128i, M>)
        { return _mm_shuffle_epi32(m, _MM_SHUFFLE(0, 1, 2, 3)); }
#elif defined __ARM_NEON__
        else if constexpr (xyu::t_is_same<uint8x16_t, M>)
        {
            uint32x4_t val32 = vreinterpretq_u32_u8(m);
            uint32x4_t rev64 = vrev64q_u32(val32);
            uint32x4_t rev = vcombine_u32(vget_high_u32(rev64), vget_low_u32(rev64));
            return vreinterpretq_u8_u32(rev);
        }
#endif
        else static_assert(xyu::t_is_void<M>, "unsupport type");
    }

    // 反转uint64块顺序 B1..B8 B9..B16 -> B9..B16 B1..B8
    template <typename M>
    decltype(auto) simd_reverse8(M m) noexcept
    {
        if constexpr (false) {}
#if defined __i386__ || defined __x86_64__
        else if constexpr (xyu::t_is_same<__m256i, M>)
        { return _mm256_permute4x64_epi64(m, _MM_SHUFFLE(0, 1, 2, 3)); }
        else if constexpr (xyu::t_is_same<__m128i, M>)
        { return _mm_shuffle_epi32(m, _MM_SHUFFLE(1, 0, 3, 2)); }
#elif defined __ARM_NEON__
        // ARM NEON 平台
        else if constexpr (xyu::t_is_same<uint8x16_t, M>)
        {
            uint64x2_t val64 = vreinterpretq_u64_u8(m);
            uint64x2_t swapped = vcombine_u64(vget_high_u64(val64), vget_low_u64(val64));
            return vreinterpretq_u8_u64(swapped);
        }
#endif
        else static_assert(xyu::t_is_void<M>, "unsupport type");
    }

    // uint32块内部交换 B1..B4 ... B12..B16 -> B4..B1 ... B16..B12
    template <typename M>
    decltype(auto) simd_swap4(M m) noexcept
    {
        if constexpr (false) {}
#if defined __i386__ || defined __x86_64__
        else if constexpr (xyu::t_is_same<__m256i, M>)
        {
            const __m256i idx = _mm256_set_epi8(28,29,30,31, 24,25,26,27, 20,21,22,23, 16,17,18,19, 12,13,14,15, 8,9,10,11, 4,5,6,7, 0,1,2,3);
            return _mm256_shuffle_epi8(m, idx);
        }
        else if constexpr (xyu::t_is_same<__m128i, M>)
        {
            const __m128i idx = _mm_set_epi8(12,13,14,15, 8,9,10,11, 4,5,6,7, 0,1,2,3);
            return _mm_shuffle_epi8(m, idx);
        }
#elif defined __ARM_NEON__
        else if constexpr (xyu::t_is_same<uint8x16_t, M>) return vrev32q_u8(m);
#endif
        else static_assert(xyu::t_is_void<M>, "unsupport type");
    }

    // uint64块内部交换 B1..B8 B9..B16 -> B8..B1 B16..B9
    template <typename M>
    decltype(auto) simd_swap8(M m) noexcept
    {
        if constexpr (false) {}
#if defined __i386__ || defined __x86_64__
        else if constexpr (xyu::t_is_same<__m256i, M>)
        {
            const __m256i idx = _mm256_set_epi8(24,25,26,27,28,29,30,31, 16,17,18,19,20,21,22,23, 8,9,10,11,12,13,14,15, 0,1,2,3,4,5,6,7);
            return _mm256_shuffle_epi8(m, idx);
        }
        else if constexpr (xyu::t_is_same<__m128i, M>)
        {
            const __m128i idx = _mm_set_epi8(8,9,10,11,12,13,14,15, 0,1,2,3,4,5,6,7);
            return _mm_shuffle_epi8(m, idx);
        }
#elif defined __ARM_NEON__
        else if constexpr (xyu::t_is_same<uint8x16_t, M>) return vrev64q_u8(m);
#endif
        else static_assert(xyu::t_is_void<M>, "unsupport type");
    }

}

/// 算数运算
namespace xylu::xycore
{
    // 1字节加法
    template <typename M1, typename M2>
    decltype(auto) simd_add1(M1 m1, M2 m2) noexcept
    {
        if constexpr (false) {}
        XY_AVX_T2_GEN(_mm256_add_epi8, m1, m2)
        XY_SSE_T2_GEN(_mm_add_epi8, m1, m2)
        XY_NEON_T2_GEN(vaddq_u8, m1, m2)
        else static_assert(xyu::t_is_void<M1, M2>, "unsupport type");
    }

    // 8字节加法
    template <typename M1, typename M2>
    decltype(auto) simd_add8(M1 m1, M2 m2) noexcept
    {
        if constexpr (false) {}
        XY_AVX_T2_GEN(_mm256_add_epi64, m1, m2)
        XY_SSE_T2_GEN(_mm_add_epi64, m1, m2)
        XY_NEON_T2_GEN(vaddq_u64, m1, m2)
        else static_assert(xyu::t_is_void<M1, M2>, "unsupport type");
    }

    // 1字节减法
    template <typename M1, typename M2>
    decltype(auto) simd_sub1(M1 m1, M2 m2) noexcept
    {
        if constexpr (false) {}
        XY_AVX_T2_GEN(_mm256_sub_epi8, m1, m2)
        XY_SSE_T2_GEN(_mm_sub_epi8, m1, m2)
        XY_NEON_T2_GEN(vsubq_u8, m1, m2)
        else static_assert(xyu::t_is_void<M1, M2>, "unsupport type");
    }

    // 8字节减法
    template <typename M1, typename M2>
    decltype(auto) simd_sub8(M1 m1, M2 m2) noexcept
    {
        if constexpr (false) {}
        XY_AVX_T2_GEN(_mm256_sub_epi64, m1, m2)
        XY_SSE_T2_GEN(_mm_sub_epi64, m1, m2)
        XY_NEON_T2_GEN(vsubq_u64, m1, m2)
        else static_assert(xyu::t_is_void<M1, M2>, "unsupport type");
    }
}

/// 其他
namespace xylu::xycore
{
    // 低4位(16字节)映射
    template <typename M1, typename M2>
    decltype(auto) simd_shuffle(M1 table, M2 indexs) noexcept
    {
        if constexpr (false) {}
        XY_AVX_T2_GEN(_mm256_shuffle_epi8, table, indexs)
        XY_SSE_T2_GEN(_mm_shuffle_epi8, table, indexs)
        XY_NEON_T2_GEN(vqtbl1q_u8, table, indexs)
        else static_assert(xyu::t_is_void<M1, M2>, "unsupport type");
    }
}

/// 宏清除
namespace
{
    #undef XY_AVX_OP_GEN
    #undef XY_AVX_T_GEN
    #undef XY_AVX_T2_GEN
    #undef XY_SSE_OP_GEN
    #undef XY_SSE_T_GEN
    #undef XY_SSE_T2_GEN
    #undef XY_NEON_OP_GEN
    #undef XY_NEON_T_GEN
    #undef XY_NEON_T2_GEN
}

#pragma GCC diagnostic pop