#pragma clang diagnostic push
#pragma ide diagnostic ignored "OCUnusedStructInspection"
#pragma ide diagnostic ignored "OCUnusedGlobalDeclarationInspection"
#pragma ide diagnostic ignored "modernize-use-nodiscard"
#pragma ide diagnostic ignored "UnreachableCallsOfFunction"
#pragma once

#include "../../link/format"

/* 文件 */

namespace xylu::xysystem
{
    /**
     * @brief 一个封装了底层文件句柄的 RAII 类，提供了统一的文件 I/O 操作接口。
     * @details
     *   `File` 类负责自动管理文件的打开和关闭，确保文件资源总能被正确释放。
     *   它支持普通文件、临时文件以及标准输入/输出/错误的访问。
     *
     * @note
     *   **`const` 对象的行为**: 一个 `const File` 对象代表一个“绑定的”文件句柄，
     *   你**仍然可以**通过它进行读写操作。`const` 语义在这里保证的是 `File` 对象
     *   本身不会被重新绑定到另一个文件（即不能调用 `open` 或 `close`），
     *   这对于确保自定义缓冲区的生命周期管理至关重要。
     */
    class File : xyu::class_no_copy_t
    {
    public:
        /// 文件打开模式
        enum OpenMode : xyu::uint
        {
            READ    = 1 << 0,           // 可读
            TRUNC   = 1 << 1,           // 可写+截断
            APPEND  = 1 << 2,           // 可写+追加
            WRITE   = TRUNC | APPEND,   // 可写+覆盖
            BINARY  = 1 << 3,           // 二进制模式 (不处理换行符)
            OWN     = 1 << 4,           // 独占仅读
        };
        /// 文件移动方式
        enum MoveMode
        {
            BEG = 0,   // 相对文件开始位置
            CUR = 1,   // 相对当前位置
            END = 2,   // 相对文件结束位置
        };
        /// 缓冲方式
        enum BufferMode
        {
            ALL,   // 全缓冲
            LINE,  // 行缓冲
            NONE,  // 无缓冲
        };

    private:
        void* h;    // 文件句柄

    public:
        /* 文件对象 */

        /**
         * @brief 默认构造函数
         */
        File() noexcept = default;

        /**
         * @brief 构造并打开文件
         * @param path 文件路径
         * @param mode 文件打开模式
         * @exception E_Logic_Null_Pointer path为 nullptr (DEBUG模式额外前置检查)
         * @exception E_Logic_Invalid_Argument mode无效
         * @exception E_File_*
         */
        File(const char* path, OpenMode mode) : File{} { open(path, mode); }

        /**
         * @brief 构造并打开文件
         * @param path 文件路径
         * @param mode 文件打开模式
         * @exception E_Logic_Invalid_Argument mode无效 (DEBUG模式额外前置检查)
         * @exception E_File_*
         */
        File(const xyu::StringView& path, OpenMode mode) : File{} { open(path, mode); }

        /**
         * @brief 移动构造函数
         * @param other 被移动的文件对象
         */
        File(File&& other) noexcept : h{other.h} { other.h = nullptr; }

        /**
         * @brief 析构函数
         * @note 自动关闭文件
         */
        ~File() noexcept { try{close();} catch(...){} }

        /* 文件打开与关闭 */

        /**
         * @brief 打开文件
         * @param path 文件路径
         * @param mode 文件打开模式
         * @note 若文件已打开，则先关闭原文件再打开新文件
         * @exception E_Logic_Null_Pointer path为 nullptr (DEBUG模式额外前置检查)
         * @exception E_Logic_Invalid_Argument mode无效
         * @exception E_File_*
         */
        void open(const char* path, OpenMode mode);

