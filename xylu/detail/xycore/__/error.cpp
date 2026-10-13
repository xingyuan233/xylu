#include "../error.h"

#include <cstdlib>
#include <bits/exception_ptr.h>
#include "../../xystring/fmt.h"

/// 异常指针
namespace xylu::xycore
{
    static_assert(sizeof(ErrorPtr) == sizeof(std::exception_ptr));
    static_assert(alignof(ErrorPtr) == sizeof(std::exception_ptr));

    ErrorPtr::ErrorPtr(const ErrorPtr& other) noexcept
    {
        new (reinterpret_cast<std::exception_ptr*>(this))
            std::exception_ptr{*reinterpret_cast<const std::exception_ptr*>(&other)};
    }

    ErrorPtr::~ErrorPtr() noexcept
    {
        reinterpret_cast<std::exception_ptr*>(this)->~exception_ptr();
    }

    ErrorPtr ErrorPtr::current() noexcept
    {
        auto e = std::current_exception();
        return xyu::move(*reinterpret_cast<ErrorPtr*>(&e));
    }

    void ErrorPtr::rethrow()
    {
        if constexpr (!XY_RELEASE) if (XY_UNLIKELY(!h))
            E_Logic::abort_null_pointer(XY_CurPos(nullptr,"xyu::ErrorPtr::rethrow"));
        std::rethrow_exception(*reinterpret_cast<std::exception_ptr*>(this));
    }
}

/// 异常终止
namespace xylu::xycore
{
    using __::func_ht;

    static void empty_handle() noexcept {}

#if XY_THREAD
    static xyu::Atomic<func_ht> abort_pre_func = empty_handle;
    static xyu::Atomic<void*> abort_thread = nullptr;
#else
    static func_ht abort_pre_func = empty_handle;
    static bool abort_allow = true;
#endif

    void abort() noexcept
    {
#if XY_THREAD
        void* old = nullptr;
        void* id = xyu::Thread::current();
        if (abort_thread.compare_exchange_strong(old, id, N_Atomic_Order::Seq_Cst))
        {
            abort_pre_func.load(N_Atomic_Order::Seq_Cst)();
            xylogger().~GLogger();
        }
        else if (old != id) xyu::Duration_hour<>{114514}.sleep();
#else
        if (abort_allow)
        {
            abort_allow = false;
            abort_pre_func();
            xylogger().~GLogger();
        }
#endif
        ::abort();
    }

    func_ht abort(nullptr_t) noexcept
    {
#if XY_THREAD
        return abort_pre_func.load();
#else
        return abort_pre_func;
#endif
    }
    func_ht abort(func_ht func) noexcept
    {
        if (XY_UNLIKELY(!func)) func = empty_handle;
#if XY_THREAD
        return abort_pre_func.exchange(func);
#else
        xyu::swap(abort_pre_func, func);
        return func;
#endif
    }
}

