#include "../path.h"
#include "../../xystring/fmt.h"
#include "../../xycoll/stack.h"
#include "../../xycoll/queue.h"

#ifdef _WIN32
#  include <windows.h>
#  include <shlwapi.h>
#  include <winioctl.h>
#else
#  include <unistd.h>
#  include <dirent.h>
#  include <fcntl.h>
#  include <cerrno>
#  include <cstdio>
#  include <cstdlib>
#  include <sys/stat.h>
#  if defined(__linux__)
#    include <sys/sendfile.h>
#  elif defined(__APPLE__)
#    include <copyfile.h>
#  endif
#endif

using namespace xyu;

/// 辅助宏与错误码
namespace
{
#ifdef _WIN32
    #define XY_GET_ERROR GetLastError()
    #define XY_SET_ERROR(e) SetLastError(e)
    #define XY_NOT_FOUND ERROR_PATH_NOT_FOUND
    #define XY_NOT_DIR ERROR_DIRECTORY
    #define XY_NO_PERMISSION ERROR_ACCESS_DENIED
    #define XY_LENGTH_LIMIT ERROR_FILENAME_EXCED_RANGE
    #define XY_NOMEM ERROR_NOT_ENOUGH_MEMORY
    #define XY_EIO ERROR_IO_DEVICE
    #define XY_LOOP ERROR_TOO_MANY_LINKS
    #define XY_EXIST ERROR_ALREADY_EXISTS
    #define XY_NOT_EMPTY ERROR_DIR_NOT_EMPTY
#else
    #define XY_GET_ERROR errno
    #define XY_SET_ERROR(e) (errno = e)
    #define XY_NOT_FOUND ENOENT
    #define XY_NOT_DIR ENOTDIR
    #define XY_NO_PERMISSION EACCES
    #define XY_LENGTH_LIMIT ENAMETOOLONG
    #define XY_NOMEM ENOMEM
    #define XY_EIO EIO
    #define XY_LOOP ELOOP
    #define XY_EXIST EEXIST
    #define XY_NOT_EMPTY ENOTEMPTY
#endif

#ifdef _WIN32
    #define XY_FUNC_EX_PARAM , String& tmp
    #define XY_CALL_EX_PARAM , tmp
    #define XY_FUNC_EX_PARAM2 , String& tmp1, String& tmp2
    #define XY_CALL_EX_PARAM2 , tmp, dst.tmp
    #define XY_CALL_EX_PARAM22 , tmp1, tmp2
#else
    #define XY_FUNC_EX_PARAM
    #define XY_CALL_EX_PARAM
    #define XY_FUNC_EX_PARAM2
    #define XY_CALL_EX_PARAM2
    #define XY_CALL_EX_PARAM22
#endif

    XY_NOINLINE XY_COLD
    int public_error(StringView path, const char* func, bool log) noexcept
    {
        int e = XY_GET_ERROR;
        switch (e)
        {
#ifdef _WIN32
        case ERROR_BAD_PATHNAME:
        case ERROR_INVALID_NAME:
            e = ES_Path::Invalid_Path;
            if (log) xyloge("{}: invalid path \"{}\"", func, path);
            break;
        case ERROR_BAD_NETPATH:
        case ERROR_NETNAME_DELETED:
        case ERROR_FILE_NOT_FOUND:
#endif
        case XY_NOT_FOUND:
            e = ES_Path::Not_Found;
            if (log) xyloge("{}: path \"{}\" is not found", func, path);
            break;
#ifdef _WIN32
        case ERROR_PRIVILEGE_NOT_HELD:
#endif
        case XY_NO_PERMISSION:
            e = ES_Path::No_Permission;
            if (log) xyloge("{}: permission denied to enter path \"{}\"", func, path);
            break;
        case XY_NOT_DIR:
            e = ES_Path::Not_Dir;
            if (log) xyloge("{}: path \"{}\" is not a directory", func, path);
            break;
        case XY_LENGTH_LIMIT:
            e = ES_Path::Length_Limit;
            if (log) xyloge("{}: path \"{}\" exceeded length limit", func, path);
            break;
        case XY_NOMEM:
            e = ES_Path::No_Memory;
            if (log) xylogf("{}: no memory to complete the operation", func);
            break;
#ifdef _WIN32
        case ERROR_FILE_EXISTS:
#endif
        case XY_EXIST:
            e = ES_Path::Path_Occupied;
            if (log) xyloge("{}: path \"{}\" already occupied by other file", func, path);
            break;
        case XY_LOOP:
            e = ES_Path::Loop_Refer;
            if (log) xyloge("{}: path \"{}\" forms a circular reference or has too many symbolic links", func, path);
            break;
#ifndef _WIN32
        case EISDIR:
#endif
        case XY_NOT_EMPTY:
            e = ES_Path::Dir_Not_Empty;
            if (log) xyloge("{}: directory \"{}\" is not empty", func, path);
            break;
#ifdef _WIN32
        case ERROR_NOT_READY:
        case ERROR_DEV_NOT_EXIST:
        case ERROR_DISK_FULL:
        case ERROR_NOT_SAME_DEVICE:
#else
        case ENOSPC:
        case EXDEV:
#endif
        case XY_EIO:
            e = ES_Path::IO_Failed;
            if (log) xyloge("{}: IO error occurred", func);
            break;
        default:
            E_Logic::abort_unknown_error(func, e);
        }
        return e;
    }
}

/// 辅助函数
namespace
{
#ifdef _WIN32
    // 将 utf-16 转为 utf-8
    template <typename Str>
    Str change_utf16to8(StringView u16s, Str&& dst) noexcept
    {
        auto size = WideCharToMultiByte(CP_UTF8, 0, (LPCWCH)u16s.data(), u16s.size() / 2, nullptr, 0, nullptr, nullptr);
        dst.reserve(size);
        dst.resize(WideCharToMultiByte(CP_UTF8, 0, (LPCWCH)u16s.data(), u16s.size() / 2, dst.data(), size, nullptr, nullptr));
        return xyu::forward<Str>(dst);
    }

    template <typename Str>
    Str change_utf16to8_app(StringView u16s, Str&& dst) noexcept
    {
        auto size = WideCharToMultiByte(CP_UTF8, 0, (LPCWCH)u16s.data(), u16s.size() / 2, nullptr, 0, nullptr, nullptr);
        size_t offest = dst.size();
        dst.reserve(offest + size);
        dst.resize(offest + WideCharToMultiByte(CP_UTF8, 0, (LPCWCH)u16s.data(), u16s.size() / 2, dst.data() + offest, size, nullptr, nullptr));
        return xyu::forward<Str>(dst);
    }

    template <typename Str>
    Str change_utf8to16(StringView u8s, Str&& dst) noexcept
    {
        auto wlen = MultiByteToWideChar(CP_UTF8, 0, u8s.data(), u8s.size(), nullptr, 0);
        dst.reserve(wlen * 2 + 1);
        dst.resize(MultiByteToWideChar(CP_UTF8, 0, u8s.data(), u8s.size(), (LPWSTR)dst.data(), wlen) * 2 + 1);
        dst[dst.size() - 1] = '\0';
        return xyu::forward<Str>(dst);
    }

    template <typename Str>
    Str change_utf8to16_app(StringView u8s, Str&& dst) noexcept
    {
        // wchar: ... (0, 0) ...  (0, 0)
        // index: ... s-1 s  ... ns-1 ns
        auto wlen = MultiByteToWideChar(CP_UTF8, 0, u8s.data(), u8s.size(), nullptr, 0);
        size_t offest = max(dst.size(), 1) - 1;
        dst.reserve(offest + wlen * 2 + 1);
        dst.resize(offest + MultiByteToWideChar(CP_UTF8, 0, u8s.data(), u8s.size(), (LPWSTR)(dst.data() + offest), wlen) * 2 + 1);
        dst[dst.size() - 1] = '\0';
        return xyu::forward<Str>(dst);
    }
#endif
}

/// 路径设置修改
namespace xylu::xysystem
{
    Path& Path::set(StringView path) noexcept
    {
        pth.resize(path.size());
        path.replace(pth.data(), '\\', '/');
#ifdef _WIN32
        // 盘符必须以 '/' 结尾
        if (pth.size() == 2 && pth[1] == ':' && pth[0] >= 'A' && pth[0] <= 'Z') pth << '/';
#endif
        return *this;
    }

    Path& Path::concat(StringView path) noexcept
    {
        if (path.empty()) return *this;
        if (!pth.empty())
        {
            if (path[0] != '/' && path[0] != '\\') { if (pth[pth.size() - 1] != '/') pth << '/'; }
            else if (pth[pth.size() - 1] == '/') path = path.subview(1);
        }
        size_t s = pth.size();
        pth.resize(s + path.size());
        path.replace(pth.data() + s, '\\', '/');
#ifdef _WIN32
        // 盘符必须以 '/' 结尾
        if (pth.size() == 2 && pth[1] == ':') pth << '/';
        else if (pth.size() > 3 && pth[pth.size() - 1] == '/') pth.resize(pth.size() - 1);
#else
        // 去除末尾 '/'
        if (pth.size() > 1 && pth[pth.size() - 1] == '/') pth.resize(pth.size() - 1);
#endif
        return *this;
    }
}

/// 工作路径
namespace xylu::xysystem
{
    static bool get_dir_help(String& dst)
    {
        dst.clear();
#ifdef _WIN32
        auto len = GetCurrentDirectoryW(0, nullptr);
        if (XY_UNLIKELY(len == 0)) return false;
        dst.reserve(len * 2);
        len = GetCurrentDirectoryW(len, (LPWSTR)dst.data());
        if (XY_UNLIKELY(len == 0)) return false;
        dst.resize(len * 2);
#elif defined __linux__
        if (!getcwd(dst.data(), dst.capacity() + 1))
        {
            if (errno != ERANGE) return false;
            char *p = getcwd(nullptr, 0);
            if (XY_UNLIKELY(!p)) return false;
            size_t size = StringView{p}.size();
            dst.resize(size);
            mem_copy(dst.data(), p, size);
            free(p);
        }
        else dst.resize(StringView{dst.data()}.size());
#else
        for (;;)
        {
            if (getcwd(dst.data(), dst.capacity() + 1))
            {
                dst.resize(StringView{dst.data()}.size());
                break;
            }
            if (errno != ERANGE) return false;
            if (dst.capacity() < 1024) dst.reserve(dst.capacity() * 4);
            else dst.reserve(dst.capacity() * 2);
        }
#endif
        return true;
    }

    Path& Path::get_dir()
    {
#ifdef _WIN32
        if (XY_UNLIKELY(!get_dir_help(tmp)))
            throw E_Path{public_error(pth, "xyu::Path::get_dir", true)};
        pth.clear();
        change_utf16to8(tmp, pth);
        pth.replace(pth.data(), '\\', '/');
#else
        if (XY_UNLIKELY(!get_dir_help(pth)))
            throw E_Path{public_error(pth, "xyu::Path::get_dir", true)};
#endif
        return *this;
    }

    Path& Path::get_dir(ES_Path& state) noexcept
    {
#ifdef _WIN32
        if (XY_UNLIKELY(!get_dir_help(tmp)))
            state = public_error(pth, "xyu::Path::get_dir", false);
        else state = ES_Path::Success;
        pth.clear();
        change_utf16to8(tmp, pth);
        pth.replace(pth.data(), '\\', '/');
#else
        if (XY_UNLIKELY(!get_dir_help(pth)))
            state = public_error(pth, "xyu::Path::get_dir", false);
        else state = ES_Path::Success;
#endif
        return *this;
    }

    void Path::set_dir() const
    {
#ifdef _WIN32
        change_utf8to16(pth, tmp);
        if (XY_UNLIKELY(!SetCurrentDirectoryW((LPCWSTR)tmp.data())))
#else
        if (XY_UNLIKELY(chdir(pth.data())))
#endif
            throw E_Path{public_error(pth.data(), "xyu::Path::set_dir", true)};
    }

