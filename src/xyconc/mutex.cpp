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

#include "../../head/xyconc/mutex.h"
#include "../../link/log"

namespace
{
#ifdef XY_WINDOWS
    SRWLOCK* srw(void*& h) noexcept { return reinterpret_cast<SRWLOCK*>(&h); }
#else
    pthread_mutex_t* mu(void* h) noexcept { return static_cast<pthread_mutex_t*>(h); }
#endif
}

namespace
{
    template <typename I>
    [[noreturn]] void unknown_error(xyu::uint line, const char* func, I err)
    {
        xyloge2(0, "E_Mutex: unknown error with code {}", line, func, err);
        throw xyu::E_Mutex{};
    }

    void is_lock_error(xyu::uint line, const char* func)
    {
#if XY_DEBUG
        xyloge2(0, "E_Mutex_Already_Locked: mutex is already locked", line, func);
        throw xyu::E_Mutex_Already_Locked{};
#else
        xylogw2(xyu::N_LOG_WARN, "E_Mutex_Already_Locked: mutex is already locked", line, func);
#endif
    }

    void is_unlock_error(xyu::uint line, const char* func)
    {
#if XY_DEBUG
        xyloge2(0, "E_Mutex_Not_Locked: mutex is not locked", line, func);
        throw xyu::E_Mutex_Not_Locked{};
#else
        xylogw2(xyu::N_LOG_WARN, "E_Mutex_Not_Locked: mutex is not locked", line, func);
#endif
    }

    [[noreturn]] void out_limit_error(xyu::uint line, const char* func)
    {
        xyloge2(0, "E_Mutex_Recursive_Limit: maximum {} recursive lock count exceeded", line, func, xyu::size_t(-1));
        throw xyu::E_Mutex_Recursive_Limit{};
    }

#ifdef XY_WINDOWS
    [[noreturn]] void create_error(xyu::uint line, const char* func, DWORD err)
    {
        switch (err) {
            case STATUS_NO_MEMORY:
                xyloge2(1, "E_Mutex_No_Memory: no memory to create mutex", line, func);
                throw xyu::E_Mutex_No_Memory{};
            case STATUS_INVALID_PARAMETER:
                xyloge2(0, "E_Mutex_Invalid_Parameter: invalid parameter to create mutex", line, func);
                throw xyu::make_error(xyu::E_Mutex{}, xyu::E_Logic_Invalid_Argument{});
            case STATUS_ACCESS_VIOLATION:
                xyloge2(0, "E_Mutex_Invalid_State: destroyed critical section cannot be used again", line, func);
                throw xyu::E_Mutex_Invalid_State{};
            default:
                unknown_error(line, func, err);
        }
    }
#else
    [[noreturn]] void create_error(xyu::uint line, const char* func, int err)
    {
        switch (err) {
            case EAGAIN:
                xyloge2(0, "E_Mutex_Tmp_Unavailable: system lacked necessary resources to create mutex", line, func);
                throw xyu::E_Mutex_Tmp_Unavailable{};
            case ENOMEM:
                xyloge2(1, "E_Mutex_No_Memory: no memory to create mutex", line, func);
                throw xyu::E_Mutex_No_Memory{};
            case EPERM:
                xyloge2(0, "E_Mutex_Permission_Denied: permission denied to create mutex", line, func);
                throw xyu::E_Mutex_Permission_Denied{};
            case EBUSY:
                xyloge2(0, "E_Mutex_Invalid_State: mutex is already initialised", line, func);
                throw xyu::make_error(xyu::E_Mutex_Invalid_State{}, xyu::E_Logic_Invalid_Argument{});
            case EINVAL:
                xyloge2(0, "E_Mutex_Invalid_Parameter: invalid parameter to create mutex", line, func);
                throw xyu::make_error(xyu::E_Mutex{}, xyu::E_Logic_Invalid_Argument{});
            default:
                unknown_error(line, func, err);
        }
    }