        /**
         * @brief 打开文件
         * @param path 文件路径
         * @param mode 文件打开模式
         * @note 若文件已打开，则先关闭原文件再打开新文件
         * @exception E_File_Open_Mode mode无效
         * @exception E_File_*
         */
        void open(const xyu::StringView& path, OpenMode mode)
        {
            if (XY_LIKELY(path.get(path.size()) == '\0')) return open(path.data(), mode);
            else {
                char buf[path.size() + 1];
                xyu::mem_copy(buf, path.data(), path.size());
                buf[path.size()] = '\0';
                open(buf, mode);
            }
        }

        /**
         * @brief 关闭文件
         * @note 文件未打开则不做处理
         * @exception E_File_*
         */
        void close();

        /**
         * @brief 判断文件是否为正常可读取状态
         * @return 如果文件已经打开且未到结尾，则返回 true，否则返回 false
         */
        explicit operator bool() const noexcept { return h && !eof(); }

        /* 文件读写 */

        /**
         * @brief 从文件读取 1 个字节 (若文件已到结尾则返回 -1，否则返回 0-255)
         * @exception E_Logic_Invalid_Argument 文件未打开 (DEBUG模式额外前置检查)
         * @exception E_File_*
         */
        int read() const;

        /**
         * @brief 从文件读取 bytes 个字节
         * @return 实际读取的字节数，若文件已到结尾则小于 bytes
         * @note 不会自动写入 '\0'
         * @exception E_Logic_Invalid_Argument 文件未打开 (DEBUG模式额外前置检查)
         * @exception E_Logic_Null_Pointer buf 为 nullptr (DEBUG模式额外前置检查)
         * @exception E_File_*
         */
        xyu::size_t read(char* buf, xyu::size_t bytes) const;

        /**
         * @brief 从文件读取 bytes 个字节
         * @return 实际读取的字节数，若文件已到结尾则小于 bytes
         * @note 追加到 app 中
         * @exception E_Logic_Invalid_Argument 文件未打开 (DEBUG模式额外前置检查)
         * @exception E_File_*
         */
        xyu::size_t read(xyu::String& app, xyu::size_t bytes) const
        {
            xyu::size_t start = app.size();
            app.reserve(start + bytes);
            xyu::size_t n = read(app.data() + start, xyu::min(app.capacity() - start, bytes));
            app.resize(start + n);
            return n;
        }

        /**
         * @brief 从文件读取 bytes 个字节
         * @return 返回 String，若文件已到结尾则其 size() 小于 bytes
         * @exception E_Logic_Invalid_Argument 文件未打开 (DEBUG模式额外前置检查)
         * @exception E_File_*
         */
        xyu::String read(xyu::size_t bytes) const { xyu::String str(bytes); read(str, bytes); return str; }

        /**
         * @brief 从文件读取到 over 字符结束 (不包括over)
         * @param buf 读取缓冲区
         * @param capa 读取缓冲区容量 (不包括'\0')
         * @param over 结束字符
         * @return 实际读取的字节数
         * @exception E_Logic_Invalid_Argument 文件未打开 (DEBUG模式额外前置检查)
         * @exception E_Logic_Null_Pointer buf 为 nullptr (DEBUG模式额外前置检查)
         * @exception E_File_*
         */
        xyu::size_t read(char* buf, xyu::size_t capa, char over) const;

        /**
         * @brief 从文件读取到 over 字符结束 (不包括over)
         * @param app 追加到 app 中
         * @param over 结束字符
         * @return 实际读取的字节数
         * @exception E_Logic_Invalid_Argument 文件未打开 (DEBUG模式额外前置检查)
         * @exception E_File_*
         */
        xyu::size_t read(xyu::String& app, char over) const;

        /**
         * @brief 从文件读取到 over 字符结束 (不包括over)
         * @param over 结束字符
         * @return 返回 String
         * @exception E_Logic_Invalid_Argument 文件未打开 (DEBUG模式额外前置检查)
         * @exception E_File_*
         */
        xyu::String read(char over) const { xyu::String str; read(str, over); return str; }

