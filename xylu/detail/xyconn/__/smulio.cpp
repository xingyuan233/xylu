#ifdef _WIN32
#  include <winsock2.h>
#  include <mswsock.h>
#else
#  include <liburing.h>
#  include <netinet/in.h>
#  include <fcntl.h>
#  include <unistd.h>
#  include <stdlib.h>
#  include <errno.h>
#  include <sys/socket.h>
#  include <cerrno>
#  include <netinet/tcp.h>
#  include "../../xysystem/time.h"
#endif

#include "../smulio.h"

#include "./wsainit.h"
#include "../../xyconc/atomic.h"

/// 跨平台转换工具
namespace
{
    using xyu::Socket;
    static_assert(sizeof(void*) == sizeof(Socket));

#ifdef _WIN32
    SOCKET sk(Socket& sock) noexcept { return (SOCKET)((uintptr_t)(*(void**)&sock) - 1); }
    Socket& skr(void*& h) noexcept { h = (void*)((uintptr_t)h + 1); return *(Socket*)&h; }

    HANDLE sh(Socket& sock) noexcept { return (HANDLE)((uintptr_t)(*(void**)&sock) - 1); }
    HANDLE sm(void* h) noexcept { return (HANDLE)h; }

#else
    int sk(Socket& sock) noexcept { return (int)((uintptr_t)(*(void**)&sock) - 1); }
    Socket& skr(void*& h) noexcept { h = (void*)((uintptr_t)h + 1); return *(Socket*)&h; }

    io_uring* sm(void* h) noexcept { return (io_uring*)h; }

    int sk(void* h) noexcept { return (int)((uintptr_t)h - 1); }
#endif

}

#ifdef _WIN32
/// 队列
namespace xylu::xyconn
{
    using xyu::Atomic;

    // buf 对于跨线程传递与释放时会比直接 malloc/free 更快
    // 但线程内使用时会慢很多，不应该使用此容器
    struct alignas(xyu::K_Cache_Line_Size)
    MPMCQueue : xyu::class_no_copy_move_t
    {
        struct Node                 // 侵入式节点
        {
            Atomic<Node*> next;     // 下一个节点指针
            alignas(xyu::K_Cache_Line_Size)
            Atomic<int> entered;    // 是否曾被链入队列
            char buf[];             //  缓冲区
        };
    private:
        Atomic<Node*> tail{};       // 队尾指针
        char _pad1[xyu::K_Cache_Line_Size - sizeof(tail)];
        Atomic<Node*> head{};       // 队首指针
        char _pad2[xyu::K_Cache_Line_Size - sizeof(head)];
        Atomic<xyu::uint> n{};      // 分配的节点数
        char _pad3[xyu::K_Cache_Line_Size - sizeof(n)];
        const xyu::uint capa{};     // 允许的节点容量 (可能会略微超过一些)
        const xyu::uint offset{};   // 缓冲区偏移
        const xyu::uint bytes{};    // 节点大小

    public:
        /// 构造
        explicit MPMCQueue(xyu::uint capa, xyu::uint size, xyu::uint align) noexcept : capa(capa),
            // 计算缓冲区大小和偏移
            offset(xyu::max<xyu::uint>(alignof(Atomic<int>), align) + xyu::K_Cache_Line_Size),
            bytes(xyu::max<xyu::uint>(xyu::K_Cache_Line_Size * 2, offset + size))
        {
            // 分配哨兵节点
            auto lead = (Node*)xymemory::__::under_alalloc(bytes, xyu::K_Cache_Line_Size);
            if (XY_UNLIKELY(!lead)) xymemory::__::abort_alloc_failed(bytes, 1, xyu::K_Cache_Line_Size);
            new (&lead->next) Atomic<Node*>{nullptr};
            new (&lead->entered) Atomic<int>{true};
            tail.store(lead, xyu::N_Atomic_Order::Relaxed);
            head.store(lead, xyu::N_Atomic_Order::Relaxed);
        }

        /// 析构
        ~MPMCQueue() noexcept
        {
            Node* cur = head.load(xyu::N_Atomic_Order::Relaxed);
            for (;;)
            {
                Node* next = cur->next.load(xyu::N_Atomic_Order::Relaxed);
                xymemory::__::under_dealalloc(cur);
                if (!next) break;
                cur = next;
            }
        }

        /// 获取一个节点
        char* take() noexcept
        {
            Node* node = head;
            Node* next = node->next;
            // 尝试从中获取节点
            if (XY_LIKELY(next) && node->next.compare_exchange_weak(next, nullptr))
            {
                if (XY_LIKELY(head.compare_exchange_strong(node, next))) return (char*)node + offset;
                node->next = next;
            }
            // 无可用节点分配一个
            node = (Node*)xymemory::__::under_alalloc(bytes, xyu::K_Cache_Line_Size);
            if (XY_UNLIKELY(!node)) return nullptr;
            n.fetch_add(1, xyu::N_Atomic_Order::Relaxed);
            new (&node->next) Atomic<Node*>{nullptr};
            new (&node->entered) Atomic<int>{false};
            return (char*)node + offset;
        }

        /// 归还一个节点
        void put(char* buf) noexcept
        {
            Node* node = (Node*)(buf - offset);
            // 未达上限或者已经被连入过链表
            if (n.load(xyu::N_Atomic_Order::Relaxed) <= capa || node->entered)
            {
                node->next.store(nullptr, xyu::N_Atomic_Order::Relaxed);
                node->entered.store(true, xyu::N_Atomic_Order::Relaxed);
                Node* last = tail.exchange(node);
                last->next = node;
            }
            else
            {
                n.fetch_sub(1, xyu::N_Atomic_Order::Relaxed);
                xymemory::__::under_dealalloc(node);
            }
        }
    };

