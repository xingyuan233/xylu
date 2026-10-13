#pragma once

/**
 * @file    xymemory/fun.h
 * @brief   内存操作相关函数
 * @date    2025-12-26
 */

#include "./_.h"
#include "../xycore/config.h"
#include "../xytraits/class.h"
#include "../xymath/of.h"

namespace xylu::xymemory
{
    /**
     * @brief 获取对象的实际地址
     * @details 用于重载了 operator& 的类
     */
    template <typename T>
    constexpr T* mem_addr(T& value) noexcept { return __builtin_addressof(value); }

    /**
     * @brief 将对象解释为其他类型
     * @tparam To 解释后的类型
     * @param value 被解释的对象
     * @attention 不能用于解释为指针、引用、数组
     */
    template <typename To, typename From>
    constexpr To mem_cast(From&& value) noexcept { return __builtin_bit_cast(To, value); }

    /**
     * @brief 字节拷贝
     * @param dst 目的内存区域的指针，不能为 nullptr
     * @param src 源内存区域的指针，不能为 nullptr
     * @param count 拷贝的元素个数 (当指针为 void* / char* 时等于拷贝的字节数)
     * @details 源和目的内存区域不能重叠，如果可能发生重叠，请使用 mem_move
     */
    template <typename T, typename U> XY_NONNULL_RELEASE(1, 2)
    constexpr void mem_copy(T* dst, const U* src, xyu::size_t count) noexcept
    {
        using Tx = xyu::t_cond<xyu::t_is_void<T>, char, T>;
        using Ux = xyu::t_cond<xyu::t_is_void<U>, char, U>;
        static_assert(sizeof(Tx) == sizeof(Ux));
        if constexpr (!XY_RELEASE)
        {
            if (XY_UNLIKELY(dst == nullptr))
                xyu::E_Logic::abort_null_pointer(XY_CurPos(), "dst");
            if (XY_UNLIKELY(src == nullptr))
                xyu::E_Logic::abort_null_pointer(XY_CurPos(), "src");
            xyu::calc_of_mul<xyu::size_t>(sizeof(Tx), count, XY_CurPos());
            if (!xyu::t_constant()) if (XY_UNLIKELY(
                    (void*)src < (void*)dst && (void*)((const Ux*)src + count) > (void*)dst ||
                    (void*)src > (void*)dst && (void*)src < (void*)((Tx*)dst + count)))
                xyu::E_Logic::abort_custom_error(XY_CurPos(), "overlapping src and dst buffers");
        }

        if constexpr (!xyu::t_is_void<T> && !xyu::t_is_void<U> && xyu::t_can_trivial_cpassign<Tx>) if (xyu::t_constant())
        {
            for (xyu::size_t i = 0; i < count; ++i) dst[i] = mem_cast<T>(src[i]);
            return;
        }
        __builtin_memcpy(dst, src, count * sizeof(Tx));
    }

    /**
     * @brief 字节拷贝
     * @param dst 目的内存区域的指针，不能为 nullptr
     * @param src 源内存区域的指针，不能为 nullptr
     * @param count 拷贝的元素个数 (当指针为 void* / char* 时等于拷贝的字节数)
     * @details 该函数能够正确处理源和目的内存区域重叠的情况，比 mem_copy 更安全
     * @note 编译期只能比较同一个数组的指针
     */
    template <typename T, typename U> XY_NONNULL_RELEASE(1, 2)
    constexpr void mem_move(T* dst, const U* src, xyu::size_t count) noexcept
    {
        using Tx = xyu::t_cond<xyu::t_is_void<T>, char, T>;
        using Ux = xyu::t_cond<xyu::t_is_void<U>, char, U>;
        static_assert(sizeof(Tx) == sizeof(Ux));
        if constexpr (!XY_RELEASE)
        {
            if (XY_UNLIKELY(dst == nullptr))
                xyu::E_Logic::abort_null_pointer(XY_CurPos(), "dst");
            if (XY_UNLIKELY(src == nullptr))
                xyu::E_Logic::abort_null_pointer(XY_CurPos(), "src");
            xyu::calc_of_mul<xyu::size_t>(sizeof(Tx), count, XY_CurPos());
        }

        if constexpr (!xyu::t_is_void<T> && !xyu::t_is_void<U> && xyu::t_can_trivial_cpassign<Tx>) if (xyu::t_constant())
        {
            if (src < dst) for (xyu::size_t i = count - 1; i < count; --i) dst[i] = mem_cast<T>(src[i]);
            else for (xyu::size_t i = 0; i < count; ++i) dst[i] = mem_cast<T>(src[i]);
            return;
        }
        __builtin_memmove(dst, src, count * sizeof(Tx));
    }

