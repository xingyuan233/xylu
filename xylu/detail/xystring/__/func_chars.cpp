#include "../strview.h"

#include "../../xymath/bit.h"
#include "../../xycore/__/simd.h"

using namespace xyu;

/// 辅助函数
namespace
{
    constexpr simd_Type option = simd_Avx | simd_Sse | simd_Neon;
    constexpr size_t BlockSize = simd_size<option>();

    // 回调: lf(p), sf(potential[匹配位为1其余为0], p), rf()
    template <bool step_return, typename LoopFunc, typename SimdFunc, typename ReturnFunc> XY_NO_VECTORIZE_GCC
    decltype(auto) string_help_simd(const char* str, size_t slen, char c, LoopFunc&& lf, SimdFunc&& sf, ReturnFunc&& rf) noexcept
    {
        // 1.处理最前面几个非整块对齐的字符
        auto ap = (const char*)(((size_t)str + (BlockSize-1)) & ~(BlockSize-1));
        auto ep = str + slen, p = str;
        #pragma clang loop vectorize(disable)
        for (; p < min(ap, ep); ++p)
        {
            if constexpr (!step_return) lf(p);
            else if (lf(p)) return rf();
        }
        if (ap >= ep) return rf();

        // 2.每次处理一块
        const auto match = simd_fill<option>(c);
        while (p + BlockSize <= ep)
        {
            auto block = simd_load<option>(p);
            auto potential = simd_msb(simd_eq(block, match));
            if constexpr (!step_return) sf(potential, p);
            else if (sf(potential, p)) return rf();
            p += BlockSize;
        }

        // 3.处理剩余不足一块的字符
        #pragma clang loop vectorize(disable)
        for (; p < ep; ++p)
        {
            if constexpr (!step_return) lf(p);
            else if (lf(p)) return rf();
        }
        return rf();
    }

    // 回调: lf(p), sf(potential[匹配位为1其余为0], p), rf()
    template <typename LoopFunc, typename SimdFunc, typename ReturnFunc> XY_NO_VECTORIZE_GCC
    decltype(auto) string_help_simd_r(const char* str, size_t slen, char c, LoopFunc&& lf, SimdFunc&& sf, ReturnFunc&& rf) noexcept
    {
        // 1.处理最后面不足一块的字符
        auto ep = str + slen, p = ep;
        auto ap = (const char*)((size_t)ep & ~(BlockSize - 1));
        #pragma clang loop vectorize(disable)
        while (p > max(ap, str)) if (lf(--p)) return rf();
        if (ap <= str) return rf();

        // 2.每次处理一块
        const auto match = simd_fill<option>(c);
        while (p >= str + BlockSize)
        {
            p -= BlockSize;
            auto block = simd_load<option>(p);
            auto potential = simd_msb(simd_eq(block, match));
            if (sf(potential, p)) return rf();
        }

        // 3.处理剩余不足一块的字符
        #pragma clang loop vectorize(disable)
        while (p > str) if (lf(--p)) return rf();
        return rf();
    }
}

/// findn
namespace xylu::xystring::__
{
    static size_t findn_simd(const char* str, size_t slen, char c) noexcept
    {
        size_t index = -1;
        return string_help_simd<true>(str, slen, c,
            [&](auto p)
            {
                if (*p == c) return false;
                index = p - str;
                return true;
            },
            [&](auto potential, auto p)
            {
                if (potential == decltype(potential)(-1)) return false;
                index = p - str + bit_count_1_low(potential);
                return true;
            },
            [&]{ return index; });
    }

    XY_NO_VECTORIZE_GCC
    static size_t findn_easy(const char* str, size_t slen, char c) noexcept
    {
        #pragma clang loop vectorize(disable)
        for (size_t i = 0; i < slen; ++i) if (str[i] != c) return i;
        return -1;
    }

    size_t findn(const char* str, size_t slen, char c) noexcept
    {
        // slen > 0
#if defined(__AVX2__) || defined(__SSE2__) || defined(__ARM_NEON__)
        if (slen >= simd_size<option>() * 2) return findn_simd(str, slen, c);
#endif
        return findn_easy(str, slen, c);
    }
}

/// rfind
namespace xylu::xystring::__
{
    static size_t rfind_simd(const char* str, size_t slen, char c) noexcept
    {
        size_t index = -1;
        return string_help_simd_r(str, slen, c,
            [&](auto p)
            {
                if (*p != c) return false;
                index = p - str;
                return true;
            },
            [&](auto potential, auto p)
            {
                if (potential == 0) return false;
                index = p - str + (sizeof(potential)*8-1) - bit_count_0_high(potential);
                return true;
            },
            [&]{ return index; });
    }

    XY_NO_VECTORIZE_GCC
    static size_t rfind_easy(const char* str, size_t slen, char c) noexcept
    {
        #pragma clang loop vectorize(disable)
        for (size_t i = slen; i > 0;) if (str[--i] == c) return i;
        return -1;
    }

    size_t rfind(const char* str, size_t slen, char c) noexcept
    {
        // slen > 0
#if defined(__AVX2__) || defined(__SSE2__) || defined(__ARM_NEON__)
        if (slen >= simd_size<option>() * 2) return rfind_simd(str, slen, c);
#endif
        return rfind_easy(str, slen, c);
    }
}

/// rfindn
namespace xylu::xystring::__
{
    static size_t rfindn_simd(const char* str, size_t slen, char c) noexcept
    {
        size_t index = -1;
        return string_help_simd_r(str, slen, c,
            [&](auto p)
            {
                if (*p == c) return false;
                index = p - str;
                return true;
            },
            [&](auto potential, auto p)
            {
                if (potential == decltype(potential)(-1)) return false;
                index = p - str + (sizeof(potential)*8-1) - bit_count_1_high(potential);
                return true;
            },
            [&]{ return index; });
    }

    XY_NO_VECTORIZE_GCC
    static size_t rfindn_easy(const char* str, size_t slen, char c) noexcept
    {
        #pragma clang loop vectorize(disable)
        for (size_t i = slen; i > 0;) if (str[--i] != c) return i;
        return -1;
    }

    size_t rfindn(const char* str, size_t slen, char c) noexcept
    {
        // slen > 0
#if defined(__AVX2__) || defined(__SSE2__) || defined(__ARM_NEON__)
        if (slen >= simd_size<option>() * 2) return rfindn_simd(str, slen, c);
#endif
        return rfindn_easy(str, slen, c);
    }
}

/// tally
namespace xylu::xystring::__
{
    static size_t tally_simd(const char* str, size_t slen, char c) noexcept
    {
        size_t times = 0;
        return string_help_simd<false>(str, slen, c,
            [&](auto p){ if (*p == c) ++times; },
            [&](auto potential, auto){ times += bit_count_1_total(potential); },
            [&]{ return times; });
    }

    XY_NO_VECTORIZE_GCC
    static size_t tally_easy(const char* str, size_t slen, char c) noexcept
    {
        size_t times = 0;
        #pragma clang loop vectorize(disable)
        for (size_t i = 0; i < slen; ++i) if (str[i] == c) ++times;
        return times;
    }

