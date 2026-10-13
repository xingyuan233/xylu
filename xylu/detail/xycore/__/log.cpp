#include "../log.h"
#include "../../xystring/fmt.h"

/// 日志文件栈
namespace xylu::xycore::__
{
    // 析构
    LogStack::~LogStack() noexcept
    {
        if (XY_UNLIKELY(!data)) return;
        for (uint32 i = 0; i < size; ++i) { data[i].~LogFile(); }
        xymemory::__::under_dealloc(data);
    }

    // 扩容
    static bool LogStack_reserve(uint32& capa, LogFile*& data) noexcept
    {
        uint32 newcapa = capa * 2;
        constexpr uint32 maxcapa = xyu::bit_floor(xyu::t_number<uint32>::max / sizeof(LogFile));
        if (XY_UNLIKELY(newcapa > maxcapa))
            E_Logic::abort_over_limit(XY_CurPos(nullptr, "xyu::Logger::LogStack::fix"), capa, capa, maxcapa, "newcapa");
        else if (XY_UNLIKELY(newcapa == 0)) newcapa = 4;
        // 资源转移
        auto newdata = (LogFile*)xymemory::__::under_realloc(data, newcapa * sizeof(LogFile));
        if (XY_UNLIKELY(!newdata)) return false;
        data = newdata;
        capa = newcapa;
        return true;
    }

    // 入栈
    void LogStack::push(xyu::File& file, N_Log_Level level) noexcept
    {
        if (XY_UNLIKELY(size >= capa) && XY_UNLIKELY(!LogStack_reserve(capa, data))) return;
        new (data+size++) LogFile{xyu::move(file), level};
    }

    // 出栈
    void LogStack::pop(xyu::File* p) noexcept
    {
        if (XY_UNLIKELY(!size)) return;
        --size;
        if (p) *p = xyu::move(static_cast<xyu::File&>(data[size]));
        else data[size].~LogFile();
    }
}

/// 日志字符串
namespace xylu::xycore::__
{
    // 确保容量
    bool LogString::reserve(size_t mincapa) noexcept
    {
        if (mincapa <= capa) return true;
        size_t newcapa = (mincapa + K_Default_Align) & -K_Default_Align;
        // 分配新内存
        char* newp = (char*)xymemory::__::under_realloc(p, newcapa);
        if (XY_LIKELY(newp)) { p = newp; capa = newcapa; }
        // 内存不足，放弃记录日志，防止崩溃
        else if (p) { xymemory::__::under_dealloc(p); *this = {};  }
        return newp;
    }

    // 写入
    void LogString::write(char c, size_t n) noexcept
    {
        if (XY_UNLIKELY(!reserve(size() + n))) return;
        xyu::mem_set(p + len, c, n);
        len += n;
    }

    void LogString::write(xyu::StringView str) noexcept
    {
        if (XY_UNLIKELY(!reserve(size() + str.size()))) return;
        xyu::mem_copy(p + len, str.data(), str.size());
        len += str.size();
    }
}

#if XY_THREAD
/// 无锁队列
namespace xylu::xycore::__
{
    // 构造
    MPSCQueue::MPSCQueue() noexcept : del{}
    {
        lead = (Node*)xymemory::__::under_alalloc(sizeof(Node), alignof(Node));
        if (XY_UNLIKELY(!lead)) xymemory::__::abort_alloc_failed(sizeof(Node), 1, alignof(Node));
        lead->next = nullptr;
        new (&lead->data) LogInfo{};
        tail.store(lead, N_Atomic_Order::Relaxed);
    }

    // 析构
    MPSCQueue::~MPSCQueue() noexcept
    {
        if (del)
        {
            del->data.release();
            xymemory::__::under_dealalloc(del);
        }
        for (Node* cur = lead; cur;)
        {
            Node* next = cur->next.load(N_Atomic_Order::Relaxed);
            cur->data.release();
            xymemory::__::under_dealalloc(cur);
            cur = next;
        }
    }

    // 入队
    bool MPSCQueue::push(LogInfo& val) noexcept
    {
        auto node = (Node*)xymemory::__::under_alalloc(sizeof(Node), alignof(Node));
        if (XY_UNLIKELY(!node)) return false;
        new (node) Node { nullptr, xyu::move(val) };
        Node* tn = tail.exchange(node);
        tn->next = node;
        return true;
    }

    // 出队
    LogInfo MPSCQueue::pop(bool* empty) noexcept
    {
        Node* node = lead->next;
        if (!node)
        {
            *empty = true;
            return LogInfo{};
        }
        if (del) xymemory::__::under_dealalloc(del);
        del = lead;
        lead = node;
        *empty = false;
        return xyu::move(node->data);
    }
}
#endif

/// 日志
namespace xylu::xycore
{
    Logger::Logger(bool add_stdout, N_Log_Level level) noexcept : flag{0}
    {
        if (add_stdout) { auto f = xyu::File::fout(); files.push(f, level); }
        if constexpr (XY_DEBUG) flag |= FLUSH;
#if XY_THREAD
        flag.notify_one();  // 预调用，对于平台原生不支持的有锁退化实现保证其先初始化
        worker.create(work, this);
#endif
    }

