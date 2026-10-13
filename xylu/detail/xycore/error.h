#pragma once

/**
 * @file    xycore/error.h
 * @brief   基础异常
 * @date    2025-12-25
 *
 * @details
 *   异常本身只携带类型信息，用于 try-catch 或处理判断。
 *   详细的错误上下文信息由日志系统在异常抛出前记录。
 *   逻辑错误、违背调用约定 或 无法恢复的异常 等直接 abort 进行快速失败。
 */

#include "./type.h"
#include "./attr.h"

/// 异常基类
namespace xylu::xycore
{
    /**
     * @brief 异常基类
     * @details 所有异常类都应该继承自该类
     */
    struct Error
    {
        virtual ~Error() = default;
    };

    /// 异常码
    struct ErrorState
    {
        int code;

        constexpr ErrorState(int e = 0) noexcept : code(e) {}

        constexpr explicit operator bool() const noexcept { return code == 0; }
        constexpr bool success() const noexcept { return code == 0; }
        constexpr bool fail() const noexcept { return code != 0; }

        constexpr friend bool operator==(ErrorState s1, ErrorState s2) noexcept { return s1.code == s2.code; }
        constexpr friend bool operator!=(ErrorState s1, ErrorState s2) noexcept { return s1.code != s2.code; }
    };

    /// 异常位置信息
    struct ErrorPos
    {
        const char* file;   // 文件名
        int line;           // 行
        const char* func;   // 函数

        // 常规构造
        constexpr ErrorPos(const char* file, int line, const char* func) noexcept : file(file), line(line), func(func) {}

        // 宏构造 - 覆盖函数名
        constexpr ErrorPos(const char* file, int line, const char*, const char* func) noexcept : ErrorPos{file, line, func} {}
        // 宏构造 - 仅函数名
        constexpr ErrorPos(const char*, int, const char*, nullptr_t, const char* func) noexcept : ErrorPos{nullptr, 0, func} {}
        // 宏构造 - 覆盖 行 + 函数名
        constexpr ErrorPos(const char* file, int, const char*, const char* func, int line) noexcept : ErrorPos{file, line, func} {}
        // 宏构造 - 覆盖 函数名 + 行
        constexpr ErrorPos(const char* file, int, const char*, int line, const char* func) noexcept : ErrorPos{file, line, func} {}
        // 宏构造 - 全覆盖
        constexpr ErrorPos(const char*, int, const char*, const char* file, int line, const char* func) noexcept : ErrorPos{file, line, func} {}
    };

    /**
     * @brief 生成异常位置信息
     * @details 默认为 __FILE__:__LINE__ __func__；
     *   可显式覆盖为:
     *     - func           --> __FILE__:__LINE__ func
     *     - nullptr,func   --> func
     *     - line,func      --> __FILE__:line func
     *     - file,line,func --> file:line func
     */
    #define XY_CurPos(...) xyu::ErrorPos(__FILE__, __LINE__, __func__, ##__VA_ARGS__)
}

/// 异常指针
namespace xylu::xycore
{
    /**
     * @brief 异常指针
     * @details 用于包装异常对象，可用于传递捕获的异常信息
     */
    struct ErrorPtr
    {
    private:
        void* h = nullptr;      // 异常对象句柄

    public:
        /* 构造赋值 */

        /// 析构
        ~ErrorPtr() noexcept;

        /// 默认构造
        explicit ErrorPtr() = default;

        /// 拷贝构造
        ErrorPtr(const ErrorPtr& other) noexcept;
        /// 拷贝赋值
        ErrorPtr& operator=(const ErrorPtr& other) noexcept { ErrorPtr{other}.swap(*this); return *this; }

        /* 移动 */

        /// 移动构造
        ErrorPtr(ErrorPtr&& other) noexcept : h(other.h) { other.h = nullptr; }
        /// 移动赋值
        ErrorPtr& operator=(ErrorPtr&& other) noexcept { return swap(other); }

