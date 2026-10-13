#pragma once

/**
 * @file    xyconn/socket.h
 * @brief   套接字
 * @date    2026-08-10
 *
 * @details
 *   依赖库：
 *   - Windows下需要链接 ws2_32
 */

#include "./_.h"
#include "../xystring/fmt.h"

/// 套接字异常类
namespace xylu::xycore
{
    /// 异常类
    struct E_Socket : E_Resource { using E_Resource::E_Resource; };

    /// 异常码
    struct ES_Socket : ErrorState
    {
        using ErrorState::ErrorState;

        enum _Socket
        {
            // 操作成功
            Success = ES_Resource::Success,
            // 无权限
            No_Permission = ES_Resource::No_Permission,
            // 内存不足
            No_Memory = ES_Resource::No_Memory,
            // 系统打开套接字数到达上限
            System_Limit = ES_Resource::System_Limit,
            // 进程打开套接字数达到上限
            Process_Limit = ES_Resource::Process_Limit,
            // 本地地址或端口已占用
            Local_Address_Occupied = 64,
            // 网络子系统错误
            Net_SubSystem_Failed,
            // 本地地址不存在
            Local_Address_Not_Found,
            // 网络不可达
            Net_Unreachable,
            // 主机不可达
            Host_Unreachable,
            // 连接超时
            Connection_Timeout,
            // 连接被拒绝
            Connection_Refused,
            // 连接断开
            Connection_Over,
            // 数据报错误 (非阻塞通过 wait/option 获取异常时无法分辨 Datagram_Limit 和 Datagram_Truncated，统一返回 Datagram_Error)
            Datagram_Error,
            // 数据报大小超过内核限制 (通常小于 65536)
            Datagram_Limit,
            // 缓存区过小，数据报被截断且丢弃
            Datagram_Truncated,
        };
    };
}

/// 套接字
namespace xylu::xyconn
{
    /**
     * @brief 跨平台套接字封装。
     *
     * 该类提供统一的接口管理底层 socket，自动处理资源释放，并集成错误状态码。
     * 支持 TCP/UDP、IPv4/IPv6，支持流式 和数据报，可通过 `| Sk_noblock` 开启非阻塞。
     * 支持 错误码 和 抛出异常 双机制。
     * 地址使用 AddressIPv4 / AddressIPv6 表示，端口为 0 时由系统自动分配。
     *
     * @code
     * // -------------------- TCP 服务器 --------------------
     * Socket server(Socket::AF_ipv4, Socket::Sk_stream);
     * server.bind(xyu::Socket::AddressIPv4({}, 8080));
     * server.listen();
     * auto client = server.accept();  // 阻塞直到有连接
     * String msg;
     * client.receive(msg);            // 接收数据
     * client.send("Hello", 5);        // 发送数据
     * 
     * // -------------------- TCP 客户端 --------------------
     * Socket client(Socket::AF_ipv4, Socket::Sk_stream);
     * client.connect(Socket::AddressIPv4("127.0.0.1:8080"));
     * client.send("Hi", 2);
     * xyu::String reply;
     * client.receive(reply);
     * 
     * // -------------------- UDP 数据报 --------------------
     * Socket udp(Socket::AF_ipv4, Socket::Sk_dgram);
     * udp.bind(Socket::AddressIPv4({}, "9999"));
     * 
     * // 发送数据报到指定地址
     * udp.send("data", 4, Socket::AddressIPv4("192.168.1.100:8888"));
     * 
     * // 接收数据报并获取发送方地址
     * char buf[1024];
     * Socket::AddressIPv4 from;
     * udp.receive(buf, sizeof(buf), from);
     * 
     * // -------------------- 非阻塞模式 --------------------
     * Socket nbio(Socket::AF_ipv4, Socket::Sk_stream | Socket::Sk_noblock);
     * nbio.connect(Socket::AddressIPv4("127.0.0.1:8080")); // 立即返回
     * if (!nbio.wait(Socket::Wt_send, 3000)) {
     *     // 连接超时
     * }
     * @endcode
     *
     * @see xylu::xycore::E_Socket, xylu::xycore::ES_Socket 用于错误处理
     */
    struct Socket
    {
        /* 地址 */

        /// 地址族
        enum _AddressFamily
        {
            AF_unspec   = 0,    // 未指定
            AF_local    = 1,    // 本机通信
            AF_ipv4     = 2,    // ipv4
            AF_ipv6     = 23,   // ipv6
        };

