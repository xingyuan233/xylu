#pragma once

/**
 * @file    xymath/random.h
 * @brief   随机数
 * @date    2026-08-28
 *
 * @details
 *   依赖库：
 *   - Windows下需要链接 bcrypt
 */

#include "./func.h"
#include "./calc.h"
#include "../xytraits/func.h"
#include "../xytraits/numeric.h"

/// 随机数生成器
namespace xylu::xymath
{
    /**
     * @brief 加密随机数获取
     * @details
     *   获取方式
     *   - windows下通过 BCryptGenRandom
     *   - 类Unix下通过 getrandom
     */
    struct RandSafe
    {
        /// 获取任意长度的强随机数
        void operator()(void* buf, xyu::uint32 len) noexcept { rand(buf, len); }

        /// 获取任意类型的强随机数
        template <typename T>
        void operator()(T& val) noexcept { rand(&val, sizeof(T)); }

        /// 获取任意类型的强随机数
        template <typename T>
        static T get() noexcept { T tmp; rand(&tmp, sizeof(T)); return tmp; }

        template <typename T>
        struct RandAnyGen { T operator()() noexcept { T tmp; rand(&tmp, sizeof(T)); return tmp; } };
        /// 获取一个生成指定类型的强随机数生成器
        template <typename T>
        static RandAnyGen<T> gen() noexcept { return {}; }

    private:
        static void rand(void* buf, xyu::uint32 len) noexcept;
    };

    /**
     * @brief PCG32 - [0,U32_MAX]
     * @details 高速高质量非加密随机数生成器
     */
    struct RandPCG32
    {
    private:
        const xyu::uint64 inc;
        xyu::uint64 state;

    public:
        /// 种子初始化
        constexpr explicit RandPCG32(xyu::uint64 initstate = RandSafe::get<xyu::uint64>(),
                                       xyu::uint64 initseq = 1442695040888963407ULL >> 1) noexcept
        : inc((initseq << 1) | 1), state(inc + initstate) { operator()(); }

        /// LCG状态转移 + XSH-RR置换
        constexpr xyu::uint32 operator()() noexcept
        {
            xyu::uint64 sta = state;
            state = sta * 6364136223846793005ULL + inc;
            xyu::uint32 tmp = ((sta >> 18) ^ sta) >> 27;
            xyu::uint32 rot = sta >> 59;
            return (tmp >> rot) | (tmp << (-rot & 31));
        }
    };

    /**
     * @brief PCG64DXSM - [0,U64_MAX]
     * @details 高速高质量非加密随机数生成器
     */
    struct RandPCG64
    {
    private:
#ifdef __SIZEOF_INT128__
        const xyu::uint128 inc;
        xyu::uint128 state;

    public:
        /// 完整种子初始化
        constexpr explicit RandPCG64(xyu::uint64 initstate_high, xyu::uint64 initstate_low,
                                     xyu::uint64 initseq_high, xyu::uint64 initseq_low) noexcept
        : inc((((xyu::uint128(initseq_high) << 64) | initseq_low) << 1) | 1),
          state(inc + ((xyu::uint128(initstate_high) << 64) | initstate_low)) { operator()(); }

        /// 部分种子初始化
        constexpr explicit RandPCG64(xyu::uint64 initstate_high, xyu::uint64 initstate_low) noexcept
        : inc((xyu::uint128(0x5851F42D4C957F2DULL) << 64) | 0x14057B7EF767814FULL),
          state(inc + ((xyu::uint128(initstate_high) << 64) | initstate_low)) { operator()(); }

        /// 默认种子初始化
        explicit RandPCG64() noexcept
        : inc((xyu::uint128(0x5851F42D4C957F2DULL) << 64) | 0x14057B7EF767814FULL),
          state(inc + RandSafe::get<xyu::uint128>()) { operator()(); }

        /// LCG状态转移 + DXSM输出
        constexpr xyu::uint64 operator()() noexcept
        {
            xyu::uint128 sta = state;
            state = sta * 0xDA942042E4DD58B5ULL + inc;
            xyu::uint64 high = sta >> 64;
            xyu::uint64 low = sta | 1;
            high = high ^ (high >> 32);
            high = high * 0xDA942042E4DD58B5ULL;
            high = high ^ (high >> 48);
            return high * low;
        }

#else
        const xyu::uint64 inc[2];
        xyu::uint64 state[2];

