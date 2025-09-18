#pragma clang diagnostic push
#pragma ide diagnostic ignored "OCUnusedGlobalDeclarationInspection"
#pragma ide diagnostic ignored "OCUnusedMacroInspection"
#pragma ide diagnostic ignored "NotInitializedField"
#pragma ide diagnostic ignored "cppcoreguidelines-pro-type-member-init"
#pragma ide diagnostic ignored "hicpp-exception-baseclass"
#pragma ide diagnostic ignored "modernize-use-nodiscard"
#pragma once

#include "../../link/file"
#include "../../link/time"
#include "../../link/atomic"

#if !XY_UNTHREAD
#include "../../link/thread"
#include "../../link/condvar"
#endif

/* 日志 */

namespace xylu::xycore
{
    // 工具类
    namespace __
    {
        struct StackCapaLmt {};

        /**
         * @brief 小容量栈数组
         * @note 不依赖于 xyu::alloc, xyu::dealloc, 避免递归调用
         * @note 容量上限为 1024，超过上限则抛出 StackCapaLmt 异常
         * @note T 不能有 指向自身的指针成员，或 虚函数,虚基类
         */
        template <typename T>
        struct SStack : xyu::class_no_copy_move_t
        {
        private:
            xyu::uint16 size;               // 元素数量
            xyu::uint16 capa;               // 容量
            T* data;                        // 元素数组指针

        public:
            // 构造
            SStack() noexcept : data{nullptr}, size{0}, capa{0} {}
            // 析构
            ~SStack() noexcept
            {
                if (XY_UNLIKELY(!data)) return;
                for (xyu::uint16 i = 0; i < size; ++i) { data[i].~T(); }
                xylu::xymemory::__::under_dealloc(data);
            }

            // 复制
            SStack(const SStack& other) : size{other.size}, capa{other.capa}
            {
                if (XY_UNLIKELY(!other.data)) { data = nullptr; return; }
                data = (T*)xylu::xymemory::__::under_alloc(capa * sizeof(T));
                xyu::mem_copy(data, other.data, size * sizeof(T));
            }

            // 操作
            template <typename... Args>
            void push(Args&&... args)
            {
                fix();
                ::new (data+size++) T(xyu::forward<Args>(args)...);
            }
            void pop() noexcept
            {
                if (XY_UNLIKELY(!size)) return;
                data[--size].~T();
            }

            // 访问
            xyu::uint count() const noexcept { return size; }
            T* begin() noexcept { return data; }
            T* end() noexcept { return data + size; }

        private:
            // 扩容 (保证至少还能容纳一个元素)
            void fix()
            {
                if (XY_UNLIKELY(size == capa))
                {
                    if (XY_UNLIKELY(capa == 0))
                    {
                        capa = 16;
                        data = (T*)xylu::xymemory::__::under_alloc(capa * sizeof(T));
                    }
                    else
                    {
                        if (XY_UNLIKELY(capa >= 1024)) throw StackCapaLmt{};
                        xyu::uint16 newcapa = capa * 2;
                        T* newdata = (T*)xylu::xymemory::__::under_alloc(newcapa * sizeof(T));
                        // mem_copy 后直接舍弃原来的内存，做资源的转移，不再使用析构
                        xyu::mem_copy(newdata, data, size * sizeof(T));
                        xylu::xymemory::__::under_dealloc(data);
                        data = newdata;
                        capa = newcapa;
                    }
                }
            }
        };

        /**
         * @brief 日志消息堆缓冲区
         * @note 不依赖于 xyu::alloc, xyu::dealloc, 避免递归调用
         * @note 作为 StringView 使用保留长度信息，不需要 '\0' 结尾
         */
        struct LString : xyu::StringBase<LString>, xyu::class_no_copy_t
        {
            char* p;            // 指向字符串首地址
            xyu::size_t len;    // 字符串长度 (不使用 '\0' 结尾)
            xyu::size_t capa;   // 字符串容量 (不使用 '\0' 结尾)

            // 构造
            LString() noexcept : p{nullptr}, len{0}, capa{0} {}
            // 析构
            ~LString() noexcept { if (p) xylu::xymemory::__::under_dealloc(p); }

