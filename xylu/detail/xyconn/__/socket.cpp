#include "../socket.h"

#include "./wsainit.h"

#ifdef _WIN32
#  include <winsock2.h>
#  include <ws2tcpip.h>
#else
#  include <sys/socket.h>
#  include <netinet/in.h>
#  include <unistd.h>
#  include <cerrno>
#  include <netinet/tcp.h>
#  include <fcntl.h>
#  include <poll.h>
#endif

#ifdef _WIN32
#  ifndef TCP_KEEPALIVE
#    define TCP_KEEPALIVE 3
#  endif
#  ifndef TCP_KEEPIDLE
#    define TCP_KEEPIDLE TCP_KEEPALIVE
#  endif
#  ifndef TCP_KEEPINTVL
#    define TCP_KEEPINTVL 17
#  endif
#  ifndef TCP_KEEPCNT
#    define TCP_KEEPCNT 16
#  endif
#endif

/// 跨平台转换工具
// 原始套接字，0为有效句柄，-1为无效
// +1偏移处理，0变1非空，-1变0空指针
namespace
{
#ifdef _WIN32
    static_assert(SOCKET_ERROR == -1);
    static_assert(INVALID_SOCKET == -1);

    using XY_SOCKET_TYPE = SOCKET;
    using XY_ADDR_LEN = int;

    void* skr(SOCKET sock) noexcept { return (void*)(uintptr_t)(sock + 1); }
    SOCKET sk(void* h) noexcept { return (SOCKET)((uintptr_t)h - 1); }

    #define XY_ERROR_CODE WSAGetLastError()
    #define XY_NOMEM(e) XY_UNLIKELY(e == WSAENOBUFS)
    #define XY_EINTR(e) XY_UNLIKELY(e == WSAEINTR)
    #define XY_EAGAIN(e) (e == WSAEWOULDBLOCK)
    #define XY_ETIMEDOUT(e) (e == WSAETIMEDOUT)
    #define XY_ECONNABORTED(e) (e == WSAECONNABORTED)

#else
    using XY_SOCKET_TYPE = int;
    using XY_ADDR_LEN = socklen_t;

    void* skr(int sock) noexcept { return (void*)(uintptr_t)(sock + 1); }
    int sk(void* h) noexcept { return (int)((uintptr_t)h - 1); }

    #define XY_ERROR_CODE errno
    #define XY_NOMEM(e) XY_UNLIKELY(e == ENOBUFS || e == ENOMEM)
    #define XY_EINTR(e) XY_UNLIKELY(e == EINTR)
    #define XY_EAGAIN(e) (e == EAGAIN)
    #define XY_ETIMEDOUT(e) (e == ETIMEDOUT)
    #define XY_ECONNABORTED(e) (e == ECONNABORTED)
#endif

    XY_NOINLINE XY_COLD
    int parse_error(int err) noexcept
    {
        switch (err)
        {
#ifdef _WIN32
        case WSAEACCES:
#else
        case EPERM:
        case EACCES:
#endif
            return xyu::ES_Socket::No_Permission;

#ifdef _WIN32
        case WSAENOBUFS:
#else
        case ENOBUFS:
        case ENOMEM:
#endif
            return xyu::ES_Socket::No_Memory;

#ifdef _WIN32
        case WSAEMFILE:
#else
        case EMFILE:
#endif
            return xyu::ES_Socket::Process_Limit;

#ifndef _WIN32
        case ENFILE:
            return xyu::ES_Socket::System_Limit;
#endif

#ifdef _WIN32
        case WSAENETDOWN:
#else
        case ENETDOWN:
#endif
            return xyu::ES_Socket::Net_SubSystem_Failed;

#ifdef _WIN32
        case WSAEADDRINUSE:
#else
        case EADDRINUSE:
#endif
            return xyu::ES_Socket::Local_Address_Occupied;

#ifdef _WIN32
        case WSAEADDRNOTAVAIL:
#else
        case EADDRNOTAVAIL:
#endif
            return xyu::ES_Socket::Local_Address_Not_Found;

#ifdef _WIN32
        case WSAENETUNREACH:
#else
        case ENETUNREACH:
#endif
            return xyu::ES_Socket::Net_Unreachable;

#ifdef _WIN32
        case WSAEHOSTUNREACH:
#else
        case EHOSTUNREACH:
#endif
            return xyu::ES_Socket::Host_Unreachable;

#ifdef _WIN32
        case WSAECONNREFUSED:
#else
        case ECONNREFUSED:
#endif
            return xyu::ES_Socket::Connection_Refused;

#ifdef _WIN32
        case WSAETIMEDOUT:
#else
        case ETIMEDOUT:
#endif
            return xyu::ES_Socket::Connection_Timeout;

#ifdef _WIN32
        case WSAECONNABORTED:
        case WSAECONNRESET:
#else
        case ECONNABORTED:
        case ECONNRESET:
        case EPIPE:
#endif
            return xyu::ES_Socket::Connection_Over;

#ifdef _WIN32
        case WSAEMSGSIZE:
#else
        case EMSGSIZE:
#endif
            return xyu::ES_Socket::Datagram_Error;

        default: return -1;
        }
    }
}

/// windows 预处理
namespace xylu::xyconn
{
    Socket::Socket() noexcept : h{} { wsa_init(); }
}

/// 创建
namespace xylu::xyconn
{
    XY_NOINLINE XY_COLD
    static int public_error(const char* func, int err, bool log) noexcept
    {
        switch (err)
        {
#ifdef _WIN32
        case WSANOTINITIALISED:
            xyloge("{}: has not called WSAStartup, or WSAStartup failed", func);
            xyu::abort();
        case WSAENETDOWN:
            if (log) xylogf("{}: network subsystem is failed", func);
            return xyu::ES_Socket::Net_SubSystem_Failed;
        case WSAENOTSOCK:
            xyloge("{}: socket handle is invalid", func);
            xyu::abort();
        case WSAENOTCONN:
            xyloge("{}: socket is not connected", func);
            xyu::abort();
        case WSAEISCONN:
            xyloge("{}: socket is already connected", func);
            xyu::abort();
#else
        case EBADF:
            xyloge("{}: socket descriptor is invalid", func);
            xyu::abort();
        case ENETDOWN:
            if (log) xylogf("{}: network subsystem is failed", func);
            return xyu::ES_Socket::Net_SubSystem_Failed;
        case ENOTSOCK:
            xyloge("{}: descriptor is not socket", func);
            xyu::abort();
        case ENOTCONN:
            xyloge("{}: socket is not connected", func);
            xyu::abort();
        case EISCONN:
            xyloge("{}: socket is already connected", func);
            xyu::abort();
#endif
        default:
            xyu::E_Logic::abort_unknown_error(func, err);
        }
    }