    static_assert(sizeof(MPMCQueue) <= xyu::K_Cache_Line_Size * 4);
    MPMCQueue& cq(void* buf) noexcept { return *(MPMCQueue*)buf; }
}

/// 跨平台上下文
namespace xylu::xyconn
{
    struct SCtx
    {
        WSAOVERLAPPED overlapped;
        char buf[];
    };
}

#else
/// 环形队列
namespace xylu::xyconn
{
    static_assert(sizeof(io_uring) <= xyu::K_Cache_Line_Size * 4);
}
#endif

/// 创建
namespace xylu::xyconn::__
{
    [[noreturn]] XY_NOINLINE XY_COLD
    static void public_error(const char* func, int err) noexcept
    {
        xyu::E_Logic::abort_unknown_error(func, err);
    }

    [[noreturn]] XY_NOINLINE XY_COLD
    static void create_error(int err) noexcept
    {
        const char* func = "xyu::Smulio::Smulio";
        switch (err)
        {
#ifdef _WIN32
        case ERROR_ACCESS_DENIED:
            xyloge("{}: no permission to create IoCompletionPort", func);
            xyu::abort();
        case ERROR_NOT_ENOUGH_MEMORY:
        case ERROR_OUTOFMEMORY:
            xylogf("{}: no memory to alloc IoCompletionPort", func);
            xyu::abort();
#else
        case EPERM:
            xyloge("{}: no permission to create io_uring", func);
            xyu::abort();
        case ENOMEM:
            xylogf("{}: no memory to alloc io_uring", func);
            xyu::abort();
        case ENOSYS:
            xyloge("{}: do not support io_uring", func);
            xyu::abort();
#endif
        default: public_error(func, err);
        }
    }

    SmulioBase::SmulioBase(xyu::uint size, xyu::uint align, xyu::uint capa) noexcept : af(Socket::AF_unspec)
    {
#ifdef _WIN32
        wsa_init();
        // 创建 iocp
        h = CreateIoCompletionPort(INVALID_HANDLE_VALUE, nullptr, 0, 0);
        if (XY_UNLIKELY(!h)) create_error(GetLastError());
        // 计算 ctx 偏移
        xyu::uint salign = xyu::max<xyu::uint>(alignof(WSAOVERLAPPED), align);
        offset = ((sizeof(WSAOVERLAPPED) + align - 1) & -align);
        xyu::uint sbytes = offset + size;
        new (&cq(queue)) MPMCQueue{capa, sbytes, salign};
#else
        // 创建 io_uring
        h = (io_uring*)queue;
        int r = io_uring_queue_init(capa, sm(h), 0);
        if (XY_UNLIKELY(r)) create_error(-r);
        // 存储偏移
        bytes = size;
        offset = align;
        xyu::prealloc(size, align, capa);
#endif
    }
}

/// 销毁
namespace xylu::xyconn::__
{
    SmulioBase::~SmulioBase() noexcept
    {
#ifdef _WIN32
        cq(queue).~MPMCQueue();
        if (!XY_UNLIKELY(CloseHandle(sm(h))))
            public_error("xyu::Smulio::~Smulio", GetLastError());
#else
        io_uring_queue_exit(sm(h));
#endif
    }
}

