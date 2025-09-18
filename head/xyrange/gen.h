#pragma clang diagnostic push
#pragma ide diagnostic ignored "OCUnusedTypeAliasInspection"
#pragma once

#include "./iter.h"

/**
 * @file
 * @brief 基于 RangeIter 框架，定义了“生成器”的概念，并提供了几种常用实现。
 * @details
 *   生成器是一种特殊的迭代器，它在被请求时动态地“生成”值，而不是指向已存在的容器中的元素。
 *   因此，它们通常是单向的、只读的。
 */

/** 生成器模板类 */
namespace xylu::xyrange
{
    /**
     * @brief 一个用于构建“生成器”的基础别名模板，它是一种简化的、只进的迭代器。
     * @details
     *   与指向内存中现有数据的标准迭代器不同，生成器在每次自增后动态地计算其值。
     *   因此，它通常只支持单向遍历，并且没有比较或距离计算等功能。
     *
     * @tparam Storage     [必须] 负责存储生成器的内部状态（如当前值、结束条件等）。
     * @tparam Valid       [必须] 负责判断生成器是否已到达序列的末尾（结束条件）。
     * @tparam Dereference [必须] 负责根据当前状态生成并返回值。
     * @tparam Increment   [必须] 负责更新生成器的状态，使其前进到下一个值。
     */
    template <typename Storage, typename Valid, typename Dereference, typename Increment>
    using RangeGenerator = RangeIter<Storage, Valid, RangeIter_None, Dereference,
        Increment, RangeIter_None, RangeIter_None, RangeIter_None, RangeIter_None, RangeIter_None, RangeIter_None>;
}

/// 生成器：等差数列
namespace xylu::xyrange
{
    // 生成器 - 存储类 - 数列
    template <typename T>
    struct RangeGenerator_Storage_Sequence
    {
        static_assert(xyu::t_is_number<T>);

        T i;    // 当前值
        T e;    // 结束值
        T step; // 步长

        // 构造函数
        constexpr RangeGenerator_Storage_Sequence(T start, T end, T step) noexcept : i{start}, e{end}, step{step} {}
        // 构造函数
        constexpr explicit RangeGenerator_Storage_Sequence(T end) noexcept : i{0}, e{end}, step{1} {}
    };

    // 生成器 - 有效判断 - 数列
    struct RangeGenerator_Valid_Sequence
    {
        template <typename Generator>
        constexpr bool operator()(Generator& gen) noexcept
        { return gen.step >= 0 ? gen.i < gen.e : gen.i > gen.e; }
    };

    // 生成器 - 自增 - 数列
    struct RangeGenerator_Increment_Sequence
    {
        template <typename Generator>
        constexpr void operator()(Generator& gen) noexcept
        { gen.i += gen.step; }
    };

    /**
     * @brief 创建一个生成等差数列的生成器。
     * @tparam T 数列中数值的类型，必须是数值类型。
     * @example
     *   // 生成 0, 1, 2, 3, 4
     *   RangeGenerator_Sequence˂int˃(5);
     *
     *   // 生成 1, 3, 5, 7, 9
     *   RangeGenerator_Sequence˂int˃(1, 10, 2);
     *
     *   // 生成 5, 4, 3, 2, 1
     *   RangeGenerator_Sequence˂int˃(5, 0, -1);
     */
    template <typename T>
    using RangeGenerator_Sequence = RangeGenerator<RangeGenerator_Storage_Sequence<T>,
            RangeGenerator_Valid_Sequence, RangeIter_Dereference_Self, RangeGenerator_Increment_Sequence>;
}

/// 生成器：重复值
namespace xylu::xyrange
{
    // 生成器 - 存储类 - 重复单个值
    template <typename T>
    struct RangeGenerator_Storage_Repeat
    {
        T i;                // 单个值
        xyu::size_t count;  // 次数

        // 构造函数
        constexpr explicit RangeGenerator_Storage_Repeat(const T& value, xyu::size_t count = 1) noexcept
            : i{value}, count{count} {}
    };

    // 生成器 - 有效判断 - 重复单个值
    struct RangeGenerator_Valid_Repeat
    {
        template <typename Generator>
        constexpr bool operator()(Generator& gen) noexcept
        { return gen.count > 0; }
    };

    // 生成器 - 递增 - 重复单个值
    struct RangeGenerator_Increment_Repeat
    {
        template <typename Generator>
        constexpr void operator()(Generator& gen) noexcept
        { --gen.count; }
    };

    /**
     * @brief 创建一个将单个值重复指定次数的生成器。
     * @tparam T 要重复的值的类型。
     * @example
     *   // 生成 'x', 'x', 'x'
     *   RangeGenerator_Repeat˂char˃('x', 3);
     *
     *   // 生成 100
     *   RangeGenerator_Repeat˂int˃(100);
     */
    template <typename T>
    using RangeGenerator_Repeat = RangeGenerator<RangeGenerator_Storage_Repeat<T>,
            RangeGenerator_Valid_Repeat, RangeIter_Dereference_Self, RangeGenerator_Increment_Repeat>;
}

#pragma clang diagnostic pop