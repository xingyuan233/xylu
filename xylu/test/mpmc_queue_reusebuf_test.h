#include <iostream>
#include <thread>
#include <vector>
#include <chrono>
#include <atomic>
#include <iomanip>
#include <random>

#include "../deprecated/queue.h"
#include "../detail/xysystem/time.h"

using namespace xyu;
using namespace xylu;

// ---------- Test Parameters ----------
constexpr int THREAD_COUNT = 8;
constexpr int ITERATIONS = 1000'000;          // timed iterations per thread
constexpr int WARMUP_ITERATIONS = 10000;      // warmup iterations (not timed)
constexpr int SLEEP_MAX_US = 10;             // max sleep microseconds
constexpr bool SLEEP_RANDOM = false;         // true: random [0, SLEEP_MAX_US]; false: fixed SLEEP_MAX_US

constexpr size_t QUEUE_CAPACITY = 1024;      // soft limit for the queue under test

// ---------- Global Objects ----------
MPMCQueue_ReuseBuf<int> g_data_queue(QUEUE_CAPACITY);

using QueueType = MPMCQueue_ReuseBuf<int>;
constexpr size_t NODE_SIZE = sizeof(QueueType::Node);

// Thread-local random generator for random sleep
static thread_local std::mt19937 rng(std::random_device{}());

// ---------- Worker 1: using MPMCQueue_ReuseBuf ----------
struct QueueResult {
    long long take_ns = 0;
    long long put_ns = 0;
};

QueueResult thread_queue_worker(int iterations, int warmup, int sleep_max, bool random_sleep) {
    Clock clk_take, clk_put;
    QueueResult res;
    std::uniform_int_distribution<int> dist(0, sleep_max);

    // Warmup
    for (int i = 0; i < warmup; ++i) {
        int* buf = g_data_queue.take();
        if (random_sleep) std::this_thread::sleep_for(std::chrono::microseconds(dist(rng)));
        else std::this_thread::sleep_for(std::chrono::microseconds(sleep_max));
        volatile int* vbuf = buf;
        *vbuf = i;
        g_data_queue.put(buf);
    }

    // Timed
    for (int i = 0; i < iterations; ++i) {
        clk_take.start();
        int* buf = g_data_queue.take();
        res.take_ns += clk_take.stop().count;

        if (random_sleep) std::this_thread::sleep_for(std::chrono::microseconds(dist(rng)));
        else std::this_thread::sleep_for(std::chrono::microseconds(sleep_max));

        volatile int* vbuf = buf;
        *vbuf = i;

        clk_put.start();
        g_data_queue.put(buf);
        res.put_ns += clk_put.stop().count;
    }
    return res;
}

// ---------- Worker 2: direct malloc/free (raw memory) ----------
struct MallocResult {
    long long malloc_ns = 0;
    long long free_ns = 0;
};

MallocResult thread_malloc_worker(int iterations, int warmup, int sleep_max, bool random_sleep) {
    Clock clk_malloc, clk_free;
    MallocResult res;
    std::uniform_int_distribution<int> dist(0, sleep_max);

    // Warmup
    for (int i = 0; i < warmup; ++i) {
        void* p = xymemory::__::under_alloc(sizeof(int));
        if (random_sleep) std::this_thread::sleep_for(std::chrono::microseconds(dist(rng)));
        else std::this_thread::sleep_for(std::chrono::microseconds(sleep_max));
        volatile int* vp = static_cast<volatile int*>(p);
        *vp = i;
        xymemory::__::under_dealloc(p);
    }

    // Timed
    for (int i = 0; i < iterations; ++i) {
        clk_malloc.start();
        void* p = xymemory::__::under_alloc(sizeof(int));
        res.malloc_ns += clk_malloc.stop().count;

        if (random_sleep) std::this_thread::sleep_for(std::chrono::microseconds(dist(rng)));
        else std::this_thread::sleep_for(std::chrono::microseconds(sleep_max));

        volatile int* vp = static_cast<volatile int*>(p);
        *vp = i;

        clk_free.start();
        xymemory::__::under_dealloc(p);
        res.free_ns += clk_free.stop().count;
    }
    return res;
}

