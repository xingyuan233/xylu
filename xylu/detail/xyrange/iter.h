#pragma once

/**
 * @file    xyrange/iter.h
 * @brief   策略式迭代器框架
 * @date    2025-12-23
 */

#include "./_.h"
#include "../xytraits/func.h"
#include "../xytraits/arrptr.h"
#include "../xytraits/tmpl.h"
#include "../xymath/of.h"
#include "../xycoll/tagval.h"

/// 迭代器模板类
namespace xylu::xyrange
{
    namespace __
    {
        // 获取所属类，默认为 void
        template <typename Iter, typename = void>
        struct get_owner_impl { using type = void; };
        template <typename Iter>
        struct get_owner_impl<Iter, xyu::t_void<typename xyu::t_rem_cr<Iter>::Owner_t>>
        { using type = typename xyu::t_rem_cr<Iter>::Owner_t; };
        template <typename Iter>
        using get_owner = typename get_owner_impl<xyu::t_rem_cr<Iter>>::type;
    }

    /**
     * @brief 基于策略设计模式的通用迭代器模板
     * @details
     *   `Iterator` 通过模板参数接受一系列“策略类”，每个策略类负责实现迭代器的一种特定行为。
     *   通过组合不同的策略，可以灵活地构建出功能各异的迭代器，而无需编写大量的重复代码。
     *
     *   策略类的实现要求:
     *   - 通过 `operator()(iter&)` 或 `operator()(const iter&)` 调用
     *   - 可以为非空类 (即可以有非静态成员变量，但必须可以默认初始化)
     *   - 不需要 Iter& 或 Iter 的返回值 (即对于 Increment/Decrement/Forward/Backward 等操作，直接声明为 void)
     *
     * @tparam Storage     [必须] 存储迭代器的核心状态（如指针、或其他迭代器）
     * @tparam Valid       [可选] 判断迭代器当前是否有效
     * @tparam Dereference [必须] 实现解引用操作 (`operator*`)
     * @tparam Address     [可选] 实现地址获取操作 (`operator->`)
     * @tparam Increment   [可选] 实现自增操作 (`operator++`)
     * @tparam Decrement   [可选] 实现自减操作 (`operator--`)
     * @tparam Forward     [可选] 实现前进N步操作 (`operator+=`)
     * @tparam Backward    [可选] 实现后退N步操作 (`operator-=`)
     * @tparam Distance    [可选] 计算两个迭代器之间的距离 (`operator-`)
     * @tparam Equal       [可选] 实现相等比较 (`operator==`,`!=`)，若未提供，可自动从 Compare 推导
     * @tparam Compare     [可选] 实现其他比较 (`operator<`,`>`,`>=`,`<=`)
     *
     * @note 对于不支持的策略，直接使用 `void` 来禁用相应的功能
     * @attention 允许不同策略类但同 Storage 的迭代器相互构造或赋值
     */
    template <typename Storage,
              typename Valid, typename Dereference, typename Address,
              typename Increment, typename Decrement, typename Forward, typename Backward,
              typename Distance, typename Equal, typename Compare
    >
    class Iterator : public Storage,
                     xyu::TagVal<Valid, -1>, xyu::TagVal<Dereference, -2>, xyu::TagVal<Address, -3>,
                     xyu::TagVal<Increment, -4>, xyu::TagVal<Decrement, -5>,
                     xyu::TagVal<Forward, -6>, xyu::TagVal<Backward, -7>, xyu::TagVal<Distance, -8>,
                     xyu::TagVal<Equal, -9>, xyu::TagVal<Compare, -10>
    {
        static_assert(!xyu::t_is_same<Dereference, void>, "Dereference policy cannot be None");

    public:
        /* 构造赋值 */
        using Storage::Storage;
        
        // 默认构造
        constexpr Iterator() = default;
        // 复制构造
        constexpr Iterator(const Iterator&) = default;
        // 移动构造
        constexpr Iterator(Iterator&&) = default;
        // 复制赋值
        constexpr Iterator& operator=(const Iterator&) = default;
        // 移动赋值
        constexpr Iterator& operator=(Iterator&&) = default;

        /// Storage + 其他组件构造
    #define XY_part_and_index Valid,valid,-1, Dereference,deref,-2, Address,addr,-3, Increment,inc,-4, Decrement,dec,-5, Forward,next,-6, Backward,prev,-7, Distance,dis,-8, Equal,eq,-9, Compare,cmp,-10
    #define XY_constr_noexcept_gen( Part, name, index ) && noexcept(xyu::TagVal<Part,index>{xyu::special_v, other.name()})
    #define XY_constr_gen( Part, name, index ) , xyu::TagVal<Part,index>{xyu::special_v, other.name()}
        template <typename... Ps, typename... Args, typename = xyu::t_enable<sizeof...(Args) && xyu::t_can_listinit<Storage, Args...>>>
        constexpr Iterator(const Iterator<Ps...>& other, Args&&... args) noexcept(xyu::t_can_nothrow_listinit<Storage, Args...> XY_PP_FORWARD( XY_constr_noexcept_gen, 3, XY_part_and_index ))
        : Storage{xyu::forward<Args>(args)...} XY_PP_FORWARD( XY_constr_gen, 3, XY_part_and_index ) {}

        /// 同 Storage 迭代器 构造
        template <typename... Ps, typename = xyu::t_enable<!xyu::t_is_same<Iterator, Iterator<Storage, Ps...>>>>
        constexpr Iterator(const Iterator<Storage, Ps...>& other) noexcept(xyu::t_can_nothrow_cpconstr<Storage> XY_PP_FORWARD( XY_constr_noexcept_gen, 3, XY_part_and_index ))
        : Storage{other} XY_PP_FORWARD( XY_constr_gen, 3, XY_part_and_index ) {}
    #undef XY_constr_gen
    #undef XY_constr_noexcept_gen

        /// 同 Storage 迭代器 赋值
    #define XY_assign_noexcept_gen( Part, name, index ) && (!xyu::t_can_assign<Part, decltype(other.name())> || xyu::t_can_nothrow_assign<Part, decltype(other.name())>)
    #define XY_assign_gen( Part, name, index ) if constexpr (xyu::t_can_assign<Part, decltype(other.name())>) xyu::TagVal<Part,index>::get() = other.name();
        template <typename... Ps, typename = xyu::t_enable<!xyu::t_is_same<Iterator, Iterator<Storage, Ps...>>>>
        constexpr Iterator& operator=(const Iterator<Storage, Ps...>& other) noexcept(xyu::t_can_nothrow_cpassign<Storage> XY_PP_FORWARD( XY_assign_noexcept_gen, 3, XY_part_and_index ))
        {
            Storage::operator=(other);
            XY_PP_FORWARD( XY_assign_gen, 3, XY_part_and_index ) {}
            return *this;
        }
    #undef XY_assign_gen
    #undef XY_assign_noexcept_gen

        template <typename... Ps, typename... Args, xyu::t_enable<sizeof...(Args) && !xyu::t_can_listinit<Storage, Args...>, bool> = false>
        constexpr Iterator(const Iterator<Ps...>& other, Args&&... args) = delete;

        template <typename Storage_t, typename... Ps, typename = xyu::t_enable<!xyu::t_is_same<Storage_t, Storage>>>
        constexpr Iterator(Iterator<Storage_t, Ps...>) = delete;

        template <typename Storage_t, typename... Ps, typename = xyu::t_enable<!xyu::t_is_same<Storage_t, Storage>>>
        constexpr Iterator& operator=(Iterator<Storage_t, Ps...>) = delete;

        /* 迭代器操作 */
        /// 判断有效
        constexpr explicit operator bool() const noexcept(noexcept(valid()(self()))) { return valid()(self()); }
        /// 解引用
        constexpr decltype(auto) operator*() const noexcept(noexcept(deref()(self()))) { return deref()(self()); }
        /// 获取对象地址
        template <typename Test = Address, typename = xyu::t_enable<!xyu::t_is_void<Test>>>
        constexpr decltype(auto) operator->() const noexcept(noexcept(addr()(self()))) { return addr()(self()); }
        template <typename Address_t = Address, xyu::t_enable<xyu::t_is_void<Address_t>, bool> = false>
        constexpr decltype(auto) operator->() = delete;

        /* 迭代器移动 */
        /// 自增
        template <typename Test = Increment, typename = xyu::t_enable<!xyu::t_is_void<Test>>>
        constexpr Iterator& operator++() noexcept(noexcept(inc()(self()))) { inc()(self()); return *this; }
        template <typename Increment_t = Increment, xyu::t_enable<xyu::t_is_void<Increment_t>, bool> = false>
        constexpr Iterator& operator++() = delete;
        /// 自增后置
        template <typename Test = Increment, typename = xyu::t_enable<!xyu::t_is_void<Test>>>
        constexpr Iterator operator++(int) noexcept(noexcept(++Iterator(*this))) { Iterator r(*this); inc()(self()); return r; }
        template <typename Increment_t = Increment, xyu::t_enable<xyu::t_is_void<Increment_t>, bool> = false>
        constexpr Iterator operator++(int) = delete;
        /// 自减
        template <typename Test = Decrement, typename = xyu::t_enable<!xyu::t_is_void<Test>>>
        constexpr Iterator& operator--() noexcept(noexcept(dec()(self()))) { dec()(self()); return *this; }
        template <typename Decrement_t = Decrement, xyu::t_enable<xyu::t_is_void<Decrement_t>, bool> = false>
        constexpr Iterator& operator--() = delete;
        /// 自减后置
        template <typename Test = Decrement, typename = xyu::t_enable<!xyu::t_is_void<Test>>>
        constexpr Iterator operator--(int) noexcept(noexcept(--Iterator(*this))) { Iterator r(*this); dec()(self()); return r; }
        template <typename Decrement_t = Decrement, xyu::t_enable<xyu::t_is_void<Decrement_t>, bool> = false>
        constexpr Iterator operator--(int) = delete;
        /// 自增
        template <typename Difference, typename Test = Forward, typename = xyu::t_enable<!xyu::t_is_void<Test>>>
        constexpr Iterator& operator+=(Difference n) noexcept(noexcept(next()(self(), n))) { next()(self(), n); return *this; }
        template <typename Forward_t = Forward, typename Difference, xyu::t_enable<xyu::t_is_void<Forward_t>, bool> = false>
        constexpr Iterator& operator+=(Difference n) = delete;
        /// 前进
        template <typename Difference, typename Test = Forward, typename = xyu::t_enable<!xyu::t_is_void<Test>>>
        constexpr Iterator operator+(Difference n) const noexcept(noexcept(Iterator(*this) += n)) { return Iterator(*this) += n; }
        template <typename Forward_t = Forward, typename Difference, xyu::t_enable<xyu::t_is_void<Forward_t>, bool> = false>
        constexpr Iterator operator+(Difference n) = delete;
        /// 自减
        template <typename Difference, typename Test = Backward, typename = xyu::t_enable<!xyu::t_is_void<Test>>>
        constexpr Iterator& operator-=(Difference n) noexcept(noexcept(prev()(self(), n))) { prev()(self(), n); return *this; }
        template <typename Backward_t = Backward, typename Difference, xyu::t_enable<xyu::t_is_void<Backward_t>, bool> = false>
        constexpr Iterator& operator-=(Difference n) = delete;
        /// 后退
        template <typename Difference, typename Test = Backward, typename = xyu::t_enable<!xyu::t_is_void<Test>>>
        constexpr Iterator operator-(Difference n) const noexcept(noexcept(Iterator(*this) -= n)) { return Iterator(*this) -= n; }
        template <typename Backward_t = Backward, typename Difference, xyu::t_enable<xyu::t_is_void<Backward_t>, bool> = false>
        constexpr Iterator operator-(Difference n) = delete;

        /* 迭代器比较 */
        /// 差值
        template <typename Test = Distance, typename = xyu::t_enable<!xyu::t_is_void<Test>>>
        constexpr auto operator-(const Iterator& other) const noexcept(noexcept(dis()(self(), other.self()))) { return dis()(self(), other.self()); }
        template <typename Distance_t = Distance, xyu::t_enable<xyu::t_is_void<Distance_t>, bool> = false>
        constexpr auto operator-(const Iterator& other) = delete;

        /// 比较
    private:
        template <typename T>
        constexpr static bool can_compare = !xyu::t_is_void<Compare> &&
            (xyu::t_is_same_nocr<T, Iterator> || xyu::t_can_call<xyu::t_add_lrefer<Compare>, Iterator&, const T&>);
    public:
        template <typename Test = Compare, xyu::t_enable<!xyu::t_is_void<Test>, bool> = true>
        constexpr auto compare(const Iterator& other) const noexcept(noexcept(cmp()(self(), other.self()))) { return cmp()(self(), other.self()); }
        template <typename T, typename Test = Compare, xyu::t_enable<!xyu::t_is_void<Test> && !xyu::t_is_same_nocr<T, Iterator> && xyu::t_can_call<xyu::t_add_lrefer<Compare>, Iterator&, const T&>, bool> = true>
        constexpr auto compare(const T& other) const noexcept(noexcept(cmp()(self(), other))) { return cmp()(self(), other); }
        template <typename T, xyu::t_enable<!can_compare<T>, bool> = false>
        constexpr auto compare(const T&) = delete;

        template <typename T, xyu::t_enable<can_compare<T>, bool> = true>
        constexpr bool operator <(const T& other) const noexcept(noexcept(compare(other))) { return compare(other) < 0; }
        template <typename T, xyu::t_enable<!can_compare<T>, bool> = false>
        constexpr bool operator <(const T&) = delete;

        template <typename T, xyu::t_enable<can_compare<T>, bool> = true>
        constexpr bool operator<=(const T& other) const noexcept(noexcept(compare(other))) { return compare(other) <= 0; }
        template <typename T, xyu::t_enable<!can_compare<T>, bool> = false>
        constexpr bool operator<=(const T&) = delete;

        template <typename T, xyu::t_enable<can_compare<T>, bool> = true>
        constexpr bool operator >(const T& other) const noexcept(noexcept(compare(other))) { return compare(other) > 0; }
        template <typename T, xyu::t_enable<!can_compare<T>, bool> = false>
        constexpr bool operator >(const T&) = delete;

        template <typename T, xyu::t_enable<can_compare<T>, bool> = true>
        constexpr bool operator>=(const T& other) const noexcept(noexcept(compare(other))) { return compare(other) >= 0; }
        template <typename T, xyu::t_enable<!can_compare<T>, bool> = false>
        constexpr bool operator>=(const T&) = delete;

        /// 相等
    private:
        template <typename T>
        constexpr static bool can_equals = can_compare<T> || (!xyu::t_is_void<Equal> &&
            (xyu::t_is_same_nocr<T, Iterator> || xyu::t_can_call<xyu::t_add_lrefer<Equal>, Iterator&, const T&>));
    public:
        template <typename Test = Equal, xyu::t_enable<!xyu::t_is_void<Test>, bool> = true>
        constexpr bool equals(const Iterator& other) const noexcept(noexcept(eq()(self(), other.self()))) { return eq()(self(), other.self()); }
        template <typename T, typename Test = Equal, xyu::t_enable<!xyu::t_is_void<Test> && !xyu::t_is_same_nocr<T, Iterator> && xyu::t_can_call<xyu::t_add_lrefer<Equal>, Iterator&, const T&>, bool> = true>
        constexpr bool equals(const T& other) const noexcept(noexcept(eq()(self(), other))) { return eq()(self(), other); }
        template <typename T, typename Test = Iterator, xyu::t_enable<xyu::t_is_void<Equal> || !xyu::t_is_same_nocr<T, Iterator> && !xyu::t_can_call<xyu::t_add_lrefer<Equal>, Iterator&, const T&>, decltype(xyu::t_val<Test>().compare(xyu::t_val<const T&>()))> = true>
        constexpr bool equals(const T& other) const noexcept(noexcept(compare(other))) { return compare(other) == 0; }
        template <typename T, xyu::t_enable<!can_equals<T>, bool> = false>
        constexpr bool equals(const T&) = delete;

        template <typename T, xyu::t_enable<can_equals<T>, bool> = true>
        constexpr bool operator==(const T& other) const noexcept(noexcept(equals(other))) { return equals(other); }
        template <typename T, xyu::t_enable<!can_equals<T>, bool> = false>
        constexpr bool operator==(const T&) = delete;

        template <typename T, xyu::t_enable<can_equals<T>, bool> = true>
        constexpr bool operator!=(const T& other) const noexcept(noexcept(equals(other))) { return !equals(other); }
        template <typename T, xyu::t_enable<!can_equals<T>, bool> = false>
        constexpr bool operator!=(const T&) = delete;

    private:
        constexpr Iterator& self() const noexcept { return const_cast<Iterator&>(*this); }
    #define XY_get_part_gen( Part, name, index ) \
        template <typename Test = Part, xyu::t_enable<!xyu::t_is_void<Test>, bool> = true> \
        constexpr Test& name() const noexcept { return const_cast<Iterator&>(*this).xyu::TagVal<Part, index>::get(); } \
        template <typename Test = Part, xyu::t_enable<xyu::t_is_void<Test>, bool> = false> \
        constexpr auto name() const noexcept { using Ret = struct{}; return Ret{}; }
    XY_PP_FORWARD( XY_get_part_gen, 3, XY_part_and_index )
    #undef XY_get_part_gen

    public:
    #define XY_get_part_gen( Part, name, index ) \
        template <typename Test = Part, xyu::t_enable<!xyu::t_is_void<Test>, bool> = true> \
        constexpr Test& _##name() noexcept { return xyu::TagVal<Part, index>::get(); } \
        template <typename Test = Part, xyu::t_enable<xyu::t_is_void<Test>, bool> = false> \
        constexpr auto _##name() noexcept = delete;
    XY_PP_FORWARD( XY_get_part_gen, 3, XY_part_and_index )
    #undef XY_get_part_gen
    #undef XY_part_and_index

        #define XY_get_type_gen( Part ) using Part##_t = Part;
        XY_PP_FORWARD( XY_get_type_gen, 1,
                       Storage, Valid, Dereference, Address,
                       Increment, Decrement, Forward, Backward,
                       Distance, Equal, Compare )
        #undef XY_get_type_gen

        using Owner_t = __::get_owner<Storage>;

        // 友元，为支持同类型 Storage 迭代器构造赋值
        template <typename S, typename V, typename D, typename A, typename I, typename Dc, typename F, typename B, typename Ds, typename E, typename C>
        friend class Iterator;
    };

}