        /// 地址基类
        struct Address { xyu::uint16 family; };

        struct _Port;

        struct _IPv4;
        struct AddressIPv4;

        struct _FlowInfo;
        struct _IPv6;
        struct AddressIPv6;

        /// 本地地址 @todo path库 ，并将异常处理的日志细化本地地址

        /* 套接字对象 */

        /// 套接字类型
        enum _SocketType
        {
            Sk_stream   = 1,    // 流式
            Sk_dgram    = 2,    // 数据报
            Sk_raw      = 3,    // 原始

            Sk_noblock  = 0x800,    // 非阻塞
        };

        /// 默认构造
        Socket() noexcept;
        /// 析构函数
        ~Socket() noexcept { xyu::ES_Socket state; destroy(state); }

        /// 移动构造
        Socket(Socket&& other) noexcept : h{other.h} { other.h = nullptr; }
        /// 移动赋值
        Socket& operator=(Socket&& other) noexcept { xyu::swap(h, other.h); return *this; }

        /// 创建套接字
        Socket(_AddressFamily af, _SocketType sk, int pf = 0) : Socket() { create(af, sk, pf); }
        /// 创建套接字
        Socket(_AddressFamily af, _SocketType sk, xyu::ES_Socket& state) : Socket() { create(af, sk, state); }
        /// 创建套接字
        Socket(_AddressFamily af, _SocketType sk, int pf, xyu::ES_Socket& state) : Socket() { create(af, sk, pf, state); }

        /* 套接字创建与销毁 */

        /// 套接字是否创建
        explicit operator bool() const noexcept { return h; }

        /**
         * @brief 创建套接字
         * @param af 地址族
         * @param sk 套接字类型
         * @param pf 协议类型 (通常不需要指定)
         * @note 不应未关闭重复打开
         */
        void create(_AddressFamily af, _SocketType sk, int pf = 0);

        /**
         * @brief 创建套接字
         * @param af 地址族
         * @param sk 套接字类型
         * @param pf 协议类型 (通常不需要指定)
         */
        void create(_AddressFamily af, _SocketType sk, int pf, xyu::ES_Socket& state) noexcept;

        /**
         * @brief 创建套接字
         * @param af 地址族
         * @param sk 套接字类型
         */
        void create(_AddressFamily af, _SocketType sk, xyu::ES_Socket& state) noexcept { create(af, sk, 0, state); }


        /**
         * @brief 销毁套接字
         * @details 若套接字已销毁则不做处理
         */
        void destroy();

        /**
         * @brief 销毁套接字
         * @details 若套接字已销毁则不做处理
         */
        void destroy(xyu::ES_Socket& state) noexcept;

        /* 绑定与监听 */

        /**
         * @brief 绑定本地端口
         * @param addr 本地地址
         */
        template <typename Addr, typename = xyu::t_enable<xyu::t_is_base<Address, Addr>>>
        void bind(Addr&& addr) { bind(&addr, sizeof(Addr)); }

        /**
         * @brief 绑定本地端口
         * @param addr 本地地址
         */
        template <typename Addr, typename = xyu::t_enable<xyu::t_is_base<Address, Addr>>>
        void bind(Addr&& addr, xyu::ES_Socket& state) noexcept { bind(&addr, sizeof(Addr), state); }

        /**
         * @brief 绑定本地端口
         * @param addr 本地地址
         * @param size 地址长度
         */
        void bind(const Address* addr, xyu::uint size);

        /**
         * @brief 绑定本地端口
         * @param addr 本地地址
         * @param size 地址长度
         */
        void bind(const Address* addr, xyu::uint size, xyu::ES_Socket& state) noexcept;


        /**
         * @brief 监听等待连接
         * @param backlog 等待队列最大长度 (0时提供一个较大值)
         */
        void listen(xyu::uint backlog = 0);

        /**
         * @brief 监听等待连接
         * @param backlog 等待队列最大长度 (0时提供一个较大值)
         */
        void listen(xyu::uint backlog, xyu::ES_Socket& state) noexcept;

        /**
         * @brief 监听等待连接
         * @details 提供一个较大的等待队列
         */
        void listen(xyu::ES_Socket& state) noexcept { listen(0, state); }

        /* 接受连接 */

