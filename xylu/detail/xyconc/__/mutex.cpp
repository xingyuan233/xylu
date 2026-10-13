#include "../mutex.h"
#include "../../xystring/fmt.h"

#if defined(_WIN32)
#  include <windows.h>
#else
#  include <pthread.h>
#  include <cerrno>
#endif

#if XY_DEBUG
#  include "../../xycoll/hash.h"
#endif

/// 多活跃锁位检查
namespace
{
#define XY_USE_ALIVEGUARD XY_DEBUG && 0
#if XY_USE_ALIVEGUARD
    class AliveGuard
    {
        using Set = xyu::HashSet<void*, true>;
        alignas(Set) xyu::uint8 buf[sizeof(Set)];
    public:
        AliveGuard() noexcept { new (buf) Set{}; }
        ~AliveGuard() noexcept
        {
            // 防止挂入调式器后影响tls析构顺序导致内存回收后才进行析构
            if ((buf[sizeof(void*) - 1] == 0xee || buf[sizeof(void*) - 1] == 0xfe) &&
                (buf[sizeof(void*) - 2] == 0xee || buf[sizeof(void*) - 2] == 0xfe)) return;
            get().~HashSet();
        }

        Set& get() noexcept { return *(Set*)buf; }
        void erase(void* h) noexcept { get().erase(h); }
        void insert(void* h) noexcept
        {
            if (XY_UNLIKELY(get().select((h))))
            {
                xyloge("xyu::E_Mutex: mutex cannot have more than one active Guard at the same time");
                xyu::abort();
            }
            get().append(h);
        }
    };

    AliveGuard& alive_guard() noexcept
    {
        thread_local AliveGuard set;
        return set;
    }
#endif
}

/// 跨平台句柄
namespace
{
#ifdef _WIN32
    SRWLOCK* mu(void*& h) noexcept { return reinterpret_cast<SRWLOCK*>(&h); }
    SRWLOCK* rwmu(void*& h) noexcept { return reinterpret_cast<SRWLOCK*>(&h); }
#else
    pthread_mutex_t* mu(void* h) noexcept { return static_cast<pthread_mutex_t*>(h); }
    pthread_rwlock_t* rwmu(void* h) noexcept { return static_cast<pthread_rwlock_t*>(h); }
#endif
}

/// 创建
namespace xylu::xyconc
{
#ifndef _WIN32
    [[noreturn]] XY_NOINLINE XY_COLD
    static void public_error(const char* func, int err) noexcept
    {
        if (err == EINVAL) xyloge("{}: mutex is not initialised or is already destroyed", func), xyu::abort();
        xyu::E_Logic::abort_unknown_error(func, err);
    }
#endif

#ifndef _WIN32
    [[noreturn]] XY_NOINLINE XY_COLD
    static void create_error(const char* func, int err) noexcept
    {
        switch (err)
        {
        case EAGAIN:
            xyloge("{}: system lacked necessary resources to create mutex", func);
            xyu::abort();
        case EPERM:
            xyloge("{}: no permission to create mutex", func);
            xyu::abort();
        case ENOMEM:
            xyloge("{}: no memory to create mutex", func);
            xyu::abort();
        case EBUSY:
            xyloge("{}: mutex is already initialised", func);
            xyu::abort();
        case EINVAL:
            xyloge("{}: invalid argument to create mutex", func);
            xyu::abort();
        default:
            public_error( func, err);
        }
    }
#endif

    Mutex::Mutex() noexcept : h{}
    {
#if XY_USE_ALIVEGUARD
        // 去掉这行可能会引起tls释放顺序混乱
        alive_guard();
#endif
#ifdef _WIN32
        // SRWLOCK 与 h 大小相同，不需要动态分配
        static_assert(sizeof(SRWLOCK) == sizeof(void*));
        *reinterpret_cast<SRWLOCK*>(&h) = SRWLOCK_INIT;
        //InitializeSRWLock(mu(h));
#else
        h = (pthread_mutex_t*)xymemory::__::under_alloc(sizeof(pthread_mutex_t));
        if (XY_UNLIKELY(!h)) xymemory::__::abort_alloc_failed(sizeof(pthread_mutex_t), 1, alignof(pthread_mutex_t));
        int r;
        for (int i = 0; i < 3; ++i)
        {
            r = pthread_mutex_init(mu(h), nullptr);
            if (XY_LIKELY(r == 0)) return;
            if (r == EAGAIN) continue;
            break;
        }
        create_error("xyu::Mutex::Mutex", r);
#endif
    }