    public:
        /// 完整种子初始化
        explicit RandPCG64(xyu::uint64 initstate_high, xyu::uint64 initstate_low,
                             xyu::uint64 initseq_high, xyu::uint64 initseq_low) noexcept
        : inc{initseq_high << 1 | initseq_low >> 63, initseq_low << 1 | 1},
          state{inc[0] + initstate_high, inc[1] + initstate_low} { operator()(); }

        /// 部分种子初始化
        explicit RandPCG64(xyu::uint64 initstate_high, xyu::uint64 initstate_low) noexcept
        : inc{0x5851F42D4C957F2DULL, 0x14057B7EF767814FULL},
          state{inc[0] + initstate_high, inc[1] + initstate_low} { operator()(); }

        /// 默认种子初始化
        explicit RandPCG64() noexcept
        : inc{0x5851F42D4C957F2DULL, 0x14057B7EF767814FULL}, state{inc[0], inc[1]}
        {
            xyu::uint64 initstate[2];
            RandSafe{}(initstate);
            state[0] += initstate[0];
            state[1] += initstate[1];
            operator()();
        }

        /// LCG状态转移 + DXSM输出
        xyu::uint64 operator()() noexcept;
#endif
    };

}

/// 范围转换器
namespace xylu::xymath
{
    /**
     * @brief 随机整数转换器
     * @tparam RandGenI 底层整数生成器 [0,UMAX]
     * @details 32位转为u32，64位转为u64
     */
    template <typename RandGenI>
    class RandInt
    {
        using I = decltype(xyu::t_val<RandGenI&>()());
        static_assert(xyu::t_is_same_one<I, xyu::uint32, xyu::uint64>);
        using U = xyu::t_cond<sizeof(I) == 4, xyu::uint32, xyu::uint64>;
    private:
        RandGenI gen;

    public:
        /// 初始化推导
        constexpr RandInt(RandGenI gen) noexcept(xyu::t_can_nothrow_cpconstr<RandGenI>) : gen(gen) {}

        /// 初始化生成器
        template <typename... Args, typename = xyu::t_enable<xyu::t_can_listinit<RandGenI, Args...>>>
        explicit constexpr RandInt(Args... args) noexcept(xyu::t_can_nothrow_listinit<RandGenI, Args...>) : gen{args...} {}

        /// 生成 [0,UMAX]
        constexpr U operator()() noexcept(xyu::t_can_nothrow_call<RandGenI&>) { return gen(); }

        /// 生成 [0,b)
        constexpr U operator()(U b) noexcept(xyu::t_can_nothrow_call<RandGenI&>)
        {
            U threshold = xyu::t_number<U>::max / b * b;

            for (;;)
            {
                U v = operator()();
                if (v >= threshold) continue;
                return v % b;
            }
        }

        /// 生成 [a,b)
        constexpr U operator()(U a, U b) noexcept(xyu::t_can_nothrow_call<RandGenI&>)
        {
            U range = b - a;
            U threshold = xyu::t_number<U>::max / range * range;

            for (;;)
            {
                U v = operator()();
                if (v >= threshold) continue;
                return a + v % range;
            }
        }

        /// 生成 [0,b]
        constexpr U operator()(xyu::special_t, U b) noexcept(xyu::t_can_nothrow_call<RandGenI&>)
        {
            if (XY_UNLIKELY(b == xyu::t_number<U>::max)) return operator()();
            return operator()(b + 1);
        }

        /// 生成 [a,b]
        constexpr U operator()(xyu::special_t, U a, U b) noexcept(xyu::t_can_nothrow_call<RandGenI&>)
        {
            if (XY_UNLIKELY(a == 0 && b == xyu::t_number<U>::max)) return operator()();
            return operator()(a, b + 1);
        }
    };
    // 推导指引
    template <typename Gen>
    RandInt(Gen&&) -> RandInt<Gen>;


