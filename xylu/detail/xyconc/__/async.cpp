#include "./nativeth.h"

#if XY_THREAD

#include "../async.h"

namespace xylu::xyconc::__
{
    void async_create(void*& h, async_func_t func, void* param) { native_create(h, func, param, "xyu::Async::create"); }
    void async_join(void* h) noexcept { native_join(h, "xyu::Async::wait"); }
    void async_detach(void* h) noexcept { native_detach(h, "xyu::Async::~Async"); }
}

#endif