    void Logger::push_file(xyu::File&& file, N_Log_Level level) noexcept
    {
#if XY_THREAD
        __::LogInfo info{xyu::move(file), level};
        if (XY_LIKELY(infos.push(info))) { flag |= NOTICE; flag.notify_one(); }
        else file = xyu::move(info.fpush.file);
#else
        files.push(file, level);
#endif
    }

    void Logger::pop_file(xyu::File* fp) noexcept
    {
#if XY_THREAD
        __::LogInfo info{fp};
        if (XY_LIKELY(infos.push(info))) { flag |= NOTICE; flag.notify_one(); }
#else
        files.pop(fp);
#endif
    }

    void Logger::log(N_Log_Level level, xyu::StringView mess) noexcept
    {
#if XY_THREAD
        __::LogString info;
#else
        info.clear();
#endif
        auto d = xyu::Duration_utc();
        auto c = xyu::Calendar{}.from_epoch_duration(d).to_sys();
#if XY_THREAD
        //多线程输出日志前缀 如:"[2021-08-10 10:10:10.123456] [0x12345678] [INFO ]: "
        xyfmtto(info, "[{:%C}.{|0-6}] [0x{|0-8:~}] [{}]: {}\n", c, d.us(), xyu::Thread::current(), slevel[(int)level], mess);
#else
        //单线程输出日志前缀 如:"[2021-08-10 10:10:10.123456] [INFO ]: "
        xyfmtto(info, "[{:%C}.{|0-6}] [{}]: {}\n", c, d.us(), slevel[(int)level], mess);
#endif
        log_native(level, xyu::move(info));
    }

#if XY_THREAD
    void Logger::log_native(N_Log_Level level, __::LogString&& mess) noexcept
#else
    void Logger::log_native(N_Log_Level level, xyu::StringView mess) noexcept
#endif
    {
        if (XY_UNLIKELY(level == N_Log_Level::None)) return;
#if XY_THREAD
        if (XY_UNLIKELY(!mess.size())) return;
        __::LogInfo info{level, xyu::move(mess)};
        if (XY_LIKELY(infos.push(info))) { flag |= NOTICE; flag.notify_one(); }
        else info.log.mess.~LogString();
#else
        write_log(level, mess, flag & FLUSH);
#endif
    }

    void Logger::write_log(N_Log_Level level, xyu::StringView mess, bool flush) noexcept
    {
        ES_File state;
        for (auto& file : files)
        {
            if (file.level() >= level)
            {
                if constexpr (XY_DEBUG)
                {
                    static_assert(sizeof(void*) == sizeof(xyu::File));
                    bool xystdout = *(void**)&xyu::xyout == *(void**)&file;
                    if (xystdout)
                    {
                        __::LogString tmp;
                        static constexpr xyu::StringView clevel[] = { "0", "41;97", "91", "93", "32", "36", "90", "0" };
# if XY_THREAD
                        //多线程输出日志前缀 如:"[2021-08-10 10:10:10.123456] [0x12345678] [INFO ]: "
                        xyfmtto(tmp, "\033[90m{} \033[32m{} \033[{}m[{}]\033[0m: {}",
                                mess.subview(0,28), mess.subview(29,12), clevel[(int)level], slevel[(int)level], mess.subview(51));
# else
                        //单线程输出日志前缀 如:"[2021-08-10 10:10:10.123456] [INFO ]: "
                        xyfmtto(tmp, "\033[90m{} \033[{}m[{}]\033[0m: {}",
                                mess.subview(0,28), clevel[(int)level], slevel[(int)level], mess.subview(38));
# endif
                        file.write(tmp, state);
                        file.flush(state);
                        continue;
                    }
                }
                file.write(mess, state);
                if (flush) file.flush(state);
            }
        }
    }

#if XY_THREAD
    void Logger::work(void* arg) noexcept
    {
        auto& logger = *static_cast<Logger*>(arg);
        auto& infos = logger.infos;
        for(;;)
        {
            // 等待任务或结束
            logger.flag.wait([](int f) -> bool { return f & (NOTICE | OVER); });
            int flag = logger.flag;
            if (flag & NOTICE) logger.flag &= ~NOTICE;
            // 处理消息
            bool empty;
            for (;;)
            {
                auto info = infos.pop(&empty);
                if (empty) break;
                switch (info.type)
                {
                    using Type = __::LogInfo::Type;
                case Type::Log:
                    logger.write_log(info.log.level, info.log.mess, flag & FLUSH);
                    info.log.mess.~LogString();
                    break;
                case Type::FilePush:
                    logger.files.push(info.fpush.file, info.fpush.level);
                    info.fpush.file.~File();
                    break;
                case Type::FilePop:
                    logger.files.pop(info.fpop.fp);
                    break;
                default:;
                }
            }
            // 结束
            if (flag & OVER) return;
        }
    }
#endif
}

/// 全局日志
namespace xylu::xycore::__
{
    static int is_glogger_inited = false;

    bool glogger_inited() noexcept { return is_glogger_inited; }

    GLogger_Inner& glogger() noexcept
    {
        static GLogger_Inner logger;
        is_glogger_inited = true;
        return logger;
    }
}
