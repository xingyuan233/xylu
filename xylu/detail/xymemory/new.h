#pragma once

/**
 * @file    xymemory/new.h
 * @brief   提供了内存分配、释放功能
 * @date    2025-12-25
 */

#include "./_.h"
#include "../xycore/config.h"
#include "../xycore/error.h"
#include "../xytraits/kind.h"
#include "../xymath/of.h"

// place-new 和 place-delete
#ifndef _NEW
constexpr void* operator new(xyu::size_t, void* p) noexcept { return p; }
constexpr void operator delete(void*, void*) noexcept {}
#endif

/// 日志等级
namespace xylu::xycore
{
#if XY_FIXED_LOG_LEVEL
    constexpr
#endif
    // 内存分配释放日志输出最低等级
    inline N_Log_Level K_Log_Level_New = N_Log_Level::Warn;
}

/// 分配失败处理函数
namespace xylu::xymemory
{
    namespace __ { using func_ht = void(*)() noexcept; }

    /**
     * @brief 获取 alloc 失败后的处理函数指针
     * @details 用于内存分配失败后的处理
     * @note 未设置时会返回一个默认处理函数，即释放部分【线程局部内存池】中未使用的内存
     */
    __::func_ht alloc(xyu::nullptr_t) noexcept XY_RETURNS_NONNULL;

    /**
     * @brief 设置 alloc 失败后的处理函数指针
     * @details 返回旧的失败处理函数指针
     * @note 未设置时会返回一个默认处理函数，即释放部分【线程局部内存池】中未使用的内存
     */
    __::func_ht alloc(__::func_ht func) noexcept XY_RETURNS_NONNULL;
}

/// 线程局部分配函数
namespace xylu::xymemory
{
    namespace __
    {
        [[noreturn]] XY_NOINLINE XY_COLD
        void abort_alloc_failed(xyu::size_t size, xyu::size_t count, xyu::size_t align) noexcept;

        void log_tls_alloc(xyu::size_t size, xyu::size_t count, xyu::size_t align) noexcept;
        XY_ALLOC_SIZE(1) XY_ALLOC_ALIGN(2) XY_MALLOC
        void* tls_alloc(xyu::size_t bytes, xyu::size_t align) noexcept;

        void log_tls_dealloc(void* p, xyu::size_t size, xyu::size_t count, xyu::size_t align) noexcept;
        void tls_dealloc(void* ptr, xyu::size_t bytes, xyu::size_t align) noexcept XY_NONNULL(1);

        void log_tls_prealloc(xyu::size_t size, xyu::size_t align, xyu::size_t count) noexcept;
        bool tls_prealloc(xyu::size_t size, xyu::size_t align, xyu::size_t count) noexcept;

        void log_tls_unalloc(xyu::size_t size, xyu::size_t align) noexcept;
        void tls_unalloc(xyu::size_t size, xyu::size_t align) noexcept;
        void tls_unalloc() noexcept;
    }

    /**
     * @brief 从【线程局部内存池】分配内存
     * @tparam T 期望分配的对象的类型
     * @param count 期望分配的对象的数量，必须 大于 0
     * @param align 内存对齐值，必须是 2 的幂，默认为 alignof(T)
     * @attention 某个线程分配的内存必须由同一线程回收，分配失败返回 nullptr
     */
    template <typename T, typename = xyu::t_enable<!xyu::t_is_void<T>>> XY_RETURNS_NONNULL
    [[nodiscard]] T* alloc(xyu::nullptr_t, xyu::size_t count, xyu::size_t align = alignof(T)) noexcept
    {
        static_assert(sizeof(T) > 0);
        if constexpr (!XY_RELEASE)
        {
            if (XY_UNLIKELY(count == 0)) xyu::E_Logic::abort_invalid_value(XY_CurPos(), 0, "count");
            xyu::calc_of_mul(count, sizeof(T), XY_CurPos());
            if (XY_UNLIKELY(!align || (align & (align - 1))))
                xyu::E_Logic::abort_invalid_value(XY_CurPos(), align, "align");
        }
        if (xyu::N_Log_Level::Debug <= xyu::K_Log_Level_New) __::log_tls_alloc(sizeof(T), count, align);
        return (T*)__::tls_alloc(count * sizeof(T), align);
    }