    /**
     * @brief 随机浮点数转换器
     * @tparam RandGenI 底层整数生成器 [0,UMAX]
     * @details 32位/64位转为 float
     */
    template <typename RandGenI>
    class RandFloat
    {
        using I = decltype(xyu::t_val<RandGenI&>()());
        static_assert(xyu::t_is_same_one<I, xyu::uint32, xyu::uint64>);
    private:
        RandGenI gen;

    public:
        /// 初始化推导
        constexpr RandFloat(RandGenI gen) noexcept(xyu::t_can_nothrow_cpconstr<RandGenI>) : gen(gen) {}

        /// 初始化生成器
        template <typename... Args, typename = xyu::t_enable<xyu::t_can_listinit<RandGenI, Args...>>>
        explicit constexpr RandFloat(Args... args) noexcept(xyu::t_can_nothrow_listinit<RandGenI, Args...>) : gen{args...} {}

        /// 生成 [0,1)
        constexpr float operator()() noexcept(xyu::t_can_nothrow_call<RandGenI&>)
        {
            if constexpr (sizeof(I) == 4) return gen() * 0x1.fffffcp-33f;
            else return gen() * 0x1.fffffcp-65f;
        }

        /// 生成 [0,fb)
        constexpr float operator()(float b) noexcept(xyu::t_can_nothrow_call<RandGenI&>)
        {
            float tmp = b * operator()();
            return XY_UNLIKELY(tmp >= b) ? 0 : tmp;
        }

        /// 生成 [fa,fb)
        constexpr float operator()(float a, float b) noexcept(xyu::t_can_nothrow_call<RandGenI&>)
        {
            float tmp = a + (b - a) * operator()();
            return XY_UNLIKELY(tmp >= b) ? a : tmp;
        }

        /// 生成 [0,1]
        constexpr float operator()(xyu::special_t) noexcept(xyu::t_can_nothrow_call<RandGenI&>)
        {
            if constexpr (sizeof(I) == 4) return gen() * 0x1p-32f;
            else return gen() * 0x1p-64f;
        }

        /// 生成 [0,fb]
        constexpr float operator()(xyu::special_t, float b) noexcept(xyu::t_can_nothrow_call<RandGenI&>)
        {
            return b * operator()(xyu::special_v);
        }

        /// 生成 [fa,fb]
        constexpr float operator()(xyu::special_t, float a, float b) noexcept(xyu::t_can_nothrow_call<RandGenI&>)
        {
            float tmp = a + (b - a) * operator()(xyu::special_v);
            return XY_UNLIKELY(tmp > b) ? b : tmp;
        }
    };
    // 推导指引
    template <typename Gen>
    RandFloat(Gen&&) -> RandFloat<Gen>;


    /**
     * @brief 随机浮点数转换器
     * @tparam RandGenI 底层整数生成器 [0,UMAX]
     * @details 32位/64位转为 double
     */
    template <typename RandGenI>
    class RandDouble
    {
        using I = decltype(xyu::t_val<RandGenI&>()());
        static_assert(xyu::t_is_same_one<I, xyu::uint32, xyu::uint64>);
    private:
        RandGenI gen;

    public:
        /// 初始化推导
        constexpr RandDouble(RandGenI gen) noexcept(xyu::t_can_nothrow_cpconstr<RandGenI>) : gen(gen) {}

        /// 初始化生成器
        template <typename... Args, typename = xyu::t_enable<xyu::t_can_listinit<RandGenI, Args...>>>
        explicit constexpr RandDouble(Args... args) noexcept(xyu::t_can_nothrow_listinit<RandGenI, Args...>) : gen{args...} {}

        /// 生成 [0,1)
        constexpr double operator()() noexcept(xyu::t_can_nothrow_call<RandGenI&>)
        {
            if constexpr (sizeof(I) == 4) return gen() * 0x1p-32;
            else return gen() * 0x1.fffffffffffffp-65;
        }

        /// 生成 [0,fb)
        constexpr double operator()(double b) noexcept(xyu::t_can_nothrow_call<RandGenI&>)
        {
            double tmp = b * operator()();
            return XY_UNLIKELY(tmp >= b) ? 0 : tmp;
        }

        /// 生成 [fa,fb)
        constexpr double operator()(double a, double b) noexcept(xyu::t_can_nothrow_call<RandGenI&>)
        {
            double tmp = a + (b - a) * operator()();
            return XY_UNLIKELY(tmp >= b) ? a : tmp;
        }

