#pragma clang diagnostic push
#pragma ide diagnostic ignored "OCUnusedGlobalDeclarationInspection"
#pragma ide diagnostic ignored "modernize-use-nodiscard"
#pragma ide diagnostic ignored "google-explicit-constructor"
#pragma ide diagnostic ignored "UnreachableCallsOfFunction"
#pragma ide diagnostic ignored "ConstantConditionsOC"
#pragma ide diagnostic ignored "ConstantParameter"
#pragma ide diagnostic ignored "UnreachableCode"
#pragma ide diagnostic ignored "misc-no-recursion"
#pragma once

#include "../../link/numfun"
#include "../../link/string"

/* 时间 */

/// 时间段
namespace xylu::xysystem
{
    namespace __
    {
        /**
         * @brief 时间段进行const运算后的结果类型
         * @note 结果时间段的类型为 wider<T, U>
         * @note 若 T,U 均为整数，则结果倍率为 gcd(Scale, Scale2)，否则为 min(Scale, Scale2)
         */
        template <typename T, typename U, T Scale, U Scale2>
        struct duration_arithmetic_new
        {
            using type = xyu::t_wider<T, U>;
            static constexpr type unit = [](){
                if constexpr (Scale == Scale2) return Scale;
                else if constexpr (xyu::t_is_integral<type>) return xyu::calc_gcd<type, type>(Scale, Scale2);
                else return xyu::min<type, type>(Scale, Scale2);
            }();
        };

        /**
         * @brief 计算 du<T,Scale> ±= du<U,Scale2> 的结果
         * @note 若 Scale == Scale2，则直接将 y 加到 x 上并返回
         * @note 否则，如果 T,U 均为整数，则计算倍率时共同除去 gcd(Scale, Scale2) (防止溢出)；否则直接将y转换为Scale倍率加到x上并返回
         */
        template <typename T, typename U, T Scale, U Scale2>
        XY_CONST constexpr T duration_arithmetic_self(T x, U y) noexcept
        {
            // 同倍率
            if constexpr (Scale == Scale2) return x + y;
            // 不同倍率整数
            else if constexpr (xyu::t_is_integral<T> && xyu::t_is_integral<U>) {
                constexpr auto gcd = xyu::calc_gcd(Scale, Scale2);
                return x + y * (Scale2 / gcd) / (Scale / gcd);
            }
            // 不同倍率浮点数
            return x + y * (Scale2 / Scale);
        }
        
        /**
         * @brief 计算同倍率下的 x,y 比较值
         * @note 若 T,U 不相同，则转换为 wider<T, U> 进行比较 (记为 type)
         * @note 若 Scale == Scale2，则直接返回 (x,y)
         * @note 否则，如果 type 为整数，则将返回倍率为 gcd(Scale, Scale2) 的结果供比较 (防止溢出)；否则浮点数返回倍率为 Scale 的结果供比较
         */
        template <typename T, typename U, T Scale, U Scale2>
        XY_CONST constexpr auto duration_compare(T x, U y) noexcept
        {
            using type = xyu::t_wider<T, U>;
            // 不同类型
            if constexpr (!xyu::t_is_same<T, U>) return duration_compare<type, type, Scale, Scale2>(x, y);
            // 相同类型
            else {
                struct Result { T a, b; };
                // 同倍率
                if constexpr (Scale == Scale2) return Result{ x, y };
                // 不同倍率整数
                else if constexpr (xyu::t_is_integral<T>) {
                    constexpr auto gcd = xyu::calc_gcd(Scale, Scale2);
                    return Result{ x * (Scale / gcd), y * (Scale2 / gcd) };
                } 
                // 不同倍率浮点数
                else return Result{ x, y * (Scale2 / Scale) };
            }
        }