            // 移动构造
            LString(LString&& other) noexcept : p{other.p}, len{other.len}, capa{other.capa} { other.p = nullptr; other.len = 0; other.capa = 0; }
            // 移动赋值
            LString& operator=(LString&& other) noexcept { xyu::swap(p, other.p); xyu::swap(len, other.len); xyu::swap(capa, other.capa); return *this; }

            // 大小
            xyu::size_t size() const noexcept { return len; }
            // 数据
            char* data() const noexcept { return p; }

            // 确保容量
            void reserve(xyu::size_t mincapa)
            {
                if (mincapa <= capa) return;
                // 超出容量上限
                auto limit = xyu::String::limit() + 1;
                if (XY_UNLIKELY(mincapa > limit)) throw xyu::E_Memory_Capacity{};
                // 新容量 = max(最小容量, 旧容量 * K_grow_factor) --> align
                xyu::size_t newcapa = xyu::max(mincapa, static_cast<xyu::size_t>(static_cast<double>(capa) * 1.5));
                if (XY_UNLIKELY(newcapa > limit)) newcapa = limit;
                else newcapa = (newcapa + xyu::K_DEFAULT_ALIGN) & -xyu::K_DEFAULT_ALIGN;
                // 分配新内存
                char* newp = (char*)xylu::xymemory::__::under_alloc(newcapa);
                xyu::mem_copy(newp, p, len);
                if (p) xylu::xymemory::__::under_dealloc(p);
                p = newp;
                capa = newcapa;
            }
            // 清空
            void clear() noexcept { len = 0; }

            // 写入
            void operator << (char c)
            {
                reserve(size() + 1);
                p[len++] = c;
            }
            void operator << (const xyu::StringView& str)
            {
                reserve(size() + str.size());
                xyu::mem_copy(p + len, str.data(), str.size());
                len += str.size();
            }
        };

        /**
         * @brief 日志消息栈缓冲区
         * @tparam N 栈容量
         * @note 自行确保写入长度不超过站容量，不做容量检查
         * @note 作为 StringView 使用保留长度信息，不需要 '\0' 结尾
         */
        template <size_t N>
        struct SString : xyu::StringBase<SString<N>>
        {
            char buf[N];    // 缓冲区 (对于此处的需求来说大小充足)
            xyu::uint s;    // 字符串长度 (不使用 '\0' 结尾)

            // 构造函数
            explicit SString() noexcept : s{0} {}
            // 获取缓冲区
            const char* data() const noexcept { return buf; }
            // 获取缓冲区大小
            xyu::size_t size() const noexcept { return s; }
            // 添加数据
            void operator<<(char ch) noexcept { buf[s++] = ch; }
            void operator<<(const xyu::StringView& str) noexcept
            {
                xyu::mem_copy(buf + s, str.data(), str.size());
                s += str.size();
            }
        };

        /**
         * @brief 日志文件
         * @note 继承自 xyu::File，增加写入级别信息
         */
        struct File_Log : xyu::File
        {
        private:
            // 写入级别
            xyu::N_LOG_LEVEL lv;
        public:
            // 构造函数
            using xyu::File::File;
            File_Log(xyu::File&& file, xyu::N_LOG_LEVEL level) noexcept : xyu::File{xyu::move(file)}, lv{level} {}
            // 获取写入级别
            xyu::N_LOG_LEVEL level() const noexcept { return lv; }
        };
    }

#if !XY_UNTHREAD
    // 多线程工具类
    namespace __
    {
        /**
         * @brief 多修改单遍历栈
         * @note 不依赖于 xyu::alloc, xyu::dealloc, 避免递归调用
         * @note T 不能有 指向自身的指针成员，或 虚函数,虚基类
         * @note push/pop 因低频使用，效率较低
         * @note get/done 用于遍历栈，效率较高
         */
        template <typename T>
        struct MStack : xyu::class_no_copy_move_t
        {
        private:
            alignas(xyu::K_CACHE_LINE_SIZE) xyu::Atomic<SStack<T>*> data;   // 栈数组指针
            alignas(xyu::K_CACHE_LINE_SIZE) xyu::Atomic<SStack<T>*> use;    // 正在读取的栈指针
            SStack<T>* del;                                                 // 延迟删除栈指针
            xyu::Mutex m;                                                   // 互斥锁