// ---------- Worker 3: alloc<T>(count) / dealloc(ptr, count) ----------
struct AllocResult {
    long long alloc_ns = 0;
    long long dealloc_ns = 0;
};

AllocResult thread_alloc_worker(int iterations, int warmup, int sleep_max, bool random_sleep) {
    Clock clk_alloc, clk_dealloc;
    AllocResult res;
    std::uniform_int_distribution<int> dist(0, sleep_max);

    // Warmup
    for (int i = 0; i < warmup; ++i) {
        auto* p = alloc<int>(1);   // allocate one int
        if (random_sleep) std::this_thread::sleep_for(std::chrono::microseconds(dist(rng)));
        else std::this_thread::sleep_for(std::chrono::microseconds(sleep_max));
        volatile int* vp = p;
        *vp = i;
        dealloc(p, 1);
    }

    // Timed
    for (int i = 0; i < iterations; ++i) {
        clk_alloc.start();
        auto* p = alloc<int>(1);
        res.alloc_ns += clk_alloc.stop().count;

        if (random_sleep) std::this_thread::sleep_for(std::chrono::microseconds(dist(rng)));
        else std::this_thread::sleep_for(std::chrono::microseconds(sleep_max));

        volatile int* vp = p;
        *vp = i;

        clk_dealloc.start();
        dealloc(p, 1);
        res.dealloc_ns += clk_dealloc.stop().count;
    }
    return res;
}

// ---------- Aggregation Helpers ----------
void aggregate_queue(const std::vector<QueueResult>& results,
                     long long& total_take, long long& total_put) {
    total_take = 0; total_put = 0;
    for (const auto& r : results) {
        total_take += r.take_ns;
        total_put  += r.put_ns;
    }
}

void aggregate_malloc(const std::vector<MallocResult>& results,
                      long long& total_malloc, long long& total_free) {
    total_malloc = 0; total_free = 0;
    for (const auto& r : results) {
        total_malloc += r.malloc_ns;
        total_free   += r.free_ns;
    }
}

void aggregate_alloc(const std::vector<AllocResult>& results,
                     long long& total_alloc, long long& total_dealloc) {
    total_alloc = 0; total_dealloc = 0;
    for (const auto& r : results) {
        total_alloc += r.alloc_ns;
        total_dealloc += r.dealloc_ns;
    }
}

