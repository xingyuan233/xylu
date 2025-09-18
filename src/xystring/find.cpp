#include "../../head/xystring/strfun.h"
#include "../../link/bitfun"
#include <immintrin.h>

using namespace xyu;

/// 经测试后落选 (i9-13900HX)
namespace xylu::xystring::__
{
    // 滑动窗口 (处理 模式串长度 为 2) (不如 find_small)
    XY_NONNULL(1, 3) XY_COLD
    static size_t find_mvwin2(const uint8* str, size_t len, const uint8* pattern) noexcept
    {
        const uint32 hp = (pattern[0] << 16) | pattern[1];
        uint32 hs = (str[0] << 16) | str[1];
        size_t i = 1;
        do {
            ++i;
            if (hs == hp) return i - 2;
            if (i == len) return -1;
            hs = (hs << 16) | str[i];
        } while (true);
    }

    // 滑动窗口 (处理 模式串长度 为 3) (不如 find_small)
    XY_NONNULL(1, 3) XY_COLD
    static size_t find_mvwin3(const uint8* str, size_t len, const uint8* pattern) noexcept
    {
        const uint32 hp = (pattern[0] << 24) | (pattern[1] << 16) | (pattern[2] << 8);
        uint32 hs = (str[0] << 24) | (str[1] << 16) | (str[2] << 8);
        size_t i = 2;
        do {
            ++i;
            if (hs == hp) return i - 3;
            if (i == len) return -1;
            hs = (hs | str[i]) << 8;
        } while (true);
    }

    // 滑动窗口 (处理 模式串长度 为 4) (不如 find_small)
    XY_NONNULL(1, 3) XY_COLD
    static size_t find_mvwin4(const uint8* str, size_t len, const uint8* pattern) noexcept
    {
        const uint32 hp = (pattern[0] << 24) | (pattern[1] << 16) | (pattern[2] << 8) | pattern[3];
        uint32 hs = (str[0] << 24) | (str[1] << 16) | (str[2] << 8) | str[3];
        size_t i = 3;
        do {
            ++i;
            if (hs == hp) return i - 4;
            if (i == len) return -1;
            hs = (hs << 8) | str[i];
        } while (true);
    }

