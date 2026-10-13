#pragma once

/**
 * @file    xysystem/time.h
 * @brief   时间相关类
 * @date    2026-04-11
 */

#include "./_.h"
#include "../xymath/calc.h"
#include "../xymemory/func.h"

/// 时间段
namespace xylu::xysystem
{
    // 前置声明
    template <typename T, xyu::uint64 Num, xyu::uint64 Den> struct Duration;
    
    template <typename T = xyu::int64> using Duration_ns   = Duration<T, 1, 1'000'000'000>;
    template <typename T = xyu::int64> using Duration_10ns = Duration<T, 1, 100'000'000>;
    template <typename T = xyu::int64> using Duration_us   = Duration<T, 1, 1'000'000>;
    template <typename T = xyu::int64> using Duration_ms   = Duration<T, 1, 1'000>;
    template <typename T = xyu::int64> using Duration_s    = Duration<T, 1, 1>;
    template <typename T = xyu::int64> using Duration_min  = Duration<T, 60, 1>;
    template <typename T = xyu::int64> using Duration_hour = Duration<T, 3600, 1>;
    template <typename T = xyu::int64> using Duration_day  = Duration<T, 86400, 1>;
    template <typename T = xyu::int64> using Duration_week = Duration<T, 604800, 1>;
    template <typename T = xyu::int64> using Duration_month= Duration<T, 2629746, 1>;
    template <typename T = xyu::int64> using Duration_year = Duration<T, 31556952, 1>;

    namespace __
    {
        /**
         * @brief 转换为指定倍率
         * @details 除去公共倍率后计算减小溢出可能
         */
        template <xyu::uint64 snum, xyu::uint64 sden, typename U, xyu::uint64 dnum, xyu::uint64 dden, typename T>
        constexpr U duration_change(T count) noexcept
        {
            using W = xyu::t_wide<T, U>;
            // 同倍率
            if constexpr (snum == dnum && sden == dden) return count;
            // 不同倍率
            else
            {
                // 交叉约分
                constexpr auto gcd_num = xyu::calc_gcd(snum, dnum);
                constexpr auto gcd_den = xyu::calc_gcd(dden, sden);
                constexpr W final_num = xyu::make_signed((snum / gcd_num) * (dden / gcd_den));
                constexpr W final_den = xyu::make_signed((sden / gcd_den) * (dnum / gcd_num));
                // 特例
                if constexpr (final_den == 1)
                {
                    if constexpr  (xyu::t_is_integer<T,U>)
                        return xyu::calc_of_mul<U,!XY_RELEASE>(count, final_num, XY_CurPos("Duration::change"));
                    else return static_cast<W>(count) * final_num;
                }
                else if constexpr (final_num == 1)
                {
                    if constexpr (xyu::t_is_integer<T,U>)
                         return xyu::calc_of_div<U,!XY_RELEASE>(count, final_den, XY_CurPos("Duration::change"));
                    else return static_cast<W>(count) / final_den;
                }
                // 通用
                else if constexpr (xyu::t_is_integer<T,U>)
                {
                    W tmp = xyu::calc_of_mul<W,!XY_RELEASE>(count, final_num, XY_CurPos("Duration::change"));
                    return xyu::calc_of_div<U,!XY_RELEASE>(tmp, final_den, XY_CurPos("Duration::change"));
                }
                else return static_cast<W>(count) * final_num / final_den;
            }
        }

