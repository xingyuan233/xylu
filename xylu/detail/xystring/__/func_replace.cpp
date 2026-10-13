#include "../strview.h"

#include "../../xymath/bit.h"
#include "../../xycore/__/simd.h"

using namespace xyu;
using namespace xylu::xystring::__;

/// 辅助函数
namespace
{
    constexpr simd_Type option = simd_Avx | simd_Sse | simd_Neon;
    constexpr size_t BlockSize = simd_size<option>();

    template <bool converse = false, typename ReplaceFunc> XY_NO_VECTORIZE_GCC
    void replace_help_easy(const char* str, size_t len, char* buf, char c, bool reverse, ReplaceFunc&& rf) noexcept
    {
        if (!reverse)
        {
            #pragma clang loop vectorize(disable)
            for (size_t i = 0; i < len; ++i)
            {
                if (converse ^ (str[i] == c)) buf[i] = rf();
                else buf[i] = str[i];
            }
        }
        else
        {
            #pragma clang loop vectorize(disable)
            for (size_t i = len; i > 0;)
            {
                --i;
                if (converse ^ (str[i] == c)) buf[i] = rf();
                else buf[i] = str[i];
            }
        }
    }

    template <bool converse = false, bool use_simd_replace = true, bool reverse = false, typename ReplaceFunc, typename SimdFunc> XY_NO_VECTORIZE_GCC
    void replace_help_simd_block(const char*& sp, const char* ep, char*& dp, char c, ReplaceFunc&& rf, SimdFunc&& sf) noexcept
    {
        const auto match = simd_fill<option>(c);
        while (reverse ? (dp >= ep + BlockSize) : (dp + BlockSize <= ep))
        {
            if constexpr (reverse) dp -= BlockSize, sp -= BlockSize;
            auto ms = simd_loadu<option>(sp);
            auto mask = simd_eq(ms, match);
            if constexpr (use_simd_replace)
            {
                decltype(simd_fill<option>('0')) md;
                if constexpr (!xyu::t_is_same_nocr<int, SimdFunc>) md = sf(ms);
                else md = simd_fill<option>(rf());
                // (mask & md) | (~mask & ms) <==> ms ^ ((ms ^ md) & mask)
                if constexpr (!converse) simd_store(simd_xor(ms, simd_and(simd_xor(ms, md), mask)), dp);
                // (~mask & md) | (mask & ms) <==> md ^ ((md ^ ms) & mask)
                else simd_store(simd_xor(md, simd_and(simd_xor(md, ms), mask)), dp);
            }
            else
            {
                auto potential = simd_msb(mask);
                simd_store(ms, dp);
                if constexpr (converse) potential = ~potential;
                while (potential)
                {
                    uint index = bit_count_0_low(potential);
                    dp[index] = rf();
                    potential &= potential - 1;
                }
            }
            if constexpr (!reverse) dp += BlockSize, sp += BlockSize;
        }
    }

    template <bool converse = false, bool use_simd_replace = true, typename ReplaceFunc, typename SimdFunc> XY_NO_VECTORIZE_GCC
    void replace_help_simd_l(const char* sp, size_t len, char* dp, char c, ReplaceFunc&& rf, SimdFunc&& sf) noexcept
    {
        // 1.处理最前面几个非整块对齐的字符，优先对齐写指针
        auto ep = dp + len;
        auto ap = (const char*)(((size_t)dp + (BlockSize-1)) & ~(BlockSize-1));
        #pragma clang loop vectorize(disable)
        for (; dp < min(ap, ep); ++dp, ++sp)
        {
            if (converse ^ (*sp == c)) *dp = rf();
            else *dp = *sp;
        }
        if (ap >= ep) return;

        // 2.每次处理一块
        replace_help_simd_block<converse, use_simd_replace>(sp, ep, dp, c, rf, sf);

        // 3.处理剩余不足一块的字符
        #pragma clang loop vectorize(disable)
        for (; dp < ep; ++dp, ++sp)
        {
            if (converse ^ (*sp == c)) *dp = rf();
            else *dp = *sp;
        }
    }

    template <bool converse = false, bool use_simd_replace = true, typename ReplaceFunc, typename SimdFunc> XY_NO_VECTORIZE_GCC
    void replace_help_simd_r(const char* sp, size_t len, char* dp, char c, ReplaceFunc&& rf, SimdFunc&& sf) noexcept
    {
        // 1.处理最前面几个非整块对齐的字符，优先对齐写指针
        auto ep = dp; dp += len; sp += len;
        auto ap = (const char*)((size_t)dp & ~(BlockSize - 1));
        #pragma clang loop vectorize(disable)
        while (dp > max(ap, ep))
        {
            --dp; --sp;
            if (converse ^ (*sp == c)) *dp = rf();
            else *dp = *sp;
        }
        if (ap <= ep) return;

        // 2.每次处理一块
        replace_help_simd_block<converse, use_simd_replace, true>(sp, ep, dp, c, rf, sf);

        // 3.处理剩余不足一块的字符
        #pragma clang loop vectorize(disable)
        while (dp > ep)
        {
            --dp; --sp;
            if (converse ^ (*sp == c)) *dp = rf();
            else *dp = *sp;
        }
    }

