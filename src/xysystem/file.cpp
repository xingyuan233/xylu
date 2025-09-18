#pragma clang diagnostic push
#pragma ide diagnostic ignored "hicpp-exception-baseclass"
#include <stdio.h>
#include <errno.h>
#include "../../head/xysystem/file.h"
#include "../../link/log"

template <typename T>
using nt = xyu::number_traits<T>;

namespace
{
    [[noreturn]] void no_handle(xyu::uint line, const char* func)
    {
        xyloge2(0, "E_File_Invalid_State: file is not open or has been closed", line, func);
        throw xyu::make_error(xyu::E_File_Invalid_State{}, xyu::E_Logic_Invalid_Argument{});
    }

    [[noreturn]] void null_buf(xyu::uint line, const char* func)
    {
        xyloge2(0, "E_File_Null_Pointer: buffer cannot be nullptr", line, func);
        throw xyu::make_error(xyu::E_Logic_Null_Pointer{}, xyu::E_File{});
    }

    [[noreturn]] void null_data(xyu::uint line, const char* func)
    {
        xyloge2(0, "E_Logic_Null_Pointer: data cannot be nullptr", line, func);
        throw xyu::make_error(xyu::E_Logic_Null_Pointer{}, xyu::E_File{});
    }

    [[noreturn]] void unknown_error(xyu::uint line, const char* func, int err)
    {
        xyloge2(0, "E_File: unknown error with code {}", line, func, err);
        throw xyu::E_File{};
    }
    
    [[noreturn]] void open_error(xyu::uint line, const char* func, const char* path, xyu::uint mode)
    {
        switch (errno) {
            case EACCES:
                xyloge2(0, "E_File_Permission_Denied: cannot access file with mode {:#b}", line, func, static_cast<xyu::uint>(mode));
                throw xyu::E_File_Permission_Denied{};
            case ENOENT:
                xyloge2(0, "E_File_Not_Found: file is not found with path {}", line, func, path);
                throw xyu::E_File_Not_Found{};
            case EISDIR:
                xyloge2(0, "E_File_Path_Is_Dir: '{}' is a directory", line, func, path);
                throw xyu::E_File_Path_Is_Dir{};
            case EMFILE:
                xyloge2(0, "E_File_Process_Limit: too many open files in process", line, func);
                throw xyu::E_File_Process_Limit{};
            case ENFILE:
                xyloge2(1, "E_File_System_Limit: too many open files in system", line, func);
                throw xyu::E_File_System_Limit{};
            case EROFS:
                xyloge2(0, "E_File_Permission_Denied: cannot access file in a read-only file system with mode {:#b}", line, func, static_cast<xyu::uint>(mode));
                throw xyu::E_File_Permission_Denied{};
            default:
                unknown_error(__LINE__, func, errno);
        }
    }

    [[noreturn]] void close_error(xyu::uint line, const char* func)
    {
        switch (errno) {
            case EIO:
                xyloge2(1, "E_File_Physical: physical I/O error occurred while flushing buffer to disk before close", line, func);
                throw xyu::E_File_Physical{};
            case ENOSPC:
                xyloge2(1, "E_File_No_Memory: no memory left while flushing buffer to disk before close", line, func);
                throw xyu::E_File_No_Memory{};
            case EBADF:
                xyloge2(0, "E_File_Invalid_State: file was closed unexpectedly", line, func);
                throw xyu::E_File_Invalid_State{};
            default:
                unknown_error(__LINE__, func, errno);
        }
    }

    [[noreturn]] void read_error(xyu::uint line, const char* func)
    {
        switch (errno) {
            case EIO:
                xyloge2(1, "E_File_Physical: physical I/O error occurred while reading file", line, func);
                throw xyu::E_File_Physical{};
            case EBADF:
                xyloge2(0, "E_File_Invalid_State: file was closed unexpectedly", line, func);
                throw xyu::E_File_Invalid_State{};
            case EAGAIN:
#if defined(EWOULDBLOCK)
#if EAGAIN != EWOULDBLOCK
            case EWOULDBLOCK:
#endif
#endif
                xyloge2(0, "E_File_IO: no data available for reading", line, func);
                throw xyu::E_File_IO{};
            default:
                unknown_error(line, func, errno);
        }
    }

