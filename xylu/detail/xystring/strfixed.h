#pragma once

/**
 * @file    string/strfixed.h
 * @brief   固定容量字符串
 * @date    2025-12-27
 */

#include "./strbase.h"

namespace xylu::xystring
{
    /**
     * @brief 固定容量字符串
     * @tparam N 缓冲区大小（不包含结尾的 '\0'，实际占用 N+1 字节）
     */
    template <xyu::size_t N>
    struct FixedString : StringBase<FixedString<N>>
    {
        static_assert(N <= StringBase<FixedString>::limit(), "FixedString capacity overflow");

    private:
        char buf[N + 1];    // 数据
        xyu::size_t len;    // 长度

    public:
        /* 构造赋值 */

        /// 默认构造
        constexpr FixedString() noexcept : len(0), buf{} {}

        /// 拷贝构造
        constexpr FixedString(const FixedString& other) noexcept : FixedString(other.view()) {}
        /// 字符串构造
        constexpr FixedString(const char* str) noexcept : FixedString(StringView{str}) {}
        /// 字符串构造
        constexpr FixedString(const char* str, xyu::size_t len) noexcept : FixedString(StringView{str, len}) {}
        /// 字符串构造
        constexpr FixedString(StringView str) noexcept : FixedString()
        {
            if (XY_UNLIKELY(str.size() > N))
                xyu::E_Logic::abort_over_limit(XY_CurPos("FixedString::operator="), str.size(), 0, limit());
            xyu::mem_copy(buf, str.data(), str.size());
            set_size(str.size());
        }

        /// 拷贝赋值
        constexpr FixedString& operator=(const FixedString& other) noexcept { return operator=(other.view()); }
        /// 字符串赋值
        constexpr FixedString& operator=(const char* str) noexcept { return operator=(StringView{str}); }
        /// 字符串赋值
        constexpr FixedString& operator=(StringView str) noexcept
        {
            if (XY_UNLIKELY(str.size() > N))
                xyu::E_Logic::abort_over_limit(XY_CurPos("FixedString::operator="), str.size(), 0, limit());
            if (XY_UNLIKELY(str.data() >= buf && str.data() < buf + len))
                xyu::mem_move(buf, str.data(), str.size());
            else xyu::mem_copy(buf, str.data(), str.size());
            set_size(str.size());
            return *this;
        }

        /* 数据获取 */

        /// 获取字符串的最大容量
        constexpr static xyu::size_t limit() noexcept { return N; }

        /// 获取字符串大小
        constexpr xyu::size_t size() const noexcept { return len; }
        /// 获取字符串容量
        constexpr static xyu::size_t capacity() noexcept { return N; }

        /// 获取字符串
        constexpr char* data() noexcept XY_RETURNS_NONNULL { return buf; }
        /// 获取字符串
        constexpr const char* data() const noexcept XY_RETURNS_NONNULL { return buf; }

        /* 数据管理 */

        /// 清空字符串
        constexpr void clear() noexcept { set_size(0); }

    private:
        friend class StringBase<FixedString>;

        // 设置新长度
        constexpr void set_size(xyu::size_t new_len) noexcept
        {
            len = new_len;
            buf[len] = '\0';
        }

        // 扩容 (实际不会被调用)
        template <typename Migration>
        XY_COLD static constexpr void do_expand(xyu::size_t mincapa, Migration&&) noexcept
        {
            XY_UNREACHABLE;
            xyu::E_Logic::abort_over_limit(XY_CurPos("FixedString::do_expand"), mincapa, 0, limit());
        }
    };

}
