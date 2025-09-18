#pragma clang diagnostic push
#pragma ide diagnostic ignored "OCUnusedGlobalDeclarationInspection"
#pragma once

#include "../../link/attr"
#include "../../link/config"
#include "../../link/traits"

/**
 * @file
 * @brief 提供了底层的内存分配、释放功能，并重载了全局的 new 和 delete 操作符。
 * @details
 *   本文件定义了内存分配失败时的处理机制，重载了全局的 `operator new` 和 `delete`
 *   以接管系统的内存分配，并提供了两套内存分配接口：
 *   1. `xymemory::alloc/dealloc`: 基于线程局部内存池的高效版本。
 *   2. `xymemory::alloc/dealloc(xyu::native_t, ...)`: 直接调用底层分配器的原生版本。
 */

// 内存分配日志信息等级限制
namespace xylu::xycore
{
    constexpr N_LOG_LEVEL K_LOG_NEW = N_LOG_WARN;
}

/* 内存分配失败处理 */
namespace xylu::xymemory
{
    /// 定义了内存分配失败时将被调用的处理函数的类型。
    using mem_new_handler_t = void(*)();

    /**
     * @brief 设置全局的内存分配失败处理函数。
     * @details 当内存分配失败时，系统会反复调用此处理函数，直到：
     *          - 某次调用后内存分配成功。
     *          - 处理函数被设为 nullptr (此时会抛出异常)。
     *          - 处理函数内部抛出异常。
     * @param handler 新的处理函数指针，可以为 nullptr。
     * @return 返回先前设置的处理函数。
     * @note 此函数是线程安全的，且与 std 指向同一个处理函数。
     */
    mem_new_handler_t mem_new_handler_set(mem_new_handler_t);

    /**
     * @brief 获取当前设置的全局内存分配失败处理函数。
     * @return 返回当前的处理函数指针。
     * @note 此函数是线程安全的，且与 std 指向同一个处理函数。
     */
    mem_new_handler_t mem_new_handler_get();
}

/* new 和 delete 重载 */
// 普通 new 和 delete
[[nodiscard]] XY_ALLOC_SIZE(1) XY_MALLOC XY_RETURNS_NONNULL
void* operator new(xyu::size_t size);
void operator delete(void*) noexcept;
void operator delete(void*, xyu::size_t size) noexcept;
// 普通 new[] 和 delete[]
[[nodiscard]] XY_ALLOC_SIZE(1) XY_MALLOC XY_RETURNS_NONNULL
void* operator new[](xyu::size_t size);
void operator delete[](void*, xyu::size_t size) noexcept;
// 无异常 new 和 delete
[[nodiscard]] XY_ALLOC_SIZE(1) XY_MALLOC
inline void* operator new(xyu::size_t size, xyu::nothrow_t) noexcept { try { return ::operator new(size); } catch (...) { return nullptr; } }
inline void operator delete(void* p, xyu::nothrow_t) noexcept { ::operator delete(p); }
inline void operator delete(void* p, xyu::size_t bytes, xyu::nothrow_t) noexcept { ::operator delete(p, bytes); }
// 无异常 new[] 和 delete[]
[[nodiscard]] XY_ALLOC_SIZE(1) XY_MALLOC
inline void* operator new[](xyu::size_t size, xyu::nothrow_t) noexcept { try { return ::operator new[](size); } catch (...) { return nullptr; } }
inline void operator delete[](void* p, xyu::nothrow_t) noexcept { ::operator delete[](p); }
inline void operator delete[](void* p, xyu::size_t bytes, xyu::nothrow_t) noexcept { ::operator delete[](p, bytes); }
// place-new 和 place-delete
#ifndef _NEW
constexpr void* operator new(xyu::size_t, void* p) noexcept { return p; }
constexpr void operator delete(void*, void*) noexcept {}
#endif