    // 回调: rf()
    template <bool converse = false, bool use_simd_replace = true, typename ReplaceFunc, typename SimdFunc = int> XY_NO_VECTORIZE_GCC
    void replace_help(const char* sp, size_t len, char* dp, char c, ReplaceFunc&& rf, SimdFunc&& sf = 0) noexcept
    {
        // len > 0
        // 无符号 sp >= dp || sp + len <= dp
        // ==> !(sp < dp < sp + len)
        // ==> !(0 <= dp - sp - 1 < len - 1)
        // ==> dp - sp - 1 >= len - 1
        bool use_left = (size_t)dp - (size_t)sp - 1 >= len - 1;
#if defined(__AVX2__) || defined(__SSE2__) || defined(__ARM_NEON__)
        if (len >= simd_size<option>() * 2)
        {
            if (use_left) return replace_help_simd_l<converse,use_simd_replace>(sp, len, dp, c, rf, sf);
            return replace_help_simd_r<converse,use_simd_replace>(sp, len, dp, c, rf, sf);
        }
#endif
        return replace_help_easy<converse>(sp, len, dp, c, !use_left, rf);
    }

}

/// replace(n)
namespace xylu::xystring::__
{
    void replace(const char* str, size_t len, char* buf, char c, char r) noexcept
    { return replace_help(str, len, buf, c, [&]{ return r; }); }

    void replace(const char* str, size_t len, char* buf, char c, char(*f)()) noexcept
    { return replace_help<false,false>(str, len, buf, c, [&]{ return f(); }); }

    void replacen(const char* str, size_t len, char* buf, char c, char r) noexcept
    { return replace_help<true>(str, len, buf, c, [&]{ return r; }); }

    void replacen(const char* str, size_t len, char* buf, char c, char(*f)()) noexcept
    { return replace_help<true,false>(str, len, buf, c, [&]{ return f(); }); }
}

/// 辅助函数
namespace
{
    alignas(BlockSize) constexpr uint8 table_high[32] {1,2,4,8,16,32,64,128, 1,2,4,8,16,32,64,128, 1,2,4,8,16,32,64,128, 1,2,4,8,16,32,64,128};

    template <bool converse = false> XY_NO_VECTORIZE_GCC
    void replacec_help_easy_table(bool(&set)[256], StringView cs) noexcept
    {
        mem_set(set, converse, 256);
        #pragma clang loop vectorize(disable)
        for (uint8 c : cs.range()) set[c] = !converse;
    }

    template <bool converse = false, typename ReplaceFunc> XY_NO_VECTORIZE_GCC
    void replacec_help_easy(const uint8* str, size_t len, char* buf, StringView cs, bool reverse, ReplaceFunc&& rf) noexcept
    {
        bool set[256];
        replacec_help_easy_table<converse>(set, cs);
        if (!reverse)
        {
            #pragma clang loop vectorize(disable)
            for (size_t i = 0; i < len; ++i)
            {
                if (set[str[i]]) buf[i] = rf(str[i]);
                else buf[i] = str[i];
            }
        }
        else
        {
            #pragma clang loop vectorize(disable)
            for (size_t i = len; i > 0;)
            {
                --i;
                if (set[str[i]]) buf[i] = rf(str[i]);
                else buf[i] = str[i];
            }
        }
    }

    template <bool converse = false, bool use_simd_replace = true, typename M, typename ReplaceFunc, typename SimdFunc> XY_NO_VECTORIZE_GCC
    void replacec_help_simd_result_deal(uint8* dp, M ms, M mask, ReplaceFunc&& rf, SimdFunc&& sf) noexcept
    {
        if constexpr (use_simd_replace)
        {
            auto md = sf(ms);
            // (mask & md) | (~mask & ms) <==> ms ^ ((ms ^ md) & mask)
            if constexpr (!converse) simd_store(simd_xor(ms, simd_and(simd_xor(ms, md), mask)), dp);
            // (~mask & md) | (mask & ms) <==> md ^ ((md ^ ms) & mask)
            else simd_store(simd_xor(md, simd_and(simd_xor(md, ms), mask)), dp);
        }
        else
        {
            auto potential = simd_msb(mask);
            simd_store(ms, dp);
            if constexpr (converse) potential = ~potential;
            while (potential)
            {
                uint index = bit_count_0_low(potential);
                dp[index] = rf(dp[index]);
                potential &= potential - 1;
            }
        }
    }

    template <bool reverse = false, typename SimdFunc> XY_NO_VECTORIZE_GCC
    void replacec_help_simd_all(const uint8*& sp, const uint8* ep, uint8*& dp, SimdFunc&& sf) noexcept
    {
        while (reverse ? (dp >= ep + BlockSize) : (dp + BlockSize <= ep))
        {
            if constexpr (reverse) dp -= BlockSize, sp -= BlockSize;
            if constexpr (!xyu::t_is_same_nocr<SimdFunc,int>) simd_store(sf(simd_loadu<option>(sp)), dp);
            if constexpr (!reverse) dp += BlockSize, sp += BlockSize;
        }
    }

