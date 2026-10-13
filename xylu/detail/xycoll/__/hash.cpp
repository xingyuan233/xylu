#include "../hash.h"

#include "../../xycore/__/simd.h"

namespace xylu::xycoll::__
{
    static constexpr xyu::simd_Type option =
        xyu::simd_Type::simd_Avx | xyu::simd_Type::simd_Sse | xyu::simd_Type::simd_Neon;

    xyu::uint32 simd_msb(const xyu::uint8* ctrl) noexcept
    {
        return xyu::simd_msb(xyu::simd_load<option>(ctrl));
    }

    xyu::uint32 simd_eq(const xyu::uint8* ctrl, xyu::uint8 val) noexcept
    {
        return xyu::simd_msb(
            xyu::simd_eq(xyu::simd_load<option>(ctrl), xyu::simd_fill<option>((char)val)));
    }
}