    size_t tally(const char* str, size_t slen, char c) noexcept
    {
        // slen > 0
#if defined(__AVX2__) || defined(__SSE2__) || defined(__ARM_NEON__)
        if (slen >= simd_size<option>() * 2) return tally_simd(str, slen, c);
#endif
        return tally_easy(str, slen, c);
    }
}

/// tallyn
namespace xylu::xystring::__
{
    static size_t tallyn_simd(const char* str, size_t slen, char c) noexcept
    {
        size_t times = 0;
        return string_help_simd<false>(str, slen, c,
            [&](auto p){ if (*p != c) ++times; },
            [&](auto potential, auto){ times += bit_count_0_total(potential); },
            [&]{ return times; });
    }

    XY_NO_VECTORIZE_GCC
    static size_t tallyn_easy(const char* str, size_t slen, char c) noexcept
    {
        size_t times = 0;
        #pragma clang loop vectorize(disable)
        for (size_t i = 0; i < slen; ++i) if (str[i] != c) ++times;
        return times;
    }

    size_t tallyn(const char* str, size_t slen, char c) noexcept
    {
        // slen > 0
#if defined(__AVX2__) || defined(__SSE2__) || defined(__ARM_NEON__)
        if (slen >= simd_size<option>() * 2) return tallyn_simd(str, slen, c);
#endif
        return tallyn_easy(str, slen, c);
    }
}

/// 辅助函数
namespace
{
    alignas(BlockSize) constexpr uint8 table_high[32] {1,2,4,8,16,32,64,128, 1,2,4,8,16,32,64,128, 1,2,4,8,16,32,64,128, 1,2,4,8,16,32,64,128};

    template <bool converse = false> XY_NO_VECTORIZE_GCC
    void string_help_easy_table(bool(&set)[256], StringView cs) noexcept
    {
        mem_set(set, converse, 256);
        #pragma clang loop vectorize(disable)
        for (uint8 c : cs.range()) set[c] = !converse;
    }

    template <bool step_return, bool converse = false, bool reverse = false, typename SimdFunc> XY_NO_VECTORIZE_GCC
    bool string_help_simd_foreach(const uint8*& p, const uint8* ep, StringView cs, SimdFunc&& sf) noexcept
    {
        // 创建匹配向量组
        decltype(simd_fill<option>('0')) matches[cs.size()];
        #pragma clang loop vectorize(disable)
        for (uint i = 0; i < cs.size(); ++i) matches[i] = simd_fill<option>(cs[i]);

        // 每次处理一块
        while (reverse ? (p >= ep + BlockSize) : (p + BlockSize <= ep))
        {
            if constexpr (reverse) p -= BlockSize;
            auto block = simd_load<option>(p);
            auto tmp = simd_eq(block, matches[0]);
            for (uint i = 1; i < cs.size(); ++i) tmp = simd_or(simd_eq(block, matches[i]), tmp);
            auto potential = simd_msb(tmp);
            if constexpr (converse) potential = ~potential;
            if constexpr (!step_return) sf(potential, p);
            else if (sf(potential, p)) return true;
            if constexpr (!reverse) p += BlockSize;
        }

        return false;
    }

    template <bool step_return, bool converse = false, bool reverse = false, typename SimdFunc> XY_NO_VECTORIZE_GCC
    bool string_help_simd_ascii_table(const uint8*& p, const uint8* ep, StringView cs, SimdFunc&& sf) noexcept
    {
        alignas(BlockSize) uint8 table_low[BlockSize]{};
        #pragma clang loop vectorize(disable)
        for (uint8 c : cs.range()) table_low[c & 0xf] |= 1 << (c >> 4);
        if constexpr (BlockSize == 32) mem_copy(table_low + 16, table_low, 16);

        auto tlow = simd_load<option>(table_low);
        auto thigh = simd_load<option>(table_high);

        while (reverse ? (p >= ep + BlockSize) : (p + BlockSize <= ep))
        {
            if constexpr (reverse) p -= BlockSize;
            auto block = simd_load<option>(p);
            auto high = simd_rshift8(block, 4);
            // neon 会对高4字节进行归0检测，而 sse/avx 仅对最高字节，可作为非 ascii 过滤（尽管不会被传递）
            if constexpr (option == simd_Neon) block = simd_and(simd_fill<option>('\x8f'), block);
            // match = table_low[c & 0x0f] & table_high[c >> 4]
            auto match = simd_and(simd_shuffle(tlow, block), simd_shuffle(thigh, high));
            auto potential = simd_msb(simd_eq(match, simd_fill<option>('\0')));

            if constexpr (!converse) potential = ~potential;
            if constexpr (!step_return) sf(potential, p);
            else if (sf(potential, p)) return true;
            if constexpr (!reverse) p += BlockSize;
        }

        return false;
    }

    template <bool step_return, bool converse = false, bool reverse = false, typename SimdFunc> XY_NO_VECTORIZE_GCC
    bool string_help_simd_expand_table(const uint8*& p, const uint8* ep, StringView cs, SimdFunc&& sf) noexcept
    {
        alignas(BlockSize) uint8 table_low1[BlockSize]{}, table_low2[BlockSize]{};
        #pragma clang loop vectorize(disable)
        for (uint8 c : cs.range()) (c & 0x80 ? table_low2 : table_low1)[c & 15] |= table_high[c >> 4];
        if constexpr (BlockSize == 32) mem_copy(table_low1 + 16, table_low1, 16);
        if constexpr (BlockSize == 32) mem_copy(table_low2 + 16, table_low2, 16);

        auto tlow1 = simd_load<option>(table_low1);
        auto tlow2 = simd_load<option>(table_low2);
        auto thigh = simd_load<option>(table_high);

        while (reverse ? (p >= ep + BlockSize) : (p + BlockSize <= ep))
        {
            if constexpr (reverse) p -= BlockSize;
            auto block = simd_load<option>(p);
            auto olow = simd_and(block, simd_fill<option>('\x0f'));
            auto mhigh = simd_shuffle(thigh, simd_rshift8(block, 4));
            // choose = c > -1 ? 1 : 0 <==> c & 0x80 ? 0 : 1
            auto choose = simd_gt(block, simd_fill<option>('\xff'));
            // mlow1 = table_low1[c & 0x0f]
            auto mlow1 = simd_shuffle(tlow1, olow);
            // mlow2 = table_low2[c & 0x0f]
            auto mlow2 = simd_shuffle(tlow2, olow);
            // (choose & (mlow1 & mhigh)) | (~choose & (mlow2 & mhigh))
            // <==> mhigh & (mlow2 ^ ((mlow2 ^ mlow1) & choose))
            auto mlow = simd_xor(mlow2, simd_and(simd_xor(mlow2, mlow1), choose));
            auto potential = simd_msb(simd_eq(simd_and(mlow, mhigh), simd_fill<option>('\0')));

            if constexpr (!converse) potential = ~potential;
            if constexpr (!step_return) sf(potential, p);
            else if (sf(potential, p)) return true;
            if constexpr (!reverse) p += BlockSize;
        }

        return false;
    }

