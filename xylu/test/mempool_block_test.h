#pragma once

/**
 * @file    mempool_block_test.h
 * @brief   测试内存池分配性能
 * @date    2025-12-26
 */

#include <iostream>
#include <vector>
#include <chrono>
#include <random>
#include <iomanip>
#include <thread>
#include <numeric>
#include <algorithm>
#include <cstring>

#include "../detail/xymemory/new.h"

class Benchmark {
public:
    struct Config {
        size_t alloc_count = 1000000; // 默认测试 100 万次
    };

    struct Result {
        std::string name;
        double std_time_ms;
        double my_time_ms;
    };

    Benchmark(Config config) : config_(config) {
        // 预热随机数生成器
        std::srand(std::time(nullptr));
    }

    // 运行所有测试
    void runAll() {
        printHeader();

        // -------------------------------------------------
        // 场景 1: 固定大小的小型对象分配 (32 Bytes)
        // -------------------------------------------------
        auto sizes_fixed = generateSizes(config_.alloc_count, 32, 32);
        auto [std_ptrs_1, my_ptrs_1, res1] = runAllocTest("Fixed Small Alloc (32B)", sizes_fixed);
        printResult(res1);

        // -------------------------------------------------
        // 场景 2: 随机大小的小型对象分配 (8 ~ 256 Bytes)
        // -------------------------------------------------
        auto sizes_small = generateSizes(config_.alloc_count, 8, 256);
        auto [std_ptrs_2, my_ptrs_2, res2] = runAllocTest("Random Small Alloc (8-256B)", sizes_small);
        printResult(res2);

        // -------------------------------------------------
        // 场景 3: 随机大小的大型对象分配 (1KB ~ 64KB)
        // -------------------------------------------------
        auto sizes_large = generateSizes(config_.alloc_count / 10, 4097, 65536); // 大对象数量减少，避免OOM
        auto [std_ptrs_3, my_ptrs_3, res3] = runAllocTest("Random Large Alloc (4KB-64KB)", sizes_large);
        printResult(res3);

        std::cout << "--------------------------------------------------------------------------------------\n";

        // -------------------------------------------------
        // 场景 4: 有序的小型对象回收 (倒序回收场景 1 的数据)
        // -------------------------------------------------
        // 生成倒序索引
        std::vector<size_t> indices_reverse(sizes_fixed);
        std::iota(indices_reverse.begin(), indices_reverse.end(), 0);

        auto res4 = runDeallocTest("Ordered Small Free (LIFO)", std_ptrs_1, my_ptrs_1, sizes_fixed, indices_reverse);
        printResult(res4);

        // -------------------------------------------------
        // 场景 5: 随机的小型对象回收 (随机回收场景 2 的数据)
        // -------------------------------------------------
        // 生成随机索引
        std::vector<size_t> indices_random(sizes_small);
        std::iota(indices_random.begin(), indices_random.end(), 0);
        std::shuffle(indices_random.begin(), indices_random.end(), std::mt19937{std::random_device{}()});

        auto res5 = runDeallocTest("Random Small Free", std_ptrs_2, my_ptrs_2, sizes_small, indices_random);
        printResult(res5);

        // -------------------------------------------------
        // 场景 6: 大型对象回收 (回收场景 3 的数据，顺序)
        // -------------------------------------------------
        std::vector<size_t> indices_large(sizes_large.size());
        std::iota(indices_large.begin(), indices_large.end(), 0);

        auto res6 = runDeallocTest("Large Free (Sequential)", std_ptrs_3, my_ptrs_3, sizes_large, indices_large);
        printResult(res6);

        std::cout << "--------------------------------------------------------------------------------------\n";
    }

private:
    Config config_;

    // 生成随机大小数组
    std::vector<size_t> generateSizes(size_t count, size_t min, size_t max) {
        std::vector<size_t> sizes(count);
        std::mt19937 gen(12345); // 固定种子保证每次运行一致
        std::uniform_int_distribution<size_t> dist(min, max);
        for (auto& s : sizes) s = dist(gen);
        return sizes;
    }

