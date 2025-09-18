#include <stdlib.h>
#include <new>
#include "../../head/xymemory/new.h"
#include "../../link/log"

#if !_GLIBCXX_HAVE_ALIGNED_ALLOC && !_GLIBCXX_HAVE__ALIGNED_MALLOC \
    && !_GLIBCXX_HAVE_POSIX_MEMALIGN && _GLIBCXX_HAVE_MEMALIGN
# if _GLIBCXX_HOSTED && __has_include(<malloc.h>)
// 如果C库在<malloc.h>中声明了memalign，一些C库会这样做
# include <malloc.h>
# else
extern "C" void *memalign(std::size_t boundary, std::size_t size);
# endif
#endif

using std::new_handler;
using std::bad_alloc;

#if ! _GLIBCXX_HOSTED
using std::size_t;
extern "C"
{
# if _GLIBCXX_HAVE_POSIX_MEMALIGN
    void *posix_memalign(void **, size_t alignment, size_t size);
# elif _GLIBCXX_HAVE_ALIGNED_ALLOC
    void *aligned_alloc(size_t alignment, size_t size);
# elif _GLIBCXX_HAVE__ALIGNED_MALLOC
    void *_aligned_malloc(size_t size, size_t alignment);
# elif _GLIBCXX_HAVE_MEMALIGN
    void *memalign(size_t alignment, size_t size);
# else
    // 独立的C运行时可能不提供“malloc”——但这是实现“operator new”的唯一合理方式。
    void *malloc(size_t);
# endif
    // 独立的C运行时可能不提供"free"--但这是实现"operator delete"的唯一合理方式。
    void free(void*);
}
#endif

namespace __gnu_cxx
{
// 如果可用，优先选择posix_memalign，因为它比aligned_alloc更古老，
// 因此更有可能由取代malloc库的库提供，这些库在添加aligned_alloc之前就存在了。参见PR libstdc++/113258。
#if _GLIBCXX_HAVE_POSIX_MEMALIGN
    static inline void* aligned_alloc (std::size_t al, std::size_t sz)
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
    static inline void* aligned_alloc (std::size_t al, std::size_t sz)
    { return _aligned_malloc(sz, al); }
#elif _GLIBCXX_HAVE_MEMALIGN
    static inline void* aligned_alloc (std::size_t al, std::size_t sz)
    {
      // Solaris要求al大于等于word的大小，QNX要求大于等于sizeof (void*)，
      // 但它们都提供了posix_memalign，因此将使用上面的定义。
      return memalign(al, sz);
    }
#else // !HAVE__ALIGNED_MALLOC && !HAVE_POSIX_MEMALIGN && !HAVE_MEMALIGN
// C库不提供任何对齐分配函数，定义一个。
// 这是gcc/config/i386/gmm_malloc.h中代码的一个修改版本。
    static inline void* aligned_alloc (std::size_t al, std::size_t sz)
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

using S64 = xylu::xycore::__::SString<64>;

// 自定义分配失败处理
namespace xylu::xymemory
{
    //设置处理函数 (保证与标准库的处理函数一致)
    mem_new_handler_t mem_new_handler_set(mem_new_handler_t handler)
    { return std::set_new_handler(handler); }

    //获取处理函数 (保证与标准库的处理函数一致)
    mem_new_handler_t mem_new_handler_get()
    { return std::get_new_handler(); }
}

// 底层分配函数
namespace xylu::xymemory::__
{
    void* under_alloc(xyu::size_t bytes)
    {
        void *p;
        while (XY_UNLIKELY((p = ::malloc(bytes)) == nullptr))
        {
            using namespace xylu::xymemory;
            mem_new_handler_t handler = mem_new_handler_get();
            if (XY_LIKELY(!handler)) {
                xylogm(xyu::N_LOG_FATAL, xyfmtt(S64{}, "E_Memory_Alloc: failed to allocate memory"));
                throw xyu::E_Memory_Alloc{};
            }
            handler();
        }
        return p;
    }

    void under_dealloc(void* ptr) noexcept
    {
        ::free(ptr);
    }

    void* under_alloc_align(xyu::size_t bytes, xyu::size_t align)
    {
        // 对齐必须是2的幂 (应该由编译器检查（PR 86878）)
        if (XY_UNLIKELY(__builtin_popcount(align) != 1)) {
            xylogm(xyu::N_LOG_ERROR, xyfmtt(S64{}, "E_Memory_Align: alignment {} is not a power of 2", align));
            throw xyu::E_Memory_Align{};
        }

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

        void *p;
        while ((p = __gnu_cxx::aligned_alloc (align, bytes)) == nullptr)
        {
            auto handler = std::get_new_handler();
            if (XY_LIKELY(!handler)) {
                xylogm(xyu::N_LOG_FATAL, "E_Memory_Alloc: failed to allocate memory");
                throw xyu::E_Memory_Alloc{};
            }
            handler();
        }
        return p;
    }

    void under_dealloc_align(void* ptr) noexcept
    {
#if _GLIBCXX_HAVE_ALIGNED_ALLOC || _GLIBCXX_HAVE_POSIX_MEMALIGN || _GLIBCXX_HAVE_MEMALIGN
        // 如果系统支持aligned_alloc，posix_memalign，memalign，则直接调用free释放内存
        std::free (ptr);
#elif _GLIBCXX_HAVE__ALIGNED_MALLOC
        // 如果系统支持对齐分配的malloc，则调用对齐释放函数
        _aligned_free (ptr);
#else
        // 见上面的aligned_alloc
        if (ptr) std::free(((void **) ptr)[-1]);
#endif
    }
}