/// 迭代器属性 - 部件标签
namespace xylu::xyrange::IterTag
{
    struct _tag {};

    struct Storage     : _tag, xyu::size_c<0> {};
    struct Valid       : _tag, xyu::size_c<1> {};
    struct Dereference : _tag, xyu::size_c<2> {};
    struct Address     : _tag, xyu::size_c<3> {};
    struct Increment   : _tag, xyu::size_c<4> {};
    struct Decrement   : _tag, xyu::size_c<5> {};
    struct Forward     : _tag, xyu::size_c<6> {};
    struct Backward    : _tag, xyu::size_c<7> {};
    struct Distance    : _tag, xyu::size_c<8> {};
    struct Equal       : _tag, xyu::size_c<9> {};
    struct Compare     : _tag, xyu::size_c<10>{};
}
namespace xylu::xytraits
{
    namespace __
    {
        template <typename T>
        struct is_iterator : false_c {};
        template <typename... Args>
        struct is_iterator<xyu::Iterator<Args...>> : true_c {};
    }
    /// 是否为迭代器
    template <typename... T>
    constexpr bool t_is_iter = (... && __::is_iterator<t_rem_cr<T>>::value);

    /// 获取迭代器解引用类型
    template <typename T>
    struct t_access_base<T, t_enable<t_is_iter<T>>> { using type = decltype(*t_val<T>()); };

