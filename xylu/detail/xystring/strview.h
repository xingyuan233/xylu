#pragma once

/**
 * @file    xystring/strview.h
 * @brief   字符串视图
 * @date    2025-12-26
 */

#include "./_.h"
#include "../xymath/calc.h"
#include "../xymemory/func.h"
#include "../xyrange/range.h"

/// 字符串视图基类
namespace xylu::xystring
{
    // 前置声明
    struct StringView;

    /**
     * @brief 字符串只读算法 CRTP 基类
     * @tparam Str 派生字符串类型
     *
     * @details
     *   方法简述：
     *   - 任何继承 `StringViewBase` 的派生类，将自动获得只读字符串操作接口
     *   - 支持 `equals`、`compare`、`find`、`rfind`、`format`、`view`、`subview` 等
     *
     *   继承要求（派生类必须提供以下方法）：
     *   - `xyu::size_t size() const noexcept`: 返回字符串的长度
     *   - `const char* data() const noexcept`: 返回指向字符串数据的指针
     *
     *   注意事项：
     *   - 空指针禁止：`data()`严禁返回空指针
     *   - c风格检测：`data()[size()]` 必须可访问
     */
    template <typename Str>
    struct StringViewBase
    {
        /* 数据容量 */
        
        /// 获取字符串大小
        constexpr xyu::size_t size() const noexcept
        {
            xyu::size_t s = derived().size();
            if (s == -1) XY_UNREACHABLE;
            return s;
        }

        /// 判断是否非空
        constexpr explicit operator bool() const noexcept { return !empty(); }
        /// 判断是否为空
        constexpr bool empty() const noexcept { return size() == 0; }

        /* 数据获取 */

        /// 获取字符串 (不会返回 nullptr)
        constexpr const char* data() const noexcept XY_RETURNS_NONNULL { return derived().data(); }

        /// 获取字符
        constexpr char get(xyu::size_t index) const noexcept
        {
            if constexpr (!XY_RELEASE) if (XY_UNLIKELY(index > size()))
                xyu::E_Logic::abort_out_range(XY_CurPos("StringViewBase::get"), index, 0, size());
            return data()[index];
        }

        /// 获取字符
        constexpr char operator[](xyu::size_t index) const noexcept { return get(index); }

        /* 范围与视图 */

        /// 获取只读范围
        constexpr auto range() const noexcept { return xyu::make_range(size(), data(), data() + size()); }
        
        /// 隐式转换为字符串视图
        constexpr operator StringView() const noexcept;

        /// 生成字符串视图
        constexpr StringView view() const noexcept;

        /**
         * @brief 获取字符串子视图
         * @param start 子视图起始位置
         * @param len 子视图长度 (默认到结尾)
         */
        constexpr StringView subview(xyu::size_t start = 0, xyu::size_t len = -1) const noexcept;

        /* 比较 */

        /// 判断两个字符串是否相等
        constexpr bool equals(StringView other) const noexcept;
        /// 比较两个字符串
        constexpr int compare(StringView other) const noexcept;
        /// 比较两个字符串，返回第一个不同字节索引，未找到返回 min(size, other.size)
        constexpr xyu::size_t contrast(StringView other) const noexcept;
        
        /* 格式化 */

        /// 生成格式化后的字符串
        template <typename... Args>
        auto format(const Args&... args) const noexcept(is_noexcept_format<Args...>());

        /* 查找 */

         /// 从 [start, end) 范围内，查找 字符串 首次出现的索引，未找到返回 -1
        constexpr xyu::size_t find(StringView target, xyu::size_t start = 0, xyu::size_t end = -1) const noexcept;
        /// 从 [start, end) 范围内，查找 字符串 最后一次出现的索引，未找到返回 -1
        constexpr xyu::size_t rfind(StringView target, xyu::size_t start = 0, xyu::size_t end = -1) const noexcept;

        /// 从 [start, end) 范围内，查找 字符 首次出现的索引，未找到返回 -1
        constexpr xyu::size_t find(char c, xyu::size_t start = 0, xyu::size_t end = -1) const noexcept;
        /// 从 [start, end) 范围内，查找 字符 首次未出现的索引，未找到返回 -1
        constexpr xyu::size_t findn(char c, xyu::size_t start = 0, xyu::size_t end = -1) const noexcept;
        /// 从 [start, end) 范围内，查找 字符 最后一次出现的索引，未找到返回 -1
        constexpr xyu::size_t rfind(char c, xyu::size_t start = 0, xyu::size_t end = -1) const noexcept;
        /// 从 [start, end) 范围内，查找 字符 最后一次未出现的索引，未找到返回 -1
        constexpr xyu::size_t rfindn(char c, xyu::size_t start = 0, xyu::size_t end = -1) const noexcept;

        /// 从 [start, end) 范围内，查找 字符集中任意字符 首次出现的索引，未找到返回 -1
        constexpr xyu::size_t findc(StringView cs, xyu::size_t start = 0, xyu::size_t end = -1) const noexcept;
        /// 从 [start, end) 范围内，查找 字符集中任意字符 首次未出现的索引，未找到返回 -1
        constexpr xyu::size_t findnc(StringView cs, xyu::size_t start = 0, xyu::size_t end = -1) const noexcept;
        /// 从 [start, end) 范围内，查找 字符集中任意字符 最后出现的索引，未找到返回 -1
        constexpr xyu::size_t rfindc(StringView cs, xyu::size_t start = 0, xyu::size_t end = -1) const noexcept;
        /// 从 [start, end) 范围内，查找 字符集中任意字符 最后未出现的索引，未找到返回 -1
        constexpr xyu::size_t rfindnc(StringView cs, xyu::size_t start = 0, xyu::size_t end = -1) const noexcept;

        /// 从 [start, end) 范围内，查找 无符号字符区间集([cus[2k],cus[2k+1]]) 中任意字符 首次出现的索引，未找到返回 -1
        constexpr xyu::size_t findu(StringView cus, xyu::size_t start = 0, xyu::size_t end = -1) const noexcept;
        /// 从 [start, end) 范围内，查找 无符号字符区间集([cus[2k],cus[2k+1]]) 中任意字符 首次未出现的索引，未找到返回 -1
        constexpr xyu::size_t findnu(StringView cus, xyu::size_t start = 0, xyu::size_t end = -1) const noexcept;
        /// 从 [start, end) 范围内，查找 无符号字符区间集([cus[2k],cus[2k+1]]) 中任意字符 最后出现的索引，未找到返回 -1
        constexpr xyu::size_t rfindu(StringView cus, xyu::size_t start = 0, xyu::size_t end = -1) const noexcept;
        /// 从 [start, end) 范围内，查找 无符号字符区间集([cus[2k],cus[2k+1]]) 中任意字符 最后未出现的索引，未找到返回 -1
        constexpr xyu::size_t rfindnu(StringView cus, xyu::size_t start = 0, xyu::size_t end = -1) const noexcept;

