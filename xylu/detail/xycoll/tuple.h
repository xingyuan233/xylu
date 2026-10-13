#pragma once

/**
 * @file    xycoll/tuple.h
 * @brief   元组
 * @date    2026-04-25
 */

#include "./tagval.h"
#include "../xymath/hash.h"
#include "../xystring/fmtimpl.h"
#include "../xytraits/arrptr.h"
#include "../xystd/structbind.h"
#include "../xyrange/cmp.h"

/// 前置声明
namespace xylu::xycoll
{
    template <typename Derived, typename Indexs> struct TupleBase;
    template <typename Refer, typename Indexs> struct TupleView;
    template <typename... Args> struct Tuple;
}
/// 类型属性
namespace xylu::xytraits
{
    namespace __
    {
        template <typename T>
        struct is_tuple : false_c {};
        template <typename D, typename I>
        struct is_tuple<xyu::TupleBase<D,I>> : true_c {};
        template <typename R, typename I>
        struct is_tuple<xyu::TupleView<R,I>> : true_c {};
        template <typename... Args>
        struct is_tuple<xyu::Tuple<Args...>> : true_c {};
    }
    // 是否为元组
    template <typename... T>
    constexpr bool t_is_tuple = (... && __::is_tuple<t_rem_cr<T>>::value);
}

/// 元组
namespace xylu::xycoll
{
    namespace __
    {
        // 获取类型
        template <typename T, xyu::size_t rel_ids>
        struct tuple_get_type_impl;
        template <typename... Args, xyu::size_t rel_ids>
        struct tuple_get_type_impl<Tuple<Args...>, rel_ids>
        { using type = xyu::t_tps_get<xyu::typelist_c<Args...>, rel_ids>; };
        template <typename R, typename I, xyu::size_t rel_idx>
        struct tuple_get_type_impl<TupleView<R,I>, rel_idx>
        { using type = typename tuple_get_type_impl<xyu::t_rem_refer<R>, xyu::t_seq_get<I,rel_idx>>::type; };
        template <typename D, typename I, xyu::size_t rel_idx>
        struct tuple_get_type_impl<TupleBase<D,I>, rel_idx>
        { using type = typename tuple_get_type_impl<D, xyu::t_seq_get<I,rel_idx>>::type; };
        template <typename T, xyu::size_t rel_idx>
        using tuple_get_type = typename tuple_get_type_impl<xyu::t_rem_cr<T>, rel_idx>::type;

        // 辅助函数 - update
        template <xyu::size_t rel_s, typename TP, typename... Args, xyu::size_t... rel_ids>
        static constexpr void tuple_update_args(TP& tb, xyu::isequence_c<rel_ids...>, Args&&... args)
        { (..., (tb.template get<rel_s + rel_ids>() = xyu::forward<Args>(args))); }
        template <xyu::size_t rel_s, typename TP, typename Arg, xyu::size_t... rel_ids, typename... Empty>
        static constexpr void tuple_update_tp(TP& tb, xyu::isequence_c<rel_ids...>, Arg&& arg, Empty...)
        { (..., (tb.template get<rel_s + rel_ids>() = xyu::forward<Arg>(arg).template get<rel_ids>())); }
        // 异常检测 - update
        template <typename TP, xyu::size_t rel_s, typename... Args, xyu::size_t... rel_ids>
        static constexpr bool tuple_update_noexcept_args(xyu::isequence_c<rel_ids...>) noexcept
        { return (... && xyu::t_can_nothrow_assign<tuple_get_type<TP, rel_s + rel_ids>&, Args>); }
        template <typename TP, xyu::size_t rel_s, typename TArg, xyu::size_t... rel_ids>
        static constexpr bool tuple_update_noexcept_tp(xyu::isequence_c<rel_ids...>) noexcept
        { return (... && xyu::t_can_nothrow_assign<tuple_get_type<TP, rel_s + rel_ids>&, tuple_get_type<TArg, rel_ids>>); }
        template <typename TP, xyu::size_t rel_s, typename... Args>
        static constexpr bool tuple_update_noexcept()
        {
            static_assert(rel_s + sizeof...(Args) <= TP::count(), "too many arguments");
            if constexpr (sizeof...(Args) == 1)
            {
                using TArg = xyu::t_tps_get<xyu::typelist_c<Args...>, 0>;
                if constexpr (!xyu::t_can_assign<tuple_get_type<TP, rel_s>&, TArg>)
                    return tuple_update_noexcept_tp<TP, rel_s, TArg>(xyu::make_isequence<xyu::min(xyu::t_rem_refer<TArg>::count(), TP::count() - rel_s)>());
                else return tuple_update_noexcept_args<TP, rel_s, Args...>(xyu::make_isequence<sizeof...(Args)>());
            }
            else return tuple_update_noexcept_args<TP, rel_s, Args...>(xyu::make_isequence<sizeof...(Args)>());
        };

        // 异常检测 - equals
        template <typename TP1, typename TP2, xyu::size_t... rel_ids>
        static  constexpr bool tuple_equals_noexcept_impl(xyu::isequence_c<rel_ids...>) noexcept
        {
            return noexcept((... && xyu::equals(xyu::t_val<const TP1&>().template get<rel_ids>(),
                                                xyu::t_val<const TP2&>().template get<rel_ids>())));
        }
        template <typename TP1, typename TP2>
        static constexpr bool tuple_equals_noexcept() noexcept
        {
            if constexpr (TP1::count() != TP2::count()) return true;
            else return tuple_equals_noexcept_impl<TP1, TP2>(xyu::make_isequence<TP1::count()>());
        }
        // 异常检测 - compare
        template <typename TP1, typename TP2, xyu::size_t... rel_ids>
        static constexpr bool tuple_compare_noexcept_impl(xyu::isequence_c<rel_ids...>) noexcept
        {
            return noexcept((..., xyu::compare(xyu::t_val<const TP1&>().template get<rel_ids>(),
                                               xyu::t_val<const TP2&>().template get<rel_ids>())));
        }
        template <typename TP1, typename TP2>
        static constexpr bool tuple_compare_noexcept() noexcept
        {
            if constexpr (TP1::count() >= TP2::count())
                return tuple_compare_noexcept_impl<TP1, TP2>(xyu::make_isequence<TP2::count()>());
            else return tuple_compare_noexcept_impl<TP1, TP2>(xyu::make_isequence<TP1::count()>());
        }

        // 异常检测 - foreach
        template <typename TP, typename Func, xyu::size_t... rel_ids>
        static constexpr bool tuple_foreach_noexcept_impl(xyu::isequence_c<rel_ids...>) noexcept
        {
            return (... && (noexcept(xyu::t_val<Func>()(xyu::t_val<TP>().template get<rel_ids>())) &&
                xyu::t_is_void<xyu::t_call<Func, decltype(xyu::t_val<TP>().template get<rel_ids>())>> ?
                true : xyu::t_can_nothrow_init<xyu::t_decay<xyu::t_call<Func, decltype(xyu::t_val<TP>().template get<rel_ids>())>>,
                xyu::t_call<Func, decltype(xyu::t_val<TP>().template get<rel_ids>())>>));
        }
        template <typename TP, typename Func>
        static constexpr bool tuple_foreach_noexcept() noexcept
        { return tuple_foreach_noexcept_impl<TP, Func>(xyu::make_isequence<xyu::t_rem_refer<TP>::count()>()); }

