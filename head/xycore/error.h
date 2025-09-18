#pragma clang diagnostic push
#pragma ide diagnostic ignored "OCUnusedGlobalDeclarationInspection"
#pragma ide diagnostic ignored "OCUnusedStructInspection"
#pragma once

#include "../../link/attr"
#include "../../link/traits"

/* 异常 */

/// 异常基类
namespace xylu::xycore
{
    /**
     * @brief 异常基类
     * @note 所有异常类都应该继承自该类
     */
    struct Error
    {
        virtual ~Error() = default;
    };

    /**
     * @brief 构建异常对象
     * @details 通常用于 没有明确的异常语义 或 可以直接通过父类类型进行捕获处理，且可由多种异常类型捕获的情况
     * @tparam Errs 异常类型列表
     * @return 继承所有异常类型的异常对象
     * @example throw make_error(E_Logic_Invalid_Argument{}, E_File{}); 来指示一个 OpenMode 不存在的错误
     */
    template <typename... Errs, typename = xyu::t_enable<(... && xyu::t_is_base_of<Error, Errs>)>>
    constexpr auto make_error(const Errs&...)
    {
        static_assert(sizeof...(Errs) > 0);
        struct Tmp_Error : Errs... {};
        return Tmp_Error{};
    }
}

/// 异常指针
namespace xylu::xycore
{
    /**
     * @brief 异常指针
     * @note 用于包装异常对象，可用于传递捕获的异常信息
     */
    struct ErrorPtr : class_no_copy_t
    {
    private:
        void* p = nullptr;  // 异常对象指针

    public:
        /* 构造赋值 */
        /// 默认构造
        ErrorPtr() noexcept = default;
        /// 空构造
        explicit ErrorPtr(nullptr_t) noexcept : p(nullptr) {}
        /// 空赋值
        ErrorPtr& operator=(nullptr_t) noexcept { p = nullptr; return *this; }

        /* 移动 */
        /// 移动构造
        ErrorPtr(ErrorPtr&& other) noexcept : p(other.p) { other.p = nullptr; }
        /// 移动赋值
        ErrorPtr& operator=(ErrorPtr&& other) noexcept { xyu::swap(p, other.p); return *this; }

        /* 状态 */
        /// 是否为空
        explicit operator bool() const noexcept { return p != nullptr; }
        /// 是否不为空
        bool operator!() const noexcept { return p == nullptr; }

        /* 操作 */
        /// 获取当前的异常指针
        static ErrorPtr current() noexcept;
        /// 抛出存储的异常
        [[noreturn]] void rethrow();
    };
}

/// 内存异常类
namespace xylu::xycore
{
    /// 内存异常基类
    struct E_Memory : Error {};

    /// 内存分配失败
    struct E_Memory_Alloc : E_Memory {};

    /// 对齐错误
    struct E_Memory_Align : E_Memory {};

    /// 内存分配容量超出允许范围
    struct E_Memory_Capacity : E_Memory {};
}

/// 逻辑异常类
namespace xylu::xycore
{
    /// 逻辑异常基类
    struct E_Logic : Error {};

    /// 超出有效范围
    struct E_Logic_Out_Of_Range : E_Logic {};

    /// 不存在的键
    struct E_Logic_Key_Not_Found : E_Logic {};

    /// 无效的参数
    struct E_Logic_Invalid_Argument : E_Logic {};

    /// 空指针
    struct E_Logic_Null_Pointer : E_Logic_Invalid_Argument {};

    /// 无效的迭代器
    struct E_Logic_Invalid_Iterator : E_Logic_Invalid_Argument {};
}

/// 格式化异常类
namespace xylu::xycore
{
    /// 格式化异常基类
    struct E_Format : Error {};

    /// 格式字符串语法错误
    struct E_Format_Syntax : E_Format {};

    /// 格式化参数错误
    struct E_Format_Argument : E_Format {};

    /// env错误
    struct E_Format_Environment : E_Format {};

    /// lay错误
    struct E_Format_Layout : E_Format {};

    /// ptn或ex错误
    struct E_Format_PtnEx : E_Format {};

    /// 动态格式错误
    struct E_Format_Dynamic : E_Format {};
}

/// 资源异常类
namespace xylu::xycore
{
    /// 资源异常基类
    struct E_Resource : Error {};

    /// 资源不存在
    struct E_Resource_Not_Found : E_Resource {};

    /// 系统内存资源不足
    struct E_Resource_No_Memory : E_Resource, E_Memory_Alloc {};

    /// 临时资源不可用
    struct E_Resource_Temp_Unavailable : E_Resource {};

    /// 操作资源的权限不足
    struct E_Resource_Permission_Denied : E_Resource {};