    XY_NOINLINE XY_COLD
    static int create_error(int err, bool log, int af, int sk, int pf) noexcept
    {
        const char* func = "xyu::Socket::create";
        switch (err)
        {
#ifdef _WIN32
        case WSAEACCES:
#else
        case EPERM:
        case EACCES:
#endif
            err = xyu::ES_Socket::No_Permission;
            if (log) xyloge("{}: no permission to create socket", func);
            break;

#ifdef _WIN32
        case WSAENOBUFS:
#else
        case ENOBUFS:
        case ENOMEM:
#endif
            err = xyu::ES_Socket::No_Memory;
            if (log) xylogf("{}: no memory to create socket", func);
            break;

#ifdef _WIN32
        case WSAEMFILE:
#else
        case EMFILE:
#endif
            err = xyu::ES_Socket::Process_Limit;
            if (log) xylogf("{}: too many active sockets in process", func);
            break;

#if !defined _WIN32
        case ENFILE:
            err = xyu::ES_Socket::System_Limit;
            if (log) xylogf("{}: too many open sockets in system", func);
            break;
#endif

#ifdef _WIN32
        case WSAEINVAL:
#else
        case EINVAL:
#endif
            xyloge("{}: invalid argument({},{},{}) to create socket", func, af, sk, pf);
            xyu::abort();

#ifdef _WIN32
        case WSAESOCKTNOSUPPORT:
#else
        case ESOCKTNOSUPPORT:
#endif
            xyloge("{}: socket type {} is not supported", func, sk);
            xyu::abort();

#ifdef _WIN32
        case WSAEPROTONOSUPPORT:
#else
        case EPROTONOSUPPORT:
#endif
            xyloge("{}: protocol {} is not supported", func, pf);
            xyu::abort();

#ifdef _WIN32
        case WSAEAFNOSUPPORT:
#else
        case EAFNOSUPPORT:
#endif
            xyloge("{}: address family {} not supported with protocol {}", func, af, pf);
            xyu::abort();

#ifdef _WIN32
        case WSAEPROTOTYPE:
#else
        case EPROTOTYPE:
#endif
            xyloge("{}: socket type {} not supported with protocol {}", func, sk, pf);
            xyu::abort();

        default:
            return public_error(func, err, log);
        }
        return err;
    }

    [[noreturn]] XY_NOINLINE XY_COLD
    static void abort_is_created(const char* func) noexcept
    {
        xyloge("{}: socket is already created", func);
        xyu::abort();
    }

    static void create_impl(void*& h, int af0, int sk0, int pf, bool throws, xyu::ES_Socket& state)
    {
        int af = af0;
        if constexpr (Socket::AF_unspec != AF_UNSPEC) { if (af0 == Socket::AF_unspec) af = AF_UNSPEC; }
        if constexpr (Socket::AF_local != AF_UNIX) { if (af0 == Socket::AF_local) af = AF_UNIX; }
        if constexpr (Socket::AF_ipv4 != AF_INET) { if (af0 == Socket::AF_ipv4) af = AF_INET; }
        if constexpr (Socket::AF_ipv6 != AF_INET6) { if (af0 == Socket::AF_ipv6) af = AF_INET6; }

        bool noblock = false;
        if (sk0 & Socket::Sk_noblock) noblock = true, sk0 &= ~Socket::Sk_noblock;

        int sk = sk0;
        if constexpr (Socket::Sk_stream != (int)SOCK_STREAM) { if (sk0 == Socket::Sk_stream) sk = SOCK_STREAM; }
        if constexpr (Socket::Sk_dgram != (int)SOCK_DGRAM) { if (sk0 == Socket::Sk_dgram) sk = SOCK_DGRAM; }
        if constexpr (Socket::Sk_raw != (int)SOCK_RAW) { if (sk0 == Socket::Sk_raw) sk = SOCK_RAW; }

        if constexpr (XY_DEBUG) if (XY_UNLIKELY(h)) abort_is_created("xyu::Socket::create");

        // 创建套接字
        XY_SOCKET_TYPE sock;
#ifdef __linux__
        if (noblock) sk |= SOCK_NONBLOCK;
#endif
        if (XY_UNLIKELY((sock = socket(af, sk, pf)) == -1))
        {
            int e = XY_ERROR_CODE;
            state = create_error( e, throws, af, sk, pf);
            if (throws) throw xyu::E_Socket{state.code};
            return;
        }

    create_success:
        // 非阻塞设置
#ifndef __linux__
        if (noblock)
        {
#ifdef _WIN32
            u_long mode = 1;
            if (XY_UNLIKELY(ioctlsocket(sock, FIONBIO, &mode) == -1))
#else
            int flags = fcntl(sock, F_GETFL, 0);
            if (XY_UNLIKELY(flags == -1 || fcntl(sock, F_SETFL, flags | O_NONBLOCK) == -1))
#endif
                public_error("xyu::Socket::create", XY_ERROR_CODE, true), xyu::abort();
        }
#endif
        h = skr(sock);
#ifndef _WIN32
        // 解除 TIME_WAIT
        int enable = 1;
        if (XY_UNLIKELY(::setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, &enable, sizeof(enable)) == -1))
            public_error("xyu::Socket::create", XY_ERROR_CODE, true), xyu::abort();
#endif
        state = xyu::ES_Socket::Success;
    }

    void Socket::create(_AddressFamily af, _SocketType sk, int pf)
    { xyu::ES_Socket state; create_impl(h, af, sk, pf, true, state); }

    void Socket::create(_AddressFamily af, _SocketType sk, int pf, xyu::ES_Socket& state) noexcept
    { create_impl(h, af, sk, pf, false, state); }
}

/// 销毁
namespace xylu::xyconn
{
    static void destroy_impl(void*& h, xyu::ES_Socket& state, bool throws)
    {
        state = xyu::ES_Socket::Success;
        if (!h) return;
        void* tmp = h;
        h = nullptr;

#ifdef _WIN32
        while (XY_UNLIKELY(closesocket(sk(tmp)) == SOCKET_ERROR))
        {
            int e = WSAGetLastError();
            if (XY_UNLIKELY(e == WSAEINPROGRESS || e == WSAEINTR)) continue;
            if (e == WSAEWOULDBLOCK)
            {
                linger l; l.l_onoff = true; l.l_linger = 0;
                setsockopt(sk(tmp), SOL_SOCKET, SO_LINGER, (char*)&l, sizeof(l));
                continue;
            }
            state = public_error("xyu::Socket::destroy", e, throws);
            if (throws) throw xyu::E_Socket{state.code};
        }
#elif defined __hpux
        while (XY_UNLIKELY(::close(sk(tmp)) == -1))
        {
            if (XY_UNLIKELY(errno == EINTR)) continue;
            state = public_error("xyu::Socket::destroy", errno, throws);
            if (throws) throw xyu::E_Socket{state.code};
        }
#else
        if (XY_UNLIKELY(::close(sk(tmp)) == -1) && errno != EINTR)
        {
            state = public_error("xyu::Socket::destroy", errno, throws);
            if (throws) throw xyu::E_Socket{state.code};
        }
#endif
    }

    void Socket::destroy() { xyu::ES_Socket state; destroy_impl(h, state, true); }

    void Socket::destroy(xyu::ES_Socket& state) noexcept { destroy_impl(h, state, false); }
}

