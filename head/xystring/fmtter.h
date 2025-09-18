#pragma clang diagnostic push
#pragma ide diagnostic ignored "google-explicit-constructor"
#pragma ide diagnostic ignored "modernize-use-nodiscard"
#pragma ide diagnostic ignored "UnreachableCallsOfFunction"
#pragma ide diagnostic ignored "hicpp-exception-baseclass"
#pragma ide diagnostic ignored "OCUnusedGlobalDeclarationInspection"
#pragma once

#include "./fmtbase.h"
#include "../../link/bitfun"

/**
 * @file
 * @brief 为所有 C++ 内置类型提供了 `Formatter<T>` 的特化。
 * @details
 *   本文件使得整数、浮点数、布尔、指针、字符、字符串以及可转换为 `Range` 的类型
 *   能够被 `xyfmt` 和 `format` 函数直接使用。它详细定义了各种类型支持的
 *   `pattern` (模式) 和 `expand` (扩展) 格式说明符。
 */

/// 封装
namespace xylu::xytraits
{
    /**
     * @brief 一个类型包装器，用于强制其关联的 `Formatter` 走运行期解析路径。
     * @details
     *   当一个参数被 `make_dynamic()` 或 `t_dynamic` 包装后，即使在 `xyfmt` 宏中，
     *   对此参数的格式化长度计算和解析也会被推迟到运行期。
     * @tparam T 被包装的类型。
     */
    template <typename T>
    struct t_dynamic : t_ebo<T, 0>
    {
        using t_ebo<T, 0>::t_ebo;
        constexpr operator const T&() const noexcept { return this->base(); }
    };

    // 推导指引
    template <typename T>
    t_dynamic(T) -> t_dynamic<T>;

    /// 创建动态封装
    template <typename T>
    constexpr t_dynamic<T> make_dynamic(T&& v) noexcept { return t_dynamic<T>(forward<T>(v)); }
}

/// 常用类型特化
namespace xylu::xystring
{
    /// pattern 格式串 - 整数
    struct Format_Int
    {
        // 是否输出基数
        bool base_sign = false;
        // 是否大写
        bool is_upper = false;
        // 正数输出的符号类型
        enum { EMPTY = 0, POSITIVE, SPACE } ps_sign = EMPTY;
        // 基数
        enum { HEX, DEC, OCT, BIN, CHAR } base = DEC;

        // 解析整数格式串
        constexpr Format_Int& parse(const StringView& pattern) noexcept
        {
            for (char c : pattern.range())
            {
                switch (c)
                {
                    case '+': ps_sign = POSITIVE; break;
                    case '-': ps_sign = SPACE; break;
                    case 'h': case 'x': base = HEX; is_upper = false; break;
                    case 'H': case 'X': base = HEX; is_upper = true; break;
                    case 'd': case 'D': base = DEC; break;
                    case 'c': case 'C': base = CHAR; break;
                    case 'b': base = BIN; is_upper = false; break;
                    case 'B': base = BIN; is_upper = true; break;
                    case 'o': base = OCT; is_upper = false; break;
                    case 'O': base = OCT; is_upper = true; break;
                    case '#': base_sign = true; break;
                    case '~': base_sign = false; break;
                    default:;
                }
            }
            if (base == DEC) base_sign = false;
            return *this;
        }
    };

    /// pattern 格式串 - 布尔
    struct Format_Bool
    {
        bool is_str = true; // 是否输出字符串
        bool upper = false; // 是否输出大写

        // 解析bool格式串
        constexpr Format_Bool& parse(const StringView& pattern) noexcept
        {
            for (char c : pattern.range())
            {
                switch (c)
                {
                    case 's': is_str = true; upper = false; break;
                    case 'S': is_str = true; upper = true; break;
                    case '~': case 'b': case 'B': is_str = false; break;
                    case '#': is_str = true; break;
                    default:;
                }
            }
            return *this;
        }
    };

    /// pattern 格式串 - 浮点数
    struct Format_Float
    {
        // 转换符
        char specifier = 'g';
        // 代替形式
        bool alt = false;
        // 正数输出的符号类型
        enum { EMPTY = 0, POSITIVE, SPACE } ps_sign = EMPTY;
        // 精度索引
        xyu::uint sp = -1, op = -1;
        // 格式串
        const StringView* ptn = nullptr;

        // 解析float格式串
        constexpr Format_Float& parse(const StringView& pattern) noexcept
        {
            ptn = &pattern;
            for (int i = 0; i < pattern.size(); ++i)
            {
                switch (pattern.get(i))
                {
                    case '+': ps_sign = POSITIVE; break;
                    case '-': ps_sign = SPACE; break;
                    case '#': alt = true; break;
                    case '~': alt = false; break;
                    case 'f': case 'F': case 'e': case 'E':
                    case 'g': case 'G': case 'a': case 'A': specifier = pattern.get(i); break;
                    case '.': {
                        sp = i;
                        while (++i < pattern.size() && pattern.get(i) >= '0' && pattern.get(i) <= '9');
                        op = i;
                        if (i != pattern.size()) --i;
                        break;
                    }
                }
            }
            return *this;
        }

        // 计算精度
        constexpr xyu::uint precision() const noexcept
        {
            if (sp == -1) return 6;
            xyu::uint precision = 0;
            for (auto p = sp; p < op; ++p) precision = precision * 10 + (ptn->get(p) - '0');
            return precision;
        }
    };

    /**
     * @brief `Formatter` 的主模板，用于为特定类型 T 提供格式化能力。
     * @details
     *   这是本格式化库的核心扩展点。要让一个自定义类型 MyType 支持格式化，
     *   你需要为 `Formatter<MyType>` 提供一个特化版本。
     *
     *   一个完整的 `Formatter` 特化可以提供以下静态成员：
     *   - `runtime` (可选): 一个 bool 常量，若为 true，则强制该类型走运行期格式化路径。
     *   - `prepare` (可选): 编译期函数，用于预估格式化后所需的缓冲区大小。
     *   - `parse`   (可选): 运行期函数，用于精确计算格式化后的长度。
     *   - `preparse`(可选): 运行期函数，用于预估动态参数的长度。
     *   - `format`  (必须): 运行期函数，执行实际的格式化操作，将结果写入输出流。
     *
     * @see `format.h` 文件头部的 "扩展自定义类型" 部分获取详细的实现指南。
     * @tparam T 要被格式化的类型。
     * @tparam void (用于 SFINAE)
     */
    template <typename T, typename>
    struct Formatter
    {
        // 运行时解析类型
        static constexpr bool runtime =
                xyu::t_can_icast<T, StringView> // 类字符串
                || xyu::t_is_range<T>           // 范围类型
                || xyu::t_can_make_range<T>     // 可构造范围类型
                ;

