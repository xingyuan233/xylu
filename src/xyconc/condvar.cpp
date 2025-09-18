#if (defined(_WIN32) || defined(__CYGWIN__))
#define XY_WINDOWS
#endif

#ifdef XY_WINDOWS
#include <windows.h>
#else
#include <pthread.h>
#include <errno.h>
#endif

#include "../../link/config"
#if !XY_UNTHREAD

#include "../../head/xyconc/condvar.h"
#include "../../link/log"

namespace
{
#ifdef XY_WINDOWS
    CONDITION_VARIABLE* cv(void*& h) noexcept { return reinterpret_cast<CONDITION_VARIABLE*>(&h); }
    SRWLOCK* m(void* h) noexcept { return reinterpret_cast<SRWLOCK*>(h); }
#else
    pthread_cond_t* cv(void* h) noexcept { return static_cast<pthread_cond_t*>(h); }
    pthread_mutex_t* m(void* h) noexcept { return static_cast<pthread_mutex_t*>(h); }
#endif
}

namespace
{
    template <typename I>
    [[noreturn]] void unknown_error(xyu::uint line, const char* func, I err)
    {
        xyloge2(0, "E_CondVar: unknown error with code {}", line, func, err);
        throw xyu::E_CondVar{};
    }

#ifdef XY_WINDOWS
    [[noreturn]] void wait_error(xyu::uint line, const char* func, DWORD err)
    {
        switch (err) {
            case ERROR_ACCESS_DENIED:
                xyloge2(0, "E_CondVar_Not_Owned: mutex not owned by thread", line, func);
                throw xyu::E_CondVar_Not_Owned{};
            case ERROR_INVALID_HANDLE:
                xyloge2(0, "E_CondVar_Invalid_State: invalid handle", line, func);
                throw xyu::E_CondVar_Invalid_State{};
            default:
                unknown_error(line, func, err);
        }
    }
#else
    [[noreturn]] void create_error(xyu::uint line, const char* func, int err)
    {
        switch (err) {
            case EAGAIN:
                xyloge2(0, "E_CondVar_Tmp_Unavailable: system lacked necessary resources to create condvar", line, func);
                throw xyu::E_CondVar_Tmp_Unavailable{};
            case ENOMEM:
                xyloge2(1, "E_CondVar_No_Memory: no memory to create condvar", line, func);
                throw xyu::E_CondVar_No_Memory{};
            case EBUSY:
                xyloge2(0, "E_CondVar_Invalid_State: condvar is already initialized", line, func);
                throw xyu::E_CondVar_Invalid_State{};
            case EINVAL:
                xyloge2(0, "E_CondVar_Invalid_Parameter: invalid parameter to create condvar", line, func);
                throw xyu::make_error(xyu::E_CondVar{}, xyu::E_Logic_Invalid_Argument{});
            default:
                unknown_error(line, func, err);
        }
    }

    void destroy_error(xyu::uint line, const char* func, int err)
    {
        switch (err) {
            case EBUSY:
                xyloge2(0, "E_CondVar_Busy: threads are waiting on condvar", line, func);
                break;
            case EINVAL:
                xyloge2(0, "E_CondVar_Invalid_State: condvar is not initialised or is already destroyed", line, func);
                break;
            default:
                xyloge2(0, "E_CondVar_Unknown_Error: unknown error with code {}", line, func, err);
        }
    }

    [[noreturn]] void notify_error(xyu::uint line, const char* func, int err)
    {
        switch (err) {
            case EINVAL:
                xyloge2(0, "E_CondVar_Invalid_State: condvar is not initialised or is already destroyed", line, func);
                throw xyu::E_CondVar_Invalid_State{};
            default:
                unknown_error(line, func, err);
        }
    }

    [[noreturn]] void wait_error(xyu::uint line, const char* func, int err)
    {
        switch (err) {
            case EINVAL:
                xyloge2(0, "E_CondVar_Invalid_Argument: invalid arguments", line, func);
                throw xyu::make_error(xyu::E_CondVar{}, xyu::E_Logic_Invalid_Argument{});
            case EPERM:
                xyloge2(0, "E_CondVar_Not_Owned: mutex not owned by thread", line, func);
                throw xyu::E_CondVar_Not_Owned{};
            default:
                unknown_error(line, func, err);
        }
    }
#endif
}

namespace xylu::xyconc
{
    CondVar::CondVar()
    {
#ifdef XY_WINDOWS
        // CONDITION_VARIABLE 与 h 大小相同，不需要动态分配
        *reinterpret_cast<CONDITION_VARIABLE*>(&h) = CONDITION_VARIABLE_INIT;
#else
        h = xylu::xymemory::__::under_alloc(sizeof(pthread_cond_t));
        if (int r = pthread_cond_init(cv(h), nullptr); XY_UNLIKELY(r))
            create_error(__LINE__, __func__, r);
#endif
    }

    CondVar::~CondVar() noexcept
    {
#ifdef XY_WINDOWS
        // CONDITION_VARIABLE 不需要显式销毁
#else
        try {
            if (int r = pthread_cond_destroy(cv(h)); XY_UNLIKELY(r))
                destroy_error(__LINE__, __func__, r);
        }
        catch (...) {}
        xylu::xymemory::__::under_dealloc(h);
#endif
    }

    void CondVar::notify_one()
    {
#ifdef XY_WINDOWS
        WakeConditionVariable(cv(h));
#else
        if (int r = pthread_cond_signal(cv(h)); XY_UNLIKELY(r))
            notify_error(__LINE__, __func__, r);
#endif
    }

    void CondVar::notify_all()
    {
#ifdef XY_WINDOWS
        WakeAllConditionVariable(cv(h));
#else
        if (int r = pthread_cond_broadcast(cv(h)); XY_UNLIKELY(r))
            notify_error(__LINE__, __func__, r);
#endif
    }

#ifdef XY_WINDOWS
    void __::wait_help(void* cvh, void* mh, bool shared)
    {
        auto flag = shared ? CONDITION_VARIABLE_LOCKMODE_SHARED : 0;
        while (XY_UNLIKELY(!SleepConditionVariableSRW((CONDITION_VARIABLE*)cvh, m(mh), INFINITE, flag)));
    }
#else
    void __::wait_help(void* cvh, void* mh)
    {
        if (int r = pthread_cond_wait(cv(cvh), m(mh)); XY_UNLIKELY(r))
            wait_error(__LINE__, __func__, r);
    }
#endif

#ifdef XY_WINDOWS
    bool __::wait_timeout_help(void* cvh, void* mh, bool shared, xyu::size_t ms)
    {
        auto flag = shared ? CONDITION_VARIABLE_LOCKMODE_SHARED : 0;
        if (!SleepConditionVariableSRW((CONDITION_VARIABLE*)cvh, m(mh), ms, flag)) {
            if (GetLastError() == ERROR_TIMEOUT) return false;
            wait_error(__LINE__, __func__, GetLastError());
        }
        return true;
    }
#else
    bool __::wait_timeout_help(void* cvh, void* mh, xyu::size_t s, xyu::size_t ns)
    {
        timespec ts;
        ts.tv_sec = s;
        ts.tv_nsec = ns;
        int r = pthread_cond_timedwait(cv(cvh), m(mh), &ts);
        if (r == 0) return true;
        if (r == ETIMEDOUT) return false;
        wait_error(__LINE__, __func__, r);
    }
#endif

}

#endif // !XY_UNTHREAD