/// 绑定
namespace xylu::xyconn
{
    XY_NOINLINE XY_COLD
    static int bind_error(int err, bool log, const Socket::Address* addr)
    {
        const char* func = "xyu::Socket::bind";
        switch (err)
        {
#ifdef _WIN32
        case WSAEADDRINUSE:
#else
        case EADDRINUSE:
#endif
            err = xyu::ES_Socket::Local_Address_Occupied;
            if (log)
            {
                if (addr->family == AF_INET)
                    xyloge("{}: port {} is occupied", func, ((Socket::AddressIPv4*)addr)->port);
                else if (addr->family == AF_INET6)
                    xyloge("{}: port {} is occupied", func, ((Socket::AddressIPv6*)addr)->port);
                else xyloge("{}: address is already in use", func);
            }
            break;

#ifdef _WIN32
        case WSAEADDRNOTAVAIL:
#else
        case EADDRNOTAVAIL:
#endif
            err = xyu::ES_Socket::Local_Address_Not_Found;
            if (log)
            {
                if (addr->family == AF_INET)
                    xyloge("{}: cannot find local address {}", func, *(Socket::AddressIPv4*)addr);
                else if (addr->family == AF_INET6)
                    xyloge("{}: cannot find local address {}", func, *(Socket::AddressIPv6*)addr);
                else xyloge("{}: cannot find local address", func);
            }
            break;

#ifdef _WIN32
        case WSAEACCES:
#else
        case EACCES:
#endif
            err = xyu::ES_Socket::No_Permission;
            if (log)
            {
                if (addr->family == AF_INET)
                    xyloge("{}: no permission to bind port {}", func, ((Socket::AddressIPv4*)addr)->port);
                else if (addr->family == AF_INET6)
                    xyloge("{}: no permission to bind port {}", func, ((Socket::AddressIPv6*)addr)->port);
                else xyloge("{}: no permission to bind port", func);
            }
            break;

#ifdef _WIN32
        case WSAEINVAL:
            xyloge("{}: socket is already bound to an address", func);
            xyu::abort();
        case WSAEFAULT:
            xyloge("{}: invalid address or length", func);
            xyu::abort();
#else
        case EINVAL:
            xyloge("{}: socket is already bound, or addrlen is wrong", func);
            xyu::abort();
        case EROFS:
            xyloge("{}: cannot bind local address to ROM", func);
            xyu::abort();
#endif
        default:
            return public_error(func, err, log);
        }
        return err;
    }

    [[noreturn]] XY_NOINLINE XY_COLD
    static void abort_not_create(const char* func) noexcept
    {
        xyloge("{}: socket is not create or already destroyed", func);
        xyu::abort();
    }

    static void bind_impl(void* h, const Socket::Address* addr, xyu::uint size, xyu::ES_Socket& state, bool throws)
    {
        if constexpr (XY_DEBUG)
        {
            if (XY_UNLIKELY(!addr))
                xyu::E_Logic::abort_null_pointer(XY_CurPos(nullptr,"xyu::Socket::bind"), "address");
            if (XY_UNLIKELY(size > xyu::t_number<XY_ADDR_LEN>::max))
                xyu::E_Logic::abort_over_limit(XY_CurPos(nullptr,"xyu::Socket::bind"), size, 0, xyu::t_number<XY_ADDR_LEN>::max, "length");
            if (XY_UNLIKELY(!h)) abort_not_create("xyu::Socket::bind");
        }

        if (XY_UNLIKELY(::bind(sk(h), (const ::sockaddr*)addr, size) == -1))
        {
            state = bind_error(XY_ERROR_CODE, throws, addr);
            if (throws) throw xyu::E_Socket{state.code};
        }
        else state = xyu::ES_Socket::Success;
    }

    void Socket::bind(const Address* addr, xyu::uint size)
    { xyu::ES_Socket state; bind_impl(h, addr, size, state, true); }

    void Socket::bind(const Address* addr, xyu::uint size, xyu::ES_Socket& state) noexcept
    { bind_impl(h, addr, size, state, false); }
}

/// 监听
namespace xylu::xyconn
{
    XY_NOINLINE XY_COLD
    static int listen_error(int err, bool log)
    {
        const char* func = "xyu::Socket::listen";
        switch (err)
        {
#ifdef _WIN32
        case WSAEADDRINUSE:
#else
        case EADDRINUSE:
#endif
            err = xyu::ES_Socket::Local_Address_Occupied;
            if (log) xyloge("{}: address is already listening", func);
            break;

#ifdef _WIN32
        case WSAENOBUFS:
#else
        case ENOBUFS:
        case ENOMEM:
#endif
            err = xyu::ES_Socket::No_Memory;
            if (log) xylogf("{}: no memory to cerate listening queue", func);
            break;

#ifdef _WIN32
        case WSAEOPNOTSUPP:
#else
        case EOPNOTSUPP:
#endif
            xyloge("{}: socket type do not support connect", func);
            xyu::abort();

#ifdef _WIN32
        case WSAEINVAL:
#else
        case EINVAL:
#endif
            xyloge("{}: socket is not bound to a local address", func);
            xyu::abort();

        default:
            return public_error(func, err, log);
        }
        return err;
    }

    static void listen_impl(void* h, xyu::uint backlog, bool throws, xyu::ES_Socket& state)
    {
        if constexpr (XY_DEBUG)
        {
            if (XY_UNLIKELY(backlog > xyu::t_number<int>::max))
                xyu::E_Logic::abort_over_limit(XY_CurPos(nullptr,"xyu::Socket::listen"), backlog, 0, xyu::t_number<int>::max, "backlog");
            if (XY_UNLIKELY(!h)) abort_not_create("xyu::Socket::listen");
        }
        state = xyu::ES_Socket::Success;

        if (backlog == 0) backlog = SOMAXCONN;
        if (XY_UNLIKELY(::listen(sk(h), backlog) == -1))
        {
            int e = XY_ERROR_CODE;
            state = listen_error(e, throws);
            if (throws) throw xyu::E_Socket{state.code};
        }
    }

    void Socket::listen(xyu::uint backlog)
    { xyu::ES_Socket state; listen_impl(h, backlog, true, state); }

    void Socket::listen(xyu::uint backlog, xyu::ES_Socket& state) noexcept
    { listen_impl(h, backlog, false, state); }
}

/// 接受
namespace xylu::xyconn
{
    XY_NOINLINE XY_COLD
    static int accept_error(int err, bool log)
    {
        const char* func = "xyu::Socket::accept";
        switch (err)
        {
#ifdef _WIN32
        case WSAEMFILE:
#else
        case EMFILE:
#endif
            err = xyu::ES_Socket::Process_Limit;
            if (log) xylogf("{}: too many active sockets in process", func);
            break;

#if !defined _WIN32
        case ENFILE:
            err = xyu::ES_Socket::System_Limit;
            if (log) xylogf("{}: too many open sockets in system", func);
            break;
#endif

#ifdef _WIN32
        case WSAENOBUFS:
#else
        case ENOBUFS:
        case ENOMEM:
#endif
            err = xyu::ES_Socket::No_Memory;
            if (log) xylogf("{}: no memory to accept socket", func);
            break;

#ifdef _WIN32
        case WSAEINVAL:
#else
        case EINVAL:
#endif
            xyloge("{}: socket has not listened", func);
            xyu::abort();

#ifdef _WIN32
        case WSAEOPNOTSUPP:
#else
        case EOPNOTSUPP:
#endif
            xyloge("{}: socket type do not support connect", func);
            xyu::abort();

        default:
            return public_error(func, err, log);
        }
        return err;
    }