    void Path::set_dir(ES_Path& state) const noexcept
    {
#ifdef _WIN32
        change_utf8to16(pth, tmp);
        if (XY_UNLIKELY(!SetCurrentDirectoryW((LPCWSTR)tmp.data())))
#else
        if (XY_UNLIKELY(chdir(pth.data())))
#endif
            state = public_error(pth.data(), "xyu::Path::set_dir", false);
        else state = ES_Path::Success;
    }
}

/// 相对|绝对路径
namespace xylu::xysystem
{
    static bool is_absolute_help(StringView path) noexcept
    {
#ifdef _WIN32
        if (path.empty() || path[0] == '.') return false;
        if (path[0] == '/') return path.size() > 1 && path[1] == '/';
        return path.size() >= 3 && path[1] == ':' && path[2] == '/';
#else
        return !path.empty() && path[0] == '/';
#endif
    }

    bool Path::is_absolute() const noexcept { return is_absolute_help(pth); }

#ifdef _WIN32
    static bool to_absolute_help(StringView src, String& dst, String& tmp)
    {
        change_utf8to16(src, dst);
        auto wlen = GetFullPathNameW((LPWSTR)dst.data(), 0, nullptr, nullptr);
        if (XY_UNLIKELY(wlen == 0)) return false;
        tmp.reserve(wlen * 2 + 1);
        tmp.resize(GetFullPathNameW((LPWSTR)dst.data(), wlen, (LPWSTR)tmp.data(), nullptr) * 2 + 1);
        tmp[tmp.size() - 1] = '\0';
        change_utf16to8(tmp, dst);
        dst.replace(dst.data(), '\\', '/');
        if (dst.size() > 3 && dst[dst.size() - 1] == '/') dst.resize(dst.size() - 1);
        return true;
    }
#else
    template <typename Func>
    static bool to_absolute_help(StringView src, String& dst, Func get_dir)
    {
        dst.clear();

        // 处理起始相对路径
        if (src.empty()) { return get_dir(dst); }
        if (src[0] != '/')
        {
            if (src[0] != '.' || src.size() <= 1 || src[1] == '/')
            {
                if (XY_UNLIKELY(!get_dir(dst))) return false;
                if (src[0] == '.') src = src.subview(2);
            }
            else if (src[1] == '.' && (src.size() == 2 || src[2] == '/'))
            {
                if (XY_UNLIKELY(!get_dir(dst))) return false;
                if (dst.size() > 1) dst.resize(dst.rfind('/', 0, dst.size()));
                src = src.subview(3);
            }
            if (dst.size() != 1) dst << '/';
        }

        // 处理后续
        for (size_t i, j = 0;;)
        {
            if (j >= src.size() || ((i = src.find('.', j)) == -1)) { dst << src; break; }
            if (i == 0 || src[i - 1] == '/')
            {
                if (i + 1 == src.size() || src[i + 1] == '/')
                {
                    dst << src.subview(0, i);
                    if (i + 1 == src.size()) break;
                    src = src.subview(i + 2);
                    i = -1;
                }
                else if (src[i + 1] == '.' && (i + 2 == src.size() || src[i + 2] == '/'))
                {
                    if (i) dst << src.subview(0, src.rfind('/', 0, i - 1) + 1);
                    else if (dst.size() > 1) dst.resize(dst.rfind('/', 0, max(dst.size(), 1) - 1) + 1);
                    if (i + 2 == src.size()) break;
                    src = src.subview(i + 3);
                    i = -1;
                }
            }
            j = i + 1;
        }

        // 去除末尾 '/'
        if (dst.size() > 1 && dst[dst.size() - 1] == '/') dst.resize(dst.size() - 1);
        else if (XY_UNLIKELY(dst.empty())) dst << '/';
        return true;
    }
#endif

    Path Path::to_absolute() const
    {
        Path ret;
#ifdef _WIN32
        if (XY_UNLIKELY(!to_absolute_help(pth, ret.pth, tmp)))
#else
        if (XY_UNLIKELY(!to_absolute_help(pth, ret.pth, [](String& dst){ return get_dir_help(dst); })))
#endif
            throw E_Path{public_error(pth, "xyu::Path::to_absolute", true)};
        return ret;
    }

    Path Path::to_absolute(ES_Path& state) const noexcept
    {
        Path ret;
#ifdef _WIN32
        if (XY_UNLIKELY(!to_absolute_help(pth, ret.pth, tmp)))
#else
        if (XY_UNLIKELY(!to_absolute_help(pth, ret.pth, [](String& dst){ return get_dir_help(dst); })))
#endif
            state = public_error(pth, "xyu::Path::to_absolute", false);
        else state = ES_Path::Success;
        return ret;
    }

#ifdef _WIN32
    static bool to_relative_help(StringView src, String& dst, String& tmp1, String& tmp2)
#else
    static bool to_relative_help(StringView src, String& dst)
#endif
    {
        // 绝对路径
        if (is_absolute_help(src))
        {
#ifdef _WIN32
            if (XY_UNLIKELY(!to_absolute_help(src, dst, tmp1))) return false;
            if (XY_UNLIKELY(!get_dir_help(tmp1))) return false;
            change_utf16to8(tmp1, dst);
            dst.replace(dst.data(), '\\', '/');
            auto& cur = dst;
            // 不可转换
            if (mem_cmp(cur.data(), dst.data(), 3) != 0)
            {
                SetLastError(ERROR_BAD_PATHNAME);
                return false;
            }
#else
            String cur;
            if (XY_UNLIKELY(!get_dir_help(cur))) return false;
            if (XY_UNLIKELY(!to_absolute_help(src, dst, [](String&){ return true; }))) return false;
#endif
            size_t i = cur.contrast(dst);
            if (i == cur.size()) dst.erase(0, cur.size() + 1);
            else
            {
                size_t prev_nodes = 1 + cur.subview(i).tally('/');
                size_t rel_index = dst.rfind('/', 0, i);
                size_t last = dst.size() - rel_index - 1;
                mem_move(dst.data() + 3 * prev_nodes, dst.data() + rel_index + 1, last);
                dst.resize(3 * prev_nodes + last);
                for (size_t j = 0; j < 3 * prev_nodes; j += 3) dst[j] = dst[j + 1] = '.', dst[j + 2] = '/';
            }
        }
        // 相对路径
        else
        {
            auto sv = src.view();
            size_t keep_pos = 0; // 最后一个不可抵消的 "../" 的边界位置

            for (size_t i, j = 0;;)
            {
                if (j >= sv.size() || ((i = sv.find('.', j)) == -1)) { dst << sv; break; }

                if (i == 0 || sv[i - 1] == '/')
                {
                    if (i + 1 == sv.size() || sv[i + 1] == '/')
                    {
                        dst << sv.subview(0, i);
                        if (i + 1 == sv.size()) break;
                        sv = sv.subview(i + 2);
                        i = -1;
                    }
                    else if (sv[i + 1] == '.' && (i + 2 == sv.size() || sv[i + 2] == '/'))
                    {
                        if (i) dst << sv.subview(0, sv.rfind('/', 0, i - 1) + 1);
                        else if (dst.size() > keep_pos) dst.resize(dst.rfind('/', 0, max(dst.size(), 1) - 1) + 1);
                        else dst << "../", keep_pos = dst.size();
                        if (i + 2 == sv.size()) break;
                        sv = sv.subview(i + 3);
                        i = -1;
                    }
                }
                j = i + 1;
            }

#ifdef _WIN32
            if (dst.size() > 3 && dst[dst.size() - 1] == '/') dst.resize(dst.size() - 1);
#else
            if (dst.size() > 1 && dst[dst.size() - 1] == '/') dst.resize(dst.size() - 1);
#endif
        }
        if (dst.empty()) dst << '.';
        return true;
    }

    Path Path::to_relative() const
    {
        Path ret;
#ifdef _WIN32
        if (XY_UNLIKELY(!to_relative_help(pth, ret.pth, tmp, ret.tmp)))
#else
        if (XY_UNLIKELY(!to_relative_help(pth, ret.pth)))
#endif
            throw E_Path{public_error(pth, "xyu::Path::to_relative", true)};
        return ret;
    }

    Path Path::to_relative(ES_Path& state) const noexcept
    {
        Path ret;
#ifdef _WIN32
        if (XY_UNLIKELY(!to_relative_help(pth, ret.pth, tmp, ret.tmp)))
#else
        if (XY_UNLIKELY(!to_relative_help(pth, ret.pth)))
#endif
            state = public_error(pth, "xyu::Path::to_relative", false);
        return ret;
    }
}

/// 路径信息
namespace xylu::xysystem
{
    static Path::Type parse_file_type(uint32 mode) noexcept
    {
#ifdef _WIN32
        if (mode & FILE_ATTRIBUTE_REPARSE_POINT) return Path::Link;
        if (mode & FILE_ATTRIBUTE_DIRECTORY) return Path::Directory;
        return Path::RegFile;
#else
        if (S_ISREG(mode)) return Path::RegFile;
        if (S_ISDIR(mode)) return Path::Directory;
        if (S_ISCHR(mode)) return Path::CharDev;
        if (S_ISBLK(mode)) return Path::BlockDev;
        if (S_ISLNK(mode)) return Path::Link;
        if (S_ISSOCK(mode)) return Path::Socket;
        if (S_ISFIFO(mode)) return Path::Fifo;
        return Path::Unknown;
#endif
    }

    Path::Type Path::get_type() const
    {
#ifdef _WIN32
        change_utf8to16(pth, tmp);
        if (uint32 r = GetFileAttributesW((LPCWSTR)tmp.data()); XY_LIKELY(r != INVALID_FILE_ATTRIBUTES))
            return parse_file_type(r);
#else
        struct stat st;
        if (XY_LIKELY(!lstat(pth.data(), &st))) return parse_file_type(st.st_mode);
#endif
        throw E_Path{public_error(pth, "xyu::Path::get_type", true)};
    }

    Path::Type Path::get_type(ES_Path& state) const noexcept
    {
        state = ES_Path::Success;
#ifdef _WIN32
        change_utf8to16(pth, tmp);
        if (uint32 r = GetFileAttributesW((LPCWSTR)tmp.data()); XY_LIKELY(r != INVALID_FILE_ATTRIBUTES))
            return parse_file_type(r);
#else
        struct stat st;
        if (XY_LIKELY(!lstat(pth.data(), &st))) return parse_file_type(st.st_mode);
#endif
        state = public_error(pth, "xyu::Path::get_type", false);
        return None;
    }
}

/// 目录遍历
namespace xylu::xysystem
{
#ifdef _WIN32
    struct WinDirHandle : class_no_copy_t
    {
    private:
        HANDLE h = nullptr;
        uint8* buf = nullptr;
        uint32 size = 0;
        uint32 offset = 0;

        // size 最高位存储是否出现错误
        static constexpr uint32 valid = (uint32(-1) >> 1) + 1;
        // size 次高位存储是否为初次遍历
        static constexpr uint32 first = valid >> 1;
        // size 次次高位存储是否禁止扩容
        static constexpr uint32 fixed = first >> 1;
        // size 有效部分
        static constexpr uint32 mask = fixed - 1;

        // 初始堆容量
        static constexpr uint32 start_size = 4 * 1024;
        static constexpr uint32 max_size = 64 * 1024;

        struct Data
        {
        private:
            FILE_ID_BOTH_DIR_INFO* p;

        public:
            Data(FILE_ID_BOTH_DIR_INFO* p) noexcept : p(p) {}
            explicit operator bool() const noexcept { return p; }

            uint32 length() const noexcept { return p->FileNameLength; }
            wchar_t* name() noexcept { return p->FileName; }
            Path::Type type() const noexcept { return parse_file_type(p->FileAttributes); }
        };

