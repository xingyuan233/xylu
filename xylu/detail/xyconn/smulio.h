#pragma once

/**
 * @file    xyconn/smulio.h
 * @brief   套接字 IO 多路复用
 * @date    2026-08-22
 *
* @details
 *   依赖库：
 *   - Windows下需要链接 ws2_32
 *   - 类Unix下需要链接 uring
 */

#include "./socket.h"

/// 异常类
namespace xylu::xycore
{
    /// 异常类
    struct E_Smulio : E_Resource { using E_Resource::E_Resource; };

    /// 异常码
    struct ES_Smulio : ErrorState
    {
        using ErrorState::ErrorState;

        enum _Smulio
        {
            // 操作成功
            Success = ES_Socket::Success,
            // 内存不足
            No_Memory = ES_Socket::No_Memory,
            // 连接中断
            Connection_Over = ES_Socket::Connection_Over,
            // 网络子系统瘫痪
            Net_SubSystem_Failed = ES_Socket::Net_SubSystem_Failed,
            // 数据报被截断
            Datagram_Truncated = ES_Socket::Datagram_Truncated,
            // 等待超时
            Timeout = 128,
            // 套接字已经关闭
            Socket_Closed,
            // 端口关闭
            MulIO_Closed,
        };
    };
}

/// 多路 IO 对象
namespace xylu::xyconn
{
    /// 多路 IO 上下文 基类
    struct SmulioCtx
    {
        Socket* sock;           // 绑定的套接字
        enum Option { Op_accept = 1, Op_send = 2, Op_receive = 3, }
        op;                     // 操作类型
        xyu::ES_Smulio state;   // 结果状态
        xyu::uint ret;          // 结果长度
        xyu::uint len;          // 缓冲区长度
        void* buf;              // 缓冲区
        void* handle;           // 新连接

        Socket& newsock() noexcept { return *(Socket*)&handle; }
    };

    namespace __
    {
        [[noreturn]] XY_NOINLINE XY_COLD void abort_unsupport_accept() noexcept;

        struct SmulioBase : xyu::class_no_copy_move_t
        {
        private:
            void* h = nullptr;
            xyu::uint bytes;
            xyu::uint offset;
            Socket::_AddressFamily af;
            Socket::_SocketType st;

            alignas(xyu::K_Cache_Line_Size)
            char queue[xyu::K_Cache_Line_Size * 4];

        public:
            SmulioBase(xyu::uint size, xyu::uint align, xyu::uint capa) noexcept;
            ~SmulioBase() noexcept;

        protected:
            bool bind(Socket& socket) noexcept;

            SmulioCtx* take() noexcept;
            void put(SmulioCtx* ctx) noexcept XY_NONNULL_NDEBUG(2);

            bool accept(SmulioCtx* ctx) noexcept XY_NONNULL(2);
            bool send(SmulioCtx* ctx) noexcept XY_NONNULL(2);
            bool receive(SmulioCtx* ctx) noexcept XY_NONNULL(2);

            SmulioCtx* get(xyu::uint ms) noexcept;
        };
    }

    /**
     * @brief 多路 IO
     * @tparam Ctx 上下文对象
     * @details
     *   实现方式：
     *   - Windows下封装 IOCP 和 无锁队列节点缓冲区，可多线程直接使用
     *   - 类Unix下封装 io_uring 和 线程局部缓冲区，只能单线程管理，分发给多线程使用
     *
     *   注意事项：
     *   - 类型擦除：Ctx 必须第一个继承 SmulioCtx 或将其作为 第一个成员
     *   - 线程安全：Windows下线程安全，类Unix不安全
     */
    template <typename Ctx = SmulioCtx>
    struct Smulio : __::SmulioBase
    {
        static_assert(xyu::t_is_base<SmulioCtx, Ctx>);
        static_assert(alignof(Ctx) <= xyu::K_Cache_Line_Size);

        /**
         * @brief 构造函数
         * @param capa 驻留节点数量上限 (可能会略微超出一些)
         * @details capa 为软上限，高并发时可能会额外分配更多，但最终会被释放
         */
        explicit Smulio(xyu::uint capa = 64) noexcept : SmulioBase(sizeof(Ctx), alignof(Ctx), capa) {}

        /**
         * @brief 将套接字与多路IO对象绑定
         * @details 通常不会失败，除非内存不足等严重错误
         */
        bool bind(Socket& socket) noexcept { return SmulioBase::bind(socket); }

        /**
         * @brief 添加一个接受连接的异步调用
         * @param socket 监听中的套接字
         * @param addr 存储对端地址的缓冲区
         * @param capa 存储对端地址的缓冲区容量
         * @details 通常不会失败，除非内存不足等严重错误
         */
        bool accept(Socket& socket, void* addr, xyu::uint capa) noexcept
        {
            if constexpr (XY_DEBUG) if (XY_UNLIKELY(!addr)) xyu::E_Logic::abort_null_pointer(XY_CurPos(), "address buffer");
            return accept(socket, [=](SmulioCtx& ctx) { ctx.buf = addr; ctx.len = capa; });
        }