    /// 是否为解引用后得到指定类型的迭代器
    template <typename Iter, typename... T>
    constexpr bool t_is_iter_to = t_is_iter<Iter> && (... || t_is_same<t_access<Iter>, T>);


    /// 是否为单向迭代器
    template <typename... T>
    constexpr bool t_is_siter = []
    {
        if constexpr (!t_is_iter<T...>) return false;
        else return (... && !xyu::t_is_void<typename t_rem_cr<T>::Increment_t>);
    }();

    /// 是否为双向迭代器
    template <typename... T>
    constexpr bool t_is_diter = []
    {
        if constexpr (!t_is_siter<T...>) return false;
        else return (... && !xyu::t_is_void<typename t_rem_cr<T>::Decrement_t>);
    }();

    /// 是否为随机迭代器
    template <typename... T>
    constexpr bool t_is_riter = []
    {
        if constexpr (!t_is_diter<T...>) return false;
        else return (... && !xyu::t_is_void<typename t_rem_cr<T>::Forward_t>) &&
                    (... && !xyu::t_is_void<typename t_rem_cr<T>::Backward_t>);
    }();


    /// 迭代器部件标签
    namespace __
    {
        // 解包模板
        template <typename Change, typename Origin>
        struct unpart_iter_part { using type = Change; };
        template <template<typename...>class TT, typename... ExArgs>
        struct unpart_iter_part<t_pack_tmpl<TT, ExArgs...>, void> { using type = void; };
        template <template<typename...>class TT, typename... ExArgs, typename Origin>
        struct unpart_iter_part<t_pack_tmpl<TT, ExArgs...>, Origin> { using type = TT<Origin>; };

