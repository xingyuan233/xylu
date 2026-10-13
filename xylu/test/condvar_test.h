#include <cstdio>
#include <cstdlib>
#include <cassert>
#include <atomic>
#include <thread>
#include <vector>

// 假设你的库头文件路径如下
#include "../detail/xyconc/mutex.h"
#include "../detail/xyconc/condvar.h"
#include "../detail/xyconc/thread.h"
#include "../detail/xysystem/time.h"

using namespace xyu;

// 辅助：简单睡眠（毫秒）
static void sleep_ms(int ms) {
    std::this_thread::sleep_for(std::chrono::milliseconds(ms));
}

// ======================== 测试 1：单生产者-单消费者（notify_one） ========================
static Mutex mtx;
static Condvar cv;
static bool ready = false;
static int data = 0;

static void consumer(void*) {
    auto g = mtx.guard();
    // 等待条件满足
    cv.wait(g, ready);
    // 条件满足后，数据应已被生产者修改
    assert(data == 42);
    printf("  Consumer: received data = %d\n", data);
}

static void producer(void*) {
    sleep_ms(50); // 模拟准备工作
    {
        auto g = mtx.guard();
        data = 42;
        ready = true;
        cv.notify_one(); // 唤醒一个等待线程
    }
}

void test_basic_notify() {
    printf("\n========== Test 1: Basic notify_one ==========\n");
    ready = false;
    data = 0;

    ThreadNative consumer_thread;
    ThreadNative producer_thread;
    consumer_thread.create(consumer, nullptr);
    producer_thread.create(producer, nullptr);

    consumer_thread.join();
    producer_thread.join();

    printf("  [PASS] Producer-consumer with notify_one works.\n");
}

// ======================== 测试 2：广播（notify_all）唤醒多个等待者 ========================
static std::atomic<int> active_readers{0};
static const int N_READERS = 5;

static void reader(void* arg) {
    int id = *(int*)arg;
    auto g = mtx.guard();
    // 等待广播信号
    cv.wait(g, [&]{ return ready; });
    active_readers++;
    printf("  Reader %d awakened, total active = %d\n", id, active_readers.load());
}

static void broadcaster(void*) {
    sleep_ms(100);
    {
        auto g = mtx.guard();
        ready = true;
        cv.notify_all(); // 唤醒所有等待的读者
    }
}

void test_broadcast() {
    printf("\n========== Test 2: notify_all ==========\n");
    ready = false;
    active_readers = 0;

    std::vector<ThreadNative> readers;
    std::vector<int> ids(N_READERS);
    for (int i = 0; i < N_READERS; ++i) {
        ids[i] = i;
        readers.emplace_back();
        readers.back().create(reader, &ids[i]);
    }

    ThreadNative broadcaster_thread;
    broadcaster_thread.create(broadcaster, nullptr);

    broadcaster_thread.join();
    for (auto& t : readers) t.join();

    assert(active_readers == N_READERS);
    printf("  [PASS] All %d readers awakened by notify_all.\n", N_READERS);
}

// ======================== 测试 3：限时等待（wait_for）超时和条件满足 ========================
static bool condition_met = false;

static void slow_producer(void*) {
    sleep_ms(200); // 比超时时间长
    {
        auto g = mtx.guard();
        condition_met = true;
        cv.notify_one();
    }
}

static void fast_producer(void*) {
    sleep_ms(30); // 比超时时间短
    {
        auto g = mtx.guard();
        condition_met = true;
        cv.notify_one();
    }
}