        /// 从 [start, end) 范围内，查找 无符号字符混合集([cus[2k],cus[2k+1]]∪{cs[k]}) 中任意字符 首次出现的索引，未找到返回 -1
        constexpr xyu::size_t findu(StringView cus, StringView cs, xyu::size_t start = 0, xyu::size_t end = -1) const noexcept;
        /// 从 [start, end) 范围内，查找 无符号字符混合集([cus[2k],cus[2k+1]]∪{cs[k]}) 中任意字符 首次未出现的索引，未找到返回 -1
        constexpr xyu::size_t findnu(StringView cus, StringView cs, xyu::size_t start = 0, xyu::size_t end = -1) const noexcept;
        /// 从 [start, end) 范围内，查找 无符号字符混合集([cus[2k],cus[2k+1]]∪{cs[k]}) 中任意字符 最后出现的索引，未找到返回 -1
        constexpr xyu::size_t rfindu(StringView cus, StringView cs, xyu::size_t start = 0, xyu::size_t end = -1) const noexcept;
        /// 从 [start, end) 范围内，查找 无符号字符混合集([cus[2k],cus[2k+1]]∪{cs[k]}) 中任意字符 最后未出现的索引，未找到返回 -1
        constexpr xyu::size_t rfindnu(StringView cus, StringView cs, xyu::size_t start = 0, xyu::size_t end = -1) const noexcept;

        /* 计数 */

        /// 统计 字符 出现次数
        constexpr xyu::size_t tally(char c) const noexcept;
        /// 统计 字符 未出现次数
        constexpr xyu::size_t tallyn(char c) const noexcept;

        /// 统计 字符集中所有字符 出现次数
        constexpr xyu::size_t tallyc(StringView cs) const noexcept;
        /// 统计 字符集中所有字符 未出现次数
        constexpr xyu::size_t tallync(StringView cs) const noexcept;

        /// 统计 无符号字符区间集 中所有字符 出现次数
        constexpr xyu::size_t tallyu(StringView cus) const noexcept;
        /// 统计 无符号字符区间集 中所有字符 未出现次数
        constexpr xyu::size_t tallynu(StringView cus) const noexcept;

        /// 统计 无符号字符混合集 中所有字符 出现次数
        constexpr xyu::size_t tallyu(StringView cus, StringView cs) const noexcept;
        /// 统计 无符号字符混合集 中所有字符 未出现次数
        constexpr xyu::size_t tallynu(StringView cus, StringView cs) const noexcept;

        /* 等长替换 */

        /// 将字符串中的 指定字符c 替换为 r 并写入 dst 中，支持任意自引用
        void replace(char* dst, char c, char r) const noexcept;
        /// 将字符串中的 指定字符c 替换为 f() 并写入 dst 中，支持任意自引用
        void replace(char* dst, char c, char(*f)()) const noexcept;
        /// 将字符串中的 非指定字符c 替换为 r 并写入 dst 中，支持任意自引用
        void replacen(char* dst, char c, char r) const noexcept;
        /// 将字符串中的 非指定字符c 替换为 f() 并写入 dst 中，支持任意自引用
        void replacen(char* dst, char c, char(*f)()) const noexcept;

        /// 将字符串中的 指定字符集cs 替换为 r 并写入 dst 中，支持任意自引用
        void replacec(char* dst, StringView cs, char r) const noexcept;
        /// 将字符串中的 指定字符集cs 回环递增 offset 并写入 dst 中，支持任意自引用
        void replacec(char* dst, StringView cs, xyu::uint offset) const noexcept;
        /// 将字符串中的 指定字符集cs 替换为 f(c) 并写入 dst 中，支持任意自引用
        void replacec(char* dst, StringView cs, char(*f)(char)) const noexcept;
        /// 将字符串中的 非指定字符集cs 替换为 r 并写入 dst 中，支持任意自引用
        void replacenc(char* dst, StringView cs, char r) const noexcept;
        /// 将字符串中的 非指定字符集cs 回环递增 offset 并写入 dst 中，支持任意自引用
        void replacenc(char* dst, StringView cs, xyu::uint offset) const noexcept;
        /// 将字符串中的 非指定字符集cs 替换为 f(c) 并写入 dst 中，支持任意自引用
        void replacenc(char* dst, StringView cs, char(*f)(char)) const noexcept;

        /// 将字符串中的 指定无符号字符区间集 替换为 r 并写入 dst 中，支持任意自引用
        void replaceu(char* dst, StringView cus, char r) const noexcept;
        /// 将字符串中的 指定无符号字符区间集 回环递增 offset 并写入 dst 中，支持任意自引用
        void replaceu(char* dst, StringView cus, xyu::uint offset) const noexcept;
        /// 将字符串中的 指定无符号字符区间集 替换为 f(c) 并写入 dst 中，支持任意自引用
        void replaceu(char* dst, StringView cus, char(*f)(char)) const noexcept;
        /// 将字符串中的 非指定无符号字符区间集 替换为 r 并写入 dst 中，支持任意自引用
        void replacenu(char* dst, StringView cus, char r) const noexcept;
        /// 将字符串中的 非指定无符号字符区间集 回环递增 offset 并写入 dst 中，支持任意自引用
        void replacenu(char* dst, StringView cus, xyu::uint offset) const noexcept;
        /// 将字符串中的 非指定无符号字符区间集 替换为 f(c) 并写入 dst 中，支持任意自引用
        void replacenu(char* dst, StringView cus, char(*f)(char)) const noexcept;

        /// 将字符串中的 指定无符号字符混合集 替换为 r 并写入 dst 中，支持任意自引用
        void replaceu(char* dst, StringView cus, StringView cs, char r) const noexcept;
        /// 将字符串中的 指定无符号字符混合集 回环递增 offset 并写入 dst 中，支持任意自引用
        void replaceu(char* dst, StringView cus, StringView cs, xyu::uint offset) const noexcept;
        /// 将字符串中的 指定无符号字符混合集 替换为 f(c) 并写入 dst 中，支持任意自引用
        void replaceu(char* dst, StringView cus, StringView cs, char(*f)(char)) const noexcept;
        /// 将字符串中的 非指定无符号字符混合集 替换为 r 并写入 dst 中，支持任意自引用
        void replacenu(char* dst, StringView cus, StringView cs, char r) const noexcept;
        /// 将字符串中的 非指定无符号字符混合集 回环递增 offset 并写入 dst 中，支持任意自引用
        void replacenu(char* dst, StringView cus, StringView cs, xyu::uint offset) const noexcept;
        /// 将字符串中的 非指定无符号字符混合集 替换为 f(c) 并写入 dst 中，支持任意自引用
        void replacenu(char* dst, StringView cus, StringView cs, char(*f)(char)) const noexcept;

