#pragma once

/**
 * @file    xymemory/pool.h
 * @brief   各种内存池
 * @date    2025-12-25
 */

#include "./_.h"
#include "../xycore/config.h"
#include "../xycore/error.h"
#include "../xytraits/kind.h"
#include "../xymath/of.h"

/// 分块内存池
namespace xylu::xymemory
{
    /**
     * @brief 一个高性能、基于分块策略的内存池
     * @details
     *   `MemPool_BC` 将内存请求分为 小块 和 大块 两种策略进行管理：
     *
     *   - 小块内存: 对于不超过 `cell_max_size` 的请求，
     *     内存池会从大小合适的 ChunkGourp[小块组] 中最近的 Chunk[小块] 中返回一个 cell[单元]。
     *     可以节省与系统的交互次数，速度较快。
     *
     *   - 大块内存: 对于超过 `cell_max_size` 的请求，
     *     直接调用底层分配器并返回一个 Block[大块]，并在 `dealloc` 时直接归还给底层系统。
     *
     *   - cell_max_szie 默认为 4096
     */
    struct MemPool_BC : xyu::class_no_copy_t
    {
        // 块属性配置
        struct Option
        {
            // 块最小的大小
            xyu::size_t chunk_min_size;
            // 块最少的单元数量
            xyu::size_t chunk_min_cells;
            // 块最大的单元数量
            xyu::size_t chunk_max_cells;
            // 块扩容因子
            float grow_factor;

            constexpr Option() noexcept : grow_factor{2.0f}, chunk_min_size{1024},
                                          chunk_min_cells{8}, chunk_max_cells{1024 * 1024 * 4} {}
        };

    private:
        // 大块信息
        alignas(xyu::K_Default_Align) char block[48];
        // 小块信息
        alignas(xyu::K_Default_Align) char chunks[1152];
        // 块配置
        Option op;
        // 是否初始化
        bool is_inited = false;

    public:
        /**
         * @brief 构造并初始化一个内存池
         * @param option 内存池的配置选项
         */
        explicit MemPool_BC(Option option = Option()) noexcept { create(option); }

        /// 延迟构造
        explicit MemPool_BC(xyu::native_t) noexcept {}

        /**
         * @brief 移动构造函数
         * @details
         *   将 other 内存池的所有权（包括已分配的内存和内部状态）转移到当前对象；
         *   转移后，other 会进入未初始化状态，若要继续使用必须重新调用 `create`
         */
        MemPool_BC(MemPool_BC&& other) noexcept;

        /// 析构函数，释放所有持有的内存
        ~MemPool_BC() noexcept { release(); }

        /// 检查内存池是否已被初始化
        explicit operator bool() const noexcept { return is_inited; }

        /// 获取当前内存池的配置选项
        Option option() const noexcept { return op; }

        /* 创建与销毁 */

        /**
         * @brief 使用指定的配置选项初始化内存池
         * @details 此函数设置内存池的内部状态，它会自动检查并修正不合理的配置值
         * @attention 一个内存池在使用前必须被初始化，如果需要重新初始化必须先调用 `release` 释放旧的状态
         */
        void create(Option option) noexcept;

        /**
         * @brief 释放内存池持有的所有内存，并重置其状态
         * @details 无条件地归还内存池分配出去的所有内存块
         */
        void release() noexcept;

        /**
         * @brief 从内存池中分配内存
         * @tparam T 期望分配的对象的类型
         * @param count 元素数量，必须 大于 0
         * @param align 内存对齐值，必须是 2 的幂，默认为 alignof(T)
         * @attention 分配失败返回 nullptr
         */
        template <typename T, typename = xyu::t_enable<!xyu::t_is_void<T>>>
        [[nodiscard]] T* alloc(xyu::size_t count, xyu::size_t align = alignof(T)) noexcept
        {
            static_assert(sizeof(T) > 0);
            if constexpr (!XY_RELEASE)
            {
                if (XY_UNLIKELY(count == 0)) xyu::E_Logic::abort_invalid_value(XY_CurPos("MemPool_BC::alloc"), 0, "count");
                xyu::calc_of_mul(count, sizeof(T), XY_CurPos("MemPool_BC::alloc"));
                if (XY_UNLIKELY(!align || (align & (align - 1))))
                    xyu::E_Logic::abort_invalid_value(XY_CurPos("MemPool_BC::alloc"), align, "align");
                xyu::calc_of_add(count, align, XY_CurPos("MemPool_BC::alloc"));
            }
            return static_cast<T*>(alloc_helper(count * sizeof(T), align));
        }

        /**
         * @brief 从内存池中分配内存
         * @param bytes 分配内存大小，必须 大于 0
         * @param align 内存对齐值，必须是 2 的幂，默认为 K_Default_Align
         * @attention 分配失败返回 nullptr
         */
        template <typename T, typename = xyu::t_enable<xyu::t_is_void<T>>, typename = void>
        [[nodiscard]] void* alloc(xyu::size_t bytes, xyu::size_t align = xyu::K_Default_Align) noexcept
        {
            static_assert(sizeof(T) > 0);
            if constexpr (!XY_RELEASE)
            {
                if (XY_UNLIKELY(bytes == 0)) xyu::E_Logic::abort_invalid_value(XY_CurPos("MemPool_BC::alloc"), 0, "bytes");
                if (XY_UNLIKELY(!align || (align & (align - 1))))
                    xyu::E_Logic::abort_invalid_value(XY_CurPos("MemPool_BC::alloc"), align, "align");
                xyu::calc_of_add(bytes, align, XY_CurPos("MemPool_BC::alloc"));
            }
            return alloc_helper(bytes, align);
        }

        /**
         * @brief 归还内存到内存池
         * @tparam T 释放对象的类型
         * @param ptr 释放的内存指针，不能为 nullptr
         * @param count 元素数量，必须 大于 0
         * @param align 内存对齐值，必须是 2 的幂，默认为 alignof(T)
         */
        template <typename T, typename = xyu::t_enable<!xyu::t_is_void<T>>> XY_NONNULL_RELEASE(2)
        void dealloc(T* ptr, xyu::size_t count, xyu::size_t align = alignof(T)) noexcept
        {
            static_assert(sizeof(T) > 0);
            if constexpr (!XY_RELEASE)
            {
                if (XY_UNLIKELY(count == 0)) xyu::E_Logic::abort_invalid_value(XY_CurPos("MemPool_BC::dealloc"), 0, "count");
                xyu::calc_of_mul(count, sizeof(T), XY_CurPos("MemPool_BC::dealloc"));
                if (XY_UNLIKELY(!align || (align & (align - 1))))
                    xyu::E_Logic::abort_invalid_value(XY_CurPos("MemPool_BC::dealloc"), align, "align");
                xyu::calc_of_add(count, align, XY_CurPos("MemPool_BC::dealloc"));
                if (XY_UNLIKELY(!ptr)) xyu::E_Logic::abort_null_pointer(XY_CurPos("MemPool_BC::dealloc"), "ptr");
            }
            dealloc_helper(ptr, count * sizeof(T), align);
        }

        /**
         * @brief 归还内存到内存池
         * @param ptr 释放的内存指针，不能为 nullptr
         * @param bytes 内存大小，必须 大于 0
         * @param align 内存对齐值，必须是 2 的幂，默认为 K_Default_Align
         */
        template <typename T, typename = xyu::t_enable<xyu::t_is_void<T>>, typename = void> XY_NONNULL_RELEASE(2)
        void dealloc(void* ptr, xyu::size_t bytes, xyu::size_t align = xyu::K_Default_Align) noexcept
        {
            static_assert(sizeof(T) > 0);
            if constexpr (!XY_RELEASE)
            {
                if (XY_UNLIKELY(bytes == 0)) xyu::E_Logic::abort_invalid_value(XY_CurPos("MemPool_BC::dealloc"), 0, "bytes");
                if (XY_UNLIKELY(!align || (align & (align - 1))))
                    xyu::E_Logic::abort_invalid_value(XY_CurPos("MemPool_BC::dealloc"), align, "align");
                xyu::calc_of_add(bytes, align, XY_CurPos("MemPool_BC::dealloc"));
                if (XY_UNLIKELY(!ptr)) xyu::E_Logic::abort_null_pointer(XY_CurPos("MemPool_BC::dealloc"), "ptr");
            }
            dealloc_helper(ptr, bytes, align);
        }

        /**
         * @brief 预分配指定大小的小块内存
         * @param size 指定块大小
         * @param align 指定块对齐，必须是 2 的幂
         * @param count 预分配单元数量
         */
        bool prealloc(xyu::size_t size, xyu::size_t align, xyu::size_t count) noexcept
        {
            if constexpr (!XY_RELEASE)
            {
                if (XY_UNLIKELY(size == 0)) xyu::E_Logic::abort_invalid_value(XY_CurPos("MemPool_BC::prealloc"), 0, "size");
                if (XY_UNLIKELY(!align || (align & (align - 1))))
                    xyu::E_Logic::abort_invalid_value(XY_CurPos("MemPool_BC::prealloc"), align, "align");
                xyu::calc_of_add(size, align, XY_CurPos("MemPool_BC::prealloc"));
            }
            if (XY_UNLIKELY(!count)) return false;
            return prealloc_helper((size + align - 1) & -align, count);
        }

        /**
         * @brief 释放内存池中指定大小的未使用内存小块
         * @param size 指定块大小
         * @param align 指定块对齐，必须是 2 的幂
         * @details 释放该对象会占用的空闲小块内存
         * @note 一个块中只要有一个单元仍在使用就无法释放
         */
        void unalloc(xyu::size_t size, xyu::size_t align) noexcept
        {
            if constexpr (!XY_RELEASE)
            {
            if (XY_UNLIKELY(size == 0)) xyu::E_Logic::abort_invalid_value(XY_CurPos("MemPool_BC::unalloc"), 0, "size");
                if (XY_UNLIKELY(!align || (align & (align - 1))))
                    xyu::E_Logic::abort_invalid_value(XY_CurPos("MemPool_BC::unalloc"), align, "align");
                xyu::calc_of_add(size, align, XY_CurPos("MemPool_BC::unalloc"));
            }
            unalloc_helper((size + align - 1) & -align);
        }

        /**
         * @brief 释放内存池中未使用的小块内存
         * @note 一个块中只要有一个单元仍在使用就无法释放
         */
        void unalloc() noexcept;

    private:
        void* XY_ALLOC_SIZE(2) XY_ALLOC_ALIGN(3) XY_MALLOC
        alloc_helper(xyu::size_t bytes, xyu::size_t align) noexcept;
        void dealloc_helper(void* ptr, xyu::size_t bytes, xyu::size_t align) noexcept XY_NONNULL_RELEASE(2);
        bool prealloc_helper(xyu::size_t cell_size, xyu::size_t count) noexcept;
        void unalloc_helper(xyu::size_t cell_size) noexcept;
    };
}
