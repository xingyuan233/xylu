#pragma once

/**
 * @file    xylu/path.h
 * @brief   路径
 * @date    2026-10-13
*/

#include "./_.h"
#include "../xystring/strsso.h"
#include "../xycoll/nearlist.h"

/// 路径异常类
namespace xylu::xycore
{
    /// 路径异常类
    struct E_Path : E_Resource { using E_Resource::E_Resource; };

    /// 异常码
    struct ES_Path : ErrorState
    {
        using ErrorState::ErrorState;

        enum _Path
        {
            // 操作成功
            Success = ES_Resource::Success,
            // 路径未找到
            Not_Found = ES_Resource::Not_Found,
            // 路径已被占用
            Path_Occupied = 17,
            // 路径不是目录
            Not_Dir = 20,
            // 非法路径
            Invalid_Path = 22,
            // 无权限
            No_Permission = ES_Resource::No_Permission,
            // 内存不足
            No_Memory = ES_Resource::No_Memory,
            // IO失败
            IO_Failed = ES_Resource::IO_Failed,
            // 路径过长
            Length_Limit = 36,
            // 循环引用或符号链接过多
            Loop_Refer = 40,
            // 目录非空
            Dir_Not_Empty = 41,
        };
    };
}

/// 路径
namespace xylu::xysystem
{
    /**
    * @brief 封装了文件系统路径解析、查询与目录操作的基础类。
    * @details
    *   `Path` 类内部维护一个规范化的 UTF-8 路径字符串，并提供统一的跨平台文件系统接口。
    *
    *   存储方式：
    *    - 分隔符规范：内部统一将 `\` 转换为 `/` 存储；除盘符或根目录外，自动去除末尾的 `/`
    *    - 编码转换：对外统一使用 UTF-8 编码，Windows 下底层调用前会自动转换为 UTF-16
    *
    *   方法简述：
    *    - 支持路径拼接、相对/绝对路径转换及工作目录的获取与切换
    *    - 支持文件类型检测，以及单层或递归（DFS/BFS）的目录遍历
    *    - 支持单层或递归的目录创建、文件与目录的删除、复制及移动
    *
    *    注意事项：
    *    - 符号链接：类型查询与遍历不穿透符号链接（或重解析点）；递归删除、复制、移动时仅处理链接本身，不会穿透破坏目标目录
    *    - 非原子性：递归过程中单个文件复制或移动时状态一致，但不保证全局状态一致性
    *    - 无限循环：复制或移动中复杂符号链接如果循环引用可能导致无限循环
    */
    struct Path
    {
    private:
        xyu::String pth;

    public:
        Path(const char* path) noexcept { set(path); }
        Path(xyu::StringView path = "") noexcept { set(path); }
        Path& operator=(xyu::StringView path) noexcept { set(path); return *this; }

        /// 设置路径
        Path& set(xyu::StringView path) noexcept;
        /// 获取路径
        const xyu::String& get() const noexcept { return pth; }
        /// 拼接路径，自动处理分隔符
        Path& concat(xyu::StringView path) noexcept;

        /// 获取当前工作路径
        Path& get_dir();
        /// 获取当前工作路径
        Path& get_dir(xyu::ES_Path& state) noexcept;
        /// 设置当前工作路径
        void set_dir() const;
        /// 设置当前工作路径
        void set_dir(xyu::ES_Path& state) const noexcept;

        /// 是否为绝对路径
        bool is_absolute() const noexcept;
        /// 是否为相对路径
        bool is_relative() const noexcept { return !is_absolute(); }
        /// 转换为绝对路径
        Path to_absolute() const;
        /// 转换为绝对路径
        Path to_absolute(xyu::ES_Path& state) const noexcept;
        /// 转换为相对路径
        Path to_relative() const;
        /// 转换为相对路径
        Path to_relative(xyu::ES_Path& state) const noexcept;

        enum Type : xyu::uint
        {
            None        =   0,
            RegFile     =   1 << 0,     // 常规文件
            Directory   =   1 << 1,     // 目录
            Link        =   1 << 2,     // 符号链接 | 重解析点
            BlockDev    =   1 << 3,     // 块设备
            CharDev     =   1 << 4,     // 字符设备
            Fifo        =   1 << 5,     // 管道
            Socket      =   1 << 6,     // 套接字
            Unknown     =   1 << 7,     // 未知
        };
        /// 获取文件类型
        Type get_type() const;
        /// 获取文件类型
        Type get_type(xyu::ES_Path& state) const noexcept;
        /// 是否为常规文件
        bool is_file() const { return get_type() == RegFile; }
        /// 是否为常规文件
        bool is_file(xyu::ES_Path& state) const noexcept { return get_type(state) == RegFile; }
        /// 是否为目录
        bool is_dir() const { return get_type() == Directory; }
        /// 是否为目录
        bool is_dir(xyu::ES_Path& state) const noexcept { return get_type(state) == Directory; }