        /**
         * @brief 转换为指定倍率
         * @note 若 Scale == Scale2，则直接返回 x
         * @note 否则，如果 T 为整数，则将除去公共倍率后计算 (防止溢出)；否则浮点数直接乘上倍率之比 Scale / Scale2
         */
        template <typename T, T Scale, T Scale2>
        XY_CONST constexpr T duration_change(T x) noexcept
        {
            // 同倍率
            if constexpr (Scale == Scale2) return x;
            // 不同倍率整数
            else if constexpr (xyu::t_is_integral<T>)
            {
                constexpr auto gcd = xyu::calc_gcd(Scale, Scale2);
                // 防止最小负数溢出
                if (x < 0) return -(-(xyu::t_change_sign2un<T>)(x) * (Scale / gcd) / (Scale2 / gcd));
                else return x * (Scale / gcd) / (Scale2 / gcd);
            }
            // 不同倍率浮点数
            else return x * (Scale / Scale2);
        }
        
        
        xyu::int64 duration_utc();
        xyu::int64 duration_any();
        xyu::int64 duration_process();
        xyu::int64 duration_thread();
    }
    /**
     * @brief 时间段
     * @tparam T 时间段的存储类型 [必须为有符号整型或浮点型] (默认为 int64_t)
     * @tparam Scale 时间段的倍率 (基数为ns)
     * @value count 时间单位数
     * @function ns, us, ms, s, min, hour, day, week, month, year 转换为对应单位的数值
     * @function utc, any, process, thread 获取对应的时间段
     * @note 实际时间 = count * Scale (ns)  
     * @note Scale决定了精度，创建新对象时会保证精度，但原对象上的操作可能会导致精度丢失
     * @note T决定了存储的范围，超过限度的值可能导致溢出
     * @note 其中的 年月 取 400年 的均值
     * @note 可使用 `Duration_s`, `Duration_ms` 等预定义的模板
     * @note 可使用 `_ns`, `_us`, `_ms`, `_s` 等 作为后缀来构造 Duration 对象
     */
    /**
     * @brief 表示一个时间段，由一个计数值和一个编译期指定的单位（相对于纳秒）组成。
     * @details
     *   `Duration` 是一个强类型的时间表示。它通过模板参数 T（存储类型）和 Scale（单位倍率）来定义一个时间间隔。
     *
     *   - **T**: 决定了时间段的存储范围。必须使用有符号整数或浮点数。
     *   - **Scale**: 决定了时间段的精度，表示一个 count 单位等于多少纳秒。
     *     例如，`Duration<int, 1000>` 表示一个单位是 1000 纳秒（即 1 微秒）。
     *
     * @note
     *   - 算术运算会自动处理不同 Scale 之间的转换，并尝试保持最高精度。
     *   - 提供了 `_ns`, `_us`, `_s` 等用户定义字面量，方便地创建 `Duration` 对象。
     *
     * @tparam T     用于存储计数值的算术类型。
     * @tparam Scale 每个计数值代表的纳秒数。
     */
    template <typename T, T Scale>
    struct Duration
    {
        static_assert((xyu::t_is_integral<T> && xyu::t_is_signed<T>) || xyu::t_is_floating<T>);
        static_assert(Scale >= 1);
        using uT = xyu::t_change_sign2un<T>;

        constexpr static T scale [[maybe_unused]] = Scale;

    public:
        T count;   // 时间单位数
        
    public:
        // 数值构造
        constexpr Duration(T count = 0) noexcept : count{count} {}
        // 其他时间段构造
        template <typename U, U Scale2>
        constexpr Duration(const Duration<U, Scale2>& other) noexcept
        : count(__::duration_change<U, Scale2, Scale>(other.count)) {}

        // 数值赋值
        constexpr Duration& operator=(T cnt) noexcept { count = cnt; return *this; }
        // 其他时间段赋值
        template <typename U, U Scale2>
        constexpr Duration& operator=(const Duration<U, Scale2>& other) noexcept
        {
            count = __::duration_change<U, Scale2, Scale>(other.count);
            return *this;
        }

        // 自运算
        constexpr Duration operator-() const noexcept { return -count; }
        constexpr Duration& operator++() noexcept { ++count; return *this; }
        constexpr Duration& operator--() noexcept { --count; return *this; }
        constexpr Duration operator++(int) noexcept { return count++; }
        constexpr Duration operator--(int) noexcept { return count--; }
        