    RwMutex::RwMutex() noexcept : h{}
    {
#ifdef _WIN32
        // SRWLOCK 与 h 大小相同，不需要动态分配
        *reinterpret_cast<SRWLOCK*>(&h) = SRWLOCK_INIT;
#else
        h = (pthread_mutex_t*)xymemory::__::under_alloc(sizeof(pthread_rwlock_t));
        if (XY_UNLIKELY(!h)) xymemory::__::abort_alloc_failed(sizeof(pthread_rwlock_t), 1, alignof(pthread_rwlock_t));
        int r;
        for (int i = 0; i < 3; ++i)
        {
            r = pthread_rwlock_init(rwmu(h), nullptr);
            if (XY_LIKELY(r == 0)) return;
            if (r == EAGAIN) continue;
            break;
        }
        create_error("xyu::RwMutex::RwMutex", r);
#endif
    }
}

/// 销毁
namespace xylu::xyconc
{
#ifndef _WIN32
    [[noreturn]] XY_NOINLINE XY_COLD
    static void destroy_error(const char* func, int err) noexcept
    {
        switch (err)
        {
        case EBUSY:
            xyloge("{}: mutex is still locked", func);
            xyu::abort();
        default:
            public_error(func, err);
        }
    }
#endif

    Mutex::~Mutex() noexcept
    {
#ifdef _WIN32
        // SRWLOCK不需要显式销毁
#else
        if (int r = pthread_mutex_destroy(mu(h)); XY_UNLIKELY(r))
            destroy_error("xyu::Mutex::~Mutex", r);
        xymemory::__::under_dealloc(h);
#endif
    }

    RwMutex::~RwMutex() noexcept
    {
#ifdef _WIN32
        // SRWLOCK不需要显式销毁
#else
        if (int r = pthread_rwlock_destroy(rwmu(h)); XY_UNLIKELY(r))
            destroy_error("xyu::RwMutex::~RwMutex", r);
        xymemory::__::under_dealloc(h);
#endif
    }
}

/// 上锁
namespace xylu::xyconc
{
#ifndef _WIN32
    [[noreturn]] XY_NOINLINE XY_COLD
    static void lock_error(const char* func, int err) noexcept
    {
        switch (err)
        {
        case EDEADLK:
            xyloge("{}: detected deadlock", func);
            xyu::abort();
        case EAGAIN:
            xyloge("{}: maximum count exceeded", func);
            xyu::abort();
        case EOWNERDEAD:
        case ENOTRECOVERABLE:
            xyloge("{}: last owner died while holding mutex", func);
            xyu::abort();
        default:
            public_error(func, err);
        }
    }
#endif

    [[noreturn]] XY_NOINLINE XY_COLD
    static void abort_is_lock(const char* func) noexcept
    {
        xyloge("{}: mutex is already locked", func);
        xyu::abort();
    }

    void Mutex::Guard::lock() noexcept
    {
#if !XY_RELEASE
        if (XY_UNLIKELY(own)) abort_is_lock("xyu::Mutex::Guard::lock");
#endif
#if XY_USE_ALIVEGUARD
        alive_guard().insert(mu(m->h));
#endif
#ifdef _WIN32
        AcquireSRWLockExclusive(mu(m->h));
#else
        if (int r = pthread_mutex_lock(mu(m->h)); XY_UNLIKELY(r))
            lock_error("xyu::Mutex::Guard::lock", r);
#endif
        own = true;
    }

    [[noreturn]] XY_NOINLINE XY_COLD
    static void out_limit_error(const char* func) noexcept
    {
        xyloge("{}: maximum {} recursive lock count exceeded", func, xyu::size_t(-1));
        xyu::abort();
    }