    /**
     * @brief 从【线程局部内存池】分配内存
     * @tparam T 期望分配的对象的类型
     * @param bytes 期望分配的内存大小，必须 大于 0
     * @param align 内存对齐值，必须是 2 的幂，默认为 K_Default_Align
     * @attention 某个线程分配的内存必须由同一线程回收，分配失败返回 nullptr
     */
    template <typename T, typename = xyu::t_enable<xyu::t_is_void<T>>, typename = void> XY_RETURNS_NONNULL
    [[nodiscard]] void* alloc(xyu::nullptr_t, xyu::size_t bytes, xyu::size_t align = xyu::K_Default_Align) noexcept
    {
        if constexpr (!XY_RELEASE)
        {
            if (XY_UNLIKELY(bytes == 0)) xyu::E_Logic::abort_invalid_value(XY_CurPos(), 0, "bytes");
            if (XY_UNLIKELY(!align || (align & (align - 1))))
                xyu::E_Logic::abort_invalid_value(XY_CurPos(), align, "align");
        }
        if (xyu::N_Log_Level::Debug <= xyu::K_Log_Level_New) __::log_tls_alloc(1, bytes, align);
        return (T*)__::tls_alloc(bytes, align);
    }

    /**
     * @brief 从【线程局部内存池】分配内存
     * @tparam T 期望分配的对象的类型
     * @param count 期望分配的对象的数量，必须 大于 0
     * @param align 内存对齐值，必须是 2 的幂，默认为 alignof(T)
     * @attention 某个线程分配的内存必须由同一线程回收
     */
    template <typename T, typename = xyu::t_enable<!xyu::t_is_void<T>>> XY_RETURNS_NONNULL
    [[nodiscard]] T* alloc(xyu::size_t count, xyu::size_t align = alignof(T)) noexcept
    {
        if (T* ptr = alloc<T>(nullptr, count, align); XY_LIKELY(ptr)) return ptr;
        alloc(nullptr)();
        if (T* ptr = alloc<T>(nullptr, count, align)) return ptr;
        __::abort_alloc_failed(sizeof(T), count, align);
    }

    /**
     * @brief 从【线程局部内存池】分配内存
     * @tparam T 期望分配的对象的类型
     * @param bytes 期望分配的内存大小，必须 大于 0
     * @param align 内存对齐值，必须是 2 的幂，默认为 K_Default_Align
     * @attention 某个线程分配的内存必须由同一线程回收
     */
    template <typename T, typename = xyu::t_enable<xyu::t_is_void<T>>, typename = void> XY_RETURNS_NONNULL
    [[nodiscard]] void* alloc(xyu::size_t bytes, xyu::size_t align = xyu::K_Default_Align) noexcept
    {
        if (T* ptr = alloc<void>(nullptr, bytes, align); XY_LIKELY(ptr)) return ptr;
        alloc(nullptr)();
        if (T* ptr = alloc<void>(nullptr, bytes, align)) return ptr;
        __::abort_alloc_failed(1, bytes, align);
    }

