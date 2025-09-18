#pragma clang diagnostic push
#pragma ide diagnostic ignored "OCUnusedGlobalDeclarationInspection"
#pragma ide diagnostic ignored "UnusedValue"
#pragma once

#include "./time.h"
#include "../../link/format"

/* 时间格式化 */

namespace xylu::xystring
{
    /**
     * @brief `xylu::xysystem::Calendar` 的格式化器特化
     * @note 提供了丰富的日期和时间格式化选项，兼容 strftime 的大部分常用占位符。
     *
     * --- [ptn] (Pattern Specifier) - 日期和时间格式 ---
     *   - **默认格式:** 如果 `pattern` 为空，则默认为 `"%C"` (等同于 `"%Y-%m-%d %H:%M:%S"`)。
     *
     *   - **组合占位符 (Compound Placeholders):**
     *     - `%F`: ISO 8601 日期格式 (等同于 `%Y-%m-%d`)。
     *     - `%T`: ISO 8601 时间格式 (等同于 `%H:%M:%S`)。
     *     - `%C`: 简洁的日期时间格式 (等同于 `%Y-%m-%d %H:%M:%S`)。
     *     - `%c`: ISO 8601 完整日期时间格式 (等同于 `%Y-%m-%d %H:%M:%S.%f`)。
     *
     *   - **年份 (Year):**
     *     - `%Y`: 四位数年份 (e.g., 2024)。
     *     - `%y`: 两位数年份 [00-99] (e.g., 24)。
     *
     *   - **月份 (Month):**
     *     - `%m`: 两位数月份 [01-12]。
     *     - `%b`: 月份的英文缩写 (e.g., Jan, Feb)。
     *     - `%B`: 月份的英文全称 (e.g., January, February)。
     *
     *   - **日期 (Day):**
     *     - `%d`: 两位数的日期 [01-31]。
     *     - `%j`: 一年中的第几天 [001-366]。
     *
     *   - **星期 (Weekday):**
     *     - `%a`: 星期几的英文缩写 (e.g., Mon, Tue)。
     *     - `%A`: 星期几的英文全称 (e.g., Monday, Tuesday)。
     *     - `%w`: 星期几的数字表示 [0-6]，其中 0 代表星期一。
     *
     *   - **周数与季度 (Week & Quarter):**
     *     - `%W`: 一年中的第几周 [01-53]。
     *     - `%q`: 季度 [1-4]。
     *
     *   - **时间 (Time):**
     *     - `%H`: 小时 (24小时制) [00-23]。
     *     - `%I`: 小时 (12小时制) [01-12]。
     *     - `%p`: 上午/下午的大写表示 (AM/PM)。
     *     - `%P`: 上午/下午的小写表示 (am/pm)。
     *     - `%M`: 分钟 [00-59]。
     *     - `%S`: 秒 [00-59]。
     *
     *   - **亚秒级与时区 (Sub-second & Timezone):**
     *     - `%f`: 毫秒 [000-999] (基于Calendar内部的特殊存储，会转换为3位)。
     *     - `%s`: 自 Epoch (1970-01-01) 以来的秒数。
     *     - `%z`: UTC 时差 (e.g., +08.0)，基于 `xyu::K_TIME_DIFFERENCE` 配置。
     *
     *   - **转义 (Escape):**
     *     - `%%`: 输出一个百分号 '%'。
     *
     * --- [?ex] (Expand Specifier) - 扩展格式 ---
     *   用于控制 `%B` (月份全称) 和 `%A` (星期全称) 的对齐方式。
     *   - `<`: 左对齐 (e.g., `"{:%B?^}"` 会使月份名称在9个字符宽度内居中)。
     *   - `>`: 右对齐。
     *   - `^`: 居中对齐。
     *   - `-`: 默认 (不作填充)。
     */
    template <>
    struct Formatter<xylu::xysystem::Calendar>
    {
        /// 预解析日期格式串
        constexpr static xyu::size_t prepare(const StringView& pattern) noexcept
        {
            xyu::size_t count = 0;
            StringView ptn = pattern.empty() ? xylu::xysystem::Calendar::DefaultFormat : pattern;
            for (xyu::size_t i = 0; i < ptn.size(); ++i)
            {
                // 占位符
                if (ptn.get(i) == '%')
                {
                    if (++i == ptn.size()) { ++count; break; }
                    switch (ptn.get(i))
                    {
                        case 'q': case 'w': case '%': count += 1; break;
                        case 'y': case 'm': case 'd': case 'W': case 'H': case 'I':
                        case 'p': case 'P': case 'M': case 'S': count += 2; break;
                        case 'b': case 'j': case 'a': case 'f': count += 3; break;
                        case 'Y': count += 4; break;
                        case 'z': count += 5; break;
                        case 'T': count += 8; break;
                        case 'B': case 'A': count += 9; break;
                        case 's': case 'F': count += 10; break;
                        case 'C': count += 19; break;
                        case 'c': count += 23; break;
                        default: count += 2; break;
                    }
                }
                // 非占位符
                else ++count;
            }
            return count;
        }