        // 倍率运算
        template <typename U, typename = xyu::t_enable<xyu::t_is_number<U>>>
        constexpr Duration operator*(U n) const noexcept { return count * n; }
        template <typename U, typename = xyu::t_enable<xyu::t_is_number<U>>>
        constexpr Duration operator/(U n) const noexcept { return count / n; }
        template <typename U, typename = xyu::t_enable<xyu::t_is_number<U>>>
        constexpr Duration operator%(U n) const noexcept { return count % n; }
        template <typename U, typename = xyu::t_enable<xyu::t_is_number<U>>>
        constexpr Duration& operator*=(U n) noexcept { count *= n; return *this; }
        template <typename U, typename = xyu::t_enable<xyu::t_is_number<U>>>
        constexpr Duration& operator/=(U n) noexcept { count /= n; return *this; }
        template <typename U, typename = xyu::t_enable<xyu::t_is_number<U>>>
        constexpr Duration& operator%=(U n) noexcept { count %= n; return *this; }

        // 加减运算
        template <typename U, U Scale2>
        constexpr auto operator+(const Duration<U, Scale2>& other) const noexcept
        {
            using t = __::duration_arithmetic_new<T, U, Scale, Scale2>;
            return Duration<typename t::type, t::unit>((uT)count * (Scale/t::unit) + (uT)other.count * (Scale2/t::unit));
        }
        template <typename U, U Scale2>
        constexpr Duration& operator+=(const Duration<U, Scale2>& other) noexcept
        {
            count = __::duration_arithmetic_self<T, U, Scale, Scale2>(count, other.count);
            return *this;
        }
        template <typename U, U Scale2>
        constexpr auto operator-(const Duration<U, Scale2>& other) const noexcept { return *this + (-other); }
        template <typename U, U Scale2>
        constexpr Duration& operator-=(const Duration<U, Scale2>& other) noexcept { return *this += (-other); }

        // 比较
        template <typename U, U Scale2>
        constexpr bool operator==(const Duration<U, Scale2>& other) const noexcept
        { 
            auto r = __::duration_compare<T, U, Scale, Scale2>(count, other.count);
            return r.a == r.b;
        }
        template <typename U, U Scale2>
        constexpr bool operator!=(const Duration<U, Scale2>& other) const noexcept
        {
            auto r = __::duration_compare<T, U, Scale, Scale2>(count, other.count);
            return r.a != r.b;
        }
        template <typename U, U Scale2>
        constexpr bool operator< (const Duration<U, Scale2>& other) const noexcept
        {
            auto r = __::duration_compare<T, U, Scale, Scale2>(count, other.count);
            return r.a <  r.b;
        }
        template <typename U, U Scale2>
        constexpr bool operator<=(const Duration<U, Scale2>& other) const noexcept
        {
            auto r = __::duration_compare<T, U, Scale, Scale2>(count, other.count);
            return r.a <= r.b;
        }
        template <typename U, U Scale2>
        constexpr bool operator> (const Duration<U, Scale2>& other) const noexcept
        {
            auto r = __::duration_compare<T, U, Scale, Scale2>(count, other.count);
            return r.a >  r.b;
        }
        template <typename U, U Scale2>
        constexpr bool operator>=(const Duration<U, Scale2>& other) const noexcept
        {
            auto r = __::duration_compare<T, U, Scale, Scale2>(count, other.count);
            return r.a >= r.b;
        }

        // 转换
        constexpr T ns()   const noexcept { return __::duration_change<T, Scale, 1>(count); }
        constexpr T us()   const noexcept { return __::duration_change<T, Scale, 1000>(count); }
        constexpr T ms()   const noexcept { return __::duration_change<T, Scale, 1000000>(count); }
        constexpr T s()    const noexcept { return __::duration_change<T, Scale, 1000000000>(count); }
        constexpr T min()  const noexcept { return __::duration_change<T, Scale, 60000000000>(count); }
        constexpr T hour() const noexcept { return __::duration_change<T, Scale, 3600000000000>(count); }
        constexpr T day()  const noexcept { return __::duration_change<T, Scale, 86400000000000>(count); }
        constexpr T week() const noexcept { return __::duration_change<T, Scale, 604800000000000>(count); }
        constexpr T month()const noexcept { return __::duration_change<T, Scale, 2629746000000000>(count); }
        constexpr T year() const noexcept { return __::duration_change<T, Scale, 31556952000000000>(count); }

        /* 获取 */

        /**
         * @brief 获取当前 utc 时间
         * @note 从1970年1月1日00:00:00 UTC 时间开始的纳秒数
         */
        static Duration utc() { return __::duration_change<T, 1, Scale>(__::duration_utc()); }

        /**
         * @brief 获取从某时刻开始的时长
         * @note 与系统时间无关
         */
        static Duration any() { return __::duration_change<T, 1, Scale>(__::duration_any()); }