        public:
            // 构造
            MStack() : data{nullptr}, use{nullptr}, del{nullptr} {}
            // 析构
            ~MStack() noexcept
            {
                if (del) {
                    del->~SStack<T>();
                    xylu::xymemory::__::under_dealloc(del);
                }
                if (auto p = data.load(xyu::N_ATOMIC_RELAXED); XY_LIKELY(p != nullptr)) {
                    p->~SStack<T>();
                    xylu::xymemory::__::under_dealloc(p);
                }
            }

            // 入栈
            template <typename... Args>
            void push(Args&&... args)
            {
                auto g = m.guard();
                SStack<T>* p = data.load(xyu::N_ATOMIC_ACQUIRE);
                auto* newp = (SStack<T>*)xylu::xymemory::__::under_alloc(sizeof(SStack<T>));
                new (newp) SStack<T>(XY_LIKELY(p != nullptr) ? *p : SStack<T>{});
                newp->push(xyu::forward<Args>(args)...);
                data.store(newp, xyu::N_ATOMIC_RELEASE);
                SStack<T>* usep = use.load(xyu::N_ATOMIC_ACQUIRE);
                if (del && usep != del) {
                    del->~SStack<T>();
                    xylu::xymemory::__::under_dealloc(del);
                    del = nullptr;
                }
                if (XY_LIKELY(p != nullptr) && usep != p) {
                    p->~SStack<T>();
                    xylu::xymemory::__::under_dealloc(p);
                }
                else del = p;
            }
            // 出栈
            void pop()
            {
                auto g = m.guard();
                SStack<T>* p = data.load(xyu::N_ATOMIC_ACQUIRE);
                auto* newp = (SStack<T>*)xylu::xymemory::__::under_alloc(sizeof(SStack<T>));
                new (newp) SStack<T>(XY_LIKELY(p != nullptr) ? *p : SStack<T>{});
                if (XY_UNLIKELY(newp->count() == 0)) return;
                newp->pop();
                data.store(newp, xyu::N_ATOMIC_RELEASE);
                SStack<T>* usep = use.load(xyu::N_ATOMIC_ACQUIRE);
                if (del && usep != del) {
                    del->~SStack<T>();
                    xylu::xymemory::__::under_dealloc(del);
                    del = nullptr;
                }
                if (XY_LIKELY(p != nullptr) && usep != p) {
                    p->~SStack<T>();
                    xylu::xymemory::__::under_dealloc(p);
                }
                else del = p;
            }

            // 获取栈
            SStack<T>& get() noexcept
            {
                SStack<T>* p = data.load(xyu::N_ATOMIC_ACQUIRE);
                use.store(p, xyu::N_ATOMIC_RELEASE);
                return *p;
            }
            // 遍历完毕
            void done() noexcept { use.store(nullptr, xyu::N_ATOMIC_RELEASE); }
        };

        /**
         * @brief 无锁队列 (多生产者单消费者)
         * @note 不依赖于 xyu::alloc, xyu::dealloc, 避免递归调用
         * @note T 必须提供默认构造函数，且默认构造作为 队列为空的判断
         * @note T 在移动构造后原对象不应该保留任何资源，即可平凡析构
         */
        template <typename T>
        struct MPSCQueue : xyu::class_no_copy_move_t
        {
        private:
            struct Node { xyu::Atomic<Node*> next; T data; };
            alignas(xyu::K_CACHE_LINE_SIZE) xyu::Atomic<Node*> tail;    // 队列尾指针
            alignas(xyu::K_CACHE_LINE_SIZE) Node* head;                 // 哨兵节点
            Node* del;                                                  // 延迟删除节点