        struct Walker;
        enum WalkType
        {
            WT_Single,  // 单层
            WT_DFS,     // 多层，深度优先
            WT_BFS,     // 多层，广度优先
        };
        /// 遍历目录，返回所需类型文件或目录
        Walker walk(WalkType wt = WT_Single, xyu::uint select_type = -1) const;
        /// 遍历目录，返回所需类型文件或目录
        Walker walk(WalkType wt, xyu::uint select_type, xyu::ES_Path& state) const noexcept;
        /// 遍历目录，返回所需类型文件或目录
        Walker walk(WalkType wt, xyu::ES_Path& state) const noexcept;
        /// 遍历目录，返回所需类型文件或目录
        Walker walk(xyu::ES_Path& state) const noexcept;

        /// 创建单层目录
        void create() const;
        /// 创建单层目录
        void create(xyu::ES_Path& state) const noexcept;
        /// 递归创建目录
        void creates() const;
        /// 递归创建目录
        void creates(xyu::ES_Path& state) const noexcept;

        /// 删除文件或空目录
        void remove() const;
        /// 删除文件或空目录
        void remove(xyu::ES_Path& state) const noexcept;
        /// 递归删除文件或目录及其所有内容
        void removes() const;
        /// 递归删除文件或目录及其所有内容
        void removes(xyu::ES_Path& state) const noexcept;

        /// 复制单个文件或空目录
        void copy(const Path& dst, bool overwrite = true) const;
        /// 复制单个文件或空目录
        void copy(const Path& dst, bool overwrite, xyu::ES_Path& state) const noexcept;
        /// 复制单个文件或空目录
        void copy(const Path& dst, xyu::ES_Path& state) const noexcept { copy(dst, true, state); }
        /// 递归复制文件或目录及其所有内容
        void copys(const Path& dst, bool overwrite = true) const;
        /// 递归复制文件或目录及其所有内容
        void copys(const Path& dst, bool overwrite, xyu::ES_Path& state) const noexcept;
        /// 递归复制文件或目录及其所有内容
        void copys(const Path& dst, xyu::ES_Path& state) const noexcept { copys(dst, true, state); }

        /// 移动单个文件或空目录
        void move(const Path& dst, bool overwrite = true) const;
        /// 移动单个文件或空目录
        void move(const Path& dst, bool overwrite, xyu::ES_Path& state) const noexcept;
        /// 移动单个文件或空目录
        void move(const Path& dst, xyu::ES_Path& state) const noexcept { move(dst, true, state); }
        /// 递归移动文件或目录及其所有内容
        void moves(const Path& dst, bool overwrite = true) const;
        /// 递归移动文件或目录及其所有内容
        void moves(const Path& dst, bool overwrite, xyu::ES_Path& state) const noexcept;
        /// 递归移动文件或目录及其所有内容
        void moves(const Path& dst, xyu::ES_Path& state) const noexcept { moves(dst, true, state); }

#ifdef _WIN32
    private:
        mutable xyu::String tmp;
    public:
        Path(const Path& other) noexcept : pth(other.pth) {}
        Path(Path&& other) noexcept = default;
        Path& operator=(const Path& other) noexcept { pth = other.pth; return *this; }
        Path& operator=(Path&& other) noexcept = default;
#endif
        operator xyu::StringView() const noexcept { return pth; }

        friend Type operator|(Type t1, Type t2) noexcept { return Type((xyu::uint)t1 | (xyu::uint)t2); }
    };

    /// 目录遍历器
    struct Path::Walker : xyu::class_no_copy_t
    {
    private:
        void* handle;
        union { xyu::String fold; };
        Type type;
        xyu::uint32 flag;
        xyu::String name;

    public:
        /// 检查并前进到下一个项
        bool next();
        /// 检查并前进到下一个项
        bool next(xyu::ES_Path& state) noexcept;

        struct Info { Type type; const xyu::String& name; };
        /// 获取项
        Info get() noexcept { return {type, name}; }

        /// 是否有效
        explicit operator bool() const noexcept;
        /// 获取项
        Info operator*() noexcept { return get(); }
        /// 前进到下一个项
        Walker& operator++() noexcept { next(); return *this; }
        /// 获取范围 (必须确保 Walker 对象存活)
        auto range() noexcept;

    private:
        friend struct Path;
        Walker() noexcept;
    public:
        Walker(Walker&& other) noexcept;
        Walker& operator=(Walker&& other) noexcept;
        ~Walker() noexcept;
    };

    inline auto Path::Walker::range() noexcept
    {
        using Gen = xyu::Generator<
            xyu::IterPolicies::Default::Storage<Walker>,
            xyu::GenPolicies::Refer::Valid,
            xyu::GenPolicies::Refer::Dereference,
            xyu::GenPolicies::Refer::Increment>;
        return xyu::Range{xyu::size_t(-2), ++Gen{this}};
    }
}