        /// 格式化日期
        template <typename Stream>
        static void format(Stream& out, const xylu::xysystem::Calendar& c, const StringView& pattern, const StringView& expand)
        {
            // 解析 expand
            auto exalign = Format_Layout::DEFAULT;
            for (char e : expand.range())
            {
                switch (e)
                {
                    case '<': exalign = Format_Layout::LEFT; break;
                    case '>': exalign = Format_Layout::RIGHT; break;
                    case '^': exalign = Format_Layout::CENTER; break;
                    case '-': exalign = Format_Layout::DEFAULT; break;
                    default:;
                }
            }

            // 解析 pattern
            StringView ptn = pattern.empty() ? xylu::xysystem::Calendar::DefaultFormat : pattern;

            using Fu8   =   Formatter<xyu::uint8>;
            using Fu16  =   Formatter<xyu::uint16>;
            using Fu32  =   Formatter<xyu::uint32>;
            using Fs    =   Formatter<StringView>;
            using Ff    =   Formatter<float>;
            auto parse = [&](char f)
            {
                switch (f)
                {
                    // 年份(4位) [0000,9999]
                    case 'Y': Fu16::format(out, c.year, Format_Layout{4, '0'}); break;
                    // 年份(2位) [00,99]
                    case 'y': Fu8::format(out, c.year, Format_Layout{2, '0'}); break;
                    // 月份(2位) [01,12]
                    case 'm': Fu8::format(out, c.month, Format_Layout{2, '0'}); break;
                    // 月份(英文缩写) [Jan,Dec]
                    case 'b': out << xylu::xysystem::Calendar::str_month_abbr[c.month - 1]; break;
                    // 月份(英文全称) [January,December]
                    case 'B': {
                        if (exalign == Format_Layout::DEFAULT) out << xylu::xysystem::Calendar::str_month_full[c.month - 1];
                        else Fs::format(out, xylu::xysystem::Calendar::str_month_full[c.month - 1], Format_Layout{9, exalign});
                        break;
                    }
                    // 日(2位) [01,31]
                    case 'd': Fu8::format(out, c.day, Format_Layout{2, '0'}); break;
                    // 一年中的第几天(3位) [001,366]
                    case 'j': Fu16::format(out, c.year_day(), Format_Layout{3, '0'}); break;
                    // 季度(1位) [1,4]
                    case 'q': Fu8::format(out, (c.month + 2) / 3, {}); break;
                    // 星期几(英文缩写) [Mon,Sun]
                    case 'a': out << xylu::xysystem::Calendar::str_week_abbr[c.week_day()]; break;
                    // 星期几(英文全称) [Monday,Sunday]
                    case 'A': {
                        if (exalign == Format_Layout::DEFAULT) out << xylu::xysystem::Calendar::str_week_full[c.week_day()];
                        else Fs::format(out, xylu::xysystem::Calendar::str_week_full[c.week_day()], Format_Layout{9, exalign});
                        break;
                    }
                    // 星期几(1位) [0,6] (0代表星期一)
                    case 'w': Fu8::format(out, c.week_day(), Format_Layout{1, '0'}); break;
                    // 一年中的第几周(2位) [01,53]
                    case 'W': Fu16::format(out, (c.year_day() - 1) / 7 + 1, Format_Layout{2, '0'}); break;
                    // 时(24小时制) [00,23]
                    case 'H': Fu8::format(out, c.hour, Format_Layout{2, '0'}); break;
                    // 时(12小时制) [01,12]
                    case 'I': Fu8::format(out, c.hour % 12, Format_Layout{2, '0'}); break;
                    // 上午/下午(2位) {AM,PM}
                    case 'p': out << (c.hour < 12? "AM" : "PM"); break;
                    // 上午/下午(2位) {am,pm}
                    case 'P': out << (c.hour < 12? "am" : "pm"); break;
                    // 分(2位) [00,59]
                    case 'M': Fu8::format(out, c.minute, Format_Layout{2, '0'}); break;
                    // 秒(2位) [00,59]
                    case 'S': Fu8::format(out, c.second, Format_Layout{2, '0'}); break;
                    // 毫秒(3位) [000,999]
                    case 'f': Fu16::format(out, c.ms(), Format_Layout{3, '0'}); break;
                    // 时差(UTC) [-12.0,+14.0]
                    case 'z': {
                        if (exalign == Format_Layout::DEFAULT) Ff::format(out, xyu::K_TIME_DIFFERENCE, Format_Layout{}, "+.1f");
                        else Ff::format(out, exalign, Format_Layout{5, exalign}, "#+.1f");
                        break;
                    }
                    // 秒级时间戳(目前10位)
                    case's': Fu32::format(out, (c-xylu::xysystem::Calendar{}).s(), Format_Layout{}); break;
                    // %
                    case '%': out << '%'; break;
                    // 其他
                    default: out << '%' << f; break;
                }
            };

            for (xyu::size_t i = 0; i < ptn.size(); ++i)
            {
                // 占位符
                if (ptn.get(i) == '%')
                {
                    if (++i == ptn.size()) { out << '%'; return; }
                    switch (ptn.get(i))
                    {
                        // 日期(10位) [YYYY-MM-DD]
                        case 'F': {
                            parse('Y'), out << '-', parse('m'), out << '-', parse('d');
                            break;
                        }
                        // 时间(8位) [HH:MM:SS]
                        case 'T': {
                            parse('H'), out << ':', parse('M'), out << ':', parse('S');
                            break;
                        }
                        // 日期时间(19位) [YYYY-MM-DD HH:MM:SS]
                        case 'C': {
                            parse('Y'), out << '-', parse('m'), out << '-', parse('d');
                            out << ' ', parse('H'), out << ':', parse('M'), out << ':', parse('S');
                            break;
                        }
                        // 日期时间(23位) [YYYY-MM-DD HH:MM:SS.fff]
                        case 'c': {
                            parse('Y'), out << '-', parse('m'), out << '-', parse('d');
                            out << ' ', parse('H'), out << ':', parse('M'), out << ':', parse('S');
                            out << '.', parse('f');
                            break;
                        }
                        // 其他
                        default: parse(ptn.get(i)); break;
                    }
                }
                // 非占位符
                else out << ptn.get(i);
            }
        }
    };