    void destroy_error(xyu::uint line, const char* func, int err)
    {
        switch (err) {
            case EBUSY:
                xyloge2(0, "E_Mutex_Busy: mutex is still locked", line, func);
                break;
            case EINVAL:
                xyloge2(0, "E_Mutex_Invalid_State: mutex is not initialised or is already destroyed", line, func);
                break;
            default:
                xyloge2(0, "E_Mutex: unknown error with code {}", line, func, err);
        }
    }

    [[noreturn]] void lock_error(xyu::uint line, const char* func, int err)
    {
        switch (err) {
            case EINVAL:
                xyloge2(0, "E_Mutex_Invalid_State: mutex is not initialised or is already destroyed", line, func);
                throw xyu::E_Mutex_Invalid_State{};
            case EDEADLK:
                xyloge2(0, "E_Mutex_Deadlock: detected deadlock", line, func);
                throw xyu::E_Mutex_Deadlock{};
            case EAGAIN:
                xyloge2(0, "E_Mutex_Recursive_Limit: maximum recursive lock count exceeded", line, func);
                throw xyu::E_Mutex_Recursive_Limit{};
            case EOWNERDEAD:
            case ENOTRECOVERABLE:
                xyloge2(0, "E_Mutex_Invalid_State: last owner died while holding mutex", line, func);
                throw xyu::E_Mutex_Invalid_State{};
            default:
                unknown_error(line, func, err);
        }
    }

    [[noreturn]] void unlock_error(xyu::uint line, const char* func, int err)
    {
        switch (err) {
            case EPERM:
            case ENOTRECOVERABLE:
                xyloge2(0, "E_Mutex_Not_Owned: thread does not own the mutex", line, func);
                throw xyu::E_Mutex_Not_Owned{};
            case EINVAL:
                xyloge2(0, "E_Mutex_Invalid_State: mutex is not initialised or is already destroyed", line, func);
                throw xyu::E_Mutex_Invalid_State{};
            default:
                unknown_error(line, func, err);
        }
    }

    [[noreturn]] void rlock_error(xyu::uint line, const char* func, int err)
    {
        switch (err) {
            case EAGAIN:
                xyloge2(0, "E_Mutex_Tmp_Unavailable: max read locks exceeded for mutex", line, func);
                throw xyu::E_Mutex_Tmp_Unavailable{};
            case EINVAL:
                xyloge2(0, "E_Mutex_Invalid_State: mutex is not initialised or is already destroyed", line, func);
                throw xyu::E_Mutex_Invalid_State{};
            case EOWNERDEAD:
            case ENOTRECOVERABLE:
                xyloge2(0, "E_Mutex_Invalid_State: last reader died while holding mutex", line, func);
                throw xyu::E_Mutex_Invalid_State{};
            default:
                unknown_error(line, func, err);
        }
    }

    [[noreturn]] void wlock_error(xyu::uint line, const char* func, int err)
    {
        switch (err) {
            case EAGAIN:
                xyloge2(0, "E_Mutex_Tmp_Unavailable: max writer waiters exceeded for mutex", line, func);
                throw xyu::E_Mutex_Tmp_Unavailable{};
            case EINVAL:
                xyloge2(0, "E_Mutex_Invalid_State: mutex is not initialised or is already destroyed", line, func);
                throw xyu::E_Mutex_Invalid_State{};
            case EOWNERDEAD:
            case ENOTRECOVERABLE:
                xyloge2(0, "E_Mutex_Invalid_State: last writer died while holding mutex", line, func);
                throw xyu::E_Mutex_Invalid_State{};
            default:
                unknown_error(line, func, err);
        }
    }
#endif
}

namespace xylu::xyconc
{
    Mutex::Mutex()
    {
#ifdef XY_WINDOWS
        // SRWLOCK 与 h 大小相同，不需要动态分配
        *reinterpret_cast<SRWLOCK*>(&h) = SRWLOCK_INIT;
#else
        h = (pthread_mutex_t*)xylu::xymemory::__::under_alloc(sizeof(pthread_mutex_t));
        if (int r = pthread_mutex_init(mu(h), nullptr); XY_UNLIKELY(r))
            create_error(__LINE__, __func__, r);
#endif
    }

