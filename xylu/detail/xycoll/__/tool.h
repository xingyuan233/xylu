#pragma once

/**
 * @file    xycoll/__/check.h
 * @brief   通用检查
 * @date    2026-07-25
 */

#include "../_.h"
#include "../../xycore/log.h"
#include "../../xystring/fmt.h"

namespace xylu::xycoll::__
{
    /// 检查类
    struct Checker
    {
        // 检查空指针
        template <typename T, typename = xyu::t_enable<!xyu::t_is_same_nocr<T, xyu::ErrorPos>>>
        static constexpr T check_ptr(T&& p, xyu::ErrorPos pos) noexcept
        {
            if constexpr (!XY_RELEASE && xyu::t_is_pointer<T>)
                if (XY_UNLIKELY(p == nullptr)) xyu::E_Logic::abort_null_pointer(pos);
            return xyu::forward<T>(p);
        }
        template <typename... T>
        static constexpr void check_ptr(xyu::ErrorPos pos, T&&... p) noexcept
        {
            if constexpr (!XY_RELEASE)
            {
                bool check = (... || (xyu::t_is_pointer<T> && XY_UNLIKELY(p == nullptr)));
                if (XY_UNLIKELY(check)) xyu::E_Logic::abort_null_pointer(pos);
            }
        }

        // 检查索引是否越界
        static constexpr void check_index(xyu::size_t index, xyu::size_t n, xyu::ErrorPos pos) noexcept
        {
            if constexpr (!XY_RELEASE) if (XY_UNLIKELY(index >= n))
                xyu::E_Logic::abort_out_range(pos, index, 0, n);
        }

        // 检查大小增加后是否超过限制
        template <xyu::size_t Limit>
        static constexpr void check_capa(xyu::size_t capa, xyu::size_t add, xyu::ErrorPos pos) noexcept
        {
            if (XY_UNLIKELY(capa > Limit))
                xyu::E_Logic::abort_over_limit(pos, capa, add, Limit);
        }

        // 检查范围是否有限
        template <typename Range>
        static constexpr void check_range(const Range& range, xyu::ErrorPos pos) noexcept
        {
            if constexpr (XY_DEBUG) if (XY_UNLIKELY(!range.definite()))
                xyu::E_Logic::abort_range_not_definite(pos);
        }

        // 检查两个范围是否元素数量相同
        template <typename Rg1, typename Rg2>
        static constexpr void check_range(const Rg1& rg1, const Rg2& rg2, xyu::ErrorPos pos) noexcept
        {
            check_range(rg1, pos);
            check_range(rg2, pos);
            if constexpr (!XY_RELEASE) if (XY_UNLIKELY(rg1.count() != rg2.count()))
            {
                if (!xyu::t_constant()) xyloge1("range not match, key count {} and val count {}", pos.file, pos.line, pos.func, rg1.count(), rg2.count());
                xyu::abort(xyu::native_v);
            }
        }

        // 自引用检查
        template <typename T, typename U>
        static constexpr U check_alias(U&& v, xyu::size_t index, const T* data, xyu::size_t n, xyu::ErrorPos epos) noexcept
        {
            if constexpr (XY_DEBUG && xyu::t_is_same_nocr<T, U>)
            {
                if (XY_UNLIKELY(&v >= data + index && &v < data + n))
                {
                    auto pos = &v - data;
                    if (!xyu::t_constant()) xyloge1("provide alias reference {} after index {}", epos.file, epos.line, epos.func, pos, index);
                    xyu::abort(xyu::native_v);
                }
            }
            return xyu::forward<U>(v);
        }

    };

    /// 通用辅助类
    struct Common
    {
        // 双策略分配
        template <typename T, bool cross_thread>
        static T* alloc(xyu::size_t count = 1) noexcept
        {
            if constexpr (!cross_thread) return xyu::alloc<T>(count);
            else return xyu::alloc<T>(xyu::native_v, count);
        }
        // 双策略释放
        template <bool cross_thread, typename T>
        static void dealloc(T* ptr, xyu::size_t count = 1) noexcept
        {
            if constexpr (!cross_thread) xyu::dealloc(ptr, count);
            else xyu::dealloc(xyu::native_v, ptr);
        }
    };

}
