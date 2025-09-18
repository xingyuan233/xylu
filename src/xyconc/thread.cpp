#if defined(__CYGWIN__)
#include <thread>
#include <system_error>
#elif defined(_WIN32)
#include <windows.h>
#include <process.h>
#else
#include <pthread.h>
#endif

#include "../../link/config"
#if !XY_UNTHREAD

#include "../../head/xyconc/thread.h"
#include "../../link/log"

// CYGWIN 下不支持直接用 pthread_t 或 HANDLE，都会导致运行崩溃
// 因此只能直接使用 std::thread，其实现中 只有一个8字节(64位操作系统)的id成员，因此直接用 void* 操作

namespace
{
#if defined(__CYGWIN__)
    std::thread th(void* handle) noexcept { return xyu::move(*reinterpret_cast<std::thread*>(&handle)); }
#elif defined(_WIN32)
    HANDLE th(void *handle) noexcept { return reinterpret_cast<HANDLE>(handle); }
#else
    pthread_t th(void* handle) noexcept { return reinterpret_cast<pthread_t>(handle); }
#endif

#if defined(__CYGWIN__)
#elif defined(_WIN32)
    xyu::uint __stdcall fp(void* tmp) noexcept
    {
        void** p = reinterpret_cast<void**>(tmp);
        try { reinterpret_cast<void*(*)(void*)>(p[0])(p[1]); }
        catch (...) {}
        xylu::xymemory::__::under_dealloc(tmp);
        return 0;
    }
#else
    void* fp(void* tmp) noexcept
    {
        void** p = reinterpret_cast<void**>(tmp);
        try { reinterpret_cast<void*(*)(void*)>(p[0])(p[1]); }
        catch (...) {}
        xylu::xymemory::__::under_dealloc(tmp);
        return nullptr;
    }
#endif
}

namespace
{
    using xylu::xyconc::__::ThreadStatus;
    constexpr auto sstatus = [](ThreadStatus::Status s) noexcept -> xyu::StringView {
        switch (s) {
            case ThreadStatus::Uninit: return "uninit";
            case ThreadStatus::Running: return "running";
            case ThreadStatus::Joined: return "joined";
            case ThreadStatus::Detached: return "detached";
            case ThreadStatus::Finished: return "finished";
            case ThreadStatus::Failed: return "failed";
            default: return "unknown";
        }
    };

    template <typename I>
    [[noreturn]] void unknown_error(xyu::uint line, const char* func, I err)
    {
        xyloge2(0, "E_Thread: unknown error with code {}", line, func, err);
        throw xyu::E_Thread{};
    }

    [[noreturn]] void create_error(xyu::uint line, const char* func, int err)
    {
        switch (err) {
            case EAGAIN:
                xyloge2(1, "E_Thread_Create_Limit: too many threads created in system", line, func);
                throw xyu::E_Thread_Create_Limit{};
            case EINVAL:
                xyloge2(0, "E_Thread_Invalid_Argument: invalid argument to create thread", line, func);
                throw xyu::make_error(xyu::E_Thread{}, xyu::E_Logic_Invalid_Argument{});
            case EPERM:
                xyloge2(0, "E_Thread_Permission_Denied: permission denied to create thread", line, func);
                throw xyu::E_Thread_Permission_Denied{};
            default:
                unknown_error(line, func, err);
        }
    }

#if defined(_WIN32)
    [[noreturn]] void join_error(xyu::uint line, const char* func, DWORD err)
    {
        switch (err) {
            case ERROR_INVALID_HANDLE:
                xyloge2(0, "E_Thread_Invalid_State: thread is not initialized or is already closed", line, func);
                throw xyu::E_Thread_Invalid_State{};
            case ERROR_ACCESS_DENIED:
                xyloge2(0, "E_Thread_Permission_Denied: permission denied to join thread", line, func);
                throw xyu::E_Thread_Permission_Denied{};
            case ERROR_NOT_ENOUGH_MEMORY:
                xyloge2(1, "E_Thread_No_Memory: system resources are exhausted", line, func);
                throw xyu::E_Thread_No_Memory{};
            default:
                unknown_error(line, func, err);
        }
    }
#else
    [[noreturn]] void join_error(xyu::uint line, const char* func, int err)
    {
        switch (err) {
            case ESRCH:
                xyloge2(0, "E_Thread_Invalid_State: thread is not found", line, func);
                throw xyu::E_Thread_Invalid_State{};
            case EINVAL:
                xyloge2(0, "E_Thread_Invalid_State: thread is not joinable", line, func);
                throw xyu::E_Thread_Invalid_State{};
            case EDEADLK:
                xyloge2(0, "E_Thread_Deadlock: thread B cannot join thread A  because thread A is already joined thread B", line, func);
                throw xyu::E_Thread_Deadlock{};
            default:
                unknown_error(line, func, err);
        }
    }
#endif

#if defined(_WIN32)
    [[noreturn]] void detach_error(xyu::uint line, const char* func, DWORD err)
    {
        switch (err) {
            case ERROR_INVALID_HANDLE:
                xyloge2(0, "E_Thread_Invalid_State: thread is not initialized or is already closed", line, func);
                throw xyu::E_Thread_Invalid_State{};
            case ERROR_GEN_FAILURE:
                xyloge2(1, "E_Thread_Device: physical device is not available while attempting to detach thread", line, func);
                throw xyu::E_Thread_Device{};
            default:
                unknown_error(line, func, err);
        }
    }
#else
    [[noreturn]] void detach_error(xyu::uint line, const char* func, int err)
    {
        switch (err) {
            case ESRCH:
                xyloge2(0, "E_Thread_Invalid_State: thread is not found", line, func);
                throw xyu::E_Thread_Invalid_State{};
            case EINVAL:
                xyloge2(0, "E_Thread_Invalid_State: thread is not detachable", line, func);
                throw xyu::E_Thread_Invalid_State{};
            default:
                unknown_error(line, func, err);
        }
    }
#endif
}