        /* 类型检查与转换 */
        
        /// 是否全为数字
        constexpr bool is_digit() const noexcept { return findnu("09") == -1; }
        /// 是否全为小写字母
        constexpr bool is_lower() const noexcept { return findnu("az") == -1; }
        /// 是否全为大写字母
        constexpr bool is_upper() const noexcept { return findnu("AZ") == -1; }
        /// 是否全为字母
        constexpr bool is_alpha() const noexcept { return findnu("AZaz") == -1; }
        /// 是否全为数字或字母
        constexpr bool is_alnum() const noexcept { return findnu("09AZaz") == -1; }

        ///  将字符串中的 大写字母转换为小写字母并写入 dst 中，支持任意自引用
        void to_lower(char* dst) const noexcept { replaceu(dst, "AZ", 0x20u); }
        ///  将字符串中的 小写字母转换为大写字母并写入 dst 中，支持任意自引用
        void to_upper(char* dst) const noexcept { replaceu(dst, "az", 0xE0u); }

        /* 开头结尾匹配与去除 */

        /// 是否以 字符c 开头
        constexpr bool starts_with(char c) const noexcept { return !empty() && get(0) == c; }
        /// 是否以 字符串 开头
        constexpr bool starts_with(StringView prefix) const noexcept;

        /// 是否以 字符c 结尾
        constexpr bool ends_with(char c) const noexcept { return !empty() && get(size() - 1) == c; }
        /// 是否以 字符串 结尾
        constexpr bool ends_with(StringView suffix) const noexcept;

        /// 去除开头的指定字符
        constexpr StringView lstrip(char c) const noexcept;
        /// 去除结尾的指定字符
        constexpr StringView rstrip(char c) const noexcept;
        /// 去除开头结尾的指定字符
        constexpr StringView strip(char c) const noexcept;

    protected:
        // 计算字符串长度 (禁止传递 nullptr)
        XY_NONNULL_RELEASE(1)
        static constexpr xyu::size_t strlen(const char* str) noexcept
        {
            if constexpr (!XY_RELEASE) if (XY_UNLIKELY(!str))
                xyu::E_Logic::abort_null_pointer(XY_CurPos("StringViewBase::strlen"));
            return __builtin_strlen(str);
        }

        // 比较字符串 (禁止传递 nullptr)
        XY_NONNULL_RELEASE(1,2)
        static constexpr int strcmp(const char* str1, const char* str2, xyu::size_t len) noexcept
        {
            if constexpr (!XY_RELEASE) if (XY_UNLIKELY(!str1 || !str2))
                xyu::E_Logic::abort_null_pointer(XY_CurPos("StringViewBase::strcmp"));
            if (xyu::t_constant())
            {
                for (const char *p = str1, *q = str2, *e = str1 + len; p < e; ++p, ++q)
                    if (int r = *p - *q; r != 0) return r;
                return 0;
            }
            return xyu::mem_cmp(str1, str2, len);
        }

    private:
        // 获取派生类
        constexpr const Str& derived() const noexcept { return static_cast<const Str&>(*this); }

        // 格式化是否异常
        template <typename... Args>
        constexpr static bool is_noexcept_format() noexcept;
    };

}

/// 字符串视图
namespace xylu::xystring
{
    namespace __
    {
        template <typename Str, typename = decltype(xyu::t_val<Str>().operator StringView())>
        xyu::size_c<1> string_type_test(char&&);
        template <typename Str, typename = xyu::t_enable<xyu::t_is_same_nocr<decltype(xyu::t_val<Str>().view()), StringView>>>
        xyu::size_c<2> string_type_test(const char&);
        template <typename Str, typename = xyu::t_enable<xyu::t_is_same_nocr<decltype(xyu::t_val<Str>().size(), xyu::t_val<Str>().data()), char*>>>
        xyu::size_c<3> string_type_test(int&&);
        template <typename Str, typename = xyu::t_enable<xyu::t_is_same_nocr<decltype(xyu::t_val<Str>().c_str()), char*>>>
        xyu::size_c<4> string_type_test(const int&&);
        template <typename Str, typename = xyu::t_enable<xyu::t_can_cast<Str, const char*> && !xyu::t_is_same_nocr<Str, void*>>>
        xyu::size_c<5> string_type_test(const int&);
        template <typename Str>
        xyu::size_c<0> string_type_test(...);

        template <typename Str>
        constexpr xyu::size_t get_string_type = decltype(string_type_test<xyu::t_rem_cr<Str>>('0'))::value;
    }

    /**
     * @brief 字符串只读视图
     *
     * @details
     *   存储方式：存储字符串 起始地址 与 长度
     *
     *   方法简述：
     *   - 自动适配：允许通过各种字符串进行构造或赋值
     *     - 提供视图：拥有成员函数 `.view()` 返回 `StringView` 的类
     *     - 提供访问：拥有成员函数 `.data()` 返回 `const char*` 及 `.size()` 返回长度 的类
     *     - 提供c风格：拥有成员函数 `.c_str()` 返回 `const char*` 的类
     *     - c风格：`const char[N]`、`const char*`
     *     - 转换为c风格：能够通过 `static_cast` 转换为 `const char*` 的类
     *
     *   注意事项：
     *   - c风格检查：不保证以 '\0' 结尾，需通过 `sv[size()]` 检查
     *   - 生命周期：视图不延长原字符串生存期，必须确保原字符串在视图使用期间存活
     */
    struct StringView : StringViewBase<StringView>
    {
        // 需要拷贝时使用栈的大小限制
        static constexpr xyu::size_t K_Stack_Buffer_Size = 1024 - 1;
    private:
        const char* p;  // 字符串指针
        xyu::size_t n;  // 字符串长度

    public:
        /// 默认构造
        constexpr StringView() noexcept : p{""}, n{0} {}

        /**
         * @brief 构造函数
         * @param str 字符串指针 (不能为 nullptr)
         * @param len 字符串长度，不包括'\0'
         */
        XY_NONNULL_RELEASE(2)
        constexpr StringView(const char* str, xyu::size_t len) noexcept : p{str}, n{len}
        {
            if constexpr (!XY_RELEASE) if (XY_UNLIKELY(!str))
                xyu::E_Logic::abort_null_pointer(XY_CurPos("StringView::StringView"));
        }

        /// 构造函数，通过各种字符串构造
        template <typename Str, typename = xyu::t_enable<(__::get_string_type<Str> > 1)>>
        constexpr StringView(const Str& s) noexcept(get_stringview_noexcept<Str>) : StringView(get_stringview(s)) {}

        /// 构造函数，通过各种字符串赋值
        template <typename Str, typename = xyu::t_enable<(__::get_string_type<Str> > 1)>>
        constexpr StringView& operator=(const Str& s) noexcept(get_stringview_noexcept<Str>) { return operator=(get_stringview(s)); }

