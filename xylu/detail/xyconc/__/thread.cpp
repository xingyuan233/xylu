#include "./nativeth.h"

#if XY_THREAD

#include "../thread.h"

namespace xylu::xyconc
{
    void* Thread::current() noexcept
    {
#if defined _WIN32
        auto tid = GetCurrentThreadId();
        return reinterpret_cast<void*>(static_cast<uintptr_t>(tid));
#else
        if constexpr (is_pthread_scalar)
        {
            auto tid = pthread_self();
            return reinterpret_cast<void*>((uintptr_t)tid);
        }
        else
        {
            static thread_local int tmp;
            return &tmp;
        }
#endif
    }

    void Thread::yield() noexcept
    {
#if defined _WIN32
        SwitchToThread();
#else
        sched_yield();
#endif
    }
}

namespace
{
    using xylu::xyconc::Thread;
    constexpr xyu::StringView sstatus(Thread::_Status s) noexcept
    {
        switch (s) {
            case Thread::Uninit: return "uninit";
            case Thread::Running: return "running";
            case Thread::Joined: return "joined";
            case Thread::Detached: return "detached";
            case Thread::Finished: return "finished";
            case Thread::Failed: return "failed";
            default: return "unknown";
        }
    }

    using Fun_t = void(*)(void*);
    using Fun_nt = void(*)(void*) noexcept;

    template <bool nothrow>
    struct ThreadParam
    {
        xyu::t_cond<nothrow, Fun_nt, Fun_t> fun;
        void* arg;
    };

    XY_NOINLINE XY_COLD
    void log_thread_throw() noexcept
    {
        xyloge("xyu::E_Thread: thread function throw an uncatched exception");
    }

    XY_native_func_r fp(void* tmp) noexcept
    {
        using Param = ThreadParam<false>;
        Param p = *reinterpret_cast<Param*>(tmp);
        xylu::xymemory::__::under_dealloc(tmp);
        try { p.fun(p.arg); }
        catch (...)
        {
            log_thread_throw();
            if constexpr (!XY_RELEASE) xyu::abort();
        }
        return 0;
    }

    XY_native_func_r nfp(void* tmp) noexcept
    {
        using Param = ThreadParam<true>;
        Param p = *(Param*)tmp;
        xylu::xymemory::__::under_dealloc(tmp);
        p.fun(p.arg);
        return 0;
    }
}

namespace xylu::xyconc
{
    Thread::~Thread() noexcept
    {
        if (s == Running) join();
        native_destroy(h);
        s = Uninit;
    }

    template <typename Fun, typename Fp>
    static void create_impl(void*& h, Thread::_Status& s, Fun fun, void* arg, Fp f) noexcept
    {
        if constexpr (!XY_RELEASE)
        {
            if (XY_UNLIKELY(!fun))
            {
                xyloge("xyu::Thread::create: can't create thread with null function");
                xyu::abort();
            }
            if (XY_UNLIKELY(s == Thread::Running || s == Thread::Joined))
            {
                xyloge("xyu::Thread::create: thread is {}, cannot create new thread", sstatus(s));
                xyu::abort();
            }
        }

        using Param =  ThreadParam<xyu::t_can_nothrow_call<Fun, void*>>;
        auto* param = (Param*)xymemory::__::under_alloc(sizeof(Param));
        if (XY_UNLIKELY(!param)) xymemory::__::abort_alloc_failed(sizeof(Param), 1, alignof(Param));
        param->fun = fun;
        param->arg = arg;
        native_create(h, f, param, "xyu::Thread::create", [](void* param){ xyu::dealloc(xyu::native_v, (Param*)param); });
        s = Thread::Running;
    }

    void Thread::create(Fun_t fun, void* arg) noexcept { create_impl(h, s, fun, arg, fp); }
    void Thread::create(Fun_nt fun, void* arg) noexcept { create_impl(h, s, fun, arg, nfp); }

    void Thread::join() noexcept
    {
        if constexpr (!XY_RELEASE) if (XY_UNLIKELY(s != Running))
        {
            xyloge("xyu::Thread::join: thread is {}, cannot join", sstatus(s));
            xyu::abort();
        }

        native_join(h, "xyu::Thread::join", [this]{ this->s = Failed; });
        s = Finished;
    }

    void Thread::detach() noexcept
    {
        if constexpr (!XY_RELEASE) if (XY_UNLIKELY(s != Running))
        {
            xyloge("xyu::Thread::detach: thread is {}, cannot detach", sstatus(s));
            xyu::abort();
        }

        native_detach(h, "xyu::Thread::deatch", [this]{ this->s = Failed; });
        s = Detached;
    }
}

#endif