    // twoway + badchar (在多重复字串的场合表现略好，但由于两头读取，影响缓存与分支预测，在现代CPU上性能很差)
    XY_NONNULL(1, 3) XY_COLD
    static size_t find_twoway(const uint8* str, size_t slen, const uint8* pattern, size_t plen) noexcept
    {
        // 1.预计算: 寻找分割点
        // 正向字典序搜索，寻找一个候选项
        size_t max_suffix = -1;
        size_t j = 0, k = 1, p = 1;
        while (j + k < plen) {
            uint8 a = pattern[j + k];
            uint8 b = pattern[max_suffix + k];
            if (a < b) {
                j += k, k = 1, p = j - max_suffix;
            }
            else if (a == b) {
                if (k != p) ++k;
                else j += p, k = 1;
            }
            else {
                max_suffix = j++;
                k = p = 1;
            }
        }
        size_t period = p;

        // 反向字典序搜索，寻找另一个候选项
        size_t max_suffix_rev = -1;
        j = 0; k = 1; p = 1;
        while (j + k < plen) {
            uint8 a = pattern[j + k];
            uint8 b = pattern[max_suffix_rev + k];
            if (b < a) {
                j += k, k = 1, p = j - max_suffix_rev;
            }
            else if (a == b) {
                if (k != p) ++k;
                else j += p, k = 1;
            }
            else {
                max_suffix_rev = j++;
                k = p = 1;
            }
        }

        // 选择两个候选中较优的一个作为分割点
        size_t suffix;
        if (max_suffix_rev + 1 < max_suffix + 1) suffix = max_suffix + 1;
        else {
            suffix = max_suffix_rev + 1;
            period = p;
        }

        // 2.预处理: 构建坏字符跳跃表
        size_t shift_table[256];
        for (auto& i : shift_table) i = plen;
        for (size_t i = 0; i < plen; ++i) shift_table[pattern[i]] = plen - 1 - i;

        // 3.根据模式串是否具有周期性，选择不同的搜索策略
        if (mem_cmp(pattern, pattern + period, suffix) == 0)
        {
            // 策略一: 模式串是周期的。需要一个 memory 变量来记住已匹配的周期部分。
            size_t memory = 0;
            j = 0;
            while (j <= slen - plen) {
                size_t shift = shift_table[str[j + plen - 1]];
                if (shift > 0) {
                    // 周期性模式串的特殊跳跃，避免跳过潜在匹配
                    if (memory && shift < period) shift = plen - period;
                    memory = 0;
                    j += shift;
                    continue;
                }

                // 从分割点开始匹配右半部分
                size_t i = max(suffix, memory);
                while (i < plen - 1 && pattern[i] == str[j + i]) ++i;

                if (i >= plen - 1) {
                    // 右半部分匹配成功，再匹配左半部分
                    i = suffix - 1;
                    if (i + 1 > memory) {
                        while (pattern[i] == str[j + i])
                            if (i-- == memory) break;
                    }
                    // 完整匹配
                    if (i + 1 < memory + 1) return j;
                    // 不匹配，跳过一个周期，并记住右半边已匹配的部分
                    j += period;
                    memory = plen - period;
                } else {
                    // 右半部分不匹配，正常跳跃
                    j += i - suffix + 1;
                    memory = 0;
                }
            }
        }
        else
        {
            // 策略二: 模式串非周期。逻辑更简单，跳跃可以更大。
            period = max(suffix, plen - suffix) + 1;
            j = 0;
            while (j <= slen - plen) {
                size_t shift = shift_table[str[j + plen - 1]];
                if (shift > 0) { j += shift; continue; }

                // 从分割点开始匹配右半部分
                size_t i = suffix;
                while (i < plen - 1 && pattern[i] == str[j + i]) ++i;

                if (i >= plen - 1) {
                    // 右半部分匹配成功，再匹配左半部分
                    i = suffix - 1;
                    while (i != -1 && pattern[i] == str[j + i]) --i;
                    // 完整匹配
                    if (i == -1) return j;
                    // 不匹配，进行最大化跳跃
                    j += period;
                } else {
                    // 右半部分不匹配，正常跳跃
                    j += i - suffix + 1;
                }
            }
        }

        return -1;
    }

