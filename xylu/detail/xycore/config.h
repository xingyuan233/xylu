#pragma once

/**
 * @file    xycore/config.h
 * @brief   编译相关配置及常量
 * @date    2025-12-19
 */

#include "./attr.h"
#include "./type.h"

/// 手动配置
namespace xylu::xycore
{
    // 是否为详细调试模式：进行更多更复杂的检测
    #define XY_DEBUG 1
    // 是否为发布使用模式：删除所有契约式检测 (设置后将 XY_DEBUG 置0)
    #define XY_RELEASE 0

    // 是否使用多线程
    #define XY_THREAD 1

    // 全局日志等级是否固定
    #define XY_FIXED_LOG_LEVEL 1

    /**
    * @brief CPU 支持的 SSE 指令集 版本
    * @details
    *   -  0: 不支持
    *   - 10: 支持 SSE 指令集
    *   - 20: 支持 SSE2 指令集
    *   - 30: 支持 SSE3 指令集
    *   - 31[~39]: 支持 SSSE3 指令集
    *   - 41: 支持 SSE4.1 指令集
    *   - 42: 支持 SSE4.2 指令集
    * @note 因SSE2已经广泛支持不再进行动态检测
    */
    #define XY_SSE 42

    /**
     * @brief CPU 支持的 AVX 指令集 版本
     * @details
     *   -  0: 不支持
     *   - 10: 支持 AVX 指令集
     *   - 20: 支持 AVX2 指令集
     * @note 因AVX2已经广泛支持不再进行动态检测
     */
    #define XY_AVX 20

    /**
     * @brief 是否支持 NEON
     * @note 已经广泛支持不再进行动态检测
     */
    #define XY_NEON 0

}

/// 枚举常量及配置
namespace xylu::xycore
{
    /// 日志级别
    enum class N_Log_Level
    {
        // 日志级别: 无日志
        None = 0,
        // 日志级别: 致命错误
        Fatal = 1,
        // 日志级别: 错误
        Error = 2,
        // 日志级别: 警告
        Warn = 3,
        // 日志级别: 信息
        Info = 4,
        // 日志级别: 调试
        Debug = 5,
        // 日志级别: 追踪
        Trace = 6,
        // 日志级别: 所有日志
        All = 7,
    };
    // 全局日志级别
#if XY_FIXED_LOG_LEVEL
    constexpr
#endif
    inline N_Log_Level K_Log_Level = N_Log_Level::All;

    /// 内存序
    enum class N_Atomic_Order
    {
        // 内存序: 仅保证原子性，不保证同步或顺序
        Relaxed = __ATOMIC_RELAXED,
        // 内存序: 消费操作，load之后依赖它的读写排在后面，保证可见其他线程 release或更高级别 的写入
        // Consume = __ATOMIC_CONSUME [[deprecated]]
        // 内存序: 获取操作，load之后的读写排在后面，可见其他线程 release或更高级别 的写入
        Acquire = __ATOMIC_ACQUIRE,
        // 内存序: 释放操作，store之前的读写排在前面，使其他线程 consume/acquire或更高级别 的读取可见
        Release = __ATOMIC_RELEASE,
        // 内存序: 获取+释放
        Acq_Rel = __ATOMIC_ACQ_REL,
        // 内存序: 获取+释放，且保证所有线程观测到的顺序一致
        Seq_Cst = __ATOMIC_SEQ_CST,
    };
}

/// 自动配置
namespace xylu::xycore
{
    // 默认对齐大小
    constexpr size_t K_Default_Align = __STDCPP_DEFAULT_NEW_ALIGNMENT__;

    // 系统缓存行大小 (用于避免伪共享)
#if defined(__x86_64__) || defined(__i386__)
    constexpr size_t K_Cache_Line_Size = 64;
#elif defined(__aarch64__)
    constexpr size_t K_Cache_Line_Size = 128;
#elif defined(__PPC64__)
    constexpr size_t K_Cache_Line_Size = 128;
#elif defined(__s390x__) || defined(__zarch__)
    constexpr size_t K_Cache_Line_Size = 256;
#else
    constexpr size_t K_Cache_Line_Size = 64;
#endif

