#pragma once

/**
 * @file    xytraits/tplis.h
 * @brief   编译期类型列表：用于管理静态类型集合的异构容器
 * @date    2025-12-21
 */

#include "./seq.h"

/** 类型列表定义 */
namespace xylu::xytraits
{
    /**
     * @brief 编译期类型列表
     * @tparam Args 类型参数包
     */
    template <typename... Args>
    struct typelist_c {};
}

/** 类型列表创建 */
namespace xylu::xytraits
{
    namespace __
    {
        template <typename TI>
        struct make_typelist_tmpl { using type = typelist_c<TI>; };
        template <template<typename...>class TT, typename... Args>
        struct make_typelist_tmpl<TT<Args...>> { using type = typelist_c<Args...>; };
    }
    /// 通过其他模板创建
    template <typename TI>
    using make_typelist_t = typename __::make_typelist_tmpl<TI>::type;

    /// 通过传入的变量类型创建类型列表
    template <typename... Args>
    constexpr typelist_c<Args...> make_typelist(Args&&...) noexcept { return {}; }
}

/** 类型列表操作 */
namespace xylu::xytraits
{
    /* 获取 */

    namespace __
    {
        template <typename TI> struct tps_count;
        template <template<typename...>class TT, typename... Args>
        struct tps_count<TT<Args...>> : size_c<sizeof...(Args)> {};
    }
    /// 获取类型列表中类型个数
    template <typename TI>
    constexpr xyu::size_t t_tps_count = __::tps_count<t_rem_cr<TI>>::value;

    namespace __
    {
#if !__has_builtin(__type_pack_element)
        template <xyu::size_t index, typename = void, typename... Args> struct tps_get_impl;

    #define XY_tps_get_impl_gen( n ) \
        template <XY_PP_TAKE(n, XY_PP_PARAM_INDEX_20( typename Arg ) ), typename... Args> \
        struct tps_get_impl<n-1, void, XY_PP_TAKE(n, XY_PP_PARAM_INDEX_20( Arg ) ), Args...> { using type = Arg##n; };
    XY_PP_FORWARD( XY_tps_get_impl_gen, 1, XY_PP_PARAM_INDEX_20() )
    #undef XY_tps_get_impl_gen
        template <xyu::size_t index, XY_PP_PARAM_INDEX_20( typename Arg ), typename... Args>
        struct tps_get_impl<index, xyu::t_enable<(index >= 20)>, XY_PP_PARAM_INDEX_20( Arg ), Args...>
        { using type = typename tps_get_impl<index - 20, void, Args...>::type; };

#endif

        template <typename TI, xyu::size_t index> struct tps_get_check;
        template <template<typename...>class TT, typename... Args, xyu::size_t index>
        struct tps_get_check<TT<Args...>, index>
        {
            static_assert(index < sizeof...(Args), "index out of range");
#if __has_builtin(__type_pack_element)
            using type = __type_pack_element<index, Args...>;
#else
            using type = typename tps_get_impl<index, void, Args...>::type;
#endif
        };
    }
    /// 获取 类型列表 中 index 索引处的类型
    template <typename TI, xyu::size_t index>
    using t_tps_get = typename __::tps_get_check<t_rem_cr<TI>, index>::type;

    namespace __
    {
#if __has_builtin(__type_pack_element)
        template <typename TI, typename Is> struct tps_sel;
        template <template<typename...>class TT, typename... Args, xyu::size_t... is>
        struct tps_sel<TT<Args...>, isequence_c<is...>>
        { using type = TT<__type_pack_element<is, Args...>...>; };
#endif

        template <typename TI>
        struct tps_sel_check
        {
#if __has_builtin(__type_pack_element)
            template <xyu::size_t... is>
            using type = typename tps_sel<TI, typename seq_sel_fix<t_tps_count<TI>, is...>::type>::type;
#else
            template <xyu::size_t... is, template<typename...>class TT, typename... Args>
            static TT<t_tps_get<typelist_c<Args...>, is>...> tps_sel_impl(TT<Args...>);

            template <xyu::size_t... is>
            using type = decltype(tps_sel_impl<is...>(t_val<TI>()));
#endif
        };
    }
    /**
     * @brief 获取 类型列表 指定索引组 的 类型组
     * @bar 示例
     * @code
     * t_tps_sel˂typelist_c˂int,bool,float,double,char˃, 0,2,4˃ == typelist_c˂int,float,char˃
     * @endcode
     */
    template <typename TI, xyu::size_t... indexs>
    using t_tps_sel = typename __::tps_sel_check<t_rem_cr<TI>>::template type<indexs...>;