/* 内存分配与释放 */
namespace xylu::xymemory
{
    /**
     * @brief 提供两套内存分配接口：线程池版本和原生版本。
     *
     * ### 线程池版本 (`alloc`/`dealloc`)
     * - **高性能**: 通过 `thread_local` 内存池为小块内存提供高效的分配和释放。
     * - **线程安全**: 每个线程拥有独立的内存池，分配操作无锁。
     * - **自动回收**: 线程结束时，其内存池中所有剩余内存会被自动释放。
     * - **注意**: 为保证性能和管理的一致性，强烈建议在哪个线程分配的内存，就在哪个线程释放。
     *          跨线程释放会导致该内存块的管理被推迟，直到分配线程结束时才被回收。
     *
     * ### 原生版本 (`alloc`/`dealloc` with `xyu::native_t`)
     * - **跨线程安全**: 直接调用底层内存分配器，没有线程归属，允许在一个线程中分配，在另一个线程中释放。
     * - **适用场景**: 用于需要跨线程传递所有权的内存，或不希望受内存池生命周期管理的场景。
     */
    namespace __
    {
        /**
         * @brief 分配内存
         * @note 底层实现，不依赖于其他
         * @note 确保 bytes > 0，否则行为未定义
         */
        XY_ALLOC_SIZE(1) XY_RETURNS_NONNULL XY_MALLOC
        void* under_alloc(xyu::size_t bytes);

        /**
         * @brief 释放内存
         * @note 底层实现，不依赖于其他
         * @note 自动存储其他信息，不需要提供参数
         */
        void under_dealloc(void* ptr) noexcept;

        /**
         * @brief 分配内存
         * @note 底层实现，不依赖于其他
         * @note 确保 bytes > 0，否则行为未定义
         */
        XY_ALLOC_SIZE(1) XY_ALLOC_ALIGN(2) XY_RETURNS_NONNULL XY_MALLOC
        void* under_alloc_align(xyu::size_t bytes, xyu::size_t align);

        /**
         * @brief 释放内存
         * @note 底层实现，不依赖于其他
         * @note 自动存储其他信息，不需要提供参数
         */
        void under_dealloc_align(void* ptr) noexcept;

        /**
         * @brief 释放线程全局内存池，以记录日志
         */
        void pool_release() noexcept;
    }

    /**
     * @brief 从【线程局部内存池】分配指定字节和对齐方式的内存。
     * @param bytes 要分配的字节数。若为 0，则实际分配 1 字节。
     * @param align 内存对齐值，必须是 2 的幂，默认为 xylu::K_DEFAULT_ALIGN。
     * @return 返回一个非空的内存块指针。
     * @throws E_Memory_Alloc 分配内存失败。
     * @throws E_Memory_Align align 不是 2 的幂。
     */
    [[nodiscard]] void* alloc(xyu::size_t bytes, xyu::size_t align = xyu::K_DEFAULT_ALIGN);

    /**
     * @brief 从【线程局部内存池】分配 count 个 T 类型对象的内存。
     * @tparam T 要分配的对象的类型。
     * @param count 要分配的对象的数量。若为 0，则分配 1 个。
     * @param align 内存对齐值，必须是 2 的幂，默认为 alignof(T)。
     * @return 返回一个 T* 类型的非空指针。
     * @throws E_Memory_Alloc 分配内存失败。
     * @exception E_Memory_Align align 不是2的幂。
     */
    template <typename T, typename = xyu::t_enable<!xyu::t_is_void<T>>>
    [[nodiscard]] T* alloc(xyu::size_t count, xyu::size_t align = alignof(T))
    { return static_cast<T*>(alloc(count * sizeof(T), align)); }

    /**
     * @brief 向【线程局部内存池】释放 ptr 指向的内存。
     * @param ptr   通过 `alloc` 分配的指针，可以为 nullptr。
     * @param bytes 分配的字节数，必须与 `alloc` 时一致。
     * @param align 内存对齐值，默认为 xylu::K_DEFAULT_ALIGN，必须与 `alloc` 时一致。
     */
    void dealloc(void* ptr, xyu::size_t bytes, xyu::size_t align = xyu::K_DEFAULT_ALIGN) noexcept;

    /**
     * @brief 向【线程局部内存池】释放 ptr 指向的 T 类型对象数组的内存。
     * @tparam T   对象的类型。
     * @param ptr   通过 `alloc<T>` 分配的指针，可以为 nullptr。
     * @param count 要释放的对象数量，必须与 `alloc<T>` 时一致。
     * @param align 内存对齐值，默认为 alignof(T)，必须与 `alloc<T>` 时一致。
     */
    template <typename T, typename = xyu::t_enable<!xyu::t_is_void<T>>>
    void dealloc(T* ptr, xyu::size_t count, xyu::size_t align = alignof(T)) noexcept
    { dealloc(static_cast<void*>(ptr), count * sizeof(T), align); }