    static void* accept_impl(void* h, Socket::Address* addr, xyu::uint* size, xyu::ES_Socket& state, bool block, bool throws)
    {
        if constexpr (XY_DEBUG)
        {
            if (size && XY_UNLIKELY(*size > xyu::t_number<XY_ADDR_LEN>::max))
                xyu::E_Logic::abort_over_limit(XY_CurPos(nullptr,"xyu::Socket::accept"), *size, 0, xyu::t_number<XY_ADDR_LEN>::max, "length");
            if (XY_UNLIKELY(!h)) abort_not_create("xyu::Socket::accept");
        }
        state = xyu::ES_Socket::Success;

        // 接受连接
        XY_SOCKET_TYPE sock;
        XY_ADDR_LEN s = size ? *size : 0;
        for (;;)
        {
#ifdef __linux__
            sock = ::accept4(sk(h), (sockaddr*)addr, size ? &s : nullptr, block ? 0 : SOCK_NONBLOCK);
#else
            sock = ::accept(sk(h), (sockaddr*)addr, size ? &s : nullptr);
#endif
            if (XY_LIKELY(sock != -1)) break;

            int e = XY_ERROR_CODE;
            if (XY_EAGAIN(e)) return {};
            if (XY_EINTR(e) || XY_ECONNABORTED(e)) continue;
#ifdef _WIN32
            if (e == WSAETIMEDOUT) continue;
#endif
            state = accept_error(e, throws);
            if (throws) throw xyu::E_Socket{state.code};
            return {};
        }

#ifndef __linux__
        // 非阻塞设置
        if (!block)
        {
#ifdef _WIN32
            u_long mode = 1;
            if (XY_UNLIKELY(ioctlsocket(sock, FIONBIO, &mode) == -1))
#else
            int flags = fcntl(sock, F_GETFL, 0);
            if (XY_UNLIKELY(flags == -1 || fcntl(sock, F_SETFL, flags | O_NONBLOCK) == -1))
#endif
                public_error("xyu::Socket::accept", XY_ERROR_CODE, true), xyu::abort();
        }
#endif

        if (size) *size = s;
        return skr(sock);
    }

    Socket Socket::accept(Address* addr, xyu::uint* size, bool block)
    {
        xyu::ES_Socket state;
        Socket sock;
        sock.h = accept_impl(h, addr, size, state, block, true);
        return sock;
    }

    Socket Socket::accept(Address* addr, xyu::uint* size, bool block, xyu::ES_Socket& state) noexcept
    {
        Socket sock;
        sock.h = accept_impl(h, addr, size, state, block, false);
        return sock;
    }
}

/// 连接
namespace xylu::xyconn
{
    XY_NOINLINE XY_COLD
    static int connect_error(int err, bool log)
    {
        const char* func = "xyu::Socket::connect";
        switch (err)
        {
#ifdef _WIN32
        case WSAENETUNREACH:
#else
        case ENETUNREACH:
#endif
            err = xyu::ES_Socket::Net_Unreachable;
            if (log) xyloge("{}: net unreachable", func);
            break;

#ifdef _WIN32
        case WSAEHOSTUNREACH:
#else
        case EHOSTUNREACH:
#endif
            err = xyu::ES_Socket::Host_Unreachable;
            if (log) xyloge("{}: host unreachable", func);
            break;

#ifdef _WIN32
        case WSAECONNREFUSED:
#else
        case ECONNREFUSED:
#endif
            err = xyu::ES_Socket::Connection_Refused;
            if (log) xyloge("{}: connection refused", func);
            break;

#ifdef _WIN32
        case WSAETIMEDOUT:
#else
        case ETIMEDOUT:
#endif
            err = xyu::ES_Socket::Connection_Timeout;
            if (log) xyloge("{}: connection timed out", func);
            break;

#ifdef _WIN32
        case WSAECONNABORTED:
#else
        case ECONNABORTED:
#endif
            err = xyu::ES_Socket::Connection_Over;
            if (log) xyloge("{}: connection closed by system", func);
            break;

#ifdef _WIN32
        case WSAEADDRNOTAVAIL:
#else
        case EADDRNOTAVAIL:
#endif
            err = xyu::ES_Socket::Local_Address_Occupied;
            if (log) xylogf("{}: no available local ports to bind", func);
            break;

#ifdef _WIN32
        case WSAEACCES:
#else
        case EACCES:
#endif
            err = xyu::ES_Socket::No_Permission;
            if (log) xyloge("{}: no permission to bind special port", func);
            break;

#ifdef _WIN32
        case WSAEINVAL:
#else
        case EAFNOSUPPORT:
#endif
            xyloge("{}: invalid address to connect", func);
            xyu::abort();

#ifndef _WIN32
        case ENOBUFS:
        case ENOMEM:
#endif
            err = xyu::ES_Socket::No_Memory;
            if (log) xylogf("{}: no memory to alloc sockbit to wait connect finish", func);
            break;

        default:
            return public_error(func, err, log);
        }
        return err;
    }

    static void connect_impl(void* h, Socket& sock, const Socket::Address* addr, xyu::uint size, xyu::ES_Socket& state, bool throws)
    {
        if constexpr (XY_DEBUG)
        {
            if (XY_UNLIKELY(!addr))
                xyu::E_Logic::abort_null_pointer(XY_CurPos(nullptr,"xyu::Socket::connect"), "address");
            if (XY_UNLIKELY(size > xyu::t_number<XY_ADDR_LEN>::max))
                xyu::E_Logic::abort_over_limit(XY_CurPos(nullptr,"xyu::Socket::connect"), size, 0, xyu::t_number<XY_ADDR_LEN>::max, "length");
            if (XY_UNLIKELY(!h)) abort_not_create("xyu::Socket::connect");
        }
        state = xyu::ES_Socket::Success;

        if (XY_UNLIKELY(::connect(sk(h), (sockaddr*)addr, size) == -1))
        {
            int e = XY_ERROR_CODE;
            // 非阻塞直接返回
#ifdef _WIN32
            if (e == WSAEWOULDBLOCK || e == WSAEALREADY) return;
#else
            if (e == EINPROGRESS || e == EALREADY) return;
#endif
            // posix 被中断手动重试
#ifndef _WIN32
            if (e == EINTR)
            {
                int flags = fcntl(sk(h), F_GETFL, 0);
                if (XY_UNLIKELY(flags == -1)) e = errno;
                if (!(flags & O_NONBLOCK))
                {
                    if (throws) sock.wait(Socket::Wt_send);
                    else return sock.wait(Socket::Wt_send, state);
                }
                return;
            }
#endif
            state = connect_error(e, throws);
            if (throws) throw xyu::E_Socket{state.code};
        }
    }

    void Socket::connect(const Address* addr, xyu::uint size)
    { xyu::ES_Socket state; connect_impl(h, *this, addr, size, state, true); }

