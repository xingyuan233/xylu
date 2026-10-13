#pragma once

/**
 * @file    xystring/fmtimpl.h
 * @brief   定义了基础类型的Format逻辑
 * @date    2026-03-05
 */

#include "./fmtbase.h"
#include "../xymath/bit.h"

/// 格式化异常
namespace xylu::xycore
{
    struct E_Format_Rep : E_Format
    {
        // precision 过大
        [[noreturn]] static void abort_precision_over_limit(xyu::StringView precision, ErrorPos) noexcept XY_COLD;

        // float 格式串非法
        [[noreturn]] static void abort_invalid_float_rep(xyu::StringView rep, ErrorPos) noexcept XY_COLD;
    };
}

/// 格式化辅助
namespace xylu::xystring
{
    /// represent 格式串 - 整数
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

        // 解析整数
        constexpr Format_Int& parse(StringView rep) noexcept
        {
            for (char c : rep.range())
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

    /// represent 格式串 - 布尔
    struct Format_Bool
    {
        bool is_str = true; // 是否输出字符串
        bool upper = false; // 是否输出大写

        // 解析bool
        constexpr Format_Bool& parse(StringView rep) noexcept
        {
            for (char c : rep.range())
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

    /// represent 格式串 - 浮点数
    struct Format_Float
    {
        // 转换符
        char specifier = 'g';
        // 代替形式
        bool alt = false;
        // 正数输出的符号类型
        enum { EMPTY = 0, POSITIVE, SPACE } ps_sign = EMPTY;
        // 精度索引 (sp->. op->数字后)
        xyu::uint sp = -1, op = -1;
        // 格式串
        const StringView* rep = nullptr;

        // 解析bool
        constexpr Format_Float& parse(StringView represent) noexcept
        {
            rep = &represent;
            for (int i = 0; i < represent.size(); ++i)
            {
                switch (represent[i])
                {
                    case '+': ps_sign = POSITIVE; break;
                    case '-': ps_sign = SPACE; break;
                    case '#': alt = true; break;
                    case '~': alt = false; break;
                    case 'f': case 'F': case 'e': case 'E':
                    case 'g': case 'G': case 'a': case 'A': specifier = represent[i]; break;
                    case '.': {
                        sp = i;
                        while (++i < represent.size() && represent[i] >= '0' && represent[i] <= '9');
                        op = i;
                        if (i != represent.size()) --i;
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
            for (auto p = sp + 1; p < op; ++p) precision = precision * 10 + (rep->get(p) - '0');
            return precision;
        }
    };
}

/// 常用类型特化
namespace xylu::xystring
{
    /**
     * @brief `FormatImpl` 的主模板，用于为特定类型 T 提供格式化能力。
     * @details
     *   要让一个自定义类型 MyType 支持格式化，需要为 `FormatImpl<MyType>` 提供一个特化版本。
     *
     *   一个完整的 `FormatImpl` 特化可以提供以下静态成员：
     *   - `runtime`(可选): 一个 bool 常量，若为 true，则强制该类型走运行期格式化路径。
     *   - `parse_compile`(可选): 编译期函数，用于预估格式化后所需的缓冲区大小。
     *   - `parse_remain` (可选): 运行期函数，用于精确计算格式化后的长度。
     *   - `parse_runtime`(可选): 运行期函数，用于预估动态参数的长度。
     *   - `format`(必须): 运行期函数，执行实际的格式化操作，将结果写入输出流。
     *
     * @see `fmt.h` 文件头部的 "扩展自定义类型" 部分获取详细的实现指南。
     * @tparam T 要被格式化的类型。
     * @tparam Check void (用于 SFINAE)
     */
    template <typename T, typename Check>
    struct FormatImpl;

    template <typename T, typename>
    struct FormatImpl
    {
        // 强制运行时解析类型 (字符串 或 范围类型 或 可构造范围类型)
        static constexpr bool runtime = __::get_string_type<T> > 0 || xyu::t_can_range<T>;

        /* 整数 */

        /// 编译期解析整数格式
        template <typename Test = T, xyu::t_enable<xyu::t_is_integer<Test>, char> = 'i'>
        constexpr static xyu::size_t parse_compile(Format_Int fi) noexcept
        {
            if (fi.base == Format_Int::CHAR) return 1;
            // 符号位
            xyu::size_t count = xyu::t_number<T>::is_signed || fi.ps_sign == Format_Int::POSITIVE;
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
        /// 编译期解析整数格式 (转发)
        template <typename Test = T, xyu::t_enable<xyu::t_is_integer<Test>, char> = 'i'>
        constexpr static xyu::size_t parse_compile(Format_Layout fl, StringView rep) noexcept
        {
            if (fl.width) return fl.width;
            return parse_compile(Format_Int{}.parse(rep));
        }

        /// 运行期解析整数格式
        template <typename Test = T, xyu::t_enable<xyu::t_is_integer<Test>, char> = 'i'>
        static xyu::size_t parse_runtime(const T& value, Format_Int fi) noexcept
        {
            //--- 字符 ----
            if (fi.base == Format_Int::CHAR) return 1;

            //---- 数字 ----

            //-- 符号 + 基数标识 --
            using U = xyu::t_wide<xyu::t_chg_sign2un<T>, xyu::uint>;
            U uv = value;                           // 去符号后的值
            bool np_sign = fi.ps_sign;              // 是否输出符号
            if constexpr (xyu::t_number<T>::is_signed)
                if (value < 0) uv = -uv, np_sign = true;
            xyu::size_t count = 2 * fi.base_sign + np_sign;  // 符号位 + 基数标识
            if (value == 0) return count + 1;

            //-- 数值 --
            xyu::uint bits = 8*sizeof(U) - xyu::bit_count_0_high(uv);
            switch (fi.base)
            {
                case Format_Int::HEX: count += (bits + 3) / 4; break;
                case Format_Int::OCT: count += (bits + 2) / 3; break;
                case Format_Int::BIN: {
                    if (XY_UNLIKELY(value == xyu::t_number<T>::min)) ++count; //-min 比 max 多一位
                    count += bits; break;
                }
                default: { // Format_Int::DEC
                    count += 1 + bits * 643L / 2136;  // (log10(2) -> 643/2136)
                    break;
                }
            }
            return count;
        }
        /// 运行期解析整数格式 (转发)
        template <typename Test = T, xyu::t_enable<xyu::t_is_integer<Test>, char> = 'i'>
        static xyu::size_t parse_runtime(const T& value, Format_Layout fl, StringView rep) noexcept
        {
            if (fl.width) return fl.width;
            return parse_runtime(value, Format_Int{}.parse(rep));
        }

        /// 格式化整数类型
        template <typename Stream, typename Test = T, xyu::t_enable<xyu::t_is_integer<Test>, char> = 'i'>
        static void format(Stream& out, const T& value, Format_Layout fl, Format_Int fi) noexcept(FormatCore::is_noexcept_write<Stream>)
        {
            //--- 字符 ----
            if (fi.base == Format_Int::CHAR) { out << static_cast<char>(value); return; }

            //---- 数字 ----

            //-- 符号 --
            using U = xyu::t_wide<xyu::t_chg_sign2un<T>, xyu::uint>;
            U uv = value;                           // 去符号后的值
            bool np_sign = fi.ps_sign;              // 是否输出符号
            int elen = 2 * fi.base_sign + np_sign;  // 额外长度 (符号位 + 基数标识)
            char sign = fi.ps_sign == Format_Int::POSITIVE ? '+' : ' '; // 符号字符
            if constexpr (xyu::t_number<T>::is_signed)
                if (value < 0) uv = -uv, np_sign = true, sign = '-';

            //-- 数值缓冲区 --
            bool autow = !fl.width;     // 是否未指定宽度
            constexpr xyu::uint nlen = 8*sizeof(T) + 3;
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
            switch (fl.align)
            {
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
        /**
         * @brief 格式化整数类型
         * @details
         * ### Represent Specifiers (`:rep`):
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
        template <typename Stream, typename Test = T, xyu::t_enable<xyu::t_is_integer<Test> && !xyu::t_is_same_noconst<Test, char>, char> = 'i'>
        static void format(Stream& out, const T& value, Format_Layout fl, StringView rep) noexcept(FormatCore::is_noexcept_write<Stream>)
        {
            if (rep.empty()) return format(out, value, fl);
            else return format(out, value, fl, Format_Int{}.parse(rep));
        }

        /// 无 rep 时的 10进制简化版
        template <typename Stream, typename Test = T, xyu::t_enable<xyu::t_is_integer<Test> && !xyu::t_is_same_noconst<Test, char>, char> = 'i'>
        static void format(Stream& out, const T& value, Format_Layout fl) noexcept(FormatCore::is_noexcept_write<Stream>)
        {
            //-- 符号 --
            using U = xyu::t_wide<xyu::t_chg_sign2un<T>, xyu::uint>;
            U uv = value;                           // 去符号后的值
            bool negtive = value < 0;
            if (negtive) uv = -uv;

            //-- 数值缓冲区 --
            constexpr xyu::uint mnlen = (8*sizeof(T)+2)/3 + 1;
            bool autow = !fl.width;     // 是否未指定宽度
            constexpr xyu::uint nlen = mnlen;
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
                    if (negtive) out.write('-');
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

        /* bool 类型 */

        /// 编译期解析布尔格式
        template <typename Test = T, xyu::t_enable<xyu::t_is_same_noconst<Test, bool>, char> = 'b'>
        constexpr static xyu::size_t parse_compile(Format_Bool fb) noexcept { return fb.is_str ? 5 : 1; }
        /// 编译期解析布尔格式 (转发)
        template <typename Test = T, xyu::t_enable<xyu::t_is_same_noconst<Test, bool>, char> = 'b'>
        constexpr static xyu::size_t parse_compile(Format_Layout fl, StringView rep) noexcept
        {
            if (fl.width) return fl.width;
            return parse_compile(Format_Bool{}.parse(rep));
        }

        /// 格式化布尔类型
        template <typename Stream, typename Test = T, xyu::t_enable<xyu::t_is_same_noconst<Test, bool>, char> = 'b'>
        static void format(Stream& out, const T& value, Format_Layout fl, Format_Bool fb) noexcept(FormatCore::is_noexcept_write<Stream>)
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
         * @brief 格式化布尔类型
         * @details
         * ### Represent Specifiers (`:rep`):
         * - 's'/'S': 输出 true/TRUE/false/FALSE
         * - 'b'/'B': 输出 1/0
         * - '#': 输出 字符串
         * - '~': 输出 0/1
         * @note 默认格式: 输出 true/false 字符串左对齐;数值右对齐
         * @attention width 过小会截断: 字符串会显示 f/t/F/T
         */
        template <typename Stream, typename Test = T, xyu::t_enable<xyu::t_is_same_noconst<Test, bool>, char> = 'b'>
        static void format(Stream& out, const T& value, Format_Layout fl, StringView rep) noexcept(FormatCore::is_noexcept_write<Stream>)
        { return format(out, value, fl, Format_Bool{}.parse(rep)); }

        /* 浮点型 */

        /// 编译期解析浮点格式
        template <typename Test = T, xyu::t_enable<xyu::t_is_floating<Test>, char> = 'f'>
        constexpr static xyu::size_t parse_compile(Format_Layout fl, Format_Float ff) noexcept
        { return xyu::max(8 + sizeof(T) + ff.precision(), fl.width); }
        /// 编译期解析浮点格式
        template <typename Test = T, xyu::t_enable<xyu::t_is_floating<Test>, char> = 'f'>
        constexpr static xyu::size_t parse_compile(Format_Layout fl, StringView rep) noexcept
        {
            xyu::uint precision = 6;    // 精度

            xyu::size_t i = rep.find('.');
            if (i != -1) {
                precision = 0;
                while (++i < rep.size()) {
                    char c = rep[i];
                    if (c < '0' || c > '9') break;
                    precision = precision * 10 + c - '0';
                }
            }
            return xyu::max(8 + sizeof(T) + precision, fl.width);
        }

        /// 运行期解析浮点格式
        template <typename Test = T, xyu::t_enable<xyu::t_is_floating<Test>, char> = 'f'>
        static xyu::size_t parse_runtime(const T& value, Format_Layout fl, Format_Float ff) noexcept
        {
            xyu::uint plen = ff.op - ff.sp;
            if constexpr (!XY_RELEASE) if (XY_UNLIKELY(plen > 3))
                xyu::E_Format_Rep::abort_precision_over_limit(ff.rep->subview(ff.sp + 1, plen - 1),
                                                              XY_CurPos("FormatImpl::parse_runtime"));
            //---- 分配格式化空间 ----
            char sfmt[64];
            //---- 构造格式化字符串 ----
            char* p = sfmt;
            *p++ = '%';
            // 替代形式
            if (ff.alt) *p++ = '#';
            // 精度
            if (ff.sp != -1) {
                xyu::mem_copy(p, ff.rep->data() + ff.sp, plen);
                p += plen;
            }
            // 长
            if constexpr (sizeof(T) > sizeof(double)) *p++ = 'L';
            // 转换符
            *p++ = ff.specifier;
            *p = '\0';

            //---- 格式化长度 ----
            bool ps_sign = ff.ps_sign != Format_Float::EMPTY && value > 0;
            int need = __builtin_snprintf(nullptr, 0, sfmt, value);
            if (XY_UNLIKELY(need < 0)) xyu::E_Format_Rep::abort_invalid_float_rep(*ff.rep, XY_CurPos("FormatImpl::parse_runtime"));
            return xyu::max(need + ps_sign, fl.width);
        }
        /// 运行期解析浮点格式 (转发)
        template <typename Test = T, xyu::t_enable<xyu::t_is_floating<Test>, char> = 'f'>
        static xyu::size_t parse_runtime(const T& value, Format_Layout fl, StringView rep) noexcept
        { return parse_runtime(value, fl, Format_Float{}.parse(rep)); }

        /// 格式化浮点类型
        template <typename Stream, typename Test = T, xyu::t_enable<xyu::t_is_floating<Test>, char> = 'f'>
        static void format(Stream& out, const T& value, Format_Layout fl, Format_Float ff) noexcept(FormatCore::is_noexcept_write<Stream>)
        {
            xyu::uint plen = ff.op - ff.sp;

            //---- 分配格式化空间 ----
            char sfmt[64];
            //---- 构造格式化字符串 ----
            char* p = sfmt;
            *p++ = '%';
            // 替代形式
            if (ff.alt) *p++ = '#';
            // 精度
            if (ff.sp != -1) xyu::mem_copy(p, ff.rep->data() + ff.sp, plen), p += plen;
            // 长
            if constexpr (xyu::t_is_same_noconst<T, long double>) *p++ = 'L';
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
            if (flen <= 0)
            {
                if (ps_sign) out << (ff.ps_sign == Format_Float::POSITIVE ? '+' : ' ');
                out << buf.view();

            }
            else switch (fl.align)
            {
                case Format_Layout::LEFT: {
                    if (ps_sign) out << (ff.ps_sign == Format_Float::POSITIVE ? '+' : ' ');
                    out.write(fl.fill, flen);
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
                    out.write(fl.fill, flen);
                    if (ps_sign) out << (ff.ps_sign == Format_Float::POSITIVE ? '+' : ' ');
                    out << buf.view();
                    break;
                }
            }
        }
        /**
         * @brief 格式化浮点类型
         * @details
         * ### Represent Specifiers (`:rep`):
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
        static void format(Stream& out, const T& value, Format_Layout fl, StringView rep) noexcept(FormatCore::is_noexcept_write<Stream>)
        { return format(out, value, fl, Format_Float{}.parse(rep)); }

        /* 字符型 */

        /// 编译期解析字符格式
        template <typename Test = T, xyu::t_enable<xyu::t_is_same_noconst<Test, char>, char> = 'c'>
        constexpr static xyu::size_t parse_compile(StringView rep) noexcept
        {
            if (rep.empty()) return 1;
            return FormatImpl<unsigned char>::parse_compile(Format_Layout{}, rep);
        }

        /// 运行期解析字符格式
        template <typename Test = T, xyu::t_enable<xyu::t_is_same_noconst<Test, char>, char> = 'c'>
        static xyu::size_t parse_runtime(const T& value, StringView rep) noexcept
        {
            if (rep.empty()) return 1;
            return FormatImpl<unsigned char>::parse_runtime(value, Format_Layout{}, rep);
        }

        /**
         * @brief 格式化字符型
         * @note 默认输出字符，若提供 rep 选项，则作为整数解析
         */
        template <typename Stream, typename Test = T, xyu::t_enable<xyu::t_is_same_noconst<Test, char>, char> = 'c'>
        static void format(Stream& out, const T& value, StringView rep) noexcept(FormatCore::is_noexcept_write<Stream>)
        {
            if (rep.empty()) { out << value; return; }
            return FormatImpl<unsigned char>::format(out, value, Format_Layout{}, rep);
        }

        /* 字符串类型 */

        /// 运行期解析字符串格式
        template <typename Test = T, xyu::t_enable<__::get_string_type<Test>, char> = 's'>
        static xyu::size_t parse_runtime(const T& str, Format_Layout fl) noexcept
        { return fl.width ? fl.width : StringView{str}.size(); }

        /**
         * @brief 格式化字符串
         * @note 默认左对齐
         * @attention width 过小会根据 align 截断 (如: 左对齐就截断右边)
         */
        template <typename Stream, typename Test = T, xyu::t_enable<__::get_string_type<Test>, char> = 's'>
        static void format(Stream& out, const T& s, Format_Layout fl) noexcept(FormatCore::is_noexcept_write<Stream>)
        {
            StringView str{s};
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

        /// 编译期解析指针格式
        template <typename Test = T, xyu::t_enable<xyu::t_is_pointer<Test> && __::get_string_type<Test> == 0, char> = 'p'>
        constexpr xyu::size_t parse_compile(Format_Layout fl, StringView rep) noexcept
        {
            if (fl.width) return fl.width;
            Format_Int fi;
            fi.base = Format_Int::HEX;
            fi.base_sign = true;
            using U = xyu::t_chg_sign2un<xyu::diff_t>;
            return FormatImpl<U>::parse_compile(fi.parse(rep));
        }

        /// 运行期解析指针格式
        template <typename Test = T, xyu::t_enable<xyu::t_is_pointer<Test> && __::get_string_type<Test> == 0, char> = 'p'>
        static xyu::size_t parse_runtime(const T& ptr, Format_Layout fl, StringView rep) noexcept
        {
            if (fl.width) return fl.width;
            Format_Int fi;
            fi.base = Format_Int::HEX;
            fi.base_sign = true;
            using U = xyu::t_chg_sign2un<xyu::diff_t>;
            return FormatImpl<U>::parse_runtime(reinterpret_cast<U>(ptr), fi.parse(rep));
        }

        /**
         * @brief 格式化指针型
         * @details 格式化参数与整数相同
         * @note 默认格式: 输出指针值，十六进制，基数标志，右对齐
         */
        template <typename Stream, typename Test = T, xyu::t_enable<xyu::t_is_pointer<Test> && __::get_string_type<Test> == 0, char> = 'p'>
        static void format(Stream& out, const T& ptr, Format_Layout fl, StringView rep) noexcept(FormatCore::is_noexcept_write<Stream>)
        {
            Format_Int fi;
            fi.base = Format_Int::HEX;
            fi.base_sign = true;
            using U = xyu::t_chg_sign2un<xyu::diff_t>;
            FormatImpl<U>::format(out, reinterpret_cast<U>(ptr), fl, fi.parse(rep));
        }

        /* 范围类型 */

        /// 运行期解析范围格式
        template <typename Test = T, xyu::t_enable<xyu::t_can_range<const Test&> && __::get_string_type<Test> == 0, char> = 'r'>
        static xyu::size_t parse_runtime(const T& rg, StringView rep, StringView ex) noexcept(noexcept(FormatCore::call_parse_runtime<xyu::t_rem_cr<xyu::t_access<const T&>>>({},{},{},{})))
        {
            xyu::size_t count = 2;
            auto range = xyu::make_range(rg);
            if constexpr (XY_DEBUG) if (XY_UNLIKELY(!range.unlimited()))
                xyu::E_Logic::abort_range_unlimited(XY_CurPos("FormatImpl::parse_runtime"));
            if (range.empty() || range.indefinite()) return count;
            auto es = parse_split(ex);
            for (auto&& e : range)
            {
                auto add = FormatCore::call_parse_runtime<xyu::t_rem_cr<decltype(e)>>(&e, Format_Layout{}, rep, es.last);
                count += add;
                if constexpr (XY_DEBUG) if (XY_UNLIKELY(count > FormatCore::limit))
                    xyu::E_Format::abort_reserve_over_limit(count, XY_CurPos("FormatImpl::parse_runtime"));
            }
            return check_total(count, es.split.size(), range.count() - 1, XY_CurPos("FormatImpl::parse_runtime"));
        }

        /// 运行期解析剩余范围格式
        template <typename Test = T, xyu::t_enable<xyu::t_can_range<const Test&> && __::get_string_type<Test> == 0, char> = 'r'>
        static xyu::size_t parse_remain(const T& rg, StringView rep, StringView ex) noexcept(noexcept(FormatCore::call_parse_remain<xyu::t_rem_cr<xyu::t_access<const T&>>>({},{},{},{})))
        {
            xyu::size_t count = 2;
            auto range = xyu::make_range(rg);
            if constexpr (XY_DEBUG) if (XY_UNLIKELY(range.unlimited()))
                xyu::E_Logic::abort_range_unlimited(XY_CurPos("FormatImpl::parse_remain"));
            if (range.empty() || range.indefinite()) return count;
            auto es = parse_split(ex);
            using U = xyu::t_rem_cr<decltype(*range.begin())>;
            if constexpr (!FormatCore::is_runtime<U>) 
            {
                xyu::size_t one = FormatCore::call_parse_compile<U>({},rep,es.last);
                count = check_total(count, one, range.count(), XY_CurPos("FormatImpl::parse_remain"));
            } 
            else for (auto&& e : range)
            {
                auto add = FormatCore::call_parse_remain<xyu::t_rem_cr<decltype(e)>>(&e, Format_Layout{}, rep, es.last);
                count += add;
                if constexpr (XY_DEBUG) if (XY_UNLIKELY(count > FormatCore::limit))
                    xyu::E_Format::abort_reserve_over_limit(count, XY_CurPos("FormatImpl::parse_remain"));
            }
            return check_total(count, es.split.size(), range.count() - 1, XY_CurPos("FormatImpl::parse_remain"));
        }

        /**
         * @brief 格式化范围类型
         * @note 可以控制每个元素的格式以及它们之间的分隔符。
         *
         * --- [rep] (Represent Specifier) ---
         * 用于控制每一个元素的 `represent` 部分格式，直接将这部分内容传递给每一个元素
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
         *   - 规则与 `rep` 部分完全相同，直接将这部分内容传递给每一个元素的 `expand` 格式。
         */
        template <typename Stream, typename Test = T, xyu::t_enable<xyu::t_can_range<const Test&> && __::get_string_type<Test> == 0, char> = 'r'>
        static void format(Stream& out, const T& rg, StringView rep, StringView ex) noexcept(noexcept(FormatCore::call_format<xyu::t_rem_cr<xyu::t_access<const T&>>>(out,{},{},{},{})))
        {
            auto range = xyu::make_range(rg);
            if constexpr (XY_DEBUG) if (XY_UNLIKELY(range.unlimited()))
                xyu::E_Logic::abort_range_unlimited(XY_CurPos("FormatImpl::format"));
            if (range.empty()) { out << "[]"; return; }
            auto es = parse_split(ex);
            out << '[';
            auto i = range.begin();
            auto j = range.end();
            for(;;)
            {
                auto &&e = *i;
                FormatCore::call_format<xyu::t_rem_cr<decltype(*i)>>(out, &e, Format_Layout{}, rep, es.last);
                if (++i == j) break;
                out << es.split;
            }
            out << ']';
        }

    public:
        // 获取分隔字符串与后续格式串
        // "" -> split = ",", last = ""
        // "a" → split = "a", last = ""
        // "a,b" → split = "a", last = "b"
        // ",b" → split = "", last = "b"
        // "(a,b),c" → split = "a,b", last = "c"
        // "(a,c" → split = "(a", last = "c"
        static constexpr char split_sign = ',';
        static constexpr StringView split_empty = ",";
        struct spec_split { StringView split, last; };
        constexpr static spec_split parse_split(StringView spec) noexcept
        {
            // 空格式串
            if (spec.empty()) return { split_empty, {} };
            // 括号包裹
            xyu::size_t i{}, j{};
            if (spec[0] == '(')
            {
                for(xyu::size_t s = 1, depth = 1;;)
                {
                    i = spec.find(')', s);
                    if (XY_UNLIKELY(i == -1)) return { spec, {} };
                    --depth;
                    while ((j = spec.find('(', s, i)) != -1) ++depth, s = j + 1;
                    if (depth == 0) break;
                    s = i + 1;
                }
                j = spec.find(split_sign, i + 1);
                if (j == -1) return { spec.subview(1, i - 1), {} };
                else return { spec.subview(1, i - 1), spec.subview(j + 1) };
            }
            // 直接
            else
            {
                i = spec.find(split_sign);
                if (i == -1) return { spec, {} };
                return { spec.subview(0, i), spec.subview(i + 1) };
            }
        }

        // total + add * count
        constexpr static xyu::size_t check_total(xyu::size_t total, xyu::size_t add, xyu::size_t count, xyu::ErrorPos pos) noexcept
        {
            xyu::size_t tmp{};
            if (XY_UNLIKELY(add > FormatCore::limit))
            {
                if (xyu::t_constant()) return -1;
                xyu::E_Format::abort_reserve_over_limit(add, pos);
            }
            if constexpr (XY_DEBUG)
            {
                if (XY_UNLIKELY(xyu::calc_of_mul(add, count, tmp)))
                {
                    if (xyu::t_constant()) return -1;
                    xyu::E_Format::abort_reserve_over_limit(-1, pos);
                }
                if (XY_UNLIKELY(tmp > FormatCore::limit))
                {
                    if (xyu::t_constant()) return -1;
                    xyu::E_Format::abort_reserve_over_limit(tmp, pos);
                }
            }
            else tmp = add * count;

            xyu::size_t ret{};
            if constexpr (XY_DEBUG)
            {
                if (XY_UNLIKELY(xyu::calc_of_add(total, tmp, ret)))
                {
                    if (xyu::t_constant()) return -1;
                    xyu::E_Format::abort_reserve_over_limit(-1, pos);
                }
                if (XY_UNLIKELY(ret > FormatCore::limit))
                {
                    if (xyu::t_constant()) return -1;
                    xyu::E_Format::abort_reserve_over_limit(ret, pos);
                }
            }
            else ret = total + tmp;

            return ret;
        }
    };

    /// long double 类型
    // 部分平台无法正常格式化，转为 double
    template <>
    struct FormatImpl<long double> : FormatImpl<double> {};

    /// nullptr 类型
    template <>
    struct FormatImpl<xyu::nullptr_t>
    {
        /// 编译期解析
        constexpr static xyu::size_t parse_compile() noexcept { return 7; }
        /// 格式化
        template <typename Stream>
        static void format(Stream& out, const xyu::nullptr_t&) noexcept(FormatCore::is_noexcept_write<Stream>)
        { out << "nullptr"; }
    };

    /// 字符数组
    template <typename T, xyu::size_t N>
    struct FormatImpl<T[N], xyu::t_enable<xyu::t_is_same_noconst<T, char>>>
    {
        /// 编译期解析字符数组格式
        constexpr static xyu::size_t parse_compile(Format_Layout fl) noexcept
        { return fl.width ? fl.width : (N > 0 ? N - 1: 0); }

        /// 格式化字符数组
        template <typename Stream>
        static void format(Stream& out, const T* arr, Format_Layout fl) noexcept(FormatCore::is_noexcept_write<Stream>)
        { FormatImpl<StringView>::format(out, StringView{arr, (N > 0 ? N - 1: 0)}, fl); }
    };

    /// 数组类型
    template <typename T, xyu::size_t N>
    struct FormatImpl<T[N], xyu::t_enable<!xyu::t_is_same_noconst<T, char>>>
    {
        // 运行时解析
        static constexpr bool runtime = FormatCore::is_runtime<T>;

        /// 编译期解析数组格式
        template <bool R = runtime, typename = xyu::t_enable<!R>>
        constexpr static xyu::size_t parse_compile(StringView rep, StringView ex) noexcept
        {
            if constexpr (N == 0) return 2;
            auto es = FormatImpl<int>::parse_split(ex);
            xyu::size_t count =  FormatCore::call_parse_compile<T>(Format_Layout{}, rep, es.last);
            count = FormatImpl<int>::check_total(2, count, N, XY_CurPos("FormatImpl::parse_compile"));
            return FormatImpl<int>::check_total(count, es.split.size(), N - 1, XY_CurPos("FormatImpl::parse_compile"));
        }

        /// 运行期解析数组格式
        static xyu::size_t parse_runtime(const T* arr, StringView rep, StringView ex) noexcept(noexcept(FormatCore::call_parse_runtime<T>(arr,{},{},{})))
        {
            if constexpr (N == 0) return 2;
            else
            {
                auto es = FormatImpl<int>::parse_split(ex);
                xyu::size_t count = 0;
                for (xyu::size_t i = 0; i < N; ++i)
                {
                    auto add = FormatCore::call_parse_runtime<T>(arr+i, Format_Layout{}, rep, es.last);
                    count += add;
                    if constexpr (XY_DEBUG) if (XY_UNLIKELY(count > FormatCore::limit))
                        xyu::E_Format::abort_reserve_over_limit(count, XY_CurPos("FormatImpl::parse_runtime"));
                }
                return FormatImpl<int>::check_total(2 + count, es.split.size(), N - 1, XY_CurPos("FormatImpl::parse_runtime"));
            }
        }

        /// 运行期解析剩余数组格式
        static xyu::size_t parse_remain(const T* arr, StringView rep, StringView ex) noexcept(noexcept(FormatCore::call_parse_remain<T>(arr,{},{},{})))
        {
            if constexpr (N == 0) return 2;
            else
            {
                auto es = FormatImpl<int>::parse_split(ex);
                xyu::size_t count = 0;
                for (xyu::size_t i = 0; i < N; ++i)
                {
                    auto add = FormatCore::call_parse_remain<T>(arr+i, Format_Layout{}, rep, es.last);
                    count += add;
                    if constexpr (XY_DEBUG) if (XY_UNLIKELY(count > FormatCore::limit))
                        xyu::E_Format::abort_reserve_over_limit(count, XY_CurPos("FormatImpl::parse_remain"));
                }
                return FormatImpl<int>::check_total(2 + count, es.split.size(), N - 1, XY_CurPos("FormatImpl::parse_remain"));
            }
        }

        /**
         * @brief 数组类型的格式化器特化
         * @note 可以控制每个元素的格式以及它们之间的分隔符。
         *
         * --- [rep] (Represent Specifier) ---
         * 用于控制每一个元素的 `represent` 部分格式，直接将这部分内容传递给每一个元素
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
         *   - 规则与 `rep` 部分完全相同，直接将这部分内容传递给每一个元素的 `expand` 格式。
         */
        template <typename Stream>
        static void format(Stream& out, const T* arr, StringView rep, StringView ex) noexcept(noexcept(FormatCore::call_format<T>(out,arr,{},{},{})))
        {
            if constexpr (N == 0) out << "[]";
            else
            {
                auto es = FormatImpl<int>::parse_split(ex);
                out << '[';
                for (xyu::size_t i = 0;;) 
                {
                    FormatCore::call_format<T>(out, &arr[i], Format_Layout{}, rep, es.last);
                    if (++i == N) break;
                    out << es.split;
                }
                out << ']';
            }
        }
    };
}

/// 动态封装
namespace xylu::xystring
{
    /**
     * @brief 一个类型包装器，用于强制其关联的 `FormatImpl` 走运行期解析路径。
     * @tparam T 被包装的类型。
     */
    template <typename T>
    struct DynamicVal : xyu::TagVal<T>
    {
        using xyu::TagVal<T>::TagVal;
        constexpr operator const T&() const noexcept { return this->get(); }
    };

    // 推导指引
    template <typename T>
    DynamicVal(T) -> DynamicVal<T>;

    /// 创建动态封装
    template <typename T>
    constexpr DynamicVal<T> make_dynamic(T&& v) noexcept { return DynamicVal<T>{v}; }

    /**
     * @brief 动态类型封装
     * @details 无论 T 本身是否可以在编译期解析，均使用运行时解析
     * @tparam T 被封装的类型
     */
    template <typename T>
    struct FormatImpl<DynamicVal<T>> : FormatImpl<T> { static constexpr bool runtime = true; };
}

/// 转发封装
namespace xylu::xystring
{
    /**
     * @brief 转发封装
     * @details 通过 transform 将原类型转换为 To 类型，并使用 To 类型进行格式化
     * @tparam From 原类型
     * @tparam To 转发后的类型
     * @par
     * @code
     * template ˂typename From˃
     * struct FormatImpl˂From˃ : FormatImpl˂xyu::typelist_c˂From, To˃˃
     * {
     *     static const To& transform(const From& ori) noexcept;
     * };
     * @endcode
     */
    template <typename From, typename To>
    struct FormatImpl<xyu::typelist_c<From, To>>
    {
        // 运行时解析
        static constexpr bool runtime = FormatCore::is_runtime<To>;

        /// 编译期解析转发
        template <typename Test = To, bool R = runtime, xyu::t_enable<__::formatimpl_parse_compile_layout<Test> && !R, bool> = true>
        static constexpr xyu::size_t parse_compile(Format_Layout fl, StringView rep, StringView ex) noexcept
        { return FormatCore::call_parse_compile<To>(fl, rep, ex); }
        /// 编译期解析转发
        template <typename Test = To, bool R = runtime, xyu::t_enable<!__::formatimpl_parse_compile_layout<Test> && !R, bool> = false>
        static constexpr xyu::size_t parse_compile(StringView rep, StringView ex) noexcept
        { return FormatCore::call_parse_compile<To>({}, rep, ex); }

        /// 运行期解析转发
        template <typename Test = To, xyu::t_enable<__::formatimpl_parse_runtime_layout<Test>, bool> = true>
        static xyu::size_t parse_runtime(const From& ori, Format_Layout fl, StringView rep, StringView ex)
        noexcept(is_noexcept_transform && noexcept(FormatCore::call_parse_runtime<To>({},{},{},{})))
        {
            auto&& val = transform(ori);
            return FormatCore::call_parse_runtime<To>(&val, fl, rep, ex);
        }
        /// 运行期解解析转发
        template <typename Test = To, xyu::t_enable<!__::formatimpl_parse_runtime_layout<Test>, bool> = false>
        static xyu::size_t parse_runtime(const From& ori, StringView rep, StringView ex)
        noexcept(is_noexcept_transform && noexcept(FormatCore::call_parse_runtime<To>({},{},{},{})))
        {
            auto&& val = transform(ori);
            return FormatCore::call_parse_runtime<To>(&val, {}, rep, ex);
        }

        /// 运行期解析剩余转发
        template <typename Test = To, xyu::t_enable<__::formatimpl_parse_remain_layout<Test>, bool> = true>
        static xyu::size_t parse_remain(const From& ori, Format_Layout fl, StringView rep, StringView ex) noexcept(is_noexcept_transform && noexcept(FormatCore::call_parse_remain<To>({},{},{},{})))
        {
            auto&& val = transform(ori);
            return FormatCore::call_parse_remain<To>(&val, fl, rep, ex);
        }
        /// 运行期解析剩余转发
        template <typename Test = To, xyu::t_enable<!__::formatimpl_parse_remain_layout<Test>, bool> = false>
        static xyu::size_t parse_remain(const From& ori, StringView rep, StringView ex) noexcept(is_noexcept_transform && noexcept(FormatCore::call_parse_remain<To>({},{},{},{})))
        {
            auto&& val = transform(ori);
            return FormatCore::call_parse_remain<To>(&val, Format_Layout{}, rep, ex);
        }

        /// 格式化转发
        template <typename Stream, typename Test = To, xyu::t_enable<__::formatimpl_format_layout<Stream, Test>, bool> = true>
        static void format(Stream& out, const From& ori, Format_Layout fl, StringView rep, StringView ex) noexcept(is_noexcept_transform && noexcept(FormatCore::call_format<To>(out,{},{},{},{})))
        {
            auto&& val = transform(ori);
            FormatCore::call_format<To>(out, &val, fl, rep, ex);
        }
        /// 格式化转发
        template <typename Stream, typename Test = To, xyu::t_enable<!__::formatimpl_format_layout<Stream, Test>, bool> = false>
        static void format(Stream& out, const From& ori, StringView rep, StringView ex) noexcept(is_noexcept_transform && noexcept(FormatCore::call_format<To>(out,{},{},{},{})))
        {
            auto&& val = transform(ori);
            FormatCore::call_format<To>(out, &val, Format_Layout{}, rep, ex);
        }

    private:
        // 值转发
        static decltype(auto) transform(const From& ori) noexcept(is_noexcept_transform)
        { return FormatImpl<From>::transform(ori); }

        // 异常
        static constexpr bool is_noexcept_transform = noexcept(static_cast<const To&>((FormatImpl<From>::transform(xyu::t_val<const From&>()))));
    };
}