namespace xylu::xyconc
{
    Thread_Native::~Thread_Native() noexcept
    {
        if (s == Running) try { join(); s = Uninit; } catch (...) {}
    }

    void Thread_Native::create(Fun_t fun, void* arg)
    {
        if (XY_UNLIKELY(s == Running || s == Joined)) {
            xyloge(0, "E_Thread_Invalid_State: thread is {}, cannot create new thread", sstatus(s));
            throw xyu::make_error(xyu::E_Thread_Invalid_State{}, xyu::E_Logic_Invalid_Argument{});
        }
        if (XY_UNLIKELY(!fun)) {
            xyloge(0, "E_Thread_Null_Pointer: cannot create thread with null function");
            throw xyu::make_error(xyu::E_Thread{}, xyu::E_Logic_Null_Pointer{});
        }
#if defined(__CYGWIN__)
        static_assert(sizeof(std::thread) == sizeof(void*), "std::thread size mismatch");
        alignas(alignof(std::thread)) xyu::uint8 XY_MAY_ALIAS buf[sizeof(std::thread)];
        try {
            ::new (buf) std::thread(fun, arg);
        } catch (std::bad_alloc&) {
            xyloge2(1, "E_Thread_No_Memory: no memory to create thread", __LINE__, __func__);
            throw xyu::E_Thread_No_Memory{};
        } catch (std::system_error& e) {
            create_error(__LINE__, __func__, e.code().value());
        }
        h = *reinterpret_cast<void**>(buf);
#else
        void** data = (void**)xylu::xymemory::__::under_alloc(2 * sizeof(void*));
        data[0] = reinterpret_cast<void*>(fun);
        data[1] = arg;
#if defined(_WIN32)
        auto handle = _beginthreadex(nullptr, 0, fp, data, 0, nullptr);
        if (XY_UNLIKELY(!handle)) create_error(__LINE__, __func__, errno);
        h = reinterpret_cast<void*>(handle);
#else
        if (int r = pthread_create(reinterpret_cast<pthread_t*>(&h), nullptr, fp, data); XY_UNLIKELY(r))
            create_error(__LINE__, __func__, r);
#endif
#endif
        s = Running;
    }

    void Thread_Native::join()
    {
        if (XY_UNLIKELY(s != Running)) {
            xyloge(0, "E_Thread_Invalid_State: thread is {}, cannot join", sstatus(s));
            throw xyu::make_error(xyu::E_Thread_Invalid_State{}, xyu::E_Logic_Invalid_Argument{});
        }
#if defined(__CYGWIN__)
        try { th(h).join(); }
        catch (std::system_error& e) {
             join_error(__LINE__, __func__, e.code().value());
        }
#elif defined(_WIN32)
        if (XY_UNLIKELY(WaitForSingleObject(th(h), INFINITE) != WAIT_OBJECT_0))
            join_error(__LINE__, __func__, GetLastError());
#else
        if (int r = pthread_join(th(h), nullptr); XY_UNLIKELY(r))
            join_error(__LINE__, __func__, r);
#endif
        s = Joined;
    }

