#pragma clang diagnostic push
#pragma ide diagnostic ignored "OCUnusedGlobalDeclarationInspection"
#pragma once

#include "../../link/attr"
#include "../../link/traits"

/* 内存操作相关函数 */

namespace xylu::xymemory
{
    /**
     * @brief 将 src 指向的内存区域中的 bytes 个字节拷贝到 dst 指向的内存区域。
     * @param dst 目的内存区域的指针。(不能为 nullptr)
     * @param src 源内存区域的指针。(不能为 nullptr)
     * @param bytes 要拷贝的字节数。
     * @note 源和目的内存区域不能重叠。如果可能发生重叠，请使用 mem_move。
     */
    XY_NONNULL(1, 2)
    inline void mem_copy(void* dst, const void* src, xyu::size_t bytes) noexcept
    { __builtin_memcpy(dst, src, bytes); }

    /**
     * @brief 将 src 指向的内存区域中的 bytes 个字节移动到 dst 指向的内存区域。
     * @details 该函数能够正确处理源和目的内存区域重叠的情况，因此比 mem_copy 更安全。
     * @param dst 目的内存区域的指针，不能为 nullptr。
     * @param src 源内存区域的指针，不能为 nullptr。
     * @param bytes 要移动的字节数。
     */
    XY_NONNULL(1, 2)
    inline void mem_move(void* dst, const void* src, xyu::size_t bytes) noexcept
    { __builtin_memmove(dst, src, bytes); }

    /**
     * @brief 将 dst 指向的内存区域的前 bytes 个字节填充为指定的 value 值。
     * @param dst 要填充的内存区域的指针，不能为 nullptr。
     * @param bytes 要填充的字节数。
     * @param value 用于填充的字节值，默认为 0。
     */
    XY_NONNULL(1)
    inline void mem_set(void* dst, xyu::size_t bytes, xyu::uint8 value = 0) noexcept
    { __builtin_memset(dst, value, bytes); }

    /**
     * @brief 按字节比较两个内存区域。
     * @param src1 指向第一个内存区域的指针。
     * @param src2 指向第二个内存区域的指针。
     * @param bytes 要比较的字节数。
     * @return 一个整数值：
     *         - ˂ 0, 表示 src1 小于 src2。
     *         - = 0, 表示 src1 等于 src2。
     *         - ˃ 0, 表示 src1 大于 src2。
     * @note 为了与语言内置的指针比较行为兼容，该函数将 nullptr 视作小于任何非空指针。
     */
    constexpr int mem_cmp(const void* src1, const void* src2, xyu::size_t bytes) noexcept
    {
        if (XY_UNLIKELY(!src1)) return src2 != nullptr;
        return __builtin_memcmp(src1, src2, bytes);
    }

    /**
     * @brief 在给定的内存区域中搜索指定值的第一次出现。
     * @param src 要搜索的内存区域的指针，不能为 nullptr。
     * @param bytes 要搜索的字节数。
     * @param value 要搜索的字节值。
     * @return 如果找到，返回该值在 src 中的字节偏移量（从0开始）；
     *         如果未找到，返回 static_cast<xyu::size_t>(-1) (即该无符号类型的最大值)。
     */
    XY_NONNULL(1)
    inline xyu::size_t mem_find(const void* src, xyu::size_t bytes, xyu::uint8 value) noexcept
    {
        auto p = static_cast<const xyu::uint8*>(__builtin_memchr(src, value, bytes));
        return p ? p - static_cast<const xyu::uint8*>(src) : -1;
    }

}

#pragma clang diagnostic pop