        /// 生成 [0,1]
        constexpr double operator()(xyu::special_t) noexcept(xyu::t_can_nothrow_call<RandGenI&>)
        {
            if constexpr (sizeof(I) == 4) return gen() * 0x1.00000001p-32;
            else return gen() * 0x1p-64;
        }

        /// 生成 [0,fb]
        constexpr double operator()(xyu::special_t, double b) noexcept(xyu::t_can_nothrow_call<RandGenI&>)
        {
            return b * operator()(xyu::special_v);
        }

        /// 生成 [fa,fb]
        constexpr double operator()(xyu::special_t, double a, double b) noexcept(xyu::t_can_nothrow_call<RandGenI&>)
        {
            double tmp = a + (b - a) * operator()(xyu::special_v);
            return XY_UNLIKELY(tmp > b) ? a : tmp;
        }
    };
    // 推导指引
    template <typename Gen>
    RandDouble(Gen&&) -> RandDouble<Gen>;


    /**
     * @brief 随机对称浮点数转换器
     * @tparam RandGenI 底层整数生成器 [0,UMAX]
     * @details 32位/64位转为 float
     */
    template <typename RandGenI>
    class RandFloatSym
    {
        using I = decltype(xyu::t_val<RandGenI&>()());
        static_assert(xyu::t_is_same_one<I, xyu::uint32, xyu::uint64>);

        static constexpr I msb = I(1) << (xyu::t_number<I>::digits - 1);
        static constexpr I mask = msb - 1;
    private:
        RandGenI gen;

    public:
        /// 初始化推导
        constexpr RandFloatSym(RandGenI gen) noexcept(xyu::t_can_nothrow_cpconstr<RandGenI>) : gen(gen) {}

        /// 初始化生成器
        template <typename... Args, typename = xyu::t_enable<xyu::t_can_listinit<RandGenI, Args...>>>
        explicit constexpr RandFloatSym(Args... args) noexcept(xyu::t_can_nothrow_listinit<RandGenI, Args...>) : gen{args...} {}

        /// 生成 (-1,1)
        constexpr float operator()() noexcept(xyu::t_can_nothrow_call<RandGenI&>)
        {
            I i = gen();
            float v = i & mask;
            if constexpr (sizeof(I) == 4) v *= 0x1.fffffep-32f;
            else v *= 0x1.fffffep-64f;
            return i & msb ? -v : v;
        }

        /// 生成 (-f,f)
        constexpr float operator()(float f) noexcept(xyu::t_can_nothrow_call<RandGenI&>)
        {
            I i = gen();
            float v = i & mask;
            if constexpr (sizeof(I) == 4) v *= 0x1.fffffep-32f;
            else v *= 0x1.fffffep-64f;
            if (XY_UNLIKELY(v >= f)) return 0;
            return i & msb ? -v : v;
        }

        /// 生成 [-1,1]
        constexpr float operator()(xyu::special_t) noexcept(xyu::t_can_nothrow_call<RandGenI&>)
        {
            I i = gen();
            float v = i & mask;
            if constexpr (sizeof(I) == 4) v *= 0x1p-31f;
            else v *= 0x1p-63f;
            return i & msb ? -v : v;
        }

        /// 生成 [-f,f]
        constexpr float operator()(xyu::special_t, float f) noexcept(xyu::t_can_nothrow_call<RandGenI&>)
        {
            I i = gen();
            float v = i & mask;
            if constexpr (sizeof(I) == 4) v *= 0x1p-31f;
            else v *= 0x1p-63f;
            if (XY_UNLIKELY(v > f)) v = f;
            return i & msb ? -v : v;
        }
    };
    // 推导指引
    template <typename Gen>
    RandFloatSym(Gen&&) -> RandFloatSym<Gen>;


    /**
     * @brief 随机对称浮点数转换器
     * @tparam RandGenI 底层整数生成器 [0,UMAX]
     * @details 32位/64位转为 double
     */
    template <typename RandGenI>
    class RandDoubleSym
    {
        using I = decltype(xyu::t_val<RandGenI&>()());
        static_assert(xyu::t_is_same_one<I, xyu::uint32, xyu::uint64>);