    /**
     * @brief 向【线程局部内存池】释放内存
     * @tparam T 释放对象的类型
     * @param ptr 释放的内存指针，不能为 nullptr
     * @param count 释放的元素数量
     * @param align 内存对齐值，必须是 2 的幂，默认为 alignof(T)
     * @attention ptr 必须为同一线程调用 `alloc˂T˃(count)` 分配的指针
     */
    template <typename T, typename = xyu::t_enable<!xyu::t_is_void<T>>> XY_NONNULL_RELEASE(1)
    void dealloc(T* ptr, xyu::size_t count, xyu::size_t align = alignof(T)) noexcept
    {
        static_assert(sizeof(T) > 0);
        if constexpr (!XY_RELEASE)
        {
            if (XY_UNLIKELY(count == 0)) xyu::E_Logic::abort_invalid_value(XY_CurPos(), 0, "count");
            xyu::calc_of_mul(count, sizeof(T), XY_CurPos());
            if (XY_UNLIKELY(!align || (align & (align - 1))))
                xyu::E_Logic::abort_invalid_value(XY_CurPos(), align, "align");
            if (XY_UNLIKELY(!ptr)) xyu::E_Logic::abort_null_pointer(XY_CurPos(), "ptr");
        }
        if (xyu::N_Log_Level::Debug <= xyu::K_Log_Level_New) __::log_tls_dealloc(ptr, sizeof(T), count, align);
        __::tls_dealloc(ptr, count * sizeof(T), align);
    }

    /**
     * @brief 向【线程局部内存池】释放内存
     * @param ptr 释放的内存指针，不能为 nullptr
     * @param bytes 释放的内存大小，必须 大于 0
     * @param align 内存对齐值，必须是 2 的幂，默认为 K_Default_Align
     * @attention ptr 必须为同一线程调用 `alloc˂T˃(count)` 分配的指针
     */
    template <typename T, typename = xyu::t_enable<xyu::t_is_void<T>>, typename = void> XY_NONNULL_RELEASE(1)
    void dealloc(void* ptr, xyu::size_t bytes, xyu::size_t align = xyu::K_Default_Align) noexcept
    {
        if constexpr (!XY_RELEASE)
        {
            if (XY_UNLIKELY(bytes == 0)) xyu::E_Logic::abort_invalid_value(XY_CurPos(), 0, "bytes");
            if (XY_UNLIKELY(!align || (align & (align - 1))))
                xyu::E_Logic::abort_invalid_value(XY_CurPos(), align, "align");
            if (XY_UNLIKELY(!ptr)) xyu::E_Logic::abort_null_pointer(XY_CurPos(), "ptr");
        }
        if (xyu::N_Log_Level::Debug <= xyu::K_Log_Level_New) __::log_tls_dealloc(ptr, 1, bytes, align);
        __::tls_dealloc(ptr, bytes, align);
    }

    /**
     * @brief 在【线程局部内存池】预分配内存
     * @param size 指定块大小
     * @param align 指定块对齐，必须是 2 的幂
     * @param count 预分配单元数量
     * @note 内存不足或请求大小超过小块返回 false
     */
    inline bool prealloc(xyu::size_t size, xyu::size_t align, xyu::size_t count) noexcept
    {
        if constexpr (!XY_RELEASE)
        {
            if (XY_UNLIKELY(size == 0)) xyu::E_Logic::abort_invalid_value(XY_CurPos(), 0, "size");
            if (XY_UNLIKELY(!align || (align & (align - 1))))
                xyu::E_Logic::abort_invalid_value(XY_CurPos(), align, "align");
        }
        if (xyu::N_Log_Level::Debug <= xyu::K_Log_Level_New) __::log_tls_prealloc(size, align, count);
        return __::tls_prealloc(size, align, count);
    }

    /**
     * @brief 在【线程局部内存池】释放指定大小的未使用内存块
     * @param size 指定块大小
     * @param align 指定块对齐，必须是 2 的幂
     */
    inline void unalloc(xyu::size_t size, xyu::size_t align) noexcept
    {
        if constexpr (!XY_RELEASE)
        {
            if (XY_UNLIKELY(size == 0)) xyu::E_Logic::abort_invalid_value(XY_CurPos(), 0, "size");
            if (XY_UNLIKELY(!align || (align & (align - 1))))
                xyu::E_Logic::abort_invalid_value(XY_CurPos(), align, "align");
        }
        if (xyu::N_Log_Level::Debug <= xyu::K_Log_Level_New) __::log_tls_unalloc(size, align);
        __::tls_unalloc(size, align);
    }