    void Thread_Native::detach()
    {
        if (XY_UNLIKELY(s != Running)) {
            xyloge(0, "E_Thread_Invalid_State: thread is {}, cannot detach", sstatus(s));
            throw xyu::make_error(xyu::E_Thread_Invalid_State{}, xyu::E_Logic_Invalid_Argument{});
        }
#if defined(__CYGWIN__)
        try { th(h).detach(); }
        catch (std::system_error& e) {
            detach_error(__LINE__, __func__, e.code().value());
        }
#elif defined(_WIN32)
        if (XY_UNLIKELY(!CloseHandle(th(h)))) detach_error(__LINE__, __func__, GetLastError());
#else
        if (int r = pthread_detach(th(h)); XY_UNLIKELY(r))
            detach_error(__LINE__, __func__, r);
#endif
        s = Detached;
    }


    void* Thread_Native::id() noexcept
    {
#if defined(__CYGWIN__)
        auto tid = std::this_thread::get_id();
        return *reinterpret_cast<void**>(&tid);
#elif defined(_WIN32)
        auto tid = GetCurrentThreadId();
        return reinterpret_cast<void*>(tid);
#else
        auto tid = pthread_self();
        return reinterpret_cast<void*>(tid);
#endif
    }

    void Thread_Native::yield() noexcept
    {
#if defined(__CYGWIN__)
        std::this_thread::yield();
#elif defined(_WIN32)
        SwitchToThread();
#else
        sched_yield();
#endif
    }
}

namespace xylu::xyconc
{
    void Thread::check_status()
    {
        if (sp)
        {
            auto& status = *reinterpret_cast<__::ThreadStatus_Del*>(sp);
            Status s = status.s.load(xyu::N_ATOMIC_ACQUIRE);
            // 线程未结束
            if (XY_UNLIKELY(s > Finished)) {
                xyloge(0, "E_Thread_Invalid_State: thread is {}, cannot create new thread", sstatus(s));
                throw xyu::make_error(xyu::E_Thread_Invalid_State{}, xyu::E_Logic_Invalid_Argument{});
            }
            // 返回值未析构
            if (s == Finished && status.dtor) status.dtor(sp);
        }
    }

#if (defined(__CYGWIN__) || defined(_WIN32))
    void Thread::create_help(xyu::uint __stdcall (*fun)(void*), void* arg)
    {
#if defined(__CYGWIN__)
        try {
            std::thread th(fun, arg);
            th.detach();
        } catch (std::bad_alloc&) {
            xyloge2(1, "E_Thread_No_Memory: no memory to create thread", __LINE__, __func__);
            throw xyu::E_Thread_No_Memory{};
        } catch (std::system_error& e) {
            create_error(__LINE__, __func__, e.code().value());
        }
#else
        auto handle = _beginthreadex(nullptr, 0, fun, arg, 0, nullptr);
        if (XY_UNLIKELY(!handle)) create_error(__LINE__, __func__, errno);
        if (XY_UNLIKELY(!CloseHandle(reinterpret_cast<HANDLE>(handle))))
            detach_error(__LINE__, __func__, GetLastError());
#endif
    }
#else
    void Thread::create_help(void* (*fun)(void*), void* arg)
    {
        pthread_t handle;
        if (int r = pthread_create(&handle, nullptr, fun, arg); XY_UNLIKELY(r))
            create_error(__LINE__, __func__, r);
        if (int r = pthread_detach(handle); XY_UNLIKELY(r))
            detach_error(__LINE__, __func__, r);
    }
#endif

    void Thread::get_help(xyu::Duration_ms du)
    {
        // 线程未创建
        if (XY_UNLIKELY(!sp)) {
            xyloge(false, "E_Thread_Invalid_State: thread is not created");
            throw xyu::E_Thread_Invalid_State{};
        }
        // 等待线程结束
        wait(du);
        auto& status = *reinterpret_cast<__::ThreadStatus_Total<void>*>(sp);
        Status s = status.s.load(xyu::N_ATOMIC_ACQUIRE);
        // 异常抛出
        if (XY_UNLIKELY(s == Failed))
        {
            xyu::ErrorPtr ep = xyu::move(status.ep);
            xyu::dealloc(xyu::native_v, sp);
            sp = nullptr;
            ep.rethrow();
        }
    }
}

#endif