        /**
         * @brief 相加得到新类型
         * @details 除去公共倍率后计算减小溢出可能
         */
        template <xyu::uint64 num1, xyu::uint64 den1, xyu::uint64 num2, xyu::uint64 den2, typename T, typename U>
        constexpr decltype(auto) duration_add(T n1, U n2) noexcept
        {
            using W = xyu::t_wide<T, U>;
            // 同倍率
            if constexpr (num1 == num2 && den1 == den2)
            {
                if constexpr (xyu::t_is_integer<T,U>)
                     return Duration<W, num1, den1>{xyu::calc_of_add<W,!XY_RELEASE>(n1, n2, XY_CurPos("Duration::change_add"))};
                else return Duration<W, num1, den1>{static_cast<W>(n1) + n2};
            }
            // 不同倍率
            else
            {
                // 寻找共同的“最小精度”
                constexpr auto gcd_num = xyu::calc_gcd(num1, num2);
                constexpr auto gcd_den = xyu::calc_gcd(den1, den2);
                constexpr auto lcm_den = (den1 / gcd_den) * den2;
                using Ret = Duration<W, gcd_num, lcm_den>;
                constexpr W m1 = (num1 / gcd_num) * (den2 / gcd_den);
                constexpr W m2 = (num2 / gcd_num) * (den1 / gcd_den);
                // 特例
                if constexpr (m1 == 1)
                {
                    if constexpr (xyu::t_is_integer<T,U>)
                    {
                        W tmp = xyu::calc_of_mul<W,!XY_RELEASE>(n2, m2, XY_CurPos("Duration::change_add"));
                        return Ret{xyu::calc_of_add<W,!XY_RELEASE>(n1, tmp, XY_CurPos("Duration::change_add"))};
                    }
                    else return Ret{n1 + static_cast<W>(n2) * m2};
                }
                else if constexpr (m2 == 1)
                {
                    if constexpr (xyu::t_is_integer<T,U>)
                    {
                        W tmp = xyu::calc_of_mul<W,!XY_RELEASE>(n1, m1, XY_CurPos("Duration::change_add"));
                        return Ret{xyu::calc_of_add<W,!XY_RELEASE>(tmp, n2, XY_CurPos("Duration::change_add"))};
                    }
                    else return Ret{static_cast<W>(n1) * m1 + n2};
                }
                // 通用
                else
                {
                    if constexpr (xyu::t_is_integer<T,U>)
                    {
                        W tmp1 = xyu::calc_of_mul<W,!XY_RELEASE>(n1, m1, XY_CurPos("Duration::change_add"));
                        W tmp2 = xyu::calc_of_mul<W,!XY_RELEASE>(n2, m2, XY_CurPos("Duration::change_add"));
                        return Ret{xyu::calc_of_add<W,!XY_RELEASE>(tmp1, tmp2, XY_CurPos("Duration::change_add"))};
                    }
                    else return Ret{static_cast<W>(n1) * m1 + static_cast<W>(n2) * m2};
                }
            }
        }

        // 单位均为 10ns
        xyu::int64 duration_utc() noexcept;
        xyu::int64 duration_any() noexcept;
        xyu::int64 duration_process() noexcept;
        xyu::int64 duration_thread() noexcept;

        void sleep_help(xyu::uint32 s, xyu::uint32 ns) noexcept;
    }

    /**
     * @brief 时间段类
     * @tparam T 时间单位存储类型 (必须为有符号类型)
     * @tparam Num 1 时间单位 == Num / Den (s)
     * @tparam Den 1 时间单位 == Num / Den (s)
     * @details
     *   - 算术运算会自动处理不同 Num/Den 之间的转换，并尝试保持最高精度
     *   - 提供了 `_ns`, `_us`, `_s` 等用户定义字面量，方便创建 `Duration` 对象
     */
    template <typename T, xyu::uint64 Num, xyu::uint64 Den>
    struct Duration
    {
        static_assert(xyu::t_is_arithmetic<T>, "T must be an arithmetic type");
        static_assert(xyu::t_number<T>::is_signed, "T must be a signed type");
        static_assert(Num != 0 && Den != 0);

    public:
        static constexpr xyu::uint64 num = Num / xyu::calc_gcd(Num, Den);
        static constexpr xyu::uint64 den = Den / xyu::calc_gcd(Num, Den);

    public:
        T count;    // 时间单位数量

    public:
        /* 构造赋值 */

        // 数值构造
        constexpr Duration(T n = 0) noexcept: count(n) {}
        // 数值赋值
        constexpr Duration& operator=(T n) noexcept { count = n; return *this; }

        // 其他时间段构造
        template<typename U, xyu::uint64 Num2, xyu::uint64 Den2>
        constexpr Duration(Duration<U, Num2, Den2> other) noexcept
        : count(__::duration_change<Duration<U, Num2, Den2>::num, Duration<U, Num2, Den2>::den, T, num, den>(other.count)) {}
        // 其他时间段赋值
        template<typename U, xyu::uint64 Num2, xyu::uint64 Den2>
        constexpr Duration& operator=(Duration<U, Num2, Den2> other) noexcept
        {
            count = __::duration_change<Duration<U, Num2, Den2>::num, Duration<U, Num2, Den2>::den, T, num, den>(other.count);
            return *this;
        }

        /* 运算 */

