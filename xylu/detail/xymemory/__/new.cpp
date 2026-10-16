#include <cstdlib>
#include <new>
#include "../new.h"

#include "../pool.h"
#include "../../xystring/fmt.h"

#if !XY_OUTPUT_ERROR_LOG
#  undef xylog
#  define xylog(...) 0
#endif

#if !_GLIBCXX_HAVE_ALIGNED_ALLOC && !_GLIBCXX_HAVE__ALIGNED_MALLOC \
    && !_GLIBCXX_HAVE_POSIX_MEMALIGN && _GLIBCXX_HAVE_MEMALIGN
# if _GLIBCXX_HOSTED && __has_include(<malloc.h>)
// 如果C库在<malloc.h>中声明了memalign，一些C库会这样做
# include <malloc.h>
# else
extern "C" void *memalign(std::size_t boundary, std::size_t size);
# endif
#endif

namespace __gnu_cxx
{
// 如果可用，优先选择posix_memalign，因为它比aligned_alloc更古老，
// 因此更有可能由取代malloc库的库提供，这些库在添加aligned_alloc之前就存在了。参见PR libstdc++/113258。
#if _GLIBCXX_HAVE_POSIX_MEMALIGN
    static void* aligned_alloc(std::size_t al, std::size_t sz)
    {
        void *ptr;
        // posix_memalign有额外的要求，而在aligned_alloc中不存在：
        // 对齐值必须是sizeof (void *)的2的幂倍数。
        if (al < sizeof(void*)) al = sizeof(void*);
        int ret = posix_memalign(&ptr, al, sz);
        if (ret == 0) return ptr;
        return nullptr;
    }
#elif _GLIBCXX_HAVE_ALIGNED_ALLOC
    using ::aligned_alloc;
#elif _GLIBCXX_HAVE__ALIGNED_MALLOC
    static void* aligned_alloc (std::size_t al, std::size_t sz)
    { return _aligned_malloc(sz, al); }
#elif _GLIBCXX_HAVE_MEMALIGN
    static void* aligned_alloc (std::size_t al, std::size_t sz)
    {
      // Solaris要求al大于等于word的大小，QNX要求大于等于sizeof (void*)，
      // 但它们都提供了posix_memalign，因此将使用上面的定义。
      return memalign(al, sz);
    }
#else // !HAVE__ALIGNED_MALLOC && !HAVE_POSIX_MEMALIGN && !HAVE_MEMALIGN
// C库不提供任何对齐分配函数，定义一个。
// 这是gcc/config/i386/gmm_malloc.h中代码的一个修改版本。
    static void* aligned_alloc (std::size_t al, std::size_t sz)
    {
        // 我们需要额外的字节来存储malloc返回的原始值。
        if (al < sizeof(void*)) al = sizeof(void*);
        void* const malloc_ptr = malloc(sz + al);
        if (!malloc_ptr) return nullptr;
        // 按照请求的值对齐，留出存储原始malloc值的空间。
        void* const aligned_ptr = (void *) (((uintptr_t) malloc_ptr + al) & -al);

        // 将原始的malloc值存储在可以通过operator delete找到的地方。
        ((void **) aligned_ptr)[-1] = malloc_ptr;

        return aligned_ptr;
    }
#endif
} // namespace __gnu_cxx

/// 底层分配器
namespace xylu::xymemory::__
{
    [[noreturn]] XY_NOINLINE XY_COLD
    void abort_alloc_failed(xyu::size_t size, xyu::size_t count, xyu::size_t align) noexcept
    {
        if (size == 1) xylogf("failed to allocate memory with bytes={}, align={}", count, align);
        else xylogf("failed to allocate memory with size={}, count={}, bytes={}, align={}", size, count, size * count, align);
        xyu::abort();
    }

    void log_under_alloc(void* p, xyu::size_t size, xyu::size_t count, xyu::size_t align) noexcept
    {
        if (size == 1) xylogd("native alloc: ptr={}, bytes={}, align={}", p, count, align);
        else xylogd("native alloc: ptr={}, size={}, count={}, bytes={}, align={}", p, size, count, size * count, align);
    }

    void log_under_dealloc(void* p, xyu::size_t align) noexcept
    {
        xylogd("native dealloc: ptr={}, align={}", p, align);
    }

    void log_under_realloc(void* p, void* op, xyu::size_t size, xyu::size_t count) noexcept
    {
        if (size == 1) xylogd("native realloc: ptr={}, old_ptr={}, bytes={}", p, op, count);
        else xylogd("native realloc: ptr={}, old_ptr={}, size={}, count={}, bytes={}", p, op, size, count, size * count);
    }

    void* under_alloc(xyu::size_t bytes) noexcept { return ::malloc(bytes); }