        /* 整数 */

        /// 预解析整数类型格式串
        template <typename Test = T, xyu::t_enable<xyu::t_is_mathint<Test> && !xyu::t_is_same_nocv<Test, char>, char> = 'i'>
        constexpr static xyu::size_t prepare(const Format_Int& fi) noexcept
        {
            if (fi.base == Format_Int::CHAR) return 1;
            // 符号位
            xyu::size_t count = xyu::t_is_signed<T> || fi.ps_sign == Format_Int::POSITIVE;
            // 基数位
            if (fi.base_sign) count += 2;
            switch (fi.base)
            {
                case Format_Int::HEX: return count + 2 * sizeof(T);
                case Format_Int::OCT: return count + (8 * sizeof(T) + 2) / 3;
                case Format_Int::BIN: return count + 8 * sizeof(T);
                default: return count + 1 + 8 * sizeof(T) * 643L / 2136;  // (log10(2) -> 643/2136)
            }
        }
        /// 预解析整数类型格式串 (转发)
        template <typename Test = T, xyu::t_enable<xyu::t_is_mathint<Test> && !xyu::t_is_same_nocvref<Test, char>, char> = 'i'>
        constexpr static xyu::size_t prepare(const Format_Layout& fl, const StringView& pattern) noexcept
        {
            if (fl.width) return fl.width;
            return prepare(Format_Int{}.parse(pattern));
        }

        /// 解析整数类型格式串
        template <typename Test = T, xyu::t_enable<xyu::t_is_mathint<Test> && !xyu::t_is_same_nocv<Test, char>, char> = 'i'>
        static xyu::size_t parse(const T& value, const Format_Int& fi)
        {
            //--- 字符 ----
            if (fi.base == Format_Int::CHAR) return 1;

            //---- 数字 ----

            //-- 符号 + 基数标识 --
            using U = xyu::t_wider<xyu::t_change_sign2un<T>, xyu::uint>;
            U uv = value;                           // 去符号后的值
            bool np_sign = fi.ps_sign;              // 是否输出符号
            if constexpr (xyu::t_is_signed<T>)
                if (value < 0) uv = -value, np_sign = true;
            xyu::size_t count = 2 * fi.base_sign + np_sign;  // 符号位 + 基数标识
            if (value == 0) return count + 1;

            //-- 数值 --
            xyu::uint bits = 8*sizeof(U) - xyu::bit_count_0_front(uv);
            switch (fi.base)
            {
                case Format_Int::HEX: count = (bits + 3) / 4; break;
                case Format_Int::OCT: count = (bits + 2) / 3; break;
                case Format_Int::BIN: count = bits; break;
                default: { // Format_Int::DEC
                    count += 1 + bits * 643L / 2136;  // (log10(2) -> 643/2136)
                    break;
                }
            }
            return count;
        }
        /// 解析整数类型格式串 (转发)
        template <typename Test = T, xyu::t_enable<xyu::t_is_mathint<Test> && !xyu::t_is_same_nocv<Test, char>, char> = 'i'>
        static xyu::size_t parse(const T& value, const Format_Layout& fl, const StringView& pattern)
        {
            if (fl.width) return fl.width;
            return parse(value, Format_Int{}.parse(pattern));
        }