    // twoway (失去了 badchar 的强大支撑，在现代CPU上性能极差)
    XY_NONNULL(1, 3) XY_COLD
    static size_t find_twoway_naive(const uint8* str, size_t slen, const uint8* pattern, size_t plen) noexcept
    {
        // 1.预计算: 寻找分割点
        // 正向字典序搜索，寻找一个候选项
        size_t max_suffix = -1;
        size_t j = 0, k = 1, p = 1;
        while (j + k < plen) {
            uint8 a = pattern[j + k];
            uint8 b = pattern[max_suffix + k];
            if (a < b) {
                j += k, k = 1, p = j - max_suffix;
            }
            else if (a == b) {
                if (k != p) ++k;
                else j += p, k = 1;
            }
            else {
                max_suffix = j++;
                k = p = 1;
            }
        }
        size_t period = p;

        // 反向字典序搜索，寻找另一个候选项
        size_t max_suffix_rev = -1;
        j = 0; k = 1; p = 1;
        while (j + k < plen) {
            uint8 a = pattern[j + k];
            uint8 b = pattern[max_suffix_rev + k];
            if (b < a) {
                j += k, k = 1, p = j - max_suffix_rev;
            }
            else if (a == b) {
                if (k != p) ++k;
                else j += p, k = 1;
            }
            else {
                max_suffix_rev = j++;
                k = p = 1;
            }
        }

        // 选择两个候选中较优的一个作为分割点
        size_t suffix;
        if (max_suffix_rev + 1 < max_suffix + 1) suffix = max_suffix + 1;
        else {
            suffix = max_suffix_rev + 1;
            period = p;
        }

        // 2. 根据模式串是否具有周期性，选择不同的搜索策略
        if (mem_cmp(pattern, pattern + period, suffix) == 0)
        {
            // 策略一: 模式串是周期的。
            // 跳跃完全依赖于 period 和 memory。
            size_t memory = 0;
            j = 0;
            while (j <= slen - plen) {
                // 从分割点开始匹配右半部分
                size_t i = max(suffix, memory);
                while (i < plen && pattern[i] == str[j + i]) ++i;

                if (i >= plen) { // 右半部分匹配成功
                    // 再匹配左半部分
                    i = suffix - 1;
                    while (i + 1 > memory && pattern[i] == str[j + i]) {
                        if (i == memory) break; // 防止无符号数下溢
                        --i;
                    }
                    // 完整匹配
                    if (i + 1 < memory + 1) return j;

                    // 不匹配，跳过一个周期，并记住右半边已匹配的部分
                    j += period;
                    memory = plen - period;
                } else {
                    // 右半部分不匹配，根据不匹配的位置进行跳跃
                    // 并重置 memory
                    j += (i > suffix) ? (i - suffix + 1) : 1;
                    memory = 0;
                }
            }
        }
        else
        {
            // 策略二: 模式串非周期。
            // 跳跃可以更大，逻辑更简单。
            period = (suffix > plen - suffix ? suffix : plen - suffix) + 1;
            j = 0;
            while (j <= slen - plen) {
                // 从分割点开始匹配右半部分
                size_t i = suffix;
                while (i < plen && pattern[i] == str[j + i]) {
                    ++i;
                }

                if (i >= plen) { // 右半部分匹配成功
                    // 再匹配左半部分
                    i = suffix - 1;
                    // 使用 `i != (size_t)-1` 来处理无符号数的循环终止条件
                    while (i != -1 && pattern[i] == str[j + i]) {
                        --i;
                    }
                    // 完整匹配
                    if (i == -1) return j;

                    // 不匹配，进行最大化跳跃 (因为非周期，可以跳得更远)
                    j += period;
                } else {
                    // 右半部分不匹配，根据不匹配的位置进行跳跃
                    j += (i > suffix) ? (i - suffix + 1) : 1;
                }
            }
        }

        return -1; // 未找到
    }

    // horspool (字符随机，字符集大的情况下性能较好，但大部分情况不如 horspool + twohash)
    XY_NONNULL(1, 3) XY_COLD
    static size_t find_horspool_naive(const uint8* str, size_t slen, const uint8* pattern, size_t plen) noexcept
    {
        if (XY_LIKELY(plen <= number_traits<uint>::max)) {
            // 1.初始化坏字符跳跃表
            uint shift[256];
            for (auto& i : shift) i = plen;

            // 2.计算模式串中每个字符的跳跃值
            for (size_t i = 0; i < plen - 1; ++i) {
                shift[pattern[i]] = plen - 1 - i;
            }

            // 3.主搜索循环
            const uint8* first = str;
            const uint8* last = str + slen - plen;
            while (str <= last) {
                // 从后向前比较
                uint8 last_char_in_window = str[plen - 1];
                // 检查是否可能匹配
                if (last_char_in_window == pattern[plen - 1]) {
                    // assert plen > 1
                    if (mem_cmp(str, pattern, plen) == 0) return str - first;
                }
                // 进行跳跃
                str += shift[last_char_in_window];
            }
        } else {
            // 1.初始化坏字符跳跃表
            uint64 shift[256];
            for (auto& i : shift) i = plen;

            // 2.计算模式串中每个字符的跳跃值
            for (size_t i = 0; i < plen - 1; ++i) {
                shift[pattern[i]] = plen - 1 - i;
            }

            // 3.主搜索循环
            const uint8* first = str;
            const uint8* last = str + slen - plen;
            while (str <= last) {
                // 从后向前比较
                uint8 last_char_in_window = str[plen - 1];
                // 检查是否可能匹配
                if (last_char_in_window == pattern[plen - 1]) {
                    // assert plen > 1
                    if (mem_cmp(str, pattern, plen) == 0) return str - first;
                }
                // 进行跳跃
                str += shift[last_char_in_window];
            }
        }
        // 4.未找到
        return -1;
    }
}

