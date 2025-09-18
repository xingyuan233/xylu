#include "../../link/log"

namespace xylu::xystring::__
{
    void inner_output_error(xyu::N_LOG_LEVEL level, const StringView& mess) { xyu::flog.log(level, mess); }
}
