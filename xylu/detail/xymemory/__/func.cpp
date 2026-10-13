#include "../func.h"

#include "../../xymath/bit.h"
#include "../../xycore/__/simd.h"

using namespace xyu;

namespace
{
    constexpr simd_Type option = simd_Avx | simd_Sse | simd_Neon;
    constexpr size_t BlockSize = simd_size<option>();
}

namespace xylu::xymemory::__
{
    // memchr 可能未启用 avx
    XY_NO_VECTORIZE_GCC size_t find(const char* src, char c, size_t len) noexcept
    {
        // 1.处理最前面几个非整块对齐的字符
        if (XY_UNLIKELY(len == 0)) return -1;
        auto ap = (char*)(((size_t)src + (BlockSize-1)) & ~(BlockSize-1));
        auto ep = src + len, sp = src;
        #pragma clang loop vectorize(disable)
        for (; sp < min(ap, ep); ++sp) if (*sp == c) return sp - src;
        if (ap >= ep) return -1;

        // 2.每次处理一块
        const auto match = simd_fill<option>(c);
        for (; sp + BlockSize <= ep; sp += BlockSize)
        {
            auto potential = simd_msb(simd_eq(simd_load<option>(sp), match));
            if (potential) return sp - src + bit_count_0_low(potential);
        }

        // 3.处理剩余不足一块的字符
        #pragma clang loop vectorize(disable)
        for (; sp < ep; ++sp) if (*sp == c) return sp - src;
        return -1;
    }

    // memcmp 可能未启用 avx
    XY_NO_VECTORIZE_GCC size_t contrast(const char* src, const char* dst, size_t len) noexcept
    {
        // 1.处理最前面几个非整块对齐的字符
        if (XY_UNLIKELY(len == 0)) return 0;
        auto ap = (const char*)(((size_t)src + (BlockSize-1)) & ~(BlockSize-1));
        auto ep = src + len, sp = src, dp = dst;
        #pragma clang loop vectorize(disable)
        for (; sp < min(ap, ep); ++sp, ++dp) if (*sp != *dp) return sp - src;
        if (ap >= ep) return len;

        // 2.每次处理一块
        while (sp + BlockSize <= ep)
        {
            auto potential = simd_msb(simd_eq(simd_load<option>(sp), simd_loadu<option>(dp)));
            if (potential != decltype(potential)(-1)) return sp - src + bit_count_1_low(potential);
            sp += BlockSize;
            dp += BlockSize;
        }

        // 3.处理剩余不足一块的字符
        #pragma clang loop vectorize(disable)
        for (; sp < ep; ++sp, ++dp) if (*sp != *dp) return sp - src;
        return len;
    }
}
