#pragma once

/**
 * @file    stingview_find.h
 * @brief   测试字符串查找性能（改进版 v2：多实例、稳健统计）
 * @date    2025-12-27
 */

#include <iostream>
#include <vector>
#include <string>
#include <cstring>
#include <chrono>
#include <random>
#include <iomanip>
#include <cmath>

#include "../detail/xystring/strsso.h"

namespace benchmark_utils {
    inline void do_not_optimize(size_t val) {
        volatile size_t sink = val;
        (void)sink;
    }
}

class StringSearchBenchmark {
public:
    enum class Scenario {
        NORMAL_FOUND,        // needle 确实出现（插入到随机位置）
        NORMAL_NOT_FOUND,    // 随机字符串，但通过重试确保不包含
    };

    struct TestCase {
        size_t slen;
        size_t tlen;
        Scenario scenario;
    };

    // iterations_per_instance: 对每个字符串对重复测量的次数
    // num_instances:           每个测试用例生成的随机实例数量
    explicit StringSearchBenchmark(size_t iterations_per_instance = 10000,
                                   size_t num_instances = 30)
        : m_iters_per_instance(iterations_per_instance),
          m_num_instances(num_instances) {}

    void addDefaultCases() {
        std::vector<std::pair<size_t, size_t>> pairs = {
            {16, 1}, {32, 2}, {64, 3}, {64, 4},{64, 16}, {64, 32},
            {128, 4}, {128, 8},{128, 16}, {128, 32}, {128, 64},
            {256, 8}, {256, 16}, {256, 32}, {256, 64}, {256, 128},
            {1024, 16},  {1024, 32}, {1024, 64}, {1024, 128}, {1024, 256},
            {2048, 16},  {2048, 32}, {2048, 64}, {2048, 128}, {2048, 256}, {2048, 512}, {2048, 1024},
            {4096, 16},  {4096, 32}, {4096, 64}, {4096, 128}, {4096, 256}, {4096, 512}, {4096, 1024}, {4096, 2048},
        };

        for (auto& p : pairs) {
            addCase(p.first, p.second, Scenario::NORMAL_FOUND);
            addCase(p.first, p.second, Scenario::NORMAL_NOT_FOUND);
        }

    }

    void addCase(size_t slen, size_t tlen, Scenario s) {
        m_tests.push_back({slen, tlen, s});
    }

    void run() {
        printHeader();

        for (const auto& tc : m_tests) {
            runTestCase(tc);
        }
    }

private:
    size_t m_iters_per_instance;
    size_t m_num_instances;
    std::vector<TestCase> m_tests;
    std::mt19937 m_rng{std::random_device{}()};

    // 单个字符串对
    struct Instance {
        std::string haystack;
        std::string needle;
    };