        /**
         * @brief 从文件读取一行 (不包括换行符)
         * @param buf 读取缓冲区
         * @param capa 读取缓冲区容量 (不包括'\0')
         * @return 实际读取的字节数
         * @exception E_Logic_Invalid_Argument 文件未打开 (DEBUG模式额外前置检查)
         * @exception E_Logic_Null_Pointer buf 为 nullptr (DEBUG模式额外前置检查)
         * @exception E_File_*
         */
        xyu::size_t read_line(char* buf, xyu::size_t capa) const { return read(buf, capa, '\n'); }

        /**
         * @brief 从文件读取一行 (不包括换行符)
         * @param app 追加到 app 中
         * @return 实际读取的字节数
         * @exception E_Logic_Invalid_Argument 文件未打开 (DEBUG模式额外前置检查)
         * @exception E_Logic_Null_Pointer buf 为 nullptr (DEBUG模式额外前置检查)
         * @exception E_File_*
         */
        xyu::size_t read_line(xyu::String& app) const { return read(app, '\n'); }

        /**
         * @brief 从文件读取一行 (不包括换行符)
         * @return 返回 String
         * @exception E_Logic_Invalid_Argument 文件未打开 (DEBUG模式额外前置检查)
         * @exception E_File_*
         */
        xyu::String read_line() const { return read('\n'); }

        /**
         * @brief 从文件读取全部内容
         * @param buf 读取缓冲区
         * @param capa 读取缓冲区容量 (不包括'\0')
         * @return 实际读取的字节数
         * @exception E_Logic_Invalid_Argument 文件未打开 (DEBUG模式额外前置检查)
         * @exception E_Logic_Null_Pointer buf 为 nullptr (DEBUG模式额外前置检查)
         * @exception E_File_*
         */
        xyu::size_t read_all(char* buf, xyu::size_t capa) const;

        /**
         * @brief 从文件读取全部内容
         * @param app 追加到 app 中
         * @return 实际读取的字节数
         * @exception E_Logic_Invalid_Argument 文件未打开 (DEBUG模式额外前置检查)
         * @exception E_File_*
         */
        xyu::size_t read_all(xyu::String& app) const;

        /**
         * @brief 从文件读取全部内容
         * @return 返回 String
         * @exception E_Logic_Invalid_Argument 文件未打开 (DEBUG模式额外前置检查)
         * @exception xx 文件读取失败
         */
        xyu::String read_all() const { xyu::String str; read_all(str); return str; }

        /**
         * @brief 文件是否到结尾
         * @exception E_Logic_Invalid_Argument 文件未打开 (DEBUG模式额外前置检查)
         */
        bool eof() const XY_NOEXCEPT_NDEBUG;

        /**
         * @brief 写入 1 个字节
         * @exception E_Logic_Invalid_Argument 文件未打开 (DEBUG模式额外前置检查)
         * @exception E_File_*
         */
        void write(char data) const;

        /**
         * @brief 写入字符串
         * @exception E_Logic_Invalid_Argument 文件未打开 (DEBUG模式额外前置检查)
         * @exception E_File_*
         */
        void write(const xyu::StringView& data) const;

        /**
         * @brief 写入字符串常量
         * @exception E_Logic_Invalid_Argument 文件未打开 (DEBUG模式额外前置检查)
         * @exception E_File_*
         */
        template <xyu::size_t N>
        void write(const char (&data) [N]) const { return write(xyu::StringView{data, N-1}); }

        /**
         * @brief 写入字符串 ('\0'结尾)
         * @return 实际写入的字节数
         * @exception E_Logic_Invalid_Argument 文件未打开 (DEBUG模式额外前置检查)
         * @exception E_Logic_Null_Pointer data 为 nullptr (DEBUG模式额外前置检查)
         * @exception E_File_*
         */
        xyu::size_t write(const char* data) const;

