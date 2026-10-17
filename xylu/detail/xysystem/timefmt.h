#pragma once

/**
 * @file    xysystem/timefmt.h
 * @brief   时间格式化
 * @date    2026-04-13
 */

#include "./time.h"
#include "../xystring/fmtimpl.h"

/// 时间段格式化
namespace xylu::xystring
{
    /**
     * @brief xyu::Duration<T, Num, Den> 的格式化器特化
     * @attention 如果考虑极致性能，请使用 `d.count` 自行格式化，该类决策时会有一些性能损失
     * @details 提供了灵活的时间段格式化选项，可以指定输出单位，并能智能选择整数或浮点数表示
     *
     * --- [?ex] - 拓展格式符 ---
     *
     *   - 单位指定: `y`-年, `m`-月, `w`-周, `d`-天, `h`-小时, `M`-分钟, `s`-秒, `f`-毫秒, `u`-微秒, `n`-纳秒。
     *
     *   - 后缀控制:
     *     - `#`: (默认) 缩写，如 y, m, min, s, ms。
     *     - `!`: 小写全称，如 year, hour, second, microsecond。
     *     - `@`: 帕斯卡命名，如 Year, Week, Second, Nanosecond。
     *     - `$`: 大写全称，如 YEAR, DAY, SECOND, MILLISECOND。
     *     - `&`: 中文，年, 月, 周, 天, 时, 分, 秒, 毫秒, 微秒, 纳秒。
     *     - `~`: 不显示后缀。
     *     - 当显示单位却未提供单位时，会自动选择最接近的单位。
     *
     * --- [:rep] - 格式符 ---
     * 
     *   - 作为整数格式化 (当转换到目标单位无精度损失，且未指定小数精度时):
     *     - 支持所有 `FormatImpl<整数>` 的格式选项。
     *     - `d, b, o, x, X, #, +, -` 等均可使用。
     *
     *   - 作为浮点数格式化 (当转换到目标单位有精度损失，或指定了小数精度时):
     *     - 支持所有 `FormatImpl<浮点数>` 的格式选项。
     *     - `f, e, g, a, .n, #, +, -` 等均可使用。
     *     - 格式化器会默认在 `represent` 前加上 `f` 以触发浮点数模式，可通过传递其他模式覆盖。
     *
     * --- [|lay] - 布局说明符 ---
     *   该部分仅对数值生效，对后缀无影响
     */
    template <typename T, xyu::uint64 Num, xyu::uint64 Den>
    struct FormatImpl<xysystem::Duration<T, Num, Den>>
    {
        using D = xysystem::Duration<T, Num, Den>;

        // 解析 Duration - expand
        struct Format_Duration
        {
            // 时间后缀
            static constexpr StringView suffixs[][10] = {
                    { "y", "m", "w", "d", "h", "min", "s", "ms", "us", "ns" },
                    { "year", "month", "week", "day", "hour", "minute", "second", "millisecond", "microsecond", "nanosecond" },
                    { "Year", "Month", "Week", "Day", "Hour", "Minute", "Second", "MilliSecond", "MicroSecond", "NanoSecond" },
                    { "YEAR", "MONTH", "WEEK", "DAY", "HOUR", "MINUTE", "SECOND", "MILLISECOND", "MICROSECOND", "NANOSECOND" },
                    { "年", "月", "周", "天", "时", "分", "秒", "毫秒", "微秒", "纳秒" }
            };
            
            // 显示的时间类型
            enum TimeType { Year, Month, Week, Day, Hour, Minute, Second, MilliSecond, MicroSecond, NanoSecond, Auto };
            TimeType ttype = Auto;
            // 显示的后缀类型
            enum SuffixType { Abbrev, Lower, Pascal, Upper, Chinese, None };
            SuffixType stype = Abbrev;
            // 能否无精度损失转换
            bool unless = false;