        /**
         * @brief 接受连接返回新套接字，并获取客户端地址
         * @param addr 用于接收客户端地址，自行保证容量足够否则会被截断
         * @param block 新套接字是否阻塞，默认为非阻塞
         * @details 非阻塞模式下无连接会返回空Socket
         */
        template <typename Addr, typename = xyu::t_enable<xyu::t_is_base<Address, Addr>>>
        Socket accept(Addr& addr, bool block = false) { xyu::uint size = sizeof(Addr); return accept(&addr, &size, block); }

        /**
         * @brief 接受连接返回新套接字，并获取客户端地址
         * @param addr 用于接收客户端地址，可选，若提供自行保证容量足够否则会被截断
         * @param size 传入地址缓冲区容量，成功调用后会变为实际地址大小
         * @param block 新套接字是否阻塞，默认为非阻塞
         * @details 非阻塞模式下无连接会返回空Socket
         */
        Socket accept(Address* addr = nullptr, xyu::uint* size = nullptr, bool block = false);

        /**
         * @brief 接受连接返回新套接字，并获取客户端地址
         * @param addr 用于接收客户端地址，自行保证容量足够否则会被截断
         * @param block 新套接字是否阻塞
         * @details 非阻塞模式下无连接会返回空Socket
         */
        template <typename Addr, typename = xyu::t_enable<xyu::t_is_base<Address, Addr>>>
        Socket accept(Addr& addr, bool block, xyu::ES_Socket& state) noexcept { xyu::uint size = sizeof(Addr); return accept(&addr, &size, block, state); }

        /**
         * @brief 接受连接返回新非阻塞套接字，并获取客户端地址
         * @param addr 用于接收客户端地址，自行保证容量足够否则会被截断
         * @details 非阻塞模式下无连接会返回空Socket
         */
        template <typename Addr, typename = xyu::t_enable<xyu::t_is_base<Address, Addr>>>
        Socket accept(Addr& addr, xyu::ES_Socket& state) noexcept { xyu::uint size = sizeof(Addr); return accept(&addr, &size, false, state); }

        /**
         * @brief 接受连接返回新套接字，并获取客户端地址
         * @param addr 用于接收客户端地址，若提供自行保证容量足够否则会被截断
         * @param size 传入地址缓冲区容量，成功调用后会变为实际地址大小
         * @param block 新套接字是否阻塞，默认为非阻塞
         * @details 非阻塞模式下无连接会返回空Socket
         */
        Socket accept(Address* addr, xyu::uint* size, bool block, xyu::ES_Socket& state) noexcept;

        /**
         * @brief 接受连接返回新非阻塞套接字
         * @details 非阻塞模式下无连接会返回空Socket
         */
        Socket accept(xyu::ES_Socket& state) noexcept { return accept(nullptr, nullptr, false, state); }

        /* 发起连接 */

        /**
         * @brief 连接到指定地址
         * @details 非阻塞模式下操作成功立即返回，需要通过 wait 等待可写
         */
        template <typename Addr, typename = xyu::t_enable<xyu::t_is_base<Address, Addr>>>
        void connect(Addr&& addr) { connect(&addr, sizeof(Addr)); }

        /**
         * @brief 连接到指定地址
         * @details 非阻塞模式下操作成功立即返回，需要通过 wait 等待可写
         */
        template <typename Addr, typename = xyu::t_enable<xyu::t_is_base<Address, Addr>>>
        void connect(Addr&& addr, xyu::ES_Socket& state) noexcept { connect(&addr, sizeof(Addr), state); }

        /**
         * @brief 连接到指定地址
         * @details 非阻塞模式下操作成功立即返回，需要通过 wait 等待可写
         */
        void connect(const Address* addr, xyu::uint size);

        /**
         * @brief 连接到指定地址
         * @details 非阻塞模式下操作成功立即返回，需要通过 wait 等待可写
         */
        void connect(const Address* addr, xyu::uint size, xyu::ES_Socket& state) noexcept;

        /* 断开连接 */

        /// shutdown 类型 (关闭接收, 关闭发送, 关闭所有)
        enum _ShutdownType { Sd_receive, Sd_send, Sd_all };

        /**
         * @brief 断开套接字
         * @param st 断开类型 (默认断开发送)
         */
        void shutdown(_ShutdownType st = Sd_send);

        /**
         * @brief 断开套接字
         * @param st 断开类型 (默认断开发送)
         */
        void shutdown(_ShutdownType st, xyu::ES_Socket& state) noexcept;

        /// 断开套接字的发送端
        void shutdown(xyu::ES_Socket& state) noexcept { shutdown(Sd_send, state); }