    /**
     * @brief `xylu::xysystem::Duration<T, Scale>` 的格式化器特化
     * @note 如果考虑极致性能，请使用 `d.count` 自行格式化，该类决策时会有一些性能损失。
     * @note 提供了灵活的时间段格式化选项，可以指定输出单位，并能智能选择整数或浮点数表示。
     *
     * --- [?ex] (Expand Specifier) - 单位和符号控制 ---
     *   用于控制 `Duration` 的输出单位和是否显示单位符号，由 '?' 引导。
     *
     *   - **单位指定 (Unit Specifiers):**
     *     - `y`: 年 (year)。
     *     - `m`: 月 (month)。
     *     - `w`: 周 (week)。
     *     - `d`: 天 (day)。
     *     - `h`: 小时 (hour)。
     *     - `M`: 分钟 (minute)。
     *     - `s`: 秒 (second)。
     *     - `f`: 毫秒 (millisecond)。
     *     - `u`: 微秒 (microsecond)。
     *     - `n`: 纳秒 (nanosecond)。
     *
     *   - **符号控制 (Sign Specifiers):**
     *     - `#`: (默认) 显示单位后缀 (e.g., "100ms")。
     *     - `~`: 不显示单位后缀 (e.g., "100")。
     *     - 当显示单位却为提供单位时，会自动选择最接近的单位。
     *
     *   - **组合使用:** 符号和单位可以组合使用，符号会影响其后的单位。
     *     - `"{?+s}"` 或 `"{?s+}"`: 以秒为单位显示，并带上 "s" 后缀。
     *     - `"{?-y}"`: 以年为单位显示，但不带 "y" 后缀。
     *
     * --- [:ptn] (Pattern Specifier) - 数值格式 ---
     *   用于控制 `Duration` 数值的输出格式，由 ':' 引导。其行为会根据输出单位和精度要求，智能地选择将 `Duration` 作为整数或浮点数进行格式化。
     *
     *   - **作为整数格式化 (当转换到目标单位无精度损失，且未指定小数精度时):**
     *     - 支持所有 `Formatter<整数>` 的格式选项。
     *     - `d, b, o, x, X, #, +, -` 等均可使用。
     *     - **示例:** `xyfmt("{:d}", Duration_s(120))` -> `"120s"`
     *     - **示例:** `xyfmt("{:x?s}", Duration_s(255))` -> `"ffs"`
     *
     *   - **作为浮点数格式化 (当转换到目标单位有精度损失，或指定了小数精度时):**
     *     - 支持所有 `Formatter<浮点数>` 的格式选项。
     *     - `f, e, g, a, .n, #, +, -` 等均可使用。
     *     - 格式化器会自动在用户提供的 `pattern` 前加上 `'f'` 以触发浮点数模式，可通过传递其他模式覆盖。
     *     - **示例:** `xyfmt("{:.2f?s}", Duration_ms(1500))` -> `"1.50s"`
     *     - **示例:** `xyfmt("{?y}", Duration_utc())` -> `"55y"`
     */
    template <typename T, T Scale>
    struct Formatter<xylu::xysystem::Duration<T, Scale>>
    {
        using D = xylu::xysystem::Duration<T, Scale>;
    private:
        struct Format_Duration
        {
            enum Type { YEAR = 0, MONTH, WEEK, DAY, HOUR, MINUTE, SECOND, MILLISECOND, MICROSECOND, NANOSECOND, AUTO };
            constexpr static double Scales[11] = {
                    31556952000000000., 2629746000000000., 604800000000000., 86400000000000.,
                    3600000000000., 60000000000., 1000000000., 1000000., 1000., 1., Scale
            };
            // 显示的时间类型
            Type type = AUTO;
            // 是否显示时间单位
            bool sign = true;