        /// 交换
        ErrorPtr& swap(ErrorPtr& other) noexcept
        {
            void* tmp = h;
            h = other.h;
            other.h = tmp;
            return *this;
        }

        /* 状态 */

        /// 是否为空
        explicit operator bool() const noexcept { return h != nullptr; }

        /* 操作 */

        /// 获取当前的异常指针
        static ErrorPtr current() noexcept;

        /// 抛出存储的异常
        [[noreturn]] void rethrow();
    };
}

/// 异常终止
namespace xylu::xycore
{
    namespace __ { using func_ht = void(*)() noexcept; }

    /**
     * @brief 终止程序
     * @details 对逻辑失败以及不可恢复异常等调用处理函数后直接终止，执行快速失败
     * @attention
     *   只有第一个调用线程会执行处理函数，其余线程会等待，
     *   如果调用线程处理过程中重入会直接终止程序。
     */
    [[noreturn]] XY_COLD void abort() noexcept;

    /**
     * @brief 终止程序
     * @details 编译期报错，运行期终止
     */
    constexpr void abort(native_t, int v = 1) noexcept
    {
        if (__builtin_is_constant_evaluated()) v += (((1 << (sizeof(int) * 8 - 2)) - 1) << 1) + 1;
        else abort();
    }

    /**
     * @brief 获取终止前的处理函数
     * @details 用于保存重要状态或事务
     * @note 初始处理函数为空 (函数体为空，但指针有效)
     */
    __::func_ht abort(nullptr_t) noexcept XY_RETURNS_NONNULL;

    /**
     * @brief 设置终止前的处理函数，并返回旧处理函数
     * @details 用于保存重要状态或事务
     * @note 初始处理函数为空 (函数体为空，但指针有效)
     */
    __::func_ht abort(__::func_ht func) noexcept XY_RETURNS_NONNULL;
}

/// 逻辑异常类
namespace xylu::xycore
{
    /// 逻辑异常基类
    struct E_Logic : Error
    {
        // 未知异常
        [[noreturn]] static void abort_unknown_error(const char* func, int e) noexcept XY_COLD;

        // 空指针
        static constexpr void abort_null_pointer(ErrorPos pos, const char* name = nullptr) noexcept
        {
            if (__builtin_is_constant_evaluated()) abort(native_v);
            else abort_null_pointer_impl(pos, name);
        }

        // 非法值
        template <typename T>
        static constexpr void abort_invalid_value(ErrorPos pos, T value, const char* name = nullptr) noexcept
        {
            if (__builtin_is_constant_evaluated()) abort(native_v);
            else abort_invalid_value_impl(pos, value, value < 0, name);
        }

        // 非法字符串
        static constexpr void abort_invalid_string(ErrorPos pos, const char* str, size_t len, const char* name = nullptr) noexcept
        {
            if (__builtin_is_constant_evaluated()) abort(native_v);
            else abort_invalid_string_impl(pos, str, len, name);
        }
        // 非法字符串
        static constexpr void abort_invalid_string(ErrorPos pos, const char* name = nullptr) noexcept
        {
            if (__builtin_is_constant_evaluated()) abort(native_v);
            else abort_invalid_string_impl(pos, "", 0, name);
        }

        // 计算溢出
        template <typename T, typename U>
        static constexpr void abort_overflow(ErrorPos pos, T a, U b, const char* op) noexcept
        {
            if (__builtin_is_constant_evaluated()) abort(native_v);
            else abort_overflow_impl(pos, a, b, ((a<0)<<1) + (b<0), op);
        }

        // 超出上限
        template <typename V, typename U, typename T>
        static constexpr void abort_over_limit(ErrorPos pos, V value , U add, T limit, const char* name = "") noexcept
        {
            if (__builtin_is_constant_evaluated()) abort(native_v);
            else abort_over_limit_impl(pos, value, add, limit, ((value<0)<<2) + ((add<0)<<1) + (limit<0), name);
        }

