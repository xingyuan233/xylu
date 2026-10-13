#pragma once

/**
 * @file    log.h
 * @brief   日志系统
 * @date    2026-04-16
 */

#include "../xysystem/file.h"

#if XY_THREAD
#include "../xyconc/thread.h"
#include "../xyconc/atomic.h"
#endif

/// 日志类
namespace xylu::xycore
{
    /// 辅助工具 (禁止其他地方使用)
    namespace __
    {
        // 日志文件
        struct LogFile : xyu::File
        {
        private:
            N_Log_Level lv;     // 写入级别
        public:
            // 构造函数
            LogFile(File&& file, N_Log_Level level) noexcept : File{xyu::move(file)}, lv{level} {}
            // 获取写入级别
            N_Log_Level level() const noexcept { return lv; }
        };

        // 存储日志文件
        struct LogStack
        {
        private:
            LogFile* data;  // 文件数组
            uint32 size;    // 元素数量
            uint32 capa;    // 容量

        public:
            // 构造
            LogStack() noexcept : data{nullptr}, size{0}, capa{0} {}
            // 析构
            ~LogStack() noexcept;

            // 增删
            void push(xyu::File& file, N_Log_Level level) noexcept;
            void pop(xyu::File* p) noexcept;

            // 访问
            uint32 count() const noexcept { return size; }
            LogFile* begin() noexcept { return data; }
            LogFile* end() noexcept { return data + size; }
        };

        // 日志字符串
        struct LogString
        {
        private:
            char* p;        // 字符串首地址 (不以 '\0' 结尾)
            size_t len;     // 字符串长度
            size_t capa;    // 字符串容量

        public:
            // 构造
            LogString() noexcept : p{nullptr}, len{0}, capa{0} {}
            // 析构
            ~LogString() noexcept { if (p) xymemory::__::under_dealloc(p); }
            // 移动构造
            LogString(LogString&& other) noexcept : p{other.p}, len{other.len}, capa{other.capa} { other = {}; }

            // 大小
            size_t size() const noexcept { return len; }
            // 数据
            char* data() noexcept { return p; }
            // 转换为视图
            operator xyu::StringView() const noexcept { return {XY_LIKELY(p) ? p : "", len}; }

            // 清空
            void clear() noexcept { len = 0; }
            // 确保容量
            bool reserve(size_t mincapa) noexcept;

            // 写入
            void write(char c, size_t n = 1) noexcept;
            void write(xyu::StringView str) noexcept;

        private:
            LogString& operator=(const LogString&) noexcept = default;
        };
    }

    /// 多线程辅助工具 (禁止其他地方使用)
#if XY_THREAD
    namespace __
    {
        // 消息
        struct LogInfo
        {
            enum Type { None, Log, FilePush, FilePop };
            Type type;        // 消息类型
            union
            {
                struct {
                    N_Log_Level level;  // 日志等级
                    LogString mess;     // 日志信息
                } log;
                struct {
                    N_Log_Level level;  // 写入等级
                    xyu::File file;     // 增加的日志文件
                } fpush;
                struct {
                    xyu::File* fp;      // 移出日志文件
                } fpop;
                struct {} empty;        // 无消息
            };

            // 构造函数 - 日志
            LogInfo(N_Log_Level lv, LogString&& mess) noexcept : type(Log), log{lv, xyu::move(mess)} {}
            // 构造函数 - 增加文件
            LogInfo(xyu::File&& file, N_Log_Level lv) noexcept : type(FilePush), fpush{lv, xyu::move(file)} {}
            // 构造函数 - 删除文件
            explicit LogInfo(xyu::File* fp) noexcept : type(FilePop), fpop{fp} {}

            // 默认构造函数
            explicit LogInfo() noexcept : type(None) {}
            // 析构函数 (有效数据已转移)
            ~LogInfo() noexcept {}

            // 移动构造
            LogInfo(LogInfo&& other) noexcept : type(other.type)
            {
                switch (type)
                {
                    case Log: new (&log) decltype(log){xyu::move(other.log)}; break;
                    case FilePush: new (&fpush) decltype(fpush){xyu::move(other.fpush)}; break;
                    case FilePop: new (&fpop) decltype(fpop){xyu::move(other.fpop)}; break;
                    default:;
                }
            }

            // 显式释放
            void release() noexcept
            {
                if (type == Log) log.mess.~LogString();
                else if (type == FilePush) fpush.file.~File();
            }
        };

        // 无锁队列 - 单读多写
        struct MPSCQueue : class_no_copy_move_t
        {
            struct alignas(K_Cache_Line_Size)
            Node { xyu::Atomic<Node*> next; LogInfo data; };
        private:
            xyu::Atomic<Node*> tail;        // 队尾指针
            char _pad[K_Cache_Line_Size - sizeof(tail)];
            Node* lead;                     // 哨兵节点
            Node* del;                      // 延迟删除节点

