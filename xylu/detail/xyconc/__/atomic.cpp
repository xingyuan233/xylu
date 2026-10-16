#include "../atomic.h"
#include "../../xystring/fmt.h"

#if !XY_OUTPUT_ERROR_LOG
#  undef xylog
#  define xylog(...) 0
#endif

#include <cerrno>
#include <cstdlib>
#include <cstdint>

#ifdef _WIN32
#  include <Windows.h>
#  include "../mutex.h"
#  include "../condvar.h"
#else
#  include <unistd.h>
#  include <syscall.h>
#  ifdef _GLIBCXX_HAVE_LINUX_FUTEX
#    define _GLIBCXX_HAVE_PLATFORM_WAIT
#  include <linux/futex.h>
#  endif
#endif

#if defined _GLIBCXX_HAS_GTHREADS && defined _GLIBCXX_USE_SCHED_YIELD
#  include <bits/gthr.h>
#endif
#if defined __i386__ || defined __x86_64__
#  include <emmintrin.h>
#endif

/// 平台函数
namespace
{
#ifdef _GLIBCXX_HAVE_PLATFORM_WAIT
    using swait_t = unsigned int;
#elif __GCC_ATOMIC_LONG_LOCK_FREE == 2
    using swait_t = unsigned long;
# else
    using swait_t = unsigned int;
#endif

#if defined _GLIBCXX_HAVE_PLATFORM_WAIT
#   define XY_HAVE_PLATFORM_WAIT 1
    // 等待值变化
    void native_wait(void* p, swait_t val) noexcept
    {
        auto r = syscall(SYS_futex, p, FUTEX_WAIT_PRIVATE, (int)val, nullptr);
        if (!r || errno == EAGAIN) return;
        if (XY_UNLIKELY(errno != EINTR)) xyu::abort();
    }

    // 唤醒等待的线程
    template <bool all>
    void native_notify(void* p) noexcept
    {
        syscall(SYS_futex, p, FUTEX_WAKE_PRIVATE, all ? xyu::t_number<int>::max : 1);
    }
#elif defined __MINGW32__ && __has_include(<synchapi.h>) && 0
    // 好奇怪啊，为什么链接不到？？？
#   define XY_HAVE_PLATFORM_WAIT 1
    // 等待值变化
    void native_wait(xyu::size_t size, void* p, const void* vp) noexcept
    {
        if (WaitOnAddress(p, const_cast<void*>(vp), size, INFINITE) == FALSE) xyu::abort();
    }

    // 唤醒等待的线程
    template <bool all>
    void native_notify(void* p) noexcept
    {
        if constexpr (all) WakeByAddressAll(p);
        else WakeByAddressSingle(p);
    }
#endif

    // 线程让片
    void thread_yield() noexcept
    {
#if defined _GLIBCXX_HAS_GTHREADS && defined _GLIBCXX_USE_SCHED_YIELD
        __gthread_yield();
#endif
    }
    // 线程暂停
    void thread_relax() noexcept
    {
#if defined __i386__ || defined __x86_64__
        _mm_pause();
#else
        thread_yield();
#endif
    }
}

/// 等待与唤醒
namespace xylu::xyconc::__
{
    /// 共享等待池
    struct wait_pool
    {
    private:
        // 等待线程数量
        alignas(xyu::K_Cache_Line_Size) swait_t count = 0;
#ifndef XY_HAVE_PLATFORM_WAIT
        Mutex m;
#endif
#ifdef XY_HAVE_PLATFORM_WAIT
        // 版本号
        alignas(xyu::K_Cache_Line_Size) swait_t version = 0;
#else
        Condvar cv;
#endif

    public:
        // 获取等待池
        static wait_pool& get(xyu::size_t size, void* p) noexcept(xyu::t_can_nothrow_construct<wait_pool>)
        {
            // 直接可等待则不进入等待池
#ifdef XY_HAVE_PLATFORM_WAIT
            static struct Fake {} fake;
# ifdef _WIN32
            if (xyu::bit_count_1_total(size) == 1 && size <= 8)
# else
            if (size == sizeof(int))
# endif
                return *reinterpret_cast<wait_pool*>(&fake);
#endif
            constexpr int pool_count = 16;

            static wait_pool pools[pool_count];
            auto hash = (reinterpret_cast<uintptr_t>(p) >> 4) & (pool_count - 1);
            return pools[hash];
        }