        /* 发送 */

        /**
         * @brief 发送数据
         * @details 非阻塞模式下可能因缓冲区满返回大小可能不等于期望大小
         */
        xyu::size_t send(const void* mess, xyu::size_t len);

        /**
         * @brief 发送数据
         * @details 非阻塞模式下可能因缓冲区满返回大小可能不等于期望大小
         */
        xyu::size_t send(const void* mess, xyu::size_t len, xyu::ES_Socket& state) noexcept;

        /* 接收 */

        /**
         * @brief 接收数据
         * @note 不会额外写入 '\0'
         */
        xyu::size_t receive(void* buf, xyu::size_t capa);

        /**
         * @brief 接收数据
         * @note 不会额外写入 '\0'
         */
        xyu::size_t receive(void* buf, xyu::size_t capa, xyu::ES_Socket& state) noexcept;

        /**
         * @brief 接收数据追加到尾部
         * @param maxn 最多接收字节数 (默认 -1，适用于非阻塞套接字)
         */
        xyu::size_t receive(xyu::String& app, xyu::size_t maxn = -1);

        /**
         * @brief 接收数据追加到尾部
         * @param maxn 最多接收字节数
         */
        xyu::size_t receive(xyu::String& app, xyu::size_t maxn, xyu::ES_Socket& state) noexcept;

        /**
         * @brief 接收数据追加到尾部
         * @note 适用于非阻塞套接字
         */
        xyu::size_t receive(xyu::String& app, xyu::ES_Socket& state) noexcept { return receive(app, -1, state); }

        /**
         * @brief 接收数据
         * @param maxn 最多接收字节数 (默认 -1，适用于非阻塞套接字)
         */
        xyu::String receive(xyu::size_t maxn = -1) { xyu::String buf; receive(buf, maxn); return buf; }

        /**
         * @brief 接收数据
         * @param maxn 最多接收字节数
         */
        xyu::String receive(xyu::size_t maxn, xyu::ES_Socket& state) noexcept { xyu::String buf; receive(buf, maxn, state); return buf; }

        /**
         * @brief 接收数据追加到尾部
         * @note 适用于非阻塞套接字
         */
        xyu::String receive(xyu::ES_Socket& state) noexcept { xyu::String buf; receive(buf, state); return buf; }

        /* 发送 - 数据报 */

        /**
         * @brief 发送数据报
         * @details 非阻塞模式下可能因缓冲区满发送失败
         */
        template <typename Addr, typename = xyu::t_enable<xyu::t_is_base<Address, Addr>>>
        bool send(const void* mess, xyu::size_t len, Addr&& addr) { return send(mess, len, &addr, sizeof(Addr)); }

        /**
         * @brief 发送数据报
         * @details 非阻塞模式下可能因缓冲区满发送失败
         */
        template <typename Addr, typename = xyu::t_enable<xyu::t_is_base<Address, Addr>>>
        bool send(const void* mess, xyu::size_t len, Addr&& addr, xyu::ES_Socket& state) noexcept { return send(mess, len, &addr, sizeof(Addr), state); }

        /**
         * @brief 发送数据报
         * @details 非阻塞模式下可能因缓冲区满发送失败
         */
        bool send(const void* mess, xyu::size_t len, const Address* addr, xyu::uint size);

        /**
         * @brief 发送数据报
         * @details 非阻塞模式下可能因缓冲区满发送失败
         */
        bool send(const void* mess, xyu::size_t len, const Address* addr, xyu::uint size, xyu::ES_Socket& state) noexcept;

        /* 接收 - 数据报 */

        /**
         * @brief 接收数据
         * @note 一次成功读取一个数据报，若长度不足会被截断并丢弃剩余部分
         */
        template <typename Addr, typename = xyu::t_enable<xyu::t_is_base<Address, Addr>>>
        xyu::size_t receive(void* buf, xyu::size_t capa, Addr& addr) { xyu::uint size = sizeof(Addr); return receive(buf, capa, &addr, &size); }

        /**
         * @brief 接收数据
         * @param capa 缓冲区容量，一次成功读取一个数据报，若长度不足会被截断并丢弃剩余部分
         * @param addr 用于接收客户端地址，自行保证容量足够否则会被截断
         * @param size 传入地址缓冲区容量，成功调用后会变为实际地址大小
         * @note 不会额外写入 '\0'
         */
        xyu::size_t receive(void* buf, xyu::size_t capa, Address* addr, xyu::uint* size);