    void Socket::connect(const Address* addr, xyu::uint size, xyu::ES_Socket& state) noexcept
    { return connect_impl(h, *this, addr, size, state, false); }
}

/// 断开
namespace xylu::xyconn
{
    XY_NOINLINE XY_COLD
    static int shutdown_error(int err, bool log) noexcept
    {
        const char* func = "xyu::Socket::shutdown";
        switch (err)
        {
#ifdef _WIN32
        case WSAEINVAL:
#else
        case EINVAL:
#endif
            xyloge("{}: invalid argument to shutdown socket", func);
            xyu::abort();
        default:
            return public_error(func, err, log);
        }
    }

    void shutdown_impl(void* h, Socket::_ShutdownType st0, xyu::ES_Socket& state, bool throws)
    {
        if constexpr (XY_DEBUG) if (XY_UNLIKELY(st0 < 0 || st0 > 2))
            xyu::E_Logic::abort_invalid_value(XY_CurPos(nullptr,"xyu::Socket::shutdown"), st0, "socket shutdown type");
        state = xyu::ES_Socket::Success;
        if (!h) return;

        int st = st0;
#ifdef _WIN32
        if constexpr (Socket::Sd_receive != SD_RECEIVE) { if (st0 == Socket::Sd_receive) st = SD_RECEIVE; }
        if constexpr (Socket::Sd_send != SD_SEND) { if (st0 == Socket::Sd_send) st = SD_SEND; }
        if constexpr (Socket::Sd_all != SD_BOTH) { if (st0 == Socket::Sd_all) st = SD_BOTH; }
        while (XY_UNLIKELY(::shutdown(sk(h), st) == -1))
        {
            int e = WSAGetLastError();
            if (e == WSAEINPROGRESS) continue;
            if (e == WSAECONNABORTED || e == WSAECONNRESET) return;
            state = shutdown_error(e, throws);
            if (throws) throw xyu::E_Socket{state.code};
        }
#else
        if constexpr (Socket::Sd_receive != SHUT_RD) { if (st0 == Socket::Sd_receive) st = SHUT_RD; }
        if constexpr (Socket::Sd_send != SHUT_WR) { if (st0 == Socket::Sd_send) st = SHUT_WR; }
        if constexpr (Socket::Sd_all != SHUT_RDWR) { if (st0 == Socket::Sd_all) st = SHUT_RDWR; }
        if (XY_UNLIKELY(::shutdown(sk(h), st) == -1))
        {
            state = shutdown_error(errno, throws);
            if (throws) throw xyu::E_Socket{state.code};
        }
#endif
    }

    void Socket::shutdown(_ShutdownType st)
    { xyu::ES_Socket state; shutdown_impl(h, st, state, true); }

    void Socket::shutdown(_ShutdownType st, xyu::ES_Socket& state) noexcept
    { shutdown_impl(h, st, state, false); }
}

/// 发送
namespace xylu::xyconn
{
    XY_NOINLINE XY_COLD
    static int send_error(const char* func, int err, bool log)
    {
        switch (err)
        {
#ifdef _WIN32
        case WSAENOBUFS:
#else
        case ENOBUFS:
        case ENOMEM:
#endif
            err = xyu::ES_Socket::No_Memory;
            if (log) xylogf("{}: no memory to alloc send buffer", func);
            break;

#ifdef _WIN32
        case WSAECONNRESET:
#else
        case ECONNRESET:
        case EPIPE:
#endif
            err = xyu::ES_Socket::Connection_Over;
            if (log) xyloge("{}: connection closed by remote host", func);
            break;

#ifdef _WIN32
        case WSAECONNABORTED:
#else
        case ECONNABORTED:
#endif
            err = xyu::ES_Socket::Connection_Over;
            if (log) xyloge("{}: connection closed by system", func);
            break;

#ifdef _WIN32
        case WSAEMSGSIZE:
#else
        case EMSGSIZE:
#endif
            err = xyu::ES_Socket::Datagram_Limit;
            if (log) xyloge("{}: datagram size exceeds system limit", func);
            break;

#ifdef _WIN32
        case WSAEACCES:
#else
        case EACCES:
#endif
            err = xyu::ES_Socket::No_Permission;
            if (log) xyloge("{}: no permission to send", func);
            break;

#ifdef _WIN32
        case WSAEINVAL:
#else
        case EINVAL:
#endif
            xyloge("{}: invalid param to send", func);
            xyu::abort();

        default:
            return public_error(func, err, log);
        }
        return err;
    }

    static xyu::size_t send_impl(void* h, const void* mess, xyu::size_t len, xyu::ES_Socket& state, bool throws)
    {
        if constexpr (XY_DEBUG)
        {
            if (XY_UNLIKELY(!mess))
                xyu::E_Logic::abort_null_pointer(XY_CurPos(nullptr,"xyu::Socket::send"), "message");
            if (XY_UNLIKELY(!h)) abort_not_create("xyu::Socket::send");
        }
        state = xyu::ES_Socket::Success;

        xyu::size_t tmp = len, n;
        int flag = 0;
#ifndef _WIN32
        flag = MSG_NOSIGNAL;
#endif
        while (len)
        {
            xyu::size_t one = len;
#ifdef _WIN32
            one = xyu::min(one, xyu::t_number<int>::max);
#endif
            if (XY_UNLIKELY((n = ::send(sk(h), (const char*)mess, one, flag)) == -1))
            {
                int e = XY_ERROR_CODE;
                if (XY_EINTR(e)) continue;
                if (XY_EAGAIN(e) || XY_ETIMEDOUT(e)) break;
                state = send_error("xyu::Socket::send", e, throws);
                if (throws) throw xyu::E_Socket{state.code};
                break;
            }
            if (XY_UNLIKELY(n == 0)) break;
            len -= n;
            mess = (char*)mess + n;
        }
        return tmp - len;
    }

    xyu::size_t Socket::send(const void* mess, xyu::size_t len)
    { xyu::ES_Socket state; return send_impl(h, mess, len, state, true); }

    xyu::size_t Socket::send(const void* mess, xyu::size_t len, xyu::ES_Socket& state) noexcept
    { return send_impl(h, mess, len, state, false); }
}

/// 接收
namespace xylu::xyconn
{
    XY_NOINLINE XY_COLD
    static int receive_error(const char* func, int err, bool log)
    {
        switch (err)
        {
#ifdef _WIN32
        case WSAENOBUFS:
#else
        case ENOBUFS:
        case ENOMEM:
#endif
            err = xyu::ES_Socket::No_Memory;
            if (log) xylogf("{}: no memory to alloc receive buffer", func);
            break;

#ifdef _WIN32
        case WSAECONNRESET:
#else
        case ECONNRESET:
#endif
            err = xyu::ES_Socket::Connection_Over;
            if (log) xyloge("{}: connection closed by remote host", func);
            break;

#ifdef _WIN32
        case WSAECONNABORTED:
#else
        case ECONNABORTED:
#endif
            err = xyu::ES_Socket::Connection_Over;
            if (log) xyloge("{}: connection closed by system", func);
            break;

#ifdef _WIN32
        case WSAEMSGSIZE:
#else
        case EMSGSIZE:
#endif
            err = xyu::ES_Socket::Datagram_Truncated;
            if (log) xyloge("{}: datagram size exceeds buffer limit and be truncated", func);
            break;

#ifdef _WIN32
        case WSAEINVAL:
#else
        case EINVAL:
#endif
            xyloge("{}: invalid param to receive", func);
            xyu::abort();

        default:
            return public_error(func, err, log);
        }
        return err;
    }