    public:
        explicit WinDirHandle(const char* path) noexcept
        {
            buf = alloc<uint8>(nullptr, start_size);
            if (XY_UNLIKELY(!buf)) { SetLastError(ERROR_NOT_ENOUGH_MEMORY); return; }
            size = start_size;

            h = CreateFileW((const wchar_t*)path,
                FILE_LIST_DIRECTORY,
                FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
                nullptr,
                OPEN_EXISTING,
                FILE_FLAG_BACKUP_SEMANTICS,
                nullptr);
            if (XY_UNLIKELY(h == INVALID_HANDLE_VALUE)) { h = nullptr; return; }
            size |= valid | first;
        }

        WinDirHandle(WinDirHandle&& other) noexcept
        {
            mem_copy(this, &other, 1);
            mem_set(&other, '\0', 1);
        }

        ~WinDirHandle() noexcept
        {
            if (buf) dealloc((uint8*)buf, size & mask);
            if (h) CloseHandle(h);
        }

        Data next() noexcept
        {
            FILE_ID_BOTH_DIR_INFO* ret = nullptr;
            if (!*this) return ret;

            if (offset == 0)
            {
                FILE_INFO_BY_HANDLE_CLASS infoClass;
                if (size & first) { infoClass = FileIdBothDirectoryRestartInfo; size &= ~first; }
                else
                {
                    infoClass = FileIdBothDirectoryInfo;
                    // 尝试扩容
                    if (!(size & fixed))
                    {
                        uint32 oldsize = size & mask;
                        if (oldsize < max_size)
                        {
                            uint32 newsize = oldsize * 2;
                            uint8* newbuf = alloc<uint8>(nullptr, newsize);
                            if (XY_LIKELY(newbuf))
                            {
                                dealloc((uint8*)buf, oldsize);
                                buf = newbuf;
                                size = newsize | (size & ~mask);
                                if (newsize >= max_size) size |= fixed;
                            }
                        }
                    }
                }

                // 读取文件
                if (XY_UNLIKELY(!GetFileInformationByHandleEx(h, infoClass, buf, size & mask)))
                {
                    if (GetLastError() != ERROR_NO_MORE_FILES) size &= ~valid;
                    return ret;
                }
                ret = (FILE_ID_BOTH_DIR_INFO*)buf;
            }
            else ret = (FILE_ID_BOTH_DIR_INFO*)(buf + offset);

            if (ret->NextEntryOffset == 0)
            {
                if (offset + 1024 < (size & mask)) size |= fixed;
                offset = 0;
            }
            else offset += ret->NextEntryOffset;

            return ret;
        }

        explicit operator bool() const noexcept { return size & valid; }
    };

    using DirHandle = WinDirHandle;

#else
    struct UnixDirHandle : class_no_copy_t
    {
    private:
        DIR* h = nullptr;

        struct Data
        {
        private:
            dirent* p;

        public:
            Data(dirent* p) noexcept : p(p) {}
            explicit operator bool() const noexcept { return p; }

            char* name() noexcept { return p->d_name; }
            Path::Type type() const noexcept
            {
                switch (p->d_type)
                {
                case DT_REG:  return Path::Type::RegFile;
                case DT_DIR:  return Path::Type::Directory;
                case DT_LNK:  return Path::Type::Link;
                case DT_BLK:  return Path::Type::BlockDev;
                case DT_CHR:  return Path::Type::CharDev;
                case DT_FIFO: return Path::Type::Fifo;
                case DT_SOCK: return Path::Type::Socket;
                default:      return Path::Type::Unknown;
                }
            }
        };

    public:
        explicit UnixDirHandle(const char* path) noexcept { h = opendir(path); }

        UnixDirHandle(UnixDirHandle&& other) noexcept : h(other.h) { other.h = nullptr; }

        ~UnixDirHandle() noexcept { if (h) closedir(h); }

        Data next() noexcept
        {
            errno = 0;
            return readdir(h);
        }

        explicit operator bool() const noexcept { return errno == 0; }
    };

    using DirHandle = UnixDirHandle;
#endif

    struct DInfo { DirHandle h; uint32 flag; };
    auto& to_stack(void* handle) noexcept { return *(NearStack<DInfo>*)handle; }

    struct BInfo { DirHandle h; String fold; };
    auto& to_queue(void* handle) noexcept { return *(NearQueue<BInfo>*)handle; }

    enum Flag : uint32
    {
        // 0-15位，select_type
        Skip_self   = 1u << 16,     // 是否处理.
        Skip_prev   = 1u << 17,     // 是否处理..
        Skip_sp     = Skip_prev | Skip_self,
        Walk_finish = 1u << 18,     // 是否遍历结束
        DFS         = 1u << 20,     // 深度递归
        BFS         = 1u << 21,     // 广度递归
        Recursion   = DFS | BFS,    // 是否递归
        New_Enter   = 1u << 22,     // 进入新目录
    };

    static bool walk_open(void*& handle, StringView path, uint& flag, String& fold XY_FUNC_EX_PARAM) noexcept
    {
        // 分配资源
        if (flag & Recursion)
        {
            if (flag & DFS) handle = alloc<NearStack<DInfo>>(nullptr, 1);
            else handle = alloc<NearQueue<BInfo>>(nullptr, 1);
            if (XY_UNLIKELY(!handle)) { XY_SET_ERROR(XY_NOMEM); return false; }
            if (flag & DFS) new (handle) NearStack<DInfo>;
            else new (handle) NearQueue<BInfo>;
        }
        new (&fold) String;

        // 预处理路径
        if (path.empty()) fold << "./";
        else
        {
            fold << path;
            if (fold[fold.size() - 1] != '/') fold << '/';
        }

#ifdef _WIN32
        change_utf8to16(fold, tmp);
        if (flag & Recursion) fold.clear(), fold << tmp;
        DirHandle dh(tmp.data());
        if (!(flag & Recursion)) fold.~CompactString();
#else
        DirHandle dh(fold.data());
        if (!(flag & Recursion) && XY_UNLIKELY(!dh)) fold.~CompactString();
#endif
        if (XY_UNLIKELY(!dh)) return false;

        static_assert(sizeof(void*) + sizeof(String) >= sizeof(DirHandle));
        if (!(flag & Recursion)) new (&handle) DirHandle(move(dh));
        else if (flag & DFS) to_stack(handle).push(move(dh));
        else to_queue(handle).push(move(dh));
        flag &= ~Walk_finish;
        return true;
    }

    // 初始化 flag
    static uint32& init_flag(uint32& flag, uint type, Path::WalkType wt) noexcept
    {
        static_assert(Path::Unknown <= uint16(-1));
        flag |= (type & uint16(-1));
        if (wt == Path::WT_DFS) flag |= DFS;
        else if (wt == Path::WT_BFS) flag |= BFS;
        return flag;
    }

    Path::Walker Path::walk(WalkType wt, uint select_type) const
    {
        Walker walker;
        if (XY_UNLIKELY(!walk_open(walker.handle, pth, init_flag(walker.flag, select_type, wt), walker.fold XY_CALL_EX_PARAM)))
            throw E_Path{public_error(pth, "xyu::Path::walk", true)};
        return walker;
    }
    Path::Walker Path::walk(WalkType wt, uint select_type, ES_Path& state) const noexcept
    {
        Walker walker;
        if (XY_UNLIKELY(!walk_open(walker.handle, pth, init_flag(walker.flag, select_type, wt), walker.fold XY_CALL_EX_PARAM)))
            state = public_error(pth, "xyu::Path::walk", false);
        else state = ES_Path::Success;
        return walker;
    }
    Path::Walker Path::walk(WalkType wt, ES_Path& state) const noexcept { return walk( wt, -1, state); }
    Path::Walker Path::walk(ES_Path& state) const noexcept { return walk( WT_Single, -1, state); }

#ifdef _WIN32
    static size_t find_s(StringView u16s, StringView u16c, size_t start, size_t end) noexcept
    {
        for (;;)
        {
            size_t index = u16s.find(u16c, start, end);
            if (index == -1 || !(index & 1)) return index;
            start = index + 1;
        }
    }
    static size_t rfind_s(StringView u16s, StringView u16c, size_t start, size_t end) noexcept
    {
        for (;;)
        {
            size_t index = u16s.rfind(u16c, start, end);
            if (index == -1 || !(index & 1)) return index;
            end = u16c.size() + index - 1;
        }
    }
#endif

    // 回退到目录  a/b --> a/  a/b/ --> a/b/
    static void to_fold(String& fold) noexcept { fold.resize(fold.rfind('/') + 1); }
    // 回退一层目录  a/b/ --> a/
    static void prev_fold(String& fold) noexcept
    {
        fold.resize(fold.rfind('/', 0, max(fold.size(), 1) - 1) + 1);
    }
    static void prev_fold_s(String& fold) noexcept
    {
#ifdef _WIN32
        // wchar: .... (/   0) (X   X) ... (/   0)  (0   0)
        // index: 0 ... i      ns-1 ns ... s-3 s-2 s-1  s
        StringView slash{(const char*)L"/", 2};
        size_t index = rfind_s(fold, slash, 0, max(fold.size(), 3) - 3);
        if (index == -1) return fold.clear();
        fold.resize(index + 3);
        fold[index + 2] = '\0';
#else
        prev_fold(fold);
#endif
    }

