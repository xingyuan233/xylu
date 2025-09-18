#pragma clang diagnostic push
#pragma ide diagnostic ignored "OCUnusedGlobalDeclarationInspection"
#pragma ide diagnostic ignored "modernize-use-nodiscard"
#pragma ide diagnostic ignored "cppcoreguidelines-pro-type-member-init"
#pragma once

#include "../../link/config"
#include "../../link/memfun"

/* 内存池 */

// 内存池日志信息等级限制
namespace xylu::xycore
{
    constexpr N_LOG_LEVEL K_LOG_MEMPOOL = N_LOG_WARN;
}

/** 分块内存池 */
namespace xylu::xymemory
{
    /**
     * @brief 一个高性能、基于分块策略的内存池。
     * @details
     *   `MemPool_Block` 将内存请求分为“小块”和“大块”两种策略进行管理：
     *   - **小块内存**: 对于不超过 `cell_max_size` 的请求，内存池会从一次性分配的、
     *     由固定大小单元组成的内存块（chunk）中进行分配。这种方式可以节省与系统的交互次数，速度较快。
     *     这些内存块由内存池统一管理，并在 `release()` 时一次性释放。
     *   - **大块内存**: 对于超过 `cell_max_size` 的请求，内存池会直接调用底层分配器来满足。
     *     它仅记录这些分配，并在 `deallocate` 时直接归还给底层系统。
     *
     *   这种设计兼顾了小块内存的分配效率和对大块内存的灵活性。
     */
    struct MemPool_Block : xyu::class_no_copy_t
    {
        // 块属性配置
        struct Option
        {
            xyu::size_t chunk_min_size;     ///< 块最小大小
            xyu::size_t chunk_min_cells;    ///< 块最小单元数量
            xyu::size_t chunk_max_cells;    ///< 块最大单元数量
            xyu::size_t cell_max_size;      ///< 单元最大大小 (超过后单独分配大块内存)
            float grow_factor;              ///< 块扩容因子

            constexpr Option() noexcept : grow_factor{2.0f}, chunk_min_size{1024},
            chunk_min_cells{8}, chunk_max_cells{1024 * 1024}, cell_max_size{4096} {}
        };
    private:
        Option op;
        xyu::size_t chunk_count = 0;                         ///< 块数量
        alignas(alignof(xyu::uint8)) xyu::uint8 block[40];   ///< 内存大块信息
        alignas(alignof(xyu::uint8)) xyu::uint8 chunks[1280];///< 内存小块信息

    public:
        /**
         * @brief 构造并初始化一个内存池。
         * @param option 内存池的配置选项。若省略，则使用默认配置。
         * @note 构造过程不会发生动态内存分配。
         */
        explicit MemPool_Block(Option option = Option()) noexcept { init(option); }

        /**
         * @brief 移动构造函数。
         * @details 将 other 内存池的所有权（包括已分配的内存和内部状态）转移到当前对象。
         *          转移后，other 会进入未初始化状态，若要继续使用必须重新调用 `init()`。
         * @param other 要被移动的内存池。
         */
        MemPool_Block(MemPool_Block&& other) noexcept : chunk_count(other.chunk_count)
        {
            if (XY_UNLIKELY(this == &other)) return;
            xyu::mem_copy(block, other.block, sizeof(block));
            xyu::mem_copy(chunks, other.chunks, sizeof(chunks));
            other.chunk_count = 0;
        }

        /// 析构函数，调用 `release()` 来释放所有持有的内存。
        ~MemPool_Block() noexcept { release(); }

        /**
         * @brief 使用指定的配置选项初始化内存池。
         * @details
         *   此函数设置内存池的内部状态。一个内存池在使用前必须被初始化。
         *   它会自动检查并修正不合理的配置值。
         * @param option 内存池的配置选项。
         * @note 如果内存池已经被初始化，则此函数不执行任何操作。
         *       必须先调用 `release()` 才能重新初始化。
         */
        void init(Option option) noexcept;