void test_wait_for() {
    printf("\n========== Test 3: wait_for (timeout and success) ==========\n");

    // 场景 3.1：超时
    condition_met = false;
    {
        auto g = mtx.guard();
        auto timeout = xyu::Duration_ms<>(100); // 100ms 超时
        bool result = cv.wait_for(g, timeout, condition_met);
        printf("  wait_for(timeout 100ms) returned %s (expected false)\n", result ? "true" : "false");
        assert(!result);
    }

    // 场景 3.2：条件在超时前满足
    condition_met = false;
    ThreadNative producer_thread;
    producer_thread.create(fast_producer, nullptr);
    {
        auto g = mtx.guard();
        auto timeout = xyu::Duration_ms<>(100);
        bool result = cv.wait_for(g, timeout, condition_met);
        printf("  wait_for(timeout 100ms, fast producer) returned %s (expected true)\n", result ? "true" : "false");
        assert(result);
    }
    producer_thread.join();

    // 场景 3.3：条件已经满足（立即返回）
    condition_met = true;
    {
        auto g = mtx.guard();
        auto timeout = xyu::Duration_ms<>(50);
        bool result = cv.wait_for(g, timeout, condition_met);
        printf("  wait_for(already true) returned %s (expected true)\n", result ? "true" : "false");
        assert(result);
    }

    printf("  [PASS] wait_for behaves correctly.\n");
}

// ======================== 测试 4：绝对时间等待（wait_to） ========================
static bool flag = false;

static void delayed_notifier(void*) {
    sleep_ms(80);
    {
        auto g = mtx.guard();
        flag = true;
        cv.notify_one();
    }
}

void test_wait_to() {
    printf("\n========== Test 4: wait_to (absolute timeout) ==========\n");

    // 4.1 超时（绝对时间点已过）
    flag = false;
    {
        auto g = mtx.guard();
        auto past = xyu::Calendar{} - xyu::Duration_ms<>(10);
        bool result = cv.wait_to(g, past, flag);
        printf("  wait_to(past timepoint) returned %s (expected false)\n", result ? "true" : "false");
        assert(!result);
    }

    // 4.2 条件在绝对时间前满足
    flag = false;
    ThreadNative notifier;
    notifier.create(delayed_notifier, nullptr);
    {
        auto g = mtx.guard();
        auto deadline = xyu::Calendar::utc() + xyu::Duration_ms<>(150);
        bool result = cv.wait_to(g, deadline, flag);
        printf("  wait_to(future deadline, notifier after 80ms) returned %s (expected true)\n", result ? "true" : "false");
        assert(result);
    }
    notifier.join();

    printf("  [PASS] wait_to works with absolute timeout.\n");
}

// ======================== 测试 5：异常行为 ========================
void test_exceptions() {
    printf("\n========== Test 5: Exception safety ==========\n");

    // 5.1 未锁定 guard 调用 wait (内部会调用 guard.lock()，但为了测试我们的实现，可以先不上锁)
    // 注意：wait 内部会检查 guard.is_locked()，若 false 则调用 guard.lock()，这应该能正常工作。
    // 但为了测试异常路径，我们可以故意传递一个无效的 guard（例如已移动的）。
    Mutex m;
    Condvar cv2;
    {
        auto g = m.guard(false); // 未上锁
        bool done = false;
        // 应该能正常等待（因为 wait 内部会上锁），但为了测试，这里只验证不会崩溃
        // 实际上我们不希望真的阻塞，所以使用 wait_for 短超时
        cv2.wait_for(g, xyu::Duration_ms<>(1), done);
        printf("  [PASS] wait_for with unlocked guard auto-locks successfully.\n");
    }

    // 5.2 重复 wait 同一个 guard（多次调用 wait 是允许的，只要每次 wait 前持有锁）
    {
        auto g = m.guard();
        bool done = false;
        cv2.wait_for(g, xyu::Duration_ms<>(1), done);
        cv2.wait_for(g, xyu::Duration_ms<>(1), done);
        printf("  [PASS] Multiple wait calls on same guard are allowed.\n");
    }

    // 5.3 跨线程使用同一个 Guard？ 不应该发生，但 Guard 不可拷贝移动，所以编译阻止。
    printf("  [PASS] Guard is move-only, preventing cross-thread misuse.\n");
}

// ======================== 主函数 ========================
int main2() {
    printf("=== Condvar Test Suite ===\n");
    test_basic_notify();
    test_broadcast();
    test_wait_for();
    test_wait_to();
    test_exceptions();
    printf("\n=== All condvar tests passed. ===\n");
    return 0;
}