        // 异常检测 - subtp
        template <typename TP, xyu::size_t... rel_ids>
        static constexpr bool tuple_subtp_noexcept_impl(xyu::isequence_c<rel_ids...>) noexcept
        {
            if constexpr ((... || (rel_ids >= TP::count()))) return true;
            else return noexcept(Tuple<tuple_get_type<TP, rel_ids>...>{xyu::t_val<const TP&>().template get<rel_ids>()...});
        }
        template <typename TP, xyu::size_t rel_s, xyu::size_t len>
        static constexpr bool tuple_subtp_noexcept() noexcept
        {
            constexpr xyu::size_t count = TP::count();
            constexpr xyu::size_t s = xyu::min(rel_s, count);
            constexpr xyu::size_t e = rel_s + len < rel_s ? count : xyu::min(rel_s + len, count);
            return tuple_subtp_noexcept_impl<TP>(xyu::make_isequence<s, e>());
        }
        template <typename TP, xyu::size_t... rel_ids>
        static constexpr bool tuple_subtp_noexcept(xyu::isequence_c<rel_ids...> seq) noexcept
        {
            constexpr xyu::size_t count = TP::count();
            if constexpr ((... || (rel_ids >= count))) return true;
            else return tuple_subtp_noexcept_impl<TP>(seq);
        }

        // 异常检测 - append
        template <typename TP, typename... Args, xyu::size_t... rel_ids>
        static constexpr bool tuple_append_noexcept_impl(xyu::isequence_c<rel_ids...>) noexcept
        {
            return noexcept(Tuple<tuple_get_type<TP, rel_ids>..., xyu::t_decay<Args>...>
                    {xyu::t_val<TP>().template get<rel_ids>()..., xyu::t_val<Args>()...});
        }
        template <typename TP, typename... Args>
        static constexpr bool tuple_append_noexcept() noexcept
        { return tuple_append_noexcept_impl<TP, Args...>(xyu::make_isequence<xyu::t_rem_refer<TP>::count()>()); }

        // 异常检测 - insert
        template <typename TP, typename... Args, xyu::size_t... rel_ls, xyu::size_t... rel_rs>
        static constexpr bool tuple_insert_noexcept_impl(xyu::isequence_c<rel_ls...>, xyu::isequence_c<rel_rs...>) noexcept
        {
            return noexcept(Tuple<tuple_get_type<TP, rel_ls>..., xyu::t_decay<Args>..., tuple_get_type<TP, rel_rs>...>
                    {xyu::t_val<TP>().template get<rel_ls>()..., xyu::t_val<Args>()..., xyu::t_val<TP>().template get<rel_rs>()...});
        }
        template <typename TP, xyu::size_t rel_idx, typename... Args>
        static constexpr bool tuple_insert_noexcept() noexcept
        {
            constexpr xyu::size_t count = xyu::t_rem_refer<TP>::count();
            if constexpr (rel_idx >= count) return tuple_append_noexcept<TP, Args...>();
            else return tuple_insert_noexcept_impl<TP, Args...>
                        (xyu::make_isequence<rel_idx>(), xyu::make_isequence<rel_idx, count>());
        }

        // 异常检测 - concat
        template <typename TP1, typename TP2, xyu::size_t... rel_ids1, xyu::size_t... rel_ids2>
        static constexpr bool tuple_concat_noexcept_impl(xyu::isequence_c<rel_ids1...>, xyu::isequence_c<rel_ids2...>) noexcept
        {
            return noexcept(Tuple<tuple_get_type<TP1, rel_ids1>..., tuple_get_type<TP2, rel_ids2>...>
                    {xyu::t_val<TP1>().template get<rel_ids1>()..., xyu::t_val<TP2>().template get<rel_ids2>()...});
        }
        template <typename TP1, typename TP2>
        static constexpr bool tuple_concat_noexcept() noexcept
        {
            constexpr xyu::size_t count1 = xyu::t_rem_refer<TP1>::count();
            constexpr xyu::size_t count2 = xyu::t_rem_refer<TP2>::count();
            return tuple_concat_noexcept_impl<TP1, TP2>(xyu::make_isequence<count1>(), xyu::make_isequence<count2>());
        }

        // 异常检测 - erase
        template <typename TP, xyu::size_t rel_idx, xyu::size_t len>
        static constexpr bool tuple_erase_noexcept() noexcept
        {
            constexpr xyu::size_t count = xyu::t_rem_refer<TP>::count();
            constexpr xyu::size_t s = xyu::min(rel_idx, count);
            constexpr xyu::size_t e = rel_idx + len < rel_idx ? count : xyu::min(rel_idx + len, count);
            return tuple_insert_noexcept_impl<TP>(xyu::make_isequence<s>(), xyu::make_isequence<e, count>());
        }
        template <typename TP, xyu::size_t... rel_ids>
        static constexpr bool tuple_erase_noexcept(xyu::isequence_c<rel_ids...>) noexcept
        {
            constexpr xyu::size_t count = xyu::t_rem_refer<TP>::count();
            if constexpr ((... || (rel_ids >= count))) return true;
            else return tuple_append_noexcept_impl<TP>(xyu::t_seq_ers<xyu::make_isequence_t<count>, rel_ids...>{});
        }

        // 异常检测 - remove
        template <typename TP, typename Target, xyu::size_t limit, xyu::size_t rel_s, xyu::size_t rel_e, xyu::size_t... rel_ids>
        static constexpr bool tuple_remove_noexcept_impl(xyu::isequence_c<rel_ids...>) noexcept
        {
            constexpr xyu::size_t count = xyu::t_rem_refer<TP>::count();
            constexpr auto lis = xytraits::__::tps_rem_impl<decltype(xyu::t_val<TP>().subtp())>::template lis<Target, limit, rel_s, xyu::min(rel_e, count)>;
            return tuple_append_noexcept_impl<TP>(xyu::isequence_c<lis.data[rel_ids]...>{});
        }
        template <typename TP, typename Target, xyu::size_t limit, xyu::size_t rel_s, xyu::size_t rel_e>
        static constexpr bool tuple_remove_noexcept() noexcept
        {
            constexpr xyu::size_t count = xyu::t_rem_refer<TP>::count();
            if constexpr (limit == 0 || rel_s >= count || rel_s >= rel_e) return noexcept(xyu::t_val<TP>().subtp());
            else {
                constexpr auto lis = xytraits::__::tps_rem_impl<decltype(xyu::t_val<TP>().subtp())>::template lis<Target, limit, rel_s, xyu::min(rel_e, count)>;
                return tuple_remove_noexcept_impl<TP, Target, limit, rel_s, rel_e>(xyu::make_isequence<lis.count>());
            }
        }
    }

    /**
     * @brief 元组 CRTP 基类
     * @tparam Derived 派生类类型
     * @tparam abs_ids 索引序列（`xyu::isequence_c<...>`），描述元素在派生类中的绝对索引映射
     *
     * @details
     *   存储方式：
     *   - 不持有任何数据，所有元素由派生类管理
     *   - 编译期维护一个绝对索引数组 `abs`，将用户操作的逻辑索引映射到实际的绝对索引上
     *
     *   方法简述：
     *   - 元素访问：`get<index>()` 按索引访问，`select<T>()` 按类型访问（支持区间搜索）
     *   - 批量修改：`update<start>(args...)` 从起始索引开始赋值，支持解包元组
     *   - 比较：`equals`、`compare`，支持不同元组类型间比较
     *   - 函数式：`apply(func)` 展开为参数调用，`foreach<ret>(func)` 遍历并可收集返回值构造新元组
     *   - 切片：`subview` 生成引用视图（`TupleView`），`subtp` 生成值副本（新 `Tuple`）
     *   - 增：`append` 末尾追加，`insert<index>` 中间插入，`concat` 拼接另一元组
     *   - 删：`erase<start, len>` 范围删除，`erase(seq)` 按索引序列删除，`remove<T>` 按类型移除
     *
     *   继承要求（派生类必须提供以下方法）：
     *   - `template<size_t abs_idx> decltype(auto) do_get()`：获取实际元素
     *   - `static constexpr count()`：获取元素个数
     */
    template <typename Derived, xyu::size_t... abs_ids>
    struct TupleBase<Derived, xyu::isequence_c<abs_ids...>>
    {
    protected:
        // 索引映射
        static constexpr xyu::size_t abs[sizeof...(abs_ids)] = {abs_ids...};

    public:
        /* 读取 */

        /// 获取元素数量
        constexpr static xyu::size_t count() noexcept { return sizeof...(abs_ids); }