        /// 解析整数类型
        template <typename Stream, typename Test = T, xyu::t_enable<xyu::t_is_mathint<Test> && !xyu::t_is_same_nocv<Test, char>, char> = 'i'>
        static void format(Stream& out, const T& value, const Format_Layout& fl, const Format_Int& fi)
        {
            //--- 字符 ----
            if (fi.base == Format_Int::CHAR) { out << static_cast<char>(value); return; }

            //---- 数字 ----

            //-- 符号 --
            using U = xyu::t_wider<xyu::t_change_sign2un<T>, xyu::uint>;
            U uv = value;                           // 去符号后的值
            bool np_sign = fi.ps_sign;              // 是否输出符号
            int elen = 2 * fi.base_sign + np_sign;  // 额外长度 (符号位 + 基数标识)
            char sign = fi.ps_sign == Format_Int::POSITIVE ? '+' : ' '; // 符号字符
            if constexpr (xyu::t_is_signed<T>)
                if (value < 0) uv = -value, np_sign = true, sign = '-';

            //-- 数值缓冲区 --
            constexpr xyu::uint mnlen[] { 2*sizeof(T), 1+8*sizeof(T)*643L/2136, (8*sizeof(T)+2)/3, 8*sizeof(T) };
            bool autow = !fl.width;     // 是否未指定宽度
            xyu::uint nlen = autow ? mnlen[fi.base] + elen : xyu::min(fl.width, mnlen[fi.base] + elen);
            char buf[nlen];
            char* p = buf + nlen;

            //-- 格式化数值 --
            switch (fi.base)
            {
                case Format_Int::HEX: {
                    // 字符表
                    constexpr char digits_lower[] = "0123456789abcdef";
                    constexpr char digits_upper[] = "0123456789ABCDEF";
                    const char* digits = fi.is_upper ? digits_upper : digits_lower;
                    // 格式化
                    if (autow) {
                        while (uv >= 0x100)
                            p[-1] = digits[uv & 0xf],
                            p[-2] = digits[(uv >> 4) & 0xf],
                            p -= 2, uv >>= 8;
                        if (uv >= 0x10)
                            *--p = digits[uv & 0xf], uv >>= 4;
                        *--p = digits[uv];
                    } else {
                        int w = static_cast<int>(fl.width) - elen;
                        while (uv >= 0x100 && w >= 2)
                            p[-1] = digits[uv & 0xf],
                            p[-2] = digits[(uv >> 4) & 0xf],
                            p -= 2, uv >>= 8, w -= 2;
                        if (uv >= 0x10 && w > 0)
                            *--p = digits[uv & 0xf], uv >>= 4, w--;
                        if (w > 0)
                            *--p = digits[uv & 0xf];
                    }
                    break;
                }
                case Format_Int::OCT: {
                    if (autow) {
                        while (uv >= 0100)
                            p[-1] = '0' + (uv & 7),
                            p[-2] = '0' + (uv >> 3 & 7),
                            p -= 2, uv >>= 6;
                        if (uv >= 010)
                            *--p = '0' + (uv & 7), uv >>= 3;
                        *--p = '0' + uv;
                    } else {
                        int w = static_cast<int>(fl.width) - elen;
                        while (uv >= 0100 && w >= 2)
                            p[-1] = '0' + (uv & 7),
                            p[-2] = '0' + (uv >> 3 & 7),
                            p -= 2, uv >>= 6, w -= 2;
                        if (uv >= 010 && w > 0)
                            *--p = '0' + (uv & 7), uv >>= 3, w--;
                        if (w > 0)
                            *--p = '0' + (uv & 7);
                    }
                    break;
                }
                case Format_Int::BIN: {
                    if (autow) {
                        while (uv > 0b1) *--p = '0' + (uv & 1), uv >>= 1;
                        *--p = '0' + uv;
                    } else {
                        int w = static_cast<int>(fl.width) - elen;
                        while (uv > 0b1 && w > 0) *--p = '0' + (uv & 1), uv >>= 1, w--;
                        if (w > 0) *--p = '0' + (uv & 1);
                    }
                    break;
                }
                default: {  // Format_Int::DEC
                    // 字符表
                    constexpr char digits[201] =
                            "0001020304050607080910111213141516171819"
                            "2021222324252627282930313233343536373839"
                            "4041424344454647484950515253545556575859"
                            "6061626364656667686970717273747576777879"
                            "8081828384858687888990919293949596979899";
                    U tmp;
                    if (autow) {
                        while (uv >= 100)
                            tmp = (uv % 100) * 2,
                            p[-1] = digits[tmp + 1],
                            p[-2] = digits[tmp],
                            p -= 2, uv /= 100;
                        if (uv >= 10)
                            tmp = uv * 2,
                            *--p = digits[tmp + 1],
                            *--p = digits[tmp];
                        else
                            *--p = '0' + uv;
                    } else {
                        int w = static_cast<int>(fl.width) - elen;
                        while (uv >= 100 && w >= 2)
                            tmp = (uv % 100) * 2,
                            p[-1] = digits[tmp + 1],
                            p[-2] = digits[tmp],
                            p -= 2, uv /= 100, w -= 2;
                        if (uv >= 10 && w >= 2)
                            tmp = uv * 2,
                            *--p = digits[tmp + 1],
                            *--p = digits[tmp];
                        else if (w > 0)
                            *--p = '0' + uv % 10;
                    }
                    break;
                }
            }

            //-- 格式化进制 --
            if (fi.base_sign && XY_LIKELY(nlen > np_sign))
            {
                if (fi.base == Format_Int::HEX) *--p = fi.is_upper ? 'X' : 'x';
                else if (fi.base == Format_Int::BIN) *--p = fi.is_upper ? 'B' : 'b';
                else *--p = fi.is_upper ? 'O' : 'o';
                // 宽度不够则只保留 x / b / o
                if (XY_LIKELY(nlen > np_sign + 1)) *--p = '0';
            }

            //-- 无对齐 --
            if (autow)
            {
                // 符号
                if (np_sign) *--p = sign;
                // 输出
                out << StringView(p, buf + nlen - p);
                return;
            }

            //-- 对齐 --
            xyu::uint flen = fl.width - np_sign - (buf + nlen - p);
            switch (fl.align) {
                case Format_Layout::LEFT: {
                    if (np_sign) *--p = sign;
                    out.write(StringView(p, buf + nlen - p)).write(fl.fill, flen);
                    break;
                }
                case Format_Layout::CENTER: {
                    if (np_sign) *--p = sign;
                    xyu::uint lfill = (flen + 1) / 2; // 偏左填充，数值偏右
                    xyu::uint rfill = flen - lfill;
                    out.write(fl.fill, lfill).write(StringView(p, buf + nlen - p)).write(fl.fill, rfill);
                    break;
                }
                case Format_Layout::NUMERIC: {
                    if (np_sign) out.write(sign);
                    out.write(fl.fill, flen);
                    out << StringView(p, buf + nlen - p);
                    break;
                }
                default: { // 默认右对齐
                    if (np_sign) *--p = sign;
                    out.write(fl.fill, flen).write(StringView(p, buf + nlen - p));
                    break;
                }
            }
        }

        /// 无 ptn 时的 10进制简化版
        template <typename Stream, typename Test = T, xyu::t_enable<xyu::t_is_mathint<Test> && !xyu::t_is_same_nocv<Test, char>, char> = 'i'>
        static void format(Stream& out, const T& value, const Format_Layout& fl)
        {
            //-- 符号 --
            using U = xyu::t_wider<xyu::t_change_sign2un<T>, xyu::uint>;
            U uv = value;                           // 去符号后的值
            bool negtive = value < 0;
            if (negtive) uv = -uv;

            //-- 数值缓冲区 --
            constexpr xyu::uint mnlen = (8*sizeof(T)+2)/3 + 1;
            bool autow = !fl.width;     // 是否未指定宽度
            xyu::uint nlen = autow ? mnlen : xyu::min(fl.width, mnlen);
            char buf[nlen];
            char* p = buf + nlen;

            //-- 格式化数值 --
            {
                // 字符表
                constexpr char digits[201] =
                        "0001020304050607080910111213141516171819"
                        "2021222324252627282930313233343536373839"
                        "4041424344454647484950515253545556575859"
                        "6061626364656667686970717273747576777879"
                        "8081828384858687888990919293949596979899";
                U tmp;
                if (autow) {
                    while (uv >= 100)
                        tmp = (uv % 100) * 2,
                        p[-1] = digits[tmp + 1],
                        p[-2] = digits[tmp],
                        p -= 2, uv /= 100;
                    if (uv >= 10)
                        tmp = uv * 2,
                        *--p = digits[tmp + 1],
                        *--p = digits[tmp];
                    else
                        *--p = '0' + uv;
                } else {
                    int w = static_cast<int>(fl.width) - negtive;
                    while (uv >= 100 && w >= 2)
                        tmp = (uv % 100) * 2,
                        p[-1] = digits[tmp + 1],
                        p[-2] = digits[tmp],
                        p -= 2, uv /= 100, w -= 2;
                    if (uv >= 10 && w >= 2)
                        tmp = uv * 2,
                        *--p = digits[tmp + 1],
                        *--p = digits[tmp];
                    else if (w > 0)
                        *--p = '0' + uv % 10;
                }
            }

            //-- 无对齐 --
            if (autow)
            {
                // 符号
                if (negtive) *--p = '-';
                // 输出
                out << StringView(p, buf + nlen - p);
                return;
            }

            //-- 对齐 --
            xyu::uint flen = fl.width - negtive - (buf + nlen - p);
            switch (fl.align) {
                case Format_Layout::LEFT: {
                    if (negtive) *--p = '-';
                    out.write(StringView(p, buf + nlen - p)).write(fl.fill, flen);
                    break;
                }
                case Format_Layout::CENTER: {
                    if (negtive) *--p = '-';
                    xyu::uint lfill = (flen + 1) / 2; // 偏左填充，数值偏右
                    xyu::uint rfill = flen - lfill;
                    out.write(fl.fill, lfill).write(StringView(p, buf + nlen - p)).write(fl.fill, rfill);
                    break;
                }
                case Format_Layout::NUMERIC: {
                    if (negtive) *--p = '-';
                    out.write(fl.fill, flen);
                    out << StringView(p, buf + nlen - p);
                    break;
                }
                default: { // 默认右对齐
                    if (negtive) *--p = '-';
                    out.write(fl.fill, flen).write(StringView(p, buf + nlen - p));
                    break;
                }
            }
        }