        // 进入等待池
        void enter(xyu::size_t size) noexcept
        {
            // 直接可等待则不进入等待池
#ifdef XY_HAVE_PLATFORM_WAIT
# ifdef _WIN32
            if (size > 8)
# else
            if (size != sizeof(int))
# endif
#endif
                inc();
        }

        // 离开等待池
        void leave(xyu::size_t size) noexcept
        {
            // 直接可等待则不进入等待池
#ifdef XY_HAVE_PLATFORM_WAIT
# ifdef _WIN32
            if (size > 8)
# else
            if (size != sizeof(int))
# endif
#endif
                dec();
        }

        // 唤醒
        template <bool all>
        void notify(xyu::size_t size, void* p) noexcept
        {
#ifdef XY_HAVE_PLATFORM_WAIT
# ifdef _WIN32
            if (xyu::bit_count_1_total(size) == 1 && size <= 8)
# else
            if (size == sizeof(int))
# endif
                native_notify<all>(p);
            else
#endif
                if (__atomic_load_n(&count, (int)xyu::N_Atomic_Order::Acquire))
            {
#ifdef XY_HAVE_PLATFORM_WAIT
                __atomic_fetch_add(&version, 1, (int)xyu::N_Atomic_Order::Seq_Cst);
                native_notify<true>(&version);
#else
                cv.notify_all();
#endif
            }
        }

        // 等待
        void wait(xyu::size_t size, void* p, const void* vp, bool(*eq)(void*, const void*)) noexcept
        {
#ifdef XY_HAVE_PLATFORM_WAIT
# ifdef _WIN32
        if (xyu::bit_count_1_total(size) == 1 && size <= 8) native_wait(size, p, vp);
# else
        if (size == sizeof(swait_t))
        {
            int val;
            xyu::mem_copy((void*)&val, vp, size);
            native_wait(p, val);
        }
# endif
        else
        {
            for(;;)
            {
                swait_t old = __atomic_load_n(&version, (int)xyu::N_Atomic_Order::Acquire);
                if (!eq(p, vp)) break;
# ifdef _WIN32
                native_wait(sizeof(swait_t), &version, &old);
# else
                native_wait(&version, old);
# endif
            }
        }
#else
            auto guard = m.guard();
            cv.wait(guard, [&]{ return !eq(p, vp); });
#endif
        }

    private:
        [[noreturn]] XY_NOINLINE XY_COLD
        static void out_limit_error() noexcept
        {
            xyloge("{}: maximum wait on atomic count exceeded", "xyu::Atomic::wait");
            xyu::abort();
        }

        // 增加等待者
        void inc() noexcept
        {
            auto old = __atomic_fetch_add(&count, 1u, (int)xyu::N_Atomic_Order::Seq_Cst);
            if constexpr (!XY_RELEASE) if (XY_UNLIKELY(old == xyu::t_number<swait_t>::max)) out_limit_error();
        }

        // 减少等待着
        void dec() noexcept { __atomic_fetch_sub(&count, 1u, (int)xyu::N_Atomic_Order::Release); }
    };

    /// 唤醒
    template <bool all>
    void notify_help(xyu::size_t size, void* p) noexcept { wait_pool::get(size, p).notify<all>(size, p); }

    void notify_one_help(xyu::size_t size, void* p) noexcept { notify_help<false>(size, p); }
    void notify_all_help(xyu::size_t size, void* p) noexcept { notify_help<true>(size, p); }

    /// 等待
    void wait_enter(xyu::size_t size, void* p) noexcept { wait_pool::get(size, p).enter(size); }
    void wait_leave(xyu::size_t size, void* p) noexcept { wait_pool::get(size, p).leave(size); }

    void wait_help(xyu::size_t size, void* p, const void* vp, bool(*eq)(void*, const void*)) noexcept
    {
        // 自旋次数
        constexpr int spin_count_relax = 12;
        constexpr int spin_count = 16;
        // 自旋
        for (int i = 0; i < spin_count; ++i)
        {
            if (i < spin_count_relax) thread_relax();
            else thread_yield();
            if (!eq(p, vp)) return;
        }
        // 睡眠等待
        wait_pool::get(size, p).wait(size, p, vp, eq);
    }
}