    static xyu::size_t receive_impl(void* h, void* buf, xyu::size_t capa, xyu::ES_Socket& state, bool throws)
    {
        if constexpr (XY_DEBUG)
        {
            if (XY_UNLIKELY(!buf))
                xyu::E_Logic::abort_null_pointer(XY_CurPos(nullptr,"xyu::Socket::receive"), "buffer");
            if (XY_UNLIKELY(!h)) abort_not_create("xyu::Socket::receive");
        }
        state = xyu::ES_Socket::Success;

        xyu::size_t tmp = capa, n;
        while (capa)
        {
            xyu::size_t one = capa;
#ifdef _WIN32
            one = xyu::min(one, xyu::t_number<int>::max);
#endif
            if (XY_UNLIKELY((n = ::recv(sk(h), (char*)buf, one, 0)) == -1))
            {
                int e = XY_ERROR_CODE;
                if (XY_EINTR(e)) continue;
                if (XY_EAGAIN(e) || XY_ETIMEDOUT(e)) break;
                state = receive_error("xyu::Socket::receive", e, throws);
                if (throws) throw xyu::E_Socket{state.code};
                break;
            }
            if (n == 0) break;

            capa -= n;
            buf = (char*)buf + n;
        }
        return tmp - capa;
    }

    xyu::size_t Socket::receive(void* buf, xyu::size_t capa)
    { xyu::ES_Socket state; return receive_impl(h, buf, capa, state, true); }

    xyu::size_t Socket::receive(void* buf, xyu::size_t capa, xyu::ES_Socket& state) noexcept
    { return receive_impl(h, buf, capa, state, false); }

    static xyu::size_t receive_impl(void* h, xyu::String& app, xyu::size_t maxn, xyu::ES_Socket& state, bool throws)
    {
        if constexpr (XY_DEBUG) if (XY_UNLIKELY(!h)) abort_not_create("xyu::Socket::receive");
        state = xyu::ES_Socket::Success;

        xyu::size_t total = 0, n;
        xyu::size_t capa = xyu::min(app.capacity() - app.size(), maxn);
        while (total < maxn)
        {
            if (app.size() == app.capacity())
            {
                if (XY_UNLIKELY(app.capacity() == xyu::String::limit())) break;
                app.reserve(xyu::min(app.size() + 64, xyu::String::limit()));
                capa = xyu::min(app.capacity() - app.size(), maxn - total);
            }

            xyu::size_t one = capa;
#ifdef _WIN32
            one = xyu::min(one, xyu::t_number<int>::max);
#endif
            if (XY_UNLIKELY((n = ::recv(sk(h), app.data() + app.size(), one, 0)) == -1))
            {
                int e = XY_ERROR_CODE;
                if (XY_EINTR(e)) continue;
                if (XY_EAGAIN(e) || XY_ETIMEDOUT(e)) break;
                state = receive_error("xyu::Socket::receive", e, throws);
                if (throws) throw xyu::E_Socket{state.code};
                break;
            }
            if (n == 0) break;

            total += n;
            app.resize(app.size() + n);
            capa -= n;
        }
        return total;
    }

    xyu::size_t Socket::receive(xyu::String& app, xyu::size_t maxn)
    { xyu::ES_Socket state; return receive_impl(h, app, maxn, state, true); }

    xyu::size_t Socket::receive(xyu::String& app, xyu::size_t maxn, xyu::ES_Socket& state) noexcept
    { return receive_impl(h, app, maxn, state, false); }
}

/// 发送 - 数据报
namespace xylu::xyconn
{
    XY_NOINLINE XY_COLD
    static int sendto_error(const char* func, int err, bool log)
    {
        switch (err)
        {
#ifdef _WIN32
        case WSAEAFNOSUPPORT:
#else
        case EAFNOSUPPORT:
#endif
            xyloge("{}: unsupport address family to send", func);
            xyu::abort();

        default:
            return send_error(func, err, log);
        }
    }

    static bool sendto_impl(void* h, const void* mess, xyu::size_t len, const Socket::Address* addr,
                            xyu::uint addrlen, xyu::ES_Socket& state, bool throws)
    {
        if constexpr (XY_DEBUG)
        {
            if (XY_UNLIKELY(!addr))
                xyu::E_Logic::abort_null_pointer(XY_CurPos(nullptr,"xyu::Socket::send"), "address");
            if (XY_UNLIKELY(!mess))
                xyu::E_Logic::abort_null_pointer(XY_CurPos(nullptr,"xyu::Socket::send"), "message");
            if (XY_UNLIKELY(addrlen > xyu::t_number<XY_ADDR_LEN>::max))
                xyu::E_Logic::abort_over_limit(XY_CurPos(nullptr,"xyu::Socket::send"), addrlen, 0, xyu::t_number<XY_ADDR_LEN>::max, "address length");
            if (XY_UNLIKELY(len > xyu::t_number<int>::max))
                xyu::E_Logic::abort_over_limit(XY_CurPos(nullptr,"xyu::Socket::bind"), len, 0, xyu::t_number<int>::max, "message length");
            if (XY_UNLIKELY(!h)) abort_not_create("xyu::Socket::send");
        }
        state = xyu::ES_Socket::Success;

        int flag = 0;
#ifndef _WIN32
        flag = MSG_NOSIGNAL;
#endif
        for (;;)
        {
            if (XY_UNLIKELY(::sendto(sk(h), (const char*)mess, len, flag, (sockaddr*)addr, addrlen) == -1))
            {
                int e = XY_ERROR_CODE;
                if (XY_EINTR(e)) continue;
                if (XY_EAGAIN(e) || XY_ETIMEDOUT(e)) return false;
                state = sendto_error("xyu::Socket::send", e, throws);
                if (throws) throw xyu::E_Socket{state.code};
                return false;
            }
            return true;
        }
    }

    bool Socket::send(const void* mess, xyu::size_t len, const Address* addr, xyu::uint size)
    { xyu::ES_Socket state; return sendto_impl(h, mess, len, addr, size, state, true); }

    bool Socket::send(const void* mess, xyu::size_t len, const Address* addr, xyu::uint size, xyu::ES_Socket& state) noexcept
    { return sendto_impl(h, mess, len, addr, size, state, false); }
}

/// 接收 - 数据报
namespace xylu::xyconn
{
    XY_NOINLINE XY_COLD
    static int recvfrom_error(const char* func, int err, bool log)
    {
        switch (err)
        {
#ifdef _WIN32
        case WSAEFAULT:
#else
        case EFAULT:
#endif
            xyloge("{}: invalid address", func);
            xyu::abort();

        default:
            return receive_error(func, err, log);
        }
    }