        // 替换单个组件
        template <typename Iter, typename Tag, typename Change>
        struct change_iter_part
        {
            static_assert(xyu::t_is_base<xylu::xyrange::IterTag::_tag, Tag> && Tag::value <= 10, "unknown tag");
            using part = typename unpart_iter_part<Change, t_tps_get<Iter, Tag::value>>::type;
            using type = t_unpack_args<t_tps_set<t_unpack_args<Iter, typelist_c>, Tag::value, part>, xyu::Iterator>;
        };

        // 依次替换每一个组件
        template <typename Iter, typename... Arg>
        struct change_iter { static_assert(sizeof...(Arg) == 0, "invalid arguments"); };
        template <typename Iter>
        struct change_iter<Iter> { using type = Iter; };
        template <typename Iter, typename Tag, typename Change, typename... Args>
        struct change_iter<Iter, Tag, Change, Args...>
        {
            using nIter = typename change_iter_part<Iter, Tag, Change>::type;
            using type = typename change_iter<nIter, Args...>::type;
        };

        template <typename Iter, typename... Args>
        struct change_iter_check
        {
            static_assert(t_is_iter<Iter>, "unsupported iterator type");
            static_assert(sizeof...(Args) % 2 == 0, "uncomplete params");
            using type = typename change_iter<t_rem_cr<Iter>, t_rem_cr<Args>...>::type;
        };
    }
    /**
     * @brief 在编译期修改一个 `Iterator` 类型的策略组合，生成一个新的迭代器类型
     *
     * @tparam Iter 原始的 `Iterator` 类型。
     * @tparam Args 一系列的修改指令，每个指令是一个 {Tag, Change}对
     *
     * ### 修改规则:
     * - `Tag`: 一个迭代器部件标志，如 `IterTag::Dereference`
     * - `Change`: 新的策略类，如果需要应用在原始策略类上，使用 `t_pack_tmpl` 包装
     *
     * @bar 示例
     * @code
     * // 将 Iter 的比较策略替换为反向比较
     * using NewIter = t_chg_iter˂Iter, IterTag::Compare, t_pack_tmpl˂IterPolicies::Compare::Reverse˃˃;
     * // NewIter 的比较策略将是 IterPolicies::Compare::Reverse˂typename Iter::Compare_t˃
     * @endcode
     */
    template <typename Iter, typename... Args>
    using t_chg_iter = typename __::change_iter_check<Iter, Args...>::type;

