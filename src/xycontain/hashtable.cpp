#include "../../head/xycontain/hashtable.h"
#include <immintrin.h>

namespace xylu::xycontain::__
{
    xyu::uint16 msb(ControlGroup& ctrl) noexcept
    {
#if defined(__SSE2__)
        const __m128i metas = _mm_load_si128(reinterpret_cast<__m128i*>(ctrl.metas));
        return _mm_movemask_epi8(metas);
#else
        static bool warned = false;
        if (!warned) {
            xylogw(xyu::K_LOG_LEVEL, "SSE2 not available, using fallback implementation");
            warned = true;
        }
        xyu::uint16 mask = 0;
        for (xyu::uint i = 0; i<16; i++)
            if (ctrl.metas[i] & 0b10000000) mask |= (1 << i);
        return mask;
#endif
    }

    xyu::uint16 cmpeq(ControlGroup& ctrl, xyu::uint8 val) noexcept
    {
#if defined(__SSE2__)
        const __m128i metas = _mm_load_si128(reinterpret_cast<__m128i*>(ctrl.metas));
        const __m128i cmp = _mm_set1_epi8(static_cast<char>(val));
        return _mm_movemask_epi8(_mm_cmpeq_epi8(metas, cmp));
#else
        static bool warned = false;
        if (!warned) {
            xylogw(xyu::K_LOG_LEVEL, "SSE2 not available, using fallback implementation");
            warned = true;
        }
        xyu::uint16 mask = 0;
        for (xyu::uint i = 0; i<16; i++)
            if (ctrl.metas[i] == val) mask |= (1 << i);
        return mask;
#endif
    }
}