        // 递增递减
        constexpr Duration operator-() const noexcept
        {
            if constexpr (!XY_RELEASE && xyu::t_is_integer<T>)
                if (XY_UNLIKELY(count == xyu::t_number<T>::min))
                    xyu::E_Logic::abort_invalid_value(XY_CurPos("Duration::op-"), count);
            return -count;
        }
        constexpr Duration& operator++() noexcept
        {
            if constexpr (xyu::t_is_integer<T>)
                count = xyu::calc_of_add<T,!XY_RELEASE>(count, 1, XY_CurPos("Duration::op++"));
            else ++count;
            return *this;
        }
        constexpr Duration& operator--() noexcept
        {
            if constexpr (xyu::t_is_integer<T>)
                count = xyu::calc_of_sub<T,!XY_RELEASE>(count, 1, XY_CurPos("Duration::op--"));
            else --count;
            return *this;
        }
        constexpr Duration operator++(int) noexcept { T tmp = count; operator++(); return tmp; }
        constexpr Duration operator--(int) noexcept { T tmp = count; operator--(); return tmp; }

        // 倍率运算
        template<typename U, typename = xyu::t_enable<xyu::t_is_arithmetic<U>>>
        constexpr Duration& operator*=(U n) noexcept
        {
            if constexpr (xyu::t_is_integer<T>)
                count = xyu::calc_of_mul<T,!XY_RELEASE>(count, n, XY_CurPos("Duration::op*"));
            else count *= n;
            return *this;
        }
        template<typename U, typename = xyu::t_enable<xyu::t_is_arithmetic<U>>>
        constexpr Duration& operator/=(U n) noexcept
        {
            if constexpr (xyu::t_is_integer<T>)
                count = xyu::calc_of_div<T,!XY_RELEASE>(count, n, XY_CurPos("Duration::op/"));
            else count /= n;
            return *this;
        }
        template<typename U, typename V = T, typename = xyu::t_enable<xyu::t_is_integer<U, V>>>
        constexpr Duration& operator%=(U n) noexcept
        {
            if constexpr (xyu::t_is_integer<T>)
                count = xyu::calc_of_mod<T,!XY_RELEASE>(count, n, XY_CurPos("Duration::op%"));
            else count %= n;
            return *this;
        }

        template<typename U, typename = xyu::t_enable<xyu::t_is_arithmetic<U>>>
        constexpr friend auto operator*(U n, Duration d) noexcept { return d * n; }
        template<typename U, typename = xyu::t_enable<xyu::t_is_arithmetic<U>>>
        constexpr auto operator*(U n) const noexcept { return Duration<xyu::t_wide<T, U>, Num, Den>(count) *= n; }
        template<typename U, typename = xyu::t_enable<xyu::t_is_arithmetic<U>>>
        constexpr auto operator/(U n) const noexcept { return Duration<xyu::t_wide<T, U>, Num, Den>(count) /= n; }
        template<typename U, typename V = T, typename = xyu::t_enable<xyu::t_is_integer<U, V>>>
        constexpr auto operator%(U n) const noexcept { return Duration<xyu::t_wide<T, U>, Num, Den>(count) %= n; }

        // 加减运算
        template<typename U, xyu::uint64 Num2, xyu::uint64 Den2>
        constexpr Duration& operator+=(Duration<U, Num2, Den2> other) noexcept
        {
            auto add = __::duration_change<Duration<U, Num2, Den2>::num, Duration<U, Num2, Den2>::den, T, num, den>(other.count);
            if constexpr (xyu::t_is_integer<T>)
                count = xyu::calc_of_add<T,!XY_RELEASE>(count, add, XY_CurPos("Duration::op+"));
            else count += add;
            return *this;
        }
        template<typename U, xyu::uint64 Num2, xyu::uint64 Den2>
        constexpr Duration& operator-=(Duration<U, Num2, Den2> other) noexcept
        {
            auto sub = __::duration_change<Duration<U, Num2, Den2>::num, Duration<U, Num2, Den2>::den, T, num, den>(other.count);
            if constexpr (xyu::t_is_integer<T>)
                count = xyu::calc_of_sub<T,!XY_RELEASE>(count, sub, XY_CurPos("Duration::op-"));
            else count -= sub;
            return *this;
        }
        template<typename U, xyu::uint64 Num2, xyu::uint64 Den2>
        constexpr auto operator+(Duration<U, Num2, Den2> other) const noexcept
        {
            return __::duration_add<num, den, Duration<U, Num2, Den2>::num, Duration<U, Num2, Den2>::den>(count, other.count);
        }
        template<typename U, xyu::uint64 Num2, xyu::uint64 Den2>
        constexpr auto operator-(Duration<U, Num2, Den2> other) const noexcept
        {
            return __::duration_add<num, den, Duration<U, Num2, Den2>::num, Duration<U, Num2, Den2>::den>(count, -other.count);
        }

        /* 获取 */