        /**
        * @brief 接收数据
        * @note 一次成功读取一个数据报，若长度不足会被截断并丢弃剩余部分
        */
        template <typename Addr, typename = xyu::t_enable<xyu::t_is_base<Address, Addr>>>
        xyu::size_t receive(void* buf, xyu::size_t capa, Addr& addr, xyu::ES_Socket& state) { xyu::uint size = sizeof(Addr); return receive(buf, capa, &addr, &size, state); }

        /**
         * @brief 接收数据
         * @param capa 缓冲区容量，一次成功读取一个数据报，若长度不足会被截断并丢弃剩余部分
         * @param addr 用于接收客户端地址，自行保证容量足够否则会被截断
         * @param size 传入地址缓冲区容量，成功调用后会变为实际地址大小
         * @note 不会额外写入 '\0'
         */
        xyu::size_t receive(void* buf, xyu::size_t capa, Address* addr, xyu::uint* size, xyu::ES_Socket& state);

        /* 等待非阻塞套接字 */

        /// 等待类型 (等待可接收, 等待可发送)
        enum _WaitMode { Wt_receive = 1 << 0, Wt_send = 1 << 1, };

        /**
         * @brief 等待非阻塞套接字
         * @param wm 等待类型，Wt_receive 则为等待接收到信息，Wt_send 则为可以继续发送信息 (缓冲区从满变为空或者连接完毕)
         * @param ms 等待时间，-1 则为无限等待，如果为非负数且超时后返回 false
         */
        bool wait(_WaitMode wm, xyu::uint ms = -1);

        /**
         * @brief 等待非阻塞套接字
         * @param wm 等待类型，Wt_receive 则为等待接收到信息，Wt_send 则为可以继续发送信息 (缓冲区从满变为空或者连接完毕)
         * @param ms 等待时间，-1 则为无限等待，如果为非负数且超时后返回 false
         */
        bool wait(_WaitMode wm, xyu::uint ms, xyu::ES_Socket& state) noexcept;

        /**
         * @brief 等待非阻塞套接字
         * @param wm 等待类型，Wt_receive 则为等待接收到信息，Wt_send 则为可以继续发送信息 (缓冲区从满变为空或者连接完毕)
         */
        void wait(_WaitMode wm, xyu::ES_Socket& state) noexcept { wait(wm, -1, state); }

        /* 选项设置 */

        enum _OptionType
        {
            Op_error,           // 获取套接字上待处理的错误并清除，0为无异常 (建议用 xyu::ES_Socket 接收)
            Op_socktype,        // 获取套接字类型
            Op_address_family,  // 获取地址族

            // 允许多个套接字绑定到相同的地址和端口 (默认禁用)
            // 必须在 bind 之前设置
            // linux下会检查用户uid相同才可绑定，windows下无检查
            Op_share_port,

            // 设置 close 超时返回 (默认禁用)，单位: s
            // 0 表示立即返回并在后台发完剩余消息
            // 正数表示如果时限内发完正常返回，如果超时则发送 RET 报文并返回
            // 负数表示直接发送 RET 报文并返回 (此处负数指转为无符号后 >= 0x8000... 的数)
            Op_timed_close,

            // 设置 send 超时返回 (默认禁用)，单位: ms
            // 仅适用与阻塞套接字
            // 0 表示阻塞，正数表示如果时限内没发完返回发了部分
            Op_timed_send,

            // 设置 receive 超时返回 (默认禁用)，单位: ms
            // 仅适用于阻塞套接字
            // 0 表示阻塞，正数表示如果时限内没发完返回发了部分
            Op_timed_receive,

            Op_send_bufsize,    // 设置发送缓冲区大小，内存不足时返回 false
            Op_receive_bufsize, // 设置接收缓冲区大小，内存不足时返回 false

            Op_no_nagle,        // 禁用 Nagle 算法 (默认启用算法)

            // TCP 保活探测 (默认关闭)
            // 格式 {低1字节:是否启用, 1字节:探测次数，2字节:探测间隔s, 高4字节:空闲时间阈值s}
            // 启用的情况下，每个参数为 0 时 表示使用默认值或者沿用上一次的值
            Op_keep_alive,

            Op_broadcast,       // 允许发送广播数据包 (默认禁用)，权限不足时返回 false
            Op_multicast_ttl,   // 设置多播包的传播范围