        public:
            // 构造
            MPSCQueue() : tail{(Node*)(xylu::xymemory::__::under_alloc(sizeof(Node)))}, head{tail}, del{nullptr}
            {
                head->next.store(nullptr, xyu::N_ATOMIC_RELAXED);
            }
            // 析构
            ~MPSCQueue() noexcept
            {
                xylu::xymemory::__::under_dealloc(del);
                Node* p = head;
                while (p) {
                    Node* q = p->next.load(xyu::N_ATOMIC_RELAXED);
                    xylu::xymemory::__::under_dealloc(p);
                    p = q;
                }
            }

            // 判读是否为空
            bool empty() const noexcept { return head->next.load(xyu::N_ATOMIC_ACQUIRE) == nullptr; }

            // 入队 (移动构造)
            void push(T&& val)
            {
                Node* node = reinterpret_cast<Node*>(xylu::xymemory::__::under_alloc(sizeof(Node)));
                new (node) Node { nullptr, xyu::move(val) };
                Node* tn = tail.exchange(node, xyu::N_ATOMIC_ACQ_REL);
                tn->next.store(node, xyu::N_ATOMIC_RELEASE);
            }
            // 出队 (没元素时返回空值)
            T pop() noexcept
            {
                Node* node = head->next.load(xyu::N_ATOMIC_ACQUIRE);
                if (!node) return T{};
                T val = xyu::move(node->data);
                xylu::xymemory::__::under_dealloc(del);
                del = head;
                head = node;
                return val;
            }
        };
    }
#endif

    /**
     * @brief 一个高性能、线程安全的日志记录类。
     * @details
     *   `Logger` 提供了一个统一的接口来记录不同级别的日志消息。它支持将日志
     *   输出到多个文件（包括标准输出），并可以为每个文件设置不同的日志级别阈值。
     *
     *   ### 工作模式:
     *   - **单线程模式 (XY_UNTHREAD 为 1)**:
     *     `log()` 函数会同步地将格式化后的消息直接写入所有符合级别的日志文件中。
     *
     *   - **多线程模式 (默认)**:
     *     `log()` 函数会将格式化后的消息放入一个无锁队列中，然后立即返回。
     *     一个专用的后台线程会负责从队列中取出消息，并将其异步地写入文件。
     *     这种设计将 I/O 操作与业务逻辑分离，最大限度地减少了日志记录对业务线程性能的影响。
     */
    class Logger : xyu::class_no_copy_move_t
    {
    protected:
#if XY_UNTHREAD
        __::SStack<__::File_Log> files;         // 日志文件
        __::LString info;                       // 日志消息缓存
#else
        struct Info { __::LString mess; xyu::N_LOG_LEVEL level; };
        __::MStack<__::File_Log> files;         // 日志文件
        __::MPSCQueue<Info> infos;              // 日志消息队列
        xyu::Thread_Native th;                  // 日志写入线程
        xyu::Mutex m;                           // 互斥锁
        xyu::CondVar cv;                        // 条件变量
        xyu::Atomic<bool> over_flag = false;    // 结束标记
#endif
        xyu::Atomic<bool> flush_flag = true;    // 每次写入日志时是否刷新缓冲区 (默认为 true)
        xyu::Atomic<bool> error_flag = false;   // 是否发生错误

    public:
        /* 日志状态 */

        /**
         * @brief 构造函数，初始化日志类
         * @param add_stdout 是否添加标准输出文件到日志文件列表中 (默认 true)
         * @param level 标准输出文件写入级别 (默认 N_LOG_ALL)
         */
        explicit Logger(bool add_stdout = true, xyu::N_LOG_LEVEL level = N_LOG_ALL)
        {
            if (add_stdout) files.push(xyu::File::fout(), level);
#if !XY_UNTHREAD
            th.create(write_log, this);
#endif
        }

#if !XY_UNTHREAD
        // 析构函数，结束日志写入线程
        ~Logger() noexcept
        {
            //写入剩下日志信息
            if (!infos.empty()) cv.notify_one();
            //等待线程结束
            over_flag.store(true, xyu::N_ATOMIC_RELEASE);
            cv.notify_one();
            //th.join()自动调用
        }
#endif