        /**
         * @brief 解析整数类型
         * @details
         * ### Pattern Specifiers (`:ptn`):
         * - 'x'/'X': 输出十六进制
         * - 'b'/'B': 输出二进制
         * - 'o'/'O': 输出八进制
         * - 'd'/'D': 输出十进制
         * - 'c'/'C': 输出 ASCII 字符
         * - '+': 正数输出正号 (负数输出负号)
         * - '-': 正数输出空格 (负数输出负号)
         * - '#': 输出基数标志 (进制标志的大小写决定是 0x 还是 0X)
         * - '~': 取消 '#' 输出基数标志
         * @note 默认格式: 负数输出负号，正数无符号，十进制，右对齐
         * @attention width 过小会截断: 数值高位 > 进制标志
         */
        template <typename Stream, typename Test = T, xyu::t_enable<xyu::t_is_mathint<Test> && !xyu::t_is_same_nocv<Test, char>, char> = 'i'>
        static void format(Stream& out, const T& value, const Format_Layout& fl, const StringView& pattern)
        {
            if (pattern.empty()) return format(out, value, fl);
            else return format(out, value, fl, Format_Int{}.parse(pattern));
        }

        /* bool 类型 */

        /// 预解析布尔类型格式串
        template <typename Test = T, xyu::t_enable<xyu::t_is_same_nocv<Test, bool>, char> = 'b'>
        constexpr static xyu::size_t prepare(const Format_Bool& fb) noexcept { return fb.is_str ? 5 : 1; }
        /// 预解析布尔类型格式串 (转发)
        template <typename Test = T, xyu::t_enable<xyu::t_is_same_nocv<Test, bool>, char> = 'b'>
        constexpr static xyu::size_t prepare(const Format_Layout& fl, const StringView& pattern) noexcept
        {
            if (fl.width) return fl.width;
            return prepare(Format_Bool{}.parse(pattern));
        }

        /// 格式化布尔类型
        template <typename Stream, typename Test = T, xyu::t_enable<xyu::t_is_same_nocv<Test, bool>, char> = 'b'>
        static void format(Stream& out, const T& value, const Format_Layout& fl, const Format_Bool& fb)
        {
            bool autow = !fl.width;

            //-- 无对齐 --
            if (autow) {
                if (!fb.is_str) out << (value ? '1' : '0');
                else out << StringView(fb.upper ? (value ? "TRUE" : "FALSE") : (value ? "true" : "false"));
            }

                //-- 对齐 --
            else {
                // 单字符 (长度不够时字符为 't' / 'f')
                bool is_single = !fb.is_str || fl.width < (5 - value);
                // 总长度
                xyu::uint clen = is_single ? 1 : (5 - value);
                xyu::uint flen = fl.width - clen;
                // 布局
                auto tostr = [&]() {
                    if (!is_single) out << StringView(fb.upper ? (value ? "TRUE" : "FALSE") : (value ? "true" : "false"));
                    else out << (fb.is_str ? (fb.upper ? (value ? 'T' : 'F') : (value ? 't' : 'f')) : (value ? '1' : '0'));
                };
                auto align = fl.align;
                if (align == Format_Layout::DEFAULT)
                    align = fb.is_str ? Format_Layout::LEFT : Format_Layout::RIGHT;
                if (align == Format_Layout::CENTER) {
                    xyu::uint lfill = (flen + 1) / 2;   // 偏左填充，字符偏右
                    xyu::uint rfill = flen - lfill;
                    out.write(fl.fill, lfill);
                    tostr();
                    out.write(fl.fill, rfill);
                }
                else if (align == Format_Layout::LEFT) tostr(), out.write(fl.fill, flen);
                else out.write(fl.fill, flen), tostr();
            }
        }

        /**
         * @brief 解析布尔类型
         * @details
         * ### Pattern Specifiers (`:ptn`):
         * - 's'/'S': 输出 true/TRUE/false/FALSE
         * - 'b'/'B': 输出 1/0
         * - '#': 输出 字符串
         * - '~': 输出 0/1
         * @note 默认格式: 输出 true/false 字符串左对齐;数值右对齐
         * @attention width 过小会截断: 字符串会显示 f/t/F/T
         */
        template <typename Stream, typename Test = T, xyu::t_enable<xyu::t_is_same_nocv<Test, bool>, char> = 'b'>
        static void format(Stream& out, const T& value, const Format_Layout& fl, const StringView& pattern)
        { return format(out, value, fl, Format_Bool{}.parse(pattern)); }

        /* 浮点型 */

        /// 预解析浮点类型格式串
        template <typename Test = T, xyu::t_enable<xyu::t_is_floating<Test>, char> = 'f'>
        constexpr static xyu::size_t prepare(const Format_Layout& fl, const Format_Float& ff) noexcept
        { return xyu::max(8 + sizeof(T) + ff.precision(), fl.width); }
        /// 预解析浮点类型格式串
        template <typename Test = T, xyu::t_enable<xyu::t_is_floating<Test>, char> = 'f'>
        constexpr static xyu::size_t prepare(const Format_Layout& fl, const StringView& pattern) noexcept
        {
            xyu::uint precision = 6;    // 精度

            xyu::size_t i = pattern.find('.');
            if (i != -1) {
                precision = 0;
                do {
                    char c = pattern.get(i);
                    if (c < '0' || c > '9') break;
                    precision = precision * 10 + c - '0';
                } while (++i < pattern.size());
            }
            return xyu::max(8 + sizeof(T) + precision, fl.width);
        }

