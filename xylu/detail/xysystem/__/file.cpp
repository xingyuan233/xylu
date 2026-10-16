#include "../file.h"
#include "../../xystring/fmt.h"

#if !XY_OUTPUT_ERROR_LOG
#  undef xylog
#  define xylog(...) 0
#endif

#define _FILE_OFFSET_BITS 64
#include <cstdio>
#include <cerrno>

#ifdef _WIN32
#  include <windows.h>
#endif

/// 跨平台工具
namespace
{
#if _WIN32
    // Windows 平台专属的 64 位扩展函数
    using xy_offset_t = long long;
    #define XY_FTELL _ftelli64
    #define XY_FSEEK _fseeki64
#else
    // POSIX 平台 通过 _FILE_OFFSET_BITS 扩展
    using xy_offset_t = off_t;
    #define XY_FTELL ::ftello
    #define XY_FSEEK ::fseeko
#endif

    FILE* f(const void* h) { return static_cast<FILE*>(const_cast<void*>(h)); }
}

/// 打开
namespace xylu::xysystem
{
    [[noreturn]] XY_NOINLINE XY_COLD
    static void public_error(const char* func, int err) noexcept
    {
        if (err == EBADF) xyloge("{}: descriptor is invalid or closed unexpectedly", func), xyu::abort();
        xyu::E_Logic::abort_unknown_error(func, err);
    }

    XY_NOINLINE XY_COLD
    static int open_error(const char* func, const char* path, xyu::uint mode, bool log) noexcept
    {
        int e = errno;
#if defined _WIN32
        if (e == EACCES)
        {
            auto attr = GetFileAttributesA(path);
            if (attr != INVALID_FILE_ATTRIBUTES && attr & FILE_ATTRIBUTE_DIRECTORY)
                e = EISDIR;
        }
#endif

        switch (e)
        {
        case EROFS:
            e = xyu::ES_File::FileSystem_Read_Only;
            if (log) xyloge("{}: no permission to open file in mode {:#b} with ROM", func, static_cast<xyu::uint>(mode));
            break;
        case EACCES:
            e = xyu::ES_File::No_Permission;
            if (log) xyloge("{}: permission denied to open file in mode {:#b}", func, static_cast<xyu::uint>(mode));
            break;
        case ENOENT:
            e = xyu::ES_File::Not_Found;
            if (log) xyloge("{}: file \"{}\" is not found", func, path);
            break;
        case EISDIR:
            e = xyu::ES_File::Path_Is_Dir;
            if (log) xyloge("{}: \"{}\" is a directory", func, path);
            break;
        case EMFILE:
            e = xyu::ES_File::Process_Limit;
            if (log) xyloge("{}: too many open files in process", func);
            break;
        case ENFILE:
            e = xyu::ES_File::System_Limit;
            if (log) xylogf("{}: too many open files in system", func);
            break;
        case ENOSPC:
            e = xyu::ES_File::No_Space;
            if (log) xylogf("{}: no space to create new file", func);
            break;
        case ENOMEM:
            e = xyu::ES_File::No_Memory;
            if (log) xylogf("{}: no memory to create new file", func);
            break;
        case EEXIST:
            e = xyu::ES_File::Already_Exist;
            if (log) xyloge("{}: \"{}\" is ready exist", func, path);
            break;
        default:
            public_error(func, e);
        }
        return e;
    }

    [[noreturn]] XY_NOINLINE XY_COLD
    static void abort_invalid_mode(xyu::uint mode) noexcept
    {
        xyloge("xyu::File::open: unsupported mode with value {:#b}", mode);
        xyu::abort();
    }