        /**
         * @brief 获取当前进程在系统中的运行时长
         */
        static Duration process() { return __::duration_change<T, 1, Scale>(__::duration_process()); }

        /**
         * @brief 获取当前线程在系统中的运行时长
         */
        static Duration thread() { return __::duration_change<T, 1, Scale>(__::duration_thread()); }

        /* 休眠 */

        /**
         * @brief 休眠指定时间
         * @note 若 count 为0或负数则直接返回
         */
        void sleep() const;

        /**
         * @brief 休眠到距离 1970 年 1 月 1 日 count 的时间点
         * @note 若 count 小于当前的 UTC 时间则直接返回
         */
        void sleep_to() const;
    };
    
    using Duration_ns   = Duration<xyu::int64, 1>;
    using Duration_us   = Duration<xyu::int64, 1000>;
    using Duration_ms   = Duration<xyu::int64, 1000000>;
    using Duration_s    = Duration<xyu::int64, 1000000000>;
    using Duration_min  = Duration<xyu::int64, 60000000000>;
    using Duration_hour = Duration<xyu::int64, 3600000000000>;
    using Duration_day  = Duration<xyu::int64, 86400000000000>;
    using Duration_week = Duration<xyu::int64, 604800000000000>;
    using Duration_month= Duration<xyu::int64, 2629746000000000>;
    using Duration_year = Duration<xyu::int64, 31556952000000000>;

    /**
     * @brief 获取当前 utc 时间
     * @note 从1970年1月1日00:00:00 UTC 时间开始的纳秒数
     */
    inline Duration_ns Duration_utc() { return __::duration_utc(); }

    /**
     * @brief 获取 utc时间 与 系统时间的差值
     * @note 单位为分钟
     */
    constexpr Duration_min Duration_utcdiff() noexcept { return {static_cast<xyu::int64>(xyu::K_TIME_DIFFERENCE * 60.)}; }

    /**
     * @brief 获取从某时刻开始的时长
     * @note 与系统时间无关
     */
    inline Duration_ns Duration_any() { return __::duration_any(); }

    /** @brief 获取当前进程在系统中的运行时长 */
    inline Duration_ns Duration_process() { return __::duration_process(); }

    /** @brief 获取当前线程在系统中的运行时长 */
    inline Duration_ns Duration_thread() { return __::duration_thread(); }

    // 构造 ns 时间段对象
    constexpr Duration_ns    operator"" _ns   (unsigned long long ns)   noexcept { return static_cast<long long>(ns);   }
    // 构造 us 时间段对象
    constexpr Duration_us    operator"" _us   (unsigned long long us)   noexcept { return static_cast<long long>(us);   }
    // 构造 ms 时间段对象
    constexpr Duration_ms    operator"" _ms   (unsigned long long ms)   noexcept { return static_cast<long long>(ms);   }
    // 构造 s 时间段对象
    constexpr Duration_s     operator"" _s    (unsigned long long s)    noexcept { return static_cast<long long>(s);    }
    // 构造 min 时间段对象
    constexpr Duration_min   operator"" _min  (unsigned long long min)  noexcept { return static_cast<long long>(min);  }
    // 构造 hour 时间段对象
    constexpr Duration_hour  operator"" _hour (unsigned long long hour) noexcept { return static_cast<long long>(hour); }
    // 构造 day 时间段对象
    constexpr Duration_day   operator"" _day  (unsigned long long day)  noexcept { return static_cast<long long>(day);  }
    // 构造 week 时间段对象
    constexpr Duration_week  operator"" _week (unsigned long long week) noexcept { return static_cast<long long>(week); }
    // 构造 month 时间段对象
    constexpr Duration_month operator"" _month(unsigned long long month)noexcept { return static_cast<long long>(month);}
    // 构造 year 时间段对象
    constexpr Duration_year  operator"" _year (unsigned long long year) noexcept { return static_cast<long long>(year); }
}