    template <bool converse = false, bool use_simd_replace = true, bool reverse = false, typename ReplaceFunc, typename SimdFunc> XY_NO_VECTORIZE_GCC
    void replacec_help_simd_foreach(const uint8*& sp, const uint8* ep, uint8*& dp, StringView cs, ReplaceFunc&& rf, SimdFunc&& sf) noexcept
    {
        // 创建匹配向量组
        decltype(simd_fill<option>('0')) matches[cs.size()];
        #pragma clang loop vectorize(disable)
        for (uint i = 0; i < cs.size(); ++i) matches[i] = simd_fill<option>(cs[i]);

        // 每次处理一块
        while (reverse ? (dp >= ep + BlockSize) : (dp + BlockSize <= ep))
        {
            if constexpr (reverse) dp -= BlockSize, sp -= BlockSize;
            auto ms = simd_loadu<option>(sp);
            auto mask = simd_eq(ms, matches[0]);
            for (uint i = 1; i < cs.size(); ++i) mask = simd_or(simd_eq(ms, matches[i]), mask);
            replacec_help_simd_result_deal<converse, use_simd_replace>(dp, ms, mask, rf, sf);
            if constexpr (!reverse) dp += BlockSize, sp += BlockSize;
        }
    }

    template <bool converse = false, bool use_simd_replace = true, bool reverse = false, typename ReplaceFunc, typename SimdFunc> XY_NO_VECTORIZE_GCC
    void replacec_help_simd_ascii_table(const uint8*& sp, const uint8* ep, uint8*& dp, StringView cs, ReplaceFunc&& rf, SimdFunc&& sf) noexcept
    {
        alignas(BlockSize) uint8 table_low[BlockSize]{};
        #pragma clang loop vectorize(disable)
        for (uint8 c : cs.range()) table_low[c & 0xf] |= 1 << (c >> 4);
        if constexpr (BlockSize == 32) mem_copy(table_low + 16, table_low, 16);

        auto tlow = simd_load<option>(table_low);
        auto thigh = simd_load<option>(table_high);

        while (reverse ? (dp >= ep + BlockSize) : (dp + BlockSize <= ep))
        {
            if constexpr (reverse) dp -= BlockSize, sp -= BlockSize;
            auto ms = simd_load<option>(sp);
            auto high = simd_rshift8(ms, 4);
            // neon 会对高4字节进行归0检测，而 sse/avx 仅对最高字节，可作为非 ascii 过滤（尽管不会被传递）
            auto mlow = option != simd_Neon ? simd_shuffle(tlow, ms) : simd_shuffle(tlow, simd_and(simd_fill<option>('\x8f'), ms));
            // match = table_low[c & 0x0f] & table_high[c >> 4]
            auto match = simd_and(mlow, simd_shuffle(thigh, high));
            auto mask = simd_eq(match, simd_fill<option>('\0'));
            replacec_help_simd_result_deal<!converse, use_simd_replace>(dp, ms, mask, rf, sf);
            if constexpr (!reverse) dp += BlockSize, sp += BlockSize;
        }
    }

    template <bool converse = false, bool use_simd_replace = true, bool reverse = false, typename ReplaceFunc, typename SimdFunc> XY_NO_VECTORIZE_GCC
    void replacec_help_simd_expand_table(const uint8*& sp, const uint8* ep, uint8*& dp, StringView cs, ReplaceFunc&& rf, SimdFunc&& sf) noexcept
    {
        alignas(BlockSize) uint8 table_low1[BlockSize]{}, table_low2[BlockSize]{};
        #pragma clang loop vectorize(disable)
        for (uint8 c : cs.range()) (c & 0x80 ? table_low2 : table_low1)[c & 15] |= table_high[c >> 4];
        if constexpr (BlockSize == 32) mem_copy(table_low1 + 16, table_low1, 16);
        if constexpr (BlockSize == 32) mem_copy(table_low2 + 16, table_low2, 16);

        auto tlow1 = simd_load<option>(table_low1);
        auto tlow2 = simd_load<option>(table_low2);
        auto thigh = simd_load<option>(table_high);

        while (reverse ? (dp >= ep + BlockSize) : (dp + BlockSize <= ep))
        {
            if constexpr (reverse) dp -= BlockSize, sp -= BlockSize;
            auto ms = simd_load<option>(sp);
            auto olow = simd_and(ms, simd_fill<option>('\x0f'));
            auto mhigh = simd_shuffle(thigh, simd_rshift8(ms, 4));
            // choose = c > -1 ? 1 : 0 <==> c & 0x80 ? 0 : 1
            auto choose = simd_gt(ms, simd_fill<option>('\xff'));
            // mlow1 = table_low1[c & 0x0f]
            auto mlow1 = simd_shuffle(tlow1, olow);
            // mlow2 = table_low2[c & 0x0f]
            auto mlow2 = simd_shuffle(tlow2, olow);
            // (choose & (mlow1 & mhigh)) | (~choose & (mlow2 & mhigh))
            // <==> mhigh & (mlow2 ^ ((mlow2 ^ mlow1) & choose))
            auto mlow = simd_xor(mlow2, simd_and(simd_xor(mlow2, mlow1), choose));
            auto mask = simd_eq(simd_and(mlow, mhigh), simd_fill<option>('\0'));
            replacec_help_simd_result_deal<!converse, use_simd_replace>(dp, ms, mask, rf, sf);
            if constexpr (!reverse) dp += BlockSize, sp += BlockSize;
        }
    }