    [[noreturn]] void write_error(xyu::uint line, const char* func)
    {
        switch (errno) {
            case ENOSPC:
                xyloge2(1, "E_File_No_Memory: no memory left while writing file", line, func);
                throw xyu::E_File_No_Memory{};
            case EIO:
                xyloge2(1, "E_File_Physical: physical I/O error occurred while writing file", line, func);
                throw xyu::E_File_Physical{};
            case EBADF:
                xyloge2(0, "E_File_Invalid_State: file was closed unexpectedly", line, func);
                throw xyu::E_File_Invalid_State{};
            case EPIPE:
                xyloge2(0, "E_File_Pipe: pipe was closed unexpectedly", line, func);
                throw xyu::E_File_Pipe{};
            case EFBIG:
                xyloge2(0, "E_File_Too_Large: file size exceeds system or file system limit", line, func);
                throw xyu::E_File_Too_Large{};
            default:
                unknown_error(line, func, errno);
        }
    }

    [[noreturn]] void seek_error(xyu::uint line, const char* func, int err)
    {
        switch (err) {
            case ESPIPE:
                xyloge2(0, "E_File_Not_Seekable: file is not seekable", line, func);
                throw xyu::make_error(xyu::E_Logic_Invalid_Argument{}, xyu::E_File{});
            case EINVAL:
                xyloge2(0, "E_File_Move_Mode: move mode is invalid or new position is invalid", line, func);
                throw xyu::make_error(xyu::E_Logic_Invalid_Argument{}, xyu::E_File{});
            case EOVERFLOW:
                xyloge2(0, "E_File_Position_Overflow: file position is too large for reception type", line, func);
                throw xyu::make_error(xyu::E_Logic_Invalid_Argument{}, xyu::E_File{});
            default:
                unknown_error(line, func, err);
        }
    }
    
    FILE* f(const void* h) { return static_cast<FILE*>(const_cast<void*>(h)); }
}

namespace xylu::xysystem
{
    void File::open(const char* path, OpenMode mode)
    {
#if XY_DEBUG
        // 路径为空
        if (XY_UNLIKELY(!path)) {
            xyloge(0, "E_File_Null_Path: path is nullptr");
            throw xyu::make_error(xyu::E_Logic_Null_Pointer{}, xyu::E_File{});
        }
#endif
        // 将打开模式转换为字符串
        auto get_mode = [](OpenMode mode, xyu::uint line, const char* func) {
            switch (mode & (READ | WRITE | BINARY | OWN))
            {
                case READ:      return "r";
                case TRUNC:     return "w";
                case APPEND:    return "a";
                case WRITE:     return "a";

                case READ | TRUNC:      return "w+";
                case READ | APPEND:     return "a+";
                case READ | WRITE:      return "a+";

                case BINARY | READ:     return "rb";
                case BINARY | TRUNC:    return "wb";
                case BINARY | APPEND:   return "ab";
                case BINARY | WRITE:    return "ab";

                case BINARY | READ | TRUNC:     return "wb+";
                case BINARY | READ | APPEND:    return "ab+";
                case BINARY | READ | WRITE:     return "rb+";

                case OWN:                   return "wx";
                case OWN | READ:            return "w+x";
                case OWN | BINARY:          return "wbx";
                case OWN | BINARY | READ:   return "wb+x";
            }
            xyloge2(0, "E_File_Open_Mode: unsupported open mode with value {:#b}", line, func, static_cast<xyu::uint>(mode));
            throw xyu::make_error(xyu::E_Logic_Invalid_Argument{}, xyu::E_File{});
        };
        auto smode = get_mode(mode, __LINE__, __func__);
        
        // 如果文件已经打开，先关闭
        if (h) close();
        // 打开文件
        h = ::fopen(path, smode);
        // 异常处理
        if (!h) open_error(__LINE__, __func__, path, mode);
        // 若为 WRITE(覆盖) 模式，则将指针移到文件开头
        if ((mode & WRITE) == WRITE) rewind();
    }

    void File::close()
    {
        if (!h) return;
        if (h == stdout || h == stderr || h == stdin) { h = nullptr; return; }
        int ret = ::fclose(f(h));
        h = nullptr;
        if (XY_UNLIKELY(ret)) close_error(__LINE__, __func__);
    }

    int File::read() const
    {
#if XY_DEBUG
        if (XY_UNLIKELY(!h)) no_handle(__LINE__, __func__);
#endif
        int ch = ::fgetc(f(h));
        if (XY_UNLIKELY(ferror(f(h)))) read_error(__LINE__, __func__);
        return ch;
    }

    xyu::size_t File::read(char *buf, xyu::size_t bytes) const
    {
#if XY_DEBUG
        if (XY_UNLIKELY(!h)) no_handle(__LINE__, __func__);
        if (XY_UNLIKELY(!buf)) null_buf(__LINE__, __func__);
#endif
        xyu::size_t ret = ::fread(buf, 1, bytes, f(h));
        if (XY_UNLIKELY(ferror(f(h)))) read_error(__LINE__, __func__);
        return ret;
    }

