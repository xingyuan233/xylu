#pragma clang diagnostic push
#pragma ide diagnostic ignored "OCUnusedGlobalDeclarationInspection"
#pragma once

#include "../../link/mutex"

namespace xylu::xyconc
{
    namespace __
    {
        // 等待辅助函数
#if (defined(_WIN32) || defined(__CYGWIN__))
        void wait_help(void* cvh, void* mh, bool shared);
#else
        void wait_help(void* cvh, void* mh);
#endif
#if (defined(_WIN32) || defined(__CYGWIN__))
        // 超时等待辅助函数 (timeout)
        bool wait_timeout_help(void* cvh, void* mh, bool shared, xyu::size_t ms);
#else
        // 超时等待辅助函数 (timepoint)
        bool wait_timeout_help(void* cvh, void* mh, xyu::size_t s, xyu::size_t ns);
#endif
    }

    /**
     * @brief 提供了线程间的等待/通知机制，用于实现复杂的同步逻辑。
     * @details
     *   条件变量允许一个或多个线程阻塞等待，直到另一个线程修改了某个共享条件，
     *   并通过 `notify_one()` 或 `notify_all()` 发出通知来唤醒它们。
     *
     *   **必须**与 `Mutex` 或 `Mutex_RW` 配合使用，以避免竞态条件。
     *   等待操作会自动释放锁，并在被唤醒后重新获取锁。
     *
     * @note 跨平台兼容性：在非 Windows 平台上，`CondVar` 无法与 `Mutex_RW` 一起使用。
     */
    class CondVar : xyu::class_no_copy_t
    {
    private:
        void* h;    // 条件变量句柄

    public:
        /// 创建条件变量
        CondVar();
        /// 销毁条件变量
        ~CondVar() noexcept;

        /// 移动构造
        CondVar(CondVar&& other) noexcept : h{other.h} { other.h = nullptr; }
        /// 移动赋值
        CondVar& operator=(CondVar&& other) noexcept { xyu::swap(h, other.h); return *this; }

        /// 至少唤醒一个等待线程
        void notify_one();
        /// 唤醒所有等待线程
        void notify_all();

        /**
         * @brief 等待条件变量，直到被唤醒
         * @note 必须先获取锁，且可能虚假唤醒，外层需要在 while/for 中
         * @example auto g = m.guard(); while (!f) cv.wait(g); ...; g.unlock();
         */
        template <typename Guard>
        void wait(Guard& guard)
        {
#if (defined(_WIN32) || defined(__CYGWIN__))
            __::wait_help(&h, &guard.m.h, xyu::t_is_same_nocvref<Guard, Mutex_RW::Guard_Read>);
#else
            __::wait_help(h, guard.m.h);
#endif
        }

        /// 等待条件变量，直到条件满足 (condition())
        template <typename Guard, typename Condition, xyu::t_enable<xyu::t_can_call<Condition>, bool> = true>
        void wait(Guard& guard, Condition&& condition)
        {
            if (XY_UNLIKELY(!guard.is_locked())) guard.lock();
            while (!condition()) wait(guard);
        }

        /// 等待条件变量，直到条件满足 (static_cast<bool>(v))
        template <typename Guard, typename Value, xyu::t_enable<!xyu::t_can_call<Value>, bool> = false>
        void wait(Guard& guard, Value& v) { wait(guard, [&v]{ return static_cast<bool>(v); }); }

        /**
         * @brief 等待条件变量，直到被唤醒 或 超时(从当前时间开始的时间段)
         * @note 必须先获取锁，且可能虚假唤醒，外层需要在 while/for 中
         * @note !!!此函数无法处理虚假唤醒，在循环中使用会使每次等待都使用完整的超时时长!!!
         * @note 若要处理虚假唤醒并保证总超时准确，请使用 *带谓词*的 wait_for 版本
         */
        template <typename Guard, typename T, T Scale>
        bool wait_for(Guard& guard, const xyu::Duration<T, Scale>& timeout)
        {
            if (XY_UNLIKELY(timeout.count <= 0)) return false;
#if (defined(_WIN32) || defined(__CYGWIN__))
            return __::wait_timeout_help(&h, &guard.m.h, xyu::t_is_same_nocvref<Guard, Mutex_RW::Guard_Read>, timeout.ms());
#else
            auto total = timeout + xyu::Duration_utc();
            xyu::Duration<T, 1000000000> s(total);
            return __::wait_timeout_help(h, guard.m.h, s.count, (total - s).ns());
#endif
        }

