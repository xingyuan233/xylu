#pragma clang diagnostic push
#pragma ide diagnostic ignored "OCUnusedGlobalDeclarationInspection"
#pragma once

#include "./type.h"

/// 配置
namespace xylu::xycore
{
    // 默认对齐大小
    constexpr size_t K_DEFAULT_ALIGN = __STDCPP_DEFAULT_NEW_ALIGNMENT__;

    // 系统缓存行大小 (用于避免伪共享)
    constexpr size_t K_CACHE_LINE_SIZE = __GCC_DESTRUCTIVE_SIZE;

    //系统时差 (单位: 小时)
    constexpr auto K_TIME_DIFFERENCE = +8;

    // 是否开启调试模式 (进行一些更精细的检测，特别是逻辑错误)
    #define XY_DEBUG 1

    // 是否不使用多线程 (导入线程库时进行静态断言)
    #define XY_UNTHREAD 0

    // 日志等级 (仅用于设置 XY_LOG_LEVEL 宏，之后会被 undef，使用 N_LOG_XX 枚举代替)
    #define XY_LOG_LEVEL_NONE   0
    #define XY_LOG_LEVEL_FATAL  1
    #define XY_LOG_LEVEL_ERROR  2
    #define XY_LOG_LEVEL_WARN   3
    #define XY_LOG_LEVEL_INFO   4
    #define XY_LOG_LEVEL_DEBUG  5
    #define XY_LOG_LEVEL_TRACE  6
    #define XY_LOG_LEVEL_ALL    7
    // 全局日志等级 (设置后会被 undef，使用 K_LOG_LEVEL 代替)
    #define XY_LOG_LEVEL 7

    // 内存序 (仅用于设置 XY_ATOMIC_ORDER 宏，之后会被 undef，使用 N_ATOMIC_XX 枚举代替)
    #define XY_ATOMIC_ORDER_RELAXED 0
    #define XY_ATOMIC_ORDER_CONSUME 1
    #define XY_ATOMIC_ORDER_ACQUIRE 2
    #define XY_ATOMIC_ORDER_RELEASE 3
    #define XY_ATOMIC_ORDER_ACQ_REL 4
    #define XY_ATOMIC_ORDER_SEQ_CST 5
    // 全局默认内存序 (设置后会被 undef，使用 K_ATOMIC_ORDER 代替)
    #define XY_ATOMIC_ORDER 5

    /**
     * @brief CPU 支持的 SSE 指令集 版本
     * @note 0: 不支持
     * @note 10: 支持 SSE 指令集
     * @note 20: 支持 SSE2 指令集
     * @note 30: 支持 SSE3 指令集
     * @note 31[~39]: 支持 SSSE3 指令集
     * @note 41: 支持 SSE4.1 指令集
     * @note 42: 支持 SSE4.2 指令集
     * @note 可以通过 <xycore/simd.h> 进行检测，因SSE2已经广泛支持不再进行动态检测
     */
    #define XY_SSE 42

    /**
     * @brief CPU 支持的 AVX 指令集 版本
     * @note 0: 不支持
     * @note 10: 支持 AVX 指令集
     * @note 20: 支持 AVX2 指令集
     * @note 可以通过 <xycore/simd.h> 进行检测，因AVX2已经广泛支持不再进行动态检测
     */
    #define XY_AVX 20
}

#pragma clang diagnostic pop