#pragma clang diagnostic push
#pragma ide diagnostic ignored "OCUnusedGlobalDeclarationInspection"
#pragma once

#include "./strview.h"
#include "../../link/memfun"
#include "../../link/calcfun"

/* 字符串函数 */
namespace xylu::xystring
{
    namespace __
    {
        // 查找字符串
        XY_NONNULL(1, 3)
        xyu::size_t find(const char* str, xyu::size_t slen, const char* pattern, xyu::size_t plen) noexcept;
        XY_NONNULL(1, 3)
        xyu::size_t rfind(const char* str, xyu::size_t slen, const char* pattern, xyu::size_t plen) noexcept;
    }

    // 判断两个字符串是否相等
    template <typename ST>
    constexpr bool StringBase<ST>::equals(const StringView& other) const noexcept
    {
        if (size() != other.size()) return false;
        return xyu::mem_cmp(data(), other.data(), size()) == 0;
    }

    // 比较两个字符串
    template <typename ST>
    constexpr int StringBase<ST>::compare(const StringView& other) const noexcept
    {
        const int r = xyu::mem_cmp(data(), other.data(), xyu::min(size(), other.size()));
        return r ? r : (size() > other.size() ? 1 : -1);
    }

    // 查找
    template <typename ST>
    constexpr xyu::size_t StringBase<ST>::find(char c, xyu::size_t start, xyu::size_t end) const noexcept
    {
        if (XY_UNLIKELY(start >= size())) return -1;
        if (end > size()) end = size();
        else if (XY_UNLIKELY(start >= end)) return -1;
        // 编译期查找
        if (xyu::t_constant()) {
            for (const char* p = data() + start; p < data() + end; ++p) if (*p == c) return p - data();
            return -1;
        }
        // 运行期查找 (c++17不支持编译期转换 void*)
        else {
            const char* p = static_cast<const char*>(__builtin_memchr(data() + start, c, end - start));
            return p ? p - data() : -1;
        }
    }
    template <typename ST>
    constexpr xyu::size_t StringBase<ST>::find(const StringView& pattern, xyu::size_t start, xyu::size_t end) const noexcept
    {
        if (XY_UNLIKELY(start >= size())) return -1;
        if (end > size()) end = size();
        else if (XY_UNLIKELY(start >= end)) return -1;
        // 编译期查找
        if (xyu::t_constant())
        {
            const char* first = data();
            const char* str = data() + start;
            for (const char* last = data() + end - pattern.size(); str <= last; ++str)
                if (*str == pattern[0] && xyu::mem_cmp(str, pattern.data(), pattern.size()) == 0) return str - first;
            return -1;
        }
        // 运行期查找
        else {
            xyu::size_t pos = __::find(data() + start, end - start, pattern.data(), pattern.size());
            return pos != -1 ? pos + start : -1;
        }
    }

    template <typename ST>
    constexpr xyu::size_t StringBase<ST>::rfind(char c, xyu::size_t start, xyu::size_t end) const noexcept
    {
        if (XY_UNLIKELY(start >= size())) return -1;
        if (end > size()) end = size();
        else if (XY_UNLIKELY(start >= end)) return -1;
        for (const char* last = data() + end - 1; last >= data() + start; --last)
            if (*last == c) return last - data();
        return -1;
    }
    template <typename ST>
    constexpr xyu::size_t StringBase<ST>::rfind(const StringView& pattern, xyu::size_t start, xyu::size_t end) const noexcept
    {
        if (XY_UNLIKELY(start >= size())) return -1;
        if (end > size()) end = size();
        else if (XY_UNLIKELY(start >= end)) return -1;
        // 编译期查找
        if (xyu::t_constant())
        {
            const char* first = data();
            const char* str = data() + end - pattern.size();
            for (const char* begin = data() + start; str >= begin; --str)
                if (*str == pattern[0] && xyu::mem_cmp(str, pattern.data(), pattern.size()) == 0) return str - first;
            return -1;
        }
        // 运行期查找
        else {
            xyu::size_t pos = __::rfind(data() + start, end - start, pattern.data(), pattern.size());
            return pos != -1 ? pos + start : -1;
        }
    }

    // 开头结尾匹配
    template <typename ST>
    constexpr bool StringBase<ST>::starts_with(char c) const noexcept
    {
        return XY_LIKELY(size()) ? data()[0] == c : false;
    }
    template <typename ST>
    constexpr bool StringBase<ST>::starts_with(const StringView& prefix) const noexcept
    {
        if (XY_UNLIKELY(prefix.size() > size())) return false;
        return xyu::mem_cmp(data(), prefix.data(), prefix.size()) == 0;
    }
    template <typename ST>
    constexpr bool StringBase<ST>::ends_with(char c) const noexcept
    {
        return XY_LIKELY(size()) ? data()[size()-1] == c : false;
    }
    template <typename ST>
    constexpr bool StringBase<ST>::ends_with(const StringView& suffix) const noexcept
    {
        if (XY_UNLIKELY(suffix.size() > size())) return false;
        return xyu::mem_cmp(data() + size() - suffix.size(), suffix.data(), suffix.size()) == 0;
    }

    namespace __
    {
        constexpr bool is_empty_char(char c) noexcept { return c == '\0' || c == ' ' || c == '\t' || c == '\r' || c == '\n'; }
    }

    // 开头结尾去除
    template <typename ST>
    constexpr StringView StringBase<ST>::lstrip() const noexcept
    {
        if (XY_UNLIKELY(!size())) return {};
        const char* s = data();
        const char* p = data() + size();
        while (__::is_empty_char(*s) && XY_LIKELY(++s < p));
        return {s, static_cast<xyu::size_t>(p - s)};
    }
    template <typename ST>
    constexpr StringView StringBase<ST>::rstrip() const noexcept
    {
        if (XY_UNLIKELY(!size())) return {};
        const char* s = data();
        const char* p = data() + size() - 1;
        while (__::is_empty_char(*p) && XY_LIKELY(--p >= s));
        return {s, static_cast<xyu::size_t>(p + 1 - s)};
    }
    template <typename ST>
    constexpr StringView StringBase<ST>::strip() const noexcept { return lstrip().rstrip(); }

    template <typename ST>
    constexpr StringView StringBase<ST>::lstrip(char c) const noexcept
    {
        if (XY_UNLIKELY(!size())) return {};
        const char* s = data();
        const char* p = data() + size();
        while (*s == c && XY_LIKELY(++s < p));
        return {s, static_cast<xyu::size_t>(p - s)};
    }
    template <typename ST>
    constexpr StringView StringBase<ST>::rstrip(char c) const noexcept
    {
        if (XY_UNLIKELY(!size())) return {};
        const char* s = data();
        const char* p = data() + size() - 1;
        while (*p == c && XY_LIKELY(--p >= s));
        return {s, static_cast<xyu::size_t>(p + 1 - s)};
    }
    template <typename ST>
    constexpr StringView StringBase<ST>::strip(char c) const noexcept { return lstrip(c).rstrip(c); }

}

#pragma clang diagnostic pop