        /// 获取字符串指针 (一定不为 nullptr)
        constexpr const char* data() const noexcept XY_RETURNS_NONNULL { return p; }
        
        /// 获取字符串长度
        constexpr xyu::size_t size() const noexcept { return n; }

    private:
        // 转换为 StringView
        template <typename Str, xyu::t_enable<__::get_string_type<Str> == 2, int> = 2>
        static constexpr StringView get_stringview(const Str& s) noexcept(noexcept(s.view())) { return s.view(); }
        template <typename Str, xyu::t_enable<__::get_string_type<Str> == 3, int> = 3>
        static constexpr StringView get_stringview(const Str& s) noexcept(noexcept(s.data(), s.size())) { return {s.data(), s.size()}; }
        template <typename Str, xyu::t_enable<__::get_string_type<Str> == 4, int> = 4>
        static constexpr StringView get_stringview(const Str& s) noexcept(noexcept(s.c_str())) { auto p = s.c_str(); return {p, strlen(p)}; }
        template <typename Str, xyu::t_enable<__::get_string_type<Str> == 5, int> = 5>
        static constexpr StringView get_stringview(const Str& s) noexcept(noexcept(static_cast<const char*>(s))) { auto p = static_cast<const char*>(s); return {p, strlen(p)}; }
        template <typename Str>
        static constexpr bool get_stringview_noexcept = noexcept(get_stringview(xyu::t_val<Str>()));
    };

    /// 字符串视图常量值
    constexpr StringView operator""_sv(const char* str, xyu::size_t len) noexcept { return {str, len}; }
}

/// 字符串视图基类其余实现
namespace xylu::xystring
{
    template <typename ST>
    constexpr StringView StringViewBase<ST>::view() const noexcept { return {data(), size()}; }
    template <typename ST>
    constexpr StringViewBase<ST>::operator StringView() const noexcept { return view(); }
    template <typename ST>
    constexpr StringView StringViewBase<ST>::subview(xyu::size_t start, xyu::size_t len) const noexcept
    {
        if (XY_UNLIKELY(start >= size())) return {};
        if (len > size() - start) len = size() - start;
        return {data() + start, len};
    }

    template <typename ST>
    constexpr bool StringViewBase<ST>::equals(StringView other) const noexcept
    {
        if (size() != other.size()) return false;
        return strcmp(data(), other.data(), size()) == 0;
    }
    template <typename ST>
    constexpr int StringViewBase<ST>::compare(StringView other) const noexcept
    {
        xyu::size_t msize = xyu::min(size(), other.size());
        if (int r = strcmp(data(), other.data(), msize); r) return r;
        return size() == other.size() ? 0 : size() < other.size() ? -1 : 1;
    }

    constexpr bool operator==(StringView s1, StringView s2) noexcept { return s1.equals(s2); }
    constexpr bool operator!=(StringView s1, StringView s2) noexcept { return !s1.equals(s2); }
    constexpr bool operator>(StringView s1, StringView s2) noexcept { return s1.compare(s2) > 0; }
    constexpr bool operator>=(StringView s1, StringView s2) noexcept { return s1.compare(s2) >= 0; }
    constexpr bool operator<(StringView s1, StringView s2) noexcept { return s1.compare(s2) < 0; }
    constexpr bool operator<=(StringView s1, StringView s2) noexcept { return s1.compare(s2) <= 0; }

    template <typename Str>
    constexpr xyu::size_t StringViewBase<Str>::contrast(StringView other) const noexcept
    {
        if (xyu::t_constant())
        {
            xyu::size_t i = 0;
            for (; i < xyu::min(size(), other.size()); ++i)
                if (data()[i] != other.data()[i]) return i;
            return i;
        }
        return xymemory::__::contrast(data(), other.data(), xyu::min(size(), other.size()));
    }

    namespace __
    {
        XY_ALWAYS_INLINE constexpr bool fix_param(xyu::size_t& start, xyu::size_t& end, xyu::size_t size) noexcept
        {
            if (XY_UNLIKELY(start >= size)) return true;
            if (end > size) end = size;
            else if (XY_UNLIKELY(start >= end)) return true;
            return false;
        }

        XY_ALWAYS_INLINE constexpr xyu::size_t fix_ret(xyu::size_t ret, xyu::size_t offset) noexcept
        {
            return ret != -1 ? ret + offset : -1;
        }

        // slen > 0 && tlen > 0 && slen >= tlen
        XY_NONNULL(1, 3)
        xyu::size_t find(const char* str, xyu::size_t slen, const char* target, xyu::size_t tlen) noexcept;
        XY_NONNULL(1, 3)
        xyu::size_t rfind(const char* str, xyu::size_t slen, const char* target, xyu::size_t tlen) noexcept;
    }
    template <typename ST>
    constexpr xyu::size_t StringViewBase<ST>::find(StringView target, xyu::size_t start, xyu::size_t end) const noexcept
    {
        if (XY_UNLIKELY(__::fix_param(start, end, size()))) return -1;
        if (XY_UNLIKELY(target.empty())) return start;
        if (XY_UNLIKELY(end - start < target.size())) return -1;

        if (xyu::t_constant())
        {
            const char* p = data();
            for (xyu::size_t i = start; i <= end - target.size(); ++i)
                if (p[i] == target[0] && strcmp(p + i, target.data(), target.size()) == 0) return i;
            return -1;
        }
        return __::fix_ret(__::find(data() + start, end - start, target.data(), target.size()), start);
    }
    template <typename ST>
    constexpr xyu::size_t StringViewBase<ST>::rfind(StringView target, xyu::size_t start, xyu::size_t end) const noexcept
    {
        if (XY_UNLIKELY(__::fix_param(start, end, size()))) return -1;
        if (XY_UNLIKELY(target.empty())) return start;
        if (XY_UNLIKELY(end - start < target.size())) return -1;

        if (xyu::t_constant())
        {
            const char* p = data();
            xyu::size_t i = end - target.size() + 1;
            while (i > start) if (--i, p[i] == target[0] && strcmp(p + i, target.data(), target.size()) == 0) return i;
            return -1;
        }
        return __::fix_ret(__::rfind(data() + start, end - start, target.data(), target.size()), start);
    }

    namespace __
    {
        template <typename LoopFunc> XY_NO_VECTORIZE_GCC
        constexpr xyu::size_t find_constexpr_help(const char* p, xyu::size_t start, xyu::size_t end, LoopFunc&& lf) noexcept
        {
            #pragma clang loop vectorize(disable)
            for (xyu::size_t i = start; i < end; ++i) if (lf(p[i])) return i;
            return -1;
        }