/// 加入
namespace xylu::xyconn::__
{
    bool SmulioBase::bind(Socket& socket) noexcept
    {
#ifdef _WIN32
        if (XY_UNLIKELY(af == Socket::AF_unspec))
        {
            af = (Socket::_AddressFamily)socket.option(Socket::Op_address_family);
            st = (Socket::_SocketType)socket.option(Socket::Op_socktype);
        }
        h = CreateIoCompletionPort(sh(socket), sm(h), (ULONG_PTR)*(void**)&socket, 0);
        return h;
#else
        if (XY_UNLIKELY(af == Socket::AF_unspec))
        {
            af = (Socket::_AddressFamily)-1;
            int flags = fcntl(sk(socket), F_GETFL, 0);
            if (XY_UNLIKELY(flags < 0)) return false;
            st = (Socket::_SocketType)(flags & O_NONBLOCK);
        }
        return true;
#endif
    }
}

/// 获取与回收
namespace xylu::xyconn::__
{
    SmulioCtx* SmulioBase::take() noexcept
    {
#ifdef _WIN32
        char* p = cq(queue).take();
        if (XY_UNLIKELY(!p)) return nullptr;
        return (SmulioCtx*)(p + offset);
#else
        return (SmulioCtx*)xyu::alloc<void>(nullptr, bytes, offset);
#endif
    }

    void SmulioBase::put(SmulioCtx* ctx) noexcept
    {
#ifdef _WIN32
        cq(queue).put((char*)ctx - offset);
#else
        xyu::dealloc<void>(ctx, bytes, offset);
#endif
    }
}

/// 异步操作
namespace xylu::xyconn::__
{
#ifdef _WIN32
    static LPFN_ACCEPTEX get_accepter(Socket& socket) noexcept
    {
        static struct Getter
        {
            LPFN_ACCEPTEX lpfnAcceptEx = nullptr;
            GUID guid = WSAID_ACCEPTEX;
            DWORD dwBytes = 0;

            explicit Getter(Socket& socket) noexcept
            {
                if (XY_LIKELY(!WSAIoctl(sk(socket), SIO_GET_EXTENSION_FUNCTION_POINTER,
                         &guid, sizeof(guid),
                         &lpfnAcceptEx, sizeof(lpfnAcceptEx),
                         &dwBytes, nullptr, nullptr))) return;
                abort_unsupport_accept();
            }
        } getter(socket);
        return getter.lpfnAcceptEx;
    }
#endif

    bool SmulioBase::accept(SmulioCtx* ctx) noexcept
    {
#ifdef _WIN32
        auto sctx = (SCtx*)((char*)ctx - offset);

        sctx->overlapped = {};
        new (&ctx->handle) Socket(af, st, (xyu::ES_Socket&)(xyu::ErrorState&)ctx->state);
        if (XY_UNLIKELY(ctx->state.fail())) return false;

        DWORD dwBytes = 0;
        int r = get_accepter(*ctx->sock)(sk(*ctx->sock), (SOCKET)ctx->handle,
                ctx->buf, 0, 0, ctx->len, &dwBytes, &sctx->overlapped);
        if (XY_UNLIKELY(r) && XY_UNLIKELY(GetLastError() != ERROR_IO_PENDING))
        {
            skr(ctx->handle).~Socket();
            return false;
        }
        return true;
#else
        auto sqe = io_uring_get_sqe(sm(h));
        if (XY_UNLIKELY(!sqe)) return false;
        ctx->ret = ctx->len;
        io_uring_prep_accept(sqe, sk(*ctx->sock), (sockaddr*)ctx->buf, &ctx->len, 0);
        io_uring_sqe_set_data(sqe, ctx);
        return XY_LIKELY(io_uring_submit(sm(h)) >= 0);
#endif
    }

