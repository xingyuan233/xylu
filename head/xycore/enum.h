#pragma clang diagnostic push
#pragma ide diagnostic ignored "OCUnusedGlobalDeclarationInspection"
#pragma once

#include "./config.h"

/// 日志
namespace xylu::xycore
{
    /// 日志级别
    enum N_LOG_LEVEL
    {
        // 日志级别: 无日志
        N_LOG_NONE = XY_LOG_LEVEL_NONE,
        // 日志级别: 致命错误
        N_LOG_FATAL = XY_LOG_LEVEL_FATAL,
        // 日志级别: 错误
        N_LOG_ERROR = XY_LOG_LEVEL_ERROR,
        // 日志级别: 警告
        N_LOG_WARN = XY_LOG_LEVEL_WARN,
        // 日志级别: 信息
        N_LOG_INFO = XY_LOG_LEVEL_INFO,
        // 日志级别: 调试
        N_LOG_DEBUG = XY_LOG_LEVEL_DEBUG,
        // 日志级别: 追踪
        N_LOG_TRACE = XY_LOG_LEVEL_TRACE,
        // 日志级别: 所有日志
        N_LOG_ALL = XY_LOG_LEVEL_ALL,
    };
    // 全局日志级别
    constexpr N_LOG_LEVEL K_LOG_LEVEL = static_cast<N_LOG_LEVEL>(XY_LOG_LEVEL);
}

/// 原子操作内存序
namespace xylu::xycore
{
    /// 内存序
    enum N_ATOMIC_ORDER
    {
        // 内存序: 仅保证原子性，不保证同步或顺序
        N_ATOMIC_RELAXED = XY_ATOMIC_ORDER_RELAXED,
        // 内存序: 消费操作，load之后依赖它的读写排在后面，保证可见其他线程 release或更高级别 的写入
        N_ATOMIC_CONSUME = XY_ATOMIC_ORDER_CONSUME,
        // 内存序: 获得操作，load之后的读写排在后面，可见其他线程 release或更高级别 的写入
        N_ATOMIC_ACQUIRE = XY_ATOMIC_ORDER_ACQUIRE,
        // 内存序: 释放操作，store之前的读写排在前面，使其他线程 consume/acquire或更高级别 的读取可见
        N_ATOMIC_RELEASE = XY_ATOMIC_ORDER_RELEASE,
        // 内存序: 获得+释放
        N_ATOMIC_ACQ_REL = XY_ATOMIC_ORDER_ACQ_REL,
        // 内存序: 获得+释放，且保证所有线程观测到的顺序一致
        N_ATOMIC_SEQ_CST = XY_ATOMIC_ORDER_SEQ_CST,
    };
    // 全局默认内存序
    constexpr N_ATOMIC_ORDER K_ATOMIC_ORDER = static_cast<N_ATOMIC_ORDER>(XY_ATOMIC_ORDER);
}

#pragma clang diagnostic pop