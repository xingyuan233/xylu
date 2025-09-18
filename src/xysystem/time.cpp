#include <time.h>
#include <errno.h>
#include "../../head/xysystem/time.h"
#include "../../link/log"

namespace
{
    [[noreturn]] void gettime_error(xyu::uint line, const char* func, int err)
    {
        switch (err)
        {
            case ENOTSUP:
                xyloge2(0, "E_Time_Invalid_Argument: unsupported clock kind", line, func);
                throw xyu::E_Logic_Invalid_Argument{};
            default:
                xyloge2(0, "Error: unknown error occured while getting time with code {}", line, func, err);
                throw xyu::Error{};
        }
    }
}

namespace xylu::xysystem::__
{
    xyu::int64 duration_utc()
    {
        timespec tp;
        if (XY_UNLIKELY(clock_gettime(CLOCK_REALTIME, &tp))) gettime_error(__LINE__, __func__, errno);
        return tp.tv_sec * 1000000000 + tp.tv_nsec;
    }

    xyu::int64 duration_any()
    {
        timespec tp;
        if (XY_UNLIKELY(clock_gettime(CLOCK_MONOTONIC, &tp))) gettime_error(__LINE__, __func__, errno);
        return tp.tv_sec * 1000000000 + tp.tv_nsec;
    }

    xyu::int64 duration_process()
    {
        timespec tp;
        if (XY_UNLIKELY(clock_gettime(CLOCK_PROCESS_CPUTIME_ID, &tp))) gettime_error(__LINE__, __func__, errno);
        return tp.tv_sec * 1000000000 + tp.tv_nsec;
    }

    xyu::int64 duration_thread()
    {
        timespec tp;
        if (XY_UNLIKELY(clock_gettime(CLOCK_THREAD_CPUTIME_ID, &tp))) gettime_error(__LINE__, __func__, errno);
        return tp.tv_sec * 1000000000 + tp.tv_nsec;
    }
}

namespace xylu::xysystem
{
    void Clock::sleep_help(xyu::uint32 s, xyu::uint32 ns)
    {
        timespec t{s, static_cast<long>(ns)};
        while (XY_UNLIKELY(nanosleep(&t, &t) == -1))
        {
            if (XY_UNLIKELY(errno == EINTR)) continue;
            xyloge2(0, "Error: unknown error occured while sleeping with code {}", __LINE__, __func__, errno);
            throw xyu::Error{};
        }
    }
}