    /// 发送
    bool SmulioBase::send(SmulioCtx* ctx) noexcept
    {
#ifdef _WIN32
        auto sctx = (SCtx*)((char*)ctx - offset);

        sctx->overlapped = {};
        WSABUF sbuf; sbuf.buf = (char*)ctx->buf; sbuf.len = ctx->len;
        char* cbuf = (char*)&ctx->buf - __builtin_offsetof(WSABUF, buf);
        xyu::mem_copy(cbuf, (void*)&sbuf, sizeof(WSABUF));

        int r = WSASend(sk(*ctx->sock), (LPWSABUF)cbuf, 1, nullptr, 0, &sctx->overlapped, nullptr);
        if (XY_UNLIKELY(r) && XY_UNLIKELY(GetLastError() != ERROR_IO_PENDING)) return false;
        return true;
#else
        auto sqe = io_uring_get_sqe(sm(h));
        if (XY_UNLIKELY(!sqe)) return false;
        io_uring_prep_send(sqe, sk(*ctx->sock), ctx->buf, ctx->len, 0);
        io_uring_sqe_set_data(sqe, ctx);
        return XY_LIKELY(io_uring_submit(sm(h)) >= 0);
#endif
    }

    /// 接收
    bool SmulioBase::receive(SmulioCtx* ctx) noexcept
    {
#ifdef _WIN32
        auto sctx = (SCtx*)((char*)ctx - offset);

        sctx->overlapped = {};
        WSABUF sbuf; sbuf.buf = (char*)ctx->buf; sbuf.len = ctx->len;
        char* cbuf = (char*)&ctx->buf - __builtin_offsetof(WSABUF, buf);
        xyu::mem_copy(cbuf, (void*)&sbuf, sizeof(WSABUF));

        DWORD flags = 0;
        int r = WSARecv(sk(*ctx->sock), (LPWSABUF)cbuf, 1, nullptr, &flags, &sctx->overlapped, nullptr);
        if (XY_UNLIKELY(r) && XY_UNLIKELY(GetLastError() != ERROR_IO_PENDING)) return false;
        return true;
#else
        auto sqe = io_uring_get_sqe(sm(h));
        if (XY_UNLIKELY(!sqe)) return false;
        io_uring_prep_recv(sqe, sk(*ctx->sock), ctx->buf, ctx->len, 0);
        io_uring_sqe_set_data(sqe, ctx);
        return XY_LIKELY(io_uring_submit(sm(h)) >= 0);
#endif
    }
}

/// 获取
namespace xylu::xyconn::__
{
#ifdef _WIN32
    // 超时或端口关闭
    XY_NOINLINE static SmulioCtx* get_nullptr(MPMCQueue& queue, xyu::uint offest) noexcept
    {
        int e = GetLastError();
        char* sctx = queue.take();
        if (XY_UNLIKELY(!sctx)) return nullptr;
        auto ctx = (SmulioCtx*)(sctx + offest);
        if (e == WAIT_TIMEOUT) ctx->state = xyu::ES_Smulio::Timeout;
        else ctx->state = xyu::ES_Smulio::MulIO_Closed;
        return ctx;
    }
#else
    // 超时或端口关闭
    XY_NOINLINE static SmulioCtx* get_nullptr(xyu::uint size, xyu::uint align, int e) noexcept
    {
        auto ctx = (SmulioCtx*)xyu::alloc<void>(nullptr, size, align);
        if (XY_UNLIKELY(!ctx)) return nullptr;
        if (e == ETIME) ctx->state = xyu::ES_Smulio::Timeout;
        else ctx->state = xyu::ES_Smulio::MulIO_Closed;
        return ctx;
    }
#endif

    // 其他错误
    XY_NOINLINE static SmulioCtx* get_failed(SmulioCtx* ctx, int e) noexcept
    {
        xyloge("error: {}", e);
#ifdef _WIN32
        if (ctx->op == SmulioCtx::Op_accept) skr(ctx->handle).~Socket();
        else ctx->ret = 0;

        switch (e)
        {
        case WSAECONNRESET:
        case WSAECONNABORTED:
            ctx->state = xyu::ES_Smulio::Connection_Over; break;
        case WSAENETDOWN:
            ctx->state = xyu::ES_Smulio::Net_SubSystem_Failed; break;
        case WSA_OPERATION_ABORTED:
            ctx->state = xyu::ES_Smulio::Socket_Closed; break;
        case ERROR_MORE_DATA:
            ctx->state = xyu::ES_Smulio::Datagram_Truncated; break;
        case WSAENOBUFS:
            ctx->state = xyu::ES_Smulio::No_Memory; break;
        default:
            ctx->state = -1; break;
        }
#else
        ctx->ret = 0;

        switch (e)
        {
        case ECONNRESET:
        case ECONNABORTED:
        case ETIMEDOUT:
        case EPIPE:
            ctx->state = xyu::ES_Smulio::Connection_Over; break;
        case ENETDOWN:
            ctx->state = xyu::ES_Smulio::Net_SubSystem_Failed; break;
        case EMSGSIZE:
            ctx->state = xyu::ES_Smulio::Datagram_Truncated; break;
        case ENOMEM:
            ctx->state = xyu::ES_Smulio::No_Memory; break;
        default:
            ctx->state = -1; break;
        }
#endif
        return ctx;
    }