/// 日期
namespace xylu::xysystem
{
    /**
     * @brief 表示一个具体的公历日期和时间。
     * @details
     *   `Calendar` 是一个 POD (Plain Old Data) 结构体，用于存储年月日时分秒毫秒。
     *   它提供了一系列方法用于日期计算（如判断闰年、计算星期几）和与 `Duration` 的相互转换。
     *
     * @note
     *   **毫秒 (`ms`) 的存储**: 为了节省空间，毫秒字段 `milli` 采用了一种特殊的压缩存储。
     *   - `[0, 127]` 范围内的毫秒值被精确存储。
     *   - `[130, 990]` 范围内的毫秒值被存储为 `value / 10`，精度为 10 毫秒。
     *   请始终使用 `ms()` 和 `ms(new_ms)` 方法来安全地访问和修改毫秒值。
     *
     * @attention 注意存储的数值范围，过大或为负数会导致溢出或异常 (如 year 必须为为 [0,65535])
     */
    struct alignas(xyu::uint64) Calendar
    {
        constexpr static xyu::StringView DefaultFormat = "%C"; // 默认格式化字符串
    public:
        xyu::uint16 year;   // 年
        xyu::uint8  month;  // 月
        xyu::uint8  day;    // 日
        xyu::uint8  hour;   // 时
        xyu::uint8  minute; // 分
        xyu::uint8  second; // 秒
    private:
        xyu::uint8  milli;  // 毫秒 (非直接精确存储)

    public:
        /* 构造 */

        /**
         * @brief 构造函数
         * @note 所有参数必须为正数，且必须在合理范围内，否则会导致溢出或异常
         * @note ms的存储方式为: 当最高位为0时，后7位精确存储[0,127]；当最高位为1时，后7位为10倍[130,990] (广义无效范围为[0,1270])
         */
        constexpr Calendar(xyu::uint16 year = 1970u, xyu::uint8 month = 1u, xyu::uint8 day = 1u,
                           xyu::uint8 hour = 0u, xyu::uint8 minute = 0u, xyu::uint8 second = 0u,
                           xyu::uint16 milli = 0u) noexcept
            : year{year}, month{month}, day{day}, hour{hour}, minute{minute}, second{second}, milli{} { ms(milli); }

        /**
         * @brief 从距离1970年1月1日的 天数 构造日期
         * @note 基于 Neri 和 Schneider 的算法
         */
        constexpr Calendar& from_epoch_day(xyu::int32 days) noexcept
        {
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
            q3 = n3 / (1ull << 16),
            r3 = n3 % (1ull << 16) / 2141;

            year = 100 * q1 + q2 + j + -1468000u;
            month = j ? q3 - 12 : q3;
            day = r3 + 1;
            return *this;
        }

        /**
         * @brief 从距离1970年1月1日的 时间戳 构造日期
         */
        template <typename T, T Scale>
        constexpr Calendar& from_epoch_duration(const Duration<T, Scale>& duration) noexcept
        {
            Duration<T, 1000000> d0 = duration;
            // 年月日
            Duration<T, 86400000000000> d1 = d0;
            from_epoch_day(d1.count);
            d0 -= d1;
            // 时
            Duration<T, 3600000000000> d2 = d0;
            hour = d2.count;
            d0 -= d2;
            // 分
            Duration<T, 60000000000> d3 = d0;
            minute = d3.count;
            d0 -= d3;
            // 秒
            Duration<T, 1000000000> d4 = d0;
            second = d4.count;
            d0 -= d4;
            // 毫秒
            Duration<T, 1000000> d5 = d0;
            ms(d5.count);
            return *this;
        }

        /**
         * @brief 获取ms ([0,127]精度为1，[130,990]精度为10)
         */
        constexpr xyu::uint32 ms() const noexcept
        { return milli & 0b01111111u ? (milli & 0b01111111u) * 10u : 0u; }

        /**
         * @brief 设置ms ([0,127]精度为1，[130,990]精度为10)
         * @note 精度为10时采取四舍五入
         */
        constexpr void ms(xyu::uint32 new_ms) noexcept
        { milli = (new_ms <= 127u ? new_ms : ((new_ms+5)/10u | 0b10000000u)); }

        /* 判断与修正 */
        /**
         * @brief 是否为闰年
         * @note 闰年: 年份能被4整除但不能被100整除的年份，或年份能被400整除的年份
         * @note 此方法仅适用于 [0,102499] (满足 [0,65535] 的范围)
         * @note
         * 1073750999u = 0b 01 0000000000000000 10001111010111 <br>
         * 3221352463u = 0b 11[A] 0000000000000 11111[B] 00000000 1111[C] <br>
         * 0000126976u = 0b 00[A] 0000000000000 11111[B] 00000000 0000[C] <br>
         * p&[A]!=0 => year%4!=0,
         * p&[B]!=B => year%100!=0,
         * p&[C]==0 => year%16==0 => year%400==0
         */
        constexpr bool leap_year() const noexcept
        { return ((year * 1073750999u) & 3221352463u) <= 126976u; }