    // 生成一个随机字符串（大小写字母 + 数字）
    std::string randomString(size_t len) {
        const char charset[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789";
        std::uniform_int_distribution<size_t> dist(0, sizeof(charset) - 2);
        std::string s(len, '\0');
        for (size_t i = 0; i < len; ++i)
            s[i] = charset[dist(m_rng)];
        return s;
    }

    // 为一个TestCase生成所有实例
    std::vector<Instance> generateInstances(const TestCase& tc) {
        std::vector<Instance> instances;
        instances.reserve(m_num_instances);

        for (size_t i = 0; i < m_num_instances; ++i) {
            Instance inst;
            switch (tc.scenario) {
            case Scenario::NORMAL_FOUND: {
                // haystack 随机，needle 随机，并插入到随机位置
                inst.haystack = randomString(tc.slen);
                inst.needle   = randomString(tc.tlen);
                if (tc.slen >= tc.tlen && tc.tlen > 0) {
                    size_t pos = (tc.slen - tc.tlen) * 3 / 4;
                    memcpy(&inst.haystack[pos], inst.needle.data(), tc.tlen);
                }
                break;
            }
            case Scenario::NORMAL_NOT_FOUND: {
                // 随机生成，确保 needle 不在 haystack 中出现
                inst.haystack = randomString(tc.slen);
                if (tc.tlen > tc.slen) {
                    // needle 比 haystack 长，一定找不到
                    inst.needle = randomString(tc.tlen);
                } else {
                    // 不断重试，直到 haystack.find(needle) == npos
                    do {
                        inst.needle = randomString(tc.tlen);
                    } while (inst.haystack.find(inst.needle) != std::string::npos);
                }
                break;
            }
            }
            instances.push_back(std::move(inst));
        }
        return instances;
    }

    // 计算平均值和标准差
    static std::pair<double, double> computeStats(const std::vector<double>& values) {
        if (values.empty()) return {0.0, 0.0};
        double sum = 0.0;
        for (double v : values) sum += v;
        double mean = sum / values.size();

        double sq_sum = 0.0;
        for (double v : values) sq_sum += (v - mean) * (v - mean);
        double stddev = std::sqrt(sq_sum / values.size());
        return {mean, stddev};
    }

    void runTestCase(const TestCase& tc) {
        auto instances = generateInstances(tc);

        std::vector<double> std_times, xyu_times, strstr_times;
        std_times.reserve(m_num_instances);
        xyu_times.reserve(m_num_instances);
        strstr_times.reserve(m_num_instances);

        const size_t warmup = 50;

        for (const auto& inst : instances) {
            // ---------- 正确性校验 ----------
            {
                size_t expected = inst.haystack.find(inst.needle);

                xyu::String xyu_hay(inst.haystack.data(), inst.haystack.size());
                xyu::String xyu_needle(inst.needle.data(), inst.needle.size());
                size_t xyu_result = xyu_hay.find(xyu_needle);
                if (expected != xyu_result) {
                    std::cerr << "ERROR: mismatch! slen=" << tc.slen
                              << " tlen=" << tc.tlen << " expected=" << expected
                              << " xyu=" << xyu_result << std::endl;
                }
            }

            // ---------- 测量 std::string::find ----------
            for (size_t k = 0; k < warmup; ++k)
                benchmark_utils::do_not_optimize(inst.haystack.find(inst.needle));

            auto t1 = std::chrono::high_resolution_clock::now();
            for (size_t k = 0; k < m_iters_per_instance; ++k) {
                benchmark_utils::do_not_optimize(inst.haystack.find(inst.needle));
            }
            auto t2 = std::chrono::high_resolution_clock::now();
            double ns_std = std::chrono::duration<double, std::nano>(t2 - t1).count()
                            / m_iters_per_instance;
            std_times.push_back(ns_std);

            // ---------- 测量 xyu::String::find ----------
            xyu::String xyu_hay(inst.haystack.data(), inst.haystack.size());
            xyu::String xyu_needle(inst.needle.data(), inst.needle.size());

            for (size_t k = 0; k < warmup; ++k)
                benchmark_utils::do_not_optimize(xyu_hay.find(xyu_needle));

            t1 = std::chrono::high_resolution_clock::now();
            for (size_t k = 0; k < m_iters_per_instance; ++k) {
                benchmark_utils::do_not_optimize(xyu_hay.find(xyu_needle));
            }
            t2 = std::chrono::high_resolution_clock::now();
            double ns_xyu = std::chrono::duration<double, std::nano>(t2 - t1).count()
                            / m_iters_per_instance;
            xyu_times.push_back(ns_xyu);

            // ---------- 测量 __builtin_strstr ----------
            const char* hay_cstr = inst.haystack.c_str();
            const char* ndl_cstr = inst.needle.c_str();

            for (size_t k = 0; k < warmup; ++k)
                benchmark_utils::do_not_optimize((size_t)strstr(hay_cstr, ndl_cstr));

            t1 = std::chrono::high_resolution_clock::now();
            for (size_t k = 0; k < m_iters_per_instance; ++k) {
                benchmark_utils::do_not_optimize((size_t)strstr(hay_cstr, ndl_cstr));
            }
            t2 = std::chrono::high_resolution_clock::now();
            double ns_strstr = std::chrono::duration<double, std::nano>(t2 - t1).count()
                               / m_iters_per_instance;
            strstr_times.push_back(ns_strstr);
        }

        // 统计
        auto [avg_std, sd_std] = computeStats(std_times);
        auto [avg_xyu, sd_xyu] = computeStats(xyu_times);
        auto [avg_strstr, sd_strstr] = computeStats(strstr_times);
        double speedup_xyu = avg_std / avg_xyu;
        double speedup_strstr = avg_strstr / avg_xyu;

        printRow(tc, avg_std, sd_std, avg_xyu, sd_xyu, avg_strstr, sd_strstr,
                 speedup_xyu, speedup_strstr);
    }

    void printHeader() {
        std::cout << std::left
                  << std::setw(8)  << "S_Len"
                  << std::setw(8)  << "T_Len"
                  << std::setw(12) << "Scenario"
                  << " | "
                  << std::setw(16) << "std(ns) avg-sd"
                  << std::setw(16) << "xyu(ns) avg-sd"
                  << std::setw(16) << "strstr(ns) avg-sd"
                  << std::setw(10) << "std/xyu"
                  << std::setw(12) << "strstr/xyu"
                  << std::endl;
        std::cout << std::string(115, '-') << std::endl;
    }

    void printRow(const TestCase& tc,
                  double std_avg, double std_sd,
                  double xyu_avg, double xyu_sd,
                  double strstr_avg, double strstr_sd,
                  double speedup_xyu, double speedup_strstr) {
        const char* scenario_str = "";
        switch (tc.scenario) {
        case Scenario::NORMAL_FOUND:      scenario_str = "Found";      break;
        case Scenario::NORMAL_NOT_FOUND:  scenario_str = "Not Found";  break;
        }

        auto color = [](double val, double baseline) -> const char* {
            return (val >= baseline) ? "\033[32m" : "\033[31m";
        };
        const char* reset = "\033[0m";

        std::cout << std::left
                  << std::setw(8)  << tc.slen
                  << std::setw(8)  << tc.tlen
                  << std::setw(12) << scenario_str
                  << " | "
                  << std::fixed << std::setprecision(1)
                  << std::setw(7) << std_avg << std::setw(5) << std_sd << "  "
                  << std::setw(7) << xyu_avg << std::setw(5) << xyu_sd << "  "
                  << std::setw(7) << strstr_avg << std::setw(5) << strstr_sd << "  "
                  << color(speedup_xyu, 1.0) << std::setw(8) << speedup_xyu
                  << (speedup_xyu >= 1.0 ? "o" : "x") << reset << "  "
                  << color(speedup_strstr, 1.0) << std::setw(10) << speedup_strstr
                  << (speedup_strstr >= 1.0 ? "o" : "x") << reset
                  << std::endl;
    }
};

class StringRfindBenchmark {
public:
    enum class Scenario {
        NORMAL_FOUND,        // needle 确实出现（插入到随机位置）
        NORMAL_NOT_FOUND,    // 随机字符串，但通过重试确保不包含
    };