        // 获取从 utc 时间开始的时长 (从1970年1月1日00:00:00 UTC 时间开始)
        static Duration utc() noexcept { return {Duration_10ns<T>{__::duration_utc()}}; }
        // 获取从某时刻开始的时长 (与系统时间无关)
        static Duration any() noexcept { return {Duration_10ns<T>{__::duration_any()}}; }
        // 获取当前进程在系统中的运行时长
        static Duration process() noexcept { return {Duration_10ns<T>{__::duration_process()}}; }
        // 获取当前线程在系统中的运行时长
        static Duration thread() noexcept { return {Duration_10ns<T>{__::duration_thread()}}; }

        /* 转换 */
    #define XY_duration_convert_gen(unit) \
        constexpr T unit() const noexcept { return Duration_##unit<T>(*this).count; }
    XY_PP_FORWARD(XY_duration_convert_gen, 1, ns, us, ms, s, min, hour, day, week, month, year)
    #undef XY_duration_convert_gen

        /* 休眠 */

        /**
         * @brief 休眠指定时间
         * @details 若 count 为0或负数则直接返回
         */
        void sleep() const noexcept
        {
            if (XY_UNLIKELY(count <= 0)) return;
            Duration_s<T> s = *this;
            Duration_ns<T> ns = *this - s;
            constexpr xyu::uint32 limit = xyu::t_number<xyu::uint32>::max;
            while (XY_UNLIKELY(s.count > limit))
            {
                __::sleep_help(limit, 0);
                s.count -= limit;
            }
            __::sleep_help(s.count, ns.count);
        }

        /**
         * @brief 休眠到距离 1970 年 1 月 1 日 为 duration 的时间点
         * @details 若 count 小于当前的 UTC 时间则直接返回
         */
        void sleep_to() const noexcept { (*this - utc()).sleep(); }

        /* 比较 */
    #define XY_duration_compare_gen(op) \
        template <typename U, xyu::uint64 Num2, xyu::uint64 Den2> \
        constexpr bool operator op(Duration<U, Num2, Den2> other) const noexcept \
        { \
            constexpr auto ynum = Duration<U, Num2, Den2>::num; \
            constexpr auto yden = Duration<U, Num2, Den2>::den; \
            if constexpr (num == ynum && den == yden) return count op other.count; \
            else \
            { \
                constexpr auto gcd_num = xyu::calc_gcd(num, ynum); \
                constexpr auto gcd_den = xyu::calc_gcd(den, yden); \
                constexpr auto x_scale = xyu::make_signed((num / gcd_num) * (yden / gcd_den)); \
                constexpr auto y_scale = xyu::make_signed((ynum / gcd_num) * (den / gcd_den)); \
                auto cmp = [&](auto v1, auto v2) noexcept -> bool \
                { \
                    if constexpr      (y_scale == 1) return v1 op other.count; \
                    else if constexpr (x_scale == 1) return count op v2; \
                    else return v1 op v2; \
                }; \
                using V = decltype(count * x_scale); \
                if constexpr (xyu::t_is_integer<V>) \
                { \
                    V tmp1 = {}, tmp2 = {}; \
                    bool overflow = false; \
                    if constexpr (x_scale != 1) \
                        if (!overflow) overflow = xyu::calc_of_mul(count, x_scale, tmp1); \
                    if constexpr (y_scale != 1) \
                        if (!overflow) overflow = xyu::calc_of_mul(other.count, y_scale, tmp2); \
                    if (XY_LIKELY(!overflow)) return cmp(tmp1, tmp2); \
                    return cmp((double)count * x_scale, (double)other.count * y_scale); \
                } \
                else return cmp(count * x_scale, other.count * y_scale); \
            } \
        }
    XY_PP_FORWARD( XY_duration_compare_gen, 1, ==, !=, >, >=, <, <= )
    #undef XY_duration_compare_gen
    };

    // 获取从 utc 时间开始的时长 (从1970年1月1日00:00:00 UTC 时间开始)
    inline Duration_10ns<> Duration_utc() noexcept { return __::duration_utc(); }
    // 获取从某时刻开始的时长 (与系统时间无关)
    inline Duration_10ns<> Duration_any() noexcept { return __::duration_any(); }
    // 获取当前进程在系统中的运行时长
    inline Duration_10ns<> Duration_process() noexcept { return __::duration_process(); }
    // 获取当前线程在系统中的运行时长
    inline Duration_10ns<> Duration_thread() noexcept { return __::duration_thread(); }