    /// 资源已被占用
    struct E_Resource_Busy : E_Resource {};

    /// 资源失效
    struct E_Resource_Invalid_State : E_Resource {};
}

/// 文件异常类
namespace xylu::xycore
{
    /// 文件异常基类
    struct E_File : E_Resource {};

    /// 路径为目录
    struct E_File_Path_Is_Dir : E_File, E_Logic_Invalid_Argument {};

    /// 文件未找到
    struct E_File_Not_Found : E_File, E_Resource_Not_Found {};

    /// 权限不足
    struct E_File_Permission_Denied : E_File, E_Resource_Permission_Denied {};

    /// 系统没有足够空间来完成操作
    struct E_File_No_Memory : E_File, E_Resource_No_Memory {};

    /// 文件未打开或已关闭
    struct E_File_Invalid_State : E_File, E_Resource_Invalid_State {};

    /// 进程打开文件过多
    struct E_File_Process_Limit : E_File {};

    /// 系统打开文件过多
    struct E_File_System_Limit : E_File {};

    /// IO错误
    struct E_File_IO : E_File {};

    /// 物理文件错误
    struct E_File_Physical : E_File_IO {};

    /// 管道错误
    struct E_File_Pipe : E_File_IO {};

    /// 文件过大
    struct E_File_Too_Large : E_File_IO {};
}

/// 线程异常类
namespace xylu::xycore
{
    /// 线程异常基类
    struct E_Thread : E_Resource {};

    /// 权限不足
    struct E_Thread_Permission_Denied : E_Thread, E_Resource_Permission_Denied {};

    /// 系统资源空间不足
    struct E_Thread_No_Memory : E_Thread, E_Resource_No_Memory {};

    /// 线程未启动或已失效
    struct E_Thread_Invalid_State : E_Thread, E_Resource_Invalid_State {};

    /// 系统创建的线程数达到上限
    struct E_Thread_Create_Limit : E_Thread {};

    /// 线程死锁
    struct E_Thread_Deadlock : E_Thread {};

    /// 驱动设备错误
    struct E_Thread_Device : E_Thread {};
}

/// 锁异常类
namespace xylu::xycore
{
    /// 锁异常基类
    struct E_Mutex : E_Resource {};

    /// 权限不足
    struct E_Mutex_Permission_Denied : E_Mutex, E_Resource_Permission_Denied {};

    /// 系统资源空间不足
    struct E_Mutex_No_Memory : E_Mutex, E_Resource_No_Memory {};

    /// 缺乏初始化锁的资源
    struct E_Mutex_Tmp_Unavailable : E_Mutex, E_Resource_Temp_Unavailable {};

    /// 锁未初始化或已被释放
    struct E_Mutex_Invalid_State : E_Mutex, E_Resource_Invalid_State {};

    /// 互斥锁已被锁定
    struct E_Mutex_Already_Locked : E_Mutex {};

    /// 互斥锁未被锁定
    struct E_Mutex_Not_Locked : E_Mutex {};

    /// 当前线程未持有锁
    struct E_Mutex_Not_Owned : E_Mutex {};

    /// 死锁
    struct E_Mutex_Deadlock : E_Mutex {};

    /// 递归锁超出最大递归次数
    struct E_Mutex_Recursive_Limit : E_Mutex {};
}

/// 条件变量异常类
namespace xylu::xycore
{
    /// 条件变量异常基类
    struct E_CondVar : E_Resource {};

    /// 系统资源空间不足
    struct E_CondVar_No_Memory : E_CondVar, E_Resource_No_Memory {};

    /// 缺乏初始化锁的资源
    struct E_CondVar_Tmp_Unavailable : E_CondVar, E_Resource_Temp_Unavailable {};

    /// 条件变量未初始化或已被释放
    struct E_CondVar_Invalid_State : E_CondVar, E_Resource_Invalid_State {};

    /// 当前线程未持有锁
    struct E_CondVar_Not_Owned : E_CondVar, E_Mutex_Not_Owned {};
}


// 异常信息输出
namespace xylu::xystring::__
{
    // 输出信息 (实现后置)
    template <xyu::size_t N, typename... Args>
    XY_COLD void output_error(bool is_fatal, const char (&fmt)[N], Args&&... args);

    // 仅供 format 依赖的基础库中使用 (其他请使用 xyloge)
#define xylogei(is_fatal, fmt, ...) \
    [&](const char* func){          \
        constexpr const char __sfmt[] = __FILE__ ":{}:{} " fmt; \
        xylu::xystring::__::output_error<sizeof(__sfmt)>(is_fatal, __sfmt, __LINE__, func, ##__VA_ARGS__); \
    }(__func__)
}

#pragma clang diagnostic pop