        static constexpr I msb = I(1) << (xyu::t_number<I>::digits - 1);
        static constexpr I mask = msb - 1;
    private:
        RandGenI gen;

    public:
        /// 初始化推导
        constexpr RandDoubleSym(RandGenI gen) noexcept(xyu::t_can_nothrow_cpconstr<RandGenI>) : gen(gen) {}

        /// 初始化生成器
        template <typename... Args, typename = xyu::t_enable<xyu::t_can_listinit<RandGenI, Args...>>>
        explicit constexpr RandDoubleSym(Args... args) noexcept(xyu::t_can_nothrow_listinit<RandGenI, Args...>) : gen{args...} {}

        /// 生成 (-1,1)
        constexpr double operator()() noexcept(xyu::t_can_nothrow_call<RandGenI&>)
        {
            I i = gen();
            double v = i & mask;
            if constexpr (sizeof(I) == 4) v *= 0x1.0p-31;
            else v *= 0x1.ffffffffffffep-64;
            return i & msb ? -v : v;
        }

        /// 生成 (-f,f)
        constexpr double operator()(double f) noexcept(xyu::t_can_nothrow_call<RandGenI&>)
        {
            I i = gen();
            double v = i & mask;
            if constexpr (sizeof(I) == 4) v *= 0x1.0p-31;
            else v *= 0x1.ffffffffffffep-64;
            if (XY_UNLIKELY(v >= f)) return 0;
            return i & msb ? -v : v;
        }

        /// 生成 [-1,1]
        constexpr double operator()(xyu::special_t) noexcept(xyu::t_can_nothrow_call<RandGenI&>)
        {
            I i = gen();
            double v = i & mask;
            if constexpr (sizeof(I) == 4) v *= 1.0 / 2147483647.0;
            else v *= 1.0842021724855044E-19;
            return i & msb ? -v : v;
        }

        /// 生成 [-f,f]
        constexpr double operator()(xyu::special_t, double f) noexcept(xyu::t_can_nothrow_call<RandGenI&>)
        {
            I i = gen();
            double v = i & mask;
            if constexpr (sizeof(I) == 4) v *= 1.0 / 2147483647.0;
            else v *= 1.0842021724855044E-19;
            if (XY_UNLIKELY(v > f)) v = f;
            return i & msb ? -v : v;
        }
    };
    // 推导指引
    template <typename Gen>
    RandDoubleSym(Gen&&) -> RandDoubleSym<Gen>;

}

/// 分布转换器
namespace xylu::xymath
{
    namespace __
    {
        extern struct GaussianTable
        {
            const double x[129]; // 第 i 层的右边界横坐标
            const double f[129]; // 第 i 层边界对应的高度
        }
        gaussian_table;
    }
    /**
     * @brief 随机正态分布转换器 - Ziggurat
     * @tparam RandGenI 底层整数生成器 [0,UMAX]
     * @details 32位/64位转为 double
     */
    template <typename RandGenI>
    struct RandNorm
    {
    private:
        using I = decltype(xyu::t_val<RandGenI&>()());
        static_assert(xyu::t_is_same_one<I, xyu::uint32, xyu::uint64>);
        RandGenI gen;

    public:
        /// 初始化推导
        constexpr RandNorm(RandGenI gen) noexcept(xyu::t_can_nothrow_cpconstr<RandGenI>) : gen(gen) {}

        /// 初始化生成器
        template <typename... Args, typename = xyu::t_enable<xyu::t_can_listinit<RandGenI, Args...>>>
        explicit constexpr RandNorm(Args... args) noexcept(xyu::t_can_nothrow_listinit<RandGenI, Args...>) : gen{args...} {}