        /**
         * @brief 是否为有效日期，即每个参数是否都在合理范围内
         */
        constexpr bool valid() const noexcept
        {
            return (month >= 1u && month <= 12u) &&
                   (day >= 1u && day <= days_of_month[month - 1u] + (month == 2u && leap_year())) &&
                   (hour <= 23u) && (minute <= 59u) && (second <= 59u) && (milli <= 0b11100011u);
        }

        /**
         * @brief 修正日期，使其符合有效范围
         * @note 先从ms修正到h，从mon修正到y，最后修正d (需根据mon与y)
         */
        constexpr Calendar& fix() noexcept
        {
            // 修正毫秒
            xyu::uint32 overflow = milli > 0b11100011u; // 10*99 (990)
            if (overflow) milli = milli > 0b11110000u ? // 10*112 (1120 -> 1s + 120，即精确表示的界限)
                    milli - 0b01100100u : 10u * (milli & 0b01111111u) + 120u;
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
            year += month / 12u;
            month %= 12u;
            // 修正日(月年)
            tmp = day + overflow;
            for(;;) {
                xyu::uint32 limit = days_of_month[month - 1u] + (month == 2u && leap_year());
                if (tmp <= limit) break;
                tmp -= limit;
                month++;
                if (month > 12u) { year++; month = 1u; }
            }
            return *this;
        }

        /* 计算信息 */

        /**
         * @brief 获取本月的总天数 {28,29,30,31}
         * @note 表示数量，返回无符号整型
         */
        constexpr xyu::uint month_days() const noexcept
        { return days_of_month[month-1] + (month==2 && leap_year()); }

        /**
         * @brief 获取本年的总天数 {365,366}
         * @note 表示数量，返回无符号整型
         */
        constexpr xyu::uint year_days() const noexcept
        { return leap_year() ? 366u : 365u; }

        /**
         * @brief 获取本年的第几天 [1,365/366]
         * @note 表示偏移，返回有符号整型
         */
        constexpr xyu::int32 year_day() const noexcept
        { return days_of_year[month-1] + (month>2 && leap_year()) + day; }