    namespace __
    {
        template <typename Iter, typename... Tags>
        struct remove_iter { using type = Iter; };
        template <typename Iter, typename Tag, typename... Tags>
        struct remove_iter<Iter, Tag, Tags...>
        {
            static_assert(xyu::t_is_base<xylu::xyrange::IterTag::_tag, Tag> && Tag::value <= 10, "unknown tag");
            static_assert(!xyu::t_is_same<Tag, xylu::xyrange::IterTag::Storage>, "cannot remove Storage_t");
            static_assert(!xyu::t_is_same<Tag, xylu::xyrange::IterTag::Dereference>, "cannot remove Dereference_t");
            using nIter = t_unpack_args<t_tps_set<t_unpack_args<Iter, typelist_c>, Tag::value, void>, xyu::Iterator>;
            using type = typename remove_iter<nIter, Tags...>::type;
        };

        template <typename Iter, typename... Tags>
        struct remove_iter_check
        {
            static_assert(t_is_iter<Iter>, "unsupported iterator type");
            using type = typename remove_iter<t_rem_cr<Iter>, t_rem_cr<Tags>...>::type;
        };
    }
    /**
     * @brief 从一个 `Iterator` 类型中移除一个或多个策略组件
     * @tparam Iter 原始的 `Iterator` 类型。
     * @tparam Tags 要移除的策略组件的标签，如 `IterTag::Decrement`
     * @note 不能移除 `Storage_t`, `Dereference_t` 这些必需的策略
     */
    template <typename Iter, typename... Tags>
    using t_rem_iter = typename __::remove_iter_check<Iter, Tags...>::type;

}

/// 迭代器策略类
namespace xylu::xyrange
{
    namespace __
    {
    #define XY_has_member_test_gen_part( fun, name, index, place ) \
        template <typename T, typename = decltype(xyu::t_val<T&>().name)> \
        xyu::size_c<index> has_member_##fun##_test(place);
    #define XY_has_member_test_gen( fun, ... ) \
        XY_PP_FORWARD( XY_has_member_test_gen_part, 4, XY_PP_PARAM_LINS(fun, 3, __VA_ARGS__) ) \
        template <typename...>             \
        xyu::size_c<0> has_member_##fun##_test(...); \
        template <typename T>              \
        constexpr xyu::size_t has_member_##fun = decltype(has_member_##fun##_test<xyu::t_rem_cr<T>>('0'))::value;

        // 是否包含成员 value/val/data/item/elem/v
        XY_has_member_test_gen( value,  value,1,char&&,  val,2,const char&&,  data,3,const char&,
                                        item,4,int&&,    elem,5,const int&&,  v,6,const int&)
        template <typename T>
        constexpr decltype(auto) get_member_value(T& v) noexcept
        {
            constexpr auto type = has_member_value<T>;
            if constexpr (type == 1) return (v.value);
            else if constexpr (type == 2) return (v.val);
            else if constexpr (type == 3) return (v.data);
            else if constexpr (type == 4) return (v.item);
            else if constexpr (type == 5) return (v.elem);
            else if constexpr (type == 6) return (v.v);
            else static_assert(type != 0, "cannot get value from this type");
        }

        // 是否包含成员 next/nxt
        XY_has_member_test_gen( next,  next,1,char,  nxt,2,int)
        template <typename T>
        constexpr decltype(auto) get_member_next(T& v) noexcept
        {
            constexpr auto type = has_member_next<T>;
            if constexpr (type == 1) return (v.next);
            else if constexpr (type == 2) return (v.nxt);
            else static_assert(type != 0, "cannot get next from this type");
        }

