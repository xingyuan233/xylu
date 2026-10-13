#pragma once

#include "../../xycore/config.h"
#include "../../xystring/fmt.h"

#if XY_THREAD

#if defined _WIN32
#  include <windows.h>
#  include <process.h>
#else
#  include <pthread.h>
#  include <sched.h>
#  include <cstdint>
#endif
#include <cerrno>

#if !defined _WIN32
// pthread_t 是否能用 void* 存储 (指针或整型)
constexpr bool is_pthread_scalar = sizeof(pthread_t) <= sizeof(void*) && (xyu::t_is_pointer<pthread_t> || xyu::t_is_integer<pthread_t>);
#endif

/// 跨平台句柄
namespace xylu::xyconc
{
#if defined _WIN32
    using native_func_t = xyu::uint32(__stdcall*)(void*);
#define XY_native_func_r xyu::uint32 __stdcall
    inline HANDLE th(void* handle) noexcept { return static_cast<HANDLE>(handle); }
#else
    using native_func_t = void*(*)(void*);
#define XY_native_func_r void* // __cdecl
    inline pthread_t th(void* handle) noexcept
    {
        if constexpr (!is_pthread_scalar) return *(pthread_t*)handle;
        else return (pthread_t)(uintptr_t)handle;
    }
#endif
}

/// 创建
namespace xylu::xyconc
{
    [[noreturn]] XY_NOINLINE XY_COLD
    inline void unknown_error(const char* func, int err) noexcept
    {
        xyu::E_Logic::abort_unknown_error(func, err);
    }
    
    [[noreturn]] XY_NOINLINE XY_COLD
    inline void create_error(const char* func, int err) noexcept
    {
        switch (err) {
        case EPERM:
            xyloge("{}: no permission to create thread", func);
            xyu::abort();
        case EAGAIN:
            xylogf("{}: too many threads created in system", func);
            xyu::abort();
        case EINVAL:
            xyloge("{}: invalid argument to create thread", func);
            xyu::abort();
        default:
            unknown_error(func, err);
        }
    }
    
    template <typename Destroy = int>
    void native_create(void*& h, native_func_t func, void* param, const char* name, Destroy destroy = 0) noexcept
    {
#if defined _WIN32
        auto handle = _beginthreadex(nullptr, 0, func, param, 0, nullptr);
        if (XY_UNLIKELY(!handle))
        {
            if constexpr (xyu::t_can_call<Destroy, void*>) destroy(param);
            create_error(name, errno);
        }
        h = reinterpret_cast<void*>(handle);
#else
        pthread_t tid;
        pthread_t* p;
        if constexpr (is_pthread_scalar) p = &tid;
        else
        {
            if (h) xyu::dealloc(xyu::native_v, (pthread_t*)h), h = nullptr;
            p = (pthread_t*)xymemory::__::under_alloc(sizeof(pthread_t));
            if (xyu::N_Log_Level::Debug <= xyu::K_Log_Level_New && xycore::__::glogger_inited())
                xymemory::__::log_under_alloc(p, sizeof(pthread_t), 1, alignof(pthread_t));
            if (XY_UNLIKELY(!p)) xymemory::__::abort_alloc_failed(sizeof(pthread_t), 1, alignof(pthread_t));
        }

        if (int r = pthread_create(p, nullptr, func, param); XY_UNLIKELY(r))
        {
            if constexpr (xyu::t_can_call<Destroy, void*>) destroy(param);
            if constexpr (!is_pthread_scalar) xyu::dealloc(xyu::native_v, (pthread_t*)p);
            create_error(name, r);
        }
        if constexpr (is_pthread_scalar) h = (void*)(uintptr_t)tid;
        else h = p;
#endif
    }
}

/// 销毁
namespace xylu::xyconc
{
    inline void native_destroy(void*& h) noexcept
    {
#ifndef _WIN32
        if constexpr (!is_pthread_scalar)
            if (h) xyu::dealloc(xyu::native_v, (pthread_t*)h), h = nullptr;
#endif
    }
}

/// 等待
namespace xylu::xyconc
{
    [[noreturn]] XY_NOINLINE XY_COLD
    inline void join_error(const char* func, int err) noexcept
    {
        switch (err) 
        {
#if defined _WIN32
        case ERROR_ACCESS_DENIED:
            xyloge("{}: no permission to join thread unexpectedly", func);
            xyu::abort();
        case ERROR_NOT_ENOUGH_MEMORY:
            xylogf("{}: system resources are exhausted to join thread", func);
            xyu::abort();
        case ERROR_INVALID_HANDLE:
            xyloge("{}: thread is invalid or was died unexpectedly", func);
            xyu::abort();
#else
        case ESRCH:
            xyloge("{}: thread is not found unexpectedly", func);
            xyu::abort();
        case EINVAL:
            xyloge("{}: thread is not joinable", func);
            xyu::abort();
        case EDEADLK:
            xyloge("{}: can't join another thread that has already joined this", func);
            xyu::abort();
#endif
        default:
            unknown_error(func, err);
        }
    }

    template <typename FailHandle = int>
    void native_join(void* h, const char* name, FailHandle fh = 0) noexcept
    {
#if defined _WIN32
        auto ret = WaitForSingleObject(th(h), INFINITE);
        int err;
        if (XY_UNLIKELY(ret != WAIT_OBJECT_0))
        {
            err = GetLastError();
            if (err == ERROR_NOT_ENOUGH_MEMORY)
            {
                xyu::alloc(nullptr)();
                ret = WaitForSingleObject(th(h), INFINITE);
                if (ret != WAIT_OBJECT_0) err = GetLastError();
            }
        }
        CloseHandle(th(h));
        if (XY_UNLIKELY(ret != WAIT_OBJECT_0))
        {
            if constexpr (xyu::t_can_call<FailHandle>) fh();
            join_error(name, err);
        }
#else
        if (int r = pthread_join(th(h), nullptr); XY_UNLIKELY(r))
        {
            if constexpr (xyu::t_can_call<FailHandle>) fh();
            join_error(name, r);
        }
#endif
    }
}

/// 分离
namespace xylu::xyconc
{
    [[noreturn]] XY_NOINLINE XY_COLD
    inline void detach_error(const char* func, int err) noexcept
    {
        switch (err)
        {
#if defined _WIN32
        case ERROR_GEN_FAILURE:
            xylogf("{}: kernel state is broken, unrecoverable failure", func);
            xyu::abort();
        case ERROR_INVALID_HANDLE:
            xyloge("{}: thread is invalid or was died unexpectedly", func);
            xyu::abort();
#else
        case ESRCH:
            xyloge("{}: thread is not found unexpectedly", func);
            xyu::abort();
        case EINVAL:
            xyloge("{}: thread is not detachable", func);
            xyu::abort();
#endif
        default:
            unknown_error(func, err);
        }
    }

    template <typename FailHandle = int>
    void native_detach(void* h, const char* name, FailHandle fh = 0) noexcept
    {
#if defined _WIN32
        if (XY_UNLIKELY(!CloseHandle(th(h))))
        {
            if constexpr (xyu::t_can_call<FailHandle>) fh();
            detach_error(name, GetLastError());
        }
#else
        if (int r = pthread_detach(th(h)); XY_UNLIKELY(r))
        {
            if constexpr (xyu::t_can_call<FailHandle>) fh();
            detach_error(name, r);
        }
#endif
    }
}

#endif