    static xyu::size_t recvfrom_impl(void* h, void* buf, xyu::size_t capa, Socket::Address* addr,
                                    xyu::uint* size, xyu::ES_Socket& state, bool throws)
    {
        if constexpr (XY_DEBUG)
        {
            if (XY_UNLIKELY(!buf))
                xyu::E_Logic::abort_null_pointer(XY_CurPos(nullptr,"xyu::Socket::receive"), "buffer");
            if (size && XY_UNLIKELY(*size > xyu::t_number<XY_ADDR_LEN>::max))
                xyu::E_Logic::abort_over_limit(XY_CurPos(nullptr,"xyu::Socket::receive"), *size, 0, xyu::t_number<XY_ADDR_LEN>::max, "address capacity");
            if (XY_UNLIKELY(capa > xyu::t_number<int>::max))
                xyu::E_Logic::abort_over_limit(XY_CurPos(nullptr,"xyu::Socket::receive"), capa, 0, xyu::t_number<int>::max, "message capacity");
            if (XY_UNLIKELY(!h)) abort_not_create("xyu::Socket::receive");
        }
        state = xyu::ES_Socket::Success;

        XY_ADDR_LEN s = size ? *size : 0;
        xyu::size_t n = 0;
        int flag = 0;
#ifdef __linux__
        flag = MSG_TRUNC;
#endif
        for (;;)
        {
            if (XY_UNLIKELY((n = ::recvfrom(sk(h), (char*)buf, capa, flag, (sockaddr*)addr, size ? &s : nullptr)) == -1))
            {
                n = 0;
                int e = XY_ERROR_CODE;
                if (XY_EINTR(e)) continue;
                if (XY_EAGAIN(e)) break;
#ifdef _WIN32
                if (e == WSAEMSGSIZE) n = capa;
#endif
                state = recvfrom_error("xyu::Socket::receive", e, throws);
                if (throws) throw xyu::E_Socket{state.code};
            }
#ifndef _WIN32
            else if (n > capa)
            {
                n = capa;
                state = recvfrom_error("xyu::Socket::receive", EMSGSIZE, throws);
                if (throws) throw xyu::E_Socket{state.code};
            }
#endif
            break;
        }
        if (size) *size = s;
        return n;
    }

    xyu::size_t Socket::receive(void* buf, xyu::size_t capa, Address* addr, xyu::uint* size)
    { xyu::ES_Socket state; return recvfrom_impl(h, buf, capa, addr, size, state, true); }

    xyu::size_t Socket::receive(void* buf, xyu::size_t capa, Address* addr, xyu::uint* size, xyu::ES_Socket& state)
    { return recvfrom_impl(h, buf, capa, addr, size, state, false); }
}

/// 等待
namespace xylu::xyconn
{
    XY_NOINLINE XY_COLD
    static int wait_error(int err, bool log) noexcept
    {
        const char* func = "xyu::socket::wait";
        switch (err)
        {
#ifdef _WIN32
        case WSAENOBUFS:
#else
        case ENOBUFS:
        case ENOMEM:
#endif
            err = xyu::ES_Socket::No_Memory;
            if (log) xylogf("{}: no memory to alloc for wait", func);
            return err;

        default:
            return public_error(func, err, log);
        }
    }

    static bool wait_help(void* h, Socket::_WaitMode wm, xyu::uint ms, xyu::ES_Socket& state, bool throws)
    {
        if constexpr (XY_DEBUG)
        {
            if (XY_UNLIKELY(!(wm & Socket::Wt_receive || wm & Socket::Wt_send)))
                xyu::E_Logic::abort_invalid_value(XY_CurPos(nullptr,"xyu::Socket::wait"), wm, "wait type");
            if (XY_UNLIKELY(ms != -1 && ms > xyu::t_number<int>::max))
                xyu::E_Logic::abort_over_limit(XY_CurPos(nullptr,"xyu::Socket::wait"), ms, 0, xyu::t_number<int>::max, "limit time");
            if (XY_UNLIKELY(!h)) abort_not_create("xyu::Socket::wait");
        }
        state = xyu::ES_Socket::Success;

        int mode = wm;
        if constexpr (Socket::Wt_receive != POLLIN) { if (wm & Socket::Wt_receive) mode = mode & ~Socket::Wt_receive | POLLIN; }
        if constexpr (Socket::Wt_send != POLLOUT) { if (wm & Socket::Wt_send) mode = mode & ~Socket::Wt_send | POLLOUT; }

        xyu::Clock clock;
        if (ms > 0) clock.start();

        int last = ms;
        for (;;)
        {
            pollfd pf;
            pf.fd = sk(h);
            pf.events = mode;
            pf.revents = 0;

#ifdef _WIN32
            int r = WSAPoll(&pf, 1, last);
#else
            int r = poll(&pf, 1, last);
#endif

            if (XY_UNLIKELY(r == -1))
            {
                int e = XY_ERROR_CODE;
                if (XY_EINTR(e)) goto next;
                state = wait_error(e, throws);
                if (throws) throw xyu::E_Socket{state.code};
                return false;
            }

            if (XY_UNLIKELY(pf.revents & (POLLERR | POLLNVAL)))
            {
                int e = 0;
                XY_ADDR_LEN len = sizeof(e);
                if (XY_UNLIKELY(getsockopt(pf.fd, SOL_SOCKET, SO_ERROR, (char*)&e, &len) != 0))
                    public_error("xyu::Socket::wait", XY_ERROR_CODE, true), xyu::abort();

                if (XY_LIKELY(e))
                {
                    state = parse_error(e);
                    if (throws) throw xyu::E_Socket{state.code};
                    return false;
                }
            }

            if (r == 0) return false;
            if (XY_LIKELY(pf.revents & mode || pf.revents & POLLHUP)) return true;

        next:
            if (ms > 0)
            {
                auto past = clock.elapse().ms();
                if (past >= ms) return false;
                last = ms - past;
            }
        }
    }

    bool Socket::wait(_WaitMode wm, xyu::uint ms)
    { xyu::ES_Socket state; return wait_help(h, wm, ms, state, true); }

    bool Socket::wait(_WaitMode wm, xyu::uint ms, xyu::ES_Socket& state) noexcept
    { return wait_help(h, wm, ms, state, false); }
}

/// 属性
namespace xylu::xyconn
{
    static xyu::StringView level_name(int level) noexcept
    {
        if (level == SOL_SOCKET) return "SOL_SOCKET";
        if (level == IPPROTO_TCP) return "IPPROTO_TCP";
        if (level == IPPROTO_IP) return "IPPROTO_IP";
        return "UNKNOWN";
    }

    XY_NOINLINE XY_COLD
    static int option_error(int err, int level, int optname) noexcept
    {
        const char* func = "xyu::socket::option";
        switch (err)
        {
#ifdef _WIN32
        case WSAEINVAL:
        case WSAEFAULT:
#else
        case EINVAL:
        case EFAULT:
        case EDOM:
#endif
            xyloge("{}: invalid param", func);
            xyu::abort();

#ifdef _WIN32
        case WSAENOPROTOOPT:
#else
        case ENOPROTOOPT:
#endif
            xyloge("{}: unspport option {} in {}", func, optname, level_name(level));
            xyu::abort();

#ifdef _WIN32
        case WSAENOBUFS:
#else
        case ENOBUFS:
        case ENOMEM:
#endif
            return xyu::ES_Socket::No_Memory;

#ifdef _WIN32
        case WSAEACCES:
#else
        case EACCES:
#endif
            return xyu::ES_Socket::No_Permission;

        default:
            return public_error(func, err, false);
        }
    }