    static void* open_impl(const char* path, xyu::uint mode) noexcept
    {
        // 路径为空
        if (XY_UNLIKELY(!path))
            xyu::E_Logic::abort_null_pointer(XY_CurPos(nullptr,"xyu::File::open"), "path");

        // 将打开模式转换为字符串
#ifdef _WIN32
        #define XY_WIN_WSTR( str ) L##str
#else
        #define XY_WIN_WSTR( str ) str
#endif
        auto get_mode = [](xyu::uint mode) noexcept
        {
            switch (mode & (File::RW | File::Bin | File::Own))
            {
            case File::R:      return XY_WIN_WSTR("r");
            case File::Trunc:  return XY_WIN_WSTR("w");
            case File::App:    return XY_WIN_WSTR("a");

            case File::R | File::Trunc: return XY_WIN_WSTR("w+");
            case File::R | File::App:   return XY_WIN_WSTR("a+");
            case File::RW:              return XY_WIN_WSTR("r+");

            case File::Bin | File::R:       return XY_WIN_WSTR("rb");
            case File::Bin | File::Trunc:   return XY_WIN_WSTR("wb");
            case File::Bin | File::App:     return XY_WIN_WSTR("ab");

            case File::Bin | File::R | File::Trunc:  return XY_WIN_WSTR("wb+");
            case File::Bin | File::R | File::App:    return XY_WIN_WSTR("ab+");
            case File::Bin | File::RW:               return XY_WIN_WSTR("rb+");

            case File::Own:                          return XY_WIN_WSTR("wx");
            case File::Own | File::R:                return XY_WIN_WSTR("w+x");
            case File::Own | File::Bin:              return XY_WIN_WSTR("wbx");
            case File::Own | File::Bin | File::R:    return XY_WIN_WSTR("wb+x");

            default: abort_invalid_mode(mode);
            }
        };
        auto smode = get_mode(mode);

        // 打开文件
#ifdef _WIN32
        return _wfopen((wchar_t*)path, smode);
#else
        return ::fopen(path, smode);
#endif
    }

#ifdef _WIN32
    static void* open_help_win(xyu::StringView path, File::_OpenMode mode) noexcept
    {
        // utf8 -> utf16
        int wlen = MultiByteToWideChar(CP_UTF8, 0, path.data(), path.size(), nullptr, 0);
        if (wlen == 0) { _set_errno(ENOFILE); return nullptr; }
        xyu::String wpath(wlen * 2 + 1);
        MultiByteToWideChar(CP_UTF8, 0, path.data(), path.size(), (LPWSTR)wpath.data(), wlen);
        wpath.resize(wlen * 2 + 1);
        wpath[wlen * 2] = '\0';
        return open_impl(wpath.data(), mode);
    }
#endif

    void File::open(const char* path, _OpenMode mode)
    {
#ifdef _WIN32
        open(xyu::StringView{path}, mode);
#else
        xyu::ES_File state;
        if (h) close(state);
        h = open_impl(path, mode);
        if (XY_UNLIKELY(!h)) throw xyu::E_File{open_error("xyu::File::open", path, mode, true)};
#endif
    }

    void File::open(const char* path, _OpenMode mode, xyu::ES_File& state) noexcept
    {
#ifdef _WIN32
        open(xyu::StringView{path}, mode, state);
#else
        if (h) close(state);
        h = open_impl(path, mode);
        if (XY_UNLIKELY(!h)) state = open_error("xyu::File::open", path, mode, false);
        else state = xyu::ES_File::Success;
#endif
    }

    void File::open(xyu::StringView path, _OpenMode mode)
    {
#ifdef _WIN32
        xyu::ES_File state;
        if (h) close(state);
        h = open_help_win(path, mode);
        if (XY_UNLIKELY(!h)) throw xyu::E_File{open_error("xyu::File::open", path.data(), mode, true)};
#else
        if (path[path.size()] == '\0') return open(path.data(), mode);
        if (path.size() <= xyu::StringView::K_Stack_Buffer_Size)
        {
            char buf[xyu::StringView::K_Stack_Buffer_Size + 1];
            xyu::mem_copy(buf, path.data(), path.size());
            buf[path.size()] = '\0';
            open(buf, mode);
        }
        else
        {
            xyu::String tmp(path);
            open(tmp.data(), mode);
        }
#endif
    }

    void File::open(xyu::StringView path, _OpenMode mode, xyu::ES_File& state) noexcept
    {
#ifdef _WIN32
        if (h) close(state);
        h = open_help_win(path, mode);
        if (XY_UNLIKELY(!h)) state = open_error("xyu::File::open", path.data(), mode, false);
        else state = xyu::ES_File::Success;
#else
        if (path[path.size()] == '\0') return open(path.data(), mode, state);
        if (path.size() <= xyu::StringView::K_Stack_Buffer_Size)
        {
            char buf[xyu::StringView::K_Stack_Buffer_Size + 1];
            xyu::mem_copy(buf, path.data(), path.size());
            buf[path.size()] = '\0';
            open(buf, mode, state);
        }
        else
        {
            xyu::String tmp(path);
            open(tmp.data(), mode, state);
        }
#endif
    }
}