    template <bool converse = false, bool use_simd_replace = true, typename ReplaceFunc, typename SimdFunc> XY_NO_VECTORIZE_GCC
    void replacec_help_simd_l(const uint8* sp, size_t len, uint8* dp, StringView cs, ReplaceFunc&& rf, SimdFunc&& sf) noexcept
    {
        // 1.预处理字符集
        bool expand = false;
        bool set[256]{};
        if constexpr (converse) mem_set(set, char(true), sizeof(set));
        #pragma clang loop vectorize(disable)
        for (uint8 c : cs.range()) set[c] = !converse, expand |= c & 0x80;

        // 2.将 最前面几个 非整块对齐 的字符 单独处理
        auto ep = dp + len;
        auto ap = (const uint8*)(((size_t)dp + (BlockSize-1)) & ~(BlockSize-1));
        #pragma clang loop vectorize(disable)
        for (; dp < min(ap, ep); ++dp, ++sp)
        {
            if (set[*sp]) *dp = rf(*sp);
            else *dp = *sp;
        }
        if (ap >= ep) return;

        // 3.simd 处理整块
        if (converse && use_simd_replace && cs.empty()) replacec_help_simd_all(sp, ep, dp, sf);
        else if (len * cs.size() < 512 + (expand ? 512 : 0)) replacec_help_simd_foreach<converse,use_simd_replace>(sp, ep, dp, cs, rf, sf);
        else if (!expand) replacec_help_simd_ascii_table<converse,use_simd_replace>(sp, ep, dp, cs, rf, sf);
        else replacec_help_simd_expand_table<converse,use_simd_replace>(sp, ep, dp, cs, rf, sf);

        // 4.处理剩余不足一块的字符
        #pragma clang loop vectorize(disable)
        for (; dp < ep; ++dp, ++sp)
        {
            if (set[*sp]) *dp = rf(*sp);
            else *dp = *sp;
        }
    }

    template <bool converse = false, bool use_simd_replace = true, typename ReplaceFunc, typename SimdFunc> XY_NO_VECTORIZE_GCC
    void replacec_help_simd_r(const uint8* sp, size_t len, uint8* dp, StringView cs, ReplaceFunc&& rf, SimdFunc&& sf) noexcept
    {
        // 1.预处理字符集
        bool expand = false;
        bool set[256]{};
        if constexpr (converse) mem_set(set, char(true), sizeof(set));
        #pragma clang loop vectorize(disable)
        for (uint8 c : cs.range()) set[c] = !converse, expand |= c & 0x80;

        // 2.处理最前面几个非整块对齐的字符，优先对齐写指针
        auto ep = dp; dp += len; sp += len;
        auto ap = (const uint8*)((size_t)dp & ~(BlockSize - 1));
        #pragma clang loop vectorize(disable)
        while (dp > max(ap, ep))
        {
            --dp; --sp;
            if (set[*sp]) *dp = rf(*sp);
            else *dp = *sp;
        }
        if (ap <= ep) return;

        // 3.simd 处理整块
        if (converse && use_simd_replace && cs.empty()) replacec_help_simd_all<true>(sp, ep, dp, sf);
        else if (len * cs.size() < 512 + (expand ? 512 : 0)) replacec_help_simd_foreach<converse,use_simd_replace,true>(sp, ep, dp, cs, rf, sf);
        else if (!expand) replacec_help_simd_ascii_table<converse,use_simd_replace,true>(sp, ep, dp, cs, rf, sf);
        else replacec_help_simd_expand_table<converse,use_simd_replace,true>(sp, ep, dp, cs, rf, sf);

        // 3.处理剩余不足一块的字符
        #pragma clang loop vectorize(disable)
        while (dp > ep)
        {
            --dp; --sp;
            if (set[*sp]) *dp = rf(*sp);
            else *dp = *sp;
        }
    }

    // 回调: rf(c) sf(m)
    template <bool converse = false, bool use_simd_replace = true, typename ReplaceFunc, typename SimdFunc = int> XY_NO_VECTORIZE_GCC
    void replacec_help(const char* sp, size_t len, char* dp, StringView cs, ReplaceFunc&& rf, SimdFunc&& sf = 0) noexcept
    {
        // len > 0, (converse || cs.size() > 0)
        if (cs.size() == 1) return replace_help<converse, use_simd_replace>(sp, len, dp, cs[0], [&]{return rf(cs[0]);}, sf);
        bool use_left = (size_t)dp - (size_t)sp - 1 >= len - 1;
#if defined(__AVX2__) || defined(__SSSE3__) || defined(__ARM_NEON__)
        if (len >= simd_size<option>() * 2 && (!converse || use_simd_replace || !cs.empty()))
        {
            if (use_left) return replacec_help_simd_l<converse,use_simd_replace>((const uint8*)sp, len, (uint8*)dp, cs, rf, sf);
            return replacec_help_simd_r<converse,use_simd_replace>((const uint8*)sp, len, (uint8*)dp, cs, rf, sf);
        }
#endif
        return replacec_help_easy<converse>((const uint8*)sp, len, dp, cs, !use_left, rf);
    }

}

/// replacec(n)
namespace xylu::xystring::__
{
    void replacec(const char* str, size_t len, char* buf, StringView cs, char r) noexcept
    {
        return replacec_help(str, len, buf, cs,
        [&](char){ return r; },
        [&](auto){ return simd_fill<option>(r); });
    }

    void replacec(const char* str, size_t len, char* buf, StringView cs, uint offest) noexcept
    {
        return replacec_help(str, len, buf, cs,
            [&](char s){ return char(s + offest); },
            [&](auto m){ return simd_add1(m, simd_fill<option>((char)offest)); });
    }