    static bool walk_next(void* handle, uint& flag, Path::Type& type, String& name, String& fold) noexcept
    {
        // name 为 utf8，fold 为 utf16(win)/utf8(unix)
        // fold utf16 下 包含 一个隐式'\0'(resize设置) 和 一个额外'\0'(fold[size-1]设置)
        // DFS 下 fold / name 同步推进；BFS 下 fold 不变，name 前缀部分存在队列中

        DirHandle* dh;

        if (!(flag & Recursion))
        {
            name.clear();
            dh = (DirHandle*)&handle;
        }
        else if (flag & DFS)
        {
            // 深搜进入下一层时添加 /
            if (flag & New_Enter) name << '/', flag &= ~New_Enter;
            // 同一层内回到目录
            else to_fold(name);
            dh = &to_stack(handle).get(0).h;
        }
        else
        {
            // 广搜回到目录
            to_fold(name);
            dh = &to_queue(handle).head().h;
        }

        for (;;)
        {
            // 循环开始时，name以'/'结尾或为空，fold一定以'/'结尾
            auto data = dh->next();
            if (XY_UNLIKELY(!data))
            {
                if (!*dh) return false;
                if (!(flag & Recursion)) goto over;

                if (flag & DFS)
                {
                    // 深搜回退到上一层，fold / name / flag_skip
                    prev_fold(name);
                    prev_fold_s(fold);
                    auto& stack = to_stack(handle);
                    stack.pop(1);
                    if (stack.empty()) goto over;
                    auto& info = stack.get(0);
                    dh = &info.h;
                    flag &= ~Skip_sp;
                    flag |= info.flag;
                }
                else
                {
                    auto& queue = to_queue(handle);
                    queue.pop(1);
                    if (queue.empty()) goto over;
                    auto& info = queue.get(0);
                    dh = &info.h;
                    name = info.fold;
                    flag &= ~Skip_sp;
                }
                continue;
            }

#ifdef _WIN32
            if ((flag & Skip_sp) != Skip_sp)
            {
                if (data.length() <= 4 && data.name()[0] == L'.')
                {
                    if (data.length() == 2) { flag |= Skip_self; continue; }
                    if (data.name()[1] == L'.') { flag |= Skip_prev; continue; }
                }
            }
            StringView ws{(char*)data.name(), data.length()};

            type = data.type();
            if (flag & Recursion && type & Path::Directory)
            {
                if (flag & DFS)
                {
                    auto oldsize = fold.size();
                    fold.resize(fold.size() + ws.size());
                    mem_copy(fold.data() + oldsize - 1, ws.data(), ws.size());
                    fold[fold.size() - 1] = '\0';
                    DirHandle newdh(fold.data());
                    if (XY_UNLIKELY(!newdh))
                    {
                        fold.resize(oldsize);
                        fold[fold.size() - 1] = '\0';
                    }
                    else
                    {
                        change_utf8to16_app("/", fold);
                        auto& stack = to_stack(handle);
                        stack.get(0).flag = flag & Skip_sp;
                        stack.push(move(newdh));
                        flag &= ~Skip_sp;
                        change_utf16to8_app(ws, name);
                        if (type & flag) { flag |= New_Enter; return true; }
                        name << '/';
                        dh = &stack.get(0).h;
                        continue;
                    }
                }
                else
                {
                    auto oldsize = fold.size();
                    change_utf8to16_app(name, fold);
                    auto tmpsize = fold.size();
                    fold.resize(tmpsize + ws.size());
                    mem_copy(fold.data() + tmpsize - 1, ws.data(), ws.size());
                    fold[fold.size() - 1] = '\0';
                    DirHandle newdh(fold.data());
                    fold.resize(oldsize);
                    fold[fold.size() - 1] = '\0';

                    oldsize = name.size();
                    change_utf16to8_app(ws, name);
                    if (XY_LIKELY(newdh))
                    {
                        to_queue(handle).push(special_v, move(newdh), name + '/');
                        dh = &to_queue(handle).head().h;    // 若触发扩容重新获取指针
                    }
                    if (type & flag) return true;
                    name.resize(oldsize);
                    continue;
                }
            }
            if (type & flag)
            {
                change_utf16to8_app(ws, name);
                return true;
            }

#else
            const char* sname = data.name();
            if ((flag & Skip_sp) != Skip_sp)
            {
                if (sname[0] == '.')
                {
                    if (sname[1] == '\0') { flag |= Skip_self; continue; }
                    if (sname[1] == '.' && sname[2] == '\0') { flag |= Skip_prev; continue; }
                }
            }

            type = data.type();
            if (XY_UNLIKELY(type == Path::Unknown))
            {
                auto old_fsize = fold.size();
                if (flag & BFS) fold << name;
                fold << sname;
                struct stat st;
                if (XY_LIKELY(lstat(fold.data(), &st) == 0))
                    type = parse_file_type(st.st_mode);
                fold.resize(old_fsize);
            }

            if (flag & Recursion && type & Path::Directory)
            {
                if (flag & DFS)
                {
                    auto size = fold.size();
                    fold << sname << '/';
                    DirHandle newdh(fold.data());
                    if (XY_UNLIKELY(!newdh)) fold.resize(size);
                    else
                    {
                        auto& stack = to_stack(handle);
                        stack.get(0).flag = flag & Skip_sp;
                        stack.push(move(newdh));
                        flag &= ~Skip_sp;
                        name << sname;
                        if (type & flag) { flag |= New_Enter; return true; }
                        name << '/';
                        dh = &stack.get(0).h;
                        continue;
                    }
                }
                else
                {
                    auto oldsize = name.size();
                    name << sname;
                    DirHandle newdh((fold + name).data());
                    if (XY_LIKELY(newdh))
                    {
                        to_queue(handle).push(special_v, move(newdh), name + '/');
                        dh = &to_queue(handle).head().h;    // 若触发扩容重新获取指针
                    }
                    if (type & flag) return true;
                    name.resize(oldsize);
                    continue;
                }
            }
            if (type & flag)
            {
                name << sname;
                return true;
            }
#endif
        }

    over:
        flag |= Walk_finish;
        return true;
    }

    bool Path::Walker::next()
    {
        if (flag & Walk_finish) return false;
        if (XY_UNLIKELY(!walk_next(handle, flag, type, name, fold)))
            throw E_Path{public_error(name, "xyu::Path::Walker::next", true)};
        return !(flag & Walk_finish);
    }
    bool Path::Walker::next(ES_Path& state) noexcept
    {
        if (flag & Walk_finish) return false;
        if (XY_UNLIKELY(!walk_next(handle, flag, type, name, fold)))
            state = public_error(name, "xyu::Path::Walker::next", false);
        else state = ES_Path::Success;
        return !(flag & Walk_finish);
    }


    Path::Walker::Walker() noexcept : handle(nullptr), flag(Walk_finish) {}

    Path::Walker::~Walker() noexcept
    {
        if (!handle) return;
#ifndef _WIN32
        fold.~CompactString();
#endif
        if (flag & Recursion)
        {
#ifdef _WIN32
            fold.~CompactString();
#endif
            if (flag & DFS)
            {
                auto& stack = to_stack(handle);
                stack.~NearStack();
                dealloc(&stack, 1);
            }
            else
            {
                auto& queue = to_queue(handle);
                queue.~NearQueue();
                dealloc(&queue, 1);
            }
        }
        else (*(DirHandle*)&handle).~DirHandle();
    }

    Path::Walker::operator bool() const noexcept { return !(flag & Walk_finish); }

    Path::Walker::Walker(Walker&& other) noexcept
    {
        mem_copy(this, &other, 1);
        mem_set(&other, '\0', 1);
        other.flag |= Walk_finish;
    }

    Path::Walker& Path::Walker::operator=(Walker&& other) noexcept
    {
        char buf[sizeof(Walker)];
        mem_copy((Walker*)buf, this, 1);
        mem_copy(this, &other, 1);
        mem_copy(&other, (Walker*)buf, 1);
        return *this;
    }

}

/// 目录创建
namespace xylu::xysystem
{
#ifndef _WIN32
    static size_t find_s(StringView u16s, char c, size_t start, size_t end) noexcept
    {
        return u16s.find(c, start, end);
    }
    static size_t rfind_s(StringView u16s, char c, size_t start, size_t end) noexcept
    {
        return u16s.rfind(c, start, end);
    }
#endif

    static bool create_help(StringView pth XY_FUNC_EX_PARAM) noexcept
    {
#ifdef _WIN32
        change_utf8to16(pth, tmp);
        return CreateDirectoryW((LPCWSTR)tmp.data(), nullptr);
#else
        return ::mkdir(pth.data(), 0777) == 0;
#endif
    }

    static bool creates_help(StringView pth XY_FUNC_EX_PARAM) noexcept
    {
        if (pth.empty()) { XY_SET_ERROR(XY_NOT_FOUND); return false; }

        // 直接尝试创建（win 下完成唯一一次转码）
        if (create_help(pth XY_CALL_EX_PARAM)) return true;

        int err = XY_GET_ERROR;
        // 如果已存在检查是否为目录
        if (err == XY_EXIST)
        {
#ifdef _WIN32
            uint32 attr = GetFileAttributesW((LPCWSTR)tmp.data());
            if (attr != INVALID_FILE_ATTRIBUTES && (attr & FILE_ATTRIBUTE_DIRECTORY)) return true;
#else
            struct stat st;
            if (lstat(pth.data(), &st) == 0 && S_ISDIR(st.st_mode)) return true;
#endif
            XY_SET_ERROR(err);
            return false;
        }
        if (err != XY_NOT_FOUND) return false;

        // 准备缓冲区与边界，用于二分查找和原地截断
#ifdef _WIN32
        String& cur = tmp;
        if (pth[pth.size() - 1] != '/') change_utf8to16_app("/", cur);
        constexpr size_t step = 2;
        StringView slash{(const char*)L"/", 2};
        size_t end = cur.size() - 1; // 排除末尾额外'\0'

        size_t low = 0;
        if (end >= 6 && mem_cmp(cur.data() + 2, (const char*)L":/", 4) == 0) low = 6;
        else if (end >= 4 && mem_cmp(cur.data(), (const char*)L"//", 4) == 0) low = 4;
        else if (end >= 2 && *(wchar_t*)cur.data() == L'/') low = 2;
#else
        String cur = pth;
        if (pth[pth.size() - 1] != '/') cur << '/';
        constexpr size_t step = 1;
        char slash = '/';
        size_t end = cur.size();

        size_t low = (end >= 1 && cur[0] == '/') ? 1 : 0;
#endif
        size_t high = end - step;   // 排除末尾 '/'，只查找父级

        auto is_dir_at = [&](size_t pos) noexcept -> int
        {
#ifdef _WIN32
            wchar_t* wp = (wchar_t*)(cur.data() + pos);
            *wp = L'\0';
            DWORD attr = GetFileAttributesW((LPCWSTR)cur.data());
            *wp = L'/';
            if (attr == INVALID_FILE_ATTRIBUTES)
            {
                DWORD e = GetLastError();
                if (e == ERROR_FILE_NOT_FOUND || e == ERROR_PATH_NOT_FOUND) return 0;
                return -1;
            }
            if (attr & FILE_ATTRIBUTE_DIRECTORY) return 1;
#else
            cur[pos] = '\0';
            struct stat st;
            int r = ::stat(cur.data(), &st);
            cur[pos] = '/';
            if (r != 0)
            {
                if (errno == ENOENT) return 0;
                return -1;
            }
            if (S_ISDIR(st.st_mode)) return 1;
#endif
            XY_SET_ERROR(XY_EXIST);
            return -1;
        };

        // 二分查找最后一个存在的父目录层级
        while (low < high)
        {
            size_t mid = low + ((high - low + step) >> 1);
#ifdef _WIN32
            mid &= ~size_t(1); // 保持宽字符(2字节)对齐
#endif
            // 向前找最后一个 '/'
            size_t pos = rfind_s(cur, slash, low, mid);
            if (pos == -1)
            {
                // 左半区没有 '/'，向后找右半区第一个 '/'
                pos = find_s(cur, slash, mid, high);
                if (pos == -1) break; // 区间内已无 '/'
            }

            int res = is_dir_at(pos);
            if (res == 1)      low = pos + step;  // 是目录，向右推进
            else if (res == 0) high = pos;        // 不存在，向左收缩
            else               return false;      // 存在但不是目录
        }

        // 从找到的缺失起点 low 开始，向右逐层创建剩余父目录
        for (size_t pos = find_s(cur, slash, low, end);
            pos != -1;
            pos = find_s(cur, slash, pos + step, end))
        {
#ifdef _WIN32
            auto* wp = (wchar_t*)(cur.data() + pos);
            *wp = L'\0';
            BOOL ok = CreateDirectoryW((LPCWSTR)cur.data(), nullptr);
            *wp = L'/';
            if (!ok && GetLastError() != ERROR_ALREADY_EXISTS) return false;
#else
            cur[pos] = '\0';
            int r = ::mkdir(cur.data(), 0777);
            cur[pos] = '/';
            if (r != 0 && errno != EEXIST) return false;
#endif
        }
        return true;
    }

    void Path::create() const
    {
        if (XY_UNLIKELY(!create_help(pth XY_CALL_EX_PARAM)))
            throw E_Path{public_error(pth, "xyu::Path::create", true)};
    }

    void Path::create(ES_Path& state) const noexcept
    {
        if (XY_UNLIKELY(!create_help(pth XY_CALL_EX_PARAM)))
            state = public_error(pth, "xyu::Path::create", false);
        else state = ES_Path::Success;
    }

    void Path::creates() const
    {
        if (XY_UNLIKELY(!creates_help(pth XY_CALL_EX_PARAM)))
            throw E_Path{public_error(pth, "xyu::Path::creates", true)};
    }

    void Path::creates(ES_Path& state) const noexcept
    {
        if (XY_UNLIKELY(!creates_help(pth XY_CALL_EX_PARAM)))
            state = public_error(pth, "xyu::Path::creates", false);
        else state = ES_Path::Success;
    }
}