    xyu::size_t File::read(char* buf, xyu::size_t capa, char over) const
    {
#if XY_DEBUG
        if (XY_UNLIKELY(!h)) no_handle(__LINE__, __func__);
        if (XY_UNLIKELY(!buf)) null_buf(__LINE__, __func__);
#endif
        if (XY_UNLIKELY(capa-- == 0)) return 0;
        xyu::size_t n = 0;
        int ch;
        while (n < capa && ((ch = ::fgetc(f(h))) != EOF) && ch != over)
            buf[n++] = static_cast<char>(ch);
        if (XY_UNLIKELY(ferror(f(h)))) read_error(__LINE__, __func__);
        buf[n] = '\0';
        return n;
    }

    xyu::size_t File::read(xyu::String& app, char over) const
    {
#if XY_DEBUG
        if (XY_UNLIKELY(!h)) no_handle(__LINE__, __func__);
#endif
        int ch;
        xyu::size_t start = app.size();
        while ((ch = ::fgetc(f(h))) != EOF && ch != over) app << ch;
        if (XY_UNLIKELY(ferror(f(h)))) read_error(__LINE__, __func__);
        return app.size() - start;
    }

    xyu::size_t File::read_all(char* buf, xyu::size_t capa) const
    {
#if XY_DEBUG
        if (XY_UNLIKELY(!h)) no_handle(__LINE__, __func__);
        if (XY_UNLIKELY(!buf)) null_buf(__LINE__, __func__);
#endif
        if (XY_UNLIKELY(capa-- == 0)) return 0;

        if constexpr (nt<xyu::size_t>::max >= nt<::fpos_t>::max)
        {
            xyu::size_t cur, over;
            bool err = false;
            try {
                cur = pos();
                move(0, END);
                over = pos();
                repos(cur);
            }
            catch (...) { err = true; }
            if (!err) return read(buf, xyu::min(capa, over - cur));
        }

        xyu::size_t n = 0;
        int ch;
        while (n < capa && ((ch = ::fgetc(f(h))) != EOF))
            buf[n++] = static_cast<char>(ch);
        if (XY_UNLIKELY(ferror(f(h)))) read_error(__LINE__, __func__);
        buf[n] = '\0';
        return n;
    }

    xyu::size_t File::read_all(xyu::String& app) const
    {
#if XY_DEBUG
        if (XY_UNLIKELY(!h)) no_handle(__LINE__, __func__);
#endif
        if constexpr (nt<xyu::size_t>::max >= nt<::fpos_t>::max)
        {
            xyu::size_t cur, over;
            bool err = false;
            try {
                cur = pos();
                move(0, END);
                over = pos();
                repos(cur);
            }
            catch (...) { err = true; }
            if (!err) return read(app, over - cur);
        }

        int ch;
        xyu::size_t start = app.size();
        while ((ch = ::fgetc(f(h))) != EOF) app << ch;
        if (XY_UNLIKELY(ferror(f(h)))) read_error(__LINE__, __func__);
        return app.size() - start;
    }

    bool File::eof() const XY_NOEXCEPT_NDEBUG
    {
#if XY_DEBUG
        if (XY_UNLIKELY(!h)) no_handle(__LINE__, __func__);
#endif
        return ::feof(f(h));
    }

    void File::write(char ch) const
    {
#if XY_DEBUG
        if (XY_UNLIKELY(!h)) no_handle(__LINE__, __func__);
#endif
        if (XY_UNLIKELY(::fputc(ch, f(h)) == EOF)) write_error(__LINE__, __func__);
    }

    void File::write(const xyu::StringView& data) const
    {
#if XY_DEBUG
        if (XY_UNLIKELY(!h)) no_handle(__LINE__, __func__);
#endif
        if (XY_UNLIKELY(::fwrite(data.data(), 1, data.size(), f(h)) != data.size()))
            write_error(__LINE__, __func__);
    }

    xyu::size_t File::write(const char *data) const
    {
#if XY_DEBUG
        if (XY_UNLIKELY(!h)) no_handle(__LINE__, __func__);
        if (XY_UNLIKELY(!data)) null_data(__LINE__, __func__);
#endif
        xyu::size_t ret = ::fputs(data, f(h));
        if (XY_UNLIKELY(ferror(f(h)))) write_error(__LINE__, __func__);
        return ret;
    }

    void File::flush() const
    {
#if XY_DEBUG
        if (XY_UNLIKELY(!h)) no_handle(__LINE__, __func__);
#endif
        if (XY_UNLIKELY(::fflush(f(h)))) write_error(__LINE__, __func__);
    }

    void File::rewind() const
    {
#if XY_DEBUG
        if (XY_UNLIKELY(!h)) no_handle(__LINE__, __func__);
#endif
        ::rewind(f(h));
        if (XY_UNLIKELY(ferror(f(h)))) seek_error(__LINE__, __func__, errno);
    }

