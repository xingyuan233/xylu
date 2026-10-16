#pragma once

/**
 * @file    xysystem/file.h
 * @brief   文件类
 * @date    2026-03-09
 */

#include "./_.h"
#include "../xystring/strsso.h"

/// 文件异常类
namespace xylu::xycore
{
    /// 文件异常类
    struct E_File : E_Resource { using E_Resource::E_Resource; };

    /// 异常码
    struct ES_File : ErrorState
    {
        using ErrorState::ErrorState;

        enum _File
        {
            // 操作成功
            Success = ES_Resource::Success,
            // 文件未找到
            Not_Found = ES_Resource::Not_Found,
            // 文件已存在
            Already_Exist = 17,
            // 路径为目录
            Path_Is_Dir = 21,
            // 无权限
            No_Permission = ES_Resource::No_Permission,
            // 系统打开文件数到达上限
            System_Limit = ES_Resource::System_Limit,
            // 进程打开文件数达到上限
            Process_Limit = ES_Resource::Process_Limit,
            // 文件系统只读
            FileSystem_Read_Only = 30,
            // 内存不足
            No_Memory = ES_Resource::No_Memory,
            // 磁盘空间不足
            No_Space = ES_Resource::No_Space,
            // IO失败 (物理磁盘IO失败 / 网络连接断开 / 特殊文件非阻塞强制读取失败 等)
            IO_Failed = ES_Resource::IO_Failed,
            // 文件大小到达上限
            Size_Limit = 27,
        };
    };
}

/// 文件类
namespace xylu::xysystem
{
    /**
     * @brief 一个封装了底层文件句柄的 RAII 类，提供了统一的文件 I/O 操作接口。
     * @details
     *   `File` 类负责自动管理文件的打开和关闭，确保文件资源总能被正确释放。
     *
     *   方法简述：
     *    - 支持普通文件、临时文件以及标准输入/输出/错误的访问。
     *    - 支持跨平台的统一 utf8 路径 和 非 \0 结尾字符串视图使用
     *
     *   注意事项：
     *    - `const` 对象的行为: 一个 `const File` 对象代表一个“绑定的”文件句柄，
     *      仍然可以通过它进行读写操作，但无法被重新绑定到另一个文件。
     */
    struct File : xyu::class_no_copy_t
    {
        enum _OpenMode : xyu::uint
        {
            R       = 1 << 0,           // 可读
            Trunc   = 1 << 1,           // 可写+截断
            App     = 1 << 2,           // 可写+追加
            RW      = R | Trunc | App,  // 读写+覆盖 (必须存在)
            Bin     = 1 << 3,           // 二进制模式 (不处理换行符)
            Own     = 1 << 4,           // 独占仅读
        };

    private:
        void* h;        // 文件句柄

    public:
        /* 文件对象 */

        /// 默认构造
        explicit File() noexcept : h{} {}

        /**
         * @brief 析构函数
         * @details 自动关闭文件
         */
        ~File() noexcept { xyu::ES_File state; close(state); }

        /// 移动构造
        File(File&& other) noexcept : h{other.h} { other.h = nullptr; }
        /// 移动赋值
        File& operator=(File&& other) noexcept { xyu::swap(h, other.h); return *this; }

        /// 构造并打开文件
        File(const char* path, _OpenMode mode) : File{} { open(path, mode); }
        /// 构造并打开文件
        File(const char* path, _OpenMode mode, xyu::ES_File& state) noexcept : File{} { open(path, mode, state); }
        /// 构造并打开文件
        File(xyu::StringView path, _OpenMode mode) : File{} { open(path, mode); }
        /// 构造并打开文件
        File(xyu::StringView path, _OpenMode mode, xyu::ES_File& state) noexcept : File{} { open(path, mode, state); }

        /* 文件状态 */

        /// 文件是否已打开
        explicit operator bool() const noexcept { return h; }

        /* 文件打开与关闭 */

        /**
         * @brief 打开文件
         * @param path 文件路径
         * @param mode 文件打开模式
         * @details 若文件已打开，则先关闭原文件再打开新文件
         */
        void open(const char* path, _OpenMode mode);

        /**
         * @brief 打开文件
         * @param path 文件路径
         * @param mode 文件打开模式
         * @details 若文件已打开，则先关闭原文件再打开新文件
         */
        void open(const char* path, _OpenMode mode, xyu::ES_File& state) noexcept;

        /**
         * @brief 打开文件
         * @param path 文件路径
         * @param mode 文件打开模式
         * @details 若文件已打开，则先关闭原文件再打开新文件
         */
        void open(xyu::StringView path, _OpenMode mode);