        /**
         * @brief 是否发生错误
         * @note 但即使发生错误，也会尝试写入可能的错误信息
         */
        explicit operator bool() const noexcept { return error_flag.load(xyu::N_ATOMIC_ACQUIRE); }

        /* 日志设置 */

        /**
         * @brief 设置日志刷新标志
         * @param flag 日志刷新标志 (默认 true)
         * @note 每次写入日志时，若 flush_flag 为 true 则刷新缓冲区，否则不刷新
         */
        void set_flush(bool flag) noexcept { flush_flag.store(flag, xyu::N_ATOMIC_RELEASE); }

        /* 增删日志文件 */

        /**
         * @brief 添加日志文件
         * @param path 日志文件路径
         * @param level 日志文件写入级别 (默认 N_LOG_ALL)
         * @return 是否添加成功 (打开文件失败, 操作失败, 或超过存储上限[1024] 则失败)
         * @note 若 level 为 0(不会写入任何日志) 直接则返回 false
         */
        bool add_file(const xyu::StringView& path, xyu::N_LOG_LEVEL level = N_LOG_ALL) noexcept
        {
            xyu::File file{};
            try { file.open(path, xyu::File::APPEND); }
            catch (...) { return false; }
            return add_file(file, level);
        }

        /**
         * @brief 添加日志文件
         * @param path 日志文件路径
         * @param level 日志文件写入级别 (默认 N_LOG_ALL)
         * @return 是否添加成功 (打开文件失败, 操作失败, 或超过存储上限[1024] 则失败)
         * @note 若 level 为 0(不会写入任何日志) 直接则返回 false
         */
        bool add_file(const char* path, xyu::N_LOG_LEVEL level = N_LOG_ALL) noexcept
        {
            xyu::File file{};
            try { file.open(path, xyu::File::APPEND); }
            catch (...) { return false; }
            return add_file(file, level);
        }

        /**
         * @brief 添加日志文件
         * @param file 日志文件对象 (必须为已经打开的，且可写)
         * @param level 日志文件写入级别 (默认 N_LOG_ALL)
         * @return 是否添加成功 (内存分配失败, 或超过存储上限[1024] 则失败)
         * @note 若 level 为 0(不会写入任何日志) 直接则返回 false
         * @note !!操作会转移文件所有权!!
         */
        bool add_file(xyu::File& file, xyu::N_LOG_LEVEL level = N_LOG_ALL) noexcept
        {
            if (XY_UNLIKELY(level == N_LOG_NONE)) return false;
            try { files.push(xyu::move(file), level); }
            catch (__::StackCapaLmt&) { return false; }
            catch (...) { error_flag.store(true, xyu::N_ATOMIC_RELEASE); return false; }
            return true;
        }

        /**
         * @brief 添加日志文件
         * @param file 日志文件对象 (必须为已经打开的，且可写)
         * @param level 日志文件写入级别 (默认 N_LOG_ALL)
         * @return 是否添加成功 (内存分配失败, 或超过存储上限[32768] 则失败)
         * @note 若 level 为 0(不会写入任何日志) 或 超出范围[0, 7] 则返回 false
         * @note !!操作会转移文件所有权!!
         */
        bool add_file(xyu::File&& file, xyu::N_LOG_LEVEL level = N_LOG_ALL) noexcept { return add_file(file, level); }

        /**
         * @brief 删除最新添加的日志文件
         * @note 允许日志文件列表为空时安全调用
         */
        void pop_file() noexcept { files.pop(); }

        /* 输出日志 */

