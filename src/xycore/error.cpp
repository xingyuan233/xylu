#include "../../head/xycore/error.h"

#include <bits/exception_ptr.h>

namespace xylu::xycore
{
    ErrorPtr ErrorPtr::current() noexcept
    {
        auto e = std::current_exception();
        return xyu::move(*reinterpret_cast<ErrorPtr*>(&e));
    }

    void ErrorPtr::rethrow()
    {
        std::rethrow_exception(*reinterpret_cast<std::exception_ptr*>(&p));
    }
}
