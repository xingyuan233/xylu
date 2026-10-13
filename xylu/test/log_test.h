#include <cstdio>
#include <vector>
#include "../detail/xycore/log.h"
#include "../detail/xyconc/thread.h"
#include "../detail/xysystem/time.h"
#include "../detail/xysystem/file.h"

using namespace xyu;

// 线程工作函数：疯狂输出日志
void worker_func(void* arg) noexcept {
    int id = *static_cast<int*>(arg);
    for (int i = 0; i < 20000; ++i) {
        // 极高频的常规日志输出
        xylogi("WorkerThread [{}] is processing task number: {}", id, i);

        // 偶尔触发警告与错误，测试不同等级的过滤
        if (XY_UNLIKELY(i % 10 == 0)) {
            xylogw("WorkerThread [{}] reached a milestone at {}!", id, i);
            xyloge2("Simulated error in thread {} at task {}", __LINE__, __func__, id, i);
        }
    }
}

int main2() {
    // 关闭主控制台的实时刷新以最大化榨干 I/O 性能
    xyglogger.set_flush(false);

    xylogi("=================================================");
    xylogi("      Starting SLogger High-Concurrency Test     ");
    xylogi("=================================================");

    // 1. 基础宏功能与格式化测试
    xylogd("This is a debug message, testing basic format: {}, {:.2f}", 42, 3.14159);
    xylogt("Trace message should be visible if K_LOG_LEVEL allows.");
    xylogf3("Fatal error testing macros.");

    // 2. 多线程极限施压测试
    xylogi("Launching 8 threads for intensive log bombardment...");

    xylu::xysystem::Clock clock;
    clock.start_ex(); // 开启高精度测算

    const int num_threads = 8;
    int thread_ids[num_threads];
    xylu::xyconc::Thread_Native threads[num_threads];

    // 发车！
    for (int i = 0; i < num_threads; ++i) {
        thread_ids[i] = i;
        threads[i].create(worker_func, &thread_ids[i]);
    }

    // 3. 并发途中的动态文件测试
    try {
        // 让主线程稍微休眠一下，确保工作线程已经开始输出
//        xyu::Duration_ms<>{10}.sleep();
//
//        xylogi(">>> Main thread pushing an extra log file for WARNINGs only...");
//        xyu::File extra_log("extra_warn_test.log", xyu::File::TRUNC | xyu::File::READ);
//
//        // 仅记录 WARN 及以上级别的日志
//        xyglogger.push_file(xyu::move(extra_log), xyu::N_LOG_LEVEL::WARN);
//
//        xyu::Duration_ms<>{20}.sleep();
//
//        xylogi(">>> Main thread popping the extra log file...");
//        xyglogger.pop_file();

    } catch (...) {
        xyloge("Failed to manipulate extra log file during concurrency.");
    }

    // 等待所有线程收工
    for (int i = 0; i < num_threads; ++i) {
        threads[i].join();
    }

    auto elapsed = clock.stop_ex();

    // 恢复实时刷新，输出最终成绩
    xyglogger.set_flush(true);
    xylogi("=================================================");
    xylogi("All threads joined. Stress test completed.");
    xylogi("Total time elapsed: {} ms", elapsed.ms());
    xylogi("=================================================");

    return 0;
}