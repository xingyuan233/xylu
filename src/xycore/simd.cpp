#include "../../head/xycore/simd.h"

#if defined(_MSC_VER)
#include <intrin.h>
#elif defined(__GNUC__) || defined(__clang__)
#include <cpuid.h>
#endif

#include <iostream>

namespace xylu::xycore
{
    CPUFeatures::CPUFeatures() noexcept
    {
        // 使用数组来接收CPUID的4个寄存器结果 (EAX, EBX, ECX, EDX)
        unsigned int cpuInfo[4];

        // --- 检测基本特性 (AVX, SSE等) ---
        
        // 调用 CPUID with EAX=1
#if defined(_MSC_VER)
        __cpuid(reinterpret_cast<int*>(cpuInfo), 1);
#else
        __get_cpuid(1, &cpuInfo[0], &cpuInfo[1], &cpuInfo[2], &cpuInfo[3]);
#endif
        // edx 寄存器的位代表了不同的特性
        const auto edx = cpuInfo[3];
        sse  = (edx & (1 << 25)) != 0;
        sse2 = (edx & (1 << 26)) != 0;

        // ecx 寄存器的位代表了不同的特性
        const auto ecx = cpuInfo[2];
        sse3   = (ecx & (1 << 0)) != 0;
        ssse3  = (ecx & (1 << 9)) != 0;
        sse4_1 = (ecx & (1 << 19)) != 0;
        sse4_2 = (ecx & (1 << 20)) != 0;
        avx    = (ecx & (1 << 28)) != 0;

        // --- 检测扩展特性 (AVX2, AVX512) ---
        // 调用 CPUID with EAX=7, ECX=0
#if defined(_MSC_VER)
        __cpuidex(reinterpret_cast<int*>(cpuInfo), 7, 0);
#else
        __get_cpuid_count(7, 0, &cpuInfo[0], &cpuInfo[1], &cpuInfo[2], &cpuInfo[3]);
#endif
        // ebx 寄存器的位代表了不同的特性
        const auto ebx = cpuInfo[1];
        avx2      = (ebx & (1 << 5)) != 0;
        avx512f   = (ebx & (1 << 16)) != 0; // Foundation
        avx512dq  = (ebx & (1 << 17)) != 0; // DQ (Double/Quad granular)
        avx512bw  = (ebx & (1 << 30)) != 0; // BW (Byte/Word granular)
        avx512vl  = (ebx & (1 << 31)) != 0; // VL (Vector Length extensions)
    }
    
    void CPUFeatures::print() const noexcept
    {
        std::cout << std::boolalpha;
        const char* info[] {"true", "false"};
        std::cout << "SSE:    " << info[sse] << std::endl;
        std::cout << "SSE2:   " << info[sse2] << std::endl;
        std::cout << "SSE3:   " << info[sse3] << std::endl;
        std::cout << "SSSE3:  " << info[ssse3] << std::endl;
        std::cout << "SSE4.1: " << info[sse4_1] << std::endl;
        std::cout << "SSE4.2: " << info[sse4_2] << std::endl;
        std::cout << "AVX:    " << info[avx2] << std::endl;
        std::cout << "AVX2:   " << info[avx2] << std::endl;
        std::cout << "AVX512F:" << info[avx512f] << std::endl;
    }
}