#define xyloge_unpack(fmt, ...) \
    xyloge("{}: " fmt, pos.file ? xyfmt("{}:{} {}", pos.file, pos.line, pos.func) : pos.func, ##__VA_ARGS__)

/// 逻辑异常
namespace xylu::xycore
{
    void E_Logic::abort_unknown_error(const char* func, int e) noexcept
    {
        xyloge("{}: unkown error occurred with code {}", func, e);
        xyu::abort();
    }

    void E_Logic::abort_null_pointer_impl(ErrorPos pos, const char* name) noexcept
    {
        if (!name) xyloge_unpack("cannot pass null pointer");
        else xyloge_unpack("cannot pass null pointer as {}", name);
        xyu::abort();
    }

    void E_Logic::abort_invalid_value_impl(ErrorPos pos, size_t value, uint sign, const char* name) noexcept
    {
        if      (!name &&  sign) xyloge_unpack("invalid value {}", (xyu::diff_t)value);
        else if (!name && !sign) xyloge_unpack("invalid value {}", value);
        else if ( name &&  sign) xyloge_unpack("invalid value {} for {}", (xyu::diff_t)value, name);
        else                     xyloge_unpack("invalid value {} for {}", value, name);
        xyu::abort();
    }

    void E_Logic::abort_invalid_string_impl(ErrorPos pos, const char* str, size_t len, const char* name) noexcept
    {
        if      (!name) xyloge_unpack("invalid string {}", xyu::StringView{str, len});
        else            xyloge_unpack("invalid string {} for {}", xyu::StringView{str, len}, name);
        xyu::abort();
    }

    void E_Logic::abort_overflow_impl(ErrorPos pos, size_t a, size_t b, uint sign, const char* op) noexcept
    {
        if      (sign == 0b00) xyloge_unpack("{} {} {} is overflow", a, op, b);
        else if (sign == 0b01) xyloge_unpack("{} {} {} is overflow", a, op, (diff_t)b);
        else if (sign == 0b10) xyloge_unpack("{} {} {} is overflow", (diff_t)a, op, b);
        else                   xyloge_unpack("{} {} {} is overflow", (diff_t)a, op, (diff_t)b);
        xyu::abort();
    }

    template <typename V, typename U, typename T> [[noreturn]]
    static void abort_over_limit_help(ErrorPos pos, V value, U add, T limit, const char* name) noexcept
    {
        if (add == 0) xyloge_unpack("{} {} over limit {}", name, value, limit);
        else xyloge_unpack("{} {} add {} over limit {}", name, value, add, limit);
        xyu::abort();
    }
    void E_Logic::abort_over_limit_impl(ErrorPos pos, size_t value, size_t add, size_t limit, uint sign, const char* name) noexcept
    {
        if      (sign == 0b000) abort_over_limit_help(pos, value, add, limit, name);
        else if (sign == 0b001) abort_over_limit_help(pos, value, add, (diff_t)limit, name);
        else if (sign == 0b010) abort_over_limit_help(pos, value, (diff_t)add, limit, name);
        else if (sign == 0b011) abort_over_limit_help(pos, value, (diff_t)add, (diff_t)limit, name);
        else if (sign == 0b100) abort_over_limit_help(pos, (diff_t)value, add, limit, name);
        else if (sign == 0b101) abort_over_limit_help(pos, (diff_t)value, add, (diff_t)limit, name);
        else if (sign == 0b110) abort_over_limit_help(pos, (diff_t)value, (diff_t)add, limit, name);
        else                    abort_over_limit_help(pos, (diff_t)value, (diff_t)add, (diff_t)limit, name);
        xyu::abort();
    }

    template <typename V, typename U, typename T> [[noreturn]]
    static void abort_under_limit_help(ErrorPos pos, V value, U sub, T limit, const char* name) noexcept
    {
        if (sub == 0) xyloge_unpack("{} {} under limit {}", name, value, limit);
        else xyloge_unpack("{} {} sub {} under limit {}", name, value, sub, limit);
        xyu::abort();
    }
    void E_Logic::abort_under_limit_impl(ErrorPos pos, size_t value, size_t sub, size_t limit, uint sign, const char* name) noexcept
    {
        if      (sign == 0b000) abort_under_limit_help(pos, value, sub, limit, name);
        else if (sign == 0b001) abort_under_limit_help(pos, value, sub, (diff_t)limit, name);
        else if (sign == 0b010) abort_under_limit_help(pos, value, (diff_t)sub, limit, name);
        else if (sign == 0b011) abort_under_limit_help(pos, value, (diff_t)sub, (diff_t)limit, name);
        else if (sign == 0b100) abort_under_limit_help(pos, (diff_t)value, sub, limit, name);
        else if (sign == 0b101) abort_under_limit_help(pos, (diff_t)value, sub, (diff_t)limit, name);
        else if (sign == 0b110) abort_under_limit_help(pos, (diff_t)value, (diff_t)sub, limit, name);
        else                    abort_under_limit_help(pos, (diff_t)value, (diff_t)sub, (diff_t)limit, name);
        xyu::abort();
    }

    template <typename I, typename S, typename E> [[noreturn]]
    static void abort_out_range_help(ErrorPos pos, I index, S start, E end, const char* name) noexcept
    {
        if (start == end) xyloge_unpack("{} {} is out of empty range", name, index);
        else if (start + 1 == end) xyloge_unpack("{} {} is out of range {{{}}}", name, index, start);
        else xyloge_unpack("{} {} is out of range [{}, {})", name, index, start, end);
        xyu::abort();
    }
    void E_Logic::abort_out_range_impl(ErrorPos pos, size_t index, size_t start, size_t end, uint sign, const char* name) noexcept
    {
        if      (sign == 0b000) abort_out_range_help(pos, index, start, end, name);
        else if (sign == 0b001) abort_out_range_help(pos, index, start, (diff_t)end, name);
        else if (sign == 0b010) abort_out_range_help(pos, index, (diff_t)start, end, name);
        else if (sign == 0b011) abort_out_range_help(pos, index, (diff_t)start, (diff_t)end, name);
        else if (sign == 0b100) abort_out_range_help(pos, (diff_t)index, start, end, name);
        else if (sign == 0b101) abort_out_range_help(pos, (diff_t)index, start, (diff_t)end, name);
        else if (sign == 0b110) abort_out_range_help(pos, (diff_t)index, (diff_t)start, end, name);
        else                    abort_out_range_help(pos, (diff_t)index, (diff_t)start, (diff_t)end, name);
        xyu::abort();
    }

    void E_Logic::abort_range_not_definite_impl(ErrorPos pos) noexcept
    {
        xyloge_unpack("can't pass indefinite or unlimited range");
        xyu::abort();
    }
    void E_Logic::abort_range_unlimited_impl(ErrorPos pos) noexcept
    {
        xyloge_unpack("can't pass unlimited range");
        xyu::abort();
    }

    void E_Logic::abort_custom_error_impl(ErrorPos pos, const char* mess) noexcept
    {
        xyloge_unpack("{}", mess);
        xyu::abort();
    }
}