        template <typename LoopFunc> XY_NO_VECTORIZE_GCC
        constexpr xyu::size_t find_constexpr_help_r(const char* p, xyu::size_t start, xyu::size_t end, LoopFunc&& lf) noexcept
        {
            #pragma clang loop vectorize(disable)
            for (xyu::size_t i = end; i > start;) if (lf(p[--i])) return i;
            return -1;
        }

        // slen > 0
        XY_NONNULL(1) xyu::size_t findn(const char* str, xyu::size_t slen, char c) noexcept;
        XY_NONNULL(1) xyu::size_t rfind(const char* str, xyu::size_t slen, char c) noexcept;
        XY_NONNULL(1) xyu::size_t rfindn(const char* str, xyu::size_t slen, char c) noexcept;
    }
    template <typename ST>
    constexpr xyu::size_t StringViewBase<ST>::find(char c, xyu::size_t start, xyu::size_t end) const noexcept
    {
        if (XY_UNLIKELY(__::fix_param(start, end, size()))) return -1;
        if (xyu::t_constant()) return __::find_constexpr_help(data(), start, end, [&](char s){ return s == c; });
        return __::fix_ret(xyu::mem_find(data() + start, c, end - start), start);
    }
    template <typename ST>
    constexpr xyu::size_t StringViewBase<ST>::findn(char c, xyu::size_t start, xyu::size_t end) const noexcept
    {
        if (XY_UNLIKELY(__::fix_param(start, end, size()))) return -1;
        if (xyu::t_constant()) return __::find_constexpr_help(data(), start, end, [&](char s){ return s != c; });
        return __::fix_ret(__::findn(data() + start, end - start, c), start);
    }
    template <typename ST>
    constexpr xyu::size_t StringViewBase<ST>::rfind(char c, xyu::size_t start, xyu::size_t end) const noexcept
    {
        if (XY_UNLIKELY(__::fix_param(start, end, size()))) return -1;
        if (xyu::t_constant()) return __::find_constexpr_help_r(data(), start, end, [&](char s){ return s == c; });
        return __::fix_ret(__::rfind(data() + start, end - start, c), start);
    }
    template <typename ST>
    constexpr xyu::size_t StringViewBase<ST>::rfindn(char c, xyu::size_t start, xyu::size_t end) const noexcept
    {
        if (XY_UNLIKELY(__::fix_param(start, end, size()))) return -1;
        if (xyu::t_constant()) return __::find_constexpr_help_r(data(), start, end, [&](char s){ return s != c; });
        return __::fix_ret(__::rfindn(data() + start, end - start, c), start);
    }

    namespace __
    {
        // slen > 0 && cs.size() > 0
        XY_NONNULL(1) xyu::size_t findc(const char* str, xyu::size_t slen, StringView cs) noexcept;
        XY_NONNULL(1) xyu::size_t findnc(const char* str, xyu::size_t slen, StringView cs) noexcept;
        XY_NONNULL(1) xyu::size_t rfindc(const char* str, xyu::size_t slen, StringView cs) noexcept;
        XY_NONNULL(1) xyu::size_t rfindnc(const char* str, xyu::size_t slen, StringView cs) noexcept;
    }
    template <typename ST>
    constexpr xyu::size_t StringViewBase<ST>::findc(StringView cs, xyu::size_t start, xyu::size_t end) const noexcept
    {
        if (XY_UNLIKELY(cs.empty() || __::fix_param(start, end, size()))) return -1;
        if (xyu::t_constant()) return __::find_constexpr_help(data(), start, end, [&](char c){ return cs.find(c) != -1; });
        return __::fix_ret(__::findc(data() + start, end - start, cs), start);
    }
    template <typename ST>
    constexpr xyu::size_t StringViewBase<ST>::findnc(StringView cs, xyu::size_t start, xyu::size_t end) const noexcept
    {
        if (XY_UNLIKELY(__::fix_param(start, end, size()))) return -1;
        if (cs.empty()) return start;
        if (xyu::t_constant()) return __::find_constexpr_help(data(), start, end, [&](char c){ return cs.find(c) == -1; });
        return __::fix_ret(__::findnc(data() + start, end - start, cs), start);
    }
    template <typename ST>
    constexpr xyu::size_t StringViewBase<ST>::rfindc(StringView cs, xyu::size_t start, xyu::size_t end) const noexcept
    {
        if (XY_UNLIKELY(cs.empty() || __::fix_param(start, end, size()))) return -1;
        if (xyu::t_constant()) return __::find_constexpr_help_r(data(), start, end, [&](char c){ return cs.find(c) != -1; });
        return __::fix_ret(__::rfindc(data() + start, end - start, cs), start);
    }
    template <typename ST>
    constexpr xyu::size_t StringViewBase<ST>::rfindnc(StringView cs, xyu::size_t start, xyu::size_t end) const noexcept
    {
        if (XY_UNLIKELY(__::fix_param(start, end, size()))) return -1;
        if (cs.empty()) return end - 1;
        if (xyu::t_constant()) return __::find_constexpr_help_r(data(), start, end, [&](char c){ return cs.find(c) == -1; });
        return __::fix_ret(__::rfindnc(data() + start, end - start, cs), start);
    }

    namespace __
    {
        XY_NO_VECTORIZE_GCC
        constexpr bool check_in_range(xyu::uint8 c, StringView cus, StringView cs) noexcept
        {
            for (xyu::size_t i = 0; i < (cus.size() & ~xyu::size_t(1)); i += 2)
                if (c >= (xyu::uint8)cus[i] && c <= (xyu::uint8)cus[i + 1]) return true;
            return find_constexpr_help(cs.data(), 0, cs.size(), [&](char s){ return s == c; }) != -1;
        }

