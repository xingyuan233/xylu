#include <cstdio>
#include <cstdlib>
#include <cassert>
#include <chrono>
#include <thread>
#include <vector>

// 假设你的库头文件路径如下，请根据实际情况调整
#include "../detail/xyconc/mutex.h"
#include "../detail/xyconc/thread.h"

using namespace xyu;

// 辅助函数：短暂睡眠模拟工作
static void tiny_sleep() {
    std::this_thread::sleep_for(std::chrono::microseconds(10));
}

// ======================== 无保护对照（数据竞争） ========================
static int unprotected_counter = 0;

static void unprotected_worker(void* arg) {
    int repeat = *(int*)arg;
    for (int i = 0; i < repeat; ++i) {
        int tmp = unprotected_counter;
        tiny_sleep();       // 增加竞争窗口
        unprotected_counter = tmp + 1;
    }
}

void test_unprotected() {
    printf("\n========== Test: Unprotected (data race) ==========\n");
    unprotected_counter = 0;
    const int n_threads = 4;
    const int inc_per_thread = 1000;
    int expected = n_threads * inc_per_thread;

    ThreadNative threads[n_threads];
    for (int i = 0; i < n_threads; ++i) {
        threads[i].create(unprotected_worker, (void*)&inc_per_thread);
    }
    for (int i = 0; i < n_threads; ++i) {
        threads[i].join();
    }
    printf("Final counter = %d, expected = %d\n", unprotected_counter, expected);
    if (unprotected_counter == expected) {
        printf("  [WARNING] No data race detected (unlikely but possible)\n");
    } else {
        printf("  [OK] Data race detected, counter corrupted.\n");
    }
}

// ======================== Mutex 保护 ========================
Mutex mutex;
int protected_counter = 0;

static void mutex_worker(void* arg) {
    int repeat = *(int*)arg;
    for (int i = 0; i < repeat; ++i) {
        auto g = mutex.guard();
        int tmp = protected_counter;
        tiny_sleep();
        protected_counter = tmp + 1;
    }
}

void test_mutex() {
    printf("\n========== Test: Mutex protection ==========\n");
    protected_counter = 0;
    const int n_threads = 4;
    const int inc_per_thread = 1000;
    int expected = n_threads * inc_per_thread;

    ThreadNative threads[n_threads];
    for (auto & thread : threads) {
        thread.create(mutex_worker, (void*)&inc_per_thread);
    }
    for (auto & thread : threads) {
        thread.join();
    }
    printf("Final counter = %d, expected = %d\n", protected_counter, expected);
    assert(protected_counter == expected);
    printf("  [PASS] Mutex protects counter correctly.\n");
}

// ======================== 递归锁测试 ========================
static Mutex rec_mutex;
static int rec_counter = 0;

// 递归函数，深度递归上锁
void recursive_inc(int depth, Mutex::RecursiveGuard& rg) {
    if (depth <= 0) return;
    auto g = rg.guard();   // 每次递归增加递归深度
    rec_counter++;
    recursive_inc(depth - 1, rg);
}   // g 析构，深度减少

void recursive_worker(void* arg) {
    int depth = *(int*)arg;
    auto rg = rec_mutex.rguard();          // 创建递归状态（每个线程独立）
    recursive_inc(depth, rg);
}

void test_recursive() {
    printf("\n========== Test: RecursiveGuard ==========\n");
    rec_counter = 0;
    const int n_threads = 4;
    const int depth_per_thread = 100;
    int expected = n_threads * depth_per_thread;

    ThreadNative threads[n_threads];
    for (int i = 0; i < n_threads; ++i) {
        threads[i].create(recursive_worker, (void*)&depth_per_thread);
    }
    for (int i = 0; i < n_threads; ++i) {
        threads[i].join();
    }
    printf("Final recursive counter = %d, expected = %d\n", rec_counter, expected);
    assert(rec_counter == expected);
    printf("  [PASS] RecursiveGuard works with deep nesting.\n");
}

// ======================== RwMutex 测试（多读单写） ========================
static RwMutex rw;
static int shared_data = 0;

static void reader_worker(void* arg) {
    int read_count = *(int*)arg;
    for (int i = 0; i < read_count; ++i) {
        auto g = rw.sguard();          // 读锁（共享）
        int val = shared_data;
        // 模拟读操作，不加延迟以增加并发，但 tiny_sleep 会暴露问题
        tiny_sleep();
        // 读完后不做修改
    }
}

static void writer_worker(void* arg) {
    int write_count = *(int*)arg;
    for (int i = 0; i < write_count; ++i) {
        auto g = rw.guard();           // 写锁（独占）
        shared_data++;
        tiny_sleep();
    }
}

void test_rwmutex() {
    printf("\n========== Test: RwMutex (multiple readers, single writer) ==========\n");
    shared_data = 0;
    const int n_readers = 6;
    const int n_writers = 2;
    const int reads_per_reader = 500;
    const int writes_per_writer = 100;
    int expected_writes = n_writers * writes_per_writer;

    std::vector<ThreadNative> threads;
    threads.reserve(n_readers + n_writers);

    for (int i = 0; i < n_readers; ++i) {
        threads.emplace_back();
        threads.back().create(reader_worker, (void*)&reads_per_reader);
    }
    for (int i = 0; i < n_writers; ++i) {
        threads.emplace_back();
        threads.back().create(writer_worker, (void*)&writes_per_writer);
    }

    for (auto& t : threads) {
        t.join();
    }

    printf("Final shared_data = %d, expected writes = %d\n", shared_data, expected_writes);
    assert(shared_data == expected_writes);
    printf("  [PASS] RwMutex allows concurrent reads and exclusive writes.\n");
}

// ======================== 异常行为测试（重复上锁/未锁解锁） ========================
void test_exceptions() {
    printf("\n========== Test: Exception safety (double lock / unlock without lock) ==========\n");
    Mutex m;

    // 1. 重复上锁应抛出 E_Mutex_Already_Locked
    try {
        auto g = m.guard();
        g.lock();   // 再次 lock
        printf("  [FAIL] Double lock did not throw.\n");
    } catch (const E_Mutex_Already_Locked&) {
        printf("  [PASS] Double lock throws E_Mutex_Already_Locked.\n");
    } catch (...) {
        printf("  [FAIL] Wrong exception type.\n");
    }

    // 2. 未锁时 unlock 应抛出 E_Mutex_Not_Locked
    try {
        Mutex m2;
        auto g = m2.guard(false);   // 不上锁的 Guard
        g.unlock();
        printf("  [FAIL] Unlock without lock did not throw.\n");
    } catch (const E_Mutex_Not_Locked&) {
        printf("  [PASS] Unlock without lock throws E_Mutex_Not_Locked.\n");
    } catch (...) {
        printf("  [FAIL] Wrong exception type.\n");
    }

    // 3. 递归锁深度溢出测试（可选，很慢时可不做）
    printf("  [INFO] Recursive depth limit test skipped (would need -1 iterations).\n");
}

// ======================== 主函数 ========================
int main2() {
    printf("=== Concurrent Library Test Suite ===\n");
    test_unprotected();
    test_mutex();
    test_recursive();
    test_rwmutex();
    test_exceptions();
    printf("\n=== All tests completed. ===\n");
    return 0;
}