/// 关闭
namespace xylu::xysystem
{
    XY_NOINLINE XY_COLD
    static int flush_error(const char* func, bool log) noexcept
    {
        int e = errno;
        switch (e)
        {
        case EIO:
            e = xyu::ES_File::IO_Failed;
            if (log) xyloge("{}: physical IO error occurred", func);
            break;
        case ENOSPC:
            e = xyu::ES_File::No_Space;
            if (log) xylogf("{}: no space left on device", func);
            break;
        case EPIPE:
            e = xyu::ES_File::IO_Failed;
            if (log) xyloge("{}: pipe was closed unexpectedly", func);
            break;
        case EFBIG:
            e = xyu::ES_File::Size_Limit;
            if (log) xyloge("{}: file size exceeds system limit", func);
            break;
        default:
            public_error(func, e);
        }
        return e;
    }

    void File::close()
    {
        if (XY_UNLIKELY(!h)) return;
        if (h == stdout || h == stderr || h == stdin) return;
        void* tmp = h;
        h = nullptr;
        if (XY_UNLIKELY(::fclose(f(tmp))))
            throw xyu::E_File{flush_error("xyu::File::close", true)};
    }

    void File::close(xyu::ES_File& state) noexcept
    {
        if (XY_UNLIKELY(!h)) return;
        if (h == stdout || h == stderr || h == stdin) return;
        void* tmp = h;
        h = nullptr;
        if (XY_UNLIKELY(::fclose(f(tmp)))) state = flush_error("xyu::File::close", false);
        else state = xyu::ES_File::Success;
    }
}

/// 写入
namespace xylu::xysystem
{
    [[noreturn]] XY_NOINLINE XY_COLD
    static void abort_not_open(const char* func) noexcept
    {
        xyloge("{}: file is not open or already closed", func);
        xyu::abort();
    }

    void File::flush() const
    {
        if constexpr (XY_DEBUG) if (XY_UNLIKELY(!h)) abort_not_open("xyu::File::flush");
        if (XY_UNLIKELY(::fflush(f(h))))
            throw xyu::E_File{flush_error("xyu::File::flush", true)};
    }

    void File::flush(xyu::ES_File& state) const noexcept
    {
        if constexpr (XY_DEBUG) if (XY_UNLIKELY(!h)) abort_not_open("xyu::File::flush");
        if (XY_LIKELY(!::fflush(f(h)))) state = xyu::ES_File::Success;
        else state = flush_error("xyu::File::flush", false);
    }

    void File::write(char ch) const
    {
        if constexpr (XY_DEBUG) if (XY_UNLIKELY(!h)) abort_not_open("xyu::File::write");
        if (XY_UNLIKELY(::fputc(ch, f(h)) == EOF))
            throw xyu::E_File{flush_error("xyu::File::write", true)};
    }

    void File::write(char ch, xyu::ES_File& state) const noexcept
    {
        if constexpr (XY_DEBUG) if (XY_UNLIKELY(!h)) abort_not_open("xyu::File::write");
        if (XY_LIKELY(::fputc(ch, f(h)) != EOF)) state = xyu::ES_File::Success;
        else state = flush_error("xyu::File::write", false);
    }

    xyu::size_t File::write(xyu::StringView data) const
    {
        if constexpr (XY_DEBUG) if (XY_UNLIKELY(!h)) abort_not_open("xyu::File::write");
        xyu::size_t ret = ::fwrite(data.data(), 1, data.size(), f(h));
        if (XY_UNLIKELY(ferror(f(h))))
            throw xyu::E_File{flush_error("xyu::File::write", true)};
        return ret;
    }

    xyu::size_t File::write(xyu::StringView data, xyu::ES_File& state) const noexcept
    {
        if constexpr (XY_DEBUG) if (XY_UNLIKELY(!h)) abort_not_open("xyu::File::write");
        xyu::size_t ret = ::fwrite(data.data(), 1, data.size(), f(h));
        if (XY_LIKELY(!ferror(f(h)))) state = xyu::ES_File::Success;
        else state = flush_error("xyu::File::write", false);
        return ret;
    }
}

/// 移动
namespace xylu::xysystem
{
    [[noreturn]] XY_NOINLINE XY_COLD
    static void seek_error(const char* func) noexcept
    {
        int e = errno;
        switch (e)
        {
        case ESPIPE:
            xyloge("{}: cannot seek on a pipe, socket, or FIFO", func);
            xyu::abort();
        case EOVERFLOW: // 9.22 EB 仅理论，实际不可达
            xyloge("{}: offset is too large to be represented in file", func);
            xyu::abort();
        default:
            public_error(func, e);
        }
    }