namespace xylu::xystring::__
{
    // 首字符匹配 + memcmp
    XY_NONNULL(1, 3)
    static size_t find_small(const char* str, size_t slen, const char* pattern, size_t plen) noexcept
    {
        const char* first = str;
        for (const char* last = str + slen - plen; str <= last; ++str)
            if (*str == *pattern && mem_cmp(str, pattern, plen) == 0) return str - first;
        return -1;
    }

#if defined(__SSE2__)
    // sse2 优化 (一次比较16个字节) (绝大多数情况下性能很好)
    XY_NONNULL(1, 3)
    static size_t find_sse(const char* str, size_t slen, const char* pattern, size_t plen) noexcept
    {
        // 1.初始化: 将 pattern 的 第1个字符 和 最后1个字符 广播到 128位 向量
        const __m128i v_first_ch = _mm_set1_epi8(pattern[0]);
        const __m128i v_last_ch = _mm_set1_epi8(pattern[plen - 1]);

        // 2.预处理: 将 最前面几个 非16对齐 的字符 单独处理
        size_t i = 0;
        auto unaligned_size = reinterpret_cast<size_t>(str) & 15;
        if (unaligned_size) unaligned_size = 16 - unaligned_size;
        for (; i < unaligned_size; ++i)
            if (str[i] == pattern[0] && str[i + plen - 1] == pattern[plen - 1] &&
                mem_cmp(str + i, pattern, plen) == 0) return i;

        // 3.每次处理16个字节，通过首尾字符匹配来快速筛选出可能的位置，然后通过 mem_cmp 进行精确匹配
        while (i + 16 + plen <= slen)
        {
            const __m128i block_first = _mm_load_si128((const __m128i*)(str + i));
            const __m128i block_last = _mm_loadu_si128((const __m128i*)(str + i + plen - 1));

            const __m128i eq_first = _mm_cmpeq_epi8(v_first_ch, block_first);
            const __m128i eq_last = _mm_cmpeq_epi8(v_last_ch, block_last);

            xyu::uint16 potential = _mm_movemask_epi8(_mm_and_si128(eq_first, eq_last));

            while (potential) {
                const size_t offset = bit_count_0_back(potential);
                if (mem_cmp(str + i + offset, pattern, plen) == 0) return i + offset;
                potential &= potential - 1;
            }
            i += 16;
        }

        // 4.处理剩余不足一块的字符
        for (; i <= slen - plen; ++i)
            if (str[i] == pattern[0] && str[i + plen - 1] == pattern[plen - 1] &&
                mem_cmp(str + i, pattern, plen) == 0) return i;

        // 5.未找到
        return -1;
    }
#endif

#if defined(__AVX2__)
    // avx2 优化 (一次比较32个字节) (绝大多数情况下性能很好)
    XY_NONNULL(1, 3)
    static size_t find_avx(const char* str, size_t slen, const char* pattern, xyu::size_t plen) noexcept
    {
        // 1.初始化: 将 pattern 的 第1个字符 和 最后1个字符 广播到 256位 向量
        const __m256i v_first_ch = _mm256_set1_epi8(pattern[0]);
        const __m256i v_last_ch = _mm256_set1_epi8(pattern[plen - 1]);

        // 2.预处理: 将 最前面几个 非32对齐 的字符 单独处理
        size_t i = 0;
        auto unaligned_size = reinterpret_cast<size_t>(str) & 31;
        if (unaligned_size) unaligned_size = 32 - unaligned_size;
        for (; i < unaligned_size; ++i)
            if (str[i] == pattern[0] && str[i + plen - 1] == pattern[plen - 1] &&
                mem_cmp(str + i, pattern, plen) == 0) return i;

        // 3.每次处理32个字节，通过首尾字符匹配来快速筛选出可能的位置，然后通过 mem_cmp 进行精确匹配
        while (i + 32 + plen <= slen)
        {
            const __m256i block_first = _mm256_load_si256((const __m256i*)(str + i));
            const __m256i block_last = _mm256_loadu_si256((const __m256i*)(str + i + plen - 1));

            const __m256i eq_first = _mm256_cmpeq_epi8(v_first_ch, block_first);
            const __m256i eq_last = _mm256_cmpeq_epi8(v_last_ch, block_last);

            uint potential = _mm256_movemask_epi8(_mm256_and_si256(eq_first, eq_last));

            while (potential) {
                const size_t offset = bit_count_0_back(potential);
                if (mem_cmp(str + i + offset, pattern, plen) == 0) return i + offset;
                potential &= potential - 1;
            }
            i += 32;
        }

        // 4.处理剩余不足一块的字符
        for (; i <= slen - plen; ++i)
            if (str[i] == pattern[0] && str[i + plen - 1] == pattern[plen - 1] &&
                mem_cmp(str + i, pattern, plen) == 0) return i;

        // 5.未找到
        return -1;
    }
#endif