        /// 获取元素类型 (xyu::typelist_c<...>)
        constexpr static auto type() noexcept { return get_type_impl(xyu::make_isequence<count()>()); }

        /// 获取元素 - by索引 (左值引用)
        template <xyu::size_t index>
        constexpr decltype(auto) get() & noexcept
        {
            static_assert(index < count(), "index out of range");
            return derived().template do_get<abs[index]>();
        }
        /// 获取元素 - by索引 (右值引用)
        template <xyu::size_t index>
        constexpr decltype(auto) get() && noexcept { return xyu::move((*this).template get<index>()); }
        /// 获取元素 - by索引 (const引用)
        template <xyu::size_t index>
        constexpr decltype(auto) get() const& noexcept { return xyu::make_const(const_cast<TupleBase&>(*this).get<index>()); }

        /// 获取元素 - by类型 (左值引用)
        template <typename Target, xyu::size_t start = 0, xyu::size_t end = count()>
        constexpr decltype(auto) select() & noexcept { return get<xyu::t_tps_find<decltype(type()), Target, start, end>>(); }
        /// 获取元素 - by类型 (右值引用)
        template <typename Target, xyu::size_t start = 0, xyu::size_t end = count()>
        constexpr decltype(auto) select() && noexcept { return xyu::move(*this).template select<Target, start, end>(); }
        /// 获取元素 - by类型 (const引用)
        template <typename Target, xyu::size_t start = 0, xyu::size_t end = count()>
        constexpr decltype(auto) select() const& noexcept { return xyu::make_const(const_cast<TupleBase&>(*this).select<Target, start, end>()); }

        /* 修改 */

        /**
         * @brief 修改指定索引范围的值
         * @tparam start 起始索引 (默认 0)
         * @details 当仅提供一个参数且无法直接赋值时，尝试解包后赋值
         */
        template <xyu::size_t start = 0, typename... Args>
        constexpr Derived& update(Args&&... args) noexcept(__::tuple_update_noexcept<Derived, start, Args...>())
        {
            // 单参数
            if constexpr (sizeof...(Args) == 1)
            {
                using Arg = xyu::t_tps_get<xyu::typelist_c<Args...>, 0>;
                // 如果不能通过参数直接赋值，则尝试解包后赋值
                if constexpr (!xyu::t_can_assign<decltype(get<start>()), Arg>)
                    __::tuple_update_tp<start>(derived(), xyu::make_isequence<xyu::min(xyu::t_rem_cr<Arg>::count(), count() - start)>(), xyu::forward<Args>(args)...);
                else __::tuple_update_args<start>(derived(), xyu::make_isequence<sizeof...(Args)>(), xyu::forward<Args>(args)...);
            }
            // 直接赋值
            else __::tuple_update_args<start>(derived(), xyu::make_isequence<sizeof...(Args)>(), xyu::forward<Args>(args)...);
            return derived();
        }

        /* 比较 */

        /// 相等
        template <typename Derived2, xyu::size_t... abs_ids2>
        constexpr bool equals(const TupleBase<Derived2, xyu::isequence_c<abs_ids2...>>& other) const noexcept(__::tuple_equals_noexcept<Derived, Derived2>())
        {
            if constexpr (count() != sizeof...(abs_ids)) return false;
            else return (... && xyu::equals(do_get<abs_ids>(), other.template do_get<abs_ids2>()));
        }

        /// 比较
        template <typename Derived2, xyu::size_t... abs_ids2>
        constexpr int compare(const TupleBase<Derived2, xyu::isequence_c<abs_ids2...>>& other) const noexcept(__::tuple_compare_noexcept<Derived, Derived2>())
        { return do_compare(*this, other, xyu::min(count(), sizeof...(abs_ids2))); }

        /* 调用 */

        /// 将元组中的所有元素作为参数调用 Func
        template <typename Func>
        constexpr decltype(auto) apply(Func&& func) & noexcept(xyu::t_can_nothrow_call<Func, decltype(do_get<abs_ids>())...>)
        { return xyu::forward<Func>(func)(do_get<abs_ids>()...); }
        /// 将元组中的所有元素作为参数调用 Func (移动)
        template <typename Func>
        constexpr decltype(auto) apply(Func&& func) && noexcept(xyu::t_can_nothrow_call<Func, decltype(do_get<abs_ids>())...>)
        { return xyu::forward<Func>(func)(do_get<abs_ids>()...); }
        /// 将元组中的所有元素作为参数调用 Func (const)
        template <typename Func>
        constexpr decltype(auto) apply(Func&& func) const& noexcept(xyu::t_can_nothrow_call<Func, decltype(do_get<abs_ids>())...>)
        { return xyu::forward<Func>(func)(do_get<abs_ids>()...); }

        /**
         * @brief 遍历元组中的每个元素并调用 Func
         * @details 将每次调用中的返回值构造一个新元组返回值
         * @note 混合有无返回值时会先顺序执行无返回值调用，再顺序执行有返回值调用收集结果
         */
        template <typename Func>
        constexpr decltype(auto) foreach(Func&& func) & noexcept(__::tuple_foreach_noexcept<Derived&, Func>())
        { return do_foreach(*this, xyu::forward<Func>(func)); }
        /**
         * @brief 遍历元组中的每个元素并调用 Func (移动)
         * @details 将每次调用中的返回值构造一个新元组返回值
         * @note 混合有无返回值时会先顺序执行无返回值调用，再顺序执行有返回值调用收集结果
         */
        template <typename Func>
        constexpr decltype(auto) foreach(Func&& func) && noexcept(__::tuple_foreach_noexcept<Derived&&, Func>())
        { return do_foreach(xyu::move(*this), xyu::forward<Func>(func)); }
        /**
         * @brief 遍历元组中的每个元素并调用 Func (const)
         * @details 将每次调用中的返回值构造一个新元组返回值
         * @note 混合有无返回值时会先顺序执行无返回值调用，再顺序执行有返回值调用收集结果
         */
        template <typename Func>
        constexpr decltype(auto) foreach(Func&& func) const& noexcept(__::tuple_foreach_noexcept<const Derived&, Func>())
        { return do_foreach(*this, xyu::forward<Func>(func)); }

        /* 切片 */

        /**
         * @brief 获取元组的引用视图
         * @details 从索引为 start 开始，长度为 len (默认剩下所有)
         */
        template <xyu::size_t start = 0, xyu::size_t len = count()>
        constexpr auto subview() noexcept { return do_subview<start, len>(*this); }
        /**
         * @brief 获取元组的常量视图
         * @details 从索引为 start 开始，长度为 len (默认剩下所有)
         */
        template <xyu::size_t start = 0, xyu::size_t len = count()>
        constexpr auto subview() const noexcept { return do_subview<start, len>(*this); }

        /**
         * @brief 获取元组的引用视图
         * @param seq 引用的索引序列
         */
        template <xyu::size_t... indexs>
        constexpr auto subview(xyu::isequence_c<indexs...> seq) noexcept { return do_subview(*this, seq); }
        /**
         * @brief 获取元组的引用视图
         * @param seq 引用的索引序列
         */
        template <xyu::size_t... indexs>
        constexpr auto subview(xyu::isequence_c<indexs...> seq) const noexcept { return do_subview(*this, seq); }

        /**
         * @brief 获取元组的切片
         * @details 从索引为 start 开始，长度为 len (默认剩下所有)
         */
        template <xyu::size_t start = 0, xyu::size_t len = count()>
        constexpr auto subtp() const noexcept(__::tuple_subtp_noexcept<Derived, start, len>())
        { return do_subtp<start, len>(*this); }
        /**
         * @brief 获取元组的切片
         * @param seq 切片的索引序列
         */
        template <xyu::size_t... indexs>
        constexpr auto subtp(xyu::isequence_c<indexs...> seq) const noexcept(__::tuple_subtp_noexcept<Derived>(xyu::isequence_c<indexs...>{}))
        { return do_subtp(*this, seq); }