        /// 解析浮点类型格式串
        template <typename Test = T, xyu::t_enable<xyu::t_is_floating<Test>, char> = 'f'>
        static xyu::size_t parse(const T& value, const Format_Layout& fl, const Format_Float& ff)
        {
            xyu::uint plen = ff.op - ff.sp;
            if (XY_UNLIKELY(plen > 3)) {
                __::output_error(xyu::N_LOG_ERROR, "{}:{}:{} E_Format_PtnEx: precision too large", __FILE__, __LINE__, __func__);
                throw xyu::E_Format_PtnEx{};
            }

            //---- 分配格式化空间 ----
            char sfmt[5 + plen + (XY_LIKELY(fl.width <= 9999) ? 4 : 10)];
            //---- 构造格式化字符串 ----
            char* p = sfmt;
            *p++ = '%';
            // 替代形式
            if (ff.alt) *p++ = '#';
            // 精度
            if (ff.sp != -1) {
                xyu::mem_copy(p, ff.ptn->data() + ff.sp, plen);
                p += plen;
            }
            // 长
            if constexpr (sizeof(T) > sizeof(double)) *p++ = 'L';
            // 转换符
            *p++ = ff.specifier;
            *p = '\0';

            //---- 格式化长度 ----
            bool ps_sign = ff.ps_sign != Format_Float::EMPTY && value > 0;
            auto need = __builtin_snprintf(nullptr, 0, sfmt, value);
            return xyu::max(need + ps_sign, fl.width);
        }
        /// 解析浮点类型格式串 (转发)
        template <typename Test = T, xyu::t_enable<xyu::t_is_floating<Test>, char> = 'f'>
        static xyu::size_t parse(const T& value, const Format_Layout& fl, const StringView& pattern)
        { return parse(value, fl, Format_Float{}.parse(pattern)); }

        /// 解析浮点类型
        template <typename Stream, typename Test = T, xyu::t_enable<xyu::t_is_floating<Test>, char> = 'f'>
        static void format(Stream& out, const T& value, const Format_Layout& fl, const Format_Float& ff)
        {
#if (defined(_WIN32) || defined(__CYGWIN__))
            // windows 不支持 %Lf 格式，因此使用 %f 代替
            if constexpr (xyu::t_is_same_nocv<T, long double>)
                return Formatter<double>::format(out, static_cast<double>(value), fl, ff);
#endif

            xyu::uint plen = ff.op - ff.sp;
            if (XY_UNLIKELY(plen > 3)) {
                xylogei(false, "E_Format_PtnEx: precision too large");
                throw xyu::E_Format_PtnEx{};
            }

            //---- 分配格式化空间 ----
            char sfmt[5 + plen + (XY_LIKELY(fl.width <= 9999) ? 4 : 10)];
            //---- 构造格式化字符串 ----
            char* p = sfmt;
            *p++ = '%';
            // 替代形式
            if (ff.alt) *p++ = '#';
            // 精度
            if (ff.sp != -1)
                xyu::mem_copy(p, ff.ptn->data() + ff.sp, plen),
                        p += plen;
            // 长
            if constexpr (sizeof(T) > sizeof(double)) *p++ = 'L';
            // 转换符
            *p++ = ff.specifier;
            *p = '\0';

            //---- 格式化 ----
            String buf;
            auto need = __builtin_snprintf(buf.data(), buf.capacity() + 1, sfmt, value);
            if (XY_UNLIKELY(need > buf.capacity())) {
                buf.reserve(need);
                need = __builtin_snprintf(buf.data(), buf.capacity() + 1, sfmt, value);
            }
            buf.resize(need);

            //-- 符号 --
            bool ps_sign = ff.ps_sign != Format_Float::EMPTY && value > 0;

            //-- 无对齐 --
            if (!fl.width)
            {
                if (ps_sign) out << (ff.ps_sign == Format_Float::POSITIVE ? '+' : ' ');
                out << buf.view();
                return;
            }

            //-- 对齐 --
            int flen = static_cast<int>(fl.width) - ps_sign - static_cast<int>(buf.size());
            switch (fl.align) {
                case Format_Layout::LEFT: {
                    if (ps_sign) out << (ff.ps_sign == Format_Float::POSITIVE ? '+' : ' ');
                    if (flen > 0) out.write(fl.fill, flen);
                    out << buf.view();
                    break;
                }
                case Format_Layout::CENTER: {
                    int lfill = (flen + 1) / 2;   // 偏左填充，字符偏右
                    int rfill = flen - lfill;
                    out.write(fl.fill, lfill);
                    if (ps_sign) out << (ff.ps_sign == Format_Float::POSITIVE ? '+' : ' ');
                    out << buf.view();
                    out.write(fl.fill, rfill);
                    break;
                }
                case Format_Layout::NUMERIC: {
                    if (buf[0] == '-') out.write('-').write(fl.fill, flen).write(buf.subview(1));
                    else {
                        if (ps_sign) out << (ff.ps_sign == Format_Float::POSITIVE ? '+' : ' ');
                        out.write(fl.fill, flen).write(buf.view());
                    }
                    break;
                }
                default: { // 默认右对齐
                    if (flen > 0) out.write(fl.fill, flen);
                    if (ps_sign) out << (ff.ps_sign == Format_Float::POSITIVE ? '+' : ' ');
                    out << buf.view();
                    break;
                }
            }
        }

        /**
         * @brief 解析浮点类型
         * @details
         * ### Pattern Specifiers (`:ptn`):
         * - 'a'/'A': 输出十六进制
         * - 'e'/'E': 输出指数记法
         * - 'f'/'F': 输出固定小数点
         * - 'g'/'G': 输出自动选择小数点或指数记法
         * - '+': 正数输出正号
         * - '.': 精度: f/a/e下表示小数点后位数，g下表示总位数
         * - '#': 在'f'/'e'下强制输出小数点并保留尾随零，在'a'/'A'下输出基数标志
         * - '~': 取消 '#' 标志
         * @note 默认格式: 输出自动选择小数点或指数记法，右对齐
         * @attention width 过小时不会生效
         */
        template <typename Stream, typename Test = T, xyu::t_enable<xyu::t_is_floating<Test>, char> = 'f'>
        static void format(Stream& out, const T& value, const Format_Layout& fl, const StringView& pattern)
        { return format(out, value, fl, Format_Float{}.parse(pattern)); }