    // 回调: lf(p), sf(potential[匹配位为1其余为0], p), rf()
    template <bool step_return, bool converse = false, typename LoopFunc, typename SimdFunc, typename ReturnFunc> XY_NO_VECTORIZE_GCC
    decltype(auto) string_help_simd(const uint8* str, size_t slen, StringView cs, LoopFunc&& lf, SimdFunc&& sf, ReturnFunc&& rf) noexcept
    {
        // 1.预处理字符集
        bool expand = false;
        bool set[256]{};
        if constexpr (converse) mem_set(set, char(true), sizeof(set));
        #pragma clang loop vectorize(disable)
        for (uint8 c : cs.range()) set[c] = !converse, expand |= c & 0x80;

        // 2.将 最前面几个 非整块对齐 的字符 单独处理
        auto ap = (const uint8*)(((size_t)str + (BlockSize-1)) & ~(BlockSize-1));
        auto ep = str + slen, p = str;
        #pragma clang loop vectorize(disable)
        for (; p < min(ap, ep); ++p)
        {
            if constexpr (!step_return) lf(set[*p], p);
            else if (lf(set[*p], p)) return rf();
        }
        if (ap >= ep) return rf();

        // 3.simd 处理整块
        bool ret;
        if (slen * cs.size() < 512 + (expand ? 512 : 0)) ret = string_help_simd_foreach<step_return, converse>(p, ep, cs, sf);
        else if (!expand) ret = string_help_simd_ascii_table<step_return, converse>(p, ep, cs, sf);
        else ret = string_help_simd_expand_table<step_return, converse>(p, ep, cs, sf);
        if (ret) return rf();

        // 4.处理剩余不足一块的字符
        #pragma clang loop vectorize(disable)
        for (; p < ep; ++p)
        {
            if constexpr (!step_return) lf(set[*p], p);
            else if (lf(set[*p], p)) return rf();
        }
        return rf();
    }

    // 回调: lf(p), sf(potential[匹配位为1其余为0], p), rf()
    template <typename LoopFunc, typename SimdFunc, typename ReturnFunc> XY_NO_VECTORIZE_GCC
    decltype(auto) string_help_simd_r(const uint8* str, size_t slen, StringView cs, LoopFunc&& lf, SimdFunc&& sf, ReturnFunc&& rf) noexcept
    {
        // 1.预处理字符集
        bool expand = false;
        bool set[256]{};
        #pragma clang loop vectorize(disable)
        for (uint8 c : cs.range()) set[c] = true, expand |= c & 0x80;

        // 2.将 最后面几个 非整块对齐 的字符 单独处理
        auto sp = str + slen, p = sp;
        auto ap = (const uint8*)((size_t)sp & ~(BlockSize - 1));
        while (p > max(ap, str)) if (--p, lf(set[*p], p)) return rf();
        if (ap <= str) return rf();

        // 3.simd 处理整块
        bool ret;
        if (slen * cs.size() < 512 + (expand ? 512 : 0)) ret = string_help_simd_foreach<true,false,true>(p, str, cs, sf);
        else if (!expand) ret = string_help_simd_ascii_table<true,false,true>(p, str, cs, sf);
        else ret = string_help_simd_expand_table<true,false,true>(p, str, cs, sf);
        if (ret) return rf();
        
        // 4.处理剩余不足一块的字符
        #pragma clang loop vectorize(disable)
        while (p > str) if (--p, lf(set[*p], p)) return rf();
        return rf();
    }
    
}

/// findc
namespace xylu::xystring::__
{
    static size_t findc_simd(const uint8* str, size_t slen, StringView cs) noexcept
    {
        size_t index = -1;
        return string_help_simd<true>(str, slen, cs,
            [&](bool match, auto p)
            {
                if (match)
                {
                    index = p - str;
                    return true;
                }
                return false;
            },
            [&](auto potential, auto p)
            {
                if (potential == 0) return false;
                index = p - str + bit_count_0_low(potential);
                return true;
            },
            [&]{ return index; });
    }

    XY_NO_VECTORIZE_GCC
    static size_t findc_easy(const uint8* str, size_t slen, StringView cs) noexcept
    {
        bool set[256];
        string_help_easy_table(set, cs);
        #pragma clang loop vectorize(disable)
        for (size_t i = 0; i < slen; ++i) if (set[str[i]]) return i;
        return -1;
    }

    size_t findc(const char* str, size_t slen, StringView cs) noexcept
    {
        // slen > 0, cs.size() > 0
        if (XY_UNLIKELY(cs.size() == 1)) return mem_find(str, cs[0], slen);
#if defined(__AVX2__) || defined(__SSSE3__) || defined(__ARM_NEON__)
        if (slen >= simd_size<option>() * 2) return findc_simd((const uint8*)str, slen, cs);
#endif
        return findc_easy((const uint8*)str, slen, cs);
    }
}

/// findnc
namespace xylu::xystring::__
{
    static size_t findnc_simd(const uint8* str, size_t slen, StringView cs) noexcept
    {
        size_t index = -1;
        return string_help_simd<true>(str, slen, cs,
            [&](bool match, auto p)
            {
                if (match) return false;
                index = p - str;
                return true;
            },
            [&](auto potential, auto p)
            {
                if (potential == decltype(potential)(-1)) return false;
                index = p - str + bit_count_1_low(potential);
                return true;
            },
            [&]{ return index; });
    }

    XY_NO_VECTORIZE_GCC
    static size_t findnc_easy(const uint8* str, size_t slen, StringView cs) noexcept
    {
        bool set[256];
        string_help_easy_table<true>(set, cs);
        #pragma clang loop vectorize(disable)
        for (size_t i = 0; i < slen; ++i) if (set[str[i]]) return i;
        return -1;
    }

    size_t findnc(const char* str, size_t slen, StringView cs) noexcept
    {
        // slen > 0, cs.size() > 0
        if (XY_UNLIKELY(cs.size() == 1)) return findn(str, slen, cs[0]);
#if defined(__AVX2__) || defined(__SSSE3__) || defined(__ARM_NEON__)
        if (slen >= simd_size<option>() * 2) return findnc_simd((const uint8*)str, slen, cs);
#endif
        return findnc_easy((const uint8*)str, slen, cs);
    }
}

/// rfindc
namespace xylu::xystring::__
{
    static size_t rfindc_simd(const uint8* str, size_t slen, StringView cs) noexcept
    {
        size_t index = -1;
        return string_help_simd_r(str, slen, cs,
            [&](bool match, auto p)
            {
                if (match)
                {
                    index = p - str;
                    return true;
                }
                return false;
            },
            [&](auto potential, auto p)
            {
                if (potential == 0) return false;
                index = p - str + (sizeof(potential)*8-1) - bit_count_0_high(potential);
                return true;
            },
            [&]{ return index; });
    }

    XY_NO_VECTORIZE_GCC
    static size_t rfindc_easy(const uint8* str, size_t slen, StringView cs) noexcept
    {
        bool set[256];
        string_help_easy_table(set, cs);
        #pragma clang loop vectorize(disable)
        for (size_t i = slen; i > 0;) if (set[str[--i]]) return i;
        return -1;
    }