        /**
         * @brief 输出日志信息
         * @param level 日志级别
         * @param fmt 日志格式化字符串
         * @param args 日志格式化参数 (不提供时直接输出 fmt)
         * @return 是否输出成功 (内存分配失败, 或格式化信息失败，或文件写入失败[不包括无该等级日志文件情况]，返回 false)
         */
        template <typename... Args>
        void log(xyu::N_LOG_LEVEL level, const xyu::StringView& fmt, const Args&... args) noexcept
        {
            // 无效日志级别 (N_LOG_LEVEL为不写入任何日志)
            if (XY_UNLIKELY(level == N_LOG_NONE)) return;
#if XY_UNTHREAD
            info.clear();
#else
            __::LString info;
#endif
            // 格式化日志消息
            //单线程输出日志前缀 如:"[2021-08-10 10:10:10.123456] [INFO ]: "
            //多线程输出日志前缀 如:"[2021-08-10 10:10:10.123456] [INFO ] [0x00000001]: "
            try {
                auto nd = xyu::Duration_utc();
                auto c [[maybe_unused]] = xyu::Calendar().from_epoch_duration(nd+ xyu::Duration_min(xyu::K_TIME_DIFFERENCE * 60.));
#if XY_UNTHREAD
                xyfmtt(info, "[{:%C}.{|6}] [{}]: ", c, nd.us(), str_level[level]);
#else
                xyfmtt(info, "[{:%C}.{|6}] [{}] [0x{|0-8:~}]: ", c, nd.us(), str_level[level], xyu::Thread_Native::id());
#endif
                //提供参数则格式化日志消息
                if constexpr (sizeof...(Args) > 0)
                {
                    try { xyu::format_to(info, fmt, args...); }
                    catch (...) { return; }
                }
                //否则直接输出日志消息
                else info << fmt;
                //换行
                info << '\n';
            }
            catch (...) { error_flag.store(true, xyu::N_ATOMIC_RELEASE); return; }
            // 写入日志文件
#if XY_UNTHREAD
            for (auto& file : files) {
                if (file.level() >= level) {
                    try {
                        file.write(info);
                        if (flush_flag) file.flush();
                    }
                    catch (...) {}
                }
            }
#else
            infos.push({xyu::move(info), level});
            cv.notify_one();
#endif
        }

    public:
        /// 日志级别字符串
        static constexpr xyu::StringView str_level[] = { "NONE ", "FATAL", "ERROR", "WARN ", "INFO ", "DEBUG", "TRACE", " ALL " };

    private:
#if !XY_UNTHREAD
        // 日志写入线程
        static void write_log(void* arg) noexcept
        {
            Logger& logger = *static_cast<Logger*>(arg);
            auto& infos = logger.infos;
            for (;;) {
                // 检查是否结束
                if (logger.over_flag.load(xyu::N_ATOMIC_ACQUIRE)) break;
                // 等待
                try {
                    auto g = logger.m.guard();
                    logger.cv.wait(g);
                }
                // 失败则计时等待
                catch (...) {
                    try { xyu::Duration_s{1}.sleep(); }
                    catch (...) {}
                }
                // 输出日志
                for (;;) {
                    // 取出日志消息
                    Info info = infos.pop();
                    if (info.mess.empty()) break;
                    // 写入日志文件
                    auto& files = logger.files.get();
                    for (auto& file : files) {
                        if (file.level() >= info.level) {
                            try {
                                file.write(info.mess);
                                if (logger.flush_flag) file.flush();
                            }
                            catch (...) {}
                        }
                    }
                    logger.files.done();
                }
            }
        }
#endif
    };

    namespace __
    {
        struct GLogger : Logger
        {
            ~GLogger() noexcept
            {
#if XY_UNTHREAD
                // 释放全局内存池 (输出析构日志)
                xylu::xymemory::__::pool_release();
#else
                // 结束写入线程
                over_flag.store(true, xyu::N_ATOMIC_RELEASE);
                cv.notify_one();
                th.join();
                // 释放全局内存池 (输出析构日志)
                xylu::xymemory::__::pool_release();
                // 处理剩下日志信息
                for(auto& fs = files.get();;)
                {
                    Info info = infos.pop();
                    if (info.mess.empty()) return;
                    // 写入日志文件
                    for (auto& file : fs) {
                        if (file.level() >= info.level) {
                            try {
                                file.write(info.mess);
                                if (flush_flag.load(xyu::N_ATOMIC_RELAXED)) file.flush();
                            }
                            catch (...) {}
                        }
                    }
                }
#endif
            }
        };
    }
    // 全局变量 - 日志
    inline __::GLogger flog;
}

#pragma clang diagnostic pop