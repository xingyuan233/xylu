#include "../fmt.h"

#if !XY_OUTPUT_ERROR_LOG
#  undef xylog
#  define xylog(...) 0
#endif

#define xyloge_unpack(fmt, ...) xyloge1(fmt, pos.file, pos.line, pos.func, ##__VA_ARGS__)

namespace xylu::xystring::__
{
    void abort_iter_unowned_impl(const void* p, const void* start, const void* end, xyu::ErrorPos pos) noexcept
    {
        if (start == end) xyloge_unpack("iter {} is not belong to empty string", p);
        else if ((char*)start + 1 == end) xyloge_unpack("iter {} is not belong to string range {{{}}}", p, start);
        else xyloge_unpack("iter {} is not belong to string range [{}, {})", p, start, end);
        xyu::abort();
    }
}

namespace xylu::xycore
{
    void E_Format::abort_env_index_over_limit(xyu::StringView index, ErrorPos pos) noexcept
    {
        xyloge_unpack("env index {} over limit", index);
        xyu::abort();
    }

    void E_Format::abort_env_invalid_param(xyu::StringView env, ErrorPos pos) noexcept
    {
        xyloge_unpack("invalid env param with {}", env);
        xyu::abort();
    }

    void E_Format::abort_syntax_uncompleted_placeholder(ErrorPos pos) noexcept
    {
        xyloge_unpack("syntax uncompleted placeholder");
        xyu::abort();
    }

    void E_Format::abort_args_too_many(ErrorPos pos) noexcept
    {
        xyloge_unpack("too many args");
        xyu::abort();
    }

    void E_Format::abort_args_too_few(ErrorPos pos) noexcept
    {
        xyloge_unpack("too few args");
        xyu::abort();
    }

    void E_Format::abort_dyn_not_number(ErrorPos pos) noexcept
    {
        xyloge_unpack("dynamic index not number");
        xyu::abort();
    }

    void E_Format::abort_dyn_index_over_limit(xyu::StringView index, ErrorPos pos) noexcept
    {
        xyloge_unpack("dynamic index {} over limit", index);
        xyu::abort();
    }

    void E_Format::abort_dyn_unallowed_type(ErrorPos pos) noexcept
    {
        xyloge_unpack("dynamic type not allowed");
        xyu::abort();
    }

    void E_Format::abort_lay_no_width(ErrorPos pos) noexcept
    {
        xyloge_unpack("layout without width");
        xyu::abort();
    }

    void E_Format::abort_lay_unknown_align(ErrorPos pos) noexcept
    {
        xyloge_unpack("unknown layout align type");
        xyu::abort();
    }

    void E_Format::abort_lay_width_not_number(ErrorPos pos) noexcept
    {
        xyloge_unpack("layout width not number");
        xyu::abort();
    }

    void E_Format::abort_lay_width_over_limit(xyu::StringView width, ErrorPos pos) noexcept
    {
        xyloge_unpack("layout width {} over limit", width);
        xyu::abort();
    }

    void E_Format::abort_repex_failed(ErrorPos pos) noexcept
    {
        xyloge_unpack("reg or ex parse failed");
        xyu::abort();
    }

    void E_Format::abort_reserve_over_limit(size_t value, ErrorPos pos) noexcept
    {
        xyloge_unpack("reserve {} over limit {}", value, xyu::FormatCore::limit);
        xyu::abort();
    }

    // precision 过大
    void E_Format_Rep::abort_precision_over_limit(xyu::StringView precision, ErrorPos pos) noexcept
    {
        xyloge_unpack("float-rep precision {} over limit", precision);
        xyu::abort();
    }

    // float 格式串非法
    void E_Format_Rep::abort_invalid_float_rep(xyu::StringView rep, ErrorPos pos) noexcept
    {
        xyloge_unpack("float-rep \"{}\" invalid", rep);
        xyu::abort();
    }
}