    size_t rfindc(const char* str, size_t slen, StringView cs) noexcept
    {
        // slen > 0, cs.size() > 0
        if (XY_UNLIKELY(cs.size() == 1)) return rfind(str, slen, cs[0]);
#if defined(__AVX2__) || defined(__SSSE3__) || defined(__ARM_NEON__)
        if (slen >= simd_size<option>() * 2) return rfindc_simd((const uint8*)str, slen, cs);
#endif
        return rfindc_easy((const uint8*)str, slen, cs);
    }
}

/// rfindnc
namespace xylu::xystring::__
{
    static size_t rfindnc_simd(const uint8* str, size_t slen, StringView cs) noexcept
    {
        size_t index = -1;
        return string_help_simd_r(str, slen, cs,
            [&](bool match, auto p)
            {
                if (match) return false;
                index = p - str;
                return true;
            },
            [&](auto potential, auto p)
            {
                if (potential == decltype(potential)(-1)) return false;
                index = p - str + (sizeof(potential)*8-1) - bit_count_1_high(potential);
                return true;
            },
            [&]{ return index; });
    }

    XY_NO_VECTORIZE_GCC
    static size_t rfindnc_easy(const uint8* str, size_t slen, StringView cs) noexcept
    {
        bool set[256];
        string_help_easy_table<true>(set, cs);
        #pragma clang loop vectorize(disable)
        for (size_t i = slen; i > 0;) if (set[str[--i]]) return i;
        return -1;
    }

    size_t rfindnc(const char* str, size_t slen, StringView cs) noexcept
    {
        // slen > 0, cs.size() > 0
        if (XY_UNLIKELY(cs.size() == 1)) return rfindn(str, slen, cs[0]);
#if defined(__AVX2__) || defined(__SSSE3__) || defined(__ARM_NEON__)
        if (slen >= simd_size<option>() * 2) return rfindnc_simd((const uint8*)str, slen, cs);
#endif
        return rfindnc_easy((const uint8*)str, slen, cs);
    }
}

/// tallyc
namespace xylu::xystring::__
{
    static size_t tallyc_simd(const uint8* str, size_t slen, StringView cs) noexcept
    {
        size_t times = 0;
        return string_help_simd<false>(str, slen, cs,
            [&](bool match, auto){ times += match; },
            [&](auto potential, auto){ times += bit_count_1_total(potential); },
            [&]{ return times; });
    }

    XY_NO_VECTORIZE_GCC
    static size_t tallyc_easy(const uint8* str, size_t slen, StringView cs) noexcept
    {
        size_t times = 0;
        bool set[256];
        string_help_easy_table(set, cs);
        #pragma clang loop vectorize(disable)
        for (size_t i = 0; i < slen; ++i) times += set[str[i]];
        return times;
    }

    size_t tallyc(const char* str, size_t slen, StringView cs) noexcept
    {
        // slen > 0, cs.size() > 0
        if (XY_UNLIKELY(cs.size() == 1)) return tally(str, slen, cs[0]);
#if defined(__AVX2__) || defined(__SSSE3__) || defined(__ARM_NEON__)
        if (slen >= simd_size<option>() * 2) return tallyc_simd((const uint8*)str, slen, cs);
#endif
        return tallyc_easy((const uint8*)str, slen, cs);
    }
}

/// tallync
namespace xylu::xystring::__
{
    static size_t tallync_simd(const uint8* str, size_t slen, StringView cs) noexcept
    {
        size_t times = 0;
        return string_help_simd<false,true>(str, slen, cs,
            [&](bool match, auto){ times += match; },
            [&](auto potential, auto){ times += bit_count_1_total(potential); },
            [&]{ return times; });
    }

    XY_NO_VECTORIZE_GCC
    static size_t tallync_easy(const uint8* str, size_t slen, StringView cs) noexcept
    {
        size_t times = 0;
        bool set[256];
        string_help_easy_table<true>(set, cs);
        #pragma clang loop vectorize(disable)
        for (size_t i = 0; i < slen; ++i) times += set[str[i]];
        return times;
    }

    size_t tallync(const char* str, size_t slen, StringView cs) noexcept
    {
        // slen > 0, cs.size() > 0
        if (XY_UNLIKELY(cs.size() == 1)) return tallyn(str, slen, cs[0]);
#if defined(__AVX2__) || defined(__SSSE3__) || defined(__ARM_NEON__)
        if (slen >= simd_size<option>() * 2) return tallync_simd((const uint8*)str, slen, cs);
#endif
        return tallync_easy((const uint8*)str, slen, cs);
    }
}

/// 辅助函数
namespace
{
    template <bool converse = false> XY_NO_VECTORIZE_GCC
    void string_help_easy_table(bool(&set)[256], StringView cus, StringView cs) noexcept
    {
        mem_set(set, converse, 256);
        #pragma clang loop vectorize(disable)
        for (const uint8 *cup = (const uint8*)cus.data(), *cue = cup + (cus.size() & ~size_t(1)); cup < cue; cup += 2)
            mem_set(set + cup[0], !converse, max(cup[1] + 1 - cup[0], 0));
        #pragma clang loop vectorize(disable)
        for (uint8 c : cs.range()) set[c] = !converse;
    }

    template <bool converse = false> XY_NO_VECTORIZE_GCC
    void string_help_simdu_pre_table(bool(&set)[256], bool& expand, StringView cus, StringView cs) noexcept
    {
        expand = false;
        mem_set(set, converse, sizeof(set));
        // 处理范围
        #pragma clang loop vectorize(disable)
        for (const uint8 *cup = (const uint8*)cus.data(), *cue = cup + (cus.size() & ~size_t(1)); cup < cue; cup += 2)
        {
            if (XY_UNLIKELY(cup[1] < cup[0])) continue;
            auto dis = cup[1] + 1 - cup[0];
            mem_set(set + cup[0], !converse, dis);
            expand |= cup[1] & 0x80;
        }
        // 处理单字符集
        #pragma clang loop vectorize(disable)
        for (uint8 c : cs.range()) set[c] = !converse, expand |= c & 0x80;
    }

    template <bool converse = false> XY_NO_VECTORIZE_GCC
    void string_help_simdu_pre_foreach(bool(&set)[256], uint8& cmin, uint8& cmax, StringView cus, StringView cs) noexcept
    {
        cmin = 255, cmax = 0;
        mem_set(set, converse, sizeof(set));
        // 处理范围
        #pragma clang loop vectorize(disable)
        for (const uint8 *cup = (const uint8*)cus.data(), *cue = cup + (cus.size() & ~size_t(1)); cup < cue; cup += 2)
        {
            if (XY_UNLIKELY(cup[1] < cup[0])) continue;
            cmin = min(cmin, cup[0]);
            cmax = max(cmax, cup[1]);
            mem_set(set + cup[0], !converse, cup[1] + 1 - cup[0]);
        }
        // 处理单字符集
        #pragma clang loop vectorize(disable)
        for (uint8 c : cs.range())
        {
            set[c] = !converse;
            cmin = min(cmin, c);
            cmax = max(cmax, c);
        }
    }

