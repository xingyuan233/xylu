#pragma once

/**
 * @file    xyrange/gen.h
 * @brief   策略式生成器框架
 * @date    2025-12-24
 */

#include "./iter.h"

/// 生成器模板类
namespace xylu::xyrange
{
    /**
     * @brief 用于构建“生成器”的基础别名模板
     * @details
     *   生成器在每次自增后动态地计算其值，支持单向遍历。
     *   并且其元素一般与范围绑定，本身不存在元素个数。
     *
     * @tparam Storage     [必须] 负责存储生成器的内部状态（如当前值、结束条件等）
     * @tparam Valid       [必须] 负责判断生成器是否已到达序列的末尾（结束条件）
     * @tparam Dereference [必须] 负责根据当前状态生成并返回值
     * @tparam Increment   [必须] 负责更新生成器的状态，使其前进到下一个值
     */
    template <typename Storage, typename Valid, typename Dereference, typename Increment,
              typename Decrement = void, typename Forward = void, typename Backward = void>
    using Generator = Iterator<Storage, Valid, Dereference, void, Increment, Decrement, Forward, Backward, void, void, void>;

}

/// 生成器属性
namespace xylu::xytraits
{
    /**
     * @brief 判断是否为生成器
     * @details 至少为单向且无法比较的迭代器是生成器
     */
    template <typename... T>
    constexpr bool t_is_gen = []
    {
        if constexpr (!t_is_siter<T...>) return false;
        else return (... && xyu::t_is_void<typename xyu::t_rem_cr<T>::Equal_t>) &&
                    (... && xyu::t_is_void<typename xyu::t_rem_cr<T>::Compare_t>);
    }();
}

/// 基础生成器策略
namespace xylu::xyrange
{
    namespace GenPolicies::Normal
    {
        // 恒有效
        struct AlwaysValid
        {
            template <typename Gen>
            constexpr bool operator()(const Gen&) noexcept { return true; }
        };

        // 解引用获取自身
        struct SelfDeref
        {
            template <typename Gen>
            constexpr decltype(auto) operator()(Gen& gen) noexcept { return (gen.p); }
        };

        // 解引用获取自身
        struct CopyDeref
        {
            template <typename Gen>
            constexpr auto operator()(Gen& gen) noexcept { return gen.p; }
        };

        // 空自增
        struct Empty
        {
            template <typename... T>
            constexpr void operator()(T&&...) noexcept {}
        };
    }
}

/// 生成器的范围迭代器封装
namespace xylu::xyrange
{
    namespace GenPolicies::Range
    {
        // 范围存储
        template <typename Gen, typename Owner = void>
        struct Storage
        {
            using Owner_t = xyu::t_cond<xyu::t_is_void<Owner>, __::get_owner<Gen>, Owner>;

            Gen p;
            xyu::size_t count;

            constexpr Storage(Gen gen, xyu::size_t count) noexcept(xyu::t_can_nothrow_cpconstr<Gen>)
                : p{gen}, count {count} {}
        };

        // 有效
        struct Valid
        {
            template <typename Iter>
            constexpr bool operator()(const Iter& it) noexcept(noexcept(static_cast<bool>(it.p)))
            { return it.count && it.p; }
        };

        // 自增
        struct Increment
        {
            template <typename Iter>
            constexpr void operator()(Iter& it) noexcept(noexcept(++it.p))
            {
                ++it.p;
                if (it.count <= xyu::size_t(-1) / 2) --it.count;
            }
        };

        // 自减
        struct Decrement
        {
            template <typename Iter>
            constexpr void operator()(Iter& it) noexcept(noexcept(--it.p))
            {
                --it.p;
                if (it.count <= xyu::size_t(-1) / 2) ++it.count;
            }
        };

        // 前进
        struct Forward
        {
            template <typename Iter, typename Diff>
            constexpr void operator()(Iter& it, Diff step) noexcept(noexcept(it.p += step))
            {
                it.p += step;
                if (it.count <= xyu::size_t(-1) / 2) it.count -= step;
            }
        };

        // 后退
        struct Backward
        {
            template <typename Iter, typename Diff>
            constexpr void operator()(Iter& it, Diff step) noexcept(noexcept(it.p -= step))
            {
                it.p -= step;
                if (it.count <= xyu::size_t(-1) / 2) it.count += step;
            }
        };

        // 范围比较适配 (范围for 的 !=)
        struct Equal
        {
            template <typename Iter, typename T>
            constexpr bool operator()(const Iter& it, const T&) noexcept(noexcept(static_cast<bool>(it.p)))
            { return !it.p || !it.count; }
        };
    }