    /**
     * @brief 在【线程局部内存池】释放所有未使用内存块
     */
    inline void unalloc() noexcept { __::tls_unalloc(); }
}

/// 全局分配函数
namespace xylu::xymemory
{
    namespace __
    {
        void log_under_alloc(void* p, xyu::size_t size, xyu::size_t count, xyu::size_t align) noexcept;
        XY_ALLOC_SIZE(1) XY_MALLOC
        void* under_alloc(xyu::size_t bytes) noexcept;
        XY_ALLOC_SIZE(1) XY_ALLOC_ALIGN(2) XY_MALLOC
        void* under_alalloc(xyu::size_t size, xyu::size_t align) noexcept;

        void log_under_dealloc(void* p, xyu::size_t align) noexcept;
        void under_dealloc(void* ptr) noexcept XY_NONNULL(1);
        void under_dealalloc(void* ptr) noexcept XY_NONNULL(1);

        void log_under_realloc(void* p, void* op, xyu::size_t size, xyu::size_t count) noexcept;
        XY_ALLOC_SIZE(2) XY_MALLOC
        void* under_realloc(void* ptr, xyu::size_t bytes) noexcept;
    }

    /**
     * @brief 从【底层分配器】分配内存
     * @param count 期望分配的对象的数量，必须 大于 0
     * @param align 内存对齐值，必须是 2 的幂，默认为 alignof(T)
     * @attention 分配失败返回 nullptr
     */
    template <typename T, typename = xyu::t_enable<!xyu::t_is_void<T>>>
    [[nodiscard]] T* alloc(xyu::native_t, xyu::nullptr_t, xyu::size_t count, xyu::size_t align = alignof(T)) noexcept
    {
        static_assert(sizeof(T) > 0);
        if constexpr (!XY_RELEASE)
        {
            if (XY_UNLIKELY(count == 0)) xyu::E_Logic::abort_invalid_value(XY_CurPos(), 0, "count");
            xyu::calc_of_mul(count, sizeof(T), XY_CurPos());
            if (XY_UNLIKELY(!align || (align & (align - 1))))
                xyu::E_Logic::abort_invalid_value(XY_CurPos(), align, "align");
        }
        T* p;
        if (align <= xyu::K_Default_Align) p = (T*)__::under_alloc(count * sizeof(T));
        else p = (T*)__::under_alalloc(count * sizeof(T), align);
        if (xyu::N_Log_Level::Debug <= xyu::K_Log_Level_New) __::log_under_alloc(p, sizeof(T), count, align);
        return p;
    }

    /**
     * @brief 从【底层分配器】分配内存
     * @param bytes 期望分配的内存大小，必须 大于 0
     * @param align 内存对齐值，必须是 2 的幂，默认为 K_Default_Align
     * @attention 分配失败返回 nullptr
     */
    template <typename T, typename = xyu::t_enable<xyu::t_is_void<T>>, typename = void>
    [[nodiscard]] void* alloc(xyu::native_t, xyu::nullptr_t, xyu::size_t bytes, xyu::size_t align = xyu::K_Default_Align) noexcept
    {
        if constexpr (!XY_RELEASE)
        {
            if (XY_UNLIKELY(bytes == 0)) xyu::E_Logic::abort_invalid_value(XY_CurPos(), 0, "bytes");
            if (XY_UNLIKELY(!align || (align & (align - 1))))
                xyu::E_Logic::abort_invalid_value(XY_CurPos(), align, "align");
        }
        T* p;
        if (align <= xyu::K_Default_Align) p = (T*)__::under_alloc(bytes);
        else p = (T*)__::under_alalloc(bytes, align);
        if (xyu::N_Log_Level::Debug <= xyu::K_Log_Level_New) __::log_under_alloc(p, 1, bytes, align);
        return p;
    }