    // horspool + 双哈希 (字符随机，字符集大的情况下性能很好)
    XY_NONNULL(1, 3)
    static size_t find_horspool(const uint8* str, size_t slen, const uint8* pattern, size_t plen) noexcept
    {
        size_t m = plen - 1;

        auto inner = [=](auto store) mutable -> size_t
        {
            // 1.预处理: 计算坏字符位移表
            decltype(store) shift[256]{0};
            for (size_t i = 1; i < m; i++)
                // 基于字符对(pattern[i-1], pattern[i])计算哈希，并存入位移表
                shift[((pattern[i] - (pattern[i-1] << 3)) & 255)] = i;

            // 额外存储字符对的哈希匹对匹配，但模式串不全部匹配时的跳跃距离
            uint8 he = (pattern[m] - (pattern[m-1] << 3)) & 255;
            uint8 sk = m - shift[he];
            shift[he] = m;

            // 2.主搜索循环
            const uint8* first = str;
            const uint8* last = str + slen - plen;
            while (str <= last) {
                // 计算当前窗口末尾的字符对哈希，并从位移表中获取跳跃距离
                size_t tmp = shift[((str[m] - (str[m-1] << 3)) & 255)];
                // 哈希匹配
                if (tmp == m) {
                    // 最后一个字符由 倒数第二个字符相同 且 哈希值相同 可以推出
                    if (mem_cmp(str, pattern, m) == 0) return str - first;
                    str += sk;
                }
                // 如果哈希不匹配，tmp是模式串中该哈希最后出现的位置
                else str += m - tmp;
            }

            // 3.未找到
            return -1;
        };

        if (m <= number_traits<uint8>::max) return inner(uint8{});
        else if (XY_LIKELY(m <= number_traits<uint>::max)) return inner(uint{});
        else return inner(uint64{});
    }