        /* 增 */

        /// 末尾添加元素
        template <typename... Args>
        constexpr auto append(Args&&... args) const& noexcept(__::tuple_append_noexcept<const Derived&, Args...>())
        { return do_append(*this, xyu::forward<Args>(args)...); }
        /// 末尾添加元素 (移动元素)
        template <typename... Args>
        constexpr auto append(Args&&... args) && noexcept(__::tuple_append_noexcept<Derived&&, Args...>())
        { return do_append(xyu::move(*this), xyu::forward<Args>(args)...); }

        /// 中间插入元素
        template <xyu::size_t index, typename... Args>
        constexpr auto insert(Args&&... args) const& noexcept(__::tuple_insert_noexcept<const Derived&, index, Args...>())
        { return do_insert<index>(*this, xyu::forward<Args>(args)...); }
        /// 中间插入元素 (移动元素)
        template <xyu::size_t index, typename... Args>
        constexpr auto insert(Args&&... args) && noexcept(__::tuple_insert_noexcept<Derived&&, index, Args...>())
        { return do_insert<index>(xyu::move(*this), xyu::forward<Args>(args)...); }

        /// 拼接元组
        template <typename TP, typename = xyu::t_enable<xyu::t_is_tuple<TP>>>
        constexpr auto concat(TP&& tp) const& noexcept(__::tuple_concat_noexcept<const Derived&, TP>())
        { return do_concat(*this, xyu::forward<TP>(tp)); }
        /// 拼接元组 (移动元素)
        template <typename TP, typename = xyu::t_enable<xyu::t_is_tuple<TP>>>
        constexpr auto concat(TP&& tp) && noexcept(__::tuple_concat_noexcept<Derived&&, TP>())
        { return do_concat(xyu::move(*this), xyu::forward<TP>(tp)); }

        /* 删 */

        /**
         * @brief 删除指定索引范围的元素
         * @details 从索引为 start 开始，长度为 len (默认剩下所有)
         */
        template <xyu::size_t start, xyu::size_t len = count()>
        constexpr auto erase() const& noexcept(__::tuple_erase_noexcept<const Derived&, start, len>())
        { return do_erase<start, len>(*this); }
        /**
         * @brief 删除指定索引范围的元素 (移动元素)
         * @details 从索引为 start 开始，长度为 len (默认剩下所有)
         */
        template <xyu::size_t start, xyu::size_t len = count()>
        constexpr auto erase() && noexcept(__::tuple_erase_noexcept<Derived&&, start, len>())
        { return do_erase<start, len>(xyu::move(*this)); }

        /**
         * @brief 删除指定索引的元素
         * @param seq 要删除的索引序列
         */
        template <xyu::size_t... indexs>
        constexpr auto erase(xyu::isequence_c<indexs...> seq) const& noexcept(__::tuple_erase_noexcept<const Derived&>(xyu::isequence_c<indexs...>{}))
        { return do_erase(*this, seq); }
        /**
         * @brief 删除指定索引的元素 (移动元素)
         * @param seq 要删除的索引序列
         */
        template <xyu::size_t... indexs>
        constexpr auto erase(xyu::isequence_c<indexs...> seq) && noexcept(__::tuple_erase_noexcept<Derived&&>(xyu::isequence_c<indexs...>{}))
        { return do_erase(xyu::move(*this), seq); }

        /**
         * @brief 移除指定类型
         * @tparam Target 要移除的类型 (忽略cv引用)
         * @tparam limit 限制移除的次数 (默认移除所有)
         * @tparam start 开始移除的位置 (默认为0)
         * @tparam end 结束移除的位置 (默认到末尾)
         */
        template <typename Target, xyu::size_t limit = count(), xyu::size_t start = 0, xyu::size_t end = count()>
        constexpr auto remove() const& noexcept(__::tuple_remove_noexcept<const Derived&, Target, limit, start, end>())
        { return do_remove<Target, limit, start, end>(*this); }
        /**
         * @brief 移除指定类型 (移动元素)
         * @tparam Target 要移除的类型 (忽略cv引用)
         * @tparam limit 限制移除的次数 (默认移除所有)
         * @tparam start 开始移除的位置 (默认为0)
         * @tparam end 结束移除的位置 (默认到末尾)
         */
        template <typename Target, xyu::size_t limit = count(), xyu::size_t start = 0, xyu::size_t end = count()>
        constexpr auto remove() && noexcept(__::tuple_remove_noexcept<Derived&&, Target, limit, start, end>())
        { return do_remove<Target, limit, start, end>(xyu::move(*this)); }

    private:
        // 辅助函数 - type
        template <xyu::size_t... rel_ids>
        static constexpr auto get_type_impl(xyu::isequence_c<rel_ids...>) noexcept
        { return xyu::typelist_c<__::tuple_get_type<Derived, rel_ids>...>{}; }

        // 辅助函数 - get
        template <xyu::size_t abs_idx>
        constexpr decltype(auto) do_get() & noexcept { return derived().template do_get<abs_idx>(); }
        template <xyu::size_t abs_idx>
        constexpr decltype(auto) do_get() && noexcept { return xyu::move(derived().template do_get<abs_idx>()); }
        template <xyu::size_t abs_idx>
        constexpr decltype(auto) do_get() const& noexcept { return xyu::make_const(const_cast<TupleBase&>(*this).do_get<abs_idx>()); }

        // 辅助函数 - compare
        template <typename TP1, typename TP2, xyu::size_t... rel_ids>
        static constexpr int do_compare(const TP1& tp1, const TP2& tp2, xyu::isequence_c<rel_ids...>)
        {
            int r = 0;
            (..., (r = r ? r : xyu::compare(tp1.template get<rel_ids>(), tp2.template get<rel_ids>())));
            return r ? r : xyu::compare(tp1.count(), tp2.count());
        }

        // 辅助函数 - foreach
        template <typename TP, typename Func>
        static constexpr auto do_foreach_check()
        {
            struct { xylu::xytraits::__::cList<xyu::size_t, count()> varr{}, arr{}; } ret {};
            ((xyu::t_is_void<decltype(xyu::t_val<Func>()(xyu::t_val<TP>().template do_get<abs_ids>()))>
                ? ret.varr.data[ret.varr.count++] = abs_ids : ret.arr.data[ret.arr.count++] = abs_ids), ...);
            return ret;
        }
        template <typename TP, typename Func, xyu::size_t... vids, xyu::size_t... ids>
        static constexpr auto do_foreach_impl(TP&& tp, Func&& func, xyu::isequence_c<vids...>, xyu::isequence_c<ids...>)
        {
            constexpr auto ret = do_foreach_check<TP, Func>();
            (..., xyu::forward<Func>(func)(xyu::forward<TP>(tp).template do_get<ret.varr.data[vids]>()));
            return Tuple<xyu::t_decay<decltype(xyu::forward<Func>(func)(xyu::forward<TP>(tp).template do_get<ret.arr.data[ids]>()))>...>
                    { xyu::forward<Func>(func)(xyu::forward<TP>(tp).template do_get<ret.arr.data[ids]>())... };
        }
        template <typename TP, typename Func>
        static constexpr decltype(auto) do_foreach(TP&& tp, Func&& func)
        {
            constexpr auto ret = do_foreach_check<TP, Func>();
            if constexpr (ret.varr.count == count())
                (..., xyu::forward<Func>(func)(xyu::forward<TP>(tp).template do_get<abs_ids>()));
            else if constexpr (ret.arr.count == count())
                return Tuple<xyu::t_decay<decltype(xyu::forward<Func>(func)(xyu::forward<TP>(tp).template do_get<abs_ids>()))>...>
                        { xyu::forward<Func>(func)(xyu::forward<TP>(tp).template do_get<abs_ids>())... };
            else return do_foreach_impl(xyu::forward<TP>(tp), xyu::forward<Func>(func),
                xyu::make_isequence<ret.varr.count>(), xyu::make_isequence<ret.arr.count>());
        }