    // 时间段对象字面值
#define XY_duration_suffix_gen( unit ) \
    constexpr Duration_##unit<> operator""_##unit (unsigned long long unit) noexcept { return xyu::make_signed(unit); } \
    constexpr Duration_##unit<long double> operator""_##unit (long double unit) noexcept { return unit; }
XY_PP_FORWARD( XY_duration_suffix_gen, 1, ns, us, ms, s, min, hour, day, week, month, year )
#undef XY_duration_suffix_gen
}

/// 日期
namespace xylu::xysystem
{
    /**
     * @brief 日期类
     * @details 用于存储年月日时分秒毫秒。
     *  - 为了节省空间毫秒为非精确存储，且无法直接操作，需通过 `ms()` 进行处理。
     *  - 注意存储的数值范围，否则可能导致异常，类型为无符号，但进行运算时会自动提升为 int 不需要额外处理。
     *  - 有趣的小特性：1s0ms > 0s1000ms (因为 1000ms 为三舍四入后的结果 即显示偏大)
     */
    struct alignas(xyu::uint64) Calendar
    {
        inline static int TimeDifference = +8 * 60;             // 区域时差 (单位:min)

#if XY_LITTLE_ENDIAN
    private:
        xyu::uint8  milli;  // 毫秒 (非直接精确存储)
    public:
        xyu::uint8  second; // 秒
        xyu::uint8  minute; // 分
        xyu::uint8  hour;   // 时
        xyu::uint8  day;    // 日
        xyu::uint8  month;  // 月
        xyu::uint16 year;   // 年 [0,65535]
#else
    public:
        xyu::uint16 year;   // 年 [0,65535]
        xyu::uint8  month;  // 月
        xyu::uint8  day;    // 日
        xyu::uint8  hour;   // 时
        xyu::uint8  minute; // 分
        xyu::uint8  second; // 秒
    private:
        xyu::uint8  milli;  // 毫秒 (非直接精确存储)
#endif

    public:
        /* 构造 */

        /**
         * @brief 构造函数
         * @attention 所有参数必须为正数，且必须在合理范围内，否则会导致溢出或异常
         * @details ms的存储方式为:
         *   当最高位为0时，后7位精确存储[0,127]；
         *   当最高位为1时，后7位为8倍[128,1000]
         */
        constexpr explicit Calendar(xyu::uint16 year = 1970, xyu::uint8 month = 1, xyu::uint8 day = 1,
                                    xyu::uint8 hour = 0, xyu::uint8 minute = 0, xyu::uint8 second = 0,
                                    xyu::uint16 milli = 0) noexcept
            : year{year}, month{month}, day{day}, hour{hour}, minute{minute}, second{second}, milli{} { ms(milli); }

        /**
         * @brief 从距离1970年1月1日的 天数 构造日期
         * @details 基于 Neri 和 Schneider 的算法
         * @attention 仅修改 年月日
         */
        constexpr Calendar& from_epoch_day(xyu::int32 days) noexcept
        {
            if constexpr (!XY_RELEASE)
            {
                constexpr xyu::int32 min_day = -719528;     // 00000-01-01
                constexpr xyu::int32 max_day = 23217003;    // 65535-12-31
                if (XY_UNLIKELY(days < min_day || days > max_day))
                    xyu::E_Logic::abort_out_range(XY_CurPos("Calendar::from_epoch_day"), days, min_day, max_day + 1);
            }

            xyu::uint32
            r0 = days + 536895458u,
            n1 = 4 * r0 + 3,
            q1 = n1 / 146097,
            r1 = n1 % 146097 / 4;

            xyu::uint32 n2 = 4 * r1 + 3;
            xyu::uint64 u2 = 2939745ull * n2;
            xyu::uint32
            q2 = u2 / (1ull << 32),
            r2 = u2 % (1ull << 32) / 2939745 / 4,
            j  = r2 >= 306,

            n3 = 2141 * r2 + 197913,
            q3 = n3 / (1ul << 16),
            r3 = n3 % (1ul << 16) / 2141;

            year = 100 * q1 + q2 + j + -1468000u;
            month = j ? q3 - 12 : q3;
            day = r3 + 1;
            return *this;
        }

        /// 从距离1970年1月1日的 时间戳 构造日期
        template <typename T, xyu::uint64 Num, xyu::uint64 Den>
        constexpr Calendar& from_epoch_duration(Duration<T, Num, Den> duration) noexcept
        {
            auto l0 = duration;
            // 年月日
            Duration_day<T> dd = l0;
            auto ld = duration - dd;
            if (ld.count < 0) { --dd; ld += Duration_day<>{1}; }
            from_epoch_day(dd.count);
            // 时
            Duration_hour<T> dh = ld;
            hour = dh.count;
            auto lh = ld - dh;
            // 分
            Duration_min<> dm = lh;
            minute = dm.count;
            auto lm = lh - dm;
            // 秒
            Duration_s<> ds = lm;
            second = ds.count;
            auto ls = lm - ds;
            // 毫秒
            Duration_ms<> dms = ls;
            ms(dms.count);
            return *this;
        }