        /**
         * @brief 打开文件
         * @param path 文件路径
         * @param mode 文件打开模式
         * @details 若文件已打开，则先关闭原文件再打开新文件
         */
        void open(xyu::StringView path, _OpenMode mode, xyu::ES_File& state) noexcept;

        /**
         * @brief 关闭文件
         * @details 若文件已关闭则不做处理
         */
        void close();

        /**
         * @brief 关闭文件
         * @details 若文件已关闭则不做处理
         */
        void close(xyu::ES_File& state) noexcept;

        /* 读取 */

        /// 是否到文件尾
        bool eof() const noexcept;

        /**
         * @brief 从文件读取 1 个字节并返回
         * @details 若文件已到结尾则返回 -1，否则返回 [0,255]
         * @attention 特殊文件非阻塞模式下强制读取会 abort
         */
        int read() const;

        /**
         * @brief 从文件读取 1 个字节并返回
         * @details 若文件已到结尾或发生错误则返回 -1，否则返回 [0,255]
         */
        int read(xyu::ES_File& state) const noexcept;

        /**
         * @brief 从文件读取 bytes 个字节，并返回实际读取的字节数
         * @attention 不会额外写入 '\0'
         */
        XY_NONNULL_RELEASE(2)
        xyu::size_t read(char* buf, xyu::size_t bytes) const;

        /**
         * @brief 从文件读取 bytes 个字节，并返回实际读取的字节数
         * @note 不会额外写入 '\0'
         */
        XY_NONNULL_RELEASE(2)
        xyu::size_t read(char* buf, xyu::size_t bytes, xyu::ES_File& state) const noexcept;

        /// 从文件读取 bytes 个字节，追加到 app 中，并返回实际读取的字节数
        template <typename Int, typename = xyu::t_enable<xyu::t_is_integer<Int>>>
        xyu::size_t read(xyu::String& app, Int bytes) const
        {
            xyu::size_t start = app.size();
            app.reserve(start + bytes);
            xyu::size_t n = read(app.data() + start, xyu::min(app.capacity() - start, bytes));
            app.resize(start + n);
            return n;
        }

        /// 从文件读取 bytes 个字节，追加到 app 中，并返回实际读取的字节数
        template <typename Int, typename = xyu::t_enable<xyu::t_is_integer<Int>>>
        xyu::size_t read(xyu::String& app, Int bytes, xyu::ES_File& state) const noexcept
        {
            xyu::size_t start = app.size();
            app.reserve(start + bytes);
            xyu::size_t n = read(app.data() + start, xyu::min(app.capacity() - start, bytes), state);
            if (XY_LIKELY(state.success())) app.resize(start + n);
            return n;
        }

        /// 从文件读取 bytes 个字节，并返回字符串
        template <typename Int, typename = xyu::t_enable<xyu::t_is_integer<Int>>>
        xyu::String read(Int bytes) const { xyu::String str(bytes); read(str, bytes); return str; }

        /// 从文件读取 bytes 个字节，并返回字符串
        template <typename Int, typename = xyu::t_enable<xyu::t_is_integer<Int>>>
        xyu::String read(Int bytes, xyu::ES_File& state) const noexcept { xyu::String str(bytes); read(str, bytes, state); return str; }

        /**
         * @brief 从文件读取到 end 字符结束 (不写入end)，返回实际读取的字节数
         * @param buf 读取缓冲区
         * @param capa 读取缓冲区容量 (不包括'\0')
         * @param end 结束字符
         * @details 若找到end，则 返回值 = strlen(buf) + 1
         * @note 二进制模式下性能更好
         */
        XY_NONNULL_RELEASE(2)
        xyu::size_t read(char* buf, xyu::size_t capa, char end) const;

        /**
         * @brief 从文件读取到 end 字符结束 (不写入end)，返回实际读取的字节数
         * @param buf 读取缓冲区
         * @param capa 读取缓冲区容量 (不包括'\0')
         * @param end 结束字符
         * @details 若找到end，则 返回值 = strlen(buf) + 1
         * @note 二进制模式下性能更好
         */
        XY_NONNULL_RELEASE(2)
        xyu::size_t read(char* buf, xyu::size_t capa, char end, xyu::ES_File& state) const noexcept;

        /**
         * @brief 从文件读取到 end 字符结束 (不写入end)，并返回实际读取的字节数
         * @param app 追加到 app 中
         * @param end 结束字符
         * @details 若找到end，则 返回值 = app增加的大小 + 1
         * @note 二进制模式下性能更好
         */
        xyu::size_t read(xyu::String& app, char end) const;