    xyu::size_t File::pos() const
    {
#if XY_DEBUG
        if (XY_UNLIKELY(!h)) no_handle(__LINE__, __func__);
#endif
        ::fpos_t pos;
        if (XY_UNLIKELY(::fgetpos(f(h), &pos))) seek_error(__LINE__, __func__, errno);
        if constexpr (nt<xyu::size_t>::max < nt<::fpos_t>::max)
            if (XY_UNLIKELY(nt<xyu::size_t>::max < pos)) seek_error(__LINE__, __func__, EOVERFLOW);
        return pos;
    }

    void File::repos(xyu::size_t pos) const
    {
#if XY_DEBUG
        if (XY_UNLIKELY(!h)) no_handle(__LINE__, __func__);
#endif
        int r;
        if constexpr (nt<xyu::size_t>::max > nt<::fpos_t>::max) {
            if (XY_UNLIKELY(nt<::fpos_t>::max < pos)) {
                r = ::fsetpos(f(h), &nt<::fpos_t>::max);
                if (XY_UNLIKELY(r)) seek_error(__LINE__, __func__, errno);
                pos -= nt<xyu::size_t>::max;
                return move(static_cast<xyu::diff_t>(pos), CUR);
            }
        }
        auto p = static_cast<::fpos_t>(pos);
        r = ::fsetpos(f(h), &p);
        if (XY_UNLIKELY(r)) seek_error(__LINE__, __func__, errno);
    }

    void File::move(xyu::diff_t offset, MoveMode mode) const
    {
#if XY_DEBUG
        if (XY_UNLIKELY(!h)) no_handle(__LINE__, __func__);
        if (XY_UNLIKELY(mode != CUR && mode != BEG && mode != END)) {
            xyloge(0, "E_Logic_Invalid_Argument: invalid move mode with value {:#b}", static_cast<int>(mode));
            throw xyu::E_Logic_Invalid_Argument{};
        }
        if (XY_UNLIKELY(offset > 0 && mode == END)) {
            xyloge(0, "E_Logic_Invalid_Argument: invalid move {} from end of file", offset);
            throw xyu::E_Logic_Invalid_Argument{};
        }
        if (XY_UNLIKELY(offset < 0 && mode == BEG)) {
            xyloge(0, "E_Logic_Invalid_Argument: invalid move {} from begin of file", offset);
            throw xyu::E_Logic_Invalid_Argument{};
        }
#endif
        if constexpr (sizeof(xyu::diff_t) > sizeof(long)) {
            if (XY_UNLIKELY(nt<long>::max < offset)) {
                if (XY_UNLIKELY(::fseek(f(h), nt<long>::max, mode))) seek_error(__LINE__, __func__, errno);
                return move(offset - nt<long>::max, CUR);
            } else if (XY_UNLIKELY(nt<long>::min > offset)) {
                if (XY_UNLIKELY(::fseek(f(h), nt<long>::min, mode))) seek_error(__LINE__, __func__, errno);
                return move(offset - nt<long>::min, CUR);
            }
        }
        if (XY_UNLIKELY(::fseek(f(h), offset, mode))) seek_error(__LINE__, __func__, errno);
    }

    void File::rebuf(void* buf, xyu::size_t size, BufferMode mode)
    {
#if XY_DEBUG
        if (XY_UNLIKELY(!h)) no_handle(__LINE__, __func__);
        if (XY_UNLIKELY(!buf)) null_buf(__LINE__, __func__);
        if (XY_UNLIKELY(mode != ALL && mode != LINE && mode != NONE)) {
            xyloge(0, "E_Logic_Invalid_Argument: invalid buffer mode with value {:#b}", static_cast<int>(mode));
            throw xyu::E_Logic_Invalid_Argument{};
        }
#endif
        constexpr int bufmode[] = {_IOFBF, _IOLBF, _IONBF};
        if (XY_UNLIKELY(::setvbuf(f(h), (char*)buf, bufmode[mode], size))) {

            xyloge(0, "E_Logic_Invalid_Argument: buffer size or mode is invalid");
            throw xyu::E_Logic_Invalid_Argument{};
        };
    }

    File File::ftmp()
    {
        FILE *h = ::tmpfile();
        if (!h) open_error(__LINE__, __func__, "*tmp", READ | TRUNC | BINARY);
        File f;
        f.h = h;
        return f;
    }
    File File::fin() noexcept
    {
        File f;
        f.h = stdin;
        return f;
    }
    File File::fout() noexcept
    {
        File f;
        f.h = stdout;
        return f;
    }
    File File::ferr() noexcept
    {
        File f;
        f.h = stderr;
        return f;
    }
}

#pragma clang diagnostic pop