    SmulioCtx* SmulioBase::get(xyu::uint ms) noexcept
    {
#ifdef _WIN32
        DWORD ret; ULONG_PTR key; char* sctx = nullptr;
        BOOL r = GetQueuedCompletionStatus(sm(h), &ret,
                    &key, (LPOVERLAPPED*)&sctx, ms == -1 ? INFINITE : ms);

        if (XY_UNLIKELY(!sctx)) return get_nullptr(cq(queue), offset);

        auto ctx = (SmulioCtx*)(sctx + offset);
        if (XY_UNLIKELY(!r)) return get_failed(ctx, GetLastError());

        if (ctx->op != SmulioCtx::Op_accept)
        {
            WSABUF sbuf;
            char* cbuf = (char*)&ctx->buf - __builtin_offsetof(WSABUF, buf);
            xyu::mem_copy((void*)&sbuf, (void*)cbuf, sizeof(WSABUF));
            ctx->buf = sbuf.buf;
            ctx->len = sbuf.len;
            ctx->ret = ret;
        }
        else
        {
            // 解析地址
            if (af == Socket::AF_ipv4) ctx->ret = xyu::min(ctx->len, sizeof(Socket::AddressIPv4));
            else if (af == Socket::AF_ipv6) ctx->ret = xyu::min(ctx->len, sizeof(Socket::AddressIPv6));
            else
            {
                if (ctx->len <= 2) ctx->ret = ctx->len;
                else for (ctx->ret=2; ctx->ret<ctx->len && ((char*)ctx->buf)[ctx->len]; ++ctx->ret);
            }

            // 同步属性
            SOCKET listenSocket = sk(*ctx->sock);
            setsockopt((SOCKET)ctx->handle, SOL_SOCKET, SO_UPDATE_ACCEPT_CONTEXT,
                        (char*)&listenSocket, sizeof(listenSocket));

            skr(ctx->handle);
        }

#else
        io_uring_cqe *cqe; int r;

        // 阻塞等待
        if (ms == -1)
        {
        retry:
            r = io_uring_wait_cqe(sm(h), &cqe);
            if (XY_UNLIKELY(r < 0))
            {
                if (XY_UNLIKELY(r == -EINTR)) goto retry;
                return get_nullptr(bytes, offset, -r);
            }
        }
        // 超时等待
        else
        {
            xyu::int64 limit = xyu::Duration_any().ms() + ms;
            __kernel_timespec t;
        timed_retry:
            auto last = limit - xyu::Duration_any().ms();
            if (XY_UNLIKELY(last <= 0)) return get_nullptr(bytes, offset, ETIME);
            t.tv_sec = last / 1000, t.tv_nsec = (last % 1000) * 1000000;

            r = io_uring_wait_cqe_timeout(sm(h), &cqe, &t);
            if (r < 0)
            {
                if (XY_UNLIKELY(r == -EINTR)) goto timed_retry;
                return get_nullptr(bytes, offset, -r);
            }
        }

        auto ctx = (SmulioCtx*)io_uring_cqe_get_data(cqe);
        auto ret = cqe->res;
        io_uring_cqe_seen(sm(h), cqe);

        if (XY_UNLIKELY(cqe->res < 0)) return get_failed(ctx, -ret);

        if (ctx->op == SmulioCtx::Op_accept)
        {
            // 交换 地址长度 和 缓存的缓冲区大小
            xyu::swap(ctx->len, ctx->ret);

            // 非阻塞设置
            if (st)
            {
                int flags = fcntl(ret, F_GETFL, 0);
                if (XY_UNLIKELY(flags == -1 || fcntl(ret, F_SETFL, flags | O_NONBLOCK) == -1))
                {
                    close(ret);
                    return get_failed(ctx, errno);
                }
            }

            ctx->handle = (void*)(uintptr_t)ret;
            skr(ctx->handle);
        }
        else ctx->ret = ret;

#endif
        ctx->state = xyu::ES_Smulio::Success;
        return ctx;
    }
}