    xyu::size_t File::pos() const noexcept
    {
        if constexpr (XY_DEBUG) if (XY_UNLIKELY(!h)) abort_not_open("xyu::File::pos");
        auto p = XY_FTELL(f(h));
        if (XY_UNLIKELY(p == -1)) seek_error("xyu::File::pos");
        return p;
    }

    void File::move(xyu::diff_t offset, _MoveMode mode) const noexcept
    {
        if constexpr (XY_DEBUG)
        {
            if (XY_UNLIKELY(mode < 0 || mode > 2))
                xyu::E_Logic::abort_invalid_value(XY_CurPos(nullptr,"xyu::File::move"), mode, "move mode");
            if (XY_UNLIKELY(!h)) abort_not_open("xyu::File::move");
        }

        int md = mode;
        if constexpr (Mv_Cur != SEEK_CUR) { if (mode == Mv_Cur) md = SEEK_CUR; }
        if constexpr (Mv_Head != SEEK_SET) { if (mode == Mv_Head) md = SEEK_SET; }
        if constexpr (Mv_Tail != SEEK_END) { if (mode == Mv_Tail) md = SEEK_END; }

        if (XY_UNLIKELY(XY_FSEEK(f(h), offset, md))) seek_error("xyu::File::move");
    }
}

/// 读取
namespace xylu::xysystem
{
    bool File::eof() const noexcept
    {
        if constexpr (XY_DEBUG) if (XY_UNLIKELY(!h)) abort_not_open("xyu::File::eof");
        return ::feof(f(h));
    }

    XY_NOINLINE XY_COLD
    static int read_error(bool log) noexcept
    {
        const char* func = "xyu::File::read";
        int e = errno;
        switch (e)
        {
        case EIO:
            e = xyu::ES_File::IO_Failed;
            if (log) xyloge("{}: physical IO error occurred", func);
            break;
        case EAGAIN:
            e = xyu::ES_File::IO_Failed;
            if (log) xyloge("{}: no data available", func);
            break;
        default:
            public_error(func, e);
        }
        return e;
    }

    int File::read() const
    {
        if constexpr (XY_DEBUG) if (XY_UNLIKELY(!h)) abort_not_open("xyu::File::read");
        int ch = ::fgetc(f(h));
        if (XY_LIKELY(ch != EOF || !ferror(f(h)))) return ch;
        throw xyu::E_File{read_error(true)};
    }

    int File::read(xyu::ES_File& state) const noexcept
    {
        if constexpr (XY_DEBUG) if (XY_UNLIKELY(!h)) abort_not_open("xyu::File::read");
        int ch = ::fgetc(f(h));
        if (XY_LIKELY(ch != EOF || !ferror(f(h)))) state = xyu::ES_File::Success;
        else ch = -1, state = read_error(false);
        return ch;
    }

    [[noreturn]] XY_NOINLINE XY_COLD
    static void abort_nullptr() noexcept
    {
        xyloge("xyu::File::read: cannot pass null pointer as buffer");
        xyu::abort();
    }

    xyu::size_t File::read(char* buf, xyu::size_t bytes) const
    {
        if constexpr (XY_DEBUG)
        {
            if (XY_UNLIKELY(!buf)) abort_nullptr();
            if (XY_UNLIKELY(!h)) abort_not_open("xyu::File::read");
        }
        xyu::size_t ret = ::fread(buf, 1, bytes, f(h));
        if (XY_UNLIKELY(ferror(f(h)))) throw xyu::E_File{read_error(true)};
        return ret;
    }

    xyu::size_t File::read(char* buf, xyu::size_t bytes, xyu::ES_File& state) const noexcept
    {
        if constexpr (XY_DEBUG)
        {
            if (XY_UNLIKELY(!buf)) abort_nullptr();
            if (XY_UNLIKELY(!h)) abort_not_open("xyu::File::read");
        }
        xyu::size_t ret = ::fread(buf, 1, bytes, f(h));
        if (XY_LIKELY(!ferror(f(h)))) state = xyu::ES_File::Success;
        else state = read_error(false);
        return ret;
    }