    // 转发函数
    XY_NONNULL(1, 3)
    size_t find(const char* str, size_t slen, const char* pattern, size_t plen) noexcept
    {
        if (XY_UNLIKELY(plen == 0)) return 0;
        if (XY_UNLIKELY(plen > slen)) return -1;

        if (plen <= 4) {
            if (XY_UNLIKELY(plen == 1)) return mem_find(str, slen, pattern[0]);
            return find_small(str, slen, pattern, plen);
        }

#if (!defined(__AVX2__) && !defined(__SSE2__))
        return find_horspool((const uint8*)str, slen, (const uint8*)pattern, plen);
#else
        // 当 R 足够大时 (比如说 R >= 40)，horspool 的性能更好 (不同CPU上比值不确定)
        float R = (float)slen / (float)plen;
#if (defined(__AVX2__) && defined(__SSE2__))
        // 设置一个阈值，同时 R取一个较小值，此时 find_horspool 极有可能仍比 simd 慢
        // 但可以 减小 CPU降频，缓存污染，带宽竞争，升温及能耗
        if (slen >= 8192 || (plen >= 80 && R >= 12.f))
             return find_horspool((const uint8*)str, slen, (const uint8*)pattern, plen);
        // 仅当 AVX2 有明显优势时才使用
        else if (R >= 20.f)
            return find_avx(str, slen, pattern, plen);
        else
            return find_sse(str, slen, pattern, plen);
#elif defined(__AVX2__)
        // 设置一个阈值，同时 R取一个较小值，此时 find_horspool 极有可能仍比 simd 慢
        // 但可以 减小 CPU降频，缓存污染，带宽竞争，升温及能耗
        if (slen >= 8192 || (plen >= 80 && R >= 12.f))
             return find_horspool((const uint8*)str, slen, (const uint8*)pattern, plen);
        else return find_avx(str, slen, pattern, plen);
#else // defined(__SSE2__)
        // 设置一个阈值，同时 R取一个较小值，此时 find_horspool 极有可能仍比 simd 慢
        // 但可以 减小 CPU降频，缓存污染，带宽竞争，升温及能耗
        if (slen >= 8192 || (plen >= 48 && R >= 8.f))
             return find_horspool((const uint8*)str, slen, (const uint8*)pattern, plen);
        else return find_sse(str, slen, pattern, plen);
#endif
#endif
    }
}

namespace xylu::xystring::__
{
    // 单字符匹配
    XY_NONNULL(1)
    static size_t rfind_char(const char* str, size_t slen, char ch) noexcept
    {
        for (const char* last = str + slen - 1; last >= str; --last)
            if (*last == ch) return last - str;
        return -1;
    }