    /**
     * @brief 生成器的范围迭代器封装
     * @tparam Gen 原始生成器类型
     */
    template <typename Gen, typename Owner = void>
    using Generator_Iter = Iterator<
        GenPolicies::Range::Storage<Gen, Owner>,
        GenPolicies::Range::Valid,
        IterPolicies::Default::Dereference,
        xyu::t_cond<xyu::t_is_void<typename Gen::Address_t>, void, IterPolicies::Iter::Address>,
        GenPolicies::Range::Increment,
        xyu::t_cond<xyu::t_is_void<typename Gen::Decrement_t>, void, GenPolicies::Range::Decrement>,
        xyu::t_cond<xyu::t_is_void<typename Gen::Forward_t>, void, GenPolicies::Range::Forward>,
        xyu::t_cond<xyu::t_is_void<typename Gen::Backward_t>, void, GenPolicies::Range::Backward>,
        xyu::t_cond<xyu::t_is_void<typename Gen::Distance_t>, void, IterPolicies::Default::Distance>,
        GenPolicies::Range::Equal, void
    >;

}

/// 等差数列
namespace xylu::xyrange
{
    namespace GenPolicies::Sequence
    {
        // 存储类
        template <typename T>
        struct Storage
        {
            static_assert(xyu::t_is_number<T>);

            T p;    // 当前值
            T step; // 步长

            constexpr explicit Storage(T start, T step) noexcept : p(start), step(step) {}
        };

        // 自增
        struct Increment
        {
            template <typename Gen>
            constexpr void operator()(Gen& gen) noexcept
            {
                gen.p = xyu::calc_of_add<decltype(gen.p),XY_DEBUG>(gen.p, gen.step, xyu::ErrorPos{__FILE__, __LINE__, "Generator::Increment"});
            }
        };

        // 自减
        struct Decrement
        {
            template <typename Gen>
            constexpr void operator()(Gen& gen) const noexcept
            {
                gen.p = xyu::calc_of_sub<decltype(gen.p),XY_DEBUG>(gen.p, gen.step, xyu::ErrorPos{__FILE__, __LINE__, "Generator::Decrement"});
            }
        };

        // 前进
        struct Forward
        {
            template <typename Gen, typename Diff>
            constexpr void operator()(Gen& gen, Diff step) const noexcept
            {
                auto tmp = xyu::calc_of_mul<decltype(gen.p),XY_DEBUG>(gen.step, step, xyu::ErrorPos{__FILE__, __LINE__, "Generator::Forward"});
                gen.p = xyu::calc_of_add<decltype(gen.p),XY_DEBUG>(gen.p, tmp, xyu::ErrorPos{__FILE__, __LINE__, "Generator::Forward"});
            }
        };

        // 后退
        struct Backward
        {
            template <typename Gen, typename Diff>
            constexpr void operator()(Gen& gen, Diff step) const noexcept
            {
                auto tmp = xyu::calc_of_mul<decltype(gen.p),XY_DEBUG>(gen.step, step, xyu::ErrorPos{__FILE__, __LINE__, "Generator::Backward"});
                gen.p = xyu::calc_of_sub<decltype(gen.p),XY_DEBUG>(gen.p, tmp, xyu::ErrorPos{__FILE__, __LINE__, "Generator::Backward"});
            }
        };
    }

    /**
     * @brief 生成等差数列的生成器
     * @tparam T 数列中数值的类型，必须是数值类型
     */
    template <typename T>
    using Generator_Sequence = Generator<
        GenPolicies::Sequence::Storage<T>,
        GenPolicies::Normal::AlwaysValid, GenPolicies::Normal::CopyDeref,
        GenPolicies::Sequence::Increment, GenPolicies::Sequence::Decrement,
        GenPolicies::Sequence::Forward, GenPolicies::Sequence::Backward
    >;
}

/// 重复序列
namespace xylu::xyrange
{
    namespace GenPolicies::Repeat
    {
        // 存储类
        template <typename T>
        struct Storage
        {
            T p;    // 重复的值

            constexpr explicit Storage(const T& value) noexcept : p{value} {}
        };
    }

    /**
     * @brief 创建一个将单个值重复指定次数的生成器
     * @tparam T 要重复的值的类型
     */
    template <typename T>
    using Generator_Repeat = Generator<
        GenPolicies::Repeat::Storage<T>, GenPolicies::Normal::AlwaysValid, GenPolicies::Normal::SelfDeref,
        GenPolicies::Normal::Empty, GenPolicies::Normal::Empty, GenPolicies::Normal::Empty, GenPolicies::Normal::Empty
    >;
}