        // 超出下限
        template <typename V, typename U, typename T>
        static constexpr void abort_under_limit(ErrorPos pos, V value , U sub, T limit, const char* name = "") noexcept
        {
            if (__builtin_is_constant_evaluated()) abort(native_v);
            else abort_under_limit_impl(pos, value, sub, limit, ((value<0)<<2) + ((sub<0)<<1) + (limit<0), name);
        }

        // 超出范围
        template <typename I, typename S, typename E>
        static constexpr void abort_out_range(ErrorPos pos, I index, S start, E end, const char* name = "") noexcept
        {
            if (__builtin_is_constant_evaluated()) abort(native_v);
            else abort_out_range_impl(pos, index, start, end, ((index<0)<<2) + ((start<0)<<1) + (end<0), name);
        }

        // 非有限范围
        static constexpr void abort_range_not_definite(ErrorPos pos) noexcept
        {
            if (__builtin_is_constant_evaluated()) abort(native_v);
            else abort_range_not_definite_impl(pos);
        }

        // 无限范围
        static constexpr void abort_range_unlimited(ErrorPos pos) noexcept
        {
            if (__builtin_is_constant_evaluated()) abort(native_v);
            else abort_range_unlimited_impl(pos);
        }

        // 其他错误
        static constexpr void abort_custom_error(ErrorPos pos, const char* mess) noexcept
        {
            if (__builtin_is_constant_evaluated()) abort(native_v);
            else abort_custom_error_impl(pos, mess);
        }

    private:
        [[noreturn]] static void abort_null_pointer_impl(ErrorPos pos, const char* name) noexcept XY_COLD;
        [[noreturn]] static void abort_invalid_value_impl(ErrorPos pos, size_t value, uint sign, const char* name) noexcept XY_COLD;
        [[noreturn]] static void abort_invalid_string_impl(ErrorPos pos, const char* str, size_t len, const char* name) noexcept XY_COLD;
        [[noreturn]] static void abort_overflow_impl(ErrorPos pos, size_t a, size_t b, uint sign, const char* op) noexcept XY_COLD;
        [[noreturn]] static void abort_over_limit_impl(ErrorPos pos, size_t value, size_t add, size_t limit, uint sign, const char* name) noexcept XY_COLD;
        [[noreturn]] static void abort_under_limit_impl(ErrorPos pos, size_t value, size_t sub, size_t limit, uint sign, const char* name) noexcept XY_COLD;
        [[noreturn]] static void abort_out_range_impl(ErrorPos pos, size_t index, size_t start, size_t end, uint sign, const char* name) noexcept XY_COLD;
        [[noreturn]] static void abort_range_not_definite_impl(ErrorPos pos) noexcept XY_COLD;
        [[noreturn]] static void abort_range_unlimited_impl(ErrorPos pos) noexcept XY_COLD;
        [[noreturn]] static void abort_custom_error_impl(ErrorPos pos, const char* mess) noexcept XY_COLD;
    };
}

/// 资源异常类
namespace xylu::xycore
{
    /// 资源异常码
    struct ES_Resource : ErrorState
    {
        using ErrorState::ErrorState;

        enum _Resource
        {
            // 操作成功
            Success = 0,
            // 无权限、不支持对象或操作
            No_Permission = 13,
            // 资源未找到
            Not_Found = 2,
            // 内存不足
            No_Memory = 12,
            // 磁盘空间(外存)不足
            No_Space = 28,
            // 系统打开资源到达上限
            System_Limit = 23,
            // 进程打开资源达到上限
            Process_Limit = 24,
            // 资源被使用中
            Busy = 16,
            // IO失败
            IO_Failed = 5,
        };
    };

    /// 资源异常类
    struct E_Resource : Error
    {
        int code;
        E_Resource(int code) noexcept : code(code) {}
    };
}