    // 是否为小端序
    #define XY_LITTLE_ENDIAN (__BYTE_ORDER__ == __ORDER_LITTLE_ENDIAN__)
    // 是否为大端序
    #define XY_BIG_ENDIAN (__BYTE_ORDER__ == __ORDER_BIG_ENDIAN__)
}

/// 配置检测与处理
namespace xylu::xycore
{
    // 默认对齐检测
    static_assert(K_Default_Align, "K_DEFAULT_ALIGN must not be zero");
    static_assert(!(K_Default_Align & (K_Default_Align - 1)), "K_DEFAULT_ALIGN must be power of 2");

    // 检查多线程设置
    static_assert(XY_THREAD == 0 || XY_THREAD == 1, "XY_THREAD must be 0 or 1");

    // 检查DEBUG
    static_assert(XY_DEBUG == 0 || XY_DEBUG == 1, "XY_DEBUG must be 0 or 1");
    // 检查RELEASE
    static_assert(XY_RELEASE == 0 || XY_RELEASE == 1, "XY_RELEASE must be 0 or 1");

#if XY_RELEASE
#    undef XY_DEBUG
#    define XY_DEBUG 0
#endif

#if XY_DEBUG
    // 非DEBUG模式下进行非空检查及判定
#    define XY_NONNULL_NDEBUG(...)
#else
    // 非DEBUG模式下进行非空检查及判定
#    define XY_NONNULL_NDEBUG(...) XY_NONNULL(__VA_ARGS__)
#endif

#if XY_RELEASE
    // RELEASE模式下不进行非空检查
#    define XY_NONNULL_RELEASE(...) XY_NONNULL(__VA_ARGS__)
#else
    // RELEASE模式下不进行非空检查
#    define XY_NONNULL_RELEASE(...)
#endif

    // SSE宏生成
#if XY_SSE >= 10 && !defined(__SSE__)
#    define __SSE__
#elif XY_SSE < 10 && defined(__SSE__)
#    undef __SSE__
#endif
#if XY_SSE >= 20 && !defined(__SSE2__)
#    define __SSE2__
#elif XY_SSE < 20 && defined(__SSE2__)
#    undef __SSE2__
#endif
#if XY_SSE >= 30 && !defined(__SSE3__)
#    define __SSE3__
#elif XY_SSE < 30 && defined(__SSE3__)
#    undef __SSE3__
#endif
#if XY_SSE >= 31 && !defined(__SSSE3__)
#    define __SSSE3__
#elif XY_SSE < 31 && defined(__SSSE3__)
#    undef __SSSE3__
#endif
#if XY_SSE >= 41 && !defined(__SSE4_1__)
#    define __SSE4_1__
#elif XY_SSE < 41 && defined(__SSE4_1__)
#    undef __SSE4_1__
#endif
#if XY_SSE >= 42 && !defined(__SSE4_2__)
#    define __SSE4_2__
#elif XY_SSE < 42 && defined(__SSE4_2__)
#    undef __SSE4_2__
#endif
#undef XY_SSE

    // AVX宏生成
#if XY_AVX >= 10 && !defined(__AVX__)
#    define __AVX__
#elif XY_AVX < 10 && defined(__AVX__)
#    undef __AVX__
#endif
#if XY_AVX >= 20 && !defined(__AVX2__)
#    define __AVX2__
#elif XY_AVX < 20 && defined(__AVX2__)
#    undef __AVX2__
#endif
#undef XY_AVX

    // NEON宏生成
#if XY_NEON
#   define __ARM_NEON__
#else
#   undef __ARM_NEON__
#endif
#undef XY_NEON

}