            // 解析格式串
            constexpr static Format_Duration parse(StringView expand) noexcept
            {
                Format_Duration fd;
                // 读取格式控制
                for (char e : expand.range())
                {
                    switch (e)
                    {
                        // 单位
                        case 'y': fd.ttype = Year; break;
                        case 'm': fd.ttype = Month; break;
                        case 'w': fd.ttype = Week; break;
                        case 'd': fd.ttype = Day; break;
                        case 'h': fd.ttype = Hour; break;
                        case 'M': fd.ttype = Minute; break;
                        case 's': fd.ttype = Second; break;
                        case 'f': fd.ttype = MilliSecond; break;
                        case 'u': fd.ttype = MicroSecond; break;
                        case 'n': fd.ttype = NanoSecond; break;
                        // 后缀
                        case '#': fd.stype = Abbrev; break;
                        case '~': fd.stype = None; break;
                        case '!': fd.stype = Lower; break;
                        case '@': fd.stype = Pascal; break;
                        case '$': fd.stype = Upper; break;
                        case '&': fd.stype = Chinese; break;
                        default:;
                    }
                }
                // 自动选择最近的单位
                if (fd.ttype == Auto)
                {
                    if constexpr (D{1} >= xyu::Duration_year<T>{1}) fd.ttype = Year, fd.unless = xyu::t_is_same<xyu::Duration_year<T>, xyu::t_wide<D, xyu::Duration_year<T>>>;
                #define XY_ttype_choose_gen( kind, name ) \
                    else if constexpr (D{1} >= xyu::Duration_##kind<T>{1}) fd.ttype = name, fd.unless = xyu::t_is_same<xyu::Duration_##kind<T>, xyu::t_wide<D, xyu::Duration_##kind<T>>>;
                XY_PP_FORWARD(XY_ttype_choose_gen, 2, month,Month, week,Week, day,Day, hour,Hour, min,Minute, s,Second, ms,MilliSecond, us,MicroSecond)
                #undef XY_ttype_choose_gen
                    else fd.ttype = NanoSecond, fd.unless = xyu::t_is_same<xyu::Duration_ns<T>, xyu::t_wide<D, xyu::Duration_ns<T>>>;
                }
                // 转换检测
                else switch (fd.ttype)
                {
                #define XY_ttype_unless_gen( kind, name ) \
                    case name: fd.unless = xyu::t_is_same<xyu::Duration_##kind<T>, xyu::t_wide<D, xyu::Duration_##kind<T>>>; break;
                XY_PP_FORWARD(XY_ttype_unless_gen, 2, year,Year, month,Month, week,Week, day,Day, hour,Hour, min,Minute, s,Second, ms,MilliSecond, us,MicroSecond, ns,NanoSecond)
                #undef XY_ttype_unless_gen
                    default:;
                }
                return fd;
            }
        };

        /// 编译期解析时间段格式串
        constexpr static xyu::size_t parse_compile(Format_Layout fl, StringView represent, StringView expand) noexcept
        {
            Format_Duration fd = Format_Duration::parse(expand);
            xyu::size_t count = fd.stype != Format_Duration::None ? Format_Duration::suffixs[fd.stype][fd.ttype].size() : 0;

            Format_Float ff;
            ff.specifier = 'f';
            ff.parse(represent);

            // 作为整数解析
            if (xyu::t_is_integral<T> && fd.unless && ff.sp == -1)
                return FormatImpl<T>::parse_compile(fl, represent) + count;
            // 作为浮点数解析
            return FormatImpl<xyu::t_wide<T, float>>::parse_compile(fl, ff) + count;
        }

        /// 运行期解析时间段格式串
        static xyu::size_t parse_runtime(const D& d, Format_Layout fl, StringView represent, StringView expand) noexcept
        {
            Format_Duration fd = Format_Duration::parse(expand);
            xyu::size_t count = fd.stype != Format_Duration::None ? Format_Duration::suffixs[fd.stype][fd.ttype].size() : 0;

            Format_Float ff;
            ff.specifier = 'f';
            ff.parse(represent);

            // 作为整数解析
            if constexpr (xyu::t_is_integral<T>) if (fd.unless && ff.sp == -1)
            {
                T value;
                switch (fd.ttype)
                {
                #define XY_ttype_int_count_gen( kind, name ) \
                    case Format_Duration::name: value = xyu::Duration_##kind<T>(d).count; break;
                XY_PP_FORWARD(XY_ttype_int_count_gen, 2, year,Year, month,Month, week,Week, day,Day, hour,Hour, min,Minute, s,Second, ms,MilliSecond, us,MicroSecond, ns,NanoSecond)
                    default:;
                }
                return FormatImpl<T>::parse_runtime(value, fl, represent) + count;
            }
            // 作为浮点数解析
            using F = xyu::t_wide<T, float>;
            F value;
            switch (fd.ttype)
            {
            #define XY_ttype_float_count_gen( kind, name ) \
                case Format_Duration::name: value = xyu::Duration_##kind<F>(d).count; break;
            XY_PP_FORWARD(XY_ttype_float_count_gen, 2, year,Year, month,Month, week,Week, day,Day, hour,Hour, min,Minute, s,Second, ms,MilliSecond, us,MicroSecond, ns,NanoSecond)
                default:;
            }
            return FormatImpl<F>::parse_runtime(value, fl, ff) + count;
        }