    template <bool step_return, bool converse = false, bool reverse = false, typename SimdFunc> XY_NO_VECTORIZE_GCC
    bool string_help_simdu_direct_forsingle(const uint8*& p, const uint8* ep, StringView cus, SimdFunc&& sf) noexcept
    {
        // 创建匹配向量组
        using M = decltype(simd_fill<option>('0'));
        M range_bound[2] { simd_fill<option>(char(cus[0] - 1)), simd_fill<option>(cus[1]) };

        // 每次处理一块
        while (reverse ? (p >= ep + BlockSize) : (p + BlockSize <= ep))
        {
            if constexpr (reverse) p -= BlockSize;
            auto block = simd_load<option>(p);
            auto mask = simd_and(simd_gt(block, range_bound[0]), simd_not(simd_gt(block, range_bound[1])));
            auto potential = simd_msb(mask);
            if constexpr (converse) potential = ~potential;
            if constexpr (!step_return) sf(potential, p);
            else if (sf(potential, p)) return true;
            if constexpr (!reverse) p += BlockSize;
        }
        return false;
    }

    template <bool step_return, bool converse = false, bool reverse = false, typename SimdFunc> XY_NO_VECTORIZE_GCC
    bool string_help_simdu_direct_foreach(const uint8*& p, const uint8* ep, StringView cus, StringView cs, SimdFunc&& sf) noexcept
    {
        // 创建匹配向量组
        uint range_count = cus.size() & ~size_t(1);
        using M = decltype(simd_fill<option>('0'));
        M range_bound[range_count], match_eq[cs.size()];
        // 处理单字符集
        #pragma clang loop vectorize(disable)
        for (uint i = 0; i < cs.size(); ++i) match_eq[i] = simd_fill<option>(cs[i]);
        // 处理范围
        const char *cup = cus.data(), *cupe = cup + range_count;
        for (M *ms = range_bound; cup < cupe; cup += 2)
        {
            if (XY_UNLIKELY(cup[1] < cup[0])) { range_count -= 2; continue; }
            // 左边界预先 - 1， 使用 gt 匹配
            ms[0] = simd_fill<option>(char(cup[0] - 1));
            ms[1] = simd_fill<option>(cup[1]);
            ms += 2;
        }

        // 每次处理一块
        while (reverse ? (p >= ep + BlockSize) : (p + BlockSize <= ep))
        {
            if constexpr (reverse) p -= BlockSize;
            auto block = simd_load<option>(p);
            auto tmp = simd_fill<option>('\0');
            // 判断范围交集
            for (M *ms = range_bound; ms < range_bound + range_count; ms += 2)
                tmp = simd_or(simd_and(simd_gt(block, ms[0]), simd_not(simd_gt(block, ms[1]))), tmp);
            // 判断单字符集
            for (uint i = 0; i < cs.size(); ++i) tmp = simd_or(simd_eq(block, match_eq[i]), tmp);
            auto potential = simd_msb(tmp);
            if constexpr (converse) potential = ~potential;
            if constexpr (!step_return) sf(potential, p);
            else if (sf(potential, p)) return true;
            if constexpr (!reverse) p += BlockSize;
        }

        return false;
    }

    template <bool step_return, bool converse = false, bool reverse = false, typename SimdFunc> XY_NO_VECTORIZE_GCC
    bool string_help_simdu_offest_forsingle(const uint8*& p, const uint8* ep, StringView cus, SimdFunc&& sf) noexcept
    {
        // 创建匹配向量组
        using M = decltype(simd_fill<option>('0'));
        M range_bound[2] { simd_fill<option>(char(cus[0] + 0x7f)), simd_fill<option>(char(cus[1] + 0x80)) };

        // 每次处理一块
        while (reverse ? (p >= ep + BlockSize) : (p + BlockSize <= ep))
        {
            if constexpr (reverse) p -= BlockSize;
            auto block = simd_add1(simd_load<option>(p), simd_fill<option>('\x80'));
            auto mask = simd_and(simd_gt(block, range_bound[0]), simd_not(simd_gt(block, range_bound[1])));
            auto potential = simd_msb(mask);
            if constexpr (converse) potential = ~potential;
            if constexpr (!step_return) sf(potential, p);
            else if (sf(potential, p)) return true;
            if constexpr (!reverse) p += BlockSize;
        }
        return false;
    }

    template <bool step_return, bool converse = false, bool reverse = false, typename SimdFunc> XY_NO_VECTORIZE_GCC
    bool string_help_simdu_offest_foreach(const uint8*& p, const uint8* ep, StringView cus, StringView cs, SimdFunc&& sf) noexcept
    {
        // 创建匹配向量组
        uint range_count = cus.size() & ~size_t(1), single_count = cs.size();
        using M = decltype(simd_fill<option>('0'));
        M range_bound[range_count], match_eq[single_count + 1];
        // 处理单字符集，所有字符 +0x80 转为有符号比较
        #pragma clang loop vectorize(disable)
        for (uint i = 0; i < single_count; ++i) match_eq[i] = simd_fill<option>(char(cs[i] + 0x80));
        // 处理范围
        bool deal_min = false;
        const uint8 *cup = (const uint8*)cus.data(), *cupe = cup + range_count;
        for (M *ms = range_bound; cup < cupe; cup += 2)
        {
            if (XY_UNLIKELY(cup[1] < cup[0])) { range_count -= 2; continue; }
            // 左边界预先 - 1， 使用 gt 匹配
            if (XY_UNLIKELY(cup[0] == 0))
            {
                if (XY_UNLIKELY(cup[1] == 0)) { range_count -= 2; continue; }
                if (!deal_min)
                {
                    match_eq[single_count] = simd_fill<option>('\x80');
                    ++single_count;
                    deal_min = true;
                }
                ms[0] = simd_fill<option>('\x80');
            }
            else ms[0] = simd_fill<option>(char(cup[0] + 0x7f));
            ms[1] = simd_fill<option>(char(cup[1] + 0x80));
            ms += 2;
        }

        // 每次处理一块
        while (reverse ? (p >= ep + BlockSize) : (p + BlockSize <= ep))
        {
            if constexpr (reverse) p -= BlockSize;
            auto block = simd_add1(simd_load<option>(p), simd_fill<option>('\x80'));
            auto tmp = simd_fill<option>('\0');
            // 判断范围交集
            for (M *ms = range_bound; ms < range_bound + range_count; ms += 2)
                tmp = simd_or(simd_and(simd_gt(block, ms[0]), simd_not(simd_gt(block, ms[1]))), tmp);
            // 判断单字符集
            for (uint i = 0; i < single_count; ++i) tmp = simd_or(simd_eq(block, match_eq[i]), tmp);
            auto potential = simd_msb(tmp);

            if constexpr (converse) potential = ~potential;
            if constexpr (!step_return) sf(potential, p);
            else if (sf(potential, p)) return true;
            if constexpr (!reverse) p += BlockSize;
        }

        return false;
    }