        // 辅助函数 - subview
        template <typename TP, xyu::size_t... rel_ids>
        static constexpr auto do_subview_impl(TP& tp, xyu::isequence_c<rel_ids...>) noexcept
        { return TupleView<TP, xyu::isequence_c<abs[rel_ids]...>>{tp}; }
        template <xyu::size_t rel_s, xyu::size_t len, typename TP>
        static constexpr auto do_subview(TP& tp) noexcept
        {
            constexpr xyu::size_t s = xyu::min(rel_s, count());
            constexpr xyu::size_t e = rel_s + len < rel_s ? count() : xyu::min(rel_s + len, count());
            return do_subview_impl(tp.derived(), xyu::make_isequence<s, e>());
        }
        template <typename TP, xyu::size_t... rel_ids>
        static constexpr auto do_subview(TP& tp, xyu::isequence_c<rel_ids...> seq) noexcept
        {
            static_assert((... && (rel_ids < count())), "index out of range");
            return do_subview_impl(tp.derived(), seq);
        }

        // 辅助函数 - subtp
        template <typename TP, xyu::size_t... rel_ids>
        static constexpr auto do_subtp_impl(const TP& tp, xyu::isequence_c<rel_ids...>)
        { return Tuple<__::tuple_get_type<TP, rel_ids>...>{tp.template get<rel_ids>()...}; }
        template <xyu::size_t rel_s, xyu::size_t len, typename TP>
        static constexpr auto do_subtp(const TP& tp)
        {
            constexpr xyu::size_t s = xyu::min(rel_s, count());
            constexpr xyu::size_t e = rel_s + len < rel_s ? count() : xyu::min(rel_s + len, count());
            return do_subtp_impl(tp, xyu::make_isequence<s, e>());
        }
        template <typename TP, xyu::size_t... rel_ids>
        static constexpr auto do_subtp(const TP& tp, xyu::isequence_c<rel_ids...> seq)
        {
            static_assert((... && (rel_ids < count())), "index out of range");
            return do_subtp_impl(tp, seq);
        }

        // 辅助函数 - append
        template <typename TP, typename... Args>
        static constexpr auto do_append(TP&& tp, Args&&... args)
        {
            return Tuple<xyu::t_rem_cr<decltype(tp.template do_get<abs_ids>())>..., xyu::t_decay<Args>...>
                    {xyu::forward<TP>(tp).template do_get<abs_ids>()..., xyu::forward<Args>(args)...};
        }

        // 辅助函数 - insert
        template <typename TP, typename... Args, xyu::size_t... rel_ls, xyu::size_t... rel_rs>
        static constexpr auto do_insert_impl(xyu::isequence_c<rel_ls...>, xyu::isequence_c<rel_rs...>, TP&& tp, Args&&... args)
        {
            return Tuple<__::tuple_get_type<TP, rel_ls>..., xyu::t_rem_cr<Args>..., __::tuple_get_type<TP, rel_rs>...>
             {xyu::forward<TP>(tp).template get<rel_ls>()..., xyu::forward<Args>(args)..., xyu::forward<TP>(tp).template get<rel_rs>()...};
        }
        template <xyu::size_t rel_idx, typename TP, typename... Args>
        static constexpr auto do_insert(TP&& tp, Args&&... args)
        {
            if constexpr (rel_idx >= count()) return do_append(xyu::forward<TP>(tp), xyu::forward<Args>(args)...);
            else return do_insert_impl(xyu::make_isequence<rel_idx>(), xyu::make_isequence<rel_idx, count()>(),
                                       xyu::forward<TP>(tp), xyu::forward<Args>(args)...);
        }

        // 辅助函数 - concat
        template <typename TP1, typename TP2, xyu::size_t... rel_ids2>
        static constexpr auto do_concat_impl(TP1&& tp1, TP2&& tp2, xyu::isequence_c<rel_ids2...>)
        {
            return do_insert_impl<TP1>(xyu::make_isequence<count()>(), xyu::isequence_c<>{},
                                       xyu::forward<TP1>(tp1), xyu::forward<TP2>(tp2).template get<rel_ids2>()...);
        }
        template <typename TP1, typename TP2>
        static constexpr auto do_concat(TP1&& tp1, TP2&& tp2)
        { return do_concat_impl(xyu::forward<TP1>(tp1), xyu::forward<TP2>(tp2), xyu::make_isequence<xyu::t_rem_cr<TP2>::count()>()); }

        // 辅助函数 - erase
        template <xyu::size_t rel_s, xyu::size_t len, typename TP>
        static constexpr auto do_erase(TP&& tp)
        {
            constexpr xyu::size_t s = xyu::min(rel_s, count());
            constexpr xyu::size_t e = rel_s + len < rel_s ? count() : xyu::min(rel_s + len, count());
            return do_insert_impl(xyu::make_isequence<s>(), xyu::make_isequence<e, count()>(), xyu::forward<TP>(tp));
        }
        template <xyu::size_t... rel_ids, typename TP>
        static constexpr auto do_erase(TP&& tp, xyu::isequence_c<rel_ids...>)
        {
            static_assert((... && (rel_ids < count())), "index out of range");
            return do_insert_impl(xyu::t_seq_ers<xyu::make_isequence_t<count()>, rel_ids...>{}, xyu::isequence_c<>{}, xyu::forward<TP>(tp));
        }

        // 辅助函数 - remove
        template <typename Target, xyu::size_t limit, xyu::size_t rel_s, xyu::size_t rel_e, typename TP, xyu::size_t... rel_ids>
        static constexpr auto do_remove_impl(TP&& tp, xyu::isequence_c<rel_ids...>)
        {
            constexpr auto lis = xylu::xytraits::__::tps_rem_impl<decltype(tp.subtp())>::template lis<Target, limit, rel_s, xyu::min(rel_e, count())>;
            return do_insert_impl(xyu::isequence_c<lis.data[rel_ids]...>{}, xyu::isequence_c<>{}, xyu::forward<TP>(tp));
        }
        template <typename Target, xyu::size_t limit, xyu::size_t rel_s, xyu::size_t rel_e, typename TP>
        static constexpr auto do_remove(TP&& tp)
        {
            if constexpr (limit == 0 || rel_s >= count() || rel_s >= rel_e) return tp.subtp();
            else {
                constexpr auto lis = xylu::xytraits::__::tps_rem_impl<decltype(tp.subtp())>::template lis<Target, limit, rel_s, xyu::min(rel_e, count())>;
                return do_remove_impl<Target, limit, rel_s, rel_e>(xyu::forward<TP>(tp), xyu::make_isequence<lis.count>());
            }
        }

    private:
        // 获取派生类
        constexpr Derived& derived() noexcept { return static_cast<Derived&>(*this); }
    };

    /**
     * @brief 元组视图
     * @tparam Refer 被引用元组的类型（`Tuple<...>&` 或 `const Tuple<...>&`）
     * @tparam indexs 索引序列（`xyu::isequence_c<...>`），指定要引用的原元组中的绝对索引
     *
     * @details
     *   存储方式：
     *   - 仅持有一个指向原元组的引用 `Refer& tp`
     *   - 不拥有数据，所有访问通过索引映射转发至原元组
     *
     *   方法简述：
     *   - 继承自 `TupleBase`，@see TupleBase，支持所有元组操作
     *   - 修改操作会直接作用于原元组（若 `Refer` 为 non-const 引用）
     *   - 支持视图的再切片和重组，形成链式引用
     *   - 支持结构化绑定，特化 `std::tuple_size` 和 `std::tuple_element`
     *
     *   注意事项：
     *   - 生命周期：视图不延长原元组生存期，必须确保原元组在视图使用期间存活
     */
    template <typename Refer, xyu::size_t... indexs>
    struct TupleView<Refer, xyu::isequence_c<indexs...>> : TupleBase<TupleView<Refer, xyu::isequence_c<indexs...>>, xyu::isequence_c<indexs...>>
    {
    private:
        Refer& tp;      // 元组引用