    struct TestCase {
        size_t slen;
        size_t tlen;
        Scenario scenario;
    };

    explicit StringRfindBenchmark(size_t iterations_per_instance = 10000,
                                  size_t num_instances = 30)
        : m_iters_per_instance(iterations_per_instance),
          m_num_instances(num_instances) {}

    void addDefaultCases() {
        std::vector<std::pair<size_t, size_t>> pairs = {
            {16, 1}, {32, 2}, {64, 3}, {64, 4},{64, 16}, {64, 32},
            {128, 4}, {128, 8},{128, 16}, {128, 32}, {128, 64},
            {256, 8}, {256, 16}, {256, 32}, {256, 64}, {256, 128},
            {1024, 16},  {1024, 32}, {1024, 64}, {1024, 128}, {1024, 256},
            {2048, 16},  {2048, 32}, {2048, 64}, {2048, 128}, {2048, 256}, {2048, 512}, {2048, 1024},
            {4096, 16},  {4096, 32}, {4096, 64}, {4096, 128}, {4096, 256}, {4096, 512}, {4096, 1024}, {4096, 2048},
        };

        for (auto& p : pairs) {
            addCase(p.first, p.second, Scenario::NORMAL_FOUND);
            addCase(p.first, p.second, Scenario::NORMAL_NOT_FOUND);
        }
    }