            // 解析格式串
            constexpr static Format_Duration parse(const StringView& expand) noexcept
            {
                Format_Duration fd;
                for (char e : expand.range())
                {
                    switch (e)
                    {
                        case 'y': fd.type = YEAR; break;
                        case 'm': fd.type = MONTH; break;
                        case 'w': fd.type = WEEK; break;
                        case 'd': fd.type = DAY; break;
                        case 'h': fd.type = HOUR; break;
                        case 'M': fd.type = MINUTE; break;
                        case 's': fd.type = SECOND; break;
                        case 'f': fd.type = MILLISECOND; break;
                        case 'u': fd.type = MICROSECOND; break;
                        case 'n': fd.type = NANOSECOND; break;
                        case '~': fd.sign = false; break;
                        case '#': fd.sign = true; break;
                        default:;
                    }
                }
                if (fd.sign && fd.type == AUTO)
                {
                    if constexpr (Scale >= Scales[YEAR]) fd.type = YEAR;
                    else if constexpr (Scale >= Scales[MONTH]) fd.type = MONTH;
                    else if constexpr (Scale >= Scales[WEEK]) fd.type = WEEK;
                    else if constexpr (Scale >= Scales[DAY]) fd.type = DAY;
                    else if constexpr (Scale >= Scales[HOUR]) fd.type = HOUR;
                    else if constexpr (Scale >= Scales[MINUTE]) fd.type = MINUTE;
                    else if constexpr (Scale >= Scales[SECOND]) fd.type = SECOND;
                    else if constexpr (Scale >= Scales[MILLISECOND]) fd.type = MILLISECOND;
                    else if constexpr (Scale >= Scales[MICROSECOND]) fd.type = MICROSECOND;
                    else fd.type = NANOSECOND;
                }
                return fd;
            }
        };
    public:
        /// 预解析时间格式串
        constexpr static xyu::size_t prepare(const Format_Layout& layout, const StringView& pattern, const StringView& expand) noexcept
        {
            Format_Duration fd = Format_Duration::parse(expand);
            xyu::size_t count = fd.sign ? (fd.type == Format_Duration::MINUTE ? 3 : 2) : 0;

            Format_Float ff;
            ff.specifier = 'f';
            ff.parse(pattern);

            // 整数
            if constexpr (xyu::t_is_integral<T>)
            {
                // 作为整数解析的条件
                // 1.没有提供精度 (小数点)
                if (ff.sp == -1)
                    // 2.指定的单位更小(不会有精度损失) 或 指定的单位很大(分,时...)
                    if (Scale >= Format_Duration::Scales[fd.type] || fd.type < Format_Duration::SECOND)
                        return Formatter<T>::prepare(layout, pattern) + count;
            }

            // 作为浮点数解析
            return Formatter<xyu::t_wider<T, double>>::prepare(layout, ff) + count;
        }

