#pragma once

/**
 * @file    string_replace_char_test.h
 * @brief   测试单字符全部替换
 * @date    2026-10-05
 */

#include <cstdio>
#include <algorithm>
#include "../detail/xystring/strsso.h"
#include "../detail/xystring/fmt.h"
#include "../detail/xysystem/file.h"
#include "../detail/xysystem/time.h"

using namespace xyu;

// 手动向量化 > 自动向量化(朴素循环) >> find+memcpy

void replace_char(const String& src, String& dst, char sc, char dc) noexcept;
void replace_char2(const String& src, String& dst, char sc, char dc) noexcept;
void replace_char3(const String& src, String& dst, char sc, char dc) noexcept;

String make_source(size_t n, double ratio, int dist)
{
    String s{n, 'a'};               // 分配 n 个字符，全部填充 'a'
    char* p = s.data();
    size_t match_count = static_cast<size_t>(n * ratio);

    if (dist == 0) {                // 均匀分布
        if (match_count > 0) {
            size_t step = n / match_count;
            if (step == 0) step = 1;
            for (size_t i = 0; i < n; i += step) {
                p[i] = 'x';
            }
        }
    } else if (dist == 1) {         // 集中在开头
        for (size_t i = 0; i < match_count && i < n; ++i) p[i] = 'x';
    } else if (dist == 2) {         // 集中在结尾
        for (size_t i = n - match_count; i < n; ++i) p[i] = 'x';
    } else if (dist == 3) {         // 交替分布（偶数位置）
        size_t set_count = 0;
        for (size_t i = 0; i < n && set_count < match_count; i += 2) {
            p[i] = 'x';
            ++set_count;
        }
    }
    return s;
}

void benchmark_replace_char()
{
    const size_t sizes[] = {16, 64, 256, 1024, 4096, 16384, 65536, 262144, 1048576};
    const double ratios[] = {0.0, 0.01, 0.1, 0.5, 1.0};
    const int dists[] = {0, 1, 2, 3};       // 0:均匀 1:开头 2:结尾 3:交替
    const bool self_refs[] = {false, true};

    char sc = 'x';
    char dc = 'y';

    printf("Size\tRatio\tDist\tSelfRef\treplace_char\treplace_char2\treplace_char3\tFastest\n");

    for (size_t n : sizes) {
        for (double ratio : ratios) {
            for (int dist : dists) {
                for (bool self : self_refs) {
                    String orig = make_source(n, ratio, dist);

                    // 根据长度调整重复次数
                    int repeats = std::max(1, (int)(10000000 / n));
                    if (repeats > 10000) repeats = 10000;
                    if (repeats < 100) repeats = 100;

                    double avg_times[3] = {0.0, 0.0, 0.0};

                    // 正确性检查
                    {
                        String dst[3];
                        replace_char(orig, dst[0], sc, dc);
                        replace_char2(orig, dst[1], sc, dc);
                        replace_char3(orig, dst[2], sc, dc);

                        bool eqs[3] { dst[0] == dst[1], dst[0] == dst[2], dst[1] == dst[2] };
                        if (!(eqs[0] && eqs[1] && eqs[2]))
                        {
                            xyout << xyfmt("n={}, ratio={:.2}, dist={}, self={} {} error\n", n, ratio, dist, self,
                                [&]
                                {
                                    if (eqs[2]) return "fun1";
                                    if (eqs[1]) return "fun2";
                                    if (eqs[0]) return "fun3";
                                    return "funs";
                                }());
                        }
                    }

                    // 依次测试三个函数
                    for (int f = 0; f < 3; ++f) {
                        Clock clock;

                        if (self) {
                            // 自引用：每次都需要新的 src（因为会被修改）
                            for (int r = 0; r < repeats; ++r) {
                                String src = orig;
                                String& dst = src;
                                clock.start();
                                if (f == 0) replace_char(src, dst, sc, dc);
                                else if (f == 1) replace_char2(src, dst, sc, dc);
                                else replace_char3(src, dst, sc, dc);
                                clock.stop();
                            }
                        } else {
                            // 非自引用：src 不变，dst 可复用
                            String src = orig;
                            String dst;
                            for (int r = 0; r < repeats; ++r) {
                                clock.start();
                                if (f == 0) replace_char(src, dst, sc, dc);
                                else if (f == 1) replace_char2(src, dst, sc, dc);
                                else replace_char3(src, dst, sc, dc);
                                clock.stop();
                            }
                        }

                        auto total = clock.total();
                        avg_times[f] = static_cast<double>(total.count) / repeats;
                    }

                    // 找出最快的实现
                    int fastest = 0;
                    if (avg_times[1] < avg_times[fastest]) fastest = 1;
                    if (avg_times[2] < avg_times[fastest]) fastest = 2;

                    const char* fname[] = {"replace_char", "replace_char2", "replace_char3"};
                    printf("%zu\t%.2f\t%d\t%s\t%.2f\t%.2f\t%.2f\t%s\n",
                           n, ratio, dist, self ? "true" : "false",
                           avg_times[0], avg_times[1], avg_times[2],
                           fname[fastest]);
                }
            }
        }
    }
}