    void addCase(size_t slen, size_t tlen, Scenario s) {
        m_tests.push_back({slen, tlen, s});
    }

    void run() {
        printHeader();

        for (const auto& tc : m_tests) {
            runTestCase(tc);
        }
    }

private:
    size_t m_iters_per_instance;
    size_t m_num_instances;
    std::vector<TestCase> m_tests;
    std::mt19937 m_rng{std::random_device{}()};

    struct Instance {
        std::string haystack;
        std::string needle;
    };

    std::string randomString(size_t len) {
        const char charset[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789";
        std::uniform_int_distribution<size_t> dist(0, sizeof(charset) - 2);
        std::string s(len, '\0');
        for (size_t i = 0; i < len; ++i)
            s[i] = charset[dist(m_rng)];
        return s;
    }

    std::vector<Instance> generateInstances(const TestCase& tc) {
        std::vector<Instance> instances;
        instances.reserve(m_num_instances);

        for (size_t i = 0; i < m_num_instances; ++i) {
            Instance inst;
            switch (tc.scenario) {
            case Scenario::NORMAL_FOUND: {
                inst.haystack = randomString(tc.slen);
                inst.needle   = randomString(tc.tlen);
                if (tc.slen >= tc.tlen && tc.tlen > 0) {
                    size_t pos = (tc.slen - tc.tlen) * 3 / 4;
                    memcpy(&inst.haystack[pos], inst.needle.data(), tc.tlen);
                }
                break;
            }
            case Scenario::NORMAL_NOT_FOUND: {
                inst.haystack = randomString(tc.slen);
                if (tc.tlen > tc.slen) {
                    inst.needle = randomString(tc.tlen);
                } else {
                    do {
                        inst.needle = randomString(tc.tlen);
                    } while (inst.haystack.find(inst.needle) != std::string::npos);
                }
                break;
            }
            }
            instances.push_back(std::move(inst));
        }
        return instances;
    }

    static std::pair<double, double> computeStats(const std::vector<double>& values) {
        if (values.empty()) return {0.0, 0.0};
        double sum = 0.0;
        for (double v : values) sum += v;
        double mean = sum / values.size();

        double sq_sum = 0.0;
        for (double v : values) sq_sum += (v - mean) * (v - mean);
        double stddev = std::sqrt(sq_sum / values.size());
        return {mean, stddev};
    }

    void runTestCase(const TestCase& tc) {
        auto instances = generateInstances(tc);

        std::vector<double> std_times, xyu_times;
        std_times.reserve(m_num_instances);
        xyu_times.reserve(m_num_instances);

        const size_t warmup = 50;

        for (const auto& inst : instances) {
            // 正确性校验
            {
                size_t expected = inst.haystack.rfind(inst.needle);
                xyu::String xyu_hay(inst.haystack.data(), inst.haystack.size());
                xyu::String xyu_needle(inst.needle.data(), inst.needle.size());
                size_t xyu_result = xyu_hay.rfind(xyu_needle);
                if (expected != xyu_result) {
                    std::cerr << "ERROR: mismatch! slen=" << tc.slen
                              << " tlen=" << tc.tlen << " expected=" << expected
                              << " xyu=" << xyu_result << std::endl;
                }
            }

            // 测量 std::string::rfind
            for (size_t k = 0; k < warmup; ++k)
                benchmark_utils::do_not_optimize(inst.haystack.rfind(inst.needle));

            auto t1 = std::chrono::high_resolution_clock::now();
            for (size_t k = 0; k < m_iters_per_instance / 20; ++k) {
                benchmark_utils::do_not_optimize(inst.haystack.rfind(inst.needle));
            }
            auto t2 = std::chrono::high_resolution_clock::now();
            double ns_std = std::chrono::duration<double, std::nano>(t2 - t1).count() / m_iters_per_instance * 20;
            std_times.push_back(ns_std);

            // 测量 xyu::String::rfind
            xyu::String xyu_hay(inst.haystack.data(), inst.haystack.size());
            xyu::String xyu_needle(inst.needle.data(), inst.needle.size());

            for (size_t k = 0; k < warmup; ++k)
                benchmark_utils::do_not_optimize(xyu_hay.rfind(xyu_needle));

            t1 = std::chrono::high_resolution_clock::now();
            for (size_t k = 0; k < m_iters_per_instance * 10; ++k) {
                benchmark_utils::do_not_optimize(xyu_hay.rfind(xyu_needle));
            }
            t2 = std::chrono::high_resolution_clock::now();
            double ns_xyu = std::chrono::duration<double, std::nano>(t2 - t1).count() / m_iters_per_instance / 10;
            xyu_times.push_back(ns_xyu);
        }

        auto [avg_std, sd_std] = computeStats(std_times);
        auto [avg_xyu, sd_xyu] = computeStats(xyu_times);
        double speedup = avg_std / avg_xyu;

        printRow(tc, avg_std, sd_std, avg_xyu, sd_xyu, speedup);
    }

    void printHeader() {
        std::cout << std::left
                  << std::setw(8)  << "S_Len"
                  << std::setw(8)  << "T_Len"
                  << std::setw(12) << "Scenario"
                  << " | "
                  << std::setw(16) << "std(ns) avg-sd"
                  << std::setw(16) << "xyu(ns) avg-sd"
                  << std::setw(10) << "Speedup"
                  << std::endl;
        std::cout << std::string(95, '-') << std::endl;
    }

    void printRow(const TestCase& tc,
                  double std_avg, double std_sd,
                  double xyu_avg, double xyu_sd,
                  double speedup) {
        const char* scenario_str = "";
        switch (tc.scenario) {
        case Scenario::NORMAL_FOUND:      scenario_str = "Found";      break;
        case Scenario::NORMAL_NOT_FOUND:  scenario_str = "Not Found";  break;
        }

        const char* color = (speedup >= 1.0) ? "\033[32m" : "\033[31m";
        const char* reset = "\033[0m";

        std::cout << std::left
                  << std::setw(8)  << tc.slen
                  << std::setw(8)  << tc.tlen
                  << std::setw(12) << scenario_str
                  << " | "
                  << std::fixed << std::setprecision(1)
                  << std::setw(7) << std_avg << std::setw(6) << std_sd << "  "
                  << std::setw(7) << xyu_avg << std::setw(6) << xyu_sd << "  "
                  << color << std::setw(8) << speedup << (std_avg > xyu_avg ? "o" : "x") << reset
                  << std::endl;
    }
};

// 主函数：测试 find
int main20() {
    std::cout << "=========================================================\n";
    std::cout << "  String Search Benchmark (multi-instance, robust)\n";
    std::cout << "  std::string vs xyu::String\n";
    std::cout << "=========================================================\n\n";

    StringSearchBenchmark benchmark(100, 2000);
    benchmark.addDefaultCases();
    benchmark.run();

    return 0;
}

// 主函数：测试 rfind
int main21() {
    std::cout << "=========================================================\n";
    std::cout << "  String RFind Benchmark (multi-instance, robust)\n";
    std::cout << "  std::string vs xyu::String\n";
    std::cout << "=========================================================\n\n";

    StringRfindBenchmark benchmark(100, 200);
    benchmark.addDefaultCases();
    benchmark.run();

    return 0;
}