            // 加入多播组
            // 格式 {_Ipv4:多播地址, _Ipv4:本地地址}
            // 必须在 bind 之前设置，ENETDOWN时返回 false
            Op_add_membership,

            Op_debug_info,      // 记录调试信息 (默认关闭)
            Op_skip_route,      // 跳过路由查找 (默认关闭)
        };

        /**
         * @brief 设置或获取指定选项
         * @param value 参数见具体枚举值
         * @details
         *   对于设置操作，成功时返回 true(1)，失败返回 false(0)；
         *   对于读取操作，成功时返回 非负数，失败返回 -1
         * @note 参数不合理均会直接 abort
         */
        int option(_OptionType op, xyu::uint64 value = 0) noexcept;

    private:
        void* h = nullptr;

    public:
        friend constexpr _SocketType operator|(_SocketType sk1, _SocketType sk2) noexcept { return _SocketType((int)sk1 | (int)sk2); }
    };
}

/// 地址等后置实现
namespace xylu::xyconn
{
    // 端口号
    struct Socket::_Port
    {
        xyu::uint8 port_high;
        xyu::uint8 port_low;

        constexpr _Port() noexcept : port_high(0), port_low(0) {}
        constexpr _Port(xyu::uint16 port) noexcept : port_high(port >> 8), port_low(port & 0xff) {}
        constexpr _Port& operator=(xyu::uint16 port) noexcept { port_high = port >> 8; port_low = port & 0xff; return *this; }
        constexpr operator xyu::uint16() const noexcept { return port_high << 8 | port_low; }
    };

    // IPv4
    struct Socket::_IPv4
    {
        xyu::uint8 ip[4]{};

        constexpr _IPv4() noexcept {}
        constexpr _IPv4(xyu::uint8 n1, xyu::uint8 n2, xyu::uint8 n3, xyu::uint8 n4) noexcept : ip{n1, n2, n3, n4} {}
        constexpr _IPv4(const char* p) noexcept : _IPv4(xyu::StringView{p}) {}
        constexpr _IPv4(xyu::StringView sip) noexcept { parse(sip); }
        constexpr _IPv4& operator=(xyu::StringView sip) noexcept { parse(sip); return *this; }

        static constexpr bool valid(xyu::StringView sip) noexcept
        {
            int part = 0;
            int index = 0;
            for (auto c : sip.range())
            {
                if (c >= '0' && c <= '9')
                {
                    part = part * 10 + c - '0';
                    if (XY_UNLIKELY(part > 255)) return false;
                }
                else if (c == '.')
                {
                    if (XY_UNLIKELY(++index >= 4)) return false;
                    part = 0;
                }
                else return false;
            }
            return index == 3;
        }

        constexpr void parse(xyu::StringView sip) noexcept
        {
            if constexpr (XY_DEBUG) if (!valid(sip))
                xyu::E_Logic::abort_invalid_string(XY_CurPos("IPv4::parse"), sip.data(), sip.size(), "ipv4");

            ip[0] = 0;
            int index = 0;
            for (auto c : sip.range())
            {
                if (c >= '0' && c <= '9') ip[index] = ip[index] * 10 + c - '0';
                else if (c == '.') ip[++index] = 0;
            }
        }

        operator xyu::String() const noexcept
        {
            return xyfmtraw(xyu::String{15}, "{}.{}.{}.{}", ip[0], ip[1], ip[2], ip[3]);
        }
    };

    /// IPv4地址
    struct Socket::AddressIPv4 : Address
    {
        _Port port;
        _IPv4 ip;
        char _pad[8]{};

        // 端口号为 0 表示系统自动选择空闲端口
        constexpr explicit AddressIPv4(_Port port) noexcept : AddressIPv4("127...1", port) {}
        constexpr AddressIPv4(_IPv4 ip, _Port port) noexcept : Address{AF_ipv4}, port(port), ip(ip) {}
        constexpr AddressIPv4(const char* p) noexcept : AddressIPv4(xyu::StringView{p}) {}
        constexpr AddressIPv4(xyu::StringView saddr = "127.0.0.1:0") noexcept : Address{AF_ipv4} { parse(saddr); }
        constexpr AddressIPv4& operator=(xyu::StringView saddr) noexcept { parse(saddr); return *this; }

        static constexpr bool valid(xyu::StringView saddr) noexcept
        {
            xyu::size_t split = saddr.find(':');
            if (XY_UNLIKELY(split == -1)) return false;
            int port = 0;
            for (auto c : saddr.subview(split + 1).range())
            {
                if (c >= '0' && c <= '9')
                {
                    port = port * 10 + c - '0';
                    if (XY_UNLIKELY(port > 65535)) return false;
                }
                else return false;
            }
            return _IPv4::valid(saddr.subview(0, split));
        }