    /**
     * @brief 字节填充
     * @param dst 要填充的内存区域的指针，不能为 nullptr
     * @param value 用于填充的字节值
     * @param count 填充的元素个数 (当指针为 void* / char* 时等于拷贝的字节数)
     */
    template <typename T, typename U> XY_NONNULL_RELEASE(1)
    constexpr void mem_set(T* dst, U value, xyu::size_t count) noexcept
    {
        using Tx = xyu::t_cond<xyu::t_is_void<T>, char, T>;
        static_assert(sizeof(U) == 1);
        if constexpr (!XY_RELEASE)
        {
            if (XY_UNLIKELY(dst == nullptr))
                xyu::E_Logic::abort_null_pointer(XY_CurPos(), "dst");
            xyu::calc_of_mul<xyu::size_t>(sizeof(Tx), count, XY_CurPos());
        }

        if constexpr (!xyu::t_is_void<T> && xyu::t_can_trivial_cpassign<Tx>) if (xyu::t_constant())
        {
            struct V { char data[sizeof(Tx)]; } v{};
            for (xyu::size_t i = 0; i < sizeof(Tx); ++i) v.data[i] = value;
            for (xyu::size_t i = 0; i < count; ++i) dst[i] = mem_cast<T>(v);
            return;
        }
        __builtin_memset(dst, value, count * sizeof(Tx));
    }

    namespace __
    {
        xyu::size_t contrast(const char* src, const char* dst, xyu::size_t len) noexcept;
    }
    /**
     * @brief 比较两块内存，返回首个不匹配位置的无符号字节差，全部相同返回 0
     * @param src1 第一个内存区域的指针，不能为 nullptr
     * @param src2 第二个内存区域的指针，不能为 nullptr
     * @param bytes 比较的字节数
     */
    template <typename T, typename U> XY_NONNULL_RELEASE(1, 2)
    constexpr int mem_cmp(const T* src1, const U* src2, xyu::size_t bytes) noexcept
    {
        using Tx = xyu::t_cond<xyu::t_is_void<T>, char, T>;
        using Ux = xyu::t_cond<xyu::t_is_void<U>, char, U>;
        static_assert(sizeof(Tx) == sizeof(Ux) && sizeof(Tx) == 1);
        if constexpr (!XY_RELEASE)
        {
            if (XY_UNLIKELY(src1 == nullptr))
                xyu::E_Logic::abort_null_pointer(XY_CurPos(), "src1");
            if (XY_UNLIKELY(src2 == nullptr))
                xyu::E_Logic::abort_null_pointer(XY_CurPos(), "src2");
        }

        if constexpr (!xyu::t_is_void<T> && !xyu::t_is_void<U>) if (xyu::t_constant())
        {
            for (xyu::size_t i = 0; i < bytes; ++i)
                if (int r = (xyu::uint8)src1[i] - (xyu::uint8)src2[i]) return r;
            return 0;
        }
        xyu::size_t index = __::contrast((const char*)src1, (const char*)src2, bytes);
        return index == bytes ? 0 : ((const xyu::uint8*)src1)[index] - ((const xyu::uint8*)src2)[index];
    }

    namespace __
    {
        xyu::size_t find(const char* src, char c, xyu::size_t len) noexcept;
    }
    /**
     * @brief 查找指定值的第一次出现的位置
     * @param src 查找的内存区域的指针，不能为 nullptr
     * @param value 查找的字节值
     * @param bytes 查找的字节数
     * @details 如果找到，返回对应位置的索引，否则返回 -1 (无符号数)
     */
    template <typename T, typename U> XY_NONNULL_RELEASE(1)
    constexpr xyu::size_t mem_find(const T* src, U value, xyu::size_t bytes) noexcept
    {
        using Tx = xyu::t_cond<xyu::t_is_void<T>, char, T>;
        static_assert(sizeof(Tx) == 1 && sizeof(U) == 1);
        if constexpr (!XY_RELEASE) if (XY_UNLIKELY(src == nullptr))
            xyu::E_Logic::abort_null_pointer(XY_CurPos(), "src");

        if constexpr (!xyu::t_is_void<T>) if (xyu::t_constant())
        {
            for (xyu::size_t i = 0; i < bytes; ++i)
                if (src[i] == (Tx)value) return i;
            return -1;
        }
        return __::find((const char*)src, value, bytes);
    }
}