    Mutex::~Mutex() noexcept
    {
#ifdef XY_WINDOWS
        // SRWLOCK不需要显式销毁
#else
        try { // 捕获防止输出日志时的异常
            if (int r = pthread_mutex_destroy(mu(h)); XY_UNLIKELY(r))
                destroy_error(__LINE__, __func__, r);
        } catch (...) {}
        xylu::xymemory::__::under_dealloc(h);
#endif
    }

    void Mutex::Guard::lock()
    {
        if (XY_UNLIKELY(own)) return is_lock_error(__LINE__, __func__);
#ifdef XY_WINDOWS
        AcquireSRWLockExclusive(srw(m.h));
#else
        if (int r = pthread_mutex_lock(mu(m.h)); XY_UNLIKELY(r))
            lock_error(__LINE__, __func__, r);
#endif
        own = true;
    }

    bool Mutex::Guard::trylock()
    {
        if (XY_UNLIKELY(own)) return is_lock_error(__LINE__, __func__), true;
#ifdef XY_WINDOWS
        own = TryAcquireSRWLockExclusive(srw(m.h));
#else
        if (int r = pthread_mutex_trylock(mu(m.h))) {
            if (XY_LIKELY(r == EBUSY)) own = false;
            else lock_error(__LINE__, __func__, r);
        }
        else own = true;
#endif
        return own;
    }

    void Mutex::Guard::unlock()
    {
        if (XY_UNLIKELY(!own)) return is_unlock_error(__LINE__, __func__);
#ifdef XY_WINDOWS
        ReleaseSRWLockExclusive(srw(m.h));
#else
        if (int r = pthread_mutex_unlock(mu(m.h)); XY_UNLIKELY(r))
            unlock_error(__LINE__, __func__, r);
#endif
        own = false;
    }

    void Mutex::Guard_Recursive::lock()
    {
        if (XY_UNLIKELY(dp == -1)) out_limit_error(__LINE__, __func__);
        if (dp == 0)
        {
#ifdef XY_WINDOWS
        AcquireSRWLockExclusive(srw(m.h));
#else
        if (int r = pthread_mutex_lock(mu(m.h)); XY_UNLIKELY(r))
            lock_error(__LINE__, __func__, r);
#endif
        }
        ++dp;
    }

    bool Mutex::Guard_Recursive::trylock()
    {
        if (XY_UNLIKELY(dp == -1)) out_limit_error(__LINE__, __func__);
        if (dp) ++dp;
        else
        {
#ifdef XY_WINDOWS
            dp = TryAcquireSRWLockExclusive(srw(m.h));
#else
            if (int r = pthread_mutex_trylock(mu(m.h))) {
                if (XY_LIKELY(r == EBUSY)) dp = 0;
                else lock_error(__LINE__, __func__, r);
            }
            else dp = 1;
#endif
        }
        return dp;
    }

    void Mutex::Guard_Recursive::unlock()
    {
        if (XY_UNLIKELY(dp == 0)) return is_unlock_error(__LINE__, __func__);
        if (--dp == 0)
        {
#ifdef XY_WINDOWS
            ReleaseSRWLockExclusive(srw(m.h));
#else
            if (int r = pthread_mutex_unlock(mu(m.h)); XY_UNLIKELY(r))
                unlock_error(__LINE__, __func__, r);
#endif
        }
    }

    Mutex::Guard_Recursive::~Guard_Recursive() noexcept
    {
        try {
            if (XY_UNLIKELY(dp)) {
                xylogw2(xyu::N_LOG_WARN, "E_Mutex_Recursive_Unlock: unlocking a mutex with {} recursive locks", __LINE__, __func__, dp);
#ifdef XY_WINDOWS
                ReleaseSRWLockExclusive(srw(m.h));
#else
                if (int r = pthread_mutex_unlock(mu(m.h)); XY_UNLIKELY(r))
                unlock_error(__LINE__, __func__, r);
#endif
            }
        }
        catch (...) {}
    }