        /// 解析时间格式串
        static xyu::size_t parse(const D& d, const StringView& layout, const StringView& pattern, const StringView& expand)
        {
            Format_Duration fd = Format_Duration::parse(expand);
            double s2 = Format_Duration::Scales[fd.type];
            xyu::size_t count = fd.sign ? (fd.type == Format_Duration::MINUTE ? 3 : 2) : 0;

            Format_Float ff;
            ff.specifier = 'f';
            ff.parse(pattern);

            // 整数
            if constexpr (xyu::t_is_integral<T>)
            {
                // 作为整数解析的条件
                // 1.没有提供精度 (小数点)
                if (ff.sp == -1)
                    // 2 指定的单位更小(不会有精度损失) 或 指定的单位很大(分,时...)
                    if (Scale >= s2 || fd.type < Format_Duration::SECOND)
                        return Formatter<T>::parse(d.count * (Scale / s2), layout, pattern) + count;
            }

            // 作为浮点数解析
            return Formatter<xyu::t_wider<T, double>>::parse(d.count * (Scale / s2), layout, ff) + count;
        }

        /// 格式化时间
        template <typename Stream>
        static void format(Stream& out, const D& d, const Format_Layout& layout, const StringView& pattern, const StringView& expand)
        {
            Format_Duration fd = Format_Duration::parse(expand);
            double s2 = Format_Duration::Scales[fd.type];
            bool asint = false;

            Format_Float ff;
            ff.specifier = 'f';
            ff.parse(pattern);

            // 整数
            if constexpr (xyu::t_is_integral<T>)
            {
                // 作为整数解析的条件
                // 1.没有提供精度 (小数点)
                if (ff.sp == -1)
                    // 2 指定的单位更小(不会有精度损失) 或 指定的单位很大(分,时...)
                    if (Scale >= s2 || fd.type < Format_Duration::SECOND) {
                        asint = true;
                        Formatter<T>::format(out, d.count * (Scale / s2), layout, pattern);
                    }
            }

            // 作为浮点数解析
            if (!asint) Formatter<xyu::t_wider<T, double>>::format(out, d.count * (Scale / s2), layout, ff);

            // 显示单位
            if (fd.sign)
            {
                switch (fd.type)
                {
                    case Format_Duration::YEAR: out << 'y'; break;
                    case Format_Duration::MONTH: out << 'm'; break;
                    case Format_Duration::WEEK: out << 'w'; break;
                    case Format_Duration::DAY: out << 'd'; break;
                    case Format_Duration::HOUR: out << 'h'; break;
                    case Format_Duration::MINUTE: out << "min"; break;
                    case Format_Duration::SECOND: out << "s"; break;
                    case Format_Duration::MILLISECOND: out << "ms"; break;
                    case Format_Duration::MICROSECOND: out << "us"; break;
                    case Format_Duration::NANOSECOND: out << "ns"; break;
                    default:;
                }
            }
        }
    };
}

#pragma clang diagnostic pop