        /// 格式化时间段
        template <typename Stream>
        static void format(Stream& out, const D& d, Format_Layout fl, StringView represent, StringView expand) noexcept(FormatCore::is_noexcept_write<Stream>)
        {
            Format_Duration fd = Format_Duration::parse(expand);

            Format_Float ff;
            ff.specifier = 'f';
            ff.parse(represent);

            do {
                // 整数
                if constexpr (xyu::t_is_integral<T>) if (fd.unless && ff.sp == -1)
                {
                    T value;
                    switch (fd.ttype)
                    {
                    XY_PP_FORWARD(XY_ttype_int_count_gen, 2, year,Year, month,Month, week,Week, day,Day, hour,Hour, min,Minute, s,Second, ms,MilliSecond, us,MicroSecond, ns,NanoSecond)
                    #undef XY_ttype_int_count_gen
                        default:;
                    }
                    FormatImpl<T>::format(out, value, fl, represent);
                    break;
                }
                // 浮点数
                using F = xyu::t_wide<T, float>;
                F value;
                switch (fd.ttype)
                {
                XY_PP_FORWARD(XY_ttype_float_count_gen, 2, year,Year, month,Month, week,Week, day,Day, hour,Hour, min,Minute, s,Second, ms,MilliSecond, us,MicroSecond, ns,NanoSecond)
                #undef XY_ttype_float_count_gen
                    default:;
                }
                FormatImpl<F>::format(out, value, fl, ff);
            } while (false);

            // 显示单位
            if (fd.stype != Format_Duration::None) out << Format_Duration::suffixs[fd.stype][fd.ttype];
        }
    };
}

/// 日期格式化
namespace xylu::xystring
{
    /**
     * @brief xylu::xysystem::Calendar 的格式化器特化
     * @details 提供了灵活的日期时间格式化选项。
     *
     * --- [:rep] - 格式符 ---
     *
     *  --- 基本占位符 ---
     *
     *   - `%Y` : 四位年份 (0000-9999)
     *   - `%y` : 两位年份 (00-99)
     *   - `%m` : 两位月份 (01-12) 或带修饰符的英文月份
     *   - `%d` : 两位日期 (01-31)
     *   - `%j` : 年内的第几天 (001-366)
     *   - `%q` : 季节 (1-4) 或带修饰符的英文/中文季节
     *   - `%w` : 星期几 (0-6, 0=周一) 或带修饰符的英文/中文星期
     *   - `%h` : 12小时制小时 (00-11)
     *   - `%H` : 24小时制小时 (00-23)
     *   - `%p` : 上午/下午 (AM/PM)
     *   - `%P` : 上午/下午 (am/pm)
     *   - `%M` : 分钟 (00-59)
     *   - `%s` : 秒 (00-59)
     *   - `%u` : 毫秒 (000-999)
     *   - `%F` : 等效于 `%Y-%m-%d`
     *   - `%T` : 等效于 `%H:%M:%S`
     *   - `%C` : 等效于 `%Y-%m-%d %H:%M:%S` (默认格式)
     *   - `%c` : 等效于 `%Y-%m-%d %H:%M:%S.%u`
     *   - `%%` : 输出单个 `%`
     *
     *  --- 修饰符 ---
     *
     *   修饰符紧跟在 `%` 之后，仅对 `m`(月份)、`w`(星期)、`q`(季节) 有效。
     *   对其他占位符（如 `%#Y`）修饰符会被忽略，等同于基本占位符。
     *
     *   - `#` : 英文缩写形式
     *     - 月份: Jan, Feb, ..., Dec
     *     - 星期: Mon, Tue, ..., Sun
     *     - 季节: Spr, Sum, Aut, Win
     *
     *   - `@` : 英文全称形式
     *     - 月份: January, February, ..., December
     *     - 星期: Monday, Tuesday, ..., Sunday
     *     - 季节: Spring, Summer, Autumn, Winter
     *
     *   - `&` : 中文形式
     *     - 星期: 星期一, 星期二, ..., 星期日
     *     - 季节: 春, 夏, 秋, 冬
     *
     *   - 示例:
     *     - `%#m` -> "Jan"
     *     - `%@w` -> "Monday"
     *     - `%&q` -> "春"
     *     - `%#Y` -> 同 `%Y` (忽略 `#`)
     *
     */
    template <>
    struct FormatImpl<xysystem::Calendar>
    {
        // 默认日期格式串
        constexpr static StringView DefaultRep = "%C";