        /**
         * @brief 写入其他
         * @note 该类型必须支持 format() 方法
         * @exception E_Logic_Invalid_Argument 文件未打开 (DEBUG模式额外前置检查)
         * @exception E_File_*
         */
        template <typename T, typename = xyu::t_enable<!xyu::t_can_icast<T, xyu::StringView>>>
        xyu::size_t write(T&& data [[maybe_unused]]) const
        {
            xyu::String str = xyfmt("{}", data);
            write(str);
            return str.size();
        }

        /**
         * @brief 格式化写入字符串
         * @note 间接调用 format() 并写入
         * @note 如果 fmt 为字符串常量，考虑性能与安全时，推荐手动调用 xyfmt
         * @exception E_Logic_Invalid_Argument 文件未打开 (DEBUG模式额外前置检查)
         * @exception E_File_*
         */
        template <typename... Args>
        xyu::size_t write(const xyu::StringView& fmt, Args&&... args) const
        {
            xyu::String str = xyu::format(fmt, xyu::forward<Args>(args)...);
            write(str);
            return str.size();
        }

        /**
         * @brief 刷新缓冲区
         * @exception E_Logic_Invalid_Argument 文件未打开 (DEBUG模式额外前置检查)
         * @exception E_File_*
         */
        void flush() const;

        /* 文件指针位置 */

        /**
         * @brief 将文件指针移到开头
         * @exception E_Logic_Invalid_Argument 文件未打开 (DEBUG模式额外前置检查)
         * @exception E_File_*
         */
        void rewind() const;

        /**
         * @brief 获取文件位置
         * @exception E_Logic_Invalid_Argument 文件未打开 (DEBUG模式额外前置检查)
         * @exception E_File_*
         */
        xyu::size_t pos() const;

        /**
         * @brief 设置文件位置
         * @param pos 新的位置
         * @exception E_Logic_Invalid_Argument 文件未打开 (DEBUG模式额外前置检查)
         * @exception E_File_* 
         */
        void repos(xyu::size_t pos) const;

        /**
         * @brief 将文件指针移动 offset 个字节
         * @param mode 移动方式 (CUR[相对当前], BEG[相对开头,offset≥0], END[相对结尾,offset≤0])
         * @exception E_Logic_Invalid_Argument 文件未打开 (DEBUG模式额外前置检查)
         * @exception E_Logic_Invalid_Argument mode无效 或 offset的正负与mode不匹配 (DEBUG模式额外前置检查) 
         * @exception E_File_* 
         */
        void move(xyu::diff_t offset, MoveMode mode = CUR) const;

        /* 其他 */

        /**
         * @brief 设置自定义缓冲区
         * @param buf 缓冲区指针
         * @param size 缓冲区大小
         * @param mode 缓冲模式 (ALL[全缓冲], LINE[行缓冲], NONE[无缓冲])
         * @note 初始默认为全缓冲，使用默认的缓冲区
         * @note buf需要手动管理，不会自动释放
         * @exception E_Logic_Invalid_Argument 文件未打开 (DEBUG模式额外前置检查)
         * @exception E_File_*
         */
        void rebuf(void* buf, xyu::size_t size, BufferMode mode = ALL);

        /* 运算符 */

        /**
         * @brief 从文件读取 1 个字节 (如果到文件尾则为 -1，无法与 0xFF 区分)
         * @note 可链式调用
         * @exception E_Logic_Invalid_Argument 文件未打开 (DEBUG模式额外前置检查)
         * @exception E_File_*
         */
        const File& operator>>(char& ch) const { ch = static_cast<char>(read()); return *this; }

        /**
         * @brief 从文件读取一行 (不包括换行符)
         * @note 可链式调用
         * @note 确保 buf 足够容纳整一行，否则行为未定义
         * @exception E_Logic_Invalid_Argument 文件未打开 (DEBUG模式额外前置检查)
         * @exception E_Logic_Null_Pointer buf 为 nullptr (DEBUG模式额外前置检查)
         * @exception E_File_*
         */
        const File& operator>>(char* buf) const { read_line(buf, -1); return *this; }