        /**
         * @brief 从文件读取到 end 字符结束 (不写入end)，并返回实际读取的字节数
         * @param app 追加到 app 中
         * @param end 结束字符
         * @details 若找到end，则 返回值 = app增加的大小 + 1
         * @note 二进制模式下性能更好
         */
        xyu::size_t read(xyu::String& app, char end, xyu::ES_File& state) const noexcept;

        /**
         * @brief 从文件读取到 end 字符结束 (不写入end)，并返回字符串
         * @param end 结束字符
         */
        xyu::String read(char end) const { xyu::String str; read(str, end); return str; }

        /**
         * @brief 从文件读取到 end 字符结束 (不写入end)，并返回字符串
         * @param end 结束字符
         */
        xyu::String read(char end, xyu::ES_File& state) const noexcept { xyu::String str; read(str, end, state); return str; }

        /* 写入 */

        /// 刷新缓冲区
        void flush() const;

        /// 刷新缓冲区
        void flush(xyu::ES_File& state) const noexcept;

        /// 写入 1 个字节
        void write(char ch) const;

        /// 写入 1 个字节
        void write(char ch, xyu::ES_File& state) const noexcept;

        /// 写入字符串，并返回实际写入的字节数
        xyu::size_t write(xyu::StringView data) const;

        /// 写入字符串，并返回实际写入的字节数
        xyu::size_t write(xyu::StringView data, xyu::ES_File& state) const noexcept;

        /**
         * @brief 写入其他
         * @note 该类型必须特化 FormatImpl
         */
        template <typename T, typename = xyu::t_enable<!xyu::t_can_icast<T, xyu::StringView>>>
        xyu::size_t write(T&& data) const;

        /**
         * @brief 写入其他
         * @note 该类型必须特化 FormatImpl
         */
        template <typename T, typename = xyu::t_enable<!xyu::t_can_icast<T, xyu::StringView>>>
        xyu::size_t write(T&& data, xyu::ES_File& state) const noexcept;
        
        /* 指针移动 */

        /// 获取文件位置
        xyu::size_t pos() const noexcept;

        /// 设置文件位置
        void repos(xyu::size_t pos) const noexcept { move(pos, Mv_Head); }

        /// 移动模式 (全缓冲, 行缓冲, 无缓冲)
        enum _MoveMode { Mv_Head, Mv_Cur, Mv_Tail };

        /**
         * @brief 将当前文件指针移动 offset 个字节
         * @attention windows 下文本模式手动计算的偏移与实际位置偏移可能不一致
         */
        void move(xyu::diff_t offset, _MoveMode mode = Mv_Cur) const noexcept;

        /* 其他 */

        /// 缓冲模式 (全缓冲, 行缓冲, 无缓冲)
        enum _BufMode { Bf_Full, Bf_Line, Bf_None };

        /**
         * @brief 设置自定义缓冲区
         * @param buf 缓冲区指针 (若为 nullptr 则自动分配)
         * @param size 缓冲区大小
         * @param mode 缓冲模式 (默认全缓冲)
         * @attention 必须在读写前进行，buf 需要手动管理，不会自动释放
         */
        void rebuf(void* buf, xyu::size_t size, _BufMode mode = Bf_Full) const noexcept;

        /* 运算符 */

        /// 从文件读取 1 个字节 (如果到文件尾则为 -1，无法与 0xFF 区分)
        const File& operator>>(char& ch) const { ch = read(); return *this; }
        /// 从文件读取 1 个字节
        const File& operator>>(int& ch) const { ch = read(); return *this; }

        /// 从文件读取一行
        const File& operator>>(xyu::String& str) const { read(str, '\n'); return *this; }

        /// 写入数据
        template <typename T>
        const File& operator<<(T&& data) const { write(xyu::forward<T>(data)); return *this; }

        /* 静态方法 */

        /// 创建临时文件 (可读可写)
        static File ftmp();
        /// 创建临时文件 (可读可写)
        static File ftmp(xyu::ES_File& state) noexcept;

        /// 获取标准输入文件，utf8编码 (只读)
        static File fin() noexcept;

        /// 获取标准输出文件，utf8编码 (只写)
        static File fout() noexcept;

        /// 获取标准错误文件，utf8编码 (只写)
        static File ferr() noexcept;

    public:
        friend constexpr _OpenMode operator|(_OpenMode m1, _OpenMode m2) noexcept { return _OpenMode((int)m1 | (int)m2); }
    };

    // 全局变量 - 标准输出流文件
    inline const File xyout = File::fout();
    // 全局变量 - 标准错误流文件
    inline const File xyerr = File::ferr();
    // 全局变量 - 标准输入流文件
    inline const File xyin  = File::fin();
}