        /* 字符型 */

        /// 预解析字符型格式串
        template <typename Test = T, xyu::t_enable<xyu::t_is_same_nocv<Test, char>, char> = 'c'>
        constexpr static xyu::size_t prepare(const StringView& pattern) noexcept
        {
            if (pattern.empty()) return 1;
            return Formatter<unsigned char>::prepare(Format_Layout{}, pattern);
        }

        /// 解析字符型格式串
        template <typename Test = T, xyu::t_enable<xyu::t_is_same_nocv<Test, char>, char> = 'c'>
        static xyu::size_t parse(const T& value, const StringView& pattern)
        {
            if (pattern.empty()) return 1;
            return Formatter<unsigned char>::parse(value, Format_Layout{}, pattern);
        }

        /**
         * @brief 格式化字符型
         * @note 默认输出字符，若提供 ptn 选项，则作为整数解析
         */
        template <typename Stream, typename Test = T, xyu::t_enable<xyu::t_is_same_nocv<Test, char>, char> = 'c'>
        static void format(Stream& out, const T& value, const StringView& pattern)
        {
            if (pattern.empty()) { out << value; return; }
            return Formatter<unsigned char>::format(out, value, Format_Layout{}, pattern);
        }

        /* 字符串类型 */

        /// 解析字符串型格式串
        template <typename Test = T, xyu::t_enable<xyu::t_can_icast<Test, StringView>, char> = 's'>
        static xyu::size_t parse(const StringView& str, const Format_Layout& fl)
        { return fl.width ? fl.width : str.size(); }

        /**
         * @brief 格式化字符串
         * @note 默认左对齐
         * @attention width 过小会根据 align 截断 (如: 左对齐就截断右边)
         */
        template <typename Stream, typename Test = T, xyu::t_enable<xyu::t_can_icast<Test, StringView>, char> = 's'>
        static void format(Stream& out, const StringView& str, const Format_Layout& fl)
        {
            // 直接输出
            if (fl.width == 0) { out << str; return; }
            // 布局
            bool overflow = str.size() >= fl.width;
            if (!overflow) out.expand(fl.width);
            switch (fl.align)
            {
                case Format_Layout::RIGHT: {
                    if (overflow) out << str.subview(str.size() - fl.width, fl.width);
                    else out.write(fl.fill, fl.width - str.size()).write(str);
                    break;
                }
                case Format_Layout::CENTER: {
                    if (overflow) out << str.subview((str.size() - fl.width)/2, fl.width);
                    else {
                        xyu::uint sfill = fl.width - str.size();
                        xyu::uint lfill = sfill / 2;
                        xyu::uint rfill = sfill - lfill;  // 偏右填充，字符串偏左
                        out.write(fl.fill, lfill).write(str).write(fl.fill, rfill);
                    }
                    break;
                }
                default: { // 左对齐
                    if (overflow) out << str.subview(0, fl.width);
                    else out.write(str).write(fl.fill, fl.width - str.size());
                    break;
                }
            }
        }

        /* 指针类型 */

        /// 预解析指针型格式串
        template <typename Test = T, xyu::t_enable<xyu::t_is_pointer<Test> && !xyu::t_can_icast<Test, StringView>, char> = 'p'>
        constexpr xyu::size_t prepare(const Format_Layout& fl, const StringView& pattern) noexcept
        {
            if (fl.width) return fl.width;
            Format_Int fi;
            fi.base = Format_Int::HEX;
            fi.base_sign = true;
            using U = xyu::t_change_sign2un<xyu::diff_t>;
            return Formatter<U>::prepare(fi.parse(pattern));
        }

        /// 解析指针型格式串
        template <typename Test = T, xyu::t_enable<xyu::t_is_pointer<Test> && !xyu::t_can_icast<Test, StringView>, char> = 'p'>
        static xyu::size_t parse(const T& ptr, const Format_Layout& fl, const StringView& pattern)
        {
            if (fl.width) return fl.width;
            Format_Int fi;
            fi.base = Format_Int::HEX;
            fi.base_sign = true;
            using U = xyu::t_change_sign2un<xyu::diff_t>;
            return Formatter<U>::parse(reinterpret_cast<U>(ptr), fi.parse(pattern));
        }

        /**
         * @brief 格式化指针型
         * @details 格式化参数与整数相同
         * @note 默认格式: 输出指针值，十六进制，基数标志，右对齐
         */
        template <typename Stream, typename Test = T, xyu::t_enable<xyu::t_is_pointer<Test> && !xyu::t_can_icast<Test, StringView>, char> = 'p'>
        static void format(Stream& out, const T& ptr, const Format_Layout& fl, const StringView& pattern)
        {
            Format_Int fi;
            fi.base = Format_Int::HEX;
            fi.base_sign = true;
            using U = xyu::t_change_sign2un<xyu::diff_t>;
            Formatter<U>::format(out, reinterpret_cast<U>(ptr), fl, fi.parse(pattern));
        }

        /* 范围类型 */

        /// 解析范围类型格式串
        template <typename Test = T, xyu::t_enable<xyu::t_is_range<Test>, char> = 'r'>
        static xyu::size_t parse(const T& range, const StringView& pattern, const StringView& expand)
        {
            xyu::size_t count = 2;
            if (range.empty() || range.count() > String::limit()) return count;
            auto es = parse_split(expand);
            count += (range.count() - 1) * es.split.size();
            for (auto&& e : range)
                count += __::call_formatter_parse<xyu::t_remove_cvref<decltype(e)>>(&e, Format_Layout{}, pattern, es.last);
            return count;
        }

        /// 运行期预解析范围类型格式串
        template <typename Test = T, xyu::t_enable<xyu::t_is_range<Test>, char> = 'r'>
        constexpr static xyu::size_t preparse(const T& range, const StringView& pattern, const StringView& expand)
        {
            xyu::size_t count = 2;
            if (range.empty() || range.count() > String::limit()) return count;
            auto es = parse_split(expand);
            count += (range.count() - 1) * es.split.size();
            using U = xyu::t_remove_cvref<decltype(*range.begin())>;
            if constexpr (!__::is_formatter_runtime<U>)
                count += __::call_formatter_prepare<U>(Format_Layout{}, pattern, es.last) * range.count();
            else for (auto&& e : range)
                count += __::call_formatter_parse<xyu::t_remove_cvref<decltype(e)>>(&e, Format_Layout{}, pattern, es.last);
            return count;
        }