        // 是否包含成员 prev/pre/prv
        XY_has_member_test_gen( prev,  prev,1,char,  pre,2,int,  prv,3,long)
        template <typename T>
        constexpr decltype(auto) get_member_prev(T& v) noexcept
        {
            constexpr auto type = has_member_prev<T>;
            if constexpr (type == 1) return (v.prev);
            else if constexpr (type == 2) return (v.pre);
            else if constexpr (type == 3) return (v.prv);
            else static_assert(type != 0, "cannot get prev from this type");
        }

    #undef XY_has_member_test_gen_part
    #undef XY_has_member_test_gen

        // 判断两个迭代器的所属类是否相同
        template <typename It1, typename It2>
        constexpr bool is_same_iter_owner = xyu::t_is_same<get_owner<It1>, get_owner<It2>>;

    }

    /// 迭代器策略类
    namespace IterPolicies
    {
        /// 默认策略
        namespace Default
        {
            // 指针
            template <typename T, typename Owner = void>
            struct Storage
            {
                using Owner_t = Owner;

                T* p;
                constexpr Storage(T* p = nullptr) noexcept : p{p} {}
            };

            // 直接转为 bool
            struct Valid
            {
                template <typename Iter>
                constexpr bool operator()(const Iter& it) noexcept(noexcept(static_cast<bool>(it.p)))
                { return static_cast<bool>(it.p); }
            };

            // 直接解引用
            struct Dereference
            {
                template <typename Iter>
                constexpr decltype(auto) operator()(Iter& it) noexcept(noexcept(*it.p))
                {
                    if constexpr (!XY_RELEASE && xyu::t_is_pointer<decltype(it.p)>)
                        if (XY_UNLIKELY(it.p == nullptr))
                            xyu::E_Logic::abort_null_pointer(XY_CurPos("Iterator::Dereference"));
                    return *it.p;
                }
            };

            // 直接获取地址
            struct Address
            {
                template <typename Iter>
                constexpr decltype(auto) operator()(Iter& it) noexcept { return (it.p); }
            };

            // 直接自增
            struct Increment
            {
                template <typename Iter>
                constexpr void operator()(Iter& it) noexcept(noexcept(++it.p))
                {
                    if constexpr (XY_DEBUG && xyu::t_is_pointer<decltype(it.p)>) if (!xyu::t_constant())
                        xyu::calc_of_add<xyu::size_t>(xyu::size_t(it.p), 1u, XY_CurPos("Iterator::Increment"));
                    ++it.p;
                }
            };

            // 直接自减
            struct Decrement
            {
                template <typename Iter>
                constexpr void operator()(Iter& it) noexcept(noexcept(--it.p))
                {
                    if constexpr (XY_DEBUG && xyu::t_is_pointer<decltype(it.p)>) if (!xyu::t_constant())
                        xyu::calc_of_sub<xyu::size_t>(xyu::size_t(it.p), 1u, XY_CurPos("Iterator::Decrement"));
                    --it.p;
                }
            };

            // 直接前进
            struct Forward
            {
                template <typename Iter, typename Diff>
                constexpr void operator()(Iter& it, Diff step) noexcept(noexcept(it.p += step))
                {
                    if constexpr (XY_DEBUG && xyu::t_is_pointer<decltype(it.p)>) if (!xyu::t_constant())
                        xyu::calc_of_add<xyu::size_t>(xyu::size_t(it.p), step, XY_CurPos("Iterator::Forward"));
                    it.p += step;
                }
            };

            // 直接后退
            struct Backward
            {
                template <typename Iter, typename Diff>
                constexpr void operator()(Iter& it, Diff step) noexcept(noexcept(it.p -= step))
                {
                    if constexpr (XY_DEBUG && xyu::t_is_pointer<decltype(it.p)>) if (!xyu::t_constant())
                        xyu::calc_of_sub<xyu::size_t>(xyu::size_t(it.p), step, XY_CurPos("Iterator::Backward"));
                    it.p -= step;
                }
            };

            // 直接相减
            struct Distance
            {
                template <typename It1, typename It2, typename = xyu::t_enable<__::is_same_iter_owner<It1, It2>>>
                constexpr auto operator()(const It1& it1, const It2& it2) noexcept(noexcept(it1.p - it2.p))
                { return it1.p - it2.p; }
            };

            // 直接比较
            struct Equal
            {
                template <typename It1, typename It2, typename = xyu::t_enable<__::is_same_iter_owner<It1, It2>>>
                constexpr bool operator()(const It1& it1, const It2& it2) noexcept(noexcept(it1.p == it2.p))
                { return it1.p == it2.p; }
            };

            // 相减比较
            struct Compare
            {
                template <typename It1, typename It2, typename = xyu::t_enable<__::is_same_iter_owner<It1, It2>>>
                constexpr auto operator()(const It1& it1, const It2& it2) noexcept(noexcept(it1.p - it2.p))
                { return it1.p - it2.p; }
            };
        }

        /// 迭代器策略
        namespace Iter
        {
            // 存储类
            template <typename It, typename Owner = void>
            struct Storage
            {
                using Owner_t = xyu::t_cond<xyu::t_is_void<Owner>, __::get_owner<It>, Owner>;

                It p;
                constexpr Storage(const It& it = {}) noexcept(xyu::t_can_nothrow_cpconstr<It>) : p{it} {}
                constexpr Storage(It&& it) noexcept(xyu::t_can_nothrow_mvconstr<It>) : p{xyu::move(it)} {}
            };