    void replacec(const char* str, size_t len, char* buf, StringView cs, char(*f)(char)) noexcept
    { return replacec_help<false,false>(str, len, buf, cs, [&](char s){ return f(s); }); }

    void replacenc(const char* str, size_t len, char* buf, StringView cs, char r) noexcept
    {
        return replacec_help<true>(str, len, buf, cs,
            [&](char){ return r; },
            [&](auto){ return simd_fill<option>(r); });
    }

    void replacenc(const char* str, size_t len, char* buf, StringView cs, uint offest) noexcept
    {
        return replacec_help<true>(str, len, buf, cs,
            [&](char s){ return char(s + offest); },
            [&](auto m){ return simd_add1(m, simd_fill<option>((char)offest)); });
    }

    void replacenc(const char* str, size_t len, char* buf, StringView cs, char(*f)(char)) noexcept
    { return replacec_help<true,false>(str, len, buf, cs, [&](char s){ return f(s); }); }
}

/// 辅助函数
namespace
{
    template <bool converse = false, typename ReplaceFunc> XY_NO_VECTORIZE_GCC
    void replaceu_help_naive(const char* str, size_t len, char* buf, StringView cus, StringView cs, bool reverse, ReplaceFunc&& rf) noexcept
    {
        if (!reverse)
        {
            #pragma clang loop vectorize(disable)
            for (size_t i = 0; i < len; ++i)
            {
                if (converse ^ check_in_range(str[i], cus, cs)) buf[i] = rf(str[i]);
                else buf[i] = str[i];
            }
        }
        else
        {
            #pragma clang loop vectorize(disable)
            for (size_t i = len; i > 0;)
            {
                --i;
                if (converse ^ check_in_range(str[i], cus, cs)) buf[i] = rf(str[i]);
                else buf[i] = str[i];
            }
        }
    }

    template <bool converse = false> XY_NO_VECTORIZE_GCC
    void replaceu_help_easy_table(bool(&set)[256], StringView cus, StringView cs) noexcept
    {
        mem_set(set, converse, 256);
        #pragma clang loop vectorize(disable)
        for (const uint8 *cup = (const uint8*)cus.data(), *cue = cup + (cus.size() & ~size_t(1)); cup < cue; cup += 2)
            mem_set(set + cup[0], !converse, max(cup[1] + 1 - cup[0], 0));
        #pragma clang loop vectorize(disable)
        for (uint8 c : cs.range()) set[c] = !converse;
    }

    template <bool converse = false, typename ReplaceFunc> XY_NO_VECTORIZE_GCC
    void replaceu_help_easy(const uint8* str, size_t len, char* buf, StringView cus, StringView cs, bool reverse, ReplaceFunc&& rf) noexcept
    {
        bool set[256];
        replaceu_help_easy_table<converse>(set, cus, cs);
        if (!reverse)
        {
            #pragma clang loop vectorize(disable)
            for (size_t i = 0; i < len; ++i)
            {
                if (set[str[i]]) buf[i] = rf(str[i]);
                else buf[i] = str[i];
            }
        }
        else
        {
            #pragma clang loop vectorize(disable)
            for (size_t i = len; i > 0;)
            {
                --i;
                if (set[str[i]]) buf[i] = rf(str[i]);
                else buf[i] = str[i];
            }
        }
    }

    template <bool converse = false> XY_NO_VECTORIZE_GCC
    void replaceu_help_simdu_pre_table(bool(&set)[256], bool& expand, StringView cus, StringView cs) noexcept
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
    void replaceu_help_simdu_pre_foreach(bool(&set)[256], uint8& cmin, uint8& cmax, StringView cus, StringView cs) noexcept
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

    template <bool converse = false, bool use_simd_replace = true, bool reverse = false, typename ReplaceFunc, typename SimdFunc> XY_NO_VECTORIZE_GCC
    void replaceu_help_simd_direct_forsingle(const uint8*& sp, const uint8* ep, uint8*& dp, StringView cus, ReplaceFunc&& rf, SimdFunc&& sf) noexcept
    {
        // 创建匹配向量组
        using M = decltype(simd_fill<option>('0'));
        M range_bound[2] { simd_fill<option>(char(cus[0] - 1)), simd_fill<option>(cus[1]) };

        // 每次处理一块
        while (reverse ? (dp >= ep + BlockSize) : (dp + BlockSize <= ep))
        {
            if constexpr (reverse) dp -= BlockSize, sp -= BlockSize;
            auto ms = simd_loadu<option>(sp);
            auto mask = simd_and(simd_gt(ms, range_bound[0]), simd_not(simd_gt(ms, range_bound[1])));
            replacec_help_simd_result_deal<converse, use_simd_replace>(dp, ms, mask, rf, sf);
            if constexpr (!reverse) dp += BlockSize, sp += BlockSize;
        }
    }

    template <bool converse = false, bool use_simd_replace = true, bool reverse = false, typename ReplaceFunc, typename SimdFunc> XY_NO_VECTORIZE_GCC
    void replaceu_help_simd_direct_foreach(const uint8*& sp, const uint8* ep, uint8*& dp, StringView cus, StringView cs, ReplaceFunc&& rf, SimdFunc&& sf) noexcept
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
        for (M *mu = range_bound; cup < cupe; cup += 2)
        {
            if (XY_UNLIKELY(cup[1] < cup[0])) { range_count -= 2; continue; }
            // 左边界预先 - 1， 使用 gt 匹配
            mu[0] = simd_fill<option>(char(cup[0] - 1));
            mu[1] = simd_fill<option>(cup[1]);
            mu += 2;
        }