    public:
        // 元组引用构造
        constexpr TupleView(Refer& tp) noexcept : tp(tp) {}
        // 元组视图构造
        template <typename Refer2, typename Indexs2, typename = xyu::t_enable<xyu::t_is_same_noconst<Refer, Refer2>>>
        constexpr TupleView(const TupleView<Refer2, Indexs2>& tpl) noexcept : tp(tpl.tp) {}

    private:
        friend class TupleBase<TupleView, xyu::isequence_c<indexs...>>;
        // 获取元素
        template <xyu::size_t abs_idx>
        constexpr decltype(auto) do_get() noexcept { return tp.template do_get<abs_idx>(); }
    };

    /**
     * @brief 元组
     * @tparam Args 元素类型列表，支持异构
     *
     * @details
     *   存储方式：
     *   - 通过私有继承 `TagVal<Arg, Index>...` 存储每个元素，利用空基类优化消除空类型开销
     *   - 内部 `Tuple<void, seq, Args...>` 偏特化实现实际存储和 `do_get` 访问，`Tuple<Args...>` 作为对外别名
     *
     *   方法简述：
     *   - 支持 默认构造（值初始化）、全参数构造、部分参数构造（剩余默认构造）、从其他元组解包构造
     *   - 继承自 `TupleBase`，@see TupleBase，拥有完整的元素访问、修改、函数式、切片、增删等接口
     *   - 支持结构化绑定，特化 `std::tuple_size` 和 `std::tuple_element`
     *
     *   注意事项：
     *   - const 传递：`const Tuple` 通过 `make_const` 确保成员指针类型为 `const T*` 而非 `T* const`
     */
    template <typename... Args>
    struct Tuple : Tuple<void, xyu::make_isequence_t<sizeof...(Args)>, Args...>
    {
        friend class TupleBase<Tuple, xyu::make_isequence_t<sizeof...(Args)>>;
        template <typename R, typename I> friend struct TupleView;
        using Tuple<void, xyu::make_isequence_t<sizeof...(Args)>, Args...>::Tuple;
    };

    template <typename... Args, xyu::size_t... indexs>
    struct Tuple<void, xyu::isequence_c<indexs...>, Args...> :
        private TagVal<Args, indexs>..., TupleBase<Tuple<Args...>, xyu::isequence_c<indexs...>>
    {
        // 获取类型 (末尾增加 void 防止 空元组 编译错误)
        template <xyu::size_t index>
        using arg_t = xyu::t_tps_get<xyu::typelist_c<Args..., void>, index>;

    public:
        // 默认构造函数
        constexpr Tuple() = default;

        // 构造函数 (辅助列表初始化推导)
        template <xyu::size_t N = sizeof...(Args), typename = xyu::t_enable<(N > 0)>>
        constexpr explicit Tuple(const Args&... args) noexcept(xyu::t_can_nothrow_cpconstr<Args...>)
                : TagVal<Args, indexs>(args)... {}

        // 任意构造函数
        template <typename Arg2, typename... Args2, xyu::t_enable<(sizeof...(Args2) > 1 || xyu::t_can_construct<arg_t<0>, Arg2>) && sizeof...(Args2) < sizeof...(Args), bool> = true>
        constexpr explicit Tuple(Arg2&& arg, Args2&&... args) noexcept(noexcept(Tuple{xyu::special_v, xyu::make_isequence<sizeof...(Args2)+1>(), xyu::make_isequence<sizeof...(Args2)+1, sizeof...(Args)>(),xyu::forward<Arg2>(arg), xyu::forward<Args2>(args)...}))
                : Tuple{xyu::special_v, xyu::make_isequence<sizeof...(Args2)+1>(), xyu::make_isequence<sizeof...(Args2)+1, sizeof...(Args)>(), xyu::forward<Arg2>(arg), xyu::forward<Args2>(args)...} {}

        // 元组解包构造
        template <typename Arg2, xyu::t_enable<xyu::t_is_tuple<Arg2> && !xyu::t_can_construct<arg_t<0>, Arg2> && xyu::t_rem_refer<Arg2>::count() == sizeof...(indexs), bool> = false>
        constexpr explicit Tuple(Arg2&& arg) noexcept((... && xyu::t_can_nothrow_construct<arg_t<indexs>, xyu::t_sync_cr<Arg2, __::tuple_get_type<Arg2, indexs>>>))
                : TagVal<Args, indexs>(xyu::forward<Arg2>(arg).template get<indexs>())... {}

        using TupleBase<Tuple<Args...>, xyu::isequence_c<indexs...>>::get;

    protected:
        // 获取元素
        template <xyu::size_t abs_idx>
        constexpr decltype(auto) do_get() noexcept { return this->TagVal<arg_t<abs_idx>, abs_idx>::get(); }

    private:
        // 辅助构造函数
        template <xyu::size_t... as, xyu::size_t... ls, typename... Args2>
        constexpr Tuple(xyu::special_t, xyu::isequence_c<as...>, xyu::isequence_c<ls...>, Args2&&... args)
        noexcept((... && xyu::t_can_nothrow_construct<arg_t<as>, Args2>) && xyu::t_can_nothrow_dfconstr<arg_t<ls>...>)
                : TagVal<arg_t<as>, as>(xyu::forward<Args2>(args))... {}
    };

    // 推导指引
    template <typename... Args>
    Tuple(Args&&...) -> Tuple<xyu::t_decay<Args>...>;
}

/// 元组构造
namespace xylu::xycoll
{
    /**
     * @brief 构造元组
     * @param args 待封装的元素
     * @return Tuple˂xyu::t_decay˂Args˃...˃
     */
    template <typename... Args>
    constexpr auto make_tuple(Args&&... args) noexcept(noexcept(Tuple{ xyu::forward<Args>(args)... }))
    { return Tuple{ xyu::forward<Args>(args)... }; }

    namespace __
    {
        struct TupleUnparkImpl
        {
            // 获取指定深度与位置的元素
            // 深度信息: cur[(indexN, countN), (indexN-1, countN-1), ..., (index0, count0)]
            // 参数处理完毕，移动构造元组 (index 达到 count，且深度is为空)
            template <xyu::size_t depth, xyu::size_t count, typename... VArgs>
            static constexpr auto unpark(xyu::isequence_c<count, count>, VArgs&&... vargs)
            noexcept(noexcept(make_tuple(xyu::forward<VArgs>(vargs)...)))
            { return make_tuple(xyu::forward<VArgs>(vargs)...); }

            // 本层参数处理完毕，深度回升 (本层 index 达到 count，但深度is非空)
            template <xyu::size_t depth, typename... Args, xyu::size_t count, xyu::size_t prev_index, xyu::size_t prev_count, xyu::size_t... ps, typename... VArgs,
                      typename I = xyu::isequence_c<prev_index + 1, prev_count, ps...>>
            static constexpr auto unpark(Args&&... args, xyu::isequence_c<count, count, prev_index, prev_count, ps...>, VArgs&&... vargs)
            noexcept(noexcept(unpark<depth+1, Args...>(xyu::t_val<Args>()..., I{}, xyu::t_val<VArgs>()...)))
            { return unpark<depth+1, Args...>(xyu::forward<Args>(args)..., I{}, xyu::forward<VArgs>(vargs)...); }