        constexpr void parse(xyu::StringView saddr) noexcept
        {
            if constexpr (XY_DEBUG) if (!valid(saddr))
                xyu::E_Logic::abort_invalid_string(XY_CurPos("AddressIPv4::parse"), saddr.data(), saddr.size(), "address_ipv4");

            xyu::size_t split = saddr.find(':');
            ip.parse(saddr.subview(0, split));
            int nport = 0;
            for (auto c : saddr.subview(split + 1).range()) nport = nport * 10 + c - '0';
            port = nport;
        }

        operator xyu::String() const noexcept;
    };

    // 流标签
    struct Socket::_FlowInfo
    {
    private:
        xyu::uint8 info[4]{};
    public:
        constexpr _FlowInfo& set_traffic(xyu::uint8 dscp, xyu::uint8 ecn = 0) noexcept
        {
            if constexpr (XY_DEBUG)
            {
                if (XY_UNLIKELY(dscp >= 64))
                    xyu::E_Logic::abort_over_limit({__FILE__,__LINE__,"FlowInfo::set_traffic"}, dscp, 0, 63, "dscp");
                if (XY_UNLIKELY(ecn >= 4))
                    xyu::E_Logic::abort_over_limit({__FILE__,__LINE__,"FlowInfo::set_traffic"}, ecn, 0, 3, "ecn");
            }

            info[0] = dscp >> 2;
            info[1] = dscp << 6 | ecn << 4 | info[1] & 0x0f;
            return *this;
        }

        constexpr _FlowInfo& set_label(xyu::uint32 label) noexcept
        {
            if constexpr (XY_DEBUG) if (XY_UNLIKELY(label > 0xfffff))
                    xyu::E_Logic::abort_over_limit({__FILE__,__LINE__,"FlowInfo::set_label"}, label, 0, 0xfffff, "label");

            info[1] = info[1] & 0xf0 | label >> 16;
            info[2] = label >> 8 & 0xff;
            info[3] = label & 0xff;
            return *this;
        }
    };

    // ipv6 地址
    struct Socket::_IPv6
    {
        xyu::uint8 ip[16]{};

        constexpr _IPv6() noexcept {}
        constexpr _IPv6(const char* p) noexcept XY_NONNULL_RELEASE(2) { parse(p); }
        constexpr _IPv6(xyu::StringView sip) noexcept { parse(sip); }
        constexpr _IPv6& operator=(xyu::StringView sip) { parse(sip); return *this; }

        static constexpr bool valid(xyu::StringView sip) noexcept
        {
            int index = 0;
            int part = 0;
            int split = -1;
            bool sign = false;
            bool first = true;
            for (xyu::size_t i = 0; i < sip.size(); ++i)
            {
                char c = sip[i];
                int add = -1;
                if (c >= '0' && c <= '9') add = c - '0';
                else if (c >= 'a' && c <= 'f') add = c - 'a' + 10;
                else if (c >= 'A' && c <= 'F') add = c - 'A' + 10;
                if (add != -1)
                {
                    if (sign && first) return false;
                    part = part * 16 + add;
                    if (XY_UNLIKELY(part > 0xffff)) return false;
                    sign = false;
                    first = false;
                }
                else if (c == ':')
                {
                    if (sign)
                    {
                        if (split != -1) return false;
                        if (index == 7) { if (i + 1 < sip.size()) return false; }
                        else if (!(first && index == 1)) ++index;
                        split = index;
                        first = false;
                    }
                    else
                    {
                        sign = true;
                        if (XY_UNLIKELY(++index >= 8)) return false;
                    }
                    part = 0;
                }
                else return false;
            }
            if (sign && split != index) return false;
            return split == -1 ? index == 7 : index <= 7;
        }