        // 每次处理一块
        while (reverse ? (dp >= ep + BlockSize) : (dp + BlockSize <= ep))
        {
            if constexpr (reverse) dp -= BlockSize, sp -= BlockSize;
            auto ms = simd_load<option>(sp);
            auto mask = simd_fill<option>('\0');
            // 判断范围交集
            for (M *mu = range_bound; mu < range_bound + range_count; mu += 2)
                mask = simd_or(simd_and(simd_gt(ms, mu[0]), simd_not(simd_gt(ms, mu[1]))), mask);
            // 判断单字符集
            for (uint i = 0; i < cs.size(); ++i) mask = simd_or(simd_eq(ms, match_eq[i]), mask);
            replacec_help_simd_result_deal<converse, use_simd_replace>(dp, ms, mask, rf, sf);
            if constexpr (!reverse) dp += BlockSize, sp += BlockSize;
        }
    }

    template <bool converse = false, bool use_simd_replace = true, bool reverse = false, typename ReplaceFunc, typename SimdFunc> XY_NO_VECTORIZE_GCC
    void replaceu_help_simd_offest_forsingle(const uint8*& sp, const uint8* ep, uint8*& dp, StringView cus, ReplaceFunc&& rf, SimdFunc&& sf) noexcept
    {
        // 创建匹配向量组
        using M = decltype(simd_fill<option>('0'));
        M range_bound[2] { simd_fill<option>(char(cus[0] + 0x7f)), simd_fill<option>(char(cus[1] + 0x80)) };

        // 每次处理一块
        while (reverse ? (dp >= ep + BlockSize) : (dp + BlockSize <= ep))
        {
            if constexpr (reverse) dp -= BlockSize, sp -= BlockSize;
            auto ms = simd_add1(simd_load<option>(sp), simd_fill<option>('\x80'));
            auto mask = simd_and(simd_gt(ms, range_bound[0]), simd_not(simd_gt(ms, range_bound[1])));
            replacec_help_simd_result_deal<converse, use_simd_replace>(dp, ms, mask, rf, sf);
            if constexpr (!reverse) dp += BlockSize, sp += BlockSize;
        }
    }

    template <bool converse = false, bool use_simd_replace = true, bool reverse = false, typename ReplaceFunc, typename SimdFunc> XY_NO_VECTORIZE_GCC
    void replaceu_help_simd_offest_foreach(const uint8*& sp, const uint8* ep, uint8*& dp, StringView cus, StringView cs, ReplaceFunc&& rf, SimdFunc&& sf) noexcept
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
        for (M *mu = range_bound; cup < cupe; cup += 2)
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
                mu[0] = simd_fill<option>('\x80');
            }
            else mu[0] = simd_fill<option>(char(cup[0] + 0x7f));
            mu[1] = simd_fill<option>(char(cup[1] + 0x80));
            mu += 2;
        }

        // 每次处理一块
        while (reverse ? (dp >= ep + BlockSize) : (dp + BlockSize <= ep))
        {
            if constexpr (reverse) dp -= BlockSize, sp -= BlockSize;
            auto ms = simd_add1(simd_load<option>(sp), simd_fill<option>('\x80'));
            auto mask = simd_fill<option>('\0');
            // 判断范围交集
            for (M *mu = range_bound; mu < range_bound + range_count; mu += 2)
                mask = simd_or(simd_and(simd_gt(ms, mu[0]), simd_not(simd_gt(ms, mu[1]))), mask);
            // 判断单字符集
            for (uint i = 0; i < single_count; ++i) mask = simd_or(simd_eq(ms, match_eq[i]), mask);
            replacec_help_simd_result_deal<converse, use_simd_replace>(dp, ms, mask, rf, sf);
            if constexpr (!reverse) dp += BlockSize, sp += BlockSize;
        }
    }

    template <bool converse = false, bool use_simd_replace = true, bool reverse = false, typename ReplaceFunc, typename SimdFunc> XY_NO_VECTORIZE_GCC
    void replaceu_help_simd_ascii_table(const uint8*& sp, const uint8* ep, uint8*& dp, StringView cus, StringView cs, ReplaceFunc&& rf, SimdFunc&& sf) noexcept
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

        while (reverse ? (dp >= ep + BlockSize) : (dp + BlockSize <= ep))
        {
            if constexpr (reverse) dp -= BlockSize, sp -= BlockSize;
            auto ms = simd_load<option>(sp);
            auto high = simd_rshift8(ms, 4);
            // neon 会对高4字节进行归0检测，而 sse/avx 仅对最高字节，可作为非 ascii 过滤（尽管不会被传递）
            auto mlow = option != simd_Neon ? simd_shuffle(tlow, ms) : simd_shuffle(tlow, simd_and(simd_fill<option>('\x8f'), ms));
            // match = table_low[c & 0x0f] & table_high[c >> 4]
            auto match = simd_and(mlow, simd_shuffle(thigh, high));
            auto mask = simd_eq(match, simd_fill<option>('\0'));
            replacec_help_simd_result_deal<!converse, use_simd_replace>(dp, ms, mask, rf, sf);
            if constexpr (!reverse) dp += BlockSize, sp += BlockSize;
        }
    }

    template <bool converse = false, bool use_simd_replace = true, bool reverse = false, typename ReplaceFunc, typename SimdFunc> XY_NO_VECTORIZE_GCC
    void replaceu_help_simd_expand_table(const uint8*& sp, const uint8* ep, uint8*& dp, StringView cus, StringView cs, ReplaceFunc&& rf, SimdFunc&& sf) noexcept
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

        while (reverse ? (dp >= ep + BlockSize) : (dp + BlockSize <= ep))
        {
            if constexpr (reverse) dp -= BlockSize, sp -= BlockSize;
            auto ms = simd_load<option>(sp);
            auto olow = simd_and(ms, simd_fill<option>('\x0f'));
            auto mhigh = simd_shuffle(thigh, simd_rshift8(ms, 4));
            // choose = c > -1 ? 1 : 0 <==> c & 0x80 ? 0 : 1
            auto choose = simd_gt(ms, simd_fill<option>('\xff'));
            // mlow1 = table_low1[c & 0x0f]
            auto mlow1 = simd_shuffle(tlow1, olow);
            // mlow2 = table_low2[c & 0x0f]
            auto mlow2 = simd_shuffle(tlow2, olow);
            // (choose & (mlow1 & mhigh)) | (~choose & (mlow2 & mhigh))
            // <==> mhigh & (mlow2 ^ ((mlow2 ^ mlow1) & choose))
            auto mlow = simd_xor(mlow2, simd_and(simd_xor(mlow2, mlow1), choose));
            auto mask = simd_eq(simd_and(mlow, mhigh), simd_fill<option>('\0'));
            replacec_help_simd_result_deal<!converse, use_simd_replace>(dp, ms, mask, rf, sf);
            if constexpr (!reverse) dp += BlockSize, sp += BlockSize;
        }
    }

    template <bool converse = false, bool use_simd_replace = true, typename ReplaceFunc, typename SimdFunc> XY_NO_VECTORIZE_GCC
    void replaceu_help_simd_l(const uint8* sp, size_t len, uint8* dp, StringView cus, StringView cs, ReplaceFunc&& rf, SimdFunc&& sf) noexcept
    {
        size_t count = cs.size() + cus.size() / 2;
        bool use_table = count > 1 && len * count > 400;

        // 1.预处理字符集
        bool set[256];
        bool expand = false;
        uint8 cmin = 255, cmax = 0;
        if (use_table) replaceu_help_simdu_pre_table<converse>(set, expand, cus, cs);
        else replaceu_help_simdu_pre_foreach<converse>(set, cmin, cmax, cus, cs);

        // 2.将 最前面几个 非整块对齐 的字符 单独处理
        auto ep = dp + len;
        auto ap = (const uint8*)(((size_t)dp + (BlockSize-1)) & ~(BlockSize-1));
        #pragma clang loop vectorize(disable)
        for (; dp < min(ap, ep); ++dp, ++sp)
        {
            if (set[*sp]) *dp = rf(*sp);
            else *dp = *sp;
        }
        if (ap >= ep) return;

        // 3.处理整块
        if (use_table)
        {
            if (!expand) replaceu_help_simd_ascii_table<converse,use_simd_replace>(sp, ep, dp, cus, cs, rf, sf);
            else replaceu_help_simd_expand_table<converse,use_simd_replace>(sp, ep, dp, cus, cs, rf, sf);
        }
        else
        {
            if (XY_UNLIKELY(cmin > cmax)) replaceu_help_simd_direct_foreach<converse,use_simd_replace>(sp, ep, dp, "", "", rf, sf);
            // [0x00 - 1 = 0xff, 0x7f] ∪ [0x81 - 1 = 0x80, 0xff] 有符号数单调递增
            else if (cmax <= 0x7f || cmin > 0x80)
            {
                if (count <= 1) replaceu_help_simd_direct_forsingle<converse,use_simd_replace>(sp, ep, dp, cus, rf, sf);
                else replaceu_help_simd_direct_foreach<converse,use_simd_replace>(sp, ep, dp, cus, cs, rf, sf);
            }
            else
            {
                if (count <= 1 && cus[0]) replaceu_help_simd_offest_forsingle<converse,use_simd_replace>(sp, ep, dp, cus, rf, sf);
                else replaceu_help_simd_offest_foreach<converse,use_simd_replace>(sp, ep, dp, cus, cs, rf, sf);
            }
        }

        // 4.处理剩余不足一块的字符
        #pragma clang loop vectorize(disable)
        for (; dp < ep; ++dp, ++sp)
        {
            if (set[*sp]) *dp = rf(*sp);
            else *dp = *sp;
        }
    }

    template <bool converse = false, bool use_simd_replace = true, typename ReplaceFunc, typename SimdFunc> XY_NO_VECTORIZE_GCC
    void replaceu_help_simd_r(const uint8* sp, size_t len, uint8* dp, StringView cus, StringView cs, ReplaceFunc&& rf, SimdFunc&& sf) noexcept
    {
        size_t count = cs.size() + cus.size() / 2;
        bool use_table = count > 1 && len * count > 400;

        // 1.预处理字符集
        bool set[256];
        bool expand = false;
        uint8 cmin = 255, cmax = 0;
        if (use_table) replaceu_help_simdu_pre_table<converse>(set, expand, cus, cs);
        else replaceu_help_simdu_pre_foreach<converse>(set, cmin, cmax, cus, cs);

        // 2.处理最前面几个非整块对齐的字符，优先对齐写指针
        auto ep = dp; dp += len; sp += len;
        auto ap = (const uint8*)((size_t)dp & ~(BlockSize - 1));
        #pragma clang loop vectorize(disable)
        while (dp > max(ap, ep))
        {
            --dp; --sp;
            if (set[*sp]) *dp = rf(*sp);
            else *dp = *sp;
        }
        if (ap <= ep) return;

        // 3.处理整块
        if (use_table)
        {
            if (!expand) replaceu_help_simd_ascii_table<converse,use_simd_replace,true>(sp, ep, dp, cus, cs, rf, sf);
            else replaceu_help_simd_expand_table<converse,use_simd_replace,true>(sp, ep, dp, cus, cs, rf, sf);
        }
        else
        {
            if (XY_UNLIKELY(cmin > cmax)) replaceu_help_simd_direct_foreach<converse,use_simd_replace,true>(sp, ep, dp, "", "", rf, sf);
            // [0x00 - 1 = 0xff, 0x7f] ∪ [0x81 - 1 = 0x80, 0xff] 有符号数单调递增
            else if (cmax <= 0x7f || cmin > 0x80)
            {
                if (count <= 1) replaceu_help_simd_direct_forsingle<converse,use_simd_replace,true>(sp, ep, dp, cus, rf, sf);
                else replaceu_help_simd_direct_foreach<converse,use_simd_replace,true>(sp, ep, dp, cus, cs, rf, sf);
            }
            else
            {
                if (count <= 1 && cus[0]) replaceu_help_simd_offest_forsingle<converse,use_simd_replace,true>(sp, ep, dp, cus, rf, sf);
                else replaceu_help_simd_offest_foreach<converse,use_simd_replace,true>(sp, ep, dp, cus, cs, rf, sf);
            }
        }

        // 3.处理剩余不足一块的字符
        #pragma clang loop vectorize(disable)
        while (dp > ep)
        {
            --dp; --sp;
            if (set[*sp]) *dp = rf(*sp);
            else *dp = *sp;
        }
    }

    // 回调: rf(c) sf(m)
    template <bool converse = false, bool use_simd_replace = true, typename ReplaceFunc, typename SimdFunc = int> XY_NO_VECTORIZE_GCC
    void replaceu_help(const char* sp, size_t len, char* dp, StringView cus, StringView cs, ReplaceFunc&& rf, SimdFunc&& sf = 0) noexcept
    {
        // len > 0, (cus.size() >= 2 || cs.size() >= 1)
        if (XY_UNLIKELY(cus.size() < 2)) return replacec_help<converse,use_simd_replace>(sp, len, dp, cs, rf, sf);
        bool use_left = (size_t)dp - (size_t)sp - 1 >= len - 1;
        if (len <= 12) return replaceu_help_naive<converse>(sp, len, dp, cus, cs, !use_left, rf);
#if defined(__AVX2__) || defined(__SSSE3__) || defined(__ARM_NEON__)
        if (len > simd_size<option>())
        {
            if (use_left) return replaceu_help_simd_l<converse,use_simd_replace>((const uint8*)sp, len, (uint8*)dp, cus, cs, rf, sf);
            return replaceu_help_simd_r<converse,use_simd_replace>((const uint8*)sp, len, (uint8*)dp, cus, cs, rf, sf);
        }
#endif
        return replaceu_help_easy<converse>((const uint8*)sp, len, dp, cus, cs, !use_left, rf);
    }

}