    namespace __
    {
        template <typename TI>
        struct tps_sub
        {
            static constexpr xyu::size_t count = t_tps_count<TI>;

#if __has_builtin(__type_pack_element)
            template <xyu::size_t start, xyu::size_t len>
            using type = typename tps_sel<TI, make_isequence_t<
                    (start < count) ? start : count,
                    (start + len < start || start + len > count) ? count : start + len
            >>::type;
#else
            template <xyu::size_t start, xyu::size_t len, typename TI_, typename = void, typename... lArgs>
            struct tps_sub_impl;
            template <xyu::size_t start, xyu::size_t len, template<typename...>class TT, typename... Args, typename... lArgs>
            struct tps_sub_impl<start, len, TT<Args...>, t_enable<len == 0>, lArgs...>
            { using type = TT<lArgs...>; };

            template <xyu::size_t start, xyu::size_t len, template<typename...>class TT, typename... Args>
            struct tps_sub_impl<start, len, TT<Args...>, t_enable<(len && start && start >= sizeof...(Args))>>
            { using type = TT<>; };
        #define XY_tps_sub_impl_gen( n ) \
            template <xyu::size_t start, xyu::size_t len, template<typename...>class TT, XY_PP_TAKE(n, XY_PP_PARAM_INDEX_20( typename Arg ) ), typename... Args> \
            struct tps_sub_impl<start, len, TT<XY_PP_TAKE(n, XY_PP_PARAM_INDEX_20( Arg ) ), Args...>, t_enable<(start == n && len)>> \
            { using type = typename tps_sub_impl<start-n, len, TT<Args...>>::type; };
        XY_PP_FORWARD( XY_tps_sub_impl_gen, 1, XY_PP_PARAM_INDEX_20() )
        #undef XY_tps_sub_impl_gen
            template <xyu::size_t start, xyu::size_t len, template<typename...>class TT, XY_PP_PARAM_INDEX_20( typename Arg ), typename... Args>
            struct tps_sub_impl<start, len, TT<XY_PP_PARAM_INDEX_20( Arg ), Args...>, t_enable<(start > 20 && len && start < 20 + sizeof...(Args))>>
            { using type = typename tps_sub_impl<start - 20, len, TT<Args...>>::type; };

            template <xyu::size_t start, xyu::size_t len, template<typename...>class TT, typename... Args, typename... lArgs>
            struct tps_sub_impl<start, len, TT<Args...>, t_enable<(start == 0 && len && len >= sizeof...(Args))>, lArgs...>
            { using type = TT<lArgs..., Args...>; };
        #define XY_tps_sub_impl_gen( n ) \
            template <xyu::size_t start, xyu::size_t len, template<typename...>class TT, XY_PP_TAKE(n, XY_PP_PARAM_INDEX_20( typename Arg ) ), typename... Args, typename... lArgs> \
            struct tps_sub_impl<start, len, TT<XY_PP_TAKE(n, XY_PP_PARAM_INDEX_20( Arg ) ), Args...>, t_enable<(start == 0 && len == n)>, lArgs...> \
            { using type = TT<lArgs..., XY_PP_TAKE(n, XY_PP_PARAM_INDEX_20( Arg ) )>; };
        XY_PP_FORWARD( XY_tps_sub_impl_gen, 1, XY_PP_PARAM_INDEX_20() )
        #undef XY_tps_sub_impl_gen
            template <xyu::size_t start, xyu::size_t len, template<typename...>class TT, XY_PP_PARAM_INDEX_20( typename Arg ), typename... Args, typename... lArgs>
            struct tps_sub_impl<start, len, TT<XY_PP_PARAM_INDEX_20( Arg ), Args...>, t_enable<(start == 0 && len > 20 && len < 20 + sizeof...(Args))>, lArgs...>
            { using type = typename tps_sub_impl<start, len - 20, TT<Args...>, void, lArgs..., XY_PP_PARAM_INDEX_20( Arg )>::type; };

            template <xyu::size_t start, xyu::size_t len>
            using type = typename tps_sub_impl<start, len, TI>::type;
#endif
        };
    }
    /**
     * @brief 获取 类型组中 [start, start + len) 范围的子类型组
     * @bar 示例
     * @code
     * t_tps_sub˂typelist_c˂int,bool,float,double˃, 0,2˃ == typelist_c˂int,bool˃
     * @endcode
     */
    template <typename TI, xyu::size_t start, xyu::size_t len = xyu::size_t(-1)>
    using t_tps_sub = typename __::tps_sub<t_rem_cr<TI>>::template type<start, len>;