    template <bool step_return, bool converse = false, bool reverse = false, typename SimdFunc> XY_NO_VECTORIZE_GCC
    bool string_help_simdu_ascii_table(const uint8*& p, const uint8* ep, StringView cus, StringView cs, SimdFunc&& sf) noexcept
    {
        alignas(BlockSize) uint8 table_low[BlockSize]{};
        // 处理单字符集
        #pragma clang loop vectorize(disable)
        for (uint8 c : cs.range()) table_low[c & 0xf] |= 1 << (c >> 4);
        // 处理范围
        for (const uint8 *cup = (const uint8*)cus.data(), *cue = cup + (cus.size() & ~size_t(1)); cup < cue; cup += 2)
        {
            if (XY_UNLIKELY(cup[1] < cup[0])) continue;
            auto dis = cup[1] + 1 - cup[0];
            if (dis > 16) for (int c = cup[0], e = c + 16; c < e; ++c)
                table_low[c & 0xf] |= (0xff >> (7 - (cup[1] - c) / 16)) << (c >> 4);
            else for (int c = cup[0], e = cup[1]; c <= e; ++c)
                table_low[c & 0xf] |= 1 << (c >> 4);
        }

        if constexpr (BlockSize == 32) mem_copy(table_low + 16, table_low, 16);
        auto tlow = simd_load<option>(table_low);
        auto thigh = simd_load<option>(table_high);

        while (reverse ? (p >= ep + BlockSize) : (p + BlockSize <= ep))
        {
            if constexpr (reverse) p -= BlockSize;
            auto block = simd_load<option>(p);
            auto high = simd_rshift8(block, 4);
            // neon 会对高4字节进行归0检测，而 sse/avx 仅对最高字节，可作为非 ascii 过滤（尽管不会被传递）
            if constexpr (option == simd_Neon) block = simd_and(simd_fill<option>('\x8f'), block);
            // match = table_low[c & 0x0f] & table_high[c >> 4]
            auto match = simd_and(simd_shuffle(tlow, block), simd_shuffle(thigh, high));
            auto potential = simd_msb(simd_eq(match, simd_fill<option>('\0')));

            if constexpr (!converse) potential = ~potential;
            if constexpr (!step_return) sf(potential, p);
            else if (sf(potential, p)) return true;
            if constexpr (!reverse) p += BlockSize;
        }

        return false;
    }

    template <bool step_return, bool converse = false, bool reverse = false, typename SimdFunc> XY_NO_VECTORIZE_GCC
    bool string_help_simdu_expand_table(const uint8*& p, const uint8* ep, StringView cus, StringView cs, SimdFunc&& sf) noexcept
    {
        alignas(BlockSize) uint8 table_low1[BlockSize]{}, table_low2[BlockSize]{};
        // 处理单字符集
        #pragma clang loop vectorize(disable)
        for (uint8 c : cs.range()) (c & 0x80 ? table_low2[c & 15] : table_low1[c & 15]) |= table_high[c >> 4];
        // 处理范围
        for (const uint8 *cup = (const uint8*)cus.data(), *cue = cup + (cus.size() & ~size_t(1)); cup < cue; cup += 2)
        {
            if (XY_UNLIKELY(cup[1] < cup[0])) continue;
            if (cup[0] < 0x80)
            {
                int last = min(cup[1], 0x7fu);
                auto dis = last + 1 - cup[0];
                if (dis > 16) for (int c = cup[0], e = c + 16; c < e; ++c)
                    table_low1[c & 0xf] |= (0xff >> (7 - (last - c) / 16)) << (c >> 4);
                else for (int c = cup[0], e = last; c <= e; ++c)
                    table_low1[c & 0xf] |= 1 << (c >> 4);
            }
            if (cup[1] >= 0x80)
            {
                int first = max(cup[0], 0x80u);
                auto dis = cup[1] + 1 - first;
                if (dis > 16) for (int c = first, e = c + 16; c < e; ++c)
                    table_low2[c & 0xf] |= (0xff >> (7 - (cup[1] - c) / 16)) << ((c >> 4) & 0x7);
                else for (int c = first, e = cup[1]; c <= e; ++c)
                    table_low2[c & 0xf] |= table_high[c >> 4];
            }
        }

        if constexpr (BlockSize == 32) mem_copy(table_low1 + 16, table_low1, 16);
        if constexpr (BlockSize == 32) mem_copy(table_low2 + 16, table_low2, 16);

        auto tlow1 = simd_load<option>(table_low1);
        auto tlow2 = simd_load<option>(table_low2);
        auto thigh = simd_load<option>(table_high);

        while (reverse ? (p >= ep + BlockSize) : (p + BlockSize <= ep))
        {
            if constexpr (reverse) p -= BlockSize;
            auto block = simd_load<option>(p);
            auto olow = simd_and(block, simd_fill<option>('\x0f'));
            auto mhigh = simd_shuffle(thigh, simd_rshift8(block, 4));
            // choose = c > -1 ? 1 : 0 <==> c & 0x80 ? 0 : 1
            auto choose = simd_gt(block, simd_fill<option>('\xff'));
            // mlow1 = table_low1[c & 0x0f]
            auto mlow1 = simd_shuffle(tlow1, olow);
            // mlow2 = table_low2[c & 0x0f]
            auto mlow2 = simd_shuffle(tlow2, olow);
            // (choose & (mlow1 & mhigh)) | (~choose & (mlow2 & mhigh))
            // <==> mhigh & (mlow2 ^ ((mlow2 ^ mlow1) & choose))
            auto mlow = simd_xor(mlow2, simd_and(simd_xor(mlow2, mlow1), choose));
            auto potential = simd_msb(simd_eq(simd_and(mlow, mhigh), simd_fill<option>('\0')));

            if constexpr (!converse) potential = ~potential;
            if constexpr (!step_return) sf(potential, p);
            else if (sf(potential, p)) return true;
            if constexpr (!reverse) p += BlockSize;
        }

        return false;
    }

    // 回调: lf(p), sf(potential[匹配位为1其余为0], p), rf()
    template <bool step_return, bool converse = false, typename LoopFunc, typename SimdFunc, typename ReturnFunc> XY_NO_VECTORIZE_GCC
    decltype(auto) string_help_simdu(const uint8* str, size_t slen, StringView cus, StringView cs, LoopFunc&& lf, SimdFunc&& sf, ReturnFunc&& rf) noexcept
    {
        size_t count = cs.size() + cus.size() / 2;
        bool use_table = count > 1 && slen * count > 400;

        // 1.预处理字符集
        bool set[256];
        bool expand = false;
        uint8 cmin = 255, cmax = 0;
        if (use_table) string_help_simdu_pre_table<converse>(set, expand, cus, cs);
        else string_help_simdu_pre_foreach<converse>(set, cmin, cmax, cus, cs);

        // 2.将 最前面几个 非整块对齐 的字符 单独处理
        auto ap = (const uint8*)(((size_t)str + (BlockSize-1)) & ~(BlockSize-1));
        auto ep = str + slen, p = str;
        #pragma clang loop vectorize(disable)
        for (; p < min(ap, ep); ++p)
        {
            if constexpr (!step_return) lf(set[*p], p);
            else if (lf(set[*p], p)) return rf();
        }
        if (ap >= ep) return rf();

        // 3.处理整块
        bool ret;
        if (use_table)
        {
            if (!expand) ret = string_help_simdu_ascii_table<step_return, converse>(p, ep, cus, cs, sf);
            else ret = string_help_simdu_expand_table<step_return, converse>(p, ep, cus, cs, sf);
        }
        else
        {
            if (XY_UNLIKELY(cmin > cmax)) ret = string_help_simdu_direct_foreach<step_return, converse>(p, ep, "", "", sf);
            // [0x00 - 1 = 0xff, 0x7f] ∪ [0x81 - 1 = 0x80, 0xff] 有符号数单调递增
            else if (cmax <= 0x7f || cmin > 0x80)
            {
                if (count <= 1) ret = string_help_simdu_direct_forsingle<step_return, converse>(p, ep, cus, sf);
                else ret = string_help_simdu_direct_foreach<step_return, converse>(p, ep, cus, cs, sf);
            }
            else
            {
                if (count <= 1 && cus[0]) ret = string_help_simdu_offest_forsingle<step_return, converse>(p, ep, cus, sf);
                else ret = string_help_simdu_offest_foreach<step_return, converse>(p, ep, cus, cs, sf);
            }
        }
        if (ret) return rf();

        // 4.处理剩余不足一块的字符
        #pragma clang loop vectorize(disable)
        for (; p < ep; ++p)
        {
            if constexpr (!step_return) lf(set[*p], p);
            else if (lf(set[*p], p)) return rf();
        }
        return rf();
    }