    // 分配测试模板
    std::tuple<std::vector<void*>, std::vector<void*>, Result>
    runAllocTest(std::string name, const std::vector<size_t>& sizes) {
        size_t count = sizes.size();
        std::vector<void*> std_ptrs(count);
        std::vector<void*> my_ptrs(count);

        // 1. 测试标准库 (malloc)
        auto start_std = std::chrono::high_resolution_clock::now();
        for (size_t i = 0; i < count; ++i) std_ptrs[i] = std::malloc(sizes[i]);
        auto end_std = std::chrono::high_resolution_clock::now();

        // 清除缓存干扰（简单模拟）
        dummyWork();

        // 2. 测试你的库 (xyu::alloc)
        auto start_my = std::chrono::high_resolution_clock::now();
        for (size_t i = 0; i < count; ++i) my_ptrs[i] = xyu::alloc(sizes[i]);
        auto end_my = std::chrono::high_resolution_clock::now();

        double std_ms = std::chrono::duration<double, std::milli>(end_std - start_std).count();
        double my_ms = std::chrono::duration<double, std::milli>(end_my - start_my).count();

        return {std_ptrs, my_ptrs, {name, std_ms, my_ms}};
    }

    // 回收测试模板
    Result runDeallocTest(std::string name,
                          std::vector<void*>& std_ptrs,
                          std::vector<void*>& my_ptrs,
                          const std::vector<size_t>& sizes,
                          const std::vector<size_t>& indices) {

        // 1. 测试标准库 (free)
        auto start_std = std::chrono::high_resolution_clock::now();
        for (size_t idx : indices) {
            // free 不需要 size，但为了保持循环体内指令与你的库尽量相似，我们读取一下 size
            volatile size_t s = sizes[idx];
            (void)s;
            std::free(std_ptrs[idx]);
        }
        auto end_std = std::chrono::high_resolution_clock::now();

        dummyWork();

        // 2. 测试你的库 (xyu::dealloc)
        auto start_my = std::chrono::high_resolution_clock::now();
        for (size_t idx : indices) {
            xyu::dealloc(my_ptrs[idx], sizes[idx]);
        }
        auto end_my = std::chrono::high_resolution_clock::now();

        // 清空指针防止重复释放
        std_ptrs.clear();
        my_ptrs.clear();

        double std_ms = std::chrono::duration<double, std::milli>(end_std - start_std).count();
        double my_ms = std::chrono::duration<double, std::milli>(end_my - start_my).count();

        return {name, std_ms, my_ms};
    }

    void dummyWork() {
        // 简单的休眠或计算，让 CPU 缓存稍微冷却一下
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }

    void printHeader() {
        std::cout << std::left << std::setw(30) << "Test Case"
                  << std::setw(15) << "Std Time(ms)"
                  << std::setw(15) << "My Time(ms)"
                  << std::setw(15) << "Rel Perf(x)"
                  << std::setw(15) << "Improvement" << "\n";
        std::cout << std::string(90, '-') << "\n";
    }

    void printResult(const Result& res) {
        double relative = res.std_time_ms / res.my_time_ms;
        double improvement = (res.std_time_ms - res.my_time_ms) / res.std_time_ms * 100.0;

        std::cout << std::left << std::setw(30) << res.name
                  << std::fixed << std::setprecision(2)
                  << std::setw(15) << res.std_time_ms
                  << std::setw(15) << res.my_time_ms
                  << std::setw(15) << relative;

        if (improvement > 0)
            std::cout << "+" << improvement << "%";
        else
            std::cout << improvement << "%";

        std::cout << "\n";
    }
};

int main2()
{
    std::cout << "Starting Memory Allocator Benchmark...\n";
    std::cout << "Warming up...\n\n";

    // 默认测试 1,000,000 次操作
    Benchmark::Config conf;
    conf.alloc_count = 1000000;

    Benchmark bench(conf);
    //bench.runAll();

    #define tcount 2
    #define tcreate( n ) std::thread t##n(std::thread(&Benchmark::runAll, &bench));
    #define tjoin( n ) t##n.join();
    XY_PP_FORWARD( tcreate, 1, XY_PP_TAKE(tcount, XY_PP_PARAM_INDEX_20()) )
    XY_PP_FORWARD( tjoin, 1, XY_PP_TAKE(tcount, XY_PP_PARAM_INDEX_20()) )

    return 0;
}