            // 获取地址
            struct Address
            {
                template <typename Iter>
                constexpr decltype(auto) operator()(Iter& it) noexcept(noexcept(it.p.operator->()))
                { return it.p.operator->(); }
            };
        }

        /// 节点策略
        namespace Node
        {
            // 解引用
            struct Dereference
            {
                template <typename Iter>
                constexpr decltype(auto) operator()(Iter& it) noexcept
                {
                    if constexpr (XY_DEBUG && xyu::t_is_pointer<decltype(it.p)>)
                        if (XY_UNLIKELY(it.p == nullptr))
                            xyu::E_Logic::abort_null_pointer(XY_CurPos("Iterator::Dereference"));
                    return __::get_member_value(*it.p);
                }
            };

            // 解引用后取地址
            struct Address
            {
                template <typename Iter>
                constexpr decltype(auto) operator()(Iter& it) noexcept(noexcept(&*it.p)) { return &*it.p; }
            };

            // 下一个节点
            struct Increment
            {
                template <typename Iter>
                constexpr void operator()(Iter& it) noexcept
                { it.p = static_cast<decltype(it.p)>(__::get_member_next(*it.p)); }
            };

            // 上一个节点
            struct Decrement
            {
                template <typename Iter>
                constexpr void operator()(Iter& it) noexcept
                { it.p = static_cast<decltype(it.p)>(__::get_member_prev(*it.p)); }
            };
        }

        /// 常量封装
        namespace Const
        {
            // 解引用
            template <typename Deref>
            struct Dereference : private Deref
            {
                template <typename Iter>
                constexpr decltype(auto) operator()(Iter& it) noexcept(noexcept(base()(it)))
                { return xyu::make_const(base()(it)); }
            private:
                constexpr Deref& base() noexcept { return static_cast<Deref&>(*this); }
            };

            // 取地址
            template <typename Addr>
            struct Address : private Addr
            {
                template <typename Iter>
                constexpr decltype(auto) operator()(Iter& it) noexcept(noexcept(base()(it)))
                { return xyu::make_const(base()(it)); }
            private:
                constexpr Addr& base() noexcept { return static_cast<Addr&>(*this); }
            };
        }

        /// 移动封装
        namespace Mode
        {
            template <typename Deref>
            struct Dereference : private Deref
            {
                template <typename Iter>
                constexpr decltype(auto) operator()(Iter& it) noexcept(noexcept(base()(it)))
                { return xyu::move(base()(it)); }
            private:
                constexpr Deref& base() noexcept { return static_cast<Deref&>(*this); }
            };
        }

        /// 反向封装
        namespace Reverse
        {
            template <typename Part> struct StepInvert;
            namespace __
            {
                template <typename T>
                struct is_reverse_test : xyu::false_c {};
                template <typename P>
                struct is_reverse_test<StepInvert<P>> : xyu::true_c {};
                template <typename T>
                constexpr static bool is_reverse = is_reverse_test<T>::value;

                template <typename T>
                struct count_reverse_test : xyu::size_c<0> {};
                template <template <typename> class Tmpl, typename U>
                struct count_reverse_test<Tmpl<U>>
                        : xyu::size_c<count_reverse_test<U>::value + is_reverse<Tmpl<U>>> {};
                template <typename T>
                constexpr static xyu::size_t count_reverse = count_reverse_test<T>::value;
            }

            // 反转一步后操作
            template <typename Part>
            struct StepInvert : private Part
            {
                template <typename Iter, xyu::size_t reverse_count = __::count_reverse<Part> + 1>
                constexpr decltype(auto) operator()(Iter& it) noexcept((reverse_count & 1) && noexcept(base()(++Iter(it))) || !(reverse_count & 1) && noexcept(base()(--Iter(it))))
                {
                    // 保证多次继承后结果依然正确
                    if constexpr (reverse_count & 1) return base()(++Iter(it));
                    else return base()(--Iter(it));
                }
            private:
                constexpr Part& base() noexcept { return static_cast<Part&>(*this); }
            };
            template <>
            struct StepInvert<void> {};
            template <typename Part>
            using StepInvert_t = xyu::t_cond<xyu::t_is_void<Part>, void, StepInvert<Part>>;

            // 交换两个对象
            template <typename Part>
            struct SwapArg : private Part
            {
                template <typename It1, typename It2, typename = decltype(xyu::t_val<Part&>()(xyu::t_val<It2&>(), xyu::t_val<It1&>()))>
                constexpr auto operator()(It1& it1, It2& it2) noexcept(noexcept(base()(it2, it1)))
                { return base()(it2, it1); }
            private:
                constexpr Part& base() noexcept { return static_cast<Part&>(*this); }
            };
            template <>
            struct SwapArg<void> {};
            template <typename Part>
            using SwapArg_t = xyu::t_cond<xyu::t_is_void<Part>, void, SwapArg<Part>>;
        }

    }
}

/// 常用迭代器
namespace xylu::xyrange
{
    /**
     * @brief 原生指针随机访问迭代器
     * @tparam T 迭代器(指针)指向的元素类型
     * @tparam Owner 所属类，仅相同所属类迭代器可进行构造赋值比较
     */
    template <typename T, typename Owner = void>
    using Iterator_Ptr = Iterator<
        IterPolicies::Default::Storage<T, Owner>,
        IterPolicies::Default::Valid, IterPolicies::Default::Dereference, IterPolicies::Default::Address,
        IterPolicies::Default::Increment, IterPolicies::Default::Decrement,
        IterPolicies::Default::Forward, IterPolicies::Default::Backward,
        IterPolicies::Default::Distance, IterPolicies::Default::Equal, IterPolicies::Default::Compare
    >;