        /**
         * @brief 添加一个接受连接的异步调用
         * @param socket 监听中的套接字
         * @param callback 通过 `callback(ctx&)` 设置自定义数据
         * @details 通常不会失败，除非内存不足等严重错误
         * @note 必须设置 ctx.buf, ctx.len 接收对端地址，ctx.buf 必须自行对齐
         */
        template <typename Callback>
        bool accept(Socket& socket, Callback&& callback) noexcept(xyu::t_can_nothrow_call<Callback, Ctx&>)
        {
            if constexpr (XY_DEBUG) if (XY_UNLIKELY(!socket)) xyu::E_Logic::abort_null_pointer(XY_CurPos(), "socket");
            return forward_op(socket, SmulioCtx::Op_accept, xyu::forward<Callback>(callback), &Smulio::accept);
        }

        /**
         * @brief 添加一个接受连接的异步调用
         * @param socket 监听中的套接字
         * @param ctx 复用的上下文
         * @param callback 通过 `callback(ctx&)` 设置自定义数据
         * @details 通常不会失败，除非内存不足等严重错误
         * @note 必须设置 ctx.buf, ctx.len 接收对端地址，ctx.buf 必须自行对齐
         */
        template <typename Callback>
        bool accept(Socket& socket, Ctx& ctx, Callback&& callback) noexcept(xyu::t_can_nothrow_call<Callback, Ctx&>)
        {
            if constexpr (XY_DEBUG) if (XY_UNLIKELY(!socket)) xyu::E_Logic::abort_null_pointer(XY_CurPos(), "socket");
            if constexpr (XY_DEBUG) if (XY_UNLIKELY(&ctx == &EmptyCtx)) xyu::E_Logic::abort_invalid_value(XY_CurPos(), (xyu::size_t)&EmptyCtx, "ctx");
            return forward_op(socket, &ctx, SmulioCtx::Op_accept, xyu::forward<Callback>(callback), &Smulio::accept);
        }

        /**
         * @brief 添加一个发送数据的异步调用
         * @param socket 要发送的套接字
         * @param mess 发送的数据
         * @param len 发送的数据长度
         * @details 通常不会失败，除非内存不足等严重错误
         */
        bool send(Socket& socket, const void* mess, xyu::uint len) noexcept
        {
            if constexpr (XY_DEBUG) if (XY_UNLIKELY(!mess)) xyu::E_Logic::abort_null_pointer(XY_CurPos(), "message");
            return send(socket, [=](SmulioCtx& ctx) { ctx.buf = (void*)mess; ctx.len = len; });
        }

        /**
         * @brief 添加一个发送数据的异步调用
         * @param socket 要发送的套接字
         * @param callback 通过 `callback(ctx&)` 设置自定义数据
         * @details 通常不会失败，除非内存不足等严重错误
         * @note 必须设置 ctx.buf, ctx.len 作为发送的数据
         */
        template <typename Callback>
        bool send(Socket& socket, Callback&& callback) noexcept(xyu::t_can_nothrow_call<Callback, Ctx&>)
        {
            if constexpr (XY_DEBUG) if (XY_UNLIKELY(!socket)) xyu::E_Logic::abort_null_pointer(XY_CurPos(), "socket");
            return forward_op(socket, SmulioCtx::Op_send, xyu::forward<Callback>(callback), &Smulio::send);
        }

        /**
         * @brief 添加一个发送数据的异步调用
         * @param socket 要发送的套接字
         * @param ctx 复用的上下文
         * @param callback 通过 `callback(ctx&)` 设置自定义数据
         * @details 通常不会失败，除非内存不足等严重错误
         * @note 必须设置 ctx.buf, ctx.len 作为发送的数据
         */
        template <typename Callback>
        bool send(Socket& socket, Ctx& ctx, Callback&& callback) noexcept(xyu::t_can_nothrow_call<Callback, Ctx&>)
        {
            if constexpr (XY_DEBUG) if (XY_UNLIKELY(!socket)) xyu::E_Logic::abort_null_pointer(XY_CurPos(), "socket");
            if constexpr (XY_DEBUG) if (XY_UNLIKELY(&ctx == &EmptyCtx)) xyu::E_Logic::abort_invalid_value(XY_CurPos(), (xyu::size_t)&EmptyCtx, "ctx");
            return forward_op(socket, &ctx, SmulioCtx::Op_send, xyu::forward<Callback>(callback), &Smulio::send);
        }

        /**
         * @brief 添加一个接收数据的异步调用
         * @param socket 要发送的套接字
         * @param buf 接收数据的缓冲区
         * @param capa 接收数据的缓冲区容量
         * @details 通常不会失败，除非内存不足等严重错误
         */
        bool receive(Socket& socket, void* buf, xyu::uint capa) noexcept
        {
            if constexpr (XY_DEBUG) if (XY_UNLIKELY(!buf)) xyu::E_Logic::abort_null_pointer(XY_CurPos(), "receive buffer");
            return receive(socket, [=](SmulioCtx& ctx) { ctx.buf = buf; ctx.len = capa; });
        }