    /**
     * @brief 直接从【底层分配器】分配指定字节和对齐方式的内存（原生版本）。
     * @param tag   `xyu::native_t` 类型的标签，用于分派到此原生版本。
     * @param bytes 要分配的字节数。若为 0，则实际分配 1 字节。
     * @param align 内存对齐值，必须是 2 的幂，默认为 xylu::K_DEFAULT_ALIGN。
     * @return 返回一个非空的内存块指针。
     * @throws E_Memory_Alloc 分配内存失败。
     * @throws E_Memory_Align align 不是 2 的幂。
     */
    [[nodiscard]] void* alloc(xyu::native_t, xyu::size_t bytes, xyu::size_t align = xyu::K_DEFAULT_ALIGN);

    /**
     * @brief 直接从【底层分配器】分配 count 个 T 类型对象的内存（原生版本）。
     * @tparam T 要分配的对象的类型。
     * @param tag   `xyu::native_t` 类型的标签，用于分派到此原生版本。
     * @param count 要分配的对象的数量。若为 0，则分配 1 个。
     * @param align 内存对齐值，必须是 2 的幂，默认为 alignof(T)。
     * @return 返回一个 T* 类型的非空指针。
     * @throws E_Memory_Alloc 分配内存失败。
     * @throws E_Memory_Align align 不是 2 的幂。
     */
    template <typename T, typename = xyu::t_enable<!xyu::t_is_void<T>>>
    [[nodiscard]] T* alloc(xyu::native_t, xyu::size_t count, xyu::size_t align = alignof(T))
    { return static_cast<T*>(alloc(xyu::native_v, count * sizeof(T), align)); }

    /**
     * @brief 直接向【底层分配器】释放 ptr 指向的内存（原生版本）。
     * @param tag   `xyu::native_t` 类型的标签，用于分派到此原生版本。
     * @param ptr   通过 `alloc` 分配的指针，可以为 nullptr。
     * @param bytes 分配的字节数，必须与 `alloc` 时一致。
     * @param align 内存对齐值，默认为 xylu::K_DEFAULT_ALIGN，必须与 `alloc` 时一致。
     */
    void dealloc(xyu::native_t, void* ptr) noexcept;

    /**
     * @brief 直接向【底层分配器】释放 ptr 指向的 T 类型对象数组的内存（原生版本）。
     * @tparam T   对象的类型。
     * @param tag   `xyu::native_t` 类型的标签，用于分派到此原生版本。
     * @param ptr   通过 `alloc<T>` 分配的指针，可以为 nullptr。
     * @param count 要释放的对象数量，必须与 `alloc<T>` 时一致。
     * @param align 内存对齐值，默认为 alignof(T)，必须与 `alloc<T>` 时一致。
     */
    template <typename T>
    void dealloc(xyu::native_t, T* ptr, xyu::size_t count [[maybe_unused]] = {}, xyu::size_t align [[maybe_unused]] = {}) noexcept
    { dealloc(xyu::native_v, static_cast<void*>(ptr)); }


    /**
     * @brief 分配内存，失败时返回 nullptr 而不抛出异常。
     * @details 这是 `alloc` 函数的 nothrow 版本。
     * @param tag   `xyu::nothrow_t` 类型的标签。
     * @param args  传递给 `alloc` 的原始参数。
     * @return 成功时返回内存指针，失败时返回 nullptr。
     */
    template <typename... Args>
    [[nodiscard]] auto alloc(xyu::nothrow_t, Args&&... args) noexcept
    {
        try { return alloc(xyu::forward<Args>(args)...); }
        catch (...) { return nullptr; }
    }

    /**
     * @brief 释放内存（nothrow 版本）。
     * @details 功能上与非 nothrow 版本的 `dealloc` 完全相同，因为 `dealloc` 本身就是 noexcept 的。
     */
    template <typename... Args>
    void dealloc(xyu::nothrow_t, Args&&... args) noexcept
    { dealloc(xyu::forward<Args>(args)...); }
}

#pragma clang diagnostic pop