    // 回调: lf(p), sf(potential[匹配位为1其余为0], p), rf()
    template <typename LoopFunc, typename SimdFunc, typename ReturnFunc> XY_NO_VECTORIZE_GCC
    decltype(auto) string_help_simdu_r(const uint8* str, size_t slen, StringView cus, StringView cs, LoopFunc&& lf, SimdFunc&& sf, ReturnFunc&& rf) noexcept
    {
        size_t count = cs.size() + cus.size() / 2;
        bool use_table = count > 1 && slen * count > 400;

        // 1.预处理字符集
        bool set[256];
        bool expand = false;
        uint8 cmin = 255, cmax = 0;
        if (use_table) string_help_simdu_pre_table(set, expand, cus, cs);
        else string_help_simdu_pre_foreach(set, cmin, cmax, cus, cs);

        // 2.将 最前面几个 非整块对齐 的字符 单独处理
        auto sp = str + slen, p = sp;
        auto ap = (const uint8*)((size_t)sp & ~(BlockSize-1));
        #pragma clang loop vectorize(disable)
        while (p > max(ap, str)) if (--p, lf(set[*p], p)) return rf();
        if (ap <= str) return rf();

        // 3.处理整块
        bool ret;
        if (use_table)
        {
            if (!expand) ret = string_help_simdu_ascii_table<true,false,true>(p, str, cus, cs, sf);
            else ret = string_help_simdu_expand_table<true,false,true>(p, str, cus, cs, sf);
        }
        else
        {
            if (XY_UNLIKELY(cmin > cmax)) ret = string_help_simdu_direct_foreach<true,false,true>(p, str, "", "", sf);
            // [0x00 - 1 = 0xff, 0x7f] ∪ [0x81 - 1 = 0x80, 0xff] 有符号数单调递增
            else if (cmax <= 0x7f || cmin > 0x80)
            {
                if (count <= 1) ret = string_help_simdu_direct_forsingle<true,false,true>(p, str, cus, sf);
                else ret = string_help_simdu_direct_foreach<true,false,true>(p, str, cus, cs, sf);
            }
            else
            {
                if (count <= 1 && cus[0]) ret = string_help_simdu_offest_forsingle<true,false,true>(p, str, cus, sf);
                else ret = string_help_simdu_offest_foreach<true,false,true>(p, str, cus, cs, sf);
            }
        }
        if (ret) return rf();

        // 4.处理剩余不足一块的字符
        #pragma clang loop vectorize(disable)
        while (p > str) if (--p, lf(set[*p], p)) return rf();
        return rf();
    }

}

/// findu
namespace xylu::xystring::__
{
    static size_t findu_simd(const uint8* str, size_t slen, StringView cus, StringView cs) noexcept
    {
        size_t index = -1;
        return string_help_simdu<true>(str, slen, cus, cs,
            [&](bool match, auto p)
            {
                if (match)
                {
                    index = p - str;
                    return true;
                }
                return false;
            },
            [&](auto potential, auto p)
            {
                if (potential == 0) return false;
                index = p - str + bit_count_0_low(potential);
                return true;
            },
            [&]{ return index; });
    }

    XY_NO_VECTORIZE_GCC
    static size_t findu_easy(const uint8* str, size_t slen, StringView cus, StringView cs) noexcept
    {
        bool set[256];
        string_help_easy_table(set, cus, cs);
        #pragma clang loop vectorize(disable)
        for (size_t i = 0; i < slen; ++i) if (set[str[i]]) return i;
        return -1;
    }

    static size_t findu_naive(const char* str, size_t slen, StringView cus, StringView cs) noexcept
    {
        return find_constexpr_help(str, 0, slen, [&](uint8 c) { return check_in_range(c, cus, cs); });
    }

    size_t findu(const char* str, size_t slen, StringView cus, StringView cs) noexcept
    {
        // slen > 0, (cus.size() >= 2 || cs.size() >= 1)
        if (XY_UNLIKELY(cus.size() < 2)) return findc(str, slen, cs);
        if (slen <= 16) return findu_naive(str, slen, cus, cs);
#if defined(__AVX2__) || defined(__SSSE3__) || defined(__ARM_NEON__)
        if (slen > simd_size<option>() * 3 / 2) return findu_simd((const uint8*)str, slen, cus, cs);
#endif
        return findu_easy((const uint8*)str, slen, cus, cs);
    }
}

/// findnu
namespace xylu::xystring::__
{
    static size_t findnu_simd(const uint8* str, size_t slen, StringView cus, StringView cs) noexcept
    {
        size_t index = -1;
        return string_help_simdu<true>(str, slen, cus, cs,
            [&](bool match, auto p)
            {
                if (match) return false;
                index = p - str;
                return true;
            },
            [&](auto potential, auto p)
            {
                if (potential == decltype(potential)(-1)) return false;
                index = p - str + bit_count_1_low(potential);
                return true;
            },
            [&]{ return index; });
    }

    XY_NO_VECTORIZE_GCC
    static size_t findnu_easy(const uint8* str, size_t slen, StringView cus, StringView cs) noexcept
    {
        bool set[256];
        string_help_easy_table<true>(set, cus, cs);
        #pragma clang loop vectorize(disable)
        for (size_t i = 0; i < slen; ++i) if (set[str[i]]) return i;
        return -1;
    }

    static size_t findnu_naive(const char* str, size_t slen, StringView cus, StringView cs) noexcept
    {
        return find_constexpr_help(str, 0, slen, [&](uint8 c) { return !check_in_range(c, cus, cs); });
    }

    size_t findnu(const char* str, size_t slen, StringView cus, StringView cs) noexcept
    {
        // slen > 0, (cus.size() >= 2 || cs.size() >= 1)
        if (XY_UNLIKELY(cus.size() < 2)) return findnc(str, slen, cs);
        if (slen <= 16) return findnu_naive(str, slen, cus, cs);
#if defined(__AVX2__) || defined(__SSSE3__) || defined(__ARM_NEON__)
        if (slen > simd_size<option>() * 3 / 2) return findnu_simd((const uint8*)str, slen, cus, cs);
#endif
        return findnu_easy((const uint8*)str, slen, cus, cs);
    }
}