/// 目录与文件删除
namespace xylu::xysystem
{
#ifdef _WIN32
    static bool del_win_item(LPCWSTR wpath, uint32 attr) noexcept
    {
        if (attr & FILE_ATTRIBUTE_READONLY)
            SetFileAttributesW(wpath, attr & ~FILE_ATTRIBUTE_READONLY);
        return (attr & FILE_ATTRIBUTE_DIRECTORY) ? RemoveDirectoryW(wpath) : DeleteFileW(wpath);
    }

    static bool del_win_file(LPCWSTR wpath) noexcept
    {
        if (DeleteFileW(wpath)) return true;
        if (GetLastError() == ERROR_ACCESS_DENIED)
        {
            uint32 attr = GetFileAttributesW(wpath);
            if (attr != INVALID_FILE_ATTRIBUTES && (attr & FILE_ATTRIBUTE_READONLY))
            {
                SetFileAttributesW(wpath, attr & ~FILE_ATTRIBUTE_READONLY);
                return DeleteFileW(wpath);
            }
        }
        return false;
    }
#endif

    static bool remove_help(StringView pth XY_FUNC_EX_PARAM) noexcept
    {
        if (pth.empty()) { XY_SET_ERROR(XY_NOT_FOUND); return false; }
#ifdef _WIN32
        change_utf8to16(pth, tmp);
        uint32 attr = GetFileAttributesW((LPCWSTR)tmp.data());
        if (XY_UNLIKELY(attr == INVALID_FILE_ATTRIBUTES)) return false;
        return del_win_item((LPCWSTR)tmp.data(), attr);
#else
        struct stat st;
        if (XY_UNLIKELY(lstat(pth.data(), &st) != 0)) return false;
        return S_ISDIR(st.st_mode) ? (::rmdir(pth.data()) == 0) : (::unlink(pth.data()) == 0);
#endif
    }

#ifdef _WIN32
    static bool removes_win_dfs(String& wpath) noexcept
    {
        uint32 attr = GetFileAttributesW((LPCWSTR)wpath.data());
        if (XY_UNLIKELY(attr == INVALID_FILE_ATTRIBUTES)) return false;

        // 若不是目录，或是重解析点（符号链接/挂载点），直接删除本身
        if (!(attr & FILE_ATTRIBUTE_DIRECTORY) || (attr & FILE_ATTRIBUTE_REPARSE_POINT))
            return del_win_item((LPCWSTR)wpath.data(), attr);

        // 确保路径以 '/' 结尾以供遍历拼接
        size_t old_size = wpath.size();
        if (((const wchar_t*)wpath.data())[old_size / 2 - 1] != L'/') change_utf8to16_app("/", wpath);
        size_t base_size = wpath.size();

        {
            WinDirHandle dh(wpath.data());
            if (XY_UNLIKELY(!dh)) return false;

            while (auto data = dh.next())
            {
                if (data.length() <= 4 && data.name()[0] == L'.')
                {
                    if (data.length() == 2) continue;
                    if (data.name()[1] == L'.') continue;
                }

                StringView ws{(const char*)data.name(), data.length()};
                wpath.resize(base_size + ws.size());
                mem_copy(wpath.data() + base_size - 1, ws.data(), ws.size());
                wpath[wpath.size() - 1] = '\0';

                Path::Type type = data.type();
                if (type == Path::Directory)
                {
                    if (XY_UNLIKELY(!removes_win_dfs(wpath))) return false;
                }
                else
                {
                    if (type == Path::Link)
                    {
                        // Windows下指向目录的符号链接需要用 RemoveDirectoryW 删除
                        uint32 sub_attr = GetFileAttributesW((LPCWSTR)wpath.data());
                        if (sub_attr != INVALID_FILE_ATTRIBUTES && (sub_attr & FILE_ATTRIBUTE_DIRECTORY))
                        {
                            if (XY_UNLIKELY(!del_win_item((LPCWSTR)wpath.data(), sub_attr))) return false;
                            continue;
                        }
                    }
                    if (XY_UNLIKELY(!del_win_file((LPCWSTR)wpath.data()))) return false;
                }
            }
            if (XY_UNLIKELY(!dh)) return false; // 遍历中途出错
        } // dh 析构，释放目录句柄，否则无法删除当前目录

        wpath.resize(old_size);
        wpath[old_size - 1] = '\0';
        return del_win_item((LPCWSTR)wpath.data(), attr);
    }
#else
    static bool removes_unix_dfs(String& upath) noexcept
    {
        struct stat st;
        if (XY_UNLIKELY(lstat(upath.data(), &st) != 0)) return false;
        if (!S_ISDIR(st.st_mode)) return unlink(upath.data()) == 0;

        size_t old_size = upath.size();
        if (upath[old_size - 1] != '/') upath << '/';
        size_t base_size = upath.size();

        {
            UnixDirHandle dh(upath.data());
            if (XY_UNLIKELY(!dh)) return false;

            while (auto data = dh.next())
            {
                const char* sname = data.name();
                if (sname[0] == '.')
                {
                    if (sname[1] == '\0') continue;
                    if (sname[1] == '.' && sname[2] == '\0') continue;
                }

                upath.resize(base_size);
                upath << sname;

                Path::Type type = data.type();
                if (type == Path::Unknown)
                {
                    struct stat sub_st;
                    if (XY_UNLIKELY(lstat(upath.data(), &sub_st) != 0)) return false;
                    type = parse_file_type(sub_st.st_mode);
                }

                if (type == Path::Directory)
                {
                    if (XY_UNLIKELY(!removes_unix_dfs(upath))) return false;
                }
                else
                {
                    if (XY_UNLIKELY(unlink(upath.data()) != 0)) return false;
                }
            }
            if (XY_UNLIKELY(errno != 0)) return false;
        }

        upath.resize(old_size);
        return ::rmdir(upath.data()) == 0;
    }
#endif

    static bool removes_help(StringView pth XY_FUNC_EX_PARAM) noexcept
    {
        if (pth.empty()) { XY_SET_ERROR(XY_NOT_FOUND); return false; }
#ifdef _WIN32
        change_utf8to16(pth, tmp);
        return removes_win_dfs(tmp);
#else
        String upath{pth};
        return removes_unix_dfs(upath);
#endif
    }

    void Path::remove() const
    {
        if (XY_UNLIKELY(!remove_help(pth XY_CALL_EX_PARAM)))
            throw E_Path{public_error(pth, "xyu::Path::remove", true)};
    }

    void Path::remove(ES_Path& state) const noexcept
    {
        if (XY_UNLIKELY(!remove_help(pth XY_CALL_EX_PARAM)))
            state = public_error(pth, "xyu::Path::remove", false);
        else state = ES_Path::Success;
    }

    void Path::removes() const
    {
        if (XY_UNLIKELY(!removes_help(pth XY_CALL_EX_PARAM)))
            throw E_Path{public_error(pth, "xyu::Path::removes", true)};
    }

    void Path::removes(ES_Path& state) const noexcept
    {
        if (XY_UNLIKELY(!removes_help(pth XY_CALL_EX_PARAM)))
            state = public_error(pth, "xyu::Path::removes", false);
        else state = ES_Path::Success;
    }
}

/// 目录与文件复制
namespace xylu::xysystem
{
#ifdef _WIN32
    struct WinReparseBuffer
    {
        ULONG ReparseTag;
        USHORT ReparseDataLength;
        USHORT Reserved;
        union
        {
            struct
            {
                USHORT SubstituteNameOffset;
                USHORT SubstituteNameLength;
                USHORT PrintNameOffset;
                USHORT PrintNameLength;
                ULONG Flags;
                WCHAR PathBuffer[1];
            } SymbolicLinkReparseBuffer;
            struct
            {
                USHORT SubstituteNameOffset;
                USHORT SubstituteNameLength;
                USHORT PrintNameOffset;
                USHORT PrintNameLength;
                WCHAR PathBuffer[1];
            } MountPointReparseBuffer;
            UCHAR GenericReparseBuffer[1];
        };
    };

    static bool copy_win_file(LPCWSTR src, LPCWSTR dst, bool overwrite) noexcept
    {
        if (overwrite)
        {
            uint32 dst_attr = GetFileAttributesW(dst);
            if (dst_attr != INVALID_FILE_ATTRIBUTES)
            {
                if ((dst_attr & FILE_ATTRIBUTE_DIRECTORY) && !(dst_attr & FILE_ATTRIBUTE_REPARSE_POINT))
                {
                    SetLastError(ERROR_DIR_NOT_EMPTY);
                    return false;
                }
                // 若目标是符号链接/重解析点，必须先删除链接本身，防止 CopyFileW 穿透写入目标文件
                if (dst_attr & FILE_ATTRIBUTE_REPARSE_POINT)
                {
                    if (XY_UNLIKELY(!del_win_item(dst, dst_attr))) return false;
                }
                else if (dst_attr & FILE_ATTRIBUTE_READONLY)
                {
                    SetFileAttributesW(dst, dst_attr & ~FILE_ATTRIBUTE_READONLY);
                }
            }
        }
        return CopyFileW(src, dst, !overwrite);
    }

    static bool copy_win_link(LPCWSTR src, LPCWSTR dst, uint32 src_attr, bool overwrite) noexcept
    {
        // 使用 FILE_FLAG_OPEN_REPARSE_POINT 打开链接本身，即使目标不存在(悬空链接)也能成功
        HANDLE h = CreateFileW(src, 0, FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
                               nullptr, OPEN_EXISTING,
                               FILE_FLAG_OPEN_REPARSE_POINT | FILE_FLAG_BACKUP_SEMANTICS, nullptr);
        if (XY_UNLIKELY(h == INVALID_HANDLE_VALUE)) return false;

        alignas(8) uint8 rbuf[MAXIMUM_REPARSE_DATA_BUFFER_SIZE];
        DWORD bytes_ret = 0;
        BOOL ok = DeviceIoControl(h, FSCTL_GET_REPARSE_POINT, nullptr, 0,
                                  rbuf, sizeof(rbuf), &bytes_ret, nullptr);
        CloseHandle(h);
        if (XY_UNLIKELY(!ok)) return false;

        auto* rdata = (const WinReparseBuffer*)rbuf;
        if (rdata->ReparseTag != IO_REPARSE_TAG_MOUNT_POINT && rdata->ReparseTag != IO_REPARSE_TAG_SYMLINK)
        {
            // 非符号链接/挂载点的文件类重解析点（如 OneDrive 云文件、AppExecLink、去重/压缩文件等），交由 CopyFileW 处理
            if (!(src_attr & FILE_ATTRIBUTE_DIRECTORY))
                return copy_win_file(src, dst, overwrite);
            SetLastError(ERROR_NOT_SUPPORTED);
            return false;
        }

        uint32 dst_attr = GetFileAttributesW(dst);
        if (dst_attr != INVALID_FILE_ATTRIBUTES)
        {
            if (!overwrite) { SetLastError(ERROR_ALREADY_EXISTS); return false; }
            if ((dst_attr & FILE_ATTRIBUTE_DIRECTORY) && !(dst_attr & FILE_ATTRIBUTE_REPARSE_POINT))
            {
                SetLastError(ERROR_DIR_NOT_EMPTY);
                return false;
            }
            if (XY_UNLIKELY(!del_win_item(dst, dst_attr))) return false;
        }

        if (rdata->ReparseTag == IO_REPARSE_TAG_MOUNT_POINT)
        {
            if (XY_UNLIKELY(!CreateDirectoryW(dst, nullptr))) return false;
            HANDLE h_dst = CreateFileW(dst, GENERIC_WRITE, 0, nullptr, OPEN_EXISTING,
                                       FILE_FLAG_OPEN_REPARSE_POINT | FILE_FLAG_BACKUP_SEMANTICS, nullptr);
            if (XY_UNLIKELY(h_dst == INVALID_HANDLE_VALUE))
            {
                DWORD err = GetLastError();
                RemoveDirectoryW(dst);
                SetLastError(err);
                return false;
            }
            DWORD bytes_written = 0;
            BOOL set_ok = DeviceIoControl(h_dst, FSCTL_SET_REPARSE_POINT, rbuf, bytes_ret,
                                          nullptr, 0, &bytes_written, nullptr);
            DWORD err = GetLastError();
            CloseHandle(h_dst);
            if (XY_UNLIKELY(!set_ok))
            {
                RemoveDirectoryW(dst);
                SetLastError(err);
                return false;
            }
            return true;
        }

        const auto& sym = rdata->SymbolicLinkReparseBuffer;
        const wchar_t* sub_name = sym.PathBuffer + (sym.SubstituteNameOffset / sizeof(WCHAR));
        size_t sub_len = sym.SubstituteNameLength / sizeof(WCHAR);
        bool is_relative = (sym.Flags & 0x1) != 0; // SYMLINK_FLAG_RELATIVE

        String target_buf;
        target_buf.reserve((sub_len + 1) * sizeof(WCHAR));
        mem_copy(target_buf.data(), (char*)sub_name, sub_len * sizeof(WCHAR));
        ((LPWSTR)target_buf.data())[sub_len] = L'\0';
        LPWSTR target = (LPWSTR)target_buf.data();

        // 剥离 NT 命名空间前缀 "\??\"
        if (!is_relative && sub_len >= 4 &&
            target[0] == L'\\' && target[1] == L'?' && target[2] == L'?' && target[3] == L'\\')
        {
            if (sub_len >= 8 && (target[4] == L'U' || target[4] == L'u') &&
                (target[5] == L'N' || target[5] == L'n') &&
                (target[6] == L'C' || target[6] == L'c') && target[7] == L'\\')
            {
                // "\??\UNC\server\share" -> "\\server\share"
                target += 6;
                target[0] = L'\\';
            }
            else target += 4;
        }

        DWORD flags = (src_attr & FILE_ATTRIBUTE_DIRECTORY) ? SYMBOLIC_LINK_FLAG_DIRECTORY : 0;
        // 0x2 = SYMBOLIC_LINK_FLAG_ALLOW_UNPRIVILEGED_CREATE (Win10 1703+ 开发者模式免管理员权限)
        if (CreateSymbolicLinkW(dst, target, flags | 0x2)) return true;
        if (GetLastError() == ERROR_INVALID_PARAMETER)
            return CreateSymbolicLinkW(dst, target, flags);
        return false;
    }