    void Mutex::RecursiveGuard::Guard::lock() noexcept
    {
#if !XY_RELEASE
        if (XY_UNLIKELY(own)) abort_is_lock("xyu::Mutex::RecursiveGuard::lock");
        if (XY_UNLIKELY(m->d == -1)) out_limit_error("xyu::Mutex::RecursiveGuard::lock");
#endif
        if (m->d == 0)
        {
#if XY_USE_ALIVEGUARD
            alive_guard().insert(mu(m->m->h));
#endif
#ifdef _WIN32
            AcquireSRWLockExclusive(mu(m->m->h));
#else
            if (int r = pthread_mutex_lock(mu(m->m->h)); XY_UNLIKELY(r))
                lock_error("xyu::Mutex::RecursiveGuard::lock", r);
#endif
        }
        own = true;
        ++m->d;
    }

    void RwMutex::Guard::lock() noexcept
    {
#if !XY_RELEASE
        if (XY_UNLIKELY(own)) abort_is_lock("xyu::RwMutex::Guard::lock");
#endif
#if XY_USE_ALIVEGUARD
        alive_guard().insert(rwmu(m->h));
#endif
#ifdef _WIN32
        AcquireSRWLockExclusive(rwmu(m->h));
#else
        if (int r = pthread_rwlock_wrlock(rwmu(m->h)); XY_UNLIKELY(r))
            lock_error("xyu::RwMutex::Guard::lock", r);
#endif
        own = true;
    }

    void RwMutex::SharedGuard::lock() noexcept
    {
#if !XY_RELEASE
        if (XY_UNLIKELY(own)) abort_is_lock("xyu::RwMutex::SharedGuard::lock");
#endif
#if XY_USE_ALIVEGUARD
        alive_guard().insert(rwmu(m->h));
#endif
#ifdef _WIN32
        AcquireSRWLockShared(rwmu(m->h));
#else
        if (int r = pthread_rwlock_rdlock(rwmu(m->h)); XY_UNLIKELY(r))
            lock_error("xyu::RwMutex::SharedGuard::lock", r);
#endif
        own = true;
    }
}

/// 解锁
namespace xylu::xyconc
{
#ifndef _WIN32
    [[noreturn]] XY_NOINLINE XY_COLD
    static void unlock_error(const char* func, int err) noexcept
    {
        switch (err)
        {
        case EPERM:
        case ENOTRECOVERABLE:
            xyloge("{}: thread does not own the mutex", func);
            xyu::abort();
        default:
            public_error(func, err);
        }
    }
#endif

    [[noreturn]] XY_NOINLINE XY_COLD
    static void abort_is_unlock(const char* func) noexcept
    {
        xyloge("{}: mutex is unlocked", func);
        xyu::abort();
    }

    void Mutex::Guard::unlock() noexcept
    {
#if !XY_RELEASE
        if (XY_UNLIKELY(!own)) abort_is_unlock("xyu::Mutex::Guard::unlock");
#endif
        own = false;
#ifdef _WIN32
        ReleaseSRWLockExclusive(mu(m->h));
#else
        if (int r = pthread_mutex_unlock(mu(m->h)); XY_UNLIKELY(r))
            unlock_error("xyu::Mutex::Guard::unlock", r);
#endif
#if XY_USE_ALIVEGUARD
        alive_guard().erase(mu(m->h));
#endif
    }

    void Mutex::RecursiveGuard::Guard::unlock() noexcept
    {
#if !XY_RELEASE
        if (XY_UNLIKELY(!own)) abort_is_unlock("xyu::Mutex::RecursiveGuard::unlock");
#endif
        own = false;
        if (--m->d == 0)
        {
#ifdef _WIN32
            ReleaseSRWLockExclusive(mu(m->m->h));
#else
            if (int r = pthread_mutex_unlock(mu(m->m->h)); XY_UNLIKELY(r))
                unlock_error("xyu::Mutex::RecursiveGuard::unlock", r);
#endif
#if XY_USE_ALIVEGUARD
            alive_guard().erase(mu(m->m->h));
#endif
        }
    }