// ---------- Main ----------
int main33() {
    std::cout << std::fixed << std::setprecision(2);
    std::cout << "Warmup iterations per thread: " << WARMUP_ITERATIONS << "\n";
    std::cout << "Sleep mode: " << (SLEEP_RANDOM ? "random [0, " : "fixed ") << SLEEP_MAX_US << " us\n\n";

    // ========== 1. Test lock-free queue ==========
    std::vector<std::thread> threads;
    std::vector<QueueResult> queue_results(THREAD_COUNT);

    auto start_queue = std::chrono::steady_clock::now();
    for (int i = 0; i < THREAD_COUNT; ++i) {
        threads.emplace_back([i, &queue_results]() {
            queue_results[i] = thread_queue_worker(ITERATIONS, WARMUP_ITERATIONS,
                                                   SLEEP_MAX_US, SLEEP_RANDOM);
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
    std::cout << "Total operations (timed): " << total_ops << "\n";
    std::cout << "Take total time: " << total_take << " ns (avg " << (double)total_take / total_ops << " ns/op)\n";
    std::cout << "Put  total time: " << total_put << " ns (avg " << (double)total_put / total_ops << " ns/op)\n";
    std::cout << "Take+Put total: " << total_queue << " ns (avg " << (double)total_queue / total_ops << " ns/op)\n";
    std::cout << "Wall clock time: " << elapsed_queue_ms << " ms (includes business sleep)\n\n";

    // ========== 2. Test direct malloc/free ==========
    std::vector<MallocResult> malloc_results(THREAD_COUNT);
    threads.clear();

    auto start_malloc = std::chrono::steady_clock::now();
    for (int i = 0; i < THREAD_COUNT; ++i) {
        threads.emplace_back([i, &malloc_results]() {
            malloc_results[i] = thread_malloc_worker(ITERATIONS, WARMUP_ITERATIONS,
                                                     SLEEP_MAX_US, SLEEP_RANDOM);
        });
    }
    for (auto& t : threads) t.join();
    auto end_malloc = std::chrono::steady_clock::now();

    long long total_malloc, total_free;
    aggregate_malloc(malloc_results, total_malloc, total_free);
    long long total_malloc_free = total_malloc + total_free;

    auto elapsed_malloc_ms = std::chrono::duration<double, std::milli>(end_malloc - start_malloc).count();

    std::cout << "===== Direct malloc / free =====\n";
    std::cout << "Total operations (timed): " << total_ops << "\n";
    std::cout << "Malloc total time: " << total_malloc << " ns (avg " << (double)total_malloc / total_ops << " ns/op)\n";
    std::cout << "Free   total time: " << total_free << " ns (avg " << (double)total_free / total_ops << " ns/op)\n";
    std::cout << "Malloc+Free total: " << total_malloc_free << " ns (avg " << (double)total_malloc_free / total_ops << " ns/op)\n";
    std::cout << "Wall clock time: " << elapsed_malloc_ms << " ms (includes business sleep)\n\n";

    // ========== 3. Test alloc<T> / dealloc ==========
    std::vector<AllocResult> alloc_results(THREAD_COUNT);
    threads.clear();

    auto start_alloc = std::chrono::steady_clock::now();
    for (int i = 0; i < THREAD_COUNT; ++i) {
        threads.emplace_back([i, &alloc_results]() {
            alloc_results[i] = thread_alloc_worker(ITERATIONS, WARMUP_ITERATIONS,
                                                   SLEEP_MAX_US, SLEEP_RANDOM);
        });
    }
    for (auto& t : threads) t.join();
    auto end_alloc = std::chrono::steady_clock::now();

    long long total_alloc, total_dealloc;
    aggregate_alloc(alloc_results, total_alloc, total_dealloc);
    long long total_alloc_dealloc = total_alloc + total_dealloc;

    auto elapsed_alloc_ms = std::chrono::duration<double, std::milli>(end_alloc - start_alloc).count();

    std::cout << "===== alloc<int>(1) / dealloc =====\n";
    std::cout << "Total operations (timed): " << total_ops << "\n";
    std::cout << "alloc total time: " << total_alloc << " ns (avg " << (double)total_alloc / total_ops << " ns/op)\n";
    std::cout << "dealloc total time: " << total_dealloc << " ns (avg " << (double)total_dealloc / total_ops << " ns/op)\n";
    std::cout << "alloc+dealloc total: " << total_alloc_dealloc << " ns (avg " << (double)total_alloc_dealloc / total_ops << " ns/op)\n";
    std::cout << "Wall clock time: " << elapsed_alloc_ms << " ms (includes business sleep)\n\n";

    // ========== 4. Comparison ==========
    double ratio_queue_vs_malloc = (double)total_queue / total_malloc_free;
    double ratio_queue_vs_alloc = (double)total_queue / total_alloc_dealloc;
    double ratio_alloc_vs_malloc = (double)total_alloc_dealloc / total_malloc_free;

    std::cout << "===== Comparison =====\n";
    std::cout << "Queue vs malloc/free : " << ratio_queue_vs_malloc << " ( <1 means queue faster )\n";
    std::cout << "Queue vs alloc/dealloc: " << ratio_queue_vs_alloc << " ( <1 means queue faster )\n";
    std::cout << "alloc/dealloc vs malloc/free: " << ratio_alloc_vs_malloc << " ( <1 means alloc faster )\n";

    return 0;
}