    static bool copy_win_dir(LPCWSTR dst, bool overwrite) noexcept
    {
        if (CreateDirectoryW(dst, nullptr)) return true;
        if (GetLastError() == ERROR_ALREADY_EXISTS && overwrite)
        {
            uint32 dst_attr = GetFileAttributesW(dst);
            if (XY_UNLIKELY(dst_attr == INVALID_FILE_ATTRIBUTES)) return false;
            // 必须是真正的目录（排除指向目录的符号链接/Junction，防止后续递归穿透污染外部目录）
            if ((dst_attr & FILE_ATTRIBUTE_DIRECTORY) && !(dst_attr & FILE_ATTRIBUTE_REPARSE_POINT))
                return true;
            // 若目标是普通文件或符号链接/Junction，先删除本身再创建真正的目录
            if (XY_UNLIKELY(!del_win_item(dst, dst_attr))) return false;
            return CreateDirectoryW(dst, nullptr);
        }
        return false;
    }

    static bool copy_win_single(LPCWSTR src, LPCWSTR dst, uint32 attr, bool overwrite) noexcept
    {
        if (attr & FILE_ATTRIBUTE_REPARSE_POINT)
            return copy_win_link(src, dst, attr, overwrite);
        if (attr & FILE_ATTRIBUTE_DIRECTORY)
            return copy_win_dir(dst, overwrite);
        return copy_win_file(src, dst, overwrite);
    }

#else
    struct CopyBuffer : class_no_copy_move_t
    {
        uint8* buf = nullptr;
        uint32 size = 0;

        static constexpr uint32 start_size = 4 * 1024;
        static constexpr uint32 max_size = 64 * 1024;

        bool prepare() noexcept
        {
            if (buf) return true;
            buf = alloc<uint8>(nullptr, start_size);
            if (XY_UNLIKELY(!buf)) { XY_SET_ERROR(XY_NOMEM); return false; }
            size = start_size;
            return true;
        }

        void expand() noexcept
        {
            if (size >= max_size) return;
            uint32 newsize = size * 2;
            auto* newbuf = alloc<uint8>(nullptr, newsize);
            if (XY_LIKELY(newbuf))
            {
                dealloc(buf, size);
                buf = newbuf;
                size = newsize;
            }
        }

        ~CopyBuffer() noexcept
        {
            if (buf) dealloc(buf, size);
        }
    };

    static bool write_all(int fd, const uint8* p, ssize_t left) noexcept
    {
        while (left > 0)
        {
            ssize_t w = ::write(fd, p, left);
            if (w < 0)
            {
                if (errno == EINTR) continue;
                return false;
            }
            p += w;
            left -= w;
        }
        return true;
    }

    static bool copy_unix_regfile(const char* src, const char* dst, mode_t mode, off_t size, bool overwrite, CopyBuffer& cbuf) noexcept
    {
        int fd_in = ::open(src, O_RDONLY | O_CLOEXEC);
        if (XY_UNLIKELY(fd_in < 0)) return false;

        // 始终先用 O_EXCL 和 0600 尝试创建，防止未完成文件暴露或 dst 为符号链接时直接穿透覆写目标文件
        int fd_out = ::open(dst, O_WRONLY | O_CREAT | O_EXCL | O_CLOEXEC, 0600);
        if (fd_out < 0 && errno == EEXIST && overwrite)
        {
            struct stat dst_st;
            if (XY_UNLIKELY(lstat(dst, &dst_st) != 0))
            {
                int err = errno;
                ::close(fd_in);
                errno = err;
                return false;
            }
            if (S_ISDIR(dst_st.st_mode))
            {
                ::close(fd_in);
                errno = EISDIR;
                return false;
            }
            // 删除原有的文件或符号链接本身，再重新创建
            if (XY_UNLIKELY(::unlink(dst) != 0))
            {
                int err = errno;
                ::close(fd_in);
                errno = err;
                return false;
            }
            fd_out = ::open(dst, O_WRONLY | O_CREAT | O_EXCL | O_CLOEXEC, 0600);
        }
        if (XY_UNLIKELY(fd_out < 0))
        {
            int err = errno;
            ::close(fd_in);
            errno = err;
            return false;
        }

        bool ok = true;
        bool need_fallback = false;
#if defined(__APPLE__)
        bool need_reset = false;
#endif

        // 针对 st_size == 0 的虚拟文件（如 /proc 或 /sys 下的文件），先用小 buf 试探读取
        if (size == 0)
        {
            uint8 probe_buf[128];
            for (;;)
            {
                ssize_t r = ::read(fd_in, probe_buf, sizeof(probe_buf));
                if (r < 0)
                {
                    if (errno == EINTR) continue;
                    ok = false;
                    break;
                }
                if (r == 0) break;
                if (XY_UNLIKELY(!write_all(fd_out, probe_buf, r)))
                {
                    ok = false;
                    break;
                }
                // 读满了探测缓冲区，升级到 CopyBuffer 继续读取
                if ((size_t)r == sizeof(probe_buf))
                {
                    need_fallback = true;
                    break;
                }
            }
        }
        else
        {
#if defined(__APPLE__)
            if (fcopyfile(fd_in, fd_out, nullptr, COPYFILE_DATA) < 0)
            {
                need_fallback = true;
                need_reset = true;
            }
#elif defined(__linux__)
            static constexpr size_t max_chunk = 0x7ffff000; // 单次系统调用最大传输上限
            off_t rem = size;
            off_t copied_total = 0;
            bool try_cfr = true;
            while (rem > 0)
            {
                size_t count = rem > (off_t)max_chunk ? max_chunk : (size_t)rem;
                ssize_t copied = -1;
                if (try_cfr)
                {
                    copied = ::copy_file_range(fd_in, nullptr, fd_out, nullptr, count, 0);
                    if (copied < 0)
                    {
                        if (errno == EINTR) continue;
                        try_cfr = false;
                    }
                    else if (copied == 0 && copied_total == 0)
                    {
                        // 旧内核对不支持的文件系统可能首次直接返回 0，降级尝试 sendfile
                        try_cfr = false;
                    }
                }
                if (!try_cfr)
                {
                    copied = ::sendfile(fd_out, fd_in, nullptr, count);
                    if (copied < 0)
                    {
                        if (errno == EINTR) continue;
                        need_fallback = true;
                        break;
                    }
                    else if (copied == 0 && copied_total == 0)
                    {
                        // sendfile 首次也返回 0（假 EOF），回退到 read/write 确认
                        need_fallback = true;
                        break;
                    }
                }
                if (copied == 0) break;
                rem -= copied;
                copied_total += copied;
            }
#else
            need_fallback = true;
#endif
        }

        if (ok && need_fallback)
        {
#if defined(__APPLE__)
            if (need_reset)
            {
                if (::lseek(fd_in, 0, SEEK_SET) < 0 ||
                    ::lseek(fd_out, 0, SEEK_SET) < 0 ||
                    ::ftruncate(fd_out, 0) < 0)
                {
                    ok = false;
                }
            }
#endif
            if (ok && XY_UNLIKELY(!cbuf.prepare())) ok = false;
            if (ok)
            {
                for (;;)
                {
                    ssize_t r = ::read(fd_in, cbuf.buf, cbuf.size);
                    if (r < 0)
                    {
                        if (errno == EINTR) continue;
                        ok = false;
                        break;
                    }
                    if (r == 0) break;

                    if (XY_UNLIKELY(!write_all(fd_out, cbuf.buf, r)))
                    {
                        ok = false;
                        break;
                    }

                    // 如果刚好读满初始 4KB 缓冲区，下次自动扩容至 64KB
                    if ((uint32)r == cbuf.size && cbuf.size < CopyBuffer::max_size)
                        cbuf.expand();
                }
            }
        }

        int err = errno;
        if (ok) ::fchmod(fd_out, mode & 0777);
        ::close(fd_out);
        ::close(fd_in);
        if (!ok) errno = err;
        return ok;
    }

    static bool copy_unix_link(const char* src, const char* dst, off_t size, bool overwrite) noexcept
    {
        String target;
        size_t cap = (size > 0 && size < 4096) ? (size_t)size : 256;
        for (;;)
        {
            target.reserve(cap);
            ssize_t len = ::readlink(src, target.data(), target.capacity());
            if (XY_UNLIKELY(len < 0)) return false;
            if ((size_t)len < target.capacity())
            {
                target.resize(len);
                break;
            }
            cap *= 2;
        }

        if (::symlink(target.data(), dst) == 0) return true;
        if (errno == EEXIST && overwrite)
        {
            struct stat dst_st;
            if (XY_UNLIKELY(lstat(dst, &dst_st) != 0)) return false;
            if (S_ISDIR(dst_st.st_mode)) { errno = EISDIR; return false; }
            if (XY_UNLIKELY(::unlink(dst) != 0)) return false;
            return ::symlink(target.data(), dst) == 0;
        }
        return false;
    }

    static bool copy_unix_dir(const char* dst, mode_t mode, bool overwrite) noexcept
    {
        if (::mkdir(dst, mode & 0777) == 0) return true;
        if (errno == EEXIST && overwrite)
        {
            struct stat dst_st;
            if (XY_UNLIKELY(lstat(dst, &dst_st) != 0)) return false;
            if (S_ISDIR(dst_st.st_mode)) return true;
            // 若目标是文件或符号链接，先 unlink 再创建真正的目录
            if (XY_UNLIKELY(::unlink(dst) != 0)) return false;
            return ::mkdir(dst, mode & 0777) == 0;
        }
        return false;
    }