        /**
         * @brief 格式化范围类型
         * @note 可以控制每个元素的格式以及它们之间的分隔符。
         *
         * --- [ptn] (Pattern Specifier) ---
         * 用于控制每一个元素的 `pattern` 部分格式，直接将这部分内容传递给每一个元素
         *
         * --- [?ex] (Expand Specifier) - 扩展格式 ---
         * 用于控制元素间的分隔符以及指定元素的 `expand` 格式，由 '?' 引导。
         *
         * - **语法:** `"?<separator>,spec"`
         *
         * - **`<separator>` (分隔符) 部分:**
         *   - **默认:** 如果 `ex` 部分为空，或以逗号开头，分隔符默认为 `","`。
         *   - **自定义:** 逗号前的部分(没有则整个 `ex` 部分)被视为分隔符。
         *     - `"{? | }"` -> 分隔符为 `" | "`
         *   - **括号包裹:**
         *     - 使用括号包裹空字符串 `"{?()}"` 来指定空分隔符。
         *     - 括号内的内容即为分隔符，例如 `"{?(, )}"` 的分隔符是 `", "`。
         *
         * - **`spec` (元素扩展格式) 部分:**
         *   - 规则与 `ptn` 部分完全相同，直接将这部分内容传递给每一个元素的 `expand` 格式。
         */
        template <typename Stream, typename Test = T, xyu::t_enable<xyu::t_is_range<Test>, char> = 'r'>
        static void format(Stream& out, const T& range, const StringView& pattern, const StringView& expand)
        {
            if (range.empty()) { out << "[]"; return; }
            auto es = parse_split(expand);
            out << '[';
            for(auto i = range.begin(), j = range.end();;) {
                auto &&e = *i;
                __::call_formatter_format<xyu::t_remove_cvref<decltype(*i)>>(out, &e, Format_Layout{}, pattern, es.last);
                if (++i == j) break;
                out << es.split;
            }
            out << ']';
        }

        /* 可构造范围类型 */

        /// 解析可构造范围类型格式串
        template <typename Test = T, xyu::t_enable<!xyu::t_is_range<Test> && xyu::t_can_make_range<Test> && !xyu::t_is_number<Test> && !xyu::t_can_icast<Test, StringView>, char> = 'x'>
        static xyu::size_t parse(const T& value, const StringView& pattern, const StringView& expand)
        { return Formatter<xyu::t_remove_cvref<decltype(xyu::make_range(value))>>::parse(xyu::make_range(value), pattern, expand); }

        /// 运行期预解析可构造范围类型格式串
        template <typename Test = T, xyu::t_enable<!xyu::t_is_range<Test> && xyu::t_can_make_range<Test> && !xyu::t_is_number<Test> && !xyu::t_can_icast<Test, StringView>, char> = 'x'>
        constexpr static xyu::size_t preparse(const T& value, const StringView& pattern, const StringView& expand)
        { return Formatter<xyu::t_remove_cvref<decltype(xyu::make_range(value))>>::preparse(xyu::make_range(value), pattern, expand); }

        /// 格式化可构造范围类型
        template <typename Stream, typename Test = T, xyu::t_enable<!xyu::t_is_range<Test> && !xyu::t_is_number<Test> && xyu::t_can_make_range<Test> && !xyu::t_can_icast<Test, StringView>, char> = 'x'>
        static void format(Stream& out, const T& value, const StringView& pattern, const StringView& expand)
        { Formatter<xyu::t_remove_cvref<decltype(xyu::make_range(value))>>::format(out, xyu::make_range(value), pattern, expand); }

    public:
        // 获取分隔字符串与后续格式串
        struct expand_split { StringView split, last; };
        constexpr static expand_split parse_split(const StringView& expand) noexcept
        {
            // 空格式串
            if (expand.empty()) return { ",", {} };
            // 括号包裹
            if (expand.get(0) == '(')
            {
                auto i = expand.find(')');
                if (i == -1) return { expand, {} };
                auto j = expand.find(',', i + 1);
                if (j == -1) return { expand.subview(1, i - 1), {} };
                else return { expand.subview(1, i - 1), expand.subview(j + 1) };
            }
            // 直接
            else
            {
                auto i = expand.find(',');
                if (i == -1) return { expand, {} };
                else if (i == 0) return { ",", expand.subview(1) };
                else return { expand.subview(0, i), expand.subview(i + 1) };
            }
        }
    };

    /// nullptr 类型
    template <>
    struct Formatter<xyu::nullptr_t>
    {
        /// 预解析格式串
        constexpr static xyu::size_t prepare() noexcept { return 7; }
        /// 格式化
        template <typename Stream>
        static void format(Stream& out, const xyu::nullptr_t&) { out << "nullptr"; }
    };

    /// 数组类型
    template <typename T, xyu::size_t N>
    struct Formatter<T[N], xyu::t_enable<!xyu::t_is_same_nocv<T, char>>>
    {
        // 运行时解析
        static constexpr bool runtime = __::is_formatter_runtime<T>;

        /// 预解析数组类型格式串
        constexpr static xyu::size_t prepare(const StringView& pattern, const StringView& expand) noexcept
        {
            if (pattern.empty()) return 2;
            auto es = Formatter<int>::parse_split(expand);
            xyu::size_t count =  __::call_formatter_prepare<T>(Format_Layout{}, pattern, es.last);
            if (XY_UNLIKELY(count == -1)) return -1;
            return 2 + count * N + (N - 1) * es.split.size();
        }

        /// 解析数组类型格式串
        static xyu::size_t parse(const T* arr, const StringView& pattern, const StringView& expand)
        {
            if (pattern.empty()) return 2;
            auto es = Formatter<int>::parse_split(expand);
            xyu::size_t count = 0;
            for (xyu::size_t i = 0; i < N; ++i)
                count += __::call_formatter_parse<T>(arr+i, Format_Layout{}, pattern, es.last);
            return 2 + count + (N - 1) * es.split.size();
        }

        /// 运行期预解析数组类型格式串
        constexpr static xyu::size_t preparse(const T* arr, const StringView& pattern, const StringView& expand) noexcept
        {
            if (pattern.empty()) return 2;
            auto es = Formatter<int>::parse_split(expand);
            xyu::size_t count = 0;
            if constexpr (!__::is_formatter_runtime<T>)
                count += __::call_formatter_prepare<T>(Format_Layout{}, pattern, es.last) * N;
            else for (xyu::size_t i = 0; i < N; ++i)
                count += __::call_formatter_parse<T>(arr+i, Format_Layout{}, pattern, es.last);
            return 2 + count + (N - 1) * es.split.size();
        }

