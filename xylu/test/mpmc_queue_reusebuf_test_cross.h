#pragma once

/**
 * @file    mpmc_queue_reusebuf_test.h
 * @brief   测试内存重用无锁队列性能
 * @date    2026-08-25
 */

#include <iostream>
#include <thread>
#include <vector>
#include <random>
#include <chrono>
#include <atomic>
#include <iomanip>
#include <queue>
#include <mutex>
#include <condition_variable>


#include "../deprecated/queue.h"
#include "../detail/xysystem/time.h"

using namespace xyu;

// ---------- Test Parameters ----------
const int THREAD_COUNT = 8;
const int ITERATIONS = 100'000;
const int WARMUP = 1000;
const int SLEEP_MAX_US = 10;          // max business sleep (microseconds)
constexpr bool SLEEP_RANDOM = true;
const size_t QUEUE_CAPACITY = 1024;   // soft limit for the queue under test

// ---------- Global Objects ----------
// The queue under test (manages int buffers)
MPMCQueue_ReuseBuf<int> g_data_queue(QUEUE_CAPACITY);

// Transfer queue for passing buffer pointers between threads (not timed)
std::queue<int*> g_transfer_q;
std::mutex g_transfer_mtx;
std::condition_variable g_transfer_cv;

// Node size for direct malloc/free (same as queue's Node)
using QueueType = MPMCQueue_ReuseBuf<int>;
constexpr size_t NODE_SIZE = sizeof(QueueType::Node);

// ---------- Worker: using MPMCQueue_ReuseBuf ----------
struct QueueResult {
    long long take_ns = 0;
    long long put_ns = 0;
};

QueueResult thread_queue_worker(int iterations, int sleep_max, int warmup) {
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<int> dist(0, sleep_max);

    Clock clk_take, clk_put;
    QueueResult res;

    // ---------- Warmup phase (not timed) ----------
    for (int i = 0; i < warmup; ++i) {
        int* buf = g_data_queue.take();          // take
        if constexpr (SLEEP_RANDOM) std::this_thread::sleep_for(std::chrono::microseconds(dist(gen)));
        else std::this_thread::sleep_for(std::chrono::microseconds(sleep_max));
        {
            std::lock_guard<std::mutex> lock(g_transfer_mtx);
            g_transfer_q.push(buf);
            g_transfer_cv.notify_one();
        }
        int* other_buf;
        {
            std::unique_lock<std::mutex> lock(g_transfer_mtx);
            while (g_transfer_q.empty())
                g_transfer_cv.wait(lock);
            other_buf = g_transfer_q.front();
            g_transfer_q.pop();
        }
        g_data_queue.put(other_buf);             // put
    }

    for (int i = 0; i < iterations; ++i) {
        // 1. Take from queue (timed)
        clk_take.start();
        int* buf = g_data_queue.take();
        res.take_ns += clk_take.stop().count;

        // 2. Business (not timed)
        if constexpr (SLEEP_RANDOM) std::this_thread::sleep_for(std::chrono::microseconds(dist(gen)));
        else std::this_thread::sleep_for(std::chrono::microseconds(sleep_max));

        // 3. Transfer: push to global transfer queue (not timed)
        {
            std::lock_guard<std::mutex> lock(g_transfer_mtx);
            g_transfer_q.push(buf);
            g_transfer_cv.notify_one();
        }

        // 4. Transfer: pop another pointer from global transfer queue (not timed)
        int* other_buf;
        {
            std::unique_lock<std::mutex> lock(g_transfer_mtx);
            while (g_transfer_q.empty()) {
                g_transfer_cv.wait(lock);
            }
            other_buf = g_transfer_q.front();
            g_transfer_q.pop();
        }

        // 5. Put the popped buffer back to queue (timed)
        clk_put.start();
        g_data_queue.put(other_buf);
        res.put_ns += clk_put.stop().count;
    }
    return res;
}

// ---------- Worker: direct malloc/free ----------
struct MallocResult {
    long long malloc_ns = 0;
    long long free_ns = 0;
};