        /* 修改 */

        /**
         * @brief 获取ms
         * @details [0,127]精度为1，[128,1000]精度为8
         */
        constexpr xyu::int32 ms() const noexcept
        { return milli & 0x80 ? ((milli & 0x7f) << 3) : (milli & 0x7f); }

        /**
         * @brief 设置ms
         * @details [0,127]精度为1，[128,1000]精度为8，精度为8时进行三舍四入
         */
        constexpr void ms(xyu::uint32 new_ms) noexcept
        { milli = (new_ms <= 0x7f ? new_ms : (((new_ms + 4) >> 3) | 0x80)); }

        /* 有效性 */

        /// 是否有效
        constexpr bool valid() const noexcept
        {
            return (month >= 1 && month <= 12) &&
                   (day >= 1 && day <= days_per_month[month] + (month == 2 && is_leap_year())) &&
                   (hour <= 23) && (minute <= 59) && (second <= 59) && (milli <= 0b1'111'1101);
        }

        /**
         * @brief 修正日期，使其符合有效范围
         * @details 先从ms修正到h，从mon修正到y，最后修正d (需根据mon与y)
         */
        constexpr Calendar& fix() noexcept
        {
            // 修正毫秒
            xyu::uint32 overflow = milli >= 0b1'111'1101;       // 125*8 (1000)
            if (overflow) milli = ((milli & 0b11) - 1) << 3;
            // 修正秒
            xyu::uint32 tmp = second + overflow;
            overflow = tmp / 60u;
            second = tmp % 60u;
            // 修正分
            tmp = minute + overflow;
            overflow = tmp / 60u;
            minute = tmp % 60u;
            // 修正时
            tmp = hour + overflow;
            overflow = tmp / 24u;
            hour = tmp % 24u;
            // 预修正年月
            if (month)
            {
                year += --month / 12u;
                month = month % 12u + 1u;
            }
            else --year, month = 12u;
            // 修正日(月年)
            tmp = day + overflow;
            if (tmp <= 58)  // 58 <= 连续两个月份的天数
            {
                if (XY_UNLIKELY(tmp == 0))
                {
                    if (--month == 0) --year, month = 12u;
                    day = days_per_month[month] + (month == 2u && is_leap_year());
                }
                else
                {
                    xyu::uint32 limit = days_per_month[month] + (month == 2u && is_leap_year());
                    if (tmp > limit)
                    {
                        tmp -= limit;
                        if (++month > 12u) ++year, month = 1u;
                    }
                    day = tmp;
                }
            }
            else from_epoch_day(epoch_day() + overflow);
            return *this;
        }

        /* 信息 */

        /**
         * @brief 判断是否为闰年
         * @attention 此方法仅适用于 [0,102499] (满足 [0,65535] 的范围)
         * @details 闰年: 年份能被4整除但不能被100整除的年份，或年份能被400整除的年份
         * 1073750999u = 0b 01 0000000000000000 10001111010111;
         * 3221352463u = 0b 11[A] 0000000000000 11111[B] 00000000 1111[C];
         * 0000126976u = 0b 00[A] 0000000000000 11111[B] 00000000 0000[C];
         * p&[A]!=0 => year%4!=0;
         * p&[B]!=B => year%100!=0;
         * p&[C]==0 => year%16==0 => year%400==0
         */
        constexpr bool is_leap_year() const noexcept { return ((year * 1073750999u) & 3221352463u) <= 126976u; }

        /// 今年的总天数 {365,366}
        constexpr xyu::int32 days_of_year() const noexcept { return is_leap_year() ? 366 : 365; }

        /// 本月的总天数 {28,29,30,31}
        constexpr xyu::int32 days_of_month() const noexcept
        { return days_per_month[month] + (month==2 && is_leap_year()); }

        /// 获取今日为今年的第几天 [1,365/366]
        constexpr xyu::int32 day_in_year() const noexcept
        { return days_before_month[month] + (month>2 && is_leap_year()) + day; }