        /**
         * @brief 从文件读取一行 (不包括换行符)
         * @note 可链式调用
         * @exception E_Logic_Invalid_Argument 文件未打开 (DEBUG模式额外前置检查)
         * @exception E_File_*
         */
        const File& operator>>(xyu::String& str) const { read_line(str); return *this; }

        /**
         * @brief 写入数据
         * @note 可链式调用
         * @exception E_Logic_Invalid_Argument 文件未打开 (DEBUG模式额外前置检查)
         * @exception E_File_*
         */
        template <typename T>
        const File& operator<<(T&& data) const { write(xyu::forward<T>(data)); return *this; }

        /**
         * @brief 移动文件指针
         * @param offset 移动的字节数 (默认向前移动，负数则向后移动)
         * @note 可链式调用
         * @exception E_Logic_Invalid_Argument 文件未打开 (DEBUG模式额外前置检查)
         * @exception E_File_* 
         */
        const File& operator+=(xyu::diff_t offset) const { move(offset); return *this; }

        /**
         * @brief 移动文件指针
         * @param offset 移动的字节数 (默认向后移动，负数则向前移动)
         * @note 可链式调用
         * @exception E_Logic_Invalid_Argument 文件未打开 (DEBUG模式额外前置检查)
         * @exception E_File_* 
         */
        const File& operator-=(xyu::diff_t offset) const { move(-offset); return *this; }

        /* 静态方法 */

        /**
         * @brief 创建临时文件
         * @note 可读可写
         * @exception E_File_*
         */
        static File ftmp();

        /**
         * @brief 获取标准输入文件
         * @note 只读，析构不会关闭文件
         */
        static File fin() noexcept;

        /**
         * @brief 获取标准输出文件
         * @note 只写，析构不会关闭文件
         */
        static File fout() noexcept;

        /**
         * @brief 获取标准错误文件
         * @note 只写，析构不会关闭文件
         */
        static File ferr() noexcept;
    };

    // 文件模式运算
    constexpr File::OpenMode operator|(File::OpenMode a, File::OpenMode b) noexcept
    { return File::OpenMode{static_cast<xyu::uint>(a) | static_cast<xyu::uint>(b)}; }
    constexpr File::OpenMode operator&(File::OpenMode a, File::OpenMode b) noexcept
    { return File::OpenMode{static_cast<xyu::uint>(a) & static_cast<xyu::uint>(b)}; }
    constexpr File::OpenMode& operator~(File::OpenMode& a) noexcept
    { return a = File::OpenMode{~static_cast<xyu::uint>(a)}; }

    constexpr File::OpenMode& operator|=(File::OpenMode& a, File::OpenMode b) noexcept { return a = a | b; }
    constexpr File::OpenMode& operator&=(File::OpenMode& a, File::OpenMode b) noexcept { return a = a & b; }
    constexpr File::OpenMode  operator+ (File::OpenMode  a, File::OpenMode b) noexcept { return a | b; }
    constexpr File::OpenMode& operator+=(File::OpenMode& a, File::OpenMode b) noexcept { return a = a + b; }
    constexpr File::OpenMode  operator- (File::OpenMode  a, File::OpenMode b) noexcept { return a & ~b; }
    constexpr File::OpenMode& operator-=(File::OpenMode& a, File::OpenMode b) noexcept { return a = a - b; }

    // 全局变量 - 标准输出流文件 (注意：移动语句后会失效)
    inline File fout = File::fout();
    // 全局变量 - 标准错误流文件 (注意：移动语句后会失效)
    inline File ferr = File::ferr();
    // 全局变量 - 标准输入流文件 (注意：移动语句后会失效)
    inline File fin  = File::fin();
}

#pragma clang diagnostic pop