    static bool set_option(void* h, int level, int option, const void* val, xyu::size_t len) noexcept
    {
        return XY_LIKELY(::setsockopt(sk(h), level, option, (char*)val, len) == 0);
    }

    static int check_param(xyu::uint64 value) noexcept
    {
        if constexpr (XY_DEBUG) if (XY_UNLIKELY(value > xyu::t_number<int>::max))
            xyu::E_Logic::abort_over_limit(XY_CurPos(nullptr,"xyu::socket::option"), value, 0, xyu::t_number<int>::max);
        return value;
    }

    int Socket::option(_OptionType op, xyu::uint64 value) noexcept
    {
        switch (op)
        {
        case Op_error:
            {
                int e = 0;
                XY_ADDR_LEN len = sizeof(e);
                if (XY_UNLIKELY(getsockopt(sk(h), SOL_SOCKET, SO_ERROR, (char*)&e, &len) == -1))
                    return option_error(XY_ERROR_CODE, SOL_SOCKET, SO_ERROR);
                if (XY_LIKELY(e == 0)) return xyu::ES_Socket::Success;
                return parse_error(e);
            }
        case Op_socktype:
            {
                int type = 0;
                XY_ADDR_LEN len = sizeof(type);
                if (XY_UNLIKELY(getsockopt(sk(h), SOL_SOCKET, SO_TYPE, (char*)&type, &len) == -1))
                {
                    option_error(XY_ERROR_CODE, SOL_SOCKET, SO_TYPE);
                    return -1;
                }
                if constexpr (Sk_stream != (int)SOCK_STREAM) { if (type == SOCK_STREAM) return Sk_stream; }
                if constexpr (Sk_dgram != (int)SOCK_DGRAM) { if (type == SOCK_DGRAM) return Sk_dgram; }
                if constexpr (Sk_raw != (int)SOCK_RAW) { if (type == SOCK_RAW) return Sk_raw; }
                return type;
            }
        case Op_address_family:
            {
                int af;
#ifdef _WIN32
                sockaddr_storage ss;
                XY_ADDR_LEN len = sizeof(ss);
                if (XY_UNLIKELY(getsockname(sk(h), (sockaddr*)&ss, &len) == -1))
                    public_error("xyu::Socket::option", GetLastError(), true), xyu::abort();
                af = ss.ss_family;
#else
                socklen_t len = sizeof(af);
                if (XY_UNLIKELY(getsockopt(sk(h), SOL_SOCKET, SO_DOMAIN, &af, &len) == -1))
                    option_error(XY_ERROR_CODE, SOL_SOCKET, SO_DOMAIN), xyu::abort();
#endif
                if constexpr (AF_ipv4 != AF_INET) { if (af == AF_INET) return AF_ipv4; }
                if constexpr (AF_ipv6 != AF_INET6) { if (af == AF_INET6) return AF_ipv6; }
                if constexpr (AF_local != AF_UNIX) { if (af == AF_UNIX) return AF_local; }
                return af;
            }

        case Op_share_port:
            {
                int enable = (bool)value;
                int option;
#ifdef _WIN32
                option = SO_REUSEADDR;
#else
                option = SO_REUSEPORT;
#endif
                return set_option(h, SOL_SOCKET, option, &enable, sizeof(enable));
            }

        case Op_timed_close:
            {
                linger l;
                if (value == 0) l.l_onoff = false;
                else
                {
                    l.l_onoff = true;
                    if ((xyu::int64)value < 0) l.l_linger = 0;
                    else l.l_linger = check_param(value);
                }
                return set_option(h, SOL_SOCKET, SO_LINGER, &l, sizeof(l));
            }

        case Op_timed_send:
            {
#ifdef _WIN32
                int tv = check_param(value);
#else
                timeval tv;
                tv.tv_sec = check_param(value) / 1000;
                tv.tv_usec = value % 1000 * 1000;
#endif
                return set_option(h, SOL_SOCKET, SO_SNDTIMEO, &tv, sizeof(tv));
            }
        case Op_timed_receive:
            {
#ifdef _WIN32
                int tv = check_param(value);
#else
                timeval tv;
                tv.tv_sec = check_param(value) / 1000;
                tv.tv_usec = value % 1000 * 1000;
#endif
                return set_option(h, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));
            }

        case Op_send_bufsize:
            {
                int size = check_param(value);
                return set_option(h, SOL_SOCKET, SO_SNDBUF, &size, sizeof(size));
            }
        case Op_receive_bufsize:
            {
                int size = check_param(value);
                return set_option(h, SOL_SOCKET, SO_RCVBUF, &size, sizeof(size));
            }

        case Op_no_nagle:
            {
                int enable = (bool)value;
                return set_option(h, IPPROTO_TCP, TCP_NODELAY, &enable, sizeof(enable));
            }
        case Op_keep_alive:
            {
                int enable = value & 0xff;
                // 禁用
                if (!enable) return set_option(h, SOL_SOCKET, SO_KEEPALIVE, &enable, sizeof(enable));
                // 探测次数
                int keepcnt = (value >> 8) & 0xff;
                if (keepcnt && !set_option(h, IPPROTO_TCP, TCP_KEEPCNT, &keepcnt, sizeof(keepcnt))) return false;
                // 探测间隔
                int keepintvl = (value >> 16) & 0xffff;
                if (keepintvl && !set_option(h, IPPROTO_TCP, TCP_KEEPINTVL, &keepintvl, sizeof(keepintvl))) return false;
                // 探测开始
                int keepidle = value >> 32;
                if (keepidle && !set_option(h, IPPROTO_TCP, TCP_KEEPIDLE, &keepidle, sizeof(keepidle))) return false;
                // 设置启用
                return set_option(h, SOL_SOCKET, SO_KEEPALIVE, &enable, sizeof(enable));
            }

        case Op_broadcast:
            {
                int enable = (bool)value;
                return set_option(h, SOL_SOCKET, SO_BROADCAST, &enable, sizeof(enable));
            }
        case Op_multicast_ttl:
            {
                int ttl = check_param(value);
                return set_option(h, IPPROTO_IP, IP_MULTICAST_TTL, &ttl, sizeof(ttl));
            }
        case Op_add_membership:
            {
                // value 已为网络字节序
                return set_option(h, IPPROTO_IP, IP_ADD_MEMBERSHIP, &value, sizeof(value));
            }

        case Op_debug_info:
            {
                int enable = (bool)value;
                return set_option(h, SOL_SOCKET, SO_DEBUG, &enable, sizeof(enable));
            }
        case Op_skip_route:
            {
                int enable = (bool)value;
                return set_option(h, SOL_SOCKET, SO_DONTROUTE, &enable, sizeof(enable));
            }

        default:
            option_error(EINVAL, 0, 0);
            XY_UNREACHABLE;
            xyu::abort();
        }
    }
}