/// rfindu
namespace xylu::xystring::__
{
    static size_t rfindu_simd(const uint8* str, size_t slen, StringView cus, StringView cs) noexcept
    {
        size_t index = -1;
        return string_help_simdu_r(str, slen, cus, cs,
            [&](bool match, auto p)
            {
                if (match)
                {
                    index = p - str;
                    return true;
                }
                return false;
            },
            [&](auto potential, auto p)
            {
                if (potential == 0) return false;
                index = p - str + (sizeof(potential)*8-1) - bit_count_0_high(potential);
                return true;
            },
            [&]{ return index; });
    }

    XY_NO_VECTORIZE_GCC
    static size_t rfindu_easy(const uint8* str, size_t slen, StringView cus, StringView cs) noexcept
    {
        bool set[256];
        string_help_easy_table(set, cus, cs);
        #pragma clang loop vectorize(disable)
        for (size_t i = slen; i > 0;) if (set[str[--i]]) return i;
        return -1;
    }

    static size_t rfindu_naive(const char* str, size_t slen, StringView cus, StringView cs) noexcept
    {
        return find_constexpr_help_r(str, 0, slen, [&](uint8 c) { return check_in_range(c, cus, cs); });
    }

    size_t rfindu(const char* str, size_t slen, StringView cus, StringView cs) noexcept
    {
        // slen > 0, (cus.size() >= 2 || cs.size() >= 1)
        if (XY_UNLIKELY(cus.size() < 2)) return rfindc(str, slen, cs);
        if (slen <= 16) return rfindu_naive(str, slen, cus, cs);
#if defined(__AVX2__) || defined(__SSSE3__) || defined(__ARM_NEON__)
        if (slen > simd_size<option>() * 3 / 2) return rfindu_simd((const uint8*)str, slen, cus, cs);
#endif
        return rfindu_easy((const uint8*)str, slen, cus, cs);
    }
}

/// rfindnu
namespace xylu::xystring::__
{
    static size_t rfindnu_simd(const uint8* str, size_t slen, StringView cus, StringView cs) noexcept
    {
        size_t index = -1;
        return string_help_simdu_r(str, slen, cus, cs,
            [&](bool match, auto p)
            {
                if (match) return false;
                index = p - str;
                return true;
            },
            [&](auto potential, auto p)
            {
                if (potential == decltype(potential)(-1)) return false;
                index = p - str + (sizeof(potential)*8-1) - bit_count_1_high(potential);
                return true;
            },
            [&]{ return index; });
    }

    XY_NO_VECTORIZE_GCC
    static size_t rfindnu_easy(const uint8* str, size_t slen, StringView cus, StringView cs) noexcept
    {
        bool set[256];
        string_help_easy_table<true>(set, cus, cs);
        #pragma clang loop vectorize(disable)
        for (size_t i = slen; i > 0;) if (set[str[--i]]) return i;
        return -1;
    }

    static size_t rfindnu_naive(const char* str, size_t slen, StringView cus, StringView cs) noexcept
    {
        return find_constexpr_help_r(str, 0, slen, [&](uint8 c) { return !check_in_range(c, cus, cs); });
    }

    size_t rfindnu(const char* str, size_t slen, StringView cus, StringView cs) noexcept
    {
        // slen > 0, (cus.size() >= 2 || cs.size() >= 1)
        if (XY_UNLIKELY(cus.size() < 2)) return rfindnc(str, slen, cs);
        if (slen <= 16) return rfindnu_naive(str, slen, cus, cs);
#if defined(__AVX2__) || defined(__SSSE3__) || defined(__ARM_NEON__)
        if (slen > simd_size<option>() * 3 / 2) return rfindnu_simd((const uint8*)str, slen, cus, cs);
#endif
        return rfindnu_easy((const uint8*)str, slen, cus, cs);
    }
}

/// tallyu
namespace xylu::xystring::__
{
    static size_t tallyu_simd(const uint8* str, size_t slen, StringView cus, StringView cs) noexcept
    {
        size_t times = 0;
        return string_help_simdu<false>(str, slen, cus, cs,
            [&](bool match, auto){ times += match; },
            [&](auto potential, auto){ times += bit_count_1_total(potential); },
            [&]{ return times; });
    }

    XY_NO_VECTORIZE_GCC
    static size_t tallyu_easy(const uint8* str, size_t slen, StringView cus, StringView cs) noexcept
    {
        size_t times = 0;
        bool set[256];
        string_help_easy_table(set, cus, cs);
        #pragma clang loop vectorize(disable)
        for (size_t i = 0; i < slen; ++i) times += set[str[i]];
        return times;
    }

    static size_t tallyu_naive(const char* str, size_t slen, StringView cus, StringView cs) noexcept
    {
        return tally_constexpr_help(str, slen, [&](uint8 c) { return check_in_range(c, cus, cs); });
    }

    size_t tallyu(const char* str, size_t slen, StringView cus, StringView cs) noexcept
    {
        // slen > 0, (cus.size() >= 2 || cs.size() >= 1)
        if (XY_UNLIKELY(cus.size() < 2)) return tallyc(str, slen, cs);
        if (slen <= 12) return tallyu_naive(str, slen, cus, cs);
#if defined(__AVX2__) || defined(__SSSE3__) || defined(__ARM_NEON__)
        if (slen > simd_size<option>()) return tallyu_simd((const uint8*)str, slen, cus, cs);
#endif
        return tallyu_easy((const uint8*)str, slen, cus, cs);
    }
}

/// tallynu
namespace xylu::xystring::__
{
    static size_t tallynu_simd(const uint8* str, size_t slen, StringView cus, StringView cs) noexcept
    {
        size_t times = 0;
        return string_help_simdu<false,true>(str, slen, cus, cs,
            [&](bool match, auto){ times += match; },
            [&](auto potential, auto){ times += bit_count_1_total(potential); },
            [&]{ return times; });
    }

    XY_NO_VECTORIZE_GCC
    static size_t tallynu_easy(const uint8* str, size_t slen, StringView cus, StringView cs) noexcept
    {
        size_t times = 0;
        bool set[256];
        string_help_easy_table<true>(set, cus, cs);
        #pragma clang loop vectorize(disable)
        for (size_t i = 0; i < slen; ++i) times += set[str[i]];
        return times;
    }

    static size_t tallynu_naive(const char* str, size_t slen, StringView cus, StringView cs) noexcept
    {
        return tally_constexpr_help(str, slen, [&](uint8 c) { return !check_in_range(c, cus, cs); });
    }

    size_t tallynu(const char* str, size_t slen, StringView cus, StringView cs) noexcept
    {
        // slen > 0, (cus.size() >= 2 || cs.size() >= 1)
        if (XY_UNLIKELY(cus.size() < 2)) return tallync(str, slen, cs);
        if (slen <= 12) return tallynu_naive(str, slen, cus, cs);
#if defined(__AVX2__) || defined(__SSSE3__) || defined(__ARM_NEON__)
        if (slen > simd_size<option>()) return tallynu_simd((const uint8*)str, slen, cus, cs);
#endif
        return tallynu_easy((const uint8*)str, slen, cus, cs);
    }
}