            // 解包元组
            template <xyu::size_t depth, typename Tp, typename... Args, xyu::size_t... ps, typename... VArgs, xyu::size_t... ids,
                      typename tArgs = xyu::typelist_c<xyu::t_sync_cr<Tp, typename std::tuple_element<ids,xyu::t_rem_cr<Tp>>::type>...>>
            static constexpr auto unpark_tp(Tp&& tp, xyu::isequence_c<ids...>, Args&&... args, xyu::isequence_c<ps...> stack, VArgs&&... vargs)
            noexcept(noexcept(unpark<depth,xyu::t_tps_get<tArgs,ids>..., Args...>(xyu::t_val<Tp>().template get<ids>()..., xyu::t_val<Args>()..., stack, xyu::t_val<VArgs>()...)))
            {
                return unpark<depth,xyu::t_tps_get<tArgs,ids>..., Args...>(xyu::forward<Tp>(tp).template get<ids>()...,
                    xyu::forward<Args>(args)..., stack, xyu::forward<VArgs>(vargs)...);
            }
            // 处理本层参数，遇到元组消耗深度解包 (本层 index < count, unpard_depth > 0, 当前元素为元组)
            template <xyu::size_t depth, typename Tp, typename... Args, xyu::size_t index, xyu::size_t count, xyu::size_t... ps, typename... VArgs,
                      xyu::size_t tp_count = xyu::t_rem_cr<Tp>::count(),
                      typename nI = xyu::isequence_c<0, tp_count, index, count, ps...>,
                      typename tI = xyu::make_isequence_t<tp_count>,
                      typename = xyu::t_enable<(index < count && depth > 0 && xyu::t_is_tuple<Tp>)>>
            static constexpr auto unpark(Tp&& tp, Args&&... args, xyu::isequence_c<index, count, ps...>, VArgs&&... vargs)
            noexcept(noexcept(unpark_tp<depth-1, Args...>(xyu::t_val<Tp>(), tI{}, xyu::t_val<Args>()..., nI{}, xyu::t_val<VArgs>()...)))
            { return unpark_tp<depth-1, Args...>(xyu::forward<Tp>(tp), tI{}, xyu::forward<Args>(args)..., nI{}, xyu::forward<VArgs>(vargs)...); }

            // 处理本层参数，直接获取参数 (本层 index < count, 当前为普通元素 或 为元组但 depth 为 0)
            template <xyu::size_t depth, typename Arg, typename... Args, xyu::size_t index, xyu::size_t count, xyu::size_t... ps, typename... VArgs,
                      typename nI = xyu::isequence_c<index + 1, count, ps...>,
                      typename = xyu::t_enable<(index < count && (depth == 0 || !xyu::t_is_tuple<Arg>))>>
            static constexpr auto unpark(Arg&& arg, Args&&... args, xyu::isequence_c<index, count, ps...>, VArgs&&... vargs)
            noexcept(noexcept(unpark<depth, Args...>(xyu::t_val<Args>()..., nI{}, xyu::t_val<VArgs>()..., xyu::t_val<Arg>())))
            { return unpark<depth, Args...>(xyu::forward<Args>(args)..., nI{}, xyu::forward<VArgs>(vargs)..., xyu::forward<Arg>(arg)); }
        };

        // 异常检测
        template <xyu::size_t unpark_depth, typename... Args>
        constexpr bool make_tuple_unpark_noexcept() noexcept
        {
            if constexpr (unpark_depth == 0) return noexcept(make_tuple(xyu::t_val<Args>()...));
            else return noexcept(TupleUnparkImpl::unpark<unpark_depth, Args...>(xyu::t_val<Args>()..., xyu::isequence_c<0, sizeof...(Args)>{}));
        }
    }
    /**
     * @brief 深度解包构造元组
     * @tparam unpark_depth 解包深度（0 表示不解包，1 表示解包一层，...）
     * @param args 可能包含嵌套元组的参数包
     *
     * @details
     *   递归展开参数中的所有元组元素，将嵌套结构展平为单一层级的元组。
     *   解包深度控制递归的层数：
     *   - `unpark_depth = 0`：不解包，直接构造
     *   - `unpark_depth = 1`：仅展开最外层的元组
     *   - `unpark_depth = 2`：展开两层嵌套，依此类推
     *
     *   非元组类型的元素原样保留；达到深度限制后，剩余元组不再展开。
     *   解包过程会进行一次初始构造，以及一次完整的移动。
     *
     *   @code
     *   auto tp = make_tuple(1, make_tuple(2, make_tuple(3, 4)), 5);
     *   auto tp1 = make_tuple(size_c˂1˃{}, tp);
     *   // tp1 = (1, (2, (3,4) ), 5)
     *   auto tp2 = make_tuple(size_c˂2˃{}, tp);
     *   // tp2 = (1, 2, (3,4), 5)
     *   auto tp3 = make_tuple(size_c˂3˃{}, tp);
     *   // tp3 = (1, 2, 3, 4, 5)
     *   @endcode
     */
    template <xyu::size_t unpark_depth, typename... Args>
    constexpr decltype(auto) make_tuple(xyu::size_c<unpark_depth>, Args&&... args) noexcept(__::make_tuple_unpark_noexcept<unpark_depth, Args...>())
    {
        if constexpr (unpark_depth == 0) return make_tuple(xyu::forward<Args>(args)...);
        else return __::TupleUnparkImpl::unpark<unpark_depth, Args...>(xyu::forward<Args>(args)..., xyu::isequence_c<0, sizeof...(Args)>{});
    }

}

/// 格式化
namespace xylu::xystring
{
    /**
     * @brief Tuple 格式化
     *
     * @details
     *   rep 和 ex 两部分均使用分号 `,` 分割多个子串，
     *
     * --- [:rep] - 格式符 ---
     *   分号分割的各个子串依次作为每个元素的 `represent` 格式。
     *   若子串数量少于元素个数，最后一个子串将重复用于后续所有元素。
     *   若 `rep` 为空，所有元素使用默认格式。
     *
     * --- [?ex] - 拓展格式符 ---
     *   - 第一个子串作为**元素间的分隔符**，在两个相邻元素之间输出
     *   - 后续子串依次作为每个元素的 `expand` 格式（最后一个子串同样传播）
     *
     * --- [|lay] - 布局说明符 ---
     *   该部分仅控制整个元组的布局，不影响内部元素
     *
     * **示例**
     * @code
     * Tuple˂int, double, std::string˃ tpl{42, 3.14, "hello"};
     *
     * xyfmt("{}", tpl);                       // (42,3.14,hello)
     * xyfmt("{? | }", tpl);                   // (42 | 3.14 | hello)
     * xyfmt("{:X, .2e, ? | }", tpl);          // (2A | 3.14e+00 | hello)
     * @endcode
     */
    template <typename D, xyu::size_t... indexs>
    struct FormatImpl<xyu::TupleBase<D, xyu::isequence_c<indexs...>>>
    {
        using T = xyu::TupleBase<D, xyu::isequence_c<indexs...>>;

        // 强制运行时解析类型
        static constexpr bool runtime = (... || FormatCore::is_runtime<xycoll::__::tuple_get_type<T, indexs>>);

        /// 编译期解析元组格式
        template <xyu::size_t index, bool R = runtime, typename = xyu::t_enable<!R>>
        constexpr static void parse_compile_impl(xyu::size_t& count, StringView& rep, StringView& ex, bool& last_rep, bool& last_ex)
        {
            if (XY_UNLIKELY(count > FormatCore::limit)) return;
            auto re = update_repex(rep, ex, last_rep, last_ex);
            using Arg = xycoll::__::tuple_get_type<T, index>;
            xyu::size_t add = FormatCore::call_parse_compile<Arg>({}, re.rep, re.ex);
            if (XY_UNLIKELY(add > FormatCore::limit)) count = -1;
            else count += add;
        }
        template <bool R = runtime, typename = xyu::t_enable<!R>>
        constexpr static xyu::size_t parse_compile(StringView represent, StringView expand) noexcept
        {
            if constexpr (sizeof...(indexs) == 0) return 2;
            auto sl = parse_split(expand);
            xyu::size_t count = FormatImpl<int>::check_total(2, sl.split.size(), sizeof...(indexs) - 1, XY_CurPos("FormatImpl<Tuple>::parse_compile"));
            StringView rep = represent.strip(' '), ex = sl.last.strip(' ');
            bool last_rep = rep.empty(), last_ex = ex.empty();
            (..., parse_compile_impl<indexs>(count, rep, ex, last_rep, last_ex));
            return count;
        }

