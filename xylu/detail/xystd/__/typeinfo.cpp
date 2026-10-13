#include <cxxabi.h>

#include "../typeinfo.h"

#include "../../xycore/log.h"

namespace std
{
    xyu::HeapString<true> operator*(const std::type_info& info) noexcept
    {
        int status = 0;
        size_t capa = 0;
        char* p = abi::__cxa_demangle(info.name(), nullptr, &capa, &status);
        if (XY_UNLIKELY(status != 0)) return {info.name()};
        return xyu::HeapString<true>{xyu::special_v, capa, p};
    }
}