        public:
            // 构造
            MPSCQueue() noexcept;
            // 析构
            ~MPSCQueue() noexcept;

            // 入队
            bool push(LogInfo& val) noexcept;
            // 出队
            LogInfo pop(bool* empty) noexcept;

            // 是否为空
            bool empty() const noexcept { return !lead->next; }
        };
    }
#endif

    /**
     * @brief 一个高性能、线程安全的日志记录类。
     * @details
     *   `Logger` 提供了一个统一的接口来记录不同级别的日志消息。
     *   它支持将日志输出到多个文件（包括标准输出），并可以为每个文件设置不同的日志级别阈值。
     *
     *   工作模式:
     *   - 单线程模式 (XY_THREAD 为 0):
     *     `log()` 函数会同步地将格式化后的消息直接写入所有符合级别的日志文件中。
     *
     *   - 多线程模式 (XY_THREAD 为 1):
     *     `log()` 函数会将格式化后的消息放入一个无锁队列中，然后立即返回。
     *     一个专用的后台线程会负责从队列中取出消息，并将其异步地写入文件。
     *     这种设计将 I/O 操作与业务逻辑分离，最大限度地减少了日志记录对业务线程性能的影响。
     *
     *   注意事项：
     *   - 文件上限：文件数量超过 2^27 会触发 abort
     *   - 内存不足：分配内存失败时会静默丢弃新增文件与日志
     */
    class Logger : class_no_copy_move_t
    {
    protected:
        enum FLAG : int
        {
            NOTICE  = 1 << 0,       // 是否有新消息
            OVER    = 1 << 1,       // 是否结束
            FLUSH   = 1 << 2,       // 是否实时刷新
        };

        xyu::Atomic<int> flag;      // 同步标志
#if XY_THREAD
        alignas(K_Cache_Line_Size)
        __::MPSCQueue infos;        // 消息队列
        __::LogStack files;         // 日志文件
        xyu::Thread worker;         // 工作线程
#else
        __::LogStack files;         // 日志文件
        __::LogString info;         // 日志消息缓存
#endif

    public:
        /**
         * @brief 构造函数，初始化日志类
         * @param add_stdout 是否添加标准输出文件到日志文件列表中 (默认 true)
         * @param level 标准输出文件写入级别 (默认 ALL)
         */
        explicit Logger(bool add_stdout = true, N_Log_Level level = N_Log_Level::All) noexcept;

#if XY_THREAD
        /// 析构函数，结束日志写入线程
        ~Logger() noexcept { flag |= OVER; flag.notify_one(); }
#endif

        /* 日志设置 */

        /// 设置是否输出日志后立即刷新
        void set_flush(bool flush) noexcept { flush ? flag |= FLUSH : flag &= ~FLUSH; }

        /* 增删日志文件 */

        /**
         * @brief 添加日志文件
         * @param file 日志文件对象 (必须已经打开且可写)
         * @param level 日志文件写入级别 (默认为 ALL)
         * @attention 操作会转移所有权，且 file 上限为 2^27 个
         */
        void push_file(xyu::File&& file, N_Log_Level level = N_Log_Level::All) noexcept;

        /**
         * @brief 删除最新添加的日志文件
         * @param fp 如果提供不会关闭文件，而是转移所有权到 fp 中
         */
        void pop_file(xyu::File* fp = nullptr) noexcept;

        /* 输出日志 */

        /**
         * @brief 输出日志信息
         * @param level 日志级别
         * @param mess 信息
         */
        void log(N_Log_Level level, xyu::StringView mess) noexcept;
        
    protected:
        // 直接输出日志
#if XY_THREAD
        void log_native(N_Log_Level level, __::LogString&& mess) noexcept;
#else
        void log_native(N_Log_Level level, xyu::StringView mess) noexcept;
#endif

    private:
        // 写入日志文件
        void write_log(N_Log_Level level, xyu::StringView mess, bool flush) noexcept;

#if XY_THREAD
        // 工作
        static void work(void* arg) noexcept;
#endif

    public:
        static constexpr char slevel[][6] = { "NONE ", "FATAL", "ERROR", "WARN ", "INFO ", "DEBUG", "TRACE", " ALL " };
    };
}

/// 全局日志
namespace xylu::xycore
{
    namespace __
    {
        // 全局日志
        struct GLogger : Logger
        {
            // 析构函数 (检查内存泄漏并输出日志)
            ~GLogger() noexcept;
        };

        // 提供外部访问的 protect 函数
        struct GLogger_Inner : GLogger
        {
            // 直接输出函数
            using Logger::log_native;
#if !XY_THREAD
            // 获取缓冲区
            LogString& tmp() noexcept { return info; }
#endif
        };

        GLogger_Inner& glogger() noexcept;
        bool glogger_inited() noexcept;
    }

    // 全局日志
    inline __::GLogger& xylogger() noexcept { return __::glogger(); }
}