        /**
         * @brief 获取距离1970年1月1日的天数 (可能为负数, 0表示1970年1月1日)
         * @note 基于 Neri 和 Schneider 的算法
         * @note 表示偏移，返回有符号整型
         */
        constexpr xyu::int32 epoch_day() const noexcept
        {
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

        /**
         * @brief 获取星期几 [0,6] -> [星期一,星期日]
         * @note 表示偏移，返回有符号整型
         */
        constexpr xyu::int32 week_day() const noexcept
        {
            auto n = epoch_day();
            return n >= -3 ? (n + 3) % 7 : (n + 10) % 7;
        }

        /* 比较 */

        constexpr bool operator==(const Calendar& other) const noexcept
        {
            return year == other.year && month == other.month && day == other.day && hour == other.hour &&
                   minute == other.minute && second == other.second && milli == other.milli;
        }
        constexpr bool operator!=(const Calendar& other) const noexcept { return !(*this == other); }
        constexpr bool operator< (const Calendar& other) const noexcept
        {
            if (year < other.year) return true;
            if (year > other.year) return false;
            if (month < other.month) return true;
            if (month > other.month) return false;
            if (day < other.day) return true;
            if (day > other.day) return false;
            if (hour < other.hour) return true;
            if (hour > other.hour) return false;
            if (minute < other.minute) return true;
            if (minute > other.minute) return false;
            if (second < other.second) return true;
            if (second > other.second) return false;
            return milli < other.milli;
        }
        constexpr bool operator<=(const Calendar& other) const noexcept
        {
            if (year > other.year) return false;
            if (year < other.year) return true;
            if (month > other.month) return false;
            if (month < other.month) return true;
            if (day > other.day) return false;
            if (day < other.day) return true;
            if (hour > other.hour) return false;
            if (hour < other.hour) return true;
            if (minute > other.minute) return false;
            if (minute < other.minute) return true;
            if (second > other.second) return false;
            if (second < other.second) return true;
            return milli <= other.milli;
        }
        constexpr bool operator> (const Calendar& other) const noexcept { return !(*this <= other); }
        constexpr bool operator>=(const Calendar& other) const noexcept { return !(*this < other); }

        /* 运算 */
        constexpr Duration_ms operator-(const Calendar& other) const noexcept
        {
            if (*this < other) return -(other - *this);
            xyu::uint32
            d = epoch_day() - other.epoch_day(),
            h = hour - other.hour,
            m = minute - other.minute,
            s = second - other.second;
            return (ms() - other.ms()) + s * 1000 + m * 60000 + h * 3600000ll + d * 86400000ll;
        }
        template <typename T, T Scale>
        constexpr Calendar& operator+=(const Duration<T, Scale>& duration) noexcept
        {
            // 初始化
            Duration<T, 1000000> dms = duration;
            Duration<T, 86400000000000> dd = dms;
            if constexpr (Scale < 86400000000000u)
                if (dms.count < 0) dd--; dms -= dd;
            Duration<T, 3600000000000> dh = dms; dms -= dh;
            Duration<T, 60000000000> dm = dms; dms -= dm;
            Duration<T, 1000000000> ds = dms; dms -= ds;
            xyu::uint32 overflow = 0, tmp = 0;
            // 毫秒
            if (dms.count) {
                tmp = ms() + dms.count;
                overflow = tmp / 1000;
                tmp %= 1000;
                ms(tmp);
            }
            // 秒
            if (ds.count + overflow) {
                tmp = second + ds.count + overflow;
                overflow = tmp / 60u;
                second = tmp % 60u;
            }
            // 分
            if (dm.count + overflow) {
                tmp = minute + dm.count + overflow;
                overflow = tmp / 60u;
                minute = tmp % 60u;
            }
            // 时
            if (dh.count + overflow) {
                tmp = hour + dh.count + overflow;
                overflow = tmp / 24u;
                hour = tmp % 24u;
            }
            // 年月日
            if (dd.count + overflow) from_epoch_day(epoch_day() + dd.count + overflow);
            return *this;
        }
        template <typename T, T Scale>
        constexpr Calendar& operator-=(const Duration<T, Scale>& duration) noexcept { return *this += -duration; }
        template <typename T, T Scale>
        constexpr Calendar operator+(const Duration<T, Scale>& duration) const noexcept { return Calendar(*this) += duration; }
        template <typename T, T Scale>
        constexpr Calendar operator-(const Duration<T, Scale>& duration) const noexcept { return Calendar(*this) -= duration; }
        template <typename T, T Scale>
        constexpr friend Calendar operator+(const Duration<T, Scale>& duration, const Calendar& calendar) noexcept { return calendar + duration; }
        template <typename T, T Scale>
        constexpr friend Calendar operator-(const Duration<T, Scale>& duration, const Calendar& calendar) noexcept { return calendar - duration; }

        /* 获取 */

        /**
         * @brief 获取当前 UTC 时间
         */
        static Calendar utc() { return Calendar().from_epoch_duration(Duration_utc()); }

        /**
         * @brief 获取当前系统时间
         * @note 可以在 config.h 中修改 K_TIME_DIFFERENCE (即时差，单位为小时)
         */
        static Calendar now() { return Calendar().from_epoch_duration(Duration_utc() + Duration_utcdiff()); }

        /* 休眠 */

        /**
         * @brief 休眠到指定时间点 (使用系统时间)
         * @note 若 target 过去的时间点则直接返回
         */
        void sleep_to() const;

    public:
        // 每月天数 (平年)
        static constexpr xyu::uint8 days_of_month[] = { 31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31 };
        // 每月积累年天数 (平年)
        static constexpr xyu::uint16 days_of_year[] = { 0, 31, 59, 90, 120, 151, 181, 212, 243, 273, 304, 334, 365 };
        // 月份英文缩写
        static constexpr xyu::StringView str_month_abbr[] = { "Jan", "Feb", "Mar", "Apr", "May", "Jun", "Jul", "Aug", "Sep", "Oct", "Nov", "Dec" };
        // 月份英文全称
        static constexpr xyu::StringView str_month_full[] = { "January", "February", "March", "April", "May", "June", "July", "August", "September", "October", "November", "December" };
        // 星期英文缩写
        static constexpr xyu::StringView str_week_abbr[] = { "Mon", "Tue", "Wed", "Thu", "Fri", "Sat", "Sun" };
        // 星期英文全称
        static constexpr xyu::StringView str_week_full[] = { "Monday", "Tuesday", "Wednesday", "Thursday", "Friday", "Saturday", "Sunday" };
    };
}

/// 计时器
namespace xylu::xysystem
{
    /**
     * @brief 一个用于测量时间间隔的高精度计时器。
     * @details
     *   `Clock` 提供了两种计时模式：
     *   - **单段计时**: `start()` -> `past()` 模式，用于测量从 `start()` 被调用开始到当前的时间。
     *     `past()` 不会重置计时器。
     *   - **累计计时**: `start()` -> `stop()` -> `start()` -> `stop()` ... 模式。
     *     每次 `stop()` 会将当前时间段累加到总时间中。`total()` 用于获取累计总时间。
     */
    struct Clock
    {
    private:
        Duration_ns tick = 0;   // 当前连续时间起点
        Duration_ns all = 0;    // 总时间

