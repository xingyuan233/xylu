#pragma clang diagnostic push
#pragma ide diagnostic ignored "OCUnusedGlobalDeclarationInspection"
#pragma ide diagnostic ignored "cppcoreguidelines-pro-type-member-init"
#pragma ide diagnostic ignored "modernize-use-nodiscard"
#include "../../head/xymemory/new.h"
#include "../../link/mempool"
#include "../../link/log"

using namespace xylu::xymemory;

// CYGWIN 关于 TLS wrapper function 或 std::thread 之类的实现中
// 会通过 new 来分配内存，然后通过 free 来释放内存，但这并不符合要求，并且会导致程序崩溃
// 因此在 CYGWIN 下，无法重载 new/delete
// 如何得知：让 operator new 返回一个栈空间，你可以选择处理或不处理 delete(因为根本不会被调用)，然后看看程序是怎么崩溃的
// 所以 CYGWIN 下 Called operator new 数量会多于 Called operator delete

// 固定大小栈缓冲区
using S64 = xylu::xycore::__::SString<64>;
using S128 = xylu::xycore::__::SString<128>;

/// 自定义 new/delete 重载

// 普通new/delete
void* operator new(xyu::size_t size)
{
    if (XY_UNLIKELY(size == 0)) size = 1;
    void* p = __::under_alloc(size);
    xylogl(xyu::N_LOG_TRACE, xyu::K_LOG_NEW, xyfmtt(S128{}, "Called operator new: (ptr={}, size={})", p, size));
    return p;
}
void operator delete(void* ptr) noexcept
{
    xylogl(xyu::N_LOG_TRACE, xyu::K_LOG_NEW, xyfmtt(S64{}, "Called operator delete: (ptr={})", ptr));
    __::under_dealloc(ptr);
}
void operator delete(void* ptr, xyu::size_t size) noexcept
{
    xylogl(xyu::N_LOG_TRACE, xyu::K_LOG_NEW, xyfmtt(S128{}, "Called operator delete: (ptr={}, size={})", ptr, size));
    __::under_dealloc(ptr);
}
// 普通new[]/delete[]
void* operator new[](xyu::size_t size)
{
    if (XY_UNLIKELY(size == 0)) size = 1;
    void* p = __::under_alloc(size);
    xylogl(xyu::N_LOG_TRACE, xyu::K_LOG_NEW, xyfmtt(S128{}, "Called operator new[]: (ptr={}, size={})", p, size));
    return p;
}
void operator delete[](void* ptr) noexcept
{
    xylogl(xyu::N_LOG_TRACE, xyu::K_LOG_NEW, xyfmtt(S64{}, "Called operator delete[]: (ptr={})", ptr));
    __::under_dealloc(ptr);
}
void operator delete[](void* ptr, xyu::size_t size) noexcept
{
    xylogl(xyu::N_LOG_TRACE, xyu::K_LOG_NEW, xyfmtt(S128{}, "Called operator delete[]: (ptr={}, size={})", ptr, size));
    __::under_dealloc(ptr);
}

// 自定义 线程池 alloc/dealloc 辅助函数
namespace xylu::xymemory
{
    /// 线程局部全局内存池
    auto& get_pool()
    {
#if !XY_UNTHREAD
        thread_local
#endif
        static xyu::MemPool_Block pool;
        return pool;
    }

    void* alloc(xyu::size_t bytes, xyu::size_t align)
    {
        xylogl(xyu::N_LOG_TRACE, xyu::K_LOG_NEW, xyfmtt(S128{}, "Called alloc: (size={}, align={})", bytes, align));
        return get_pool().allocate(bytes, align);;
    }

    void dealloc(void* ptr, xyu::size_t bytes, xyu::size_t align) noexcept
    {
        xylogl(xyu::N_LOG_TRACE, xyu::K_LOG_NEW, xyfmtt(S128{}, "Called dealloc: (ptr={}, size={}, align={})", ptr, bytes, align));
        get_pool().deallocate(ptr, bytes, align);
    }

   namespace __
   {
       void pool_release() noexcept { get_pool().release(); }
   }
}


// 自定义 底层 alloc/dealloc 辅助函数
namespace xylu::xymemory
{
    void* alloc(xyu::native_t, xyu::size_t bytes, xyu::size_t align)
    {
        void* p = __::under_alloc_align(bytes, align);
        xylogl(xyu::N_LOG_TRACE, xyu::K_LOG_NEW, xyfmtt(S128{}, "Called under alloc: (ptr={}, size={}, align={})", p, bytes, align));
        return p;
    }

    void dealloc(xyu::native_t, void* ptr) noexcept
    {
        xylogl(xyu::N_LOG_TRACE, xyu::K_LOG_NEW, xyfmtt(S64{}, "Called under dealloc: (ptr={})", ptr));
        __::under_dealloc_align(ptr);
    }
}

#pragma clang diagnostic pop