        constexpr void parse(xyu::StringView sip) noexcept
        {
            if constexpr (XY_DEBUG) if (!valid(sip))
                xyu::E_Logic::abort_invalid_string(XY_CurPos("IPv6::parse"), sip.data(), sip.size(), "ipv6");

            int n = 0;
            int index = 0;
            int split = -1;
            for (xyu::size_t i = 0; i < sip.size(); ++i)
            {
                char c = sip[i];
                int add = -1;
                if (c >= '0' && c <= '9') add = c - '0';
                else if (c >= 'a' && c <= 'f') add = c - 'a' + 10;
                else if (c >= 'A' && c <= 'F') add = c - 'A' + 10;
                if (add != -1) n = n * 16 + add;
                else
                {
                    ip[index++] = n >> 8;
                    ip[index++] = n & 0xff;
                    if (sip[i + 1] == ':') { ++i; split = index; }
                    n = 0;
                }
            }
            ip[index++] = n >> 8;
            ip[index++] = n & 0xff;
            if (index == 16) return;

            // 处理零压缩
            int fill = 16 - index;
            xyu::mem_move(ip + split + fill, ip + split, index - split);
            xyu::mem_set(ip + split, '\0', fill);
        }

        operator xyu::String() const noexcept
        {
            return xyfmtraw(xyu::String{39}, "{:x}:{:x}:{:x}:{:x}:{:x}:{:x}:{:x}:{:x}", ip[0]<<8|ip[1], ip[2]<<8|ip[3], ip[4]<<8|ip[5], ip[6]<<8|ip[7], ip[8]<<8|ip[9], ip[10]<<8|ip[11], ip[12]<<8|ip[13], ip[14]<<8|ip[15]);
        }
    };

    /// IPv6地址
    struct Socket::AddressIPv6 : Address
    {
        _Port port;
        _FlowInfo flow;
        _IPv6 ip;
        xyu::uint32 scope = 0;

        constexpr explicit AddressIPv6(_Port port) noexcept : AddressIPv6("::1", port) {}
        constexpr AddressIPv6(_IPv6 ip, _Port port) noexcept : Address{AF_ipv6}, ip(ip), port(port) {}
        constexpr AddressIPv6(const char* p) noexcept : AddressIPv6(xyu::StringView{p}) {}
        constexpr AddressIPv6(xyu::StringView saddr = "::1:0") noexcept : Address{AF_ipv6} { parse(saddr); }
        constexpr AddressIPv6& operator=(xyu::StringView saddr) noexcept { parse(saddr); return *this; }

        static constexpr bool valid(xyu::StringView saddr) noexcept
        {
            if (XY_UNLIKELY(saddr.size() < 3 || saddr[0] != '[')) return false;
            xyu::size_t over = saddr.find(']');
            if (XY_UNLIKELY(over == -1)) return false;
            if (XY_UNLIKELY(over + 1 >= saddr.size() || saddr[over + 1] != ':')) return false;
            int port = 0;
            for (auto c : saddr.subview(over + 2).range())
            {
                if (c >= '0' && c <= '9')
                {
                    port = port * 10 + c - '0';
                    if (XY_UNLIKELY(port > 65535)) return false;
                }
                else return false;
            }
            return _IPv6::valid(saddr.subview(1, over - 1));
        }

        constexpr void parse(xyu::StringView saddr) noexcept
        {
            if constexpr (XY_DEBUG) if (!valid(saddr))
                xyu::E_Logic::abort_invalid_string(XY_CurPos("AddressIPv6::parse"), saddr.data(), saddr.size(), "address_ipv6");

            xyu::size_t over = saddr.find(']');
            ip.parse(saddr.subview(1, over - 1));
            int nport = 0;
            for (auto c : saddr.subview(over + 2).range()) nport = nport * 10 + c - '0';
            port = nport;
        }

        operator xyu::String() const noexcept;
    };
}

/// 格式化
namespace xylu::xystring
{
    template <>
    struct FormatImpl<xyu::Socket::_Port> : FormatImpl<xyu::uint16> {};
    template <>
    struct FormatImpl<xyu::Socket::_IPv4> : FormatImpl<String> {};
    template <>
    struct FormatImpl<xyu::Socket::AddressIPv4> : FormatImpl<String> {};
    template <>
    struct FormatImpl<xyu::Socket::_IPv6> : FormatImpl<String> {};
    template <>
    struct FormatImpl<xyu::Socket::AddressIPv6> : FormatImpl<String> {};

}
namespace xylu::xyconn
{
    inline Socket::AddressIPv4::operator xyu::String() const noexcept
    {
        return xyfmtraw(xyu::String{21}, "{}:{}", ip, (xyu::uint16)port);
    }
    inline Socket::AddressIPv6::operator xyu::String() const noexcept
    {
        return xyfmtraw(xyu::String{47}, "[{}]:{}", ip, (xyu::uint16)port);
    }
}