        /**
         * @brief 数组类型的格式化器特化
         * @note 可以控制每个元素的格式以及它们之间的分隔符。
         *
         * --- [ptn] (Pattern Specifier) ---
         * 用于控制每一个元素的 `pattern` 部分格式，直接将这部分内容传递给每一个元素
         *
         * --- [?ex] (Expand Specifier) - 扩展格式 ---
         * 用于控制元素间的分隔符以及指定元素的 `expand` 格式，由 '?' 引导。
         *
         * - **语法:** `"?<separator>,spec"`
         *
         * - **`<separator>` (分隔符) 部分:**
         *   - **默认:** 如果 `ex` 部分为空，或以逗号开头，分隔符默认为 `","`。
         *   - **自定义:** 逗号前的部分(没有则整个 `ex` 部分)被视为分隔符。
         *     - `"{? | }"` -> 分隔符为 `" | "`
         *   - **括号包裹:**
         *     - 使用括号包裹空字符串 `"{?()}"` 来指定空分隔符。
         *     - 括号内的内容即为分隔符，例如 `"{?(, )}"` 的分隔符是 `", "`。
         *
         * - **`spec` (元素扩展格式) 部分:**
         *   - 规则与 `ptn` 部分完全相同，直接将这部分内容传递给每一个元素的 `expand` 格式。
         */
        template <typename Stream>
        static void format(Stream& out, const T* arr, const StringView& pattern, const StringView& expand)
        {
            if (pattern.empty()) { out << "[]"; return; }
            auto es = Formatter<int>::parse_split(expand);
            out << '[';
            for (xyu::size_t i = 0;;) {
                __::call_formatter_format<T>(out, &arr[i], Format_Layout{}, pattern, es.last);
                if (++i == N) break;
                out << es.split;
            }
            out << ']';
        }
    };

    /// 引用封装
    template <typename T>
    struct Formatter<xyu::t_refer<T>> : Formatter<xyu::t_remove_cvref<T>> {};

    /**
     * @brief 动态类型封装
     * @details 无论 T 本身是否可以在编译期解析，均使用运行时解析
     * @tparam T 被封装的类型
     */
    template <typename T>
    struct Formatter<xyu::t_dynamic<T>> : Formatter<T> { static constexpr bool runtime = true; };

    /**
     * @brief 转发封装
     * @details 通过 transform 将原类型转换为 To 类型，并使用 To 类型进行格式化
     * @tparam From 原类型
     * @tparam To 转发后的类型
     * @example
     * template ˂typename From˃
     * struct Formatter˂From˃ : Formatter<xyu::typelist_c˂From, To˃
     * {
     *     static const To& transform(const From& ori) noexcept { ... }
     * };
     */
    template <typename From, typename To>
    struct Formatter<xyu::typelist_c<From, To>>
    {
        // 运行时解析
        static constexpr bool runtime = __::is_formatter_runtime<To>;

        /// 预解析转发
        template <typename Test = To, xyu::t_enable<__::get_formatter_prepare_type<Test> & 1, bool> = true>
        static constexpr xyu::size_t prepare(const Format_Layout& fl, const StringView& pattern, const StringView& expand) noexcept
        { return __::call_formatter_prepare<To>(fl, pattern, expand); }
        /// 预解析转发
        template <typename Test = To, xyu::t_enable<!(__::get_formatter_prepare_type<Test> & 1), bool> = false>
        static constexpr xyu::size_t prepare(const StringView& pattern, const StringView& expand) noexcept
        { return __::call_formatter_prepare<To>(Format_Layout{}, pattern, expand); }

        /// 解析转发
        template <typename Test = To, xyu::t_enable<__::get_formatter_parse_type<Test> & 1, bool> = true>
        static xyu::size_t parse(const From& ori, const Format_Layout& fl, const StringView& pattern, const StringView& expand)
        {
            auto&& val = transform(ori);
            return __::call_formatter_parse<To>(&val, fl, pattern, expand);
        }
        /// 解析转发
        template <typename Test = To, xyu::t_enable<!(__::get_formatter_parse_type<Test> & 1), bool> = false>
        static xyu::size_t parse(const From& ori, const StringView& pattern, const StringView& expand)
        {
            auto&& val = transform(ori);
            return __::call_formatter_parse<To>(&val, Format_Layout{}, pattern, expand);
        }

        /// 运行期预解析转发
        template <typename Test = To, xyu::t_enable<__::get_formatter_preparse_type<Test> & 1, bool> = true>
        static constexpr xyu::size_t preparse(const From& ori, const Format_Layout& fl, const StringView& pattern, const StringView& expand) noexcept
        {
            auto&& val = transform(ori);
            return __::call_formatter_preparse<To>(&val, fl, pattern, expand);
        }
        /// 运行期预解析转发
        template <typename Test = To, xyu::t_enable<!(__::get_formatter_preparse_type<Test> & 1), bool> = false>
        static constexpr xyu::size_t preparse(const From& ori, const StringView& pattern, const StringView& expand) noexcept
        {
            auto&& val = transform(ori);
            return __::call_formatter_preparse<To>(&val, Format_Layout{}, pattern, expand);
        }

        /// 格式化转发
        template <typename Stream, typename Test = To, xyu::t_enable<__::get_formatter_format_type<Stream, Test> & 1, bool> = true>
        static void format(Stream& out, const From& ori, const Format_Layout& fl, const StringView& pattern, const StringView& expand)
        {
            auto&& val = transform(ori);
            __::call_formatter_format<To, Stream>(out, &val, fl, pattern, expand);
        }
        /// 格式化转发
        template <typename Stream, typename Test = To, xyu::t_enable<!(__::get_formatter_format_type<Stream, Test> & 1), bool> = false>
        static void format(Stream& out, const From& ori, const StringView& pattern, const StringView& expand)
        {
            auto&& val = transform(ori);
            __::call_formatter_format<To, Stream>(out, &val, Format_Layout{}, pattern, expand);
        }

    private:
        // 值转发
        static decltype(auto) transform(const From& ori) noexcept
        { return Formatter<From>::transform(ori); }
    };
}

#pragma clang diagnostic pop