    static bool copy_unix_fifo(const char* dst, mode_t mode, bool overwrite) noexcept
    {
        mode_t perm = mode & 0777;
        if (::mkfifo(dst, perm) == 0)
        {
            ::chmod(dst, perm);
            return true;
        }
        if (errno == EEXIST && overwrite)
        {
            struct stat dst_st;
            if (XY_UNLIKELY(lstat(dst, &dst_st) != 0)) return false;
            if (S_ISDIR(dst_st.st_mode)) { errno = EISDIR; return false; }
            if (XY_UNLIKELY(::unlink(dst) != 0)) return false;
            if (::mkfifo(dst, perm) == 0)
            {
                ::chmod(dst, perm);
                return true;
            }
        }
        return false;
    }

    static bool copy_unix_single(const char* src, const char* dst, const struct stat& st, bool overwrite) noexcept
    {
        if (S_ISLNK(st.st_mode))
            return copy_unix_link(src, dst, st.st_size, overwrite);
        if (S_ISDIR(st.st_mode))
            return copy_unix_dir(dst, st.st_mode, overwrite);
        if (S_ISFIFO(st.st_mode))
            return copy_unix_fifo(dst, st.st_mode, overwrite);
        if (S_ISREG(st.st_mode))
        {
            CopyBuffer cbuf;
            return copy_unix_regfile(src, dst, st.st_mode, st.st_size, overwrite, cbuf);
        }
        errno = EIO;
        return false;
    }
#endif

    // 检测 dst 是否为 src 的子路径或相同路径 (返回: 0-正常, 1-相同路径, -1-错误或子路径循环)
    static int check_subpath(StringView src, StringView dst, bool is_dir XY_FUNC_EX_PARAM2) noexcept
    {
        String abs_src, abs_dst;
#ifdef _WIN32
        if (XY_UNLIKELY(!to_absolute_help(src, abs_src, tmp1))) return -1;
        if (XY_UNLIKELY(!to_absolute_help(dst, abs_dst, tmp2))) return -1;
        abs_src.to_lower(abs_src.data());
        abs_dst.to_lower(abs_dst.data());
#else
        if (XY_UNLIKELY(!to_absolute_help(src, abs_src, [](String& d){ return get_dir_help(d); }))) return -1;
        if (XY_UNLIKELY(!to_absolute_help(dst, abs_dst, [](String& d){ return get_dir_help(d); }))) return -1;
#endif
        size_t i = abs_src.contrast(abs_dst);
        if (i == abs_src.size())
        {
            if (abs_dst.size() == abs_src.size()) return 1;
            if (is_dir && (abs_dst[i] == '/' || abs_src[abs_src.size() - 1] == '/'))
            {
                XY_SET_ERROR(XY_LOOP);
                return -1;
            }
        }
        return 0;
    }

    static bool copy_help(StringView src, StringView dst, bool overwrite XY_FUNC_EX_PARAM2) noexcept
    {
        if (src.empty() || dst.empty()) { XY_SET_ERROR(XY_NOT_FOUND); return false; }
#ifdef _WIN32
        change_utf8to16(src, tmp1);
        uint32 attr = GetFileAttributesW((LPCWSTR)tmp1.data());
        if (XY_UNLIKELY(attr == INVALID_FILE_ATTRIBUTES)) return false;

        int sub = check_subpath(src, dst, false, tmp1, tmp2);
        if (XY_UNLIKELY(sub != 0))
        {
            if (sub < 0) return false;
            if (overwrite) return true;
            XY_SET_ERROR(XY_EXIST);
            return false;
        }

        change_utf8to16(src, tmp1);
        change_utf8to16(dst, tmp2);
        return copy_win_single((LPCWSTR)tmp1.data(), (LPCWSTR)tmp2.data(), attr, overwrite);
#else
        struct stat st;
        if (XY_UNLIKELY(lstat(src.data(), &st) != 0)) return false;

        int sub = check_subpath(src, dst, false);
        if (XY_UNLIKELY(sub != 0))
        {
            if (sub < 0) return false;
            if (overwrite) return true;
            XY_SET_ERROR(XY_EXIST);
            return false;
        }

        return copy_unix_single(src.data(), dst.data(), st, overwrite);
#endif
    }

#ifdef _WIN32
    static bool copys_win_dfs(String& wsrc, String& wdst, bool overwrite) noexcept
    {
        if (XY_UNLIKELY(!copy_win_dir((LPCWSTR)wdst.data(), overwrite))) return false;

        size_t old_src = wsrc.size();
        if (((const wchar_t*)wsrc.data())[old_src / 2 - 1] != L'/') change_utf8to16_app("/", wsrc);
        size_t base_src = wsrc.size();

        size_t old_dst = wdst.size();
        if (((const wchar_t*)wdst.data())[old_dst / 2 - 1] != L'/') change_utf8to16_app("/", wdst);
        size_t base_dst = wdst.size();

        {
            WinDirHandle dh(wsrc.data());
            if (XY_UNLIKELY(!dh)) return false;

            while (auto data = dh.next())
            {
                if (data.length() <= 4 && data.name()[0] == L'.')
                {
                    if (data.length() == 2) continue;
                    if (data.name()[1] == L'.') continue;
                }

                StringView ws{(const char*)data.name(), data.length()};

                wsrc.resize(base_src + ws.size());
                mem_copy(wsrc.data() + base_src - 1, ws.data(), ws.size());
                wsrc[wsrc.size() - 1] = '\0';

                wdst.resize(base_dst + ws.size());
                mem_copy(wdst.data() + base_dst - 1, ws.data(), ws.size());
                wdst[wdst.size() - 1] = '\0';

                Path::Type type = data.type();
                if (type == Path::Link)
                {
                    uint32 sub_attr = GetFileAttributesW((LPCWSTR)wsrc.data());
                    if (XY_UNLIKELY(sub_attr == INVALID_FILE_ATTRIBUTES)) return false;
                    if (XY_UNLIKELY(!copy_win_link((LPCWSTR)wsrc.data(), (LPCWSTR)wdst.data(), sub_attr, overwrite))) return false;
                }
                else if (type == Path::Directory)
                {
                    if (XY_UNLIKELY(!copys_win_dfs(wsrc, wdst, overwrite))) return false;
                }
                else
                {
                    if (XY_UNLIKELY(!copy_win_file((LPCWSTR)wsrc.data(), (LPCWSTR)wdst.data(), overwrite))) return false;
                }
            }
            if (XY_UNLIKELY(!dh)) return false;
        }

        wsrc.resize(old_src);
        wsrc[old_src - 1] = '\0';
        wdst.resize(old_dst);
        wdst[old_dst - 1] = '\0';
        return true;
    }
#else
     static bool copys_unix_dfs(String& usrc, String& udst, const struct stat& st, bool overwrite, CopyBuffer& cbuf) noexcept
    {
        mode_t orig_perm = st.st_mode & 0777;
        mode_t work_perm = orig_perm | 0700;
        if (XY_UNLIKELY(!copy_unix_dir(udst.data(), work_perm, overwrite))) return false;
        // 若复用了已存在的只读目录，临时提权确保能向其中写入子项
        if (overwrite) ::chmod(udst.data(), work_perm);

        size_t old_src = usrc.size();
        if (usrc[old_src - 1] != '/') usrc << '/';
        size_t base_src = usrc.size();

        size_t old_dst = udst.size();
        if (udst[old_dst - 1] != '/') udst << '/';
        size_t base_dst = udst.size();

        {
            UnixDirHandle dh(usrc.data());
            if (XY_UNLIKELY(!dh)) return false;

            while (auto data = dh.next())
            {
                const char* sname = data.name();
                if (sname[0] == '.')
                {
                    if (sname[1] == '\0') continue;
                    if (sname[1] == '.' && sname[2] == '\0') continue;
                }

                usrc.resize(base_src);
                usrc << sname;
                udst.resize(base_dst);
                udst << sname;

                struct stat sub_st;
                if (XY_UNLIKELY(lstat(usrc.data(), &sub_st) != 0)) return false;

                if (S_ISLNK(sub_st.st_mode))
                {
                    if (XY_UNLIKELY(!copy_unix_link(usrc.data(), udst.data(), sub_st.st_size, overwrite))) return false;
                }
                else if (S_ISDIR(sub_st.st_mode))
                {
                    if (XY_UNLIKELY(!copys_unix_dfs(usrc, udst, sub_st, overwrite, cbuf))) return false;
                }
                else if (S_ISFIFO(sub_st.st_mode))
                {
                    if (XY_UNLIKELY(!copy_unix_fifo(udst.data(), sub_st.st_mode, overwrite))) return false;
                }
                else if (S_ISREG(sub_st.st_mode))
                {
                    if (XY_UNLIKELY(!copy_unix_regfile(usrc.data(), udst.data(), sub_st.st_mode, sub_st.st_size, overwrite, cbuf))) return false;
                }
                else
                {
                    errno = EIO;
                    return false;
                }
            }
            if (XY_UNLIKELY(errno != 0)) return false;
        }

        usrc.resize(old_src);
        udst.resize(old_dst);
        if (work_perm != orig_perm)
        {
            if (XY_UNLIKELY(::chmod(udst.data(), orig_perm) != 0)) return false;
        }
        return true;
    }
#endif

    static bool copys_help(StringView src, StringView dst, bool overwrite XY_FUNC_EX_PARAM2) noexcept
    {
        if (src.empty() || dst.empty()) { XY_SET_ERROR(XY_NOT_FOUND); return false; }
#ifdef _WIN32
        change_utf8to16(src, tmp1);
        uint32 attr = GetFileAttributesW((LPCWSTR)tmp1.data());
        if (XY_UNLIKELY(attr == INVALID_FILE_ATTRIBUTES)) return false;

        bool is_dir = (attr & FILE_ATTRIBUTE_DIRECTORY) && !(attr & FILE_ATTRIBUTE_REPARSE_POINT);
        int sub = check_subpath(src, dst, is_dir, tmp1, tmp2);
        if (XY_UNLIKELY(sub != 0))
        {
            if (sub < 0) return false;
            if (overwrite) return true;
            XY_SET_ERROR(XY_EXIST);
            return false;
        }

        change_utf8to16(src, tmp1);
        change_utf8to16(dst, tmp2);

        if (!is_dir)
            return copy_win_single((LPCWSTR)tmp1.data(), (LPCWSTR)tmp2.data(), attr, overwrite);
        return copys_win_dfs(tmp1, tmp2, overwrite);
#else
        struct stat st;
        if (XY_UNLIKELY(lstat(src.data(), &st) != 0)) return false;

        bool is_dir = S_ISDIR(st.st_mode);
        int sub = check_subpath(src, dst, is_dir);
        if (XY_UNLIKELY(sub != 0))
        {
            if (sub < 0) return false;
            if (overwrite) return true;
            XY_SET_ERROR(XY_EXIST);
            return false;
        }

        if (!is_dir)
            return copy_unix_single(src.data(), dst.data(), st, overwrite);

        String usrc{src}, udst{dst};
        CopyBuffer cbuf;
        return copys_unix_dfs(usrc, udst, st, overwrite, cbuf);
#endif
    }

    void Path::copy(const Path& dst, bool overwrite) const
    {
        if (XY_UNLIKELY(!copy_help(pth, dst.pth, overwrite XY_CALL_EX_PARAM2)))
            throw E_Path{public_error(pth, "xyu::Path::copy", true)};
    }

    void Path::copy(const Path& dst, bool overwrite, ES_Path& state) const noexcept
    {
        if (XY_UNLIKELY(!copy_help(pth, dst.pth, overwrite XY_CALL_EX_PARAM2)))
            state = public_error(pth, "xyu::Path::copy", false);
        else state = ES_Path::Success;
    }

    void Path::copys(const Path& dst, bool overwrite) const
    {
        if (XY_UNLIKELY(!copys_help(pth, dst.pth, overwrite XY_CALL_EX_PARAM2)))
            throw E_Path{public_error(pth, "xyu::Path::copys", true)};
    }