        /// 生成 N(0,1)
        double operator()() noexcept(xyu::t_can_nothrow_call<RandGenI&>)
        {
            for (;;)
            {
                constexpr I msb = I(1) << (xyu::t_number<I>::digits - 1);
                constexpr I mask = msb - 1;
                auto& table = __::gaussian_table;

                I v = gen();
                bool sign = v & msb;
                v &= mask;

                double f;   // [0,2^31-1] -> [0.0,1.0)
                if constexpr (sizeof(I) == 4) f = double(v) * 0x1.0p-31;
                else f = double(v) * 0x1.ffffffffffffep-64;

                int i = v & 127;
                double x = f * table.x[i];
                if (XY_LIKELY(x < table.x[i + 1])) return sign ? -x : x;

                if (XY_UNLIKELY(i == 0)) return fallback(sign);

                f = RandDouble<RandGenI&>{gen}();   // [0,1)
                if ((table.f[i + 1] + f * (table.f[i] - table.f[i + 1]))
                    < calc_exp(-0.5 * x * x)) return sign ? -x : x;
            }
        }

        /// 生成 N(mu, sigma)
        double operator()(double mu, double sigma = 1) noexcept(xyu::t_can_nothrow_call<RandGenI&>)
        {
            return mu + operator()() * calc_sqrt(sigma);
        }

    private:
        XY_NOINLINE XY_COLD
        double fallback(bool sign) noexcept(xyu::t_can_nothrow_call<RandGenI&>)
        {
            static constexpr double kR = 3.442619855899;          // Start of the tail
            static constexpr double kRInv = 0.29047645161474317;  // ~= (1.0 / kR)

            double x, y;
            RandDouble<RandGenI&> randf(gen);
            do { // kRInv = 1/r, U(0, 1)
                x = kRInv * calc_ln(randf());
                y = -calc_ln(randf());
            } while ((y + y) < (x * x));
            return sign ? (x - kR) : (kR - x);
        }
    };
    // 推导指引
    template <typename Gen>
    RandNorm(Gen&&) -> RandNorm<Gen>;


    /**
     * @brief 随机正态分布转换器 - Marsaglia Polar
     * @tparam RandGenF 底层浮点数生成器 (-1,1)
     * @details float生成float，double生成double
     */
    template <typename RandGenF>
    struct RandNormMP
    {
    private:
        using F = decltype(xyu::t_val<RandGenF&>()());
        static_assert(xyu::t_is_same<F, float> || xyu::t_is_same<F, double>);
        RandGenF gen;

    public:
        /// 初始化推导
        constexpr RandNormMP(RandGenF gen) noexcept(xyu::t_can_nothrow_cpconstr<RandGenF>) : gen(gen) {}

        /// 初始化生成器
        template <typename... Args, typename = xyu::t_enable<xyu::t_can_listinit<RandGenF, Args...>>>
        explicit constexpr RandNormMP(Args... args) noexcept(xyu::t_can_nothrow_listinit<RandGenF, Args...>) : gen{args...} {}

        /// 生成 2个 N(0,1)
        struct FloatPair { F n1, n2; };
        FloatPair operator()() noexcept(xyu::t_can_nothrow_call<RandGenF&>)
        {
            F x, y, s;
            do {
                x = gen(); y = gen();
                s = x * x + y * y;
            } while (s >= 1 || s == 0);
            F factor = calc_sqrt(-2 * calc_ln(s) / s);
            return { x * factor, y * factor };
        }

        /// 生成 2个 N(mu, sigma)
        FloatPair operator()(F mu, F sigma = 1) noexcept(xyu::t_can_nothrow_call<RandGenF&>)
        {
            auto pair = operator()();
            F sd = calc_sqrt(sigma);
            return {mu + pair.n1 * sd, mu + pair.n2 * sd};
        }

        /// 生成 1个 N(0,1)
        F operator()(xyu::native_t) noexcept(xyu::t_can_nothrow_call<RandGenF&>)
        {
            F x, y, s;
            do {
                x = gen(); y = gen();
                s = x * x + y * y;
            } while (s >= 1 || s == 0);
            double factor = calc_sqrt(-2 * calc_ln(s) / s);
            return x * factor, y * factor;
        }

        /// 生成 2个 N(mu, sigma)
        F operator()(xyu::native_t, F mu, F sigma = 1) noexcept(xyu::t_can_nothrow_call<RandGenF&>)
        {
            F v = operator()(xyu::native_v);
            return mu + v * calc_sqrt(sigma);
        }
    };
    // 推导指引
    template <typename Gen>
    RandNormMP(Gen&&) -> RandNormMP<Gen>;

}