    // 首字符匹配 + memcmp
    XY_NONNULL(1, 3)
    static size_t rfind_small(const char* str, size_t slen, const char* pattern, size_t plen) noexcept
    {
        for (const char* last = str + slen - plen; str <= last; --last)
            if (*last == *pattern && mem_cmp(last, pattern, plen) == 0) return last - str;
        return -1;
    }

#if defined(__SSE2__)
    // sse2 优化 (一次比较16个字节)
    XY_NONNULL(1, 3)
    static size_t rfind_sse(const char* str, size_t slen, const char* pattern, size_t plen) noexcept
    {
        // 1.初始化: 将 pattern 的 第1个字符 和 最后1个字符 广播到 128位 向量
        const __m128i v_first_ch = _mm_set1_epi8(pattern[0]);
        const __m128i v_last_ch = _mm_set1_epi8(pattern[plen - 1]);

        // 2.处理最后面不足一块的字符
        size_t i = slen - plen;
        auto sstr = reinterpret_cast<size_t>(str);
        // +1 可以使 i的指针位置%16==15时，刚好有一个完整的块，小于aligned_index，不执行for循环，直接进行第三步
        size_t aligned_index = ((sstr + slen - plen + 1) & ~size_t(15)) - sstr;
        for (; i >= aligned_index; --i) {
            if (str[i] == pattern[0] && str[i + plen - 1] == pattern[plen - 1] &&
                mem_cmp(str + i, pattern, plen) == 0) return i;
            if (XY_UNLIKELY(!i)) return -1;
        }
        ++i; // 退出循环时 i 是 aligned_index-1，需要加1

        // 3.每次处理16个字节，通过首尾字符匹配来快速筛选出可能的位置，然后通过 mem_cmp 进行精确匹配
        while (i >= 16)
        {
            i -= 16;

            const __m128i block_first = _mm_load_si128((const __m128i*)(str + i));
            const __m128i block_last = _mm_loadu_si128((const __m128i*)(str + i + plen - 1));

            const __m128i eq_first = _mm_cmpeq_epi8(v_first_ch, block_first);
            const __m128i eq_last = _mm_cmpeq_epi8(v_last_ch, block_last);

            uint16 potential = _mm_movemask_epi8(_mm_and_si128(eq_first, eq_last));

            while (potential) {
                const uint offset = 31 - bit_count_0_front(potential);
                if (mem_cmp(str + i + offset, pattern, plen) == 0) return i + offset;
                potential &= ~(1u << offset);
            }
        }

        // 4.处理最前面不对其的字符
        for (; i < slen; --i)
            if (str[i] == pattern[0] && str[i + plen - 1] == pattern[plen - 1] &&
                mem_cmp(str + i, pattern, plen) == 0) return i;

        // 5.未找到
        return -1;
    }
#endif

#if defined(__AVX2__)
    // avx2 优化 (一次比较32个字节)
    XY_NONNULL(1, 3)
    static size_t rfind_avx(const char* str, size_t slen, const char* pattern, size_t plen) noexcept
    {
        // 1.初始化: 将 pattern 的 第1个字符 和 最后1个字符 广播到 256位 向量
        const __m256i v_first_ch = _mm256_set1_epi8(pattern[0]);
        const __m256i v_last_ch = _mm256_set1_epi8(pattern[plen - 1]);

        // 2.处理最后面不足一块的字符
        size_t i = slen - plen;
        auto sstr = reinterpret_cast<size_t>(str);
        // +1 可以使 i的指针位置%32==31时，刚好有一个完整的块，小于aligned_index，不执行for循环，直接进行第三步
        size_t aligned_index = ((sstr + slen - plen + 1) & ~size_t(31)) - sstr;
        for (; i >= aligned_index; --i) {
            if (str[i] == pattern[0] && str[i + plen - 1] == pattern[plen - 1] &&
                mem_cmp(str + i, pattern, plen) == 0) return i;
            if (XY_UNLIKELY(!i)) return -1;
        }
        ++i; // 退出循环时 i 是 aligned_index-1，需要加1

        // 3.每次处理32个字节，通过首尾字符匹配来快速筛选出可能的位置，然后通过 mem_cmp 进行精确匹配
        while (i >= 32)
        {
            i -= 32;

            const __m256i block_first = _mm256_load_si256((const __m256i*)(str + i));
            const __m256i block_last = _mm256_loadu_si256((const __m256i*)(str + i + plen - 1));

            const __m256i eq_first = _mm256_cmpeq_epi8(v_first_ch, block_first);
            const __m256i eq_last = _mm256_cmpeq_epi8(v_last_ch, block_last);

            uint32 potential = _mm256_movemask_epi8(_mm256_and_si256(eq_first, eq_last));

            while (potential) {
                uint offset = 31 - bit_count_0_front(potential);
                if (mem_cmp(str + i + offset, pattern, plen) == 0) return i + offset;
                potential &= ~(1u << offset);
            }
        }

        // 4.处理最前面不对其的字符
        for (; i < slen; --i)
            if (str[i] == pattern[0] && str[i + plen - 1] == pattern[plen - 1] &&
                mem_cmp(str + i, pattern, plen) == 0) return i;

        // 5.未找到
        return -1;
    }
#endif