    static xyu::size_t read_impl(void* h, char *buf, xyu::size_t capa, char end, bool throws, xyu::ES_File& state)
    {
        if constexpr (XY_DEBUG)
        {
            if (XY_UNLIKELY(!buf)) abort_nullptr();
            if (XY_UNLIKELY(!h)) abort_not_open("xyu::File::read");
        }
        bool is_seekable = XY_FTELL(f(h)) != -1;
        state = xyu::ES_File::Success;

        // 不可随机访问
        if (XY_UNLIKELY(!is_seekable))
        {
            size_t total = 0;
            while (total < capa)
            {
                int ch = ::fgetc(f(h));
                if (XY_UNLIKELY(ch == EOF))
                {
                    if (XY_UNLIKELY(ferror(f(h))))
                    {
                        state = read_error(throws);
                        if (throws) throw xyu::E_File{state.code};
                    }
                    break;
                }
                if (ch == xyu::uint8(end))
                {
                    buf[total++] = '\0';
                    return total;
                }
                buf[total++] = xyu::uint8(ch);
            }
            buf[total] = '\0';
            return total;
        }

        // 可以随机访问
        size_t total = 0;
        size_t block = xyu::K_Cache_Line_Size;      // 初始块大小
        size_t MAX_BLOCK = 65536;                   // 最大块大小
        while (total < capa)
        {
            size_t count = xyu::min(capa - total, block);
#ifdef _WIN32
            // windows 记录物理偏移 (逻辑偏移也不影响)
            auto start = XY_FTELL(f(h));
            if (XY_UNLIKELY(start == -1)) seek_error("xyu::File::read");
#endif
            // 读取一块
            size_t n = ::fread(buf, 1, count, f(h));
            if (::ferror(f(h)))
            {
                state = read_error(throws);
                if (throws) throw xyu::E_File{state.code};
                break;
            }
            // 查找结束符
            if (size_t pos = xyu::mem_find(buf, end, n); pos != -1)
            {
                total += pos + 1;
                buf += pos;
#ifdef _WIN32
                // pos 逻辑偏移 <= 物理偏移
                if (XY_UNLIKELY(XY_FSEEK(f(h), start + xyu::diff_t(pos), SEEK_SET)))
                    seek_error("xyu::File::read");
                for (;;)
                {
                    int ch = ::fgetc(f(h));
                    if (XY_UNLIKELY(ch == EOF))
                    {
                        state = read_error(throws);
                        if (throws) throw xyu::E_File{state.code};
                        break;
                    }
                    if (ch == xyu::uint8(end)) break;
                }
#else
                if (XY_UNLIKELY(XY_FSEEK(f(h), xyu::diff_t(pos) - xyu::diff_t(n) + 1, SEEK_CUR)))
                    seek_error("xyu::File::read");
#endif
                break;
            }
            // 未找到结束符
            total += n;
            buf += n;
            if (n < count) break;
            // 增大块大小
            if (block < 4096) block *= 4;
            else if (block < MAX_BLOCK) block *= 2;
        }
        *buf = '\0';
        return total;
    }

    xyu::size_t File::read(char* buf, xyu::size_t capa, char end) const
    {
        xyu::ES_File state;
        return read_impl(h, buf, capa, end, true, state);
    }

    xyu::size_t File::read(char* buf, xyu::size_t capa, char end, xyu::ES_File& state) const noexcept
    { return read_impl(h, buf, capa, end, false, state); }


    static xyu::size_t read_impl(void* h, xyu::String& app, char end, bool throws, xyu::ES_File& state)
    {
        if constexpr (XY_DEBUG) if (XY_UNLIKELY(!h)) abort_not_open("xyu::File::read");
        bool is_seekable = XY_FTELL(f(h)) != -1;
        state = xyu::ES_File::Success;

        // 不可随机访问
        if (XY_UNLIKELY(!is_seekable))
        {
            size_t total = 0;
            for(;;)
            {
                int ch = ::fgetc(f(h));
                if (XY_UNLIKELY(ch == EOF))
                {
                    if (::ferror(f(h)))
                    {
                        state = read_error(throws);
                        if (throws) throw xyu::E_File{state.code};
                    }
                    break;
                }
                ++total;
                if (ch == xyu::uint8(end)) break;
                app << (char)(xyu::uint8)ch;
            }
            return total;
        }

        // 可以随机访问
        size_t total = 0;
        size_t block = xyu::K_Cache_Line_Size;      // 初始块大小
        size_t MAX_BLOCK = 65536;                   // 最大块大小
        for(;;)
        {
            size_t count = xyu::min(xyu::String::limit() - app.size(), block);
            if (XY_UNLIKELY(count == 0)) break;
            app.reserve(app.size() + count);
#ifdef _WIN32
            // windows 记录物理偏移 (逻辑偏移也不影响)
            auto start = XY_FTELL(f(h));
            if (XY_UNLIKELY(start == -1)) seek_error("xyu::File::read");
#endif
            // 读取一块
            size_t n = ::fread(app.data() + app.size(), 1, count, f(h));
            if (::ferror(f(h)))
            {
                state = read_error(throws);
                if (throws) throw xyu::E_File{state.code};
                break;
            }
            // 查找结束符
            if (size_t pos = xyu::mem_find(app.data() + app.size(), end, n); pos != -1)
            {
                total += pos + 1;
                app.resize(app.size() + pos);
#ifdef _WIN32
                // pos 逻辑偏移 <= 物理偏移
                if (XY_UNLIKELY(XY_FSEEK(f(h), start + xyu::diff_t(pos), SEEK_SET)))
                    seek_error("xyu::File::read");
                for (;;)
                {
                    int ch = ::fgetc(f(h));
                    if (XY_UNLIKELY(ch == EOF))
                    {
                        state = read_error(throws);
                        if (throws) throw xyu::E_File{state.code};
                        break;
                    }
                    if (ch == xyu::uint8(end)) break;
                }
#else
                if (XY_UNLIKELY(XY_FSEEK(f(h), xyu::diff_t(pos) - xyu::diff_t(n) + 1, SEEK_CUR)))
                    seek_error("xyu::File::read");
#endif
                break;
            }
            // 未找到结束符
            total += n;
            app.resize(app.size() + n);
            if (n < count) break;
            // 增大块大小
            if (block < 4096) block *= 4;
            else if (block < MAX_BLOCK) block *= 2;
        }
        return total;
    }