        /**
         * @brief 获取距离1970年1月1日的天数 (可能为负数, 0表示1970年1月1日)
         * @details 基于 Neri 和 Schneider 的算法
         */
        constexpr xyu::int32 epoch_day() const noexcept
        {
            // 请将 -1468000u 视为一个整体，左手持剑(-)，右手持盾(u)
            xyu::uint32
            j = month < 3,
            y0 = year - -1468000u - j,
            m0 = j ? month + 12 : month,
            d0 = day - 1,

            q = y0 / 100,
            yc = 1461 * y0 / 4 - q + q / 4,
            mc = (979 * m0 - 2919) / 32,
            dc = d0;

            return static_cast<xyu::int32>(yc + mc + dc - 536895458u);
        }

        /// 获取星期几 [0,6] -> [星期一,星期日]
        constexpr xyu::int32 week_day() const noexcept { return (epoch_day() % 7 + 10) % 7; }

        /* 运算 */

        // 日期差值
        constexpr Duration_ms<> operator-(Calendar other) const noexcept
        {
            // uint8 及 uint16 计算时自动提升为 int
            Duration_ms<> d;
            d += Duration_day<>{epoch_day() - other.epoch_day()};
            d += Duration_hour<>{hour - other.hour};
            d += Duration_min<>{minute - other.minute};
            d += Duration_s<>{second - other.second};
            d += Duration_ms<>{ms() - other.ms()};
            return d;
        }
        // 日期增减
        template <typename T, xyu::uint64 Num, xyu::uint64 Den>
        constexpr friend Calendar operator+(Duration<T, Num, Den> duration, Calendar calendar) noexcept { return calendar + duration; }
        template <typename T, xyu::uint64 Num, xyu::uint64 Den>
        constexpr Calendar& operator+=(Duration<T, Num, Den> duration) noexcept
        {
            // 初始化
            Duration_day<T> dd = duration;
            auto ld = duration - dd;
            if (ld.count < 0) { --dd; ld += Duration_day<>{1}; }
            Duration_hour<T> dh = ld;
            auto lh = ld - dh;
            Duration_min<T> dm = lh;
            auto lm = lh - dm;
            Duration_s<T> ds = lm;
            auto ls = lm - ds;
            Duration_ms<T> dms = ls;
            xyu::uint32 tmp = 0;
            // 毫秒
            if (dms.count)
            {
                tmp = ms() + dms.count;
                ds.count += tmp / 1000u;
                ms(tmp % 1000u);
            }
            // 秒
            if (ds.count)
            {
                tmp = second + ds.count;
                dm.count += tmp / 60u;
                second = tmp % 60u;
            }
            // 分
            if (dm.count)
            {
                tmp = minute + dm.count;
                dh.count += tmp / 60u;
                minute = tmp % 60u;
            }
            // 时
            if (dh.count)
            {
                tmp = hour + dh.count;
                dd.count += tmp / 24u;
                hour = tmp % 24u;
            }
            // 年月日
            if (dd.count)
            {
                if constexpr (!XY_RELEASE)
                {
                    if (XY_UNLIKELY(dd.count > xyu::t_number<xyu::int32>::max || dd.count < xyu::t_number<xyu::int32>::min))
                        xyu::E_Logic::abort_out_range(XY_CurPos("Calendar::op+-"), dd.count, xyu::t_number<xyu::int32>::min, xyu::t_number<xyu::int32>::max);
                }
                xyu::int32 tmp2 = xyu::calc_of_add<xyu::int32,!XY_RELEASE>(day, (xyu::int32)dd.count, XY_CurPos("Calendar::op+-"));
                if (XY_UNLIKELY(month == 0))
                {
                    year = xyu::calc_of_sub<xyu::uint16,!XY_RELEASE>(year, 1, XY_CurPos("Calendar::op+-"));
                    month = 12u;
                }
                if (tmp2 <= 58 && tmp2 > -28)
                {
                    if (tmp2 <= 0)
                    {
                        if (--month == 0)
                        {
                            year = xyu::calc_of_sub<xyu::uint16,!XY_RELEASE>(year, 1, XY_CurPos("Calendar::op+-"));
                            month = 12u;
                        }
                        day = days_per_month[month] + (month == 2u && is_leap_year()) + tmp2;
                    }
                    else
                    {
                        xyu::uint32 limit = days_per_month[month] + (month == 2u && is_leap_year());
                        if (tmp2 > limit)
                        {
                            tmp2 -= limit;
                            if (++month > 12u)
                            {
                                year = xyu::calc_of_add<xyu::uint16,!XY_RELEASE>(year, 1, XY_CurPos("Calendar::op+-"));
                                month = 1u;
                            }
                        }
                        day = tmp2;
                    }
                }
                else from_epoch_day(xyu::calc_of_add<xyu::int32,!XY_RELEASE>(epoch_day(), (xyu::int32)dd.count, XY_CurPos("Calendar::+-")));
            }
            return *this;
        }
        template <typename T, xyu::uint64 Num, xyu::uint64 Den>
        constexpr Calendar& operator-=(Duration<T, Num, Den> duration) noexcept { return *this += -duration; }
        template <typename T, xyu::uint64 Num, xyu::uint64 Den>
        constexpr Calendar operator+(Duration<T, Num, Den> duration) const noexcept { return Calendar{*this} += duration; }
        template <typename T, xyu::uint64 Num, xyu::uint64 Den>
        constexpr Calendar operator-(Duration<T, Num, Den> duration) const noexcept { return Calendar{*this} -= duration; }