        // 月
        static constexpr StringView smonth[][12] = {
                { "Jan", "Feb", "Mar", "Apr", "May", "Jun", "Jul", "Aug", "Sep", "Oct", "Nov", "Dec" },
                { "January", "February", "March", "April", "May", "June", "July", "August", "September", "October", "November", "December" },
        };
        // 星期
        static constexpr StringView sweek[][7] = {
                { "Mon", "Tue", "Wed", "Thu", "Fri", "Sat", "Sun" },
                { "Monday", "Tuesday", "Wednesday", "Thursday", "Friday", "Saturday", "Sunday" },
                { "星期一", "星期二", "星期三", "星期四", "星期五", "星期六", "星期日" },
        };
        // 季节
        static constexpr StringView sseason[][4] = {
                { "Spr", "Sum", "Aut", "Win" },
                { "Spring", "Summer", "Autumn", "Winter" },
                { "春", "夏", "秋", "冬" },
        };
        
        /// 编译期解析日期格式串
        constexpr static xyu::size_t parse_compile(StringView represent) noexcept
        {
            auto& rep = represent.empty() ? DefaultRep : represent;
            xyu::size_t count = 0, i = 0, j = 0;
            for (;;)
            {
                // 查找占位符
                j = rep.find('%', i);
                if (j == -1) break;
                count += j - i;
                i = j;
                // 占位符处理
                if (++i == rep.size()) return ++count;
                switch (rep[i])
                {
                    case 'q': case 'w': case '%': count += 1; break;
                    case 'y': case 'm': case 'd': case 'p':
                    case 'h': case 'H': case 'M': case 's': count += 2; break;
                    case 'j': case 'u': count += 3; break;
                    case 'Y': count += 4; break;
                    case 'T': count += 8; break;
                    case 'F': count += 10; break;
                    case 'C': count += 19; break;
                    case 'c': count += 23; break;
                    case '#':
                    {
                        if (++i == rep.size()) return count + 2;
                        if (rep[i] == 'm' || rep[i] == 'w' || rep[i] == 'q') count += 6;
                        else count += 2;
                        break;
                    }
                    case '@':
                    {
                        if (++i == rep.size()) return count + 2;
                        if (rep[i] == 'm' || rep[i] == 'w') count +=9;
                        else if (rep[i] == 'q') count += 6;
                        else count += 2;
                        break;
                    }
                    case '&':
                    {
                        if (++i == rep.size()) return count + 2;
                        if (rep[i] == 'w') count += 12;
                        else if (rep[i] == 'q') count += 4;
                        else count += 2;
                        break;
                    }
                    default: count += 2; break;
                }
                if (++i == rep.size()) break;
            }
            count += rep.size() - i;
            return count;
        }