    /**
     * @brief 遍历链表等节点的迭代器
     * @tparam Node 节点类型，应包含 val, next, prev(若双向) 成员
     * @tparam Owner 所属类，仅相同所属类迭代器可进行构造赋值比较
     */
    template <typename Node, typename Owner = void>
    using Iterator_Node = Iterator<
        IterPolicies::Default::Storage<Node, Owner>,
        IterPolicies::Default::Valid, IterPolicies::Node::Dereference, IterPolicies::Node::Address,
        IterPolicies::Node::Increment, xyu::t_cond<__::has_member_prev<Node> != 0, IterPolicies::Node::Decrement, void>,
        void, void, void, IterPolicies::Default::Equal, void
    >;

    /**
     * @brief 迭代器 const 封装
     * @details 修改解引用和地址获取策略，使其返回的数据经过深层 const 修饰
     * @tparam Iter 原始迭代器类型
     */
    template <typename Iter>
    using Iterator_Const = xyu::t_chg_iter<Iter,
        IterTag::Dereference, xyu::t_pack_tmpl<IterPolicies::Const::Dereference>,
        IterTag::Address, xyu::t_pack_tmpl<IterPolicies::Const::Address>
    >;

    /**
     * @brief 迭代器 移动 封装
     * @details 修改解引用策略返回右值引用
     * @tparam Iter 原始迭代器类型
     */
    template <typename Iter>
    using Iterator_Move = xyu::t_chg_iter<Iter,
        IterTag::Dereference, xyu::t_pack_tmpl<IterPolicies::Mode::Dereference>
    >;

    /**
     * @brief 迭代器 反向 封装
     * @details 通过反转自操作或参数实现，直接将 [begin,end) 反转为 (end,begin]
     * @tparam Iter 原始迭代器类型
     */
    template <typename Iter>
    using Iterator_Reverse = Iterator<typename Iter::Storage_t,
        IterPolicies::Reverse::StepInvert<typename Iter::Valid_t>,
        IterPolicies::Reverse::StepInvert<typename Iter::Dereference_t>,
        IterPolicies::Reverse::StepInvert_t<typename Iter::Address_t>,
        typename Iter::Decrement_t, typename Iter::Increment_t,
        typename Iter::Backward_t, typename Iter::Forward_t,
        IterPolicies::Reverse::SwapArg_t<typename Iter::Distance_t>,
        typename Iter::Equal_t,
        IterPolicies::Reverse::SwapArg_t<typename Iter::Compare_t>
    >;

    /**
     * @brief 迭代器 反向 直接封装
     * @details 通过反转自操作或参数实现，需直接传入 [--end,--begin)
     * @tparam Iter 原始迭代器类型
     */
    template <typename Iter>
    using Iterator_Reverse_Direct = Iterator<typename Iter::Storage_t,
        typename Iter::Valid_t, typename Iter::Dereference_t, typename Iter::Address_t,
        typename Iter::Decrement_t, typename Iter::Increment_t,
        typename Iter::Backward_t, typename Iter::Forward_t,
        IterPolicies::Reverse::SwapArg_t<typename Iter::Distance_t>,
        typename Iter::Equal_t,
        IterPolicies::Reverse::SwapArg_t<typename Iter::Compare_t>
    >;

    namespace __
    {
    #define XY_has_method_test_gen( name, test, Part ) \
        template <typename Iter, typename = decltype(test)> \
        Part has_method_##name##_test(int);           \
        template <typename>                            \
        void has_method_##name##_test(...);

        XY_PP_FORWARD(XY_has_method_test_gen, 3,
                      address, xyu::t_val<Iter>().operator->(), IterPolicies::Iter::Address,
                      increment, ++xyu::t_val<Iter&>(), IterPolicies::Default::Increment,
                      decrement, --xyu::t_val<Iter&>(), IterPolicies::Default::Decrement,
                      forward, xyu::t_val<Iter&>()+=1, IterPolicies::Default::Forward,
                      backward, xyu::t_val<Iter&>()-=1, IterPolicies::Default::Backward,
                      distance, xyu::t_val<Iter>()-xyu::t_val<Iter>(), IterPolicies::Default::Distance,
                      equals, xyu::t_val<Iter>()==xyu::t_val<Iter>(), IterPolicies::Default::Equal,
                      compare, xyu::t_val<Iter>()-xyu::t_val<Iter>(), IterPolicies::Default::Compare)

    #undef XY_has_method_test_gen
    }
    /**
     * @brief 将任意符合 C++ 标准的迭代器封装成 `Iterator`
     * @details 通过 SFINAE 检测传入的 `Iter` 类型支持哪些操作，自动匹配合适的组件
     * @tparam Iter 任意有效的迭代器类型
     * @tparam Owner 所属类，仅相同所属类迭代器可进行构造赋值比较，默认与原 Iter 相同
     */
    template <typename Iter, typename Owner = void>
    using Iterator_iter = Iterator<IterPolicies::Iter::Storage<Iter, Owner>,
            IterPolicies::Default::Valid, IterPolicies::Default::Dereference, IterPolicies::Default::Address,
            decltype(__::has_method_increment_test<Iter>(0)), decltype(__::has_method_decrement_test<Iter>(0)),
            decltype(__::has_method_forward_test<Iter>(0)), decltype(__::has_method_backward_test<Iter>(0)),
            decltype(__::has_method_distance_test<Iter>(0)),
            decltype(__::has_method_equals_test<Iter>(0)),
            decltype(__::has_method_compare_test<Iter>(0))
    >;

}