    /**
     * @brief 从【底层分配器】分配内存
     * @param count 期望分配的对象的数量，必须 大于 0
     * @param align 内存对齐值，必须是 2 的幂，默认为 alignof(T)
     */
    template <typename T, typename = xyu::t_enable<!xyu::t_is_void<T>>> XY_RETURNS_NONNULL
    [[nodiscard]] T* alloc(xyu::native_t, xyu::size_t count, xyu::size_t align = alignof(T)) noexcept
    {
        if (T* ptr = alloc<T>(xyu::native_v, nullptr, count, align); XY_LIKELY(ptr)) return ptr;
        alloc(nullptr)();
        if (T* ptr = alloc<T>(xyu::native_v, nullptr, count, align)) return ptr;
        __::abort_alloc_failed(sizeof(T), count, align);
    }

    /**
     * @brief 从【底层分配器】分配内存
     * @param bytes 期望分配的内存大小，必须 大于 0
     * @param align 内存对齐值，必须是 2 的幂，默认为 K_Default_Align
     */
    template <typename T, typename = xyu::t_enable<xyu::t_is_void<T>>, typename = void> XY_RETURNS_NONNULL
    [[nodiscard]] void* alloc(xyu::native_t, xyu::size_t bytes, xyu::size_t align = xyu::K_Default_Align) noexcept
    {
        if (T* ptr = alloc<void>(xyu::native_v, nullptr, bytes, align); XY_LIKELY(ptr)) return ptr;
        alloc(nullptr)();
        if (T* ptr = alloc<void>(xyu::native_v, nullptr, bytes, align)) return ptr;
        __::abort_alloc_failed(1, bytes, align);
    }

    /**
     * @brief 向【底层分配器】释放内存
     * @param ptr 释放的内存指针，不能为 nullptr
     * @param align 内存对齐值，必须是 2 的幂，默认为 alignof(T)
     */
    template <typename T, typename = xyu::t_enable<!xyu::t_is_void<T>>> XY_NONNULL_RELEASE(2)
    void dealloc(xyu::native_t, T* ptr, xyu::size_t align = alignof(T)) noexcept
    {
        static_assert(sizeof(T) > 0);
        if constexpr (!XY_RELEASE)
        {
            if (XY_UNLIKELY(!align || (align & (align - 1))))
                xyu::E_Logic::abort_invalid_value(XY_CurPos(), align, "align");
            if (XY_UNLIKELY(!ptr)) xyu::E_Logic::abort_null_pointer(XY_CurPos(), "ptr");
        }
        if (xyu::N_Log_Level::Debug <= xyu::K_Log_Level_New) __::log_under_dealloc(ptr, align);
        if (align <= xyu::K_Default_Align) __::under_dealloc(ptr);
        else __::under_dealalloc(ptr);
    }

    /**
     * @brief 向【底层分配器】释放内存
     * @param ptr 释放的内存指针，不能为 nullptr
     * @param align 内存对齐值，必须是 2 的幂，默认为 K_Default_Align
     */
    template <typename T, typename = xyu::t_enable<xyu::t_is_void<T>>, typename = void> XY_NONNULL_RELEASE(2)
    void dealloc(xyu::native_t, void* ptr, xyu::size_t align = xyu::K_Default_Align) noexcept
    {
        if constexpr (!XY_RELEASE)
        {
            if (XY_UNLIKELY(!align || (align & (align - 1))))
                xyu::E_Logic::abort_invalid_value(XY_CurPos(), align, "align");
            if (XY_UNLIKELY(!ptr)) xyu::E_Logic::abort_null_pointer(XY_CurPos(), "ptr");
        }
        if (xyu::N_Log_Level::Debug <= xyu::K_Log_Level_New) __::log_under_dealloc(ptr, align);
        if (align <= xyu::K_Default_Align) __::under_dealloc(ptr);
        else __::under_dealalloc(ptr);
    }

