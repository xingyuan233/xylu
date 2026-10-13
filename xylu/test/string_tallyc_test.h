#pragma once
#include <string>
#include <cstdio>
#include <cstdint>
#include <algorithm>

#include "../detail/xystring/fmt.h"

namespace xyu::bench {

using namespace xylu::xystring;

// 防止结果被编译器优化掉
volatile xyu::size_t g_sink = 0;

// ============================================================
// 测试数据构造
// ============================================================

// 生成可复现的随机文本（小写字母）
inline std::string make_text(xyu::size_t n)
{
    std::string s;
    s.reserve(n);
    uint32_t seed = 0x9E3779B9u;
    for (xyu::size_t i = 0; i < n; ++i) {
        seed = seed * 1664525u + 1013904223u;
        // s.push_back(static_cast<char>('a' + (seed >> 16) % 26));
        s.push_back(0x7f & static_cast<char>(seed >> 16));
    }
    return s;
}

// 生成指定数量的匹配字符集合
inline std::string make_chars(xyu::size_t k)
{
    std::string s;
    s.reserve(k);
    for (xyu::size_t i = 0; i < k; ++i)
        s.push_back(static_cast<char>('a' + i % 26));
    return s;
}

// ============================================================
// 计时工具
// ============================================================

// 返回单次调用平均耗时（纳秒）
template <typename Fn>
inline double bench(Fn&& fn, xyu::size_t iters)
{
    // 预热
    for (xyu::size_t i = 0; i < iters / 8 + 1; ++i)
        g_sink += fn();

    Clock clk;
    clk.start();
    for (xyu::size_t i = 0; i < iters; ++i)
        g_sink += fn();
    auto d = clk.stop();

    // Duration_10ns::count 单位为 10ns
    return double(d.count) * 10.0 / double(iters);
}

// 校验三种实现结果一致
inline bool verify_consistency(StringView sv, StringView cv) noexcept
{
    auto r1 = sv.tallyc (cv);
    auto r2 = sv.tallyc2(cv);
    auto r3 = sv.tallyc3(cv);
    if (r1 != r2 || r2 != r3) xyout << xyfmt("src:{}, match:{}, ret:{},{},{}\n", sv, cv, r1, r2, r3);
    return r1 == r2 && r2 == r3;
}

// ============================================================
// 测试三：长度 × 字符集 的矩阵式综合测试
// ============================================================
inline void test_matrix()
{
    const xyu::size_t lengths[] = { 32, 64, 128, 256, 512, 1024, 2048, 4096 };
    const xyu::size_t counts[]  = { 1, 2, 3, 4, 5, 6, 7, 8 };

    std::printf("== [Matrix] ==\n");
    std::printf("%-10s %-10s %-14s %-14s %-14s\n",
                "len", "chars", "tallyc(ns)", "tallyc2(ns)", "tallyc3(ns)");

    for (auto n : lengths) {
        auto text = make_text(n);
        StringView sv{text.data(), text.size()};

        for (auto k : counts) {
            auto chars = make_chars(k);
            StringView cv{chars.data(), chars.size()};

            if (!verify_consistency(sv, cv)) { std::printf("consistency failed!\n"); continue; }

            size_t times = 1'000'1000 / (n * k);
            if (times < 1000) times = 1000;
            xyu::size_t iters = std::max<xyu::size_t>(64, times);

            double t1 = bench([&]{ return sv.tallyc (cv); }, iters);
            double t2 = bench([&]{ return sv.tallyc2(cv); }, iters);
            double t3 = bench([&]{ return sv.tallyc3(cv); }, iters);

            std::printf("%-10zu %-10zu %-14.2f %-14.2f %-14.2f\n",
                        n, k, t1, t2, t3);
        }
    }
    std::printf("\n");
}

// ============================================================
// 测试四：单位吞吐量（每字节耗时），便于跨长度横向比较
// ============================================================
inline void test_throughput()
{
    const xyu::size_t lengths[] = { 64, 1024, 16384, 262144 };
    const xyu::size_t kChar = 8;

    auto chars = make_chars(kChar);
    StringView cv{chars.data(), chars.size()};

    std::printf("== [Throughput | charset_size=%zu] ==\n", cv.size());
    std::printf("%-10s %-14s %-14s %-14s\n",
                "len", "c1(ns/byte)", "c2(ns/byte)", "c3(ns/byte)");

    for (auto n : lengths) {
        auto text = make_text(n);
        StringView sv{text.data(), text.size()};

        xyu::size_t iters = std::max<xyu::size_t>(8, 16'000'000 / (n + 1));

        double t1 = bench([&]{ return sv.tallyc (cv); }, iters);
        double t2 = bench([&]{ return sv.tallyc2(cv); }, iters);
        double t3 = bench([&]{ return sv.tallyc3(cv); }, iters);

        std::printf("%-10zu %-14.4f %-14.4f %-14.4f\n",
                    n, t1 / n, t2 / n, t3 / n);
    }
    std::printf("\n");
}

// ============================================================
// 入口
// ============================================================
inline void main34()
{
    test_matrix();
    test_throughput();

    // 使用 g_sink 让编译器无法消除整个测试流程
    std::printf("sink=%zu\n", (xyu::size_t)g_sink);
}

} // namespace xyu::bench