        /**
         * @brief 等待条件变量，直到被唤醒 或 超时(从当前时间开始的时间段)
         * @note 必须先获取锁，且可能虚假唤醒，外层需要在 while/for 中
         * @note 唤醒成功后减少超时时间，防止虚假唤醒导致超时时间被延长
         */
        template <typename Guard, typename T, T Scale>
        bool wait_for(Guard& guard, xyu::Duration<T, Scale>& timeout)
        {
            if (XY_UNLIKELY(timeout.count <= 0)) return false;
            // 缓存当前时间
            auto tmp = xyu::Duration_utc();
#if (defined(_WIN32) || defined(__CYGWIN__))
            bool ret = __::wait_timeout_help(&h, &guard.m.h, xyu::t_is_same_nocvref<Guard, Mutex_RW::Guard_Read>, timeout.ms());
#else
            auto total = timeout + tmp;
            xyu::Duration<T, 1000000000> s(total);
            bool ret = __::wait_timeout_help(h, guard.m.h, s.count, (total - s).ns());
#endif
            // 超时返回
            if (!ret) return false;
            // 重置超时时间
            auto past = xyu::Duration_utc() - tmp;
            timeout -= past;
            if (XY_UNLIKELY(timeout.count < 0)) timeout = 0;
            return true;
        }

        /// 等待条件变量，直到条件满足(condition()) 或 超时(从当前时间开始的时间段)
        template <typename Guard, typename Condition, typename T, T Scale, xyu::t_enable<xyu::t_can_call<Condition>, bool> = true>
        bool wait_for(Guard& guard, Condition&& condition, const xyu::Duration<T, Scale>& timeout)
        {
            // 缓存当前时间
#if (defined(_WIN32) || defined(__CYGWIN__))
            if (XY_UNLIKELY(timeout.count <= 0)) return false;
            auto tmp = xyu::Duration_utc();
            xyu::size_t ms = timeout.ms();
            // 检查上锁情况
            if (XY_UNLIKELY(!guard.is_locked())) guard.lock();
            // 检测条件
            if (XY_UNLIKELY(condition())) return true;
            for (;;)
            {
                // 等待条件变量
                bool ret = __::wait_timeout_help(&h, &guard.m.h, xyu::t_is_same_nocvref<Guard, Mutex_RW::Guard_Read>, ms);
                // 超时直接返回
                if (!ret) return false;
                // 非虚假唤醒，返回
                if (condition()) return true;
                // 重置超时时间
                auto newt = xyu::Duration_utc();
                auto past = newt - tmp;
                auto past_ms = past.ms();
                if (XY_LIKELY(past_ms >= ms)) return false;
                ms -= past_ms;
                tmp = newt;
            }
#else
            return wait_to(guard, xyu::forward<Condition>(condition), xyu::Calendar::now() + timeout);
#endif
        }

        /// 等待条件变量，直到条件满足(static_cast<bool>(v)) 或 超时(从当前时间开始的时间段)
        template <typename Guard, typename Value, typename T, T Scale, xyu::t_enable<!xyu::t_can_call<Value>, bool> = false>
        bool wait_for(Guard& guard, Value& v, const xyu::Duration<T, Scale>& timeout)
        { return wait_for(guard, [&v]{ return static_cast<bool>(v); }, timeout); }

        /**
         * @brief 等待条件变量，直到条件满足 或 超时(系统时间点)
         * @note 必须先获取锁，且可能虚假唤醒，外层需要在 while/for 中
         */
        template <typename Guard>
        bool wait_to(Guard& guard, const xyu::Calendar& timepoint)
        {
#if (defined(_WIN32) || defined(__CYGWIN__))
            auto timeout = timepoint - xyu::Calendar::now();
            if (XY_UNLIKELY(timeout.count <= 0)) return false;
            return __::wait_timeout_help(&h, &guard.m.h, xyu::t_is_same_nocvref<Guard, Mutex_RW::Guard_Read>, timeout.ms());
#else
            // timepoint 为 系统时间，减去 Duration_utcdiff 后为 UTC 时间
            auto s = (timepoint - xyu::Calendar{} - xyu::Duration_utcdiff()).s();
            if (XY_UNLIKELY(s < 0)) return false;
            auto ns = timepoint.ms() * 1000000;
            return __::wait_timeout_help(h, guard.m.h, s, ns);
#endif
        }

