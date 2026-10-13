#pragma once

/**
 * @file    xyconn/__/wsainit.h
 * @brief   windows 网络初始化
 * @date    2026-08-23
 */

#ifdef _WIN32
#  include <winsock2.h>
#endif

/// windows 预处理
namespace xylu::xyconn
{
#ifdef _WIN32
    namespace __
    {
        [[noreturn]] XY_NOINLINE XY_COLD
        void abort_wsa_unsupport(int e) noexcept;
    }

    inline void wsa_init() noexcept
    {
        static struct WSA
        {
            WSA() noexcept
            {
                // 初始化
                WSADATA wsaData;
                int ret = WSAStartup(MAKEWORD(2, 2), &wsaData);
                if (XY_UNLIKELY(ret != 0)) __::abort_wsa_unsupport(ret);
                // 检查版本
                if (LOBYTE(wsaData.wVersion) != 2 || HIBYTE(wsaData.wVersion) != 2) __::abort_wsa_unsupport(0);
            }
            ~WSA() noexcept { WSACleanup(); }
        } wsa;
        (void)wsa;
    }
#else
    inline void wsa_init() noexcept {}
#endif
}