    /* 增加 */

    namespace __
    {
        template <typename TI, typename... Args> struct tps_app;
        template <template<typename...>class TT, typename... oArgs, typename... nArgs>
        struct tps_app<TT<oArgs...>, nArgs...> { using type = TT<oArgs..., nArgs...>; };
    }
    /// 在类型组的 末尾 追加 Args...
    template <typename TI, typename... Args>
    using t_tps_app = typename __::tps_app<t_rem_cr<TI>, Args...>::type;

    namespace __
    {
        template <typename LTI, typename RTI, typename... Args> struct tps_cat;
        template <template<typename...>class LT, typename... lArgs, template<typename...>class RT, typename... rArgs, typename... Args>
        struct tps_cat<LT<lArgs...>, RT<rArgs...>, Args...> { using type = LT<lArgs..., Args..., rArgs...>; };
    }
    /// 拼接两个类型组并用 Args 连接 (lArgs..., Args..., rArgs...)
    template <typename LTI, typename RTI, typename... Args>
    using t_tps_cat = typename __::tps_cat<t_rem_cr<LTI>, t_rem_cr<RTI>, Args...>::type;

    /// 在类型组的 index 索引位置之前插入 Args...
    template <typename TI, xyu::size_t index, typename... Args>
    using t_tps_ins = t_tps_cat<t_tps_sub<TI, 0, index>, t_tps_sub<TI, index>, Args...>;

    /* 删除 */

    /// 删除类型组 中从 start 索引开始的 len 个类型
    template <typename TI, xyu::size_t start, xyu::size_t len = 1>
    using t_tps_del = t_tps_cat<t_tps_sub<TI, 0, start>, t_tps_sub<TI, start+len>>;

    namespace __
    {
        template <typename TI, xyu::size_t... indexs>
        struct tps_ers
        {
            static constexpr auto ids = seq_filter_index(make_isequence<t_tps_count<TI>>(),
                                                         t_seq_sort<isequence_c<indexs...>>{});

            template <xyu::size_t... is>
            static t_tps_sel<TI, ids.data[is]...> tps_ers_impl(isequence_c<is...>);
            using type = decltype(tps_ers_impl(make_isequence<ids.count>()));
        };
    }
    /// 删除类型组 中指定的 索引组
    template <typename TI, xyu::size_t... indexs>
    using t_tps_ers = typename __::tps_ers<t_rem_cr<TI>, indexs...>::type;

    namespace __
    {
        template <typename TI> struct tps_rem_impl;
        template <template<typename...>class TT, typename... Args>
        struct tps_rem_impl<TT<Args...>>
        {
            template <typename Target, xyu::size_t limit, xyu::size_t start, xyu::size_t end>
            static constexpr auto lis = []
            {
                cList<xyu::size_t, sizeof...(Args)> lis;
                xyu::size_t index = 0, count = 0;
                (((index >= start && index < end && count < limit && t_is_same<Args, Target> ?
                   count++ : lis.data[lis.count++] = index), index++), ...);
                return lis;
            }();
        };

        template <typename TI, typename Target, xyu::size_t limit, xyu::size_t start, xyu::size_t end, typename = void>
        struct tps_rem_check
        {
            using impl = tps_rem_impl<TI>;
            static constexpr auto lis = impl::template lis<Target, limit, start, end>;

            template <xyu::size_t... is>
            static t_tps_sel<TI, lis.data[is]...> tps_rem_help(isequence_c<is...>);
            using type = decltype(tps_rem_help(make_isequence<lis.count>()));
        };
        template <typename TI, typename Target, xyu::size_t limit, xyu::size_t start, xyu::size_t end>
        struct tps_rem_check<TI, Target, limit, start, end, t_enable<(limit == 0 || start > t_tps_count<TI> || start >= end)>>
        { using type = TI; };
    }
    /// 删除类型组 的子序列 [start, end) 中等于 Target 的类型，最多删除 limit 个
    template <typename TI, typename Target, xyu::size_t limit = static_cast<xyu::size_t>(-1), xyu::size_t start = 0, xyu::size_t end = xyu::size_t(-1)>
    using t_tps_rem = typename __::tps_rem_check<t_rem_cr<TI>, Target, limit, start, end>::type;

    /* 查找 */