MallocResult thread_malloc_worker(int iterations, int sleep_max, int warmup) {
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<int> dist(0, sleep_max);

    Clock clk_malloc, clk_free;
    MallocResult res;

    // ---------- Warmup phase (not timed) ----------
    for (int i = 0; i < warmup; ++i) {
        void* p = xylu::xymemory::__::under_alloc(NODE_SIZE);
        if constexpr (SLEEP_RANDOM)  std::this_thread::sleep_for(std::chrono::microseconds(dist(gen)));
        else std::this_thread::sleep_for(std::chrono::microseconds(sleep_max));
        {
            std::lock_guard<std::mutex> lock(g_transfer_mtx);
            g_transfer_q.push(static_cast<int*>(p)); // treat as int* for uniformity
            g_transfer_cv.notify_one();
        }
        int* other_p;
        {
            std::unique_lock<std::mutex> lock(g_transfer_mtx);
            while (g_transfer_q.empty()) {
                g_transfer_cv.wait(lock);
            }
            other_p = g_transfer_q.front();
            g_transfer_q.pop();
        }
        xylu::xymemory::__::under_dealloc(other_p);
    }

    for (int i = 0; i < iterations; ++i) {
        // 1. malloc (timed)
        clk_malloc.start();
        void* p = xylu::xymemory::__::under_alloc(NODE_SIZE);
        res.malloc_ns += clk_malloc.stop().count;

        // 2. Business (not timed)
        if constexpr (SLEEP_RANDOM)  std::this_thread::sleep_for(std::chrono::microseconds(dist(gen)));
        else std::this_thread::sleep_for(std::chrono::microseconds(sleep_max));

        // 3. Transfer: push to global transfer queue (not timed)
        {
            std::lock_guard<std::mutex> lock(g_transfer_mtx);
            g_transfer_q.push(static_cast<int*>(p)); // treat as int* for uniformity
            g_transfer_cv.notify_one();
        }

        // 4. Transfer: pop another pointer (not timed)
        int* other_p;
        {
            std::unique_lock<std::mutex> lock(g_transfer_mtx);
            while (g_transfer_q.empty()) {
                g_transfer_cv.wait(lock);
            }
            other_p = g_transfer_q.front();
            g_transfer_q.pop();
        }

        // 5. free (timed)
        clk_free.start();
        xylu::xymemory::__::under_dealloc(other_p);
        res.free_ns += clk_free.stop().count;
    }
    return res;
}

// ---------- Aggregation Helpers ----------
void aggregate_queue(const std::vector<QueueResult>& results,
                     long long& total_take, long long& total_put) {
    total_take = 0;
    total_put = 0;
    for (const auto& r : results) {
        total_take += r.take_ns;
        total_put  += r.put_ns;
    }
}

void aggregate_malloc(const std::vector<MallocResult>& results,
                      long long& total_malloc, long long& total_free) {
    total_malloc = 0;
    total_free = 0;
    for (const auto& r : results) {
        total_malloc += r.malloc_ns;
        total_free   += r.free_ns;
    }
}

// ---------- Main ----------
int main33() {
    std::cout << std::fixed << std::setprecision(2);

    // ========== 1. Test lock-free queue ==========
    std::vector<std::thread> threads;
    std::vector<QueueResult> queue_results(THREAD_COUNT);

    auto start_queue = std::chrono::steady_clock::now();
    for (int i = 0; i < THREAD_COUNT; ++i) {
        threads.emplace_back([i, &queue_results]() {
            queue_results[i] = thread_queue_worker(ITERATIONS, SLEEP_MAX_US, WARMUP);
        });
    }
    for (auto& t : threads) t.join();
    auto end_queue = std::chrono::steady_clock::now();

    long long total_take, total_put;
    aggregate_queue(queue_results, total_take, total_put);
    long long total_queue = total_take + total_put;
    long long total_ops = THREAD_COUNT * ITERATIONS;

    auto elapsed_queue_ms = std::chrono::duration<double, std::milli>(end_queue - start_queue).count();

    std::cout << "===== Lock-free Queue (MPMCQueue_ReuseBuf) =====\n";
    std::cout << "Total operations: " << total_ops << "\n";
    std::cout << "Take total time: " << total_take << " ns (avg " << (double)total_take / total_ops << " ns/op)\n";
    std::cout << "Put  total time: " << total_put << " ns (avg " << (double)total_put / total_ops << " ns/op)\n";
    std::cout << "Take+Put total: " << total_queue << " ns (avg " << (double)total_queue / total_ops << " ns/op)\n";
    std::cout << "Wall clock time: " << elapsed_queue_ms << " ms (includes business sleep & transfer)\n\n";

    // ========== 2. Test direct malloc/free ==========
    std::vector<MallocResult> malloc_results(THREAD_COUNT);
    threads.clear();

    auto start_malloc = std::chrono::steady_clock::now();
    for (int i = 0; i < THREAD_COUNT; ++i) {
        threads.emplace_back([i, &malloc_results]() {
            malloc_results[i] = thread_malloc_worker(ITERATIONS, SLEEP_MAX_US, WARMUP);
        });
    }
    for (auto& t : threads) t.join();
    auto end_malloc = std::chrono::steady_clock::now();

    long long total_malloc, total_free;
    aggregate_malloc(malloc_results, total_malloc, total_free);
    long long total_malloc_free = total_malloc + total_free;

    auto elapsed_malloc_ms = std::chrono::duration<double, std::milli>(end_malloc - start_malloc).count();

    std::cout << "===== Direct malloc / free =====\n";
    std::cout << "Total operations: " << total_ops << "\n";
    std::cout << "Malloc total time: " << total_malloc << " ns (avg " << (double)total_malloc / total_ops << " ns/op)\n";
    std::cout << "Free   total time: " << total_free << " ns (avg " << (double)total_free / total_ops << " ns/op)\n";
    std::cout << "Malloc+Free total: " << total_malloc_free << " ns (avg " << (double)total_malloc_free / total_ops << " ns/op)\n";
    std::cout << "Wall clock time: " << elapsed_malloc_ms << " ms (includes business sleep & transfer)\n";

    // ========== 3. Comparison ==========
    double ratio = (double)total_queue / total_malloc_free;
    std::cout << "\nQueue vs malloc/free time ratio: " << ratio << " ( <1 means queue is faster )\n";

    return 0;
}