        /// 格式化日期
        template <typename Stream>
        static void format(Stream& out, const xysystem::Calendar& c, StringView represent) noexcept(FormatCore::is_noexcept_write<Stream>)
        {
            using Fu = FormatImpl<xyu::uint>;
            auto& rep = represent.empty() ? DefaultRep : represent;

            // 解析单个占位符
            auto parse = [&](char k, char f = 0)
            {
                switch (k)
                {
                    // 年份 [0000,9999]
                    case 'Y': Fu::format(out, c.year, Format_Layout{4,'0'}); break;
                    // 年份 [00,99]
                    case 'y': Fu::format(out, c.year, Format_Layout{2,'0'}); break;
                    // 月份 [01,12]
                    case 'm': if (f == '#') out << smonth[0][(c.month + 11) % 12];
                              else if (f == '@') out << smonth[1][(c.month + 11) % 12];
                              else Fu::format(out, c.month, Format_Layout{2,'0'}); break;
                    // 日 [01,31]
                    case 'd': Fu::format(out, c.day, Format_Layout{2,'0'}); break;
                    // 一年中的第几天 [001,366]
                    case 'j': Fu::format(out, c.day_in_year(), Format_Layout{3,'0'}); break;
                    // 季节 [1,4]
                    case 'q': if (f == '#') out << sseason[0][(c.month + 9) / 3 % 4];
                              else if (f == '@') out << sseason[1][(c.month + 9) / 3 % 4];
                              else if (f == '&') out << sseason[2][(c.month + 9) / 3 % 4];
                              else Fu::format(out, (c.month + 9) / 3 % 4 + 1, Format_Layout{}); break;
                    // 星期 [0,6] (0代表星期一)
                    case 'w': if (f == '#') out << sweek[0][c.week_day()];
                              else if (f == '@') out << sweek[1][c.week_day()];
                              else if (f == '&') out << sweek[2][c.week_day()];
                              else Fu::format(out, c.week_day(), Format_Layout{}); break;
                    // 时(12小时制) [00,11]
                    case 'h': Fu::format(out, c.hour % 12, Format_Layout{2,'0'}); break;
                    // 时(24小时制) [00,23]
                    case 'H': Fu::format(out, c.hour, Format_Layout{2,'0'}); break;
                    // 上午/下午 {AM,PM}
                    case 'p': out << (c.hour < 12 ? "AM" : "PM"); break;
                    // 上午/下午 {am,pm}
                    case 'P': out << (c.hour < 12 ? "am" : "pm"); break;
                    // 分 [00,59]
                    case 'M': Fu::format(out, c.minute, Format_Layout{2,'0'}); break;
                    // 秒 [00,59]
                    case 's': Fu::format(out, c.second, Format_Layout{2,'0'}); break;
                    // 毫秒 [000,999]
                    case 'u': Fu::format(out, c.ms(), Format_Layout{3,'0'}); break;
                    // %
                    case '%': out << '%'; break;
                    // 其他
                    default: out << '%' << k;
                             if (f) out << f;
                             break;
                }
            };

            // 逐个解析
            xyu::size_t i = 0, j = 0;
            for (;;)
            {
                // 查找占位符
                j = rep.find('%', i);
                if (j == -1) break;
                out << rep.subview(i, j - i);
                i = j;
                // 占位符处理
                if (++i == rep.size()) { out << '%'; return; }
                switch (rep[i])
                {
                    // 日期 [YYYY-MM-DD]
                    case 'F': parse('Y'); out << '-'; parse('m'); out << '-'; parse('d'); break;
                    // 时间 [HH:MM:SS]
                    case 'T': parse('H'); out << ':'; parse('M'); out << ':'; parse('s'); break;
                    // 日期时间 [YYYY-MM-DD HH:MM:SS]
                    case 'C': parse('Y'); out << '-'; parse('m'); out << '-'; parse('d'); out << ' ';
                              parse('H'); out << ':'; parse('M'); out << ':'; parse('s'); break;
                    // 日期时间 [YYYY-MM-DD HH:MM:SS.uuu]
                    case 'c': parse('Y'); out << '-'; parse('m'); out << '-'; parse('d'); out << ' ';
                              parse('H'); out << ':'; parse('M'); out << ':'; parse('s'); out << '.';
                              parse('u'); break;
                    // 英文缩写 | 英文全称 | 中文
                    case '#': case '@': case '&':
                        if (i + 1 == rep.size()) { out << '%' << rep[i]; return; }
                        parse(rep[i + 1], rep[i]), ++i; break;
                    // 其他
                    default: parse(rep[i]); break;
                }
                if (++i == rep.size()) break;
            }
            out << rep.subview(i);
        }
    };
}