    public:
        Clock() noexcept { start(); }

        /* 计时 */

        /**
         * @brief 开始计时
         */
        void start() noexcept { tick = Duration_utc(); }
        /**
         * @brief 获取本次持续时间
         */
        Duration_ns past() noexcept { return Duration_utc() - tick; }
        /**
         * @brief 停止计时并返回本次持续时间
         */
        Duration_ns stop() noexcept { Duration_ns pt = past(); all += pt; return pt; }
        /**
         * @brief 获取总时间
         */
        Duration_ns total() const noexcept { return all; }
        /**
         * @brief 重置总时间
         */
        void reset() noexcept { all = 0; }
        /**
         * @brief 重置总时间并开始计时
         */
        void restart() noexcept { reset(); start(); }

        /* 休眠 */

        /**
         * @brief 休眠指定时间
         * @note 若 dt 为0或正数则直接返回
         */
        template <typename T, T Scale>
        static void sleep(Duration<T, Scale> dt)
        {
            if (XY_UNLIKELY(dt.count <= 0)) return;
            Duration<T, 1000000000> s = dt;
            Duration<T, 1> ns = dt - s;
            sleep_help(s.count, ns.count);
        }

        /**
         * @brief 休眠到指定时间点 (使用系统时间)
         * @note 若 target 过去的时间点则直接返回
         */
        static void sleep_to(const Calendar& target) { sleep(target - Calendar::now()); }

        /**
         * @brief 休眠到距离 1970 年 1 月 1 日 dt_utc 的时间点
         * @note 若 dt_utc 小于当前的 UTC 时间则直接返回
         */
        template <typename T, T Scale>
        static void sleep_to(Duration<T, Scale> dt_utc) { sleep(dt_utc - Duration_utc()); }

    private:
        static void sleep_help(xyu::uint32 s, xyu::uint32 ns);
    };

    template <typename T, T Scale>
    void Duration<T, Scale>::sleep() const { Clock::sleep(*this); }

    template <typename T, T Scale>
    void Duration<T, Scale>::sleep_to() const { Clock::sleep_to(*this); }

    inline void Calendar::sleep_to() const { Clock::sleep_to(*this); }
}

/// 属性
namespace xylu::xytraits
{
    // 获取更宽的类型
    template <typename T, typename U, T S1, U S2>
    struct t_wider_base<xylu::xysystem::Duration<T, S1>, xylu::xysystem::Duration<U, S2>>
    {
        using type = decltype(xylu::xysystem::Duration<T, S1>{} + xylu::xysystem::Duration<U, S2>{});
    };
    template <typename T, T S1, typename U>
    struct t_wider_base<xylu::xysystem::Duration<T, S1>, U>
    {
        using type = decltype(xylu::xysystem::Duration<t_wider<T, U>, S1>{} + U{});
    };
    template <typename T, T S1, typename U>
    struct t_wider_base<U, xylu::xysystem::Duration<T, S1>>
    {
        using type = decltype(xylu::xysystem::Duration<t_wider<T, U>, S1>{} + U{});
    };
}

#pragma clang diagnostic pop