    void Path::copys(const Path& dst, bool overwrite, ES_Path& state) const noexcept
    {
        if (XY_UNLIKELY(!copys_help(pth, dst.pth, overwrite XY_CALL_EX_PARAM2)))
            state = public_error(pth, "xyu::Path::copys", false);
        else state = ES_Path::Success;
    }
}

/// 目录与文件移动
namespace xylu::xysystem
{
#ifdef _WIN32
    static bool is_win_dir_empty(String& wpath) noexcept
    {
        size_t old_size = wpath.size();
        if (((const wchar_t*)wpath.data())[old_size / 2 - 1] != L'/') change_utf8to16_app("/", wpath);
        bool empty = true;
        {
            WinDirHandle dh(wpath.data());
            if (XY_UNLIKELY(!dh))
            {
                wpath.resize(old_size);
                wpath[old_size - 1] = '\0';
                return false;
            }
            while (auto data = dh.next())
            {
                if (data.length() <= 4 && data.name()[0] == L'.')
                {
                    if (data.length() == 2) continue;
                    if (data.name()[1] == L'.') continue;
                }
                empty = false;
                break;
            }
            if (XY_UNLIKELY(!dh))
            {
                wpath.resize(old_size);
                wpath[old_size - 1] = '\0';
                return false;
            }
        }
        wpath.resize(old_size);
        wpath[old_size - 1] = '\0';
        if (!empty) SetLastError(ERROR_DIR_NOT_EMPTY);
        return empty;
    }
#else
    static bool is_unix_dir_empty(const char* path) noexcept
    {
        UnixDirHandle dh(path);
        if (XY_UNLIKELY(!dh)) return false;
        while (auto data = dh.next())
        {
            const char* sname = data.name();
            if (sname[0] == '.')
            {
                if (sname[1] == '\0') continue;
                if (sname[1] == '.' && sname[2] == '\0') continue;
            }
            errno = ENOTEMPTY;
            return false;
        }
        return errno == 0;
    }
#endif

    static bool move_help(StringView src, StringView dst, bool overwrite XY_FUNC_EX_PARAM2) noexcept
    {
        if (src.empty() || dst.empty()) { XY_SET_ERROR(XY_NOT_FOUND); return false; }
#ifdef _WIN32
        change_utf8to16(src, tmp1);
        uint32 src_attr = GetFileAttributesW((LPCWSTR)tmp1.data());
        if (XY_UNLIKELY(src_attr == INVALID_FILE_ATTRIBUTES)) return false;

        bool src_is_dir = (src_attr & FILE_ATTRIBUTE_DIRECTORY) && !(src_attr & FILE_ATTRIBUTE_REPARSE_POINT);
        int sub = check_subpath(src, dst, src_is_dir, tmp1, tmp2);
        if (XY_UNLIKELY(sub != 0))
        {
            if (sub < 0) return false;
            if (overwrite) return true;
            XY_SET_ERROR(XY_EXIST);
            return false;
        }

        change_utf8to16(src, tmp1);
        change_utf8to16(dst, tmp2);

        // move 仅支持移动空目录
        if (src_is_dir && XY_UNLIKELY(!is_win_dir_empty(tmp1))) return false;

        uint32 dst_attr = GetFileAttributesW((LPCWSTR)tmp2.data());
        if (dst_attr != INVALID_FILE_ATTRIBUTES)
        {
            if (!overwrite) { SetLastError(ERROR_ALREADY_EXISTS); return false; }
            bool dst_is_dir = (dst_attr & FILE_ATTRIBUTE_DIRECTORY) && !(dst_attr & FILE_ATTRIBUTE_REPARSE_POINT);
            if (!src_is_dir && dst_is_dir) { SetLastError(ERROR_DIR_NOT_EMPTY); return false; }
            // RemoveDirectoryW 会自动校验目标目录是否为空，非空则报错 ERROR_DIR_NOT_EMPTY
            if (XY_UNLIKELY(!del_win_item((LPCWSTR)tmp2.data(), dst_attr))) return false;
        }

        if (MoveFileExW((LPCWSTR)tmp1.data(), (LPCWSTR)tmp2.data(), 0)) return true;
        if (GetLastError() != ERROR_NOT_SAME_DEVICE) return false;

        // 跨分区回退：直接调用底层单节点复制 + 删除
        if (XY_UNLIKELY(!copy_win_single((LPCWSTR)tmp1.data(), (LPCWSTR)tmp2.data(), src_attr, overwrite))) return false;
        return del_win_item((LPCWSTR)tmp1.data(), src_attr);
#else
        struct stat src_st;
        if (XY_UNLIKELY(lstat(src.data(), &src_st) != 0)) return false;

        bool src_is_dir = S_ISDIR(src_st.st_mode);
        int sub = check_subpath(src, dst, src_is_dir);
        if (XY_UNLIKELY(sub != 0))
        {
            if (sub < 0) return false;
            if (overwrite) return true;
            XY_SET_ERROR(XY_EXIST);
            return false;
        }

        // move 仅支持移动空目录
        if (src_is_dir && XY_UNLIKELY(!is_unix_dir_empty(src.data()))) return false;

        struct stat dst_st;
        if (lstat(dst.data(), &dst_st) == 0)
        {
            if (!overwrite) { errno = EEXIST; return false; }
            bool dst_is_dir = S_ISDIR(dst_st.st_mode);
            if (!src_is_dir && dst_is_dir) { errno = EISDIR; return false; }
            if (src_is_dir && !dst_is_dir)
            {
                if (XY_UNLIKELY(::unlink(dst.data()) != 0)) return false;
            }
        }
        else if (errno != ENOENT) return false;

        if (::rename(src.data(), dst.data()) == 0) return true;
        if (errno != EXDEV) return false;

        // 跨文件系统回退：直接调用底层单节点复制 + 删除
        if (XY_UNLIKELY(!copy_unix_single(src.data(), dst.data(), src_st, overwrite))) return false;
        return src_is_dir ? (::rmdir(src.data()) == 0) : (::unlink(src.data()) == 0);
#endif
    }

    static bool moves_help(StringView src, StringView dst, bool overwrite XY_FUNC_EX_PARAM2) noexcept
    {
        if (src.empty() || dst.empty()) { XY_SET_ERROR(XY_NOT_FOUND); return false; }
#ifdef _WIN32
        change_utf8to16(src, tmp1);
        uint32 src_attr = GetFileAttributesW((LPCWSTR)tmp1.data());
        if (XY_UNLIKELY(src_attr == INVALID_FILE_ATTRIBUTES)) return false;

        bool src_is_dir = (src_attr & FILE_ATTRIBUTE_DIRECTORY) && !(src_attr & FILE_ATTRIBUTE_REPARSE_POINT);
        int sub = check_subpath(src, dst, src_is_dir, tmp1, tmp2);
        if (XY_UNLIKELY(sub != 0))
        {
            if (sub < 0) return false;
            if (overwrite) return true;
            XY_SET_ERROR(XY_EXIST);
            return false;
        }

        change_utf8to16(src, tmp1);
        change_utf8to16(dst, tmp2);

        uint32 dst_attr = GetFileAttributesW((LPCWSTR)tmp2.data());
        bool need_merge = false;
        if (dst_attr != INVALID_FILE_ATTRIBUTES)
        {
            if (!overwrite) { SetLastError(ERROR_ALREADY_EXISTS); return false; }
            bool dst_is_dir = (dst_attr & FILE_ATTRIBUTE_DIRECTORY) && !(dst_attr & FILE_ATTRIBUTE_REPARSE_POINT);
            if (!src_is_dir && dst_is_dir) { SetLastError(ERROR_DIR_NOT_EMPTY); return false; }

            if (src_is_dir && dst_is_dir)
            {
                // 若目标也是目录，尝试删除空目标目录以直接 rename，若非空则走递归合并
                if (!del_win_item((LPCWSTR)tmp2.data(), dst_attr))
                {
                    if (GetLastError() == ERROR_DIR_NOT_EMPTY) need_merge = true;
                    else return false;
                }
            }
            else
            {
                if (XY_UNLIKELY(!del_win_item((LPCWSTR)tmp2.data(), dst_attr))) return false;
            }
        }

        if (!need_merge)
        {
            if (MoveFileExW((LPCWSTR)tmp1.data(), (LPCWSTR)tmp2.data(), 0)) return true;
            if (GetLastError() != ERROR_NOT_SAME_DEVICE) return false;
        }

        // 跨分区或目录合并回退：直接调用底层复制与删除
        if (!src_is_dir)
        {
            if (XY_UNLIKELY(!copy_win_single((LPCWSTR)tmp1.data(), (LPCWSTR)tmp2.data(), src_attr, overwrite))) return false;
            return del_win_item((LPCWSTR)tmp1.data(), src_attr);
        }
        if (XY_UNLIKELY(!copys_win_dfs(tmp1, tmp2, overwrite))) return false;
        return removes_win_dfs(tmp1);
#else
        struct stat src_st;
        if (XY_UNLIKELY(lstat(src.data(), &src_st) != 0)) return false;

        bool src_is_dir = S_ISDIR(src_st.st_mode);
        int sub = check_subpath(src, dst, src_is_dir);
        if (XY_UNLIKELY(sub != 0))
        {
            if (sub < 0) return false;
            if (overwrite) return true;
            XY_SET_ERROR(XY_EXIST);
            return false;
        }

        struct stat dst_st;
        if (lstat(dst.data(), &dst_st) == 0)
        {
            if (!overwrite) { errno = EEXIST; return false; }
            bool dst_is_dir = S_ISDIR(dst_st.st_mode);
            if (!src_is_dir && dst_is_dir) { errno = EISDIR; return false; }
            if (src_is_dir && !dst_is_dir)
            {
                if (XY_UNLIKELY(::unlink(dst.data()) != 0)) return false;
            }
        }
        else if (errno != ENOENT) return false;

        if (::rename(src.data(), dst.data()) == 0) return true;
        if (errno != ENOTEMPTY && errno != EEXIST && errno != EXDEV) return false;

        // 跨文件系统或目录合并回退：直接调用底层复制与删除
        if (!src_is_dir)
        {
            if (XY_UNLIKELY(!copy_unix_single(src.data(), dst.data(), src_st, overwrite))) return false;
            return ::unlink(src.data()) == 0;
        }
        String usrc{src}, udst{dst};
        CopyBuffer cbuf;
        if (XY_UNLIKELY(!copys_unix_dfs(usrc, udst, src_st, overwrite, cbuf))) return false;
        return removes_unix_dfs(usrc);
#endif
    }

    void Path::move(const Path& dst, bool overwrite) const
    {
        if (XY_UNLIKELY(!move_help(pth, dst.pth, overwrite XY_CALL_EX_PARAM2)))
            throw E_Path{public_error(pth, "xyu::Path::move", true)};
    }

    void Path::move(const Path& dst, bool overwrite, ES_Path& state) const noexcept
    {
        if (XY_UNLIKELY(!move_help(pth, dst.pth, overwrite XY_CALL_EX_PARAM2)))
            state = public_error(pth, "xyu::Path::move", false);
        else state = ES_Path::Success;
    }

    void Path::moves(const Path& dst, bool overwrite) const
    {
        if (XY_UNLIKELY(!moves_help(pth, dst.pth, overwrite XY_CALL_EX_PARAM2)))
            throw E_Path{public_error(pth, "xyu::Path::moves", true)};
    }

    void Path::moves(const Path& dst, bool overwrite, ES_Path& state) const noexcept
    {
        if (XY_UNLIKELY(!moves_help(pth, dst.pth, overwrite XY_CALL_EX_PARAM2)))
            state = public_error(pth, "xyu::Path::moves", false);
        else state = ES_Path::Success;
    }
}