    // horspool + 双哈希
    XY_NONNULL(1, 3)
    static size_t rfind_horspool(const uint8* str, size_t slen, const uint8* pattern, size_t plen) noexcept
    {
        auto inner = [=](auto store) mutable -> size_t
        {
            // 1.初始化：分配并初始化表空间
            decltype(store) shift[256];
            // 开头字符对哈希值不同时，不能保证其首字符和模式串尾字符不相同，因此安全的跳跃距离为 plen - 1
            for (auto& s : shift) s = plen - 1;

            // 2.预处理: 计算坏字符位移表
            for (size_t i = plen - 2; i > 0; i--)
                // 基于字符对(pattern[i], pattern[i+1])计算哈希，并存入位移表
                shift[((pattern[i] - (pattern[i+1] << 3)) & 255)] = i;

            // 额外存储字符对的哈希匹对匹配，但模式串不全部匹配时的跳跃距离
            uint8 hs = (pattern[0] - (pattern[1] << 3)) & 255;
            size_t sk = shift[hs];
            shift[hs] = 0;

            // 3.主搜索循环
            const uint8* last = str + slen - plen;
            while (last >= str) {
                // 计算当前窗口开头的字符对哈希，并从位移表中获取跳跃距离
                size_t tmp = shift[((last[0] - (last[1] << 3)) & 255)];
                // 哈希匹配
                if (tmp == 0) {
                    // 第一个字符由 第二个字符相同 且 哈希值相同 可以推出
                    if (mem_cmp(last + 1, pattern + 1, plen - 1) == 0) return last - str;
                    last -= sk;
                }
                    // 如果哈希不匹配，tmp是模式串中该哈希最先出现的位置
                else last -= tmp;
            }

            // 4.未找到
            return -1;
        };

        if (plen <= number_traits<uint8>::max) return inner(uint8{});
        else if (XY_LIKELY(plen <= number_traits<uint>::max)) return inner(uint{});
        else return inner(uint64{});
    }

    // 转发函数
    XY_NONNULL(1, 3)
    size_t rfind(const char* str, size_t slen, const char* pattern, size_t plen) noexcept
    {
        if (XY_UNLIKELY(plen == 0)) return slen;
        if (XY_UNLIKELY(plen > slen)) return -1;

        // rfind 中，即使 plen 很小，simd优化算法 仍然会比 rfind_small 快很多
        if (XY_UNLIKELY(plen == 1)) return rfind_char(str, slen, pattern[0]);
#if (!defined(__AVX2__) && !defined(__SSE2__))
        // plen 过短时 horspool 对于重复串效果很差，因此 取一个较大值 避免极端情况影响
        if (plen <= 8) return rfind_small(str, slen, pattern, plen);
        else return rfind_horspool((const uint8*)str, slen, (const uint8*)pattern, plen);
#else
        // 当 R 足够大时，horspool 的性能更好 (不同CPU上比值不确定)
        float R = (float)slen / (float)plen;
#if (defined(__AVX2__) && defined(__SSE2__))
        // 设置一个阈值，同时 R取一个较小值，此时 find_horspool 极有可能仍比 simd 慢
        // 但可以 减小 CPU降频，缓存污染，带宽竞争，升温及能耗
        if (slen >= 8192 || (plen >= 80 && R >= 12.f))
            return rfind_horspool((const uint8*)str, slen, (const uint8*)pattern, plen);
        // 仅当 AVX2 有明显优势时使用
        else if (R >= 20.f)
            return rfind_avx(str, slen, pattern, plen);
        else
            return rfind_sse(str, slen, pattern, plen);
#elif defined(__AVX2__)
        // 设置一个较小的阈值，此时 rfind_horspool 极有可能仍比 simd 慢
        // 但可以 减小 CPU降频，缓存污染，带宽竞争，升温及能耗
        if (slen >= 8192 || (plen >= 80 && R >= 12.f))
            return rfind_horspool((const uint8*)str, slen, (const uint8*)pattern, plen);
        else
            return rfind_avx(str, slen, pattern, plen);
#else
        // 设置一个较小的阈值，此时 rfind_horspool 极有可能仍比 simd 慢
        // 但可以 减小 CPU降频，缓存污染，带宽竞争，升温及能耗
        if (slen >= 8192 || (plen >= 48 && R >= 8.f))
            return rfind_horspool((const uint8*)str, slen, (const uint8*)pattern, plen);
        else
            return rfind_sse(str, slen, pattern, plen);
#endif
#endif
    }
}