        // slen > 0 && (cus.size() >= 2 || cs.size() >= 1)
        XY_NONNULL(1) xyu::size_t findu(const char* str, xyu::size_t slen, StringView cus, StringView cs) noexcept;
        XY_NONNULL(1) xyu::size_t findnu(const char* str, xyu::size_t slen, StringView cus, StringView cs) noexcept;
        XY_NONNULL(1) xyu::size_t rfindu(const char* str, xyu::size_t slen, StringView cus, StringView cs) noexcept;
        XY_NONNULL(1) xyu::size_t rfindnu(const char* str, xyu::size_t slen, StringView cus, StringView cs) noexcept;
    }
    template <typename ST>
    constexpr xyu::size_t StringViewBase<ST>::findu(StringView cus, StringView cs, xyu::size_t start, xyu::size_t end) const noexcept
    {
        if (XY_UNLIKELY((cus.size() < 2 && cs.empty()) || __::fix_param(start, end, size()))) return -1;
        if (xyu::t_constant()) return __::find_constexpr_help(data(), start, end, [&](xyu::uint8 c){ return __::check_in_range(c, cus, cs); });
        return __::fix_ret(__::findu(data() + start, end - start, cus, cs), start);
    }
    template <typename ST>
    constexpr xyu::size_t StringViewBase<ST>::findnu(StringView cus, StringView cs, xyu::size_t start, xyu::size_t end) const noexcept
    {
        if (XY_UNLIKELY(__::fix_param(start, end, size()))) return -1;
        if (XY_UNLIKELY(cus.size() < 2 && cs.empty())) return start;
        if (xyu::t_constant()) return __::find_constexpr_help(data(), start, end, [&](xyu::uint8 c){ return !__::check_in_range(c, cus, cs); });
        return __::fix_ret(__::findnu(data() + start, end - start, cus, cs), start);
    }
    template <typename ST>
    constexpr xyu::size_t StringViewBase<ST>::rfindu(StringView cus, StringView cs, xyu::size_t start, xyu::size_t end) const noexcept
    {
        if (XY_UNLIKELY((cus.size() < 2 && cs.empty()) || __::fix_param(start, end, size()))) return -1;
        if (xyu::t_constant()) return __::find_constexpr_help_r(data(), start, end, [&](xyu::uint8 c){ return __::check_in_range(c, cus, cs); });
        return __::fix_ret(__::rfindu(data() + start, end - start, cus, cs), start);
    }
    template <typename ST>
    constexpr xyu::size_t StringViewBase<ST>::rfindnu(StringView cus, StringView cs, xyu::size_t start, xyu::size_t end) const noexcept
    {
        if (XY_UNLIKELY(__::fix_param(start, end, size()))) return -1;
        if (XY_UNLIKELY(cus.size() < 2 && cs.empty())) return end - 1;
        if (xyu::t_constant()) return __::find_constexpr_help_r(data(), start, end, [&](xyu::uint8 c){ return !__::check_in_range(c, cus, cs); });
        return __::fix_ret(__::rfindnu(data() + start, end - start, cus, cs), start);
    }

    template <typename ST>
    constexpr xyu::size_t StringViewBase<ST>::findu(StringView cus, xyu::size_t start, xyu::size_t end) const noexcept { return findu(cus, "", start, end); }
    template <typename ST>
    constexpr xyu::size_t StringViewBase<ST>::findnu(StringView cus, xyu::size_t start, xyu::size_t end) const noexcept { return findnu(cus, "", start, end); }
    template <typename ST>
    constexpr xyu::size_t StringViewBase<ST>::rfindu(StringView cus, xyu::size_t start, xyu::size_t end) const noexcept { return rfindu(cus, "", start, end); }
    template <typename ST>
    constexpr xyu::size_t StringViewBase<ST>::rfindnu(StringView cus, xyu::size_t start, xyu::size_t end) const noexcept { return rfindnu(cus, "", start, end); }

    namespace __
    {
        template <typename LoopFunc> XY_NO_VECTORIZE_GCC
        constexpr xyu::size_t tally_constexpr_help(const char* p, xyu::size_t end, LoopFunc&& lf) noexcept
        {
            xyu::size_t times = 0;
            #pragma clang loop vectorize(disable)
            for (xyu::size_t i = 0; i < end; ++i) if (lf(p[i])) ++times;
            return times;
        }

        // slen > 0
        XY_NONNULL(1) xyu::size_t tally(const char* str, xyu::size_t slen, char c) noexcept;
        XY_NONNULL(1) xyu::size_t tallyn(const char* str, xyu::size_t slen, char c) noexcept;
    }
    template <typename ST>
    constexpr xyu::size_t StringViewBase<ST>::tally(char c) const noexcept
    {
        if (xyu::t_constant()) return __::tally_constexpr_help(data(), size(), [&](char s){ return s == c; });
        return __::tally(data(), size(), c);
    }
    template <typename ST>
    constexpr xyu::size_t StringViewBase<ST>::tallyn(char c) const noexcept
    {
        if (xyu::t_constant()) return __::tally_constexpr_help(data(), size(), [&](char s){ return s != c; });
        return __::tallyn(data(), size(), c);
    }

    namespace __
    {
        // slen > 0 && cs.size() > 0
        XY_NONNULL(1) xyu::size_t tallyc(const char* str, xyu::size_t slen, StringView cs) noexcept;
        XY_NONNULL(1) xyu::size_t tallync(const char* str, xyu::size_t slen, StringView cs) noexcept;
    }
    template <typename ST>
    constexpr xyu::size_t StringViewBase<ST>::tallyc(StringView cs) const noexcept
    {
        if (XY_UNLIKELY(cs.empty())) return 0;
        if (xyu::t_constant()) return __::tally_constexpr_help(data(), size(), [&](char c){ return cs.find(c) != -1; });
        return __::tallyc(data(), size(), cs);
    }
    template <typename ST>
    constexpr xyu::size_t StringViewBase<ST>::tallync(StringView cs) const noexcept
    {
        if (XY_UNLIKELY(cs.empty())) return size();
        if (xyu::t_constant()) return __::tally_constexpr_help(data(), size(), [&](char c){ return cs.find(c) == -1; });
        return __::tallync(data(), size(), cs);
    }

    namespace __
    {
        // slen > 0 && (cus.size() >= 2 || cs.size() >= 1)
        XY_NONNULL(1) xyu::size_t tallyu(const char* str, xyu::size_t slen, StringView cus, StringView cs) noexcept;
        XY_NONNULL(1) xyu::size_t tallynu(const char* str, xyu::size_t slen, StringView cus, StringView cs) noexcept;
    }
    template <typename ST>
    constexpr xyu::size_t StringViewBase<ST>::tallyu(StringView cus, StringView cs) const noexcept
    {
        if (XY_UNLIKELY((cus.size() < 2 && cs.empty()))) return 0;
        if (xyu::t_constant()) return __::tally_constexpr_help(data(), size(), [&](xyu::uint8 c){ return __::check_in_range(c, cus, cs); });
        return __::tallyu(data(), size(), cus, cs);
    }
    template <typename ST>
    constexpr xyu::size_t StringViewBase<ST>::tallynu(StringView cus, StringView cs) const noexcept
    {
        if (XY_UNLIKELY(cus.size() < 2 && cs.empty())) return size();
        if (xyu::t_constant()) return __::tally_constexpr_help(data(), size(), [&](xyu::uint8 c){ return !__::check_in_range(c, cus, cs); });
        return __::tallynu(data(), size(), cus, cs);
    }

    template <typename ST>
    constexpr xyu::size_t StringViewBase<ST>::tallyu(StringView cus) const noexcept { return tallyu(cus, ""); }
    template <typename ST>
    constexpr xyu::size_t StringViewBase<ST>::tallynu(StringView cus) const noexcept { return tallynu(cus, ""); }