    void under_dealloc(void* ptr) noexcept { ::free(ptr); }

    void* under_realloc(void* ptr, xyu::size_t bytes) noexcept { return ::realloc(ptr, bytes); }

    void* under_alalloc(xyu::size_t size, xyu::size_t align) noexcept
    {
#if _GLIBCXX_HAVE_POSIX_MEMALIGN
#elif _GLIBCXX_HAVE_ALIGNED_ALLOC
# if defined _AIX || defined __APPLE__
        /* AIX 7.2.0.0的aligned_alloc错误地具有posix_memalign的要求
         * 即对齐值是sizeof (void *)的倍数。
         * OS X 10.15也有相同的要求。  */
        if (align < sizeof(void*)) align = sizeof(void*);
# endif
        /* C11：size的值必须是alignment的整数倍。  */
        bytes = (bytes + align - 1) & ~(align - 1);
#endif

        return __gnu_cxx::aligned_alloc(align, size);
    }

    void under_dealalloc(void* ptr) noexcept
    {
#if _GLIBCXX_HAVE_ALIGNED_ALLOC || _GLIBCXX_HAVE_POSIX_MEMALIGN || _GLIBCXX_HAVE_MEMALIGN
        // 如果系统支持aligned_alloc，posix_memalign，memalign，则直接调用free释放内存
        std::free (ptr);
#elif _GLIBCXX_HAVE__ALIGNED_MALLOC
        // 如果系统支持对齐分配的malloc，则调用对齐释放函数
        _aligned_free (ptr);
#else
        // 见上面的aligned_alloc
        std::free(((void **) ptr)[-1]);
#endif
    }
}

/// 线程局部分配器
namespace xylu::xymemory::__
{
    static int is_pool_inited = false;

    /// 线程局部内存池
    auto& get_pool()
    {
#if XY_THREAD
        thread_local
#endif
        static struct MP
        {
            MemPool_BC pool;
            MP() noexcept
            {
                // 内存池析构可能会输出日志
                // 确保在其析构前日志已经完成构造
                xyu::xylogger();
            }
        } mp;
        if constexpr (XY_DEBUG) is_pool_inited = true;
        return mp.pool;
    }

    void log_tls_alloc(xyu::size_t size, xyu::size_t count, xyu::size_t align) noexcept
    {
        if (size == 1) xylogd("alloc: bytes={}, align={}", count, align);
        else xylogd("alloc: size={}, count={}, bytes={}, align={}", size, count, count * size, align);
    }
    void* tls_alloc(xyu::size_t bytes, xyu::size_t align) noexcept
    {
        return get_pool().alloc<char>(bytes, align);
    }

    void log_tls_dealloc(void* p, xyu::size_t size, xyu::size_t count, xyu::size_t align) noexcept
    {
        if (size == 1) xylogd("dealloc: ptr={}, bytes={}, align={}", p, count, align);
        else xylogd("dealloc: ptr={}, size={}, count={}, bytes={}, align={}", p, size, count, count * size, align);
    }
    void tls_dealloc(void* ptr, xyu::size_t bytes, xyu::size_t align) noexcept
    {
        get_pool().dealloc((char*)ptr, bytes, align);
    }

    void log_tls_prealloc(xyu::size_t size, xyu::size_t align, xyu::size_t count) noexcept
    {
        xylogd("prealloc: size={}, align={}, count={}", size, align, count);
    }
    bool tls_prealloc(xyu::size_t size, xyu::size_t align, xyu::size_t count) noexcept
    {
        return get_pool().prealloc(size, align, count);
    }

    void log_tls_unalloc(xyu::size_t size, xyu::size_t align) noexcept
    {
        if (align == -1) xylogd("unalloc: all");
        else xylogd("unalloc: size={}, align={}", size, align);
    }
    void tls_unalloc(xyu::size_t size, xyu::size_t align) noexcept { return get_pool().unalloc(size, align); }
    void tls_unalloc() noexcept { return get_pool().unalloc(); }
}

/// 全局日志
namespace xylu::xycore::__
{
    GLogger::~GLogger() noexcept
    {
        bool release = true;
        // 释放全局内存池 (输出内存释放日志)
        if constexpr (XY_DEBUG) release = xymemory::__::is_pool_inited;
        if (release) xymemory::__::get_pool().release();
    }
}

/// 失败处理函数
namespace xylu::xymemory
{
    using __::func_ht;

    static void default_func() noexcept { unalloc(); }

    func_ht alloc(xyu::nullptr_t) noexcept
    {
        auto f = (func_ht)std::get_new_handler();
        return f ? f : default_func;
    }

    func_ht alloc(xyu::special_t, func_ht func) noexcept
    {
        auto f = (func_ht)std::set_new_handler(func);
        return f ? f : default_func;
    }
}
