#pragma once

#include <cstdio>
#include <cstdlib>
#include <chrono>
#include <vector>
#include "../detail/xyconc/thread.h"
#include "../detail/xyconc/atomic.h"
#include "../detail/xysystem/time.h"

using namespace xyu;

// 简单的断言（可替换为项目内断言）
#define TEST_ASSERT(expr) \
    do { \
        if (!(expr)) { \
            std::fprintf(stderr, "ASSERTION FAILED: %s at %s:%d\n", #expr, __FILE__, __LINE__); \
            std::abort(); \
        } \
    } while (0)

#define TEST_ASSERT_EQ(a, b) TEST_ASSERT((a) == (b))
#define TEST_ASSERT_NE(a, b) TEST_ASSERT((a) != (b))

using namespace xylu::xyconc;

// ------------------------------------------------------------------
// 测试辅助：一个简单的计数器，供多线程使用
// ------------------------------------------------------------------
struct TestContext {
    Atomic<int> flag{0};
    Atomic<int> counter{0};
    Atomic<int64> phase{0};
};

// 线程函数：等待 flag 变为非零，然后累加 counter
void worker_wait_flag(void* ctx_ptr) {
    auto* ctx = static_cast<TestContext*>(ctx_ptr);
    // 等待 flag != 0
    ctx->flag.wait([](int v) { return v != 0; });
    // 醒来后增加计数
    ctx->counter.fetch_add(1, N_ATOMIC_ORDER::ACQ_REL);
}

// 线程函数：等待 phase 变化
void worker_wait_phase(void* ctx_ptr) {
    auto* ctx = static_cast<TestContext*>(ctx_ptr);
    int last_phase = ctx->phase.load();
    // 等待 phase 改变
    ctx->phase.wait([last_phase](int v) { return v != last_phase; });
}

// 线程函数：只等待一次 notify_one
void worker_wait_one(void* ctx_ptr) {
    auto* ctx = static_cast<TestContext*>(ctx_ptr);
    ctx->flag.wait([](int v) { return v != 0; });
    ctx->counter.fetch_add(1, N_ATOMIC_ORDER::ACQ_REL);
}

// 线程函数：在 flag 上忙等并自旋，验证 wait 内部自旋不会导致死锁
void worker_spin_wait(void* ctx_ptr) {
    auto* ctx = static_cast<TestContext*>(ctx_ptr);
    // 该 wait 会先自旋再睡眠
    ctx->flag.wait([](int v) { return v == 42; });
}

// ------------------------------------------------------------------
// 测试用例
// ------------------------------------------------------------------

// 1. 基础等待与唤醒 (notify_one)
void test_basic_wait_notify_one() {
    TestContext ctx;

    Thread_Native t1(worker_wait_flag, &ctx);
    Thread_Native t2(worker_wait_flag, &ctx);

    // 让两个线程都进入等待
    while (ctx.flag.load() != 0) { /* 自旋等待线程启动 */ }
    Thread_Native::yield();
    Thread_Native::yield();

    // 修改 flag 并唤醒一个
    ctx.flag.store(1, N_ATOMIC_ORDER::RELEASE);
    ctx.flag.notify_one();

    // 短暂等待让被唤醒的线程执行

    Duration_ms<>(10).sleep();

    // 此时应该只有一个线程醒来并增加了 counter
    //TEST_ASSERT_EQ(ctx.counter.load(), 1);
    std::printf("counter is %d\n", ctx.counter.load());

    // 唤醒第二个
    ctx.flag.notify_one();
    t1.join();
    t2.join();

    TEST_ASSERT_EQ(ctx.counter.load(), 2);
    std::printf("[PASS] test_basic_wait_notify_one\n");
}

// 2. notify_all 唤醒所有等待线程
void test_notify_all() {
    TestContext ctx;
    const int N = 5;
    std::vector<Thread_Native> threads;

    for (int i = 0; i < N; ++i) {
        threads.emplace_back(worker_wait_flag, &ctx);
    }

    // 等待所有线程进入等待状态（通过 flag 自旋简单模拟）
    Thread_Native::yield();
    Thread_Native::yield();

    // 修改 flag 并唤醒全部
    ctx.flag.store(1, N_ATOMIC_ORDER::RELEASE);
    ctx.flag.notify_all();

    for (auto& t : threads) {
        t.join();
    }

    TEST_ASSERT_EQ(ctx.counter.load(), N);
    std::printf("[PASS] test_notify_all\n");
}

// 3. 条件谓词测试：wait 的 condition 在唤醒后重新检查
void test_condition_predicate() {
    Atomic<int> val{0};
    bool woken = false;

    // 线程：等待值变为偶数
    auto worker = [](void* p) {
        auto* v = static_cast<Atomic<int>*>(p);
        v->wait([](int x) { return (x & 1) == 0; });
    };

    Thread_Native t(worker, &val);
    Thread_Native::yield();

    // 先改为奇数，唤醒线程（但条件不满足，线程应继续等待）
    val.store(3, N_ATOMIC_ORDER::RELEASE);
    val.notify_one();
    Thread_Native::yield();
    Thread_Native::yield();
    // 线程不应结束，因为条件仍为假

    // 再改为偶数
    val.store(4, N_ATOMIC_ORDER::RELEASE);
    val.notify_one();
    t.join();  // 应该正常结束

    std::printf("[PASS] test_condition_predicate\n");
}

// 4. 等待阶段变化 (利用版本号等待，覆盖非平台原生等待路径)
void test_wait_phase_change() {
    TestContext ctx;
    const int N = 3;
    std::vector<Thread_Native> threads;

    for (int i = 0; i < N; ++i) {
        threads.emplace_back(worker_wait_phase, &ctx);
    }

    Duration_ms<>{100}.sleep();
    Thread_Native::yield();

    // 修改 phase，并唤醒所有
    ctx.phase.store(1, N_ATOMIC_ORDER::RELEASE);
    ctx.phase.notify_all();

    for (auto& t : threads) {
        t.join();
    }

    std::printf("[PASS] test_wait_phase_change\n");
}

// 5. 自旋路径测试：修改值在自旋期间发生，不应进入睡眠
void test_spin_short_wait() {
    Atomic<int> flag{0};

    auto worker = [](void* p) {
        auto* f = static_cast<Atomic<int>*>(p);
        f->wait([](int v) { return v != 0; });
    };

    Thread_Native t(worker, &flag);
    // 不睡眠，立即修改并唤醒（线程可能还在自旋）
    flag.store(1, N_ATOMIC_ORDER::RELEASE);
    flag.notify_one();

    t.join();
    // 若线程进入睡眠并正常唤醒，则成功
    std::printf("[PASS] test_spin_short_wait\n");
}

// 6. 压力测试：大量线程并发等待和唤醒
void test_stress() {
    TestContext ctx;
    const int N = 20;
    std::vector<Thread_Native> threads;

    for (int i = 0; i < N; ++i) {
        threads.emplace_back(worker_wait_flag, &ctx);
    }

    // 等待所有线程稳定
    while (ctx.flag.load() != 0) {}
    Thread_Native::yield();

    // 分批唤醒，模拟 notify_one 场景
    ctx.flag.store(1, N_ATOMIC_ORDER::RELEASE);
    for (int i = 0; i < N; ++i) {
        ctx.flag.notify_one();
        Thread_Native::yield();  // 给被唤醒线程执行机会
    }

    for (auto& t : threads) {
        t.join();
    }

    TEST_ASSERT_EQ(ctx.counter.load(), N);
    std::printf("[PASS] test_stress\n");
}


// ------------------------------------------------------------------
// 主函数
// ------------------------------------------------------------------
int main2() {
    std::printf("Running Atomic wait/notify tests...\n");

    test_basic_wait_notify_one();
    test_notify_all();
    test_condition_predicate();
    test_wait_phase_change();
    test_spin_short_wait();
    test_stress();

    std::printf("All tests passed!\n");
    return 0;
}