        /**
         * @brief 添加一个接收数据的异步调用
         * @param socket 要发送的套接字
         * @param callback 通过 `callback(ctx&)` 设置自定义数据
         * @details 通常不会失败，除非内存不足等严重错误
         * @note 必须设置 ctx.buf, ctx.len 作为接收数据的缓冲区
         */
        template <typename Callback>
        bool receive(Socket& socket, Callback&& callback) noexcept(xyu::t_can_nothrow_call<Callback, Ctx&>)
        {
            if constexpr (XY_DEBUG) if (XY_UNLIKELY(!socket)) xyu::E_Logic::abort_null_pointer(XY_CurPos(), "socket");
            return forward_op(socket, SmulioCtx::Op_receive, xyu::forward<Callback>(callback), &Smulio::receive);
        }

        /**
         * @brief 添加一个接收数据的异步调用
         * @param socket 要发送的套接字
         * @param ctx 复用的上下文
         * @param callback 通过 `callback(ctx&)` 设置自定义数据
         * @details 通常不会失败，除非内存不足等严重错误
         * @note 必须设置 ctx.buf, ctx.len 作为接收数据的缓冲区
         */
        template <typename Callback>
        bool receive(Socket& socket, Ctx& ctx, Callback&& callback) noexcept(xyu::t_can_nothrow_call<Callback, Ctx&>)
        {
            if constexpr (XY_DEBUG) if (XY_UNLIKELY(!socket)) xyu::E_Logic::abort_null_pointer(XY_CurPos(), "socket");
            if constexpr (XY_DEBUG) if (XY_UNLIKELY(&ctx == &EmptyCtx)) xyu::E_Logic::abort_invalid_value(XY_CurPos(), (xyu::size_t)&EmptyCtx, "ctx");
            return forward_op(socket, &ctx, SmulioCtx::Op_receive, xyu::forward<Callback>(callback), &Smulio::receive);
        }

        /**
         * @brief 获取一个异步调用结果
         * @param ms 超时时间，-1 为无限阻塞 (默认)
         * @details
         *   ctx 字段简述：
         *   - state: 用于判断结果状态
         *            - No_Memory: 内存不足，其他字段均不可访问，且该操作超时或多路IO异常关闭，此 ctx 禁止复用
         *            - Timtout: 超时时间不为 -1 且超时时返回
         *            - MulIO_Closed: 多路 IO 被异常关闭，通常不会发生
         *            - Datagram_Truncated: 对于 UDP 协议，接收缓冲区无法容纳完整报文，此时数据被截断
         *            - Connection_Over: 连接已关闭
         *            - Socket_Closed: 套接字被手动关闭，但队列中仍有其他异步操作时返回
         *            - -1: 其余未知状态，可重试
         *   - op: 操作类型
         *   - ret: send成功发送字节数 / recv成功接收字节数 / accept获取的对端地址长度，为 0 表示正常关闭
         *   - sock: 绑定的套接字
         *   - buf / len: 原缓冲区或待发送数据信息
         *   - handle: accept成功时新建立的套接字句柄
         * @note 其他自定义部分的生命周期需要自行管理，ctx 处理完毕后通过 `put` 归还 或 复用
         */
        Ctx& get(xyu::uint ms = -1) noexcept
        {
            auto ctx = (Ctx*)SmulioBase::get(ms);
            if (XY_UNLIKELY(!ctx)) return *(Ctx*)&EmptyCtx;
            return *ctx;
        }

        /// 归还 `get` 处理完毕后的 ctx
        void put(Ctx& ctx) noexcept
        {
            if (XY_UNLIKELY(&ctx == &EmptyCtx)) return;
            SmulioBase::put(&ctx);
        }

    private:
        using SmulioBase::accept;
        using SmulioBase::send;
        using SmulioBase::receive;

        static constexpr SmulioCtx EmptyCtx{nullptr, {}, xyu::ES_Smulio::No_Memory};

        template <typename Callback, bool nothrow = xyu::t_can_nothrow_call<Callback, Ctx&>>
        bool forward_op(Socket& socket, SmulioCtx* ctx, SmulioCtx::Option op, Callback&& callback, bool(Smulio::*func)(SmulioCtx*)) noexcept(nothrow)
        {
            static_assert(xyu::t_can_call<Callback, Ctx&>);
            ctx->sock = &socket;
            ctx->op = op;
            if constexpr (nothrow) xyu::forward<Callback>(callback)(*(Ctx*)ctx);
            else try { xyu::forward<Callback>(callback)(*(Ctx*)ctx); }
            catch (...) { SmulioBase::put(ctx); throw; }
            return (this->*func)(ctx);
        }

        template <typename Callback, bool nothrow = xyu::t_can_nothrow_call<Callback, Ctx&>>
        bool forward_op(Socket& socket, SmulioCtx::Option op, Callback&& callback, bool(Smulio::*func)(SmulioCtx*)) noexcept(nothrow)
        {
            SmulioCtx* ctx = take();
            if (XY_UNLIKELY(!ctx)) return false;
            return forward_op(socket, ctx, op, xyu::forward<Callback>(callback), func);
        }
    };
}