    Mutex_RW::Mutex_RW()
    {
#ifdef XY_WINDOWS
        // SRWLOCK 与 h 大小相同，不需要动态分配
        *reinterpret_cast<SRWLOCK*>(&h) = SRWLOCK_INIT;
#else
        h = (pthread_rwlock_t*)xylu::xymemory::__::under_alloc(sizeof(pthread_rwlock_t));
        if (int r = pthread_rwlock_init(mu(h), nullptr); XY_UNLIKELY(r))
            create_error(__LINE__, __func__, r);
#endif
    }

    Mutex_RW::~Mutex_RW() noexcept
    {
#ifdef XY_WINDOWS
        // SRWLOCK不需要显式销毁
#else
        try { // 捕获防止输出日志时的异常
            if (int r = pthread_rwlock_destroy(mu(h)); XY_UNLIKELY(r))
                destroy_error(__LINE__, __func__, r);
        } catch (...) {}
        xylu::xymemory::__::under_dealloc(h);
#endif
    }

    void Mutex_RW::Guard_Read::lock()
    {
        if (XY_UNLIKELY(have)) return is_lock_error(__LINE__, __func__);
#ifdef XY_WINDOWS
        AcquireSRWLockShared(srw(m.h));
#else
        if (int r = pthread_rwlock_rdlock(mu(m.h)); XY_UNLIKELY(r))
            rlock_error(__LINE__, __func__, r);
#endif
        have = true;
    }

    bool Mutex_RW::Guard_Read::trylock()
    {
        if (XY_UNLIKELY(have)) return is_lock_error(__LINE__, __func__), true;
#ifdef XY_WINDOWS
        have = TryAcquireSRWLockShared(srw(m.h));
#else
        if (int r = pthread_rwlock_tryrdlock(mu(m.h))) {
            if (XY_LIKELY(r == EBUSY)) have = false;
            else rlock_error(__LINE__, __func__, r);
        }
        else have = true;
#endif
        return have;
    }

    void Mutex_RW::Guard_Read::unlock()
    {
        if (XY_UNLIKELY(!have)) return is_unlock_error(__LINE__, __func__);
#ifdef XY_WINDOWS
        ReleaseSRWLockShared(srw(m.h));
#else
        if (int r = pthread_rwlock_unlock(mu(m.h)); XY_UNLIKELY(r))
            unlock_error(__LINE__, __func__, r);
#endif
        have = false;
    }

    void Mutex_RW::Guard_Write::lock()
    {
        if (XY_UNLIKELY(own)) return is_lock_error(__LINE__, __func__);
#ifdef XY_WINDOWS
        AcquireSRWLockExclusive(srw(m.h));
#else
        if (int r = pthread_rwlock_wrlock(mu(m.h)); XY_UNLIKELY(r))
            wlock_error(__LINE__, __func__, r);
#endif
        own = true;
    }

    bool Mutex_RW::Guard_Write::trylock()
    {
        if (XY_UNLIKELY(own)) return is_lock_error(__LINE__, __func__), true;
#ifdef XY_WINDOWS
        own = TryAcquireSRWLockExclusive(srw(m.h));
#else
        if (int r = pthread_rwlock_trywrlock(mu(m.h))) {
            if (XY_LIKELY(r == EBUSY)) own = false;
            else wlock_error(__LINE__, __func__, r);
        }
        else own = true;
#endif
        return own;
    }

    void Mutex_RW::Guard_Write::unlock()
    {
        if (XY_UNLIKELY(!own)) return is_unlock_error(__LINE__, __func__);
#ifdef XY_WINDOWS
        ReleaseSRWLockExclusive(srw(m.h));
#else
        if (int r = pthread_rwlock_unlock(mu(m.h)); XY_UNLIKELY(r))
            unlock_error(__LINE__, __func__, r);
#endif
        own = false;
    }
}

#endif // XY_UNTHREAD