    namespace __
    {
        // len > 0
        void replace(const char* str, xyu::size_t len, char* buf, char c, char r) noexcept;
        void replace(const char* str, xyu::size_t len, char* buf, char c, char(*f)()) noexcept;
        void replacen(const char* str, xyu::size_t len, char* buf, char c, char r) noexcept;
        void replacen(const char* str, xyu::size_t len, char* buf, char c, char(*f)()) noexcept;
    }
    template <typename ST>
    void StringViewBase<ST>::replace(char* dst, char c, char r) const noexcept
    {
        __::replace(data(), size(), dst, c, r);
    }
    template <typename ST>
    void StringViewBase<ST>::replace(char* dst, char c, char(*f)()) const noexcept
    {
        __::replace(data(), size(), dst, c, f);
    }
    template <typename ST>
    void StringViewBase<ST>::replacen(char* dst, char c, char r) const noexcept
    {
        __::replacen(data(), size(), dst, c, r);
    }
    template <typename ST>
    void StringViewBase<ST>::replacen(char* dst, char c, char(*f)()) const noexcept
    {
        __::replacen(data(), size(), dst, c, f);
    }

    namespace __
    {
        // slen > 0 && cs.size() > 0
        void replacec(const char* str, xyu::size_t len, char* buf, StringView cs, char r) noexcept;
        void replacec(const char* str, xyu::size_t len, char* buf, StringView cs, xyu::uint offset) noexcept;
        void replacec(const char* str, xyu::size_t len, char* buf, StringView cs, char(*f)(char)) noexcept;
        // slen > 0 && cs.size() >= 0
        void replacenc(const char* str, xyu::size_t len, char* buf, StringView cs, char r) noexcept;
        void replacenc(const char* str, xyu::size_t len, char* buf, StringView cs, xyu::uint offset) noexcept;
        void replacenc(const char* str, xyu::size_t len, char* buf, StringView cs, char(*f)(char)) noexcept;
    }
    template <typename ST>
    void StringViewBase<ST>::replacec(char* dst, StringView cs, char r) const noexcept
    {
        if (XY_UNLIKELY(cs.empty())) return;
        __::replacec(data(), size(), dst, cs, r);
    }
    template <typename ST>
    void StringViewBase<ST>::replacec(char* dst, StringView cs, xyu::uint offset) const noexcept
    {
        if (XY_UNLIKELY(cs.empty())) return;
        __::replacec(data(), size(), dst, cs, offset);
    }
    template <typename ST>
    void StringViewBase<ST>::replacec(char* dst, StringView cs, char(*f)(char)) const noexcept
    {
        if (XY_UNLIKELY(cs.empty())) return;
        __::replacec(data(), size(), dst, cs, f);
    }
    template <typename ST>
    void StringViewBase<ST>::replacenc(char* dst, StringView cs, char r) const noexcept
    {
        if (XY_UNLIKELY(cs.empty())) return xyu::mem_set(dst, r, size());
        __::replacenc(data(), size(), dst, cs, r);
    }
    template <typename ST>
    void StringViewBase<ST>::replacenc(char* dst, StringView cs, xyu::uint offset) const noexcept
    {
        __::replacenc(data(), size(), dst, cs, offset);
    }
    template <typename ST>
    void StringViewBase<ST>::replacenc(char* dst, StringView cs, char(*f)(char)) const noexcept
    {
        __::replacenc(data(), size(), dst, cs, f);
    }

    namespace __
    {
        // slen > 0 && (cus.size() >= 2 || cs.size() >= 1)
        void replaceu(const char* str, xyu::size_t len, char* buf, StringView cus, StringView cs, char r) noexcept;
        void replaceu(const char* str, xyu::size_t len, char* buf, StringView cus, StringView cs, xyu::uint offset) noexcept;
        void replaceu(const char* str, xyu::size_t len, char* buf, StringView cus, StringView cs, char(*f)(char)) noexcept;
        void replacenu(const char* str, xyu::size_t len, char* buf, StringView cus, StringView cs, char r) noexcept;
        void replacenu(const char* str, xyu::size_t len, char* buf, StringView cus, StringView cs, xyu::uint offset) noexcept;
        void replacenu(const char* str, xyu::size_t len, char* buf, StringView cus, StringView cs, char(*f)(char)) noexcept;
    }
    template <typename ST>
    void StringViewBase<ST>::replaceu(char* dst, StringView cus, StringView cs, char r) const noexcept
    {
        if (XY_UNLIKELY((cus.size() < 2 && cs.empty()))) return;
        __::replaceu(data(), size(), dst, cus, cs, r);
    }
    template <typename ST>
    void StringViewBase<ST>::replaceu(char* dst, StringView cus, StringView cs, xyu::uint offset) const noexcept
    {
        if (XY_UNLIKELY((cus.size() < 2 && cs.empty()))) return;
        __::replaceu(data(), size(), dst, cus, cs, offset);
    }
    template <typename ST>
    void StringViewBase<ST>::replaceu(char* dst, StringView cus, StringView cs, char(*f)(char)) const noexcept
    {
        if (XY_UNLIKELY((cus.size() < 2 && cs.empty()))) return;
        __::replaceu(data(), size(), dst, cus, cs, f);
    }
    template <typename ST>
    void StringViewBase<ST>::replacenu(char* dst, StringView cus, StringView cs, char r) const noexcept
    {
        if (XY_UNLIKELY(cus.size() < 2 && cs.empty())) return __::replacenc(data(), size(), dst, "", r);
        __::replacenu(data(), size(), dst, cus, cs, r);
    }
    template <typename ST>
    void StringViewBase<ST>::replacenu(char* dst, StringView cus, StringView cs, xyu::uint offset) const noexcept
    {
        if (XY_UNLIKELY(cus.size() < 2 && cs.empty())) return __::replacenc(data(), size(), dst, "", offset);
        __::replacenu(data(), size(), dst, cus, cs, offset);
    }
    template <typename ST>
    void StringViewBase<ST>::replacenu(char* dst, StringView cus, StringView cs, char(*f)(char)) const noexcept
    {
        if (XY_UNLIKELY(cus.size() < 2 && cs.empty())) return __::replacenc(data(), size(), dst, "", f);
        __::replacenu(data(), size(), dst, cus, cs, f);
    }

    template <typename ST>
    void StringViewBase<ST>::replaceu(char* dst, StringView cus, char r) const noexcept { replaceu(dst, cus, "", r); }
    template <typename ST>
    void StringViewBase<ST>::replaceu(char* dst, StringView cus, xyu::uint offset) const noexcept { replaceu(dst, cus, "", offset); }
    template <typename ST>
    void StringViewBase<ST>::replaceu(char* dst, StringView cus, char(*f)(char)) const noexcept { replaceu(dst, cus, "", f); }
    template <typename ST>
    void StringViewBase<ST>::replacenu(char* dst, StringView cus, char r) const noexcept { replacenu(dst, cus, "", r); }
    template <typename ST>
    void StringViewBase<ST>::replacenu(char* dst, StringView cus, xyu::uint offset) const noexcept { replacenu(dst, cus, "", offset); }
    template <typename ST>
    void StringViewBase<ST>::replacenu(char* dst, StringView cus, char(*f)(char)) const noexcept { replacenu(dst, cus, "", f); }