    xyu::size_t File::read(xyu::String& app, char end) const
    { xyu::ES_File state; return read_impl(h, app, end, true, state); }

    xyu::size_t File::read(xyu::String& app, char end, xyu::ES_File& state) const noexcept
    { return read_impl(h, app, end, false, state); }
}

/// 其他
namespace xylu::xysystem
{
    [[noreturn]] XY_NOINLINE XY_COLD
    static void abort_bufsize(void* buf, xyu::size_t size, const char* func) noexcept
    {
        if (buf == nullptr && size < BUFSIZ)
            xyloge("{}: buffer size {} too small than {}", func, size, BUFSIZ);
        else if (buf == nullptr)
            xyloge("{}: buffer size {} too large", func, size);
        else
            xyloge("{}: set buf failed", func);
        xyu::abort();
    }

    void File::rebuf(void* buf, xyu::size_t size, _BufMode mode) const noexcept
    {
        if constexpr (XY_DEBUG)
        {
            if (XY_UNLIKELY(mode < 0 || mode > 2))
                xyu::E_Logic::abort_invalid_value(XY_CurPos(nullptr,"xyu::File::rebuf"), mode, "buffer mode");
            if (XY_UNLIKELY(!h)) abort_not_open("xyu::File::rebuf");
        }

        int md = mode;
        if constexpr (Bf_Full != _IOFBF) { if (mode == Bf_Full) md = _IOFBF; }
        if constexpr (Bf_Line != _IOLBF) { if (mode == Bf_Line) md = _IOLBF; }
        if constexpr (Bf_None != _IONBF) { if (mode == Bf_None) md = _IONBF; }

        if (XY_UNLIKELY(::setvbuf(f(h), (char*)buf, md, size)))
            abort_bufsize(buf, size, "xyu::File::rebuf");
    }


    File File::ftmp()
    {
        FILE *h = ::tmpfile();
        if (XY_UNLIKELY(!h)) throw xyu::E_File{open_error("xyu::File::ftmp", "*tmp", R | Trunc | Bin, true)};
        File f;
        f.h = h;
        return f;
    }
    File File::ftmp(xyu::ES_File& state) noexcept
    {
        FILE *h = ::tmpfile();
        if (XY_LIKELY(h)) state = xyu::ES_File::Success;
        else state = open_error("xyu::File::ftmp", "*tmp", R | Trunc | Bin, false);
        File f;
        f.h = h;
        return f;
    }
    File File::fin() noexcept
    {
#ifdef _WIN32
        SetConsoleCP(CP_UTF8);
#endif
        File f;
        f.h = stdin;
        return f;
    }
    File File::fout() noexcept
    {
#ifdef _WIN32
        SetConsoleOutputCP(CP_UTF8);
#endif
        File f;
        f.h = stdout;
        return f;
    }
    File File::ferr() noexcept
    {
#ifdef _WIN32
        SetConsoleOutputCP(CP_UTF8);
#endif
        File f;
        f.h = stderr;
        return f;
    }
}