        /// 运行期解析元组格式
        template <xyu::size_t index>
        static void parse_runtime_impl(xyu::size_t& count, const T& tp, StringView& rep, StringView& ex, bool& last_rep, bool& last_ex)
        {
            if (XY_UNLIKELY(count > FormatCore::limit)) return;
            auto re = update_repex(rep, ex, last_rep, last_ex);
            using Arg = xycoll::__::tuple_get_type<T, index>;
            xyu::size_t add = FormatCore::call_parse_runtime<Arg>(&tp.template get<index>(), {}, re.rep, re.ex);
            if (XY_UNLIKELY(add > FormatCore::limit)) count = -1;
            else count += add;
        }
        static xyu::size_t parse_runtime(const T& tp, StringView represent, StringView expand) noexcept(noexcept((... && FormatCore::call_parse_runtime<xycoll::__::tuple_get_type<T,indexs>>({},{},{},{}))))
        {
            auto sl = parse_split(expand);
            if constexpr (sizeof...(indexs) == 0) return 2;
            xyu::size_t count = FormatImpl<int>::check_total(2, sl.split.size(), sizeof...(indexs) - 1, XY_CurPos("FormatImpl<Tuple>::parse_runtime"));
            StringView rep = represent.strip(' '), ex = sl.last.strip(' ');
            bool last_rep = rep.empty(), last_ex = ex.empty();
            (..., parse_runtime_impl<indexs>(tp, count, rep, ex, last_rep, last_ex));
            return count;
        }

        /// 运行期解析剩余元组格式
        template <xyu::size_t index>
        static void parse_remain_impl(const T& tp, xyu::size_t& count, StringView& rep, StringView& ex, bool& last_rep, bool& last_ex)
        {
            if (XY_UNLIKELY(count >= FormatCore::limit)) return;
            auto re = update_repex(rep, ex, last_rep, last_ex);
            using Arg = xycoll::__::tuple_get_type<T, index>;
            xyu::size_t add;
            if constexpr (!FormatCore::is_runtime<Arg>) add = FormatCore::call_parse_compile<Arg>({}, re.rep, re.ex);
            else add = FormatCore::call_parse_remain<Arg>(&tp.template get<index>(), Format_Layout{}, re.rep, re.ex);
            if (XY_UNLIKELY(add > FormatCore::limit)) count = -1;
            else count += add;
        }
        static xyu::size_t parse_remain(const T& tp, StringView represent, StringView expand) noexcept(noexcept((... && FormatCore::call_parse_remain<xycoll::__::tuple_get_type<T,indexs>>({},{},{},{}))))
        {
            if constexpr (sizeof...(indexs) == 0) return 2;
            auto sl = parse_split(expand);
            xyu::size_t count = FormatImpl<int>::check_total(2, sl.split.size(), sizeof...(indexs) - 1, XY_CurPos("FormatImpl<Tuple>::parse_remain"));
            StringView rep = represent.strip(' '), ex = sl.last.strip(' ');
            bool last_rep = rep.empty(), last_ex = ex.empty();
            (..., parse_remain_impl<indexs>(tp, count, rep, ex, last_rep, last_ex));
            return count;
        }

        /// 格式化元组
        template <typename Stream, xyu::size_t index>
        constexpr static void format_impl(Stream& out, const T& tp, StringView split, StringView& rep, StringView& ex, bool& last_rep, bool& last_ex)
        {
            if (index) out << split;
            auto re = update_repex(rep, ex, last_rep, last_ex);
            using Arg = xycoll::__::tuple_get_type<T, index>;
            FormatCore::call_format<Arg>(out, &tp.template get<index>(), Format_Layout{}, re.rep, re.ex);
        }
        template <typename Stream>
        static void format(Stream& out, const T& tp, StringView represent, StringView expand) noexcept((... && noexcept(FormatCore::call_format<xycoll::__::tuple_get_type<T,indexs>>(out,{},{},{},{}))))
        {
            if constexpr (sizeof...(indexs) == 0) { out << "()"; return; }
            auto sl = parse_split(expand);
            StringView rep = represent.strip(' '), ex = sl.last.strip(' ');
            bool last_rep = rep.empty(), last_ex = ex.empty();
            out << '(';
            (..., format_impl<Stream, indexs>(out, tp, sl.split, rep, ex, last_rep, last_ex));
            out << ')';
        }

    private:
        // 获取分隔字符串与后续格式串
        // "" -> split = ",", last = ""
        // "a" → split = "a", last = ""
        // "a,b" → split = "a", last = "b"
        // ",b" → split = "", last = "b"
        // "(a,b),c" → split = "a,b", last = "c"
        // "(a,c" → split = "(a", last = "c"
        constexpr static auto parse_split(StringView re) noexcept { return FormatImpl<int>::parse_split(re); }

        // 构建 rep 和 ex 格式串
        struct repex_split { StringView rep; StringView ex; };
        constexpr static repex_split update_repex(StringView& rep, StringView& ex, bool& last_rep, bool& last_ex) noexcept
        {
            repex_split repex;
            if (last_rep) repex.rep = rep;
            else {
                auto sl = parse_split(rep);
                rep = sl.last.lstrip(' ');
                if (rep.empty()) rep = sl.split, last_rep = true;
                repex.rep = sl.split;
            }
            if (last_ex) repex.ex = ex;
            else {
                auto sl = parse_split(ex);
                ex = sl.last.lstrip(' ');
                if (ex.empty()) ex = sl.split, last_ex = true;
                repex.ex = sl.split;
            }
            return repex;
        }
    };

    template <typename R, typename I>
    struct FormatImpl<xyu::TupleView<R, I>> : FormatImpl<xyu::TupleBase<xyu::TupleView<R, I>, I>> {};

    template <typename... Args>
    struct FormatImpl<xyu::Tuple<Args...>> : FormatImpl<xyu::Tuple<void, xyu::make_isequence_t<sizeof...(Args)>, Args...>> {};

    template <typename... Args, xyu::size_t... indexs>
    struct FormatImpl<xyu::Tuple<void, xyu::isequence_c<indexs...>, Args...>>
    : FormatImpl<xyu::TupleBase<xyu::Tuple<Args...>, xyu::isequence_c<indexs...>>> {};
}

/// 结构化绑定
namespace std
{
    //获取元组容量
    template <typename D, typename I>
    struct tuple_size<xyu::TupleBase<D,I>> : xyu::size_c<xyu::TupleBase<D,I>::count()> {};
    template <typename R, typename I>
    struct tuple_size<xyu::TupleView<R,I>> : xyu::size_c<xyu::TupleView<R,I>::count()> {};
    template <typename... Args>
    struct tuple_size<xyu::Tuple<Args...>> : xyu::size_c<xyu::Tuple<Args...>::count()> {};

    //获取数据类型
    template <xyu::size_t index, typename D, typename I>
    struct tuple_element<index, xyu::TupleBase<D,I>>
    { using type = xylu::xycoll::__::tuple_get_type<xyu::TupleBase<D,I>, index>; };
    template <xyu::size_t index, typename R, typename I>
    struct tuple_element<index, xyu::TupleView<R,I>>
    { using type = xylu::xycoll::__::tuple_get_type<xyu::TupleView<R,I>, index>; };
    template <xyu::size_t index, typename... Args>
    struct tuple_element<index, xyu::Tuple<Args...>>
    { using type = xylu::xycoll::__::tuple_get_type<xyu::Tuple<Args...>, index>; };
}

/// 哈希
namespace xylu::xymath
{
    template <typename Tp>
    struct Hasher<Tp, xyu::t_enable<xyu::t_is_tuple<Tp>>, 6>
    {
        constexpr xyu::size_t operator()(const Tp& tp) noexcept
        {
            constexpr xyu::size_t count = Tp::count();
            xyu::size_t hash = count;
            if constexpr (count) tp.apply([&](const auto&... vs){ hash = make_hash(hash, vs...); });
            return hash;
        }
    };
}