    /**
     * @brief 从【底层分配器】扩容内存
     * @param old_ptr 原内存指针，可以为 nullptr
     * @param count 期望分配的对象的数量，必须 大于 0
     * @note align 必须 <= K_Default_Align
     * @attention 分配失败返回 nullptr
     */
    template <typename T, typename = xyu::t_enable<!xyu::t_is_void<T>>>
    [[nodiscard]] T* realloc(xyu::native_t, xyu::nullptr_t, T* old_ptr, xyu::size_t count) noexcept
    {
        static_assert(sizeof(T) > 0 && alignof(T) <= xyu::K_Default_Align);
        if constexpr (!XY_RELEASE)
        {
            if (XY_UNLIKELY(count == 0)) xyu::E_Logic::abort_invalid_value(XY_CurPos(), 0, "count");
            xyu::calc_of_mul(count, sizeof(T), XY_CurPos());
        }
        T* ptr = (T*)__::under_realloc(old_ptr, count * sizeof(T));
        if (xyu::N_Log_Level::Debug <= xyu::K_Log_Level_New) __::log_under_realloc(ptr, old_ptr, sizeof(T), count);
        return ptr;
    }

    /**
     * @brief 从【底层分配器】扩容内存
     * @param old_ptr 原内存指针，可以为 nullptr
     * @param bytes 期望分配的内存大小，必须 大于 0
     * @note align 必须 <= K_Default_Align
     * @attention 分配失败返回 nullptr
     */
    template <typename T, typename = xyu::t_enable<xyu::t_is_void<T>>, typename = void>
    [[nodiscard]] void* realloc(xyu::native_t, xyu::nullptr_t, void* old_ptr, xyu::size_t bytes) noexcept
    {
        if constexpr (!XY_RELEASE) if (XY_UNLIKELY(bytes == 0)) xyu::E_Logic::abort_invalid_value(XY_CurPos(), 0, "bytes");
        T* ptr = (T*)__::under_realloc(old_ptr, bytes);
        if (xyu::N_Log_Level::Debug <= xyu::K_Log_Level_New) __::log_under_realloc(ptr, old_ptr, 1, bytes);
        return ptr;
    }

    /**
     * @brief 从【底层分配器】扩容内存
     * @param old_ptr 原内存指针，可以为 nullptr
     * @param count 期望分配的对象的数量，必须 大于 0
     * @note align 必须 <= K_Default_Align
     * @attention 分配失败返回 nullptr
     */
    template <typename T, typename = xyu::t_enable<!xyu::t_is_void<T>>> XY_RETURNS_NONNULL
    [[nodiscard]] T* realloc(xyu::native_t, T* old_ptr, xyu::size_t count) noexcept
    {
        if (T* ptr = realloc<T>(xyu::native_v, nullptr, old_ptr, count); XY_LIKELY(ptr)) return ptr;
        alloc(nullptr)();
        if (T* ptr = realloc<T>(xyu::native_v, nullptr, old_ptr, count)) return ptr;
        __::abort_alloc_failed(sizeof(T), count, xyu::K_Default_Align);
    }

    /**
     * @brief 从【底层分配器】扩容内存
     * @param old_ptr 原内存指针，可以为 nullptr
     * @param bytes 期望分配的内存大小，必须 大于 0
     * @note align 必须 <= K_Default_Align
     * @attention 分配失败返回 nullptr
     */
    template <typename T, typename = xyu::t_enable<xyu::t_is_void<T>>, typename = void> XY_RETURNS_NONNULL
    [[nodiscard]] void* realloc(xyu::native_t, void* old_ptr, xyu::size_t bytes) noexcept
    {
        if (T* ptr = realloc<void>(xyu::native_v, nullptr, old_ptr, bytes); XY_LIKELY(ptr)) return ptr;
        alloc(nullptr)();
        if (T* ptr = realloc<void>(xyu::native_v, nullptr, old_ptr, bytes)) return ptr;
        __::abort_alloc_failed(1, bytes, xyu::K_Default_Align);
    }
}
