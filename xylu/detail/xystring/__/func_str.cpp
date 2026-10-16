#include "../strview.h"

#include "../../xymath/bit.h"
#include "../../xycore/__/simd.h"

using namespace xyu;

/// 经测试后落选 (i9-13900HX, C++17编译)
namespace xylu::xystring::__
{
    // 下方函数的参数均为窗口模式 i ∈ [start, end - tlen]

    // 滑动窗口 (处理 模式串长度 为 2) (不如 find_small)
    static size_t XY_NONNULL(1, 3) find_mvwin2(const uint8* str, size_t len, const uint8* target) noexcept
    {
        const uint32 hp = (target[0] << 16) | target[1];
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
    static size_t XY_NONNULL(1, 3) find_mvwin3(const uint8* str, size_t len, const uint8* target) noexcept
    {
        const uint32 hp = (target[0] << 24) | (target[1] << 16) | (target[2] << 8);
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
    static size_t XY_NONNULL(1, 3) find_mvwin4(const uint8* str, size_t len, const uint8* target) noexcept
    {
        const uint32 hp = (target[0] << 24) | (target[1] << 16) | (target[2] << 8) | target[3];
        uint32 hs = (str[0] << 24) | (str[1] << 16) | (str[2] << 8) | str[3];
        size_t i = 3;
        do {
            ++i;
            if (hs == hp) return i - 4;
            if (i == len) return -1;
            hs = (hs << 8) | str[i];
        } while (true);
    }

    // 首字符匹配 + memcmp (不如 find_easy)
    static size_t XY_NONNULL(1, 3) find_small(const char* str, size_t slen, const char* target, size_t tlen) noexcept
    {
        const char* first = str;
        for (const char* last = str + slen - tlen; str <= last; ++str)
            if (*str == *target && mem_cmp(str, target, tlen) == 0) return str - first;
        return -1;
    }

    namespace
    {
        // 读取 16 位整数
        uint16 load16_unaligned(const void* p) noexcept
        {
            uint16 v;
            __builtin_memcpy(&v, p, 2);
            return v;
        }

        // 读取 32 位整数
        uint32 load32_unaligned(const void* p) noexcept
        {
            uint32 v;
            __builtin_memcpy(&v, p, 4);
            return v;
        }
    }
    // 类型转换 (2 <= tlen <= 4)
    static size_t XY_NONNULL(1, 3) find_change(const char* str, size_t slen, const char* target, size_t tlen) noexcept
    {
        if (tlen == 2)
        {
            const uint16 needle = load16_unaligned(target);
            for (size_t i = 0; i <= slen - 2; ++i)
                if (load16_unaligned(str + i) == needle) return i;
            return -1;
        }

        // Case 4: 查找 4 字节 (转换为 uint32_t 比较)
        if (tlen == 4)
        {
            const uint32 needle = load32_unaligned(target);
            for (size_t i = 0; i <= slen - 4; ++i)
                if (load32_unaligned(str + i) == needle) return i;
            return -1;
        }

        // Case 3: 查找 3 字节 (比较前2字节 + 第3字节)
        if (tlen == 3)
        {
            const uint16 needle_head = load16_unaligned(target);
            const char needle_tail = target[2];

            for (size_t i = 0; i <= slen - 3; ++i) {
                if (load16_unaligned(str + i) == needle_head && str[i+2] == needle_tail) {
                    return i;
                }
            }
            return -1;
        }

        XY_UNREACHABLE;
    }

    // horspool (基本不如 horspool双哈希优化)
    static size_t XY_NONNULL(1, 3) find_horspool_naive(const uint8* str, size_t slen, const uint8* target, size_t tlen) noexcept
    {
        size_t m = tlen - 1;

        auto inner = [=](auto store) mutable -> size_t
        {
            // 1.初始化坏字符跳跃表
            decltype(store) shift[256];
            for (auto& i : shift) i = tlen;

            // 2.计算模式串中每个字符的跳跃值
            for (size_t i = 0; i < m; ++i) shift[target[i]] = m - i;

            // 3.主搜索循环
            const auto* first = str;
            const auto* last = str + slen - tlen;
            while (str <= last)
            {
                if (str[m] == target[m])
                    if (mem_cmp(str, target, tlen) == 0) return str - first;
                str += shift[str[m]];
            }
            // 4.未找到
            return -1;
        };

        if (m <= t_number<uint8>::max) return inner(uint8{});
        if (XY_LIKELY(m <= t_number<uint32>::max)) return inner(uint32{});
        return inner(uint64{});
    }

    // twoway + badchar (在多重复字串的场合表现略好，但由于两头读取，影响缓存与分支预测，在现代CPU上性能很差，C编译可能有所不同？)
    static size_t XY_NONNULL(1, 3) find_twoway(const uint8* str, size_t slen, const uint8* target, size_t tlen) noexcept
    {
        // 1.预计算: 寻找分割点
        // 正向字典序搜索，寻找一个候选项
        size_t max_suffix = -1;
        size_t j = 0, k = 1, p = 1;
        while (j + k < tlen)
        {
            auto a = target[j + k];
            auto b = target[max_suffix + k];
            if (a < b) j += k, k = 1, p = j - max_suffix;
            else if (a == b)
            {
                if (k != p) ++k;
                else j += p, k = 1;
            }
            else
            {
                max_suffix = j++;
                k = p = 1;
            }
        }
        size_t period = p;

        // 反向字典序搜索，寻找另一个候选项
        size_t max_suffix_rev = -1;
        j = 0; k = 1; p = 1;
        while (j + k < tlen)
        {
            auto a = target[j + k];
            auto b = target[max_suffix_rev + k];
            if (b < a) j += k, k = 1, p = j - max_suffix_rev;
            else if (a == b)
            {
                if (k != p) ++k;
                else j += p, k = 1;
            }
            else
            {
                max_suffix_rev = j++;
                k = p = 1;
            }
        }

        // 选择两个候选中较优的一个作为分割点
        size_t suffix;
        if (max_suffix_rev + 1 < max_suffix + 1) suffix = max_suffix + 1;
        else
        {
            suffix = max_suffix_rev + 1;
            period = p;
        }

        // 2.预处理: 构建坏字符跳跃表
        size_t shift_table[256];
        for (auto& i : shift_table) i = tlen;
        for (size_t i = 0; i < tlen; ++i) shift_table[target[i]] = tlen - 1 - i;

        // 3.根据模式串是否具有周期性，选择不同的搜索策略
        if (mem_cmp(target, target + period, suffix) == 0)
        {
            // 策略一: 模式串是周期的。需要一个 memory 变量来记住已匹配的周期部分。
            size_t memory = 0;
            j = 0;
            while (j <= slen - tlen)
            {
                size_t shift = shift_table[str[j + tlen - 1]];
                if (shift > 0) {
                    // 周期性模式串的特殊跳跃，避免跳过潜在匹配
                    if (memory && shift < period) shift = tlen - period;
                    memory = 0;
                    j += shift;
                    continue;
                }

                // 从分割点开始匹配右半部分
                size_t i = max(suffix, memory);
                while (i < tlen - 1 && target[i] == str[j + i]) ++i;

                if (i >= tlen - 1) {
                    // 右半部分匹配成功，再匹配左半部分
                    i = suffix - 1;
                    if (i + 1 > memory) {
                        while (target[i] == str[j + i])
                            if (i-- == memory) break;
                    }
                    // 完整匹配
                    if (i + 1 < memory + 1) return j;
                    // 不匹配，跳过一个周期，并记住右半边已匹配的部分
                    j += period;
                    memory = tlen - period;
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
            period = max(suffix, tlen - suffix) + 1;
            j = 0;
            while (j <= slen - tlen)
            {
                size_t shift = shift_table[str[j + tlen - 1]];
                if (shift > 0) { j += shift; continue; }

                // 从分割点开始匹配右半部分
                size_t i = suffix;
                while (i < tlen - 1 && target[i] == str[j + i]) ++i;

                if (i >= tlen - 1) {
                    // 右半部分匹配成功，再匹配左半部分
                    i = suffix - 1;
                    while (i != -1 && target[i] == str[j + i]) --i;
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

    // twoway (失去了 badchar 的强大支撑，在现代CPU上性能极差，C编译可能有所不同？)
    static size_t XY_NONNULL(1, 3) find_twoway_naive(const uint8* str, size_t slen, const uint8* target, size_t tlen) noexcept
    {
        // 1.预计算: 寻找分割点
        // 正向字典序搜索，寻找一个候选项
        size_t max_suffix = -1;
        size_t j = 0, k = 1, p = 1;
        while (j + k < tlen)
        {
            auto a = target[j + k];
            auto b = target[max_suffix + k];
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
        while (j + k < tlen)
        {
            auto a = target[j + k];
            auto b = target[max_suffix_rev + k];
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
        if (mem_cmp(target, target + period, suffix) == 0)
        {
            // 策略一: 模式串是周期的。
            // 跳跃完全依赖于 period 和 memory。
            size_t memory = 0;
            j = 0;
            while (j <= slen - tlen)
            {
                // 从分割点开始匹配右半部分
                size_t i = max(suffix, memory);
                while (i < tlen && target[i] == str[j + i]) ++i;

                if (i >= tlen) { // 右半部分匹配成功
                    // 再匹配左半部分
                    i = suffix - 1;
                    while (i + 1 > memory && target[i] == str[j + i]) {
                        if (i == memory) break; // 防止无符号数下溢
                        --i;
                    }
                    // 完整匹配
                    if (i + 1 < memory + 1) return j;

                    // 不匹配，跳过一个周期，并记住右半边已匹配的部分
                    j += period;
                    memory = tlen - period;
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
            period = (suffix > tlen - suffix ? suffix : tlen - suffix) + 1;
            j = 0;
            while (j <= slen - tlen)
            {
                // 从分割点开始匹配右半部分
                size_t i = suffix;
                while (i < tlen && target[i] == str[j + i]) ++i;

                if (i >= tlen) { // 右半部分匹配成功
                    // 再匹配左半部分
                    i = suffix - 1;
                    // 使用 `i != (size_t)-1` 来处理无符号数的循环终止条件
                    while (i != -1 && target[i] == str[j + i]) --i;
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

}

/// find
namespace xylu::xystring::__
{
    // memchr + memcmp
    static size_t XY_NONNULL(1, 3) find_easy(const char* str, size_t slen, const char* target, size_t tlen) noexcept
    {
        const char *start = str, *last = str + slen - tlen, *end = last + 1;
        while (start <= last)
        {
            if (mem_find(start, target[0], end - start) == -1) break;
            if (mem_cmp(start, target, tlen) == 0) return start - str;
            ++start;
        }
        return -1;
    }

    // horspool + 双哈希 (字符随机，字符集大的情况下性能很好) (tlen > 1)
    static size_t XY_NONNULL(1, 3) find_horspool(const uint8* str, size_t slen, const uint8* target, size_t tlen) noexcept
    {
        size_t m = tlen - 1;

        auto hasher = [](const uint8* p) -> uint8
        {
            return ((uint)p[0] - ((uint)p[-1] << 3));
        };

        auto inner = [=](auto store) mutable -> size_t
        {
            // 1.预处理: 计算坏字符位移表
            decltype(store) shift[256]{0};
            for (size_t i = 1; i < m; i++)
                shift[hasher(target + i)] = i;

            // 字符串末尾哈希值
            uint8 he = hasher(target + m);
            // 末尾匹配但模式串不全部匹配时的跳跃距离
            size_t sk = m - shift[he];
            shift[he] = m;

            // 2.主搜索循环
            const auto* p = str;
            const auto* last = str + slen - tlen;
            size_t tmp, offset = 0;
            while (p <= last)
            {
                // 跳过未出现的字符
                do {
                    p += m;
                    tmp = shift[hasher(p)];
                } while (tmp == 0 && p <= last);

                p -= tmp;
                // 中间出现的字符，更新窗口位置到 s + (m - tmp) = (s + m) - tmp = p - tmp
                if (tmp < m) continue;

                // 轮转预匹配
                if (m < 15 || mem_cmp(p + offset, target + offset, 8) == 0)
                {
                    if (mem_cmp(p, target, m) == 0)
                        return p - str;
                    offset = (offset >= 8 ? offset : m) - 8;
                }

                p += sk;
            }

            // 3.未找到
            return -1;
        };

        if (m <= t_number<uint8>::max) return inner(uint8{});
        if (XY_LIKELY(m <= t_number<uint32>::max)) return inner(uint32{});
        return inner(uint64{});
    }

    // simd 优化
    template <simd_Type option> XY_NO_VECTORIZE_GCC
    static size_t XY_NONNULL(1, 3) find_simd(const char* str, size_t slen, const char* target, size_t tlen) noexcept
    {
        constexpr size_t BlockSize = simd_size<option>();

        // 1.预处理: 将 最前面几个 非整块对齐 的字符 单独处理
        auto ap = (const char*)(((size_t)str + (BlockSize-1)) & ~(BlockSize-1));
        auto ep = str + slen - tlen + 1, p = str;
        for (; p < min(ap, ep); ++p)
            if (p[0] == target[0] && p[tlen - 1] == target[tlen - 1] &&
                mem_cmp(p, target, tlen) == 0) return p - str;
        if (ap >= ep) return -1;

        if (tlen <= BlockSize)
        {
            // 2.初始化: 将 target 的 第1个字符 和 最后1个字符 广播到完整向量
            const auto v_first_ch = simd_fill<option>(target[0]);
            const auto v_last_ch = simd_fill<option>(target[tlen - 1]);

            // 3.每次处理一块，通过首尾字符匹配来快速筛选出可能的位置，然后通过 mem_cmp 进行精确匹配
            while (p + BlockSize <= ep)
            {
                const auto block_first = simd_load<option>(p);
                const auto block_last = simd_loadu<option>(p + tlen - 1);

                const auto eq_first = simd_eq(v_first_ch, block_first);
                const auto eq_last = simd_eq(v_last_ch, block_last);

                uint32 potential = simd_msb(simd_and(eq_first, eq_last));

                while (potential)
                {
                    const size_t offset = bit_count_0_low(potential);
                    if (mem_cmp(p + offset, target, tlen) == 0) return p - str + offset;
                    potential &= potential - 1;
                }
                p += BlockSize;
            }
        }
        else
        {
            // 2.初始化: 将 target 的 第1个字符 和 最后1个对齐字符 广播到完整向量
            size_t ftlen = (tlen - 1) & ~(BlockSize-1);
            const auto v_first_ch = simd_fill<option>(target[0]);
            const auto v_last_ch = simd_fill<option>(target[ftlen]);

            // 3.每次处理一块，通过首尾字符匹配来快速筛选出可能的位置，然后通过 mem_cmp 进行精确匹配
            while (p + BlockSize <= ep)
            {
                const auto block_first = simd_load<option>(p);
                const auto block_last = simd_load<option>(p + ftlen);

                const auto eq_first = simd_eq(v_first_ch, block_first);
                const auto eq_last = simd_eq(v_last_ch, block_last);

                uint32 potential = simd_msb(simd_and(eq_first, eq_last));

                while (potential)
                {
                    const size_t offset = bit_count_0_low(potential);
                    if (p[offset + tlen - 1] == target[tlen - 1] &&
                        mem_cmp(p + offset + ftlen, target + ftlen, tlen - ftlen) == 0 &&
                        mem_cmp(p + offset, target, tlen) == 0) return p - str + offset;
                    potential &= potential - 1;
                }
                p += BlockSize;
            }
        }

        // 4.处理剩余不足一块的字符
        for (; p < ep; ++p)
            if (p[0] == target[0] && p[tlen - 1] == target[tlen - 1] &&
                mem_cmp(p, target, tlen) == 0) return p - str;

        // 5.未找到
        return -1;
    }

#if defined(__AVX2__)
    // avx2 优化 (一次比较32个字节) (绝大多数情况下性能很好)
    static size_t XY_NONNULL(1, 3) find_avx(const char* str, size_t slen, const char* target, size_t tlen) noexcept
    { return find_simd<simd_Avx>(str, slen, target, tlen); }
#endif
#if defined(__SSE2__) || defined(__ARM_NEON__)
    // sse2 优化 (一次比较16个字节) (大多数情况下性能很好)
    static size_t XY_NONNULL(1, 3) find_sse(const char* str, size_t slen, const char* target, size_t tlen) noexcept
    { return find_simd<simd_Sse | simd_Neon>(str, slen, target, tlen); }
#endif

    // 转发函数
    size_t XY_NONNULL(1, 3) find(const char* str, size_t slen, const char* target, size_t tlen) noexcept
    {
        // slen > 0 && tlen > 0 && slen >= tlen
#if defined(__AVX2__)
        if (slen >= 256) return find_avx(str, slen, target, tlen);
#endif
        if (tlen == 1) return mem_find(str, target[0], slen);
#if defined(__SSE2__) || defined(__ARM_NEON__)
        if (tlen >= 32) return find_sse(str, slen, target, tlen);
#endif
        if (tlen * 4 < slen) return find_horspool((const uint8*)str, slen, (const uint8*)target, tlen);
        return find_easy(str, slen, target, tlen);
    }
}

/// rfind
namespace xylu::xystring::__
{
    // 单字符匹配
    static size_t XY_NONNULL(1) rfind_char(const char* str, size_t slen, char ch) noexcept
    {
        for (const char* last = str + slen - 1; last >= str; --last)
            if (*last == ch) return last - str;
        return -1;
    }

    // 首字符匹配 + memcmp (大部分情况下不如 horspool)
    static size_t XY_NONNULL(1, 3) rfind_small(const char* str, size_t slen, const char* target, size_t tlen) noexcept
    {
        for (const char* last = str + slen - tlen; str <= last; --last)
            if (*last == *target && mem_cmp(last, target, tlen) == 0) return last - str;
        return -1;
    }

    template <simd_Type option>
    static size_t XY_NONNULL(1, 3) rfind_simd(const char* str, size_t slen, const char* target, size_t tlen) noexcept
    {
        constexpr size_t BlockSize = simd_size<option>();

        // 1.处理最后面不足一块的字符
        auto ep = str + slen - tlen + 1, p = ep;
        auto ap = (const char*)((size_t)ep & ~(BlockSize - 1));
        while (p > max(ap, str))
        {
            --p;
            if (p[0] == target[0] && p[tlen - 1] == target[tlen - 1] &&
                mem_cmp(p, target, tlen) == 0) return p - str;
        }
        if (ap <= str) return -1;

        if (tlen <= BlockSize)
        {
            // 2.初始化: 将 target 的 第1个字符 和 最后1个字符 广播到完整向量
            const auto v_first_ch = simd_fill<option>(target[0]);
            const auto v_last_ch = simd_fill<option>(target[tlen - 1]);

            // 3.每次处理一块，通过首尾字符匹配来快速筛选出可能的位置，然后通过 mem_cmp 进行精确匹配
            while (p >= str + BlockSize)
            {
                p -= BlockSize;

                const auto block_first = simd_load<option>(p);
                const auto block_last = simd_loadu<option>(p + tlen - 1);

                const auto eq_first = simd_eq(v_first_ch, block_first);
                const auto eq_last = simd_eq(v_last_ch, block_last);

                uint32 potential = simd_msb(simd_and(eq_first, eq_last));

                while (potential)
                {
                    uint offset = 31 - bit_count_0_high(potential);
                    if (mem_cmp(p + offset, target, tlen) == 0) return p - str + offset;
                    potential &= ~(uint32(1) << offset);
                }
            }
        }
        else
        {
            // 2.初始化: 将 target 的 第1个字符 和 最后1个对齐字符 广播到完整向量
            const auto index = (tlen - 1) & ~(BlockSize - 1);
            const auto v_first_ch = simd_fill<option>(target[0]);
            const auto v_last_ch = simd_fill<option>(target[index]);

            // 3.每次处理一块，通过首尾字符匹配来快速筛选出可能的位置，然后通过 mem_cmp 进行精确匹配
            while (p >= str + BlockSize)
            {
                p -= BlockSize;

                const auto block_first = simd_load<option>(p);
                const auto block_last = simd_load<option>(p + index);

                const auto eq_first = simd_eq(v_first_ch, block_first);
                const auto eq_last = simd_eq(v_last_ch, block_last);

                uint32 potential = simd_msb(simd_and(eq_first, eq_last));

                while (potential)
                {
                    uint offset = 31 - bit_count_0_high(potential);
                    if (mem_cmp(p + offset, target, tlen) == 0) return p - str + offset;
                    potential &= ~(uint32(1) << offset);
                }
            }
        }

        // 4.处理最前面不对齐的字符
        while (p > str)
        {
            --p;
            if (p[0] == target[0] && p[tlen - 1] == target[tlen - 1] &&
                mem_cmp(p, target, tlen) == 0) return p - str;
        }

        // 5.未找到
        return -1;
    }

#if defined(__AVX2__)
    // avx2 优化 (一次比较32个字节)
    static size_t XY_NONNULL(1, 3) rfind_avx(const char* str, size_t slen, const char* target, size_t tlen) noexcept
    { return rfind_simd<simd_Avx>(str, slen, target, tlen); }
#endif
#if defined(__SSE2__) || defined(__ARM_NEON__)
    // sse2 优化 (一次比较16个字节)
    static size_t XY_NONNULL(1, 3) rfind_sse(const char* str, size_t slen, const char* target, size_t tlen) noexcept
    { return rfind_simd<simd_Sse | simd_Neon>(str, slen, target, tlen); }
#endif

    // horspool
    static size_t XY_NONNULL(1, 3) rfind_horspool_naive(const uint8* str, size_t slen, const uint8* target, size_t tlen) noexcept
    {
        size_t m = tlen - 1;

        auto inner = [=](auto store) -> size_t
        {
            // 1. 初始化坏字符跳跃表
            decltype(store) shift[256];
            for (auto& i : shift) i = tlen;

            // 2. 计算模式串中每个字符的跳跃值
            for (size_t i = m; i > 0; --i) shift[target[i]] = i;

            // 3. 主搜索循环
            const uint8* last = str + slen - tlen;
            while (last >= str)
            {
                if (last[0] == target[0])
                    if (mem_cmp(last, target, tlen) == 0) return last - str;
                last -= shift[last[0]];
            }

            // 4. 未找到
            return -1;
        };

        // 根据模式串长度选择移位表存储类型，避免溢出
        if (m < t_number<uint8>::max) return inner(uint8{});
        if (XY_LIKELY(m < t_number<uint32>::max)) return inner(uint32{});
        return inner(uint64{});
    }

    // horspool + 双哈希 (tlen >= 2)
    static size_t XY_NONNULL(1, 3) rfind_horspool(const uint8* str, size_t slen, const uint8* target, size_t tlen) noexcept
    {
        size_t m = tlen - 1;

        auto hasher = [](const uint8* p) -> uint8
        {
            return ((uint)p[0] - ((uint)p[1] << 3));
        };

        auto inner = [=](auto store) mutable -> size_t
        {
            // 1.初始化：分配并初始化表空间
            decltype(store) shift[256];
            // 开头字符对哈希值不同时，不能保证其首字符和模式串尾字符不相同，因此安全的跳跃距离为 tlen - 1
            for (auto& s : shift) s = tlen - 1;

            // 2.预处理: 计算坏字符位移表
            for (size_t i = tlen - 2; i > 0; i--)
                // 基于字符对(target[i], target[i+1])计算哈希，并存入位移表
                shift[hasher(target + i)] = i;

            // 额外存储字符对的哈希匹对匹配，但模式串不全部匹配时的跳跃距离
            uint8 hs = hasher(target);
            size_t sk = shift[hs];
            shift[hs] = 0;

            // 3.主搜索循环
            const auto* last = str + slen - tlen;
            size_t tmp;
            while (last >= str)
            {
                // 跳过开头字符对完全不存在的区域
                tmp = shift[((last[0] - (last[1] << 3)) & 255)];
                if (tmp == m)
                {
                    last -= m;
                    continue;
                }
                // 开头字符对完全匹配
                if (tmp == 0)
                {
                    if (mem_cmp(last + 1, target + 1, m) == 0) return last - str;
                    last -= sk;
                }
                else last -= tmp;
            }

            // 4.未找到
            return -1;
        };

        if (tlen <= t_number<uint8>::max) return inner(uint8{});
        if (XY_LIKELY(tlen <= t_number<uint32>::max)) return inner(uint32{});
        return inner(uint64{});
    }

    // 转发函数
    size_t XY_NONNULL(1, 3) rfind(const char* str, size_t slen, const char* target, size_t tlen) noexcept
    {
        // slen > 0 && tlen > 0 && slen >= tlen
        if (XY_UNLIKELY(tlen == 1)) return rfind_char(str, slen, target[0]);
#if defined(__AVX2__)
        if (slen >= 384) return rfind_avx(str, slen, target, tlen);
#endif
#if defined(__SSE2__) || defined(__ARM_NEON__)
        return rfind_sse(str, slen, target, tlen);
#endif
        if (slen >= 1024 && tlen >= 96)
            return rfind_horspool((const uint8*)str, slen, (const uint8*)target, tlen);
        return rfind_horspool_naive((const uint8*)str, slen, (const uint8*)target, tlen);
    }
}
