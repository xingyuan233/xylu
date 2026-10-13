#include <cerrno>
#include <ctime>

#include "../time.h"
#include "../../xystring/fmt.h"

#ifdef _WIN32
# define WIN32_LEAN_AND_MEAN
# include <windows.h>
# undef ERROR
# undef PASCAL
#endif

/// 时间获取
namespace xylu::xysystem::__
{
#ifdef _WIN32
    static xyu::uint64 filetime_to_100ns(const FILETIME& ft)
    {
        return (static_cast<xyu::uint64>(ft.dwHighDateTime) << 32) | ft.dwLowDateTime;
    }
#endif

    xyu::int64 duration_utc() noexcept
    {
#ifdef _WIN32
        FILETIME ft;
        GetSystemTimeAsFileTime(&ft);
        // 从 1601-01-01 到 1970-01-01 之间的 100ns 数
        constexpr xyu::uint64 UNIX_EPOCH_OFFSET = 116444736000000000ull;
        xyu::uint64 ns100_since_1601 = filetime_to_100ns(ft);
        xyu::int64 ns100_since_1970 = ns100_since_1601 - UNIX_EPOCH_OFFSET;
        return ns100_since_1970 * 10;
#else
        timespec tp;
        if (XY_UNLIKELY(clock_gettime(CLOCK_REALTIME, &tp))) xyu::abort();
        return tp.tv_sec * 100'000'000LL + tp.tv_nsec / 10;
#endif
    }

    xyu::int64 duration_any() noexcept
    {
#ifdef _WIN32
        static const auto freq = []() {
            LARGE_INTEGER frequency;
            if (XY_UNLIKELY(!QueryPerformanceFrequency(&frequency))) xyu::abort();
            return frequency.QuadPart;
        }();
        LARGE_INTEGER counter;
        if (XY_UNLIKELY(!QueryPerformanceCounter(&counter))) xyu::abort();
        const xyu::int64 cnt = counter.QuadPart;
        const xyu::int64 sec_part = cnt / freq;
        const xyu::int64 ten_ns_part = (cnt % freq) * 100'000'000LL / freq;
        return sec_part * 100'000'000LL + ten_ns_part;
#else
        timespec tp;
        if (XY_UNLIKELY(clock_gettime(CLOCK_MONOTONIC, &tp))) xyu::abort();
        return tp.tv_sec * 100'000'000LL + tp.tv_nsec / 10;
#endif
    }

    xyu::int64 duration_process() noexcept
    {
#ifdef _WIN32
        FILETIME creation, exit, kernel, user;
        HANDLE hProcess = GetCurrentProcess();
        if (XY_UNLIKELY(!GetProcessTimes(hProcess, &creation, &exit, &kernel, &user))) xyu::abort();
        // 进程 CPU 时间 = 内核时间 + 用户时间
        return (filetime_to_100ns(kernel) + filetime_to_100ns(user)) * 10;
#else
        timespec tp;
        if (XY_UNLIKELY(clock_gettime(CLOCK_PROCESS_CPUTIME_ID, &tp))) xyu::abort();
        return tp.tv_sec * 100'000'000LL + tp.tv_nsec / 10;
#endif
    }

    xyu::int64 duration_thread() noexcept
    {
#ifdef _WIN32
        FILETIME creation, exit, kernel, user;
        HANDLE hThread = GetCurrentThread();
        if (XY_UNLIKELY(!GetThreadTimes(hThread, &creation, &exit, &kernel, &user))) xyu::abort();
        // 线程 CPU 时间 = 内核时间 + 用户时间
        return (filetime_to_100ns(kernel) + filetime_to_100ns(user)) * 10;
#else
        timespec tp;
        if (XY_UNLIKELY(clock_gettime(CLOCK_THREAD_CPUTIME_ID, &tp))) xyu::abort();
        return tp.tv_sec * 100'000'000LL + tp.tv_nsec / 10;
#endif
    }
}

/// sleep
namespace xylu::xysystem::__
{
#ifndef _WIN32
    [[noreturn]] XY_NOINLINE XY_COLD
    static void abort_sleep_error() noexcept
    {
        xyu::E_Logic::abort_unknown_error("xyu::E_Sleep", errno);
    }
#endif

    void sleep_help(xyu::uint32 s, xyu::uint32 ns) noexcept
    {
#ifdef _WIN32
        // Windows Sleep 精度通常约 1-16 毫秒，不能精确到纳秒
        xyu::uint64 ms = static_cast<xyu::uint64>(s) * 1000ull + (ns + 999999ull) / 1000000ull;
        // 分片休眠
        constexpr xyu::uint32 max_sleep = 0xFFFFFFFE; // 避免 0xFFFFFFFF (INFINITE) 造成语义混淆
        while(XY_UNLIKELY(ms >= max_sleep)) { Sleep(max_sleep); ms -= max_sleep; }
        Sleep(ms);
#else
        timespec t;
        t.tv_sec = s;
        t.tv_nsec = ns;
        while (XY_UNLIKELY(nanosleep(&t, &t) == -1))
        {
            if (errno == EINTR) continue;
            abort_sleep_error();
        }
#endif
    }
}