        /* 获取 */

        /// 获取当前 UTC 时间
        static Calendar utc() noexcept { return Calendar().from_epoch_duration(Duration_utc()); }
        /// 获取当前系统时间
        static Calendar sys() noexcept { return Calendar().from_epoch_duration(Duration_utc() + Duration_min<>{TimeDifference}); }

        /* 转换 */

        /// 转换为系统时间 (当前为 UTC时间)
        Calendar& to_sys() noexcept { return operator+=(Duration_min<>{TimeDifference}); }
        /// 转换为 UTC 时间 (当前为系统时间)
        Calendar& to_utc() noexcept { return operator-=(Duration_min<>{TimeDifference}); }

        /* 休眠 */

        /**
         * @brief 休眠到指定 系统 时间点
         * @details 过去的时间点则直接返回
         */
        void sleep_as_sys() const noexcept { (*this - Duration_min<>{TimeDifference}).sleep_as_utc(); }

        /**
         * @brief 休眠到指定 utc 时间点
         * @details 过去的时间点则直接返回
         */
        void sleep_as_utc() const noexcept { (*this - utc()).sleep(); }

        /* 比较 */
    #define XY_calender_compare_gen( op ) \
        constexpr bool operator op(const Calendar& other) const noexcept \
        { return xyu::mem_cast<xyu::uint64>(*this) op xyu::mem_cast<xyu::uint64>(other); }
    XY_PP_FORWARD(XY_calender_compare_gen, 1, ==, !=, >, >=, <, <=)
    #undef XY_calender_compare_gen

    private:
        // 每月天数 (平年) [1,12]
        static constexpr xyu::int8 days_per_month[] = { 0, 31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31 };
        // 每月积累年天数 (平年) [1,12]
        static constexpr xyu::int16 days_before_month[] = { 0, 0, 31, 59, 90, 120, 151, 181, 212, 243, 273, 304, 334, 365 };
    };
}

/// 计时器
namespace xylu::xysystem
{
    /**
     * @brief 高精度计时器
     * @details 用于测量代码块的执行流逝时间
     */
    struct Clock
    {
    private:
        Duration_10ns<> tmp = 0;    // 本次计时起点
        Duration_10ns<> all = 0;    // 总时间

    public:
        /* 计时 */

        // 开始计时
        void start() noexcept { tmp = Duration_any(); }
        // 获取本次经过时间
        Duration_10ns<> elapse() const noexcept { return Duration_any() - tmp; }
        // 停止计时
        Duration_10ns<> stop() noexcept { auto dd = elapse();  all += dd; return dd; }

        // 获取总时间
        Duration_10ns<> total() const noexcept { return all; }
        // 重置总时间
        void reset() noexcept { all = 0; }
    };
}

/// 其他属性
namespace xylu::xytraits
{
    // 获取更宽的类型
    template <typename T, xyu::uint64 Num, xyu::uint64 Den, typename U>
    struct t_wide_base<xysystem::Duration<T, Num, Den>, U, t_enable<t_is_integer<U>>>
    { using type = xysystem::Duration<t_wide<T, U>, Num, Den>; };
    template <typename T, xyu::uint64 Num, xyu::uint64 Den, typename U>
    struct t_wide_base<U, xysystem::Duration<T, Num, Den>, t_enable<t_is_integer<U>>>
    { using type = xysystem::Duration<t_wide<U, T>, Num, Den>; };
    template <typename T, xyu::uint64 Num1, xyu::uint64 Den1, typename U, xyu::uint64 Num2, xyu::uint64 Den2>
    struct t_wide_base<xysystem::Duration<T, Num1, Den1>, xysystem::Duration<U, Num2, Den2>, t_enable<!(xyu::t_is_same<T,U> && Num1 == Num2 && Den1 == Den2)>>
    { using type = decltype(t_val<xysystem::Duration<T, Num1, Den1>>() + t_val<xysystem::Duration<U, Num2, Den2>>()); };
}
