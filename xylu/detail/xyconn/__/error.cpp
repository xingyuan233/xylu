#include "../socket.h"
#include "../smulio.h"
#include "./wsainit.h"

#if !XY_OUTPUT_ERROR_LOG
#  undef xylog
#  define xylog(...) 0
#endif

namespace xylu::xyconn::__
{
    void abort_wsa_unsupport(int e) noexcept
    {
        if (e) xylogf("xyu::Error: WSAStartup failed with error {}", e);
        else
        {
            xylogf("xyu::Error: WSA unsupport version 2.2");
#ifdef _WIN32
            WSACleanup();
#endif
        }
        xyu::abort();
    }

    void abort_unsupport_accept() noexcept
    {
        xyloge("xyu::Error: WAS do not support AcceptEx");
        xyu::abort();
    }
}