    void RwMutex::Guard::unlock() noexcept
    {
#if !XY_RELEASE
        if (XY_UNLIKELY(!own)) abort_is_unlock("xyu::RwMutex::Guard::unlock");
#endif
        own = false;
#ifdef _WIN32
        ReleaseSRWLockExclusive(rwmu(m->h));
#else
        if (int r = pthread_rwlock_unlock(rwmu(m->h)); XY_UNLIKELY(r))
            unlock_error("xyu::RwMutex::Guard::unlock", r);
#endif
#if XY_USE_ALIVEGUARD
        alive_guard().erase(rwmu(m->h));
#endif
    }

    void RwMutex::SharedGuard::unlock() noexcept
    {
#if !XY_RELEASE
        if (XY_UNLIKELY(!own)) abort_is_unlock("xyu::RwMutex::SharedGuard::unlock");
#endif
        own = false;
#ifdef _WIN32
        ReleaseSRWLockShared(rwmu(m->h));
#else
        if (int r = pthread_rwlock_unlock(rwmu(m->h)); XY_UNLIKELY(r))
            unlock_error("xyu::RwMutex::SharedGuard::unlock", r);
#endif
#if XY_USE_ALIVEGUARD
        alive_guard().erase(rwmu(m->h));
#endif
    }
}

/// 尝试上锁
namespace xylu::xyconc
{
    bool Mutex::Guard::trylock() noexcept
    {
#if !XY_RELEASE
        if (XY_UNLIKELY(own)) abort_is_lock("xyu::Mutex::Guard::trylock");
#endif
#ifdef _WIN32
        own = TryAcquireSRWLockExclusive(mu(m->h));
#else
        if (int r = pthread_mutex_trylock(mu(m->h)))
        {
            if (XY_UNLIKELY(r != EBUSY)) lock_error("xyu::Mutex::Guard::trylock", r);
            own = false;
        }
        else own = true;
#endif
#if XY_USE_ALIVEGUARD
        if (own) alive_guard().insert(mu(m->h));
#endif
        return own;
    }

    bool Mutex::RecursiveGuard::Guard::trylock() noexcept
    {
#if !XY_RELEASE
        if (XY_UNLIKELY(own)) abort_is_lock("xyu::Mutex::RecursiveGuard::trylock");
        if (XY_UNLIKELY(m->d == -1)) out_limit_error("xyu::Mutex::RecursiveGuard::trylock");
#endif
        if (m->d) ++m->d;
        else
        {
#ifdef _WIN32
            m->d = own = TryAcquireSRWLockExclusive(mu(m->m->h));
#else
            if (int r = pthread_mutex_trylock(mu(m->m->h)))
            {
                if (XY_UNLIKELY(r != EBUSY)) lock_error("xyu::Mutex::RecursiveGuard::trylock", r);
                m->d = own = false;
            }
            else m->d = own = true;
#endif
#if XY_USE_ALIVEGUARD
            if (own) alive_guard().insert(mu(m->m->h));
#endif
        }
        return own;
    }

    bool RwMutex::Guard::trylock() noexcept
    {
#if !XY_RELEASE
        if (XY_UNLIKELY(own)) abort_is_lock("xyu::RwMutex::Guard::trylock");
#endif
#ifdef _WIN32
        own = TryAcquireSRWLockExclusive(rwmu(m->h));
#else
        if (int r = pthread_rwlock_trywrlock(rwmu(m->h))) {
            if (XY_LIKELY(r == EBUSY)) own = false;
            else lock_error("xyu::RwMutex::Guard::trylock", r);
        }
        else own = true;
#endif
#if XY_USE_ALIVEGUARD
        if (own) alive_guard().insert(rwmu(m->h));
#endif
        return own;
    }

    bool RwMutex::SharedGuard::trylock() noexcept
    {
#if !XY_RELEASE
        if (XY_UNLIKELY(own)) abort_is_lock("xyu::RwMutex::SharedGuard::trylock");
#endif
#ifdef _WIN32
        own = TryAcquireSRWLockShared(rwmu(m->h));
#else
        if (int r = pthread_rwlock_tryrdlock(rwmu(m->h))) {
            if (XY_LIKELY(r == EBUSY)) own = false;
            else lock_error("xyu::RwMutex::SharedGuard::trylock", r);
        }
        else own = true;
#endif
#if XY_USE_ALIVEGUARD
        if (own) alive_guard().insert(rwmu(m->h));
#endif
        return own;
    }
}
