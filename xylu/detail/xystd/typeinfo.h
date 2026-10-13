#pragma once

/**
 * @file    xystd/typeinfo.h
 * @brief   类型信息
 * @date    2026-08-10
 */

#include "../xystring/strheap.h"

#ifndef _TYPEINFO
#define _TYPEINFO

/// 类型信息
namespace std
{
    class type_info
    {
    public:
        // 获取类型唯一标识
        const char* name() const noexcept { return p[0] == '*' ? p + 1 : p; }

        // 比较实现定义顺序
        bool before(const type_info&) const noexcept;
        bool operator==(const type_info&) const noexcept;
        bool operator!=(const type_info& info) const noexcept { return !operator==(info); }

    protected:
        const char *p;

        explicit type_info(const char *p): p(p) {}
    private:
        type_info(const type_info&) = delete;
        type_info& operator=(const type_info&) = delete;
    public:
        virtual ~type_info();
    };
}

#endif

/// 类型标识翻译
namespace std
{
    xyu::HeapString<true> operator*(const std::type_info& info) noexcept;
}

/// 哈希
namespace xylu::xymath
{
    template <>
    struct Hasher<std::type_info, void, 5>
    {
        xyu::size_t operator()(const std::type_info& info) noexcept
        { return make_hash(info.name()); }
    };
}
namespace xylu::xytraits::__
{
    // String -> type_info
    template <typename T>
    struct is_same_hash_impl<T, std::type_info, t_enable<xystring::__::get_string_type<T>>> : true_c {};
}
