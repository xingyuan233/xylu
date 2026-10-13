#pragma once

/**
 * @file    xyconc/thread.h
 * @brief   提供并发的基础线程组件
 * @date    2026-03-07
 */

#include "./_.h"
#include "../xycore/config.h"
#include "../xytraits/class.h"

static_assert(XY_THREAD, "can't import thread without setting XY_THREAD to 1");

/// 原生线程
namespace xylu::xyconc
{
    /**
     * @brief 封装了原生线程句柄的RAII线程类
     * @details
     *   - 使用 `void(*)(void*)` 形式的函数指针作为线程入口
     *   - 负责管理线程从创建到销毁的完整生命周期
     *   - 其析构函数会自动等待（join）尚未结束的线程，以防止资源泄露
     */
    struct Thread : xyu::class_no_copy_t
    {
        /// 线程状态
        enum _Status { Uninit, Failed, Finished, Running, Joined, Detached };

    private:
        void* h = nullptr;      // 线程句柄
        _Status s = Uninit;     // 线程状态

    public:
        /* 构造析构 */

        /// 默认构造函数
        explicit Thread() noexcept = default;

        /**
          * @brief 析构函数
          * @details
          *   如果线程对象在销毁时仍处于 Running 状态，
          *   析构函数会自动调用 join() 等待线程执行完毕
          */
        ~Thread() noexcept;

        /// 构造并创建线程
        explicit Thread(void(*fun)(void*), void* arg = nullptr) noexcept { create(fun, arg); }

        /// 构造并创建线程
        explicit Thread(void(*fun)(void*) noexcept, void* arg = nullptr) noexcept { create(fun, arg); }

        /* 移动 */

        /// 移动构造函数
        Thread(Thread&& other) noexcept : s{other.s}, h{other.h} { other.h = nullptr; other.s = Uninit; }

        /**
         * @brief 移动赋值运算符
         * @attention 为了防止阻塞主线程，如果原线程仍在运行会将其分离(datch)
         */
        Thread& operator=(Thread&& other) noexcept
        {
            if (XY_UNLIKELY(this == &other)) return *this;
            if (s == Running) detach();
            xyu::swap(s, other.s);
            xyu::swap(h, other.h);
            return *this;
        }

        /// 交换
        Thread& swap(Thread& other) noexcept
        {
            xyu::swap(h, other.h);
            xyu::swap(s, other.s);
            return *this;
        }

        /* 线程管理 */

        /// 获取线程状态
        _Status status() const noexcept { return s; }

        /**
         * @brief 在当前线程对象上创建并启动一个新线程
         * @param fun 要在新线程中执行的函数，禁止为 nullptr
         * @param arg 传递给线程函数的参数
         */
        void create(void(*fun)(void*), void* arg = nullptr) noexcept XY_NONNULL_RELEASE(2);

        /**
         * @brief 在当前线程对象上创建并启动一个新线程
         * @param fun 要在新线程中执行的函数，禁止为 nullptr
         * @param arg 传递给线程函数的参数
         */
        void create(void(*fun)(void*) noexcept , void* arg = nullptr) noexcept XY_NONNULL_RELEASE(2);

        /// 等待线程结束
        void join() noexcept;

        /// 分离线程
        void detach() noexcept;

        /* 静态函数 */

        /// 线程让步
        static void yield() noexcept;

        /**
         * @brief 返回当前线程标识
         * @details 可以用于区分不同的线程
         * @attention 仅能用于区分存活线程，不保证标识不会被重用
         */
        static void* current() noexcept;
    };

}