/// replaceu(n)
namespace xylu::xystring::__
{
    void replaceu(const char* str, size_t len, char* buf, StringView cus, StringView cs, char r) noexcept
    {
        return replaceu_help(str, len, buf, cus, cs,
        [&](char){ return r; },
        [&](auto){ return simd_fill<option>(r); });
    }

    void replaceu(const char* str, size_t len, char* buf, StringView cus, StringView cs, uint offest) noexcept
    {
        return replaceu_help(str, len, buf, cus, cs,
            [&](char s){ return char(s + offest); },
            [&](auto m){ return simd_add1(m, simd_fill<option>((char)offest)); });
    }

    void replaceu(const char* str, size_t len, char* buf, StringView cus, StringView cs, char(*f)(char)) noexcept
    { return replaceu_help<false,false>(str, len, buf, cus, cs, [&](char s){ return f(s); }); }

    void replacenu(const char* str, size_t len, char* buf, StringView cus, StringView cs, char r) noexcept
    {
        return replaceu_help<true>(str, len, buf, cus, cs,
            [&](char){ return r; },
            [&](auto){ return simd_fill<option>(r); });
    }

    void replacenu(const char* str, size_t len, char* buf, StringView cus, StringView cs, uint offest) noexcept
    {
        return replaceu_help<true>(str, len, buf, cus, cs,
            [&](char s){ return char(s + offest); },
            [&](auto m){ return simd_add1(m, simd_fill<option>((char)offest)); });
    }

    void replacenu(const char* str, size_t len, char* buf, StringView cus, StringView cs, char(*f)(char)) noexcept
    { return replaceu_help<true,false>(str, len, buf, cus, cs, [&](char s){ return f(s); }); }
}
