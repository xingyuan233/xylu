#pragma clang diagnostic push
#pragma ide diagnostic ignored "OCUnusedGlobalDeclarationInspection"
#pragma ide diagnostic ignored "OCUnusedStructInspection"
#pragma once

/**
 * @brief 检测CPU对 SIMD 指令集的支持
 * @note 仅提供测试方法，实际不使用 (编译前在 xycore/config.h 中配置合适的版本)
 */

namespace xylu::xycore
{
    struct CPUFeatures
    {
        bool sse;
        bool sse2;
        bool sse3;
        bool ssse3;
        bool sse4_1;
        bool sse4_2;
        bool avx;
        bool avx2;
        bool avx512f;
        bool avx512dq;
        bool avx512bw;
        bool avx512vl;

        // 构造函数，获取CPU支持的SIMD指令集
        CPUFeatures() noexcept;

        // 打印到控制台
        void print() const noexcept;
    };
}

#pragma clang diagnostic pop