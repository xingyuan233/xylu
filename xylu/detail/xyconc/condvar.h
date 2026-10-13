#pragma once

/**
 * @file    condvar.h
 * @brief   条件变量
 * @date    2026-04-15
 */

#include "./mutex.h"
#include "../xytraits/func.h"
#include "../xysystem/time.h"

/// 条件变量
namespace xylu::xyconc
{
    /**
     * @brief 条件变量
     * @details 用于线程间的同步等待与唤醒。需配合 @ref Mutex 使用
     *
     * @par 基本用法
     * @code
     * Mutex mtx;
     * Condvar cv;
     * bool ready = false;
     *
     * // 等待线程
     * {
     *     auto g = mtx.guard();
     *     cv.wait(g, ready);
     * }
     *
     * // 通知线程
     * {
     *     auto g = mtx.guard();
     *     ready = true;
     *     cv.notify_one();
     * }
     * @endcode
     *
     * @note 等待前以及成功唤醒后均为持有锁的状态
     */
    struct Condvar : xyu::class_no_copy_move_t
    {
    private:
        void* h;    // 条件变量句柄

    public:
        /* 构造析构 */

        /// 创建条件变量
        Condvar() noexcept;

        /**
         * @brief 销毁条件变量
         * @details 若仍被线程占有导致销毁失败会输出日志
         */
        ~Condvar() noexcept;

        /* 唤醒 */

        /// 至少唤醒一个等待线程
        void notify_one() noexcept;
        /// 唤醒所有等待线程
        void notify_all() noexcept;

        /* 等待 */

        /**
         * @brief 等待条件变量，直到指定值转为 true
         * @tparam Value 可转换为 bool 的类型
         */
        template <typename Value, xyu::t_enable<!xyu::t_can_call<Value>, bool> = false>
        void wait(Mutex::Guard& guard, Value& v) noexcept(noexcept(static_cast<bool>(v)))
        { wait(guard, [&v]{ return static_cast<bool>(v); }); }
        /**
         * @brief 等待条件变量，直到指定条件满足
         * @tparam Condition 返回 bool 的可调用对象
         */
        template <typename Condition, xyu::t_enable<xyu::t_can_call<Condition>, bool> = true>
        void wait(Mutex::Guard& guard, Condition&& condition) noexcept(noexcept(condition()))
        {
            if (!guard.is_locked()) guard.lock();
            while (!condition()) wait_help(*guard.m);
        }

        /* 限时等待 */

        /**
         * @brief 限时等待条件变量，返回值是否转为 true 或 超时
         * @tparam Value 可转换为 bool 的类型
         * @param timeout 最大等待时长
         */
        template <typename T, xyu::uint64 Num, xyu::uint64 Den, typename Value, xyu::t_enable<!xyu::t_can_call<Value>, bool> = false>
        bool wait_for(Mutex::Guard& guard, xyu::Duration<T,Num,Den> timeout, Value& v) noexcept(noexcept(static_cast<bool>(v)))
        { return wait_for(guard, timeout, [&v]{ return static_cast<bool>(v); }); }
        /**
         * @brief 限时等待条件变量，返回指定条件是否满足 或 超时
         * @tparam Condition 返回 bool 的可调用对象
         * @param timeout 最大等待时长
         */
        template <typename T, xyu::uint64 Num, xyu::uint64 Den, typename Condition, xyu::t_enable<xyu::t_can_call<Condition>, bool> = true>
        bool wait_for(Mutex::Guard& guard, xyu::Duration<T,Num,Den> timeout, Condition&& condition) noexcept(noexcept(condition()))
        {
            if (!guard.is_locked()) guard.lock();
            if (condition()) return true;
            if (XY_UNLIKELY(timeout.count <= 0)) return false;
            return wait_for_help(*guard.m, timeout, xyu::forward<Condition>(condition));
        }

        /**
         * @brief 限时等待条件变量，返回值是否转为 true 或 到达指定 UTC 时间点
         * @tparam Value 可转换为 bool 的类型
         * @param timepoint 超时的绝对 UTC 时间点
         */
        template <typename Value, xyu::t_enable<!xyu::t_can_call<Value>, bool> = false>
        bool wait_to(Mutex::Guard& guard, xyu::Calendar timepoint, Value& v) noexcept(noexcept(static_cast<bool>(v)))
        { return wait_to(guard, timepoint, [&v]{ return static_cast<bool>(v); }); }
        /**
         * @brief 限时等待条件变量，返回指定条件是否满足 或 到达指定 UTC 时间点
         * @tparam Condition 返回 bool 的可调用对象
         * @param timepoint 超时的绝对 UTC 时间点
         */
        template <typename Condition, xyu::t_enable<xyu::t_can_call<Condition>, bool> = true>
        bool wait_to(Mutex::Guard& guard, xyu::Calendar timepoint, Condition&& condition) noexcept(noexcept(condition()))
        {
            if (!guard.is_locked()) guard.lock();
            if (condition()) return true;
            auto dms = timepoint - xyu::Calendar{};
            if (XY_UNLIKELY(dms.count <= 0)) return false;
            return wait_to_help(*guard.m, dms, xyu::forward<Condition>(condition));
        }

    private:
        // 等待辅助函数
        void wait_help(Mutex& m) noexcept;

        // 限时等待辅助函数
#ifdef _WIN32
        bool wait_help(Mutex& m, xyu::uint32 ms) noexcept;
#else
        bool wait_help(Mutex& m, xyu::uint64 s, xyu::uint32 ns) noexcept;
#endif
        // wait_for 辅助函数
        template <typename T, xyu::uint64 Num, xyu::uint64 Den, typename Condition>
        bool wait_for_help(Mutex& m, xyu::Duration<T,Num,Den> timeout, Condition&& condition) noexcept(noexcept(condition()))
        {
#if defined _WIN32
            xyu::Duration_ms<T> dms = timeout;
            xyu::Duration_ms<T> ddl = dms; ddl += xyu::Duration_utc();
            constexpr xyu::uint32 limit = xyu::t_number<xyu::uint32>::max;
            for(;;)
            {
                xyu::uint32 t = dms.count < limit ? dms.count : limit;
                if (wait_help(m, t) && condition()) return true;
                dms = ddl - xyu::Duration_utc();
                if (dms.count <= 0) return false;
            }
#else
            auto d = timeout + xyu::Duration_utc();
            auto ds = d.s();
            xyu::uint64 s = ds.count;
            xyu::uint32 ns = (d - ds).ns();
            for(;;)
            {
                if (!wait_help(m, s, ns)) return false;
                if (condition()) return true;
            }
#endif
        }

        // wait_to 辅助函数
        template <typename T, typename Condition>
        bool wait_to_help(Mutex& m, xyu::Duration_ms<T> timepoint, Condition&& condition) noexcept(noexcept(condition()))
        {
#ifdef _WIN32
            constexpr xyu::uint32 limit = xyu::t_number<xyu::uint32>::max;
            for(;;)
            {
                xyu::Duration_ms<T> dms = timepoint - xyu::Duration_utc();
                if (dms.count <= 0) return false;
                xyu::uint32 t = dms.count < limit ? dms.count : limit;
                if (wait_help(m, t) && condition()) return true;
            }
#else
            xyu::uint64 s = timepoint.s();
            xyu::uint32 ns = timepoint.count % 1000 * 1'000'000;
            for(;;)
            {
                if (!wait_help(m, s, ns)) return false;
                if (condition()) return true;
            }
#endif
        }
    };
}