        /// 等待条件变量，直到条件满足(condition()) 或 超时(系统时间点)
        template <typename Guard, typename Condition, xyu::t_enable<xyu::t_can_call<Condition>, bool> = true>
        bool wait_to(Guard& guard, Condition&& condition, const xyu::Calendar& timepoint)
        {
#if (defined(_WIN32) || defined(__CYGWIN__))
            auto tmp = xyu::Calendar::now();
            return wait_for(guard, condition, timepoint - tmp);
#else
            // 计算超时时间
            // timepoint 为 系统时间，减去 Duration_utcdiff 后为 UTC 时间
            auto s = (timepoint - xyu::Calendar{} - xyu::Duration_utcdiff()).s();
            if (XY_UNLIKELY(s < 0)) return false;
            auto ns = timepoint.ms() * 1000000;
            // 检查上锁情况
            if (XY_UNLIKELY(!guard.is_locked())) guard.lock();
            // 检测条件
            if (XY_UNLIKELY(condition())) return true;
            for (;;)
            {
                // 等待条件变量
                bool ret = __::wait_timeout_help(h, guard.m.h, s, ns);
                // 超时直接返回
                if (!ret) return false;
                // 非虚假唤醒，返回
                if (condition()) return true;
            }
#endif
        }

        /// 等待条件变量，直到条件满足(static_cast<bool>(v)) 或 超时(系统时间点)
        template <typename Guard, typename Value, xyu::t_enable<!xyu::t_can_call<Value>, bool> = false>
        bool wait_to(Guard& guard, Value& v, const xyu::Calendar& timepoint)
        { return wait_to(guard, [&v]{ return static_cast<bool>(v); }, timepoint); }

        /**
         * @brief 等待条件变量，直到被唤醒 或 超时(从1970年1月1日开始的时间段)
         * @note 必须先获取锁，且可能虚假唤醒，外层需要在 while/for 中
         */
        template <typename Guard, typename T, T Scale>
        bool wait_to(Guard& guard, const xyu::Duration<T, Scale>& utc_timeout)
        {
#if (defined(_WIN32) || defined(__CYGWIN__))
            auto timeout = utc_timeout - xyu::Duration_utc();
            if (XY_UNLIKELY(timeout.count <= 0)) return false;
            return __::wait_timeout_help(&h, &guard.m.h, xyu::t_is_same_nocvref<Guard, Mutex_RW::Guard_Read>, timeout.ms());
#else
            if (XY_UNLIKELY(utc_timeout.count <= 0)) return false;
            xyu::Duration<T, 1000000000> ds(utc_timeout);
            return __::wait_timeout_help(h, guard.m.h, ds.count, (utc_timeout - ds).ns());
#endif
        }

        /// 等待条件变量，直到条件满足(condition()) 或 超时(从1970年1月1日开始的时间段)
        template <typename Guard, typename Condition, typename T, T Scale, xyu::t_enable<xyu::t_can_call<Condition>, bool> = true>
        bool wait_to(Guard& guard, Condition&& condition, const xyu::Duration<T, Scale>& utc_timeout)
        {
#if (defined(_WIN32) || defined(__CYGWIN__))
            auto tmp = xyu::Duration_utc();
            return wait_for(guard, condition, utc_timeout - tmp);
#else
            if (XY_UNLIKELY(utc_timeout.count <= 0)) return false;
            // 计算超时时间
            xyu::Duration<T, 1000000000> ds(utc_timeout);
            auto s = ds.count;
            auto ns = (utc_timeout - ds).ns();
            // 检查上锁情况
            if (XY_UNLIKELY(!guard.is_locked())) guard.lock();
            // 检测条件
            if (XY_UNLIKELY(condition())) return true;
            for (;;)
            {
                // 等待条件变量
                bool ret = __::wait_timeout_help(h, guard.m.h, s, ns);
                // 超时直接返回
                if (!ret) return false;
                // 非虚假唤醒，返回
                if (condition()) return true;
            }
#endif
        }

        /// 等待条件变量，直到条件满足(static_cast<bool>(v)) 或 超时(从1970年1月1日开始的时间段)
        template <typename Guard, typename Value, typename T, T Scale, xyu::t_enable<!xyu::t_can_call<Value>, bool> = false>
        bool wait_to(Guard& guard, Value& v, const xyu::Duration<T, Scale>& utc_timeout)
        { return wait_to(guard, [&v]{ return static_cast<bool>(v); }, utc_timeout); }
    };
}

#pragma clang diagnostic pop