        /**
         * @brief 释放内存池持有的所有内存，并重置其状态。
         * @warning
         *   这是一个危险操作！它会无条件地归还内存池分配出去的所有内存块。
         *   调用前，请务必确保所有通过此内存池分配的内存都不再被使用。
         */
        void release() noexcept;

        /// 检查内存池是否已被初始化。
        explicit operator bool() const noexcept { return chunk_count > 0; }

        /**
         * @brief 从内存池中分配指定字节和对齐的内存。
         * @param bytes 要分配的字节数。若为 0，则实际分配 1 字节。
         * @param align 内存对齐值，必须是 2 的幂，默认为 K_DEFAULT_ALIGN。
         * @return 返回一个非空的内存块指针。
         * @attention 在调用前，必须确保内存池已被初始化。
         * @throws E_Memory_Alloc 分配内存失败。
         * @throws E_Memory_Align align 不是 2 的幂。
         */
        [[nodiscard]] void* allocate(xyu::size_t bytes, xyu::size_t align = xyu::K_DEFAULT_ALIGN)
        {
            if (XY_UNLIKELY(bytes == 0)) bytes = 1;
            return alloc_helper(bytes, align);
        }

        /**
         * @brief 从内存池中分配 count 个 T类型 的内存。
         * @param count 元素数量。若为 0，则实际分配 1 个元素。
         * @param align 内存对齐值，必须是 2 的幂，默认为 alignof(T)。
         * @return 返回一个 T* 类型的非空指针。
         * @attention 在调用前，必须确保内存池已被初始化。
         * @exception E_Memory_Alloc 分配内存失败。
         * @exception E_Memory_Align align 不是2的幂。
         */
        template <typename T, typename = xyu::t_enable<!xyu::t_is_void<T>>>
        [[nodiscard]] T* allocate(xyu::size_t count, xyu::size_t align = alignof(T))
        {
            if (XY_UNLIKELY(count == 0)) count = 1;
            return static_cast<T*>(alloc_helper(count * sizeof(T), align));
        }

        /**
         * @brief 归还 ptr 指向的内存到内存池。
         * @param ptr   通过此内存池的 `allocate` 分配的指针，可以为 nullptr。
         * @param bytes 释放的字节数，必须与分配时的大小匹配。
         * @param align 内存对齐值，必须与分配时一致。
         * @attention 在调用前，必须确保内存池已被初始化。
         */
        void deallocate(void* ptr, xyu::size_t bytes, xyu::size_t align = xyu::K_DEFAULT_ALIGN) XY_NOEXCEPT_NDEBUG
        {
            if (XY_UNLIKELY(ptr == nullptr)) return;
            dealloc_helper(ptr, bytes, align);
        }

        /**
         * @brief 释放 ptr 指向的 T 类型对象数组的内存。
         * @param ptr 通过此内存池的 `allocate<T>` 分配的指针，可以为 nullptr。
         * @param count 元素数量，必须与分配时一致。
         * @param align 内存对齐值，必须与分配时一致。
         * @attention 在调用前，必须确保内存池已被初始化。
         */
        template <typename T, typename = xyu::t_enable<!xyu::t_is_void<T>>>
        void deallocate(T* ptr, xyu::size_t count, xyu::size_t align = alignof(T)) XY_NOEXCEPT_NDEBUG
        {
            if (XY_UNLIKELY(ptr == nullptr)) return;
            dealloc_helper(ptr, count * sizeof(T), align);
        }

        /// 获取当前内存池的配置选项。
        Option option() const noexcept { return op; }

    private:
        void* alloc_helper(xyu::size_t bytes, xyu::size_t align);
        void dealloc_helper(void* ptr, xyu::size_t bytes, xyu::size_t align) XY_NOEXCEPT_NDEBUG;
    };
}

#pragma clang diagnostic pop