    template <typename ST>
    constexpr bool StringViewBase<ST>::starts_with(StringView prefix) const noexcept
    {
        if (prefix.size() > size()) return false;
        return strcmp(data(), prefix.data(), prefix.size()) == 0;
    }
    template <typename ST>
    constexpr bool StringViewBase<ST>::ends_with(StringView suffix) const noexcept
    {
        if (suffix.size() > size()) return false;
        return strcmp(data() + size() - suffix.size(), suffix.data(), suffix.size()) == 0;
    }

    template <typename ST>
    constexpr StringView StringViewBase<ST>::lstrip(char c) const noexcept
    {
        const char* s = data();
        const char* p = s + size();
        while (s < p && *s == c) ++s;
        return {s, static_cast<xyu::size_t>(p - s)};
    }
    template <typename ST>
    constexpr StringView StringViewBase<ST>::rstrip(char c) const noexcept
    {
        xyu::size_t n = size();
        const char* s = data();
        while (n > 0 && s[n - 1] == c) --n;
        return {s, n};
    }
    template <typename ST>
    constexpr StringView StringViewBase<ST>::strip(char c) const noexcept { return rstrip(c).lstrip(c); }
}

/// 字符串哈希
namespace xylu::xymath
{
    template <typename Str>
    struct Hasher<Str, xyu::t_enable<xystring::__::get_string_type<Str>>, 6>
    {
        constexpr xyu::size_t operator()(const Str& str) noexcept
        {
            return hash_stringview(xyu::StringView{str});
        }

        // xxHash64 常量
        static constexpr xyu::uint64 XXH_PRIME64_1 = 0x9E3779B97F4A7C15ULL;
        static constexpr xyu::uint64 XXH_PRIME64_2 = 0xBB67AE8584CAA73BULL;
        static constexpr xyu::uint64 XXH_PRIME64_3 = 0xC2B2AE3D27D4EB4FULL;
        static constexpr xyu::uint64 XXH_PRIME64_4 = 0x8F1EADFE0AFB6F37ULL;
        static constexpr xyu::uint64 XXH_PRIME64_5 = 0x2870177450012606ULL;

        static constexpr xyu::uint64 read64le(const char* p) noexcept
        {
            return (xyu::uint64)(unsigned char)p[0]
                | ((xyu::uint64)(unsigned char)p[1] << 8)
                | ((xyu::uint64)(unsigned char)p[2] << 16)
                | ((xyu::uint64)(unsigned char)p[3] << 24)
                | ((xyu::uint64)(unsigned char)p[4] << 32)
                | ((xyu::uint64)(unsigned char)p[5] << 40)
                | ((xyu::uint64)(unsigned char)p[6] << 48)
                | ((xyu::uint64)(unsigned char)p[7] << 56);
        }

        static constexpr xyu::uint64 xxh64_round(xyu::uint64 acc, xyu::uint64 input) noexcept
        {
            acc += input * XXH_PRIME64_2;
            acc = (acc << 31) | (acc >> 33);
            acc *= XXH_PRIME64_1;
            return acc;
        }

        static constexpr xyu::uint64 xxh64_merge(xyu::uint64 h, xyu::uint64 v) noexcept
        {
            h ^= xxh64_round(0, v);
            h = h * XXH_PRIME64_1 + XXH_PRIME64_4;
            return h;
        }

        static constexpr xyu::size_t hash_stringview(xyu::StringView sv) noexcept
        {
            const char* data = sv.data();
            xyu::size_t len = sv.size();

            xyu::uint64 h64{};
            if (len >= 32)
            {
                // 初始化四个累加器
                xyu::uint64 v1 = XXH_PRIME64_1 + XXH_PRIME64_2;
                xyu::uint64 v2 = XXH_PRIME64_2;
                xyu::uint64 v3 = 0;
                xyu::uint64 v4 = -XXH_PRIME64_1;

                const char* end = data + (len & ~xyu::size_t(31));
                while (data < end)
                {
                    v1 = xxh64_round(v1, read64le(data));      data += 8;
                    v2 = xxh64_round(v2, read64le(data));      data += 8;
                    v3 = xxh64_round(v3, read64le(data));      data += 8;
                    v4 = xxh64_round(v4, read64le(data));      data += 8;
                }

                h64 = ((v1 << 1) | (v1 >> 63))
                    + ((v2 << 7) | (v2 >> 57))
                    + ((v3 << 12) | (v3 >> 52))
                    + ((v4 << 18) | (v4 >> 46));
                h64 = xxh64_merge(h64, v1);
                h64 = xxh64_merge(h64, v2);
                h64 = xxh64_merge(h64, v3);
                h64 = xxh64_merge(h64, v4);
            }
            else h64 = XXH_PRIME64_5;

            h64 += len;
            len &= 31; // 剩余字节数

            // 处理剩余 8 字节块
            while (len >= 8)
            {
                h64 ^= xxh64_round(0, read64le(data));
                h64 = ((h64 << 27) | (h64 >> 37)) * XXH_PRIME64_1 + XXH_PRIME64_4;
                data += 8;
                len -= 8;
            }
            if (len >= 4)
            {
                h64 ^= (xyu::uint64)(unsigned char)data[0]
                    | ((xyu::uint64)(unsigned char)data[1] << 8)
                    | ((xyu::uint64)(unsigned char)data[2] << 16)
                    | ((xyu::uint64)(unsigned char)data[3] << 24);
                h64 = ((h64 << 23) | (h64 >> 41)) * XXH_PRIME64_3 + XXH_PRIME64_4;
                data += 4;
                len -= 4;
            }
            while (len-- > 0)
            {
                h64 ^= (xyu::uint64)(unsigned char)*data << 40;
                h64 = ((h64 << 11) | (h64 >> 53)) * XXH_PRIME64_1;
                ++data;
            }

            // 最终混淆
            h64 ^= h64 >> 33;
            h64 *= XXH_PRIME64_2;
            h64 ^= h64 >> 29;
            h64 *= XXH_PRIME64_3;
            h64 ^= h64 >> 32;

            // 适配 size_t 位数
            if constexpr (sizeof(xyu::size_t) == 8) return h64;
            else return h64 & 0xFFFFFFFFULL;
        }
    };

}
namespace xylu::xytraits::__
{
    // String <-> String
    template <typename T, typename U>
    struct is_same_hash_impl<T, U, t_enable<xylu::xystring::__::get_string_type<T> && xylu::xystring::__::get_string_type<U>>> : true_c {};
}
