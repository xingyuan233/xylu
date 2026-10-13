#include "../condvar.h"
#include "../../xystring/fmt.h"

#ifdef _WIN32
#  include <windows.h>
#else
#  include <pthread.h>
#  include <errno.h>
#endif

/// 跨平台句柄
namespace
{
#ifdef _WIN32
    CONDITION_VARIABLE* cv(void*& h) noexcept { return reinterpret_cast<CONDITION_VARIABLE*>(&h); }
    SRWLOCK* m(void*& h) noexcept { return reinterpret_cast<SRWLOCK*>(&h); }
#else
    pthread_cond_t* cv(void* h) noexcept { return static_cast<pthread_cond_t*>(h); }
    pthread_mutex_t* m(void* h) noexcept { return static_cast<pthread_mutex_t*>(h); }
#endif
}

/// 创建
namespace xylu::xyconc
{
    [[noreturn]] XY_NOINLINE XY_COLD
    static void public_error(const char* func, int err) noexcept
    {
#ifndef _WIN32
        if (err == EINVAL) xyloge("{}: condvar is not initialised or is already destroyed", func), xyu::abort();
#endif
        xyu::E_Logic::abort_unknown_error(func, err);
    }
    
#ifndef _WIN32
    [[noreturn]] XY_NOINLINE XY_COLD
    static void create_error(int err) noexcept
    {
        const char* func = "xyu::Condvar::Condvar";
        switch (err) 
        {
        case EAGAIN:
            xyloge("{}: system lacked necessary resources to create condvar", func);
            xyu::abort();
        case ENOMEM:
            xylogf("{}: no memory to create condvar", func);
            xyu::abort();
        case EBUSY:
            xyloge("{}: condvar is already initialized", func);
            xyu::abort();
        case EINVAL:
            xyloge("{}: invalid argument to create condvar", func);
            xyu::abort();
        default:
            public_error(func, err);
        }
    }
#endif

    Condvar::Condvar() noexcept : h{}
    {
#ifdef _WIN32
        static_assert(sizeof(CONDITION_VARIABLE) == sizeof(void*));
        // CONDITION_VARIABLE 与 h 大小相同，不需要动态分配
        *reinterpret_cast<CONDITION_VARIABLE*>(&h) = CONDITION_VARIABLE_INIT;
#else
        h = xymemory::__::under_alloc(sizeof(pthread_cond_t));
        if (XY_UNLIKELY(!h)) xymemory::__::abort_alloc_failed(sizeof(pthread_cond_t), 1, alignof(pthread_cond_t));
        int r;
        for (int i = 0; i < 3; ++i)
        {
            r = pthread_cond_init(cv(h), nullptr);
            if (XY_LIKELY(r == 0)) return;
            if (r == EAGAIN) continue;
            break;
        }
        create_error(r);
#endif
    }
}

/// 销毁
namespace xylu::xyconc
{
#ifndef _WIN32
    [[noreturn]] XY_NOINLINE XY_COLD
    static void destroy_error(int err) noexcept
    {
        const char* func = "xyu::Condvar::~Condvar";
        switch (err) 
        {
        case EBUSY:
            xyloge("{}: threads are waiting on condvar", func);
            xyu::abort();
        default:
            public_error(func, err);
        }
    }
#endif

    Condvar::~Condvar() noexcept
    {
#ifdef _WIN32
        // CONDITION_VARIABLE 不需要显式销毁
#else
        if (int r = pthread_cond_destroy(cv(h)); XY_UNLIKELY(r)) destroy_error(r);
        xymemory::__::under_dealloc(h);
#endif
    }
}

/// 唤醒
namespace xylu::xyconc
{
    void Condvar::notify_one() noexcept
    {
#ifdef _WIN32
        WakeConditionVariable(cv(h));
#else
        if (int r = pthread_cond_signal(cv(h)); XY_UNLIKELY(r))
            public_error("xyu::Condvar::notify_one", r);
#endif
    }

    void Condvar::notify_all() noexcept
    {
#ifdef _WIN32
        WakeAllConditionVariable(cv(h));
#else
        if (int r = pthread_cond_broadcast(cv(h)); XY_UNLIKELY(r))
            public_error("xyu::Condvar::notify_all", r);
#endif
    }
}

/// 等待
namespace xylu::xyconc
{
    [[noreturn]] XY_NOINLINE XY_COLD
    static void wait_error(const char* func, int err) noexcept
    {
        switch (err)
        {
#ifdef _WIN32
        case ERROR_ACCESS_DENIED:
            xyloge("{}: mutex not owned this thread", func);
            xyu::abort();
        case ERROR_INVALID_HANDLE:
            xyloge("{}: invalid condvar handle", func);
            xyu::abort();
#else
        case EPERM:
            xyloge("{}: mutex not owned by thread", func);
            xyu::abort();
#endif
        default:
            public_error(func, err);
        }
    }

    void Condvar::wait_help(Mutex& mtx) noexcept
    {
#ifdef _WIN32
        if (!SleepConditionVariableSRW(cv(h), m(mtx.h), INFINITE, 0))
            wait_error("xyu::Condvar::wait", GetLastError());
#else
        if (int r = pthread_cond_wait(cv(h), m(mtx.h)); XY_UNLIKELY(r))
            wait_error("xyu::Condvar::wait", r);
#endif
    }

#ifdef _WIN32
    // 传递相对时间
    bool Condvar::wait_help(Mutex& mtx, xyu::uint32 ms) noexcept
    {
        if (SleepConditionVariableSRW(cv(h), m(mtx.h), ms, 0)) return true;
        if (auto r = GetLastError(); XY_UNLIKELY(r != ERROR_TIMEOUT))
            wait_error("xyu::Condvar::wait_timed", GetLastError());
        return false;
    }
#else
    // 传递距离 epoch 的绝对时间
    bool Condvar::wait_help(Mutex& mtx, xyu::uint64 s, xyu::uint32 ns) noexcept
    {
        timespec ts;
        ts.tv_sec = s;
        ts.tv_nsec = ns;
        int r = pthread_cond_timedwait(cv(h), m(mtx.h), &ts);
        if (r == 0) return true;
        if (r == ETIMEDOUT) return false;
        wait_error("xyu::Condvar::wait_timed", r);
    }
#endif
}