    namespace __
    {
        template <typename TI> struct tps_find_impl;
        template <template<typename...>class TT, typename... Args>
        struct tps_find_impl<TT<Args...>>
        {
            template <typename Target>
            static constexpr xyu::size_t get(xyu::size_t start, xyu::size_t end)
            {
                xyu::size_t ret = xyu::size_t(-1), cur = 0;
                ((cur >= start && cur < end && ret == -1 && t_is_same<Args, Target> ? ret = cur : cur++), ...);
                return ret;
            }
        };

        template <typename Target, xyu::size_t start, xyu::size_t end, typename TI>
        struct tps_find_check
        {
            static constexpr xyu::size_t value = []
            {
                constexpr xyu::size_t count = t_tps_count<TI>;
                if constexpr (start >= count || start >= end) return xyu::size_t(-1);
                else return tps_find_impl<TI>::template get<Target>(start, end);
            }();
        };
    }
    /// 在 类型组 的子类型组 [start, end) 中查找 Target，并返回其首次出现的索引，不存在则返回 -1
    template <typename TI, typename Target, xyu::size_t start = 0, xyu::size_t end = xyu::size_t(-1)>
    constexpr xyu::size_t t_tps_find = __::tps_find_check<Target, start, end, t_rem_cr<TI>>::value;

    /* 修改 */

    namespace __
    {
        template <typename TI, xyu::size_t index, typename Change>
        struct tps_set_check
        {
            static_assert(index < t_tps_count<TI>, "index out of range");
            using type = t_tps_cat<t_tps_sub<TI, 0, index>, t_tps_sub<TI, index+1>, Change>;
        };
    }
    /// 将类型组中 index 索引处的类型替换为 Change
    template <typename TI, xyu::size_t index, typename Change>
    using t_tps_set = typename __::tps_set_check<t_rem_cr<TI>, index, Change>::type;

    namespace __
    {
        template <typename SI, typename Transform> struct t_tps_map;
        template <template<typename... Args>class ST, typename Transform, typename... Args>
        struct t_tps_map<ST<Args...>, Transform> { using type = ST<decltype(Transform{}(t_val<Args>()))...>; };
    }
    /**
     * @brief 将序列中的每个元素应用 Transform 变换
     * @details Transform 必须有 operator(T) 成员函数，并通过返回值类型来表示新类型
     */
    template <typename SI, typename Transform>
    using t_tps_map = typename __::t_tps_map<t_rem_cr<SI>, Transform>::type;

    namespace __
    {
        template <typename TI>
        struct tps_uniq
        {
            template <typename TI_, typename = void, typename... lArgs> struct tps_uniq_impl;
            template <template<typename...>class TT, typename... lArgs>
            struct tps_uniq_impl<TT<>, void, lArgs...> { using type = TT<lArgs...>; };
            template <template<typename...>class TT, typename Arg, typename... Args, typename... lArgs>
            struct tps_uniq_impl<TT<Arg, Args...>, t_enable<t_tps_find<typelist_c<lArgs...>, Arg> == -1>, lArgs...>
            { using type = typename tps_uniq_impl<TT<Args...>, void, lArgs..., Arg>::type; };
            template <template<typename...>class TT, typename Arg, typename... Args, typename... lArgs>
            struct tps_uniq_impl<TT<Arg, Args...>, t_enable<t_tps_find<typelist_c<lArgs...>, Arg> != -1>, lArgs...>
            { using type = typename tps_uniq_impl<TT<Args...>, void, lArgs...>::type; };

            using type = typename tps_uniq_impl<TI>::type;
        };
    }
    /// 类型去重
    template <typename TI>
    using t_tps_uniq = typename __::tps_uniq<t_rem_cr<TI>>::type;

    /* 顺序 */

    namespace __
    {
        template <typename TI>
        struct tps_rev
        {
            using Is = typename seq_rev_index<t_tps_count<TI>>::type;
#if __has_builtin(__type_pack_element)
            using type = typename tps_sel<TI, Is>::type;
#else
            template <template<typename...>class TT, typename... Args, xyu::size_t... Is>
            static TT<t_tps_get<typelist_c<Args...>, Is>...> tps_sel_impl(TT<Args...>, isequence_c<Is...>);
            using type = decltype(tps_sel_impl(t_val<TI>(), Is{}));
#endif
        };
    }
    /// 反转类型组
    template <typename TI>
    using t_tps_rev = typename __::tps_rev<t_rem_cr<TI>>::type;
}
