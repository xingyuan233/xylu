#pragma once

/**
 * @file    xytraits/seq.h
 * @brief   编译期数值序列：支持同构与异构常量的元编程算法库
 * @date    2025-12-21
 */

#include "./crts.h"
#include "./kind.h"
#include "../xycore/macro.h"

/** 模板序列定义 */
namespace xylu::xytraits
{
    /**
     * @brief 编译期序列
     * @tparam T      序列类型
     * @tparam indexs 序列常量
     */
    template <typename T, T... indexs>
    struct sequence_c
    {
        using type = T;
        static constexpr xyu::size_t count = sizeof...(indexs);
    };

    /// 编译期索引序列
    template <xyu::size_t... indexs>
    using isequence_c = sequence_c<xyu::size_t, indexs...>;
}

/** 整数序列创建 */
namespace xylu::xytraits
{
    namespace __
    {
        template <typename T, T... params> struct make_sequence_impl;
        template <typename T>
        struct make_sequence_impl<T> { using type = sequence_c<T>; };
        template <typename T, T size>
        struct make_sequence_impl<T, size>
        {
            using type =
#if __has_builtin(__make_integer_seq)
            __make_integer_seq<sequence_c, T, size>;
#else
            sequence_c<T, __integer_pack(size)...>;
#endif
        };
        template <typename T, T start, T end>
        struct make_sequence_impl<T, start, end>
        {
            template <T... is>
            static sequence_c<T, (is + start)...> make_sequence_range(sequence_c<T, is...>);

            using type = decltype(make_sequence_range(typename
                    make_sequence_impl<T, (start < end) ? end - start : 0>::type{}));
        };
    }
    /**
     * @brief 创建指定整数序列
     * @details
     * 当 params 为空时，返回空的整数序列；
     * 当 params 为一个参数 size 时，返回 [0, size) 的整数序列；
     * 当 params 为两个参数 start, end 时，返回 [start, end) 的整数序列
     */
    template <typename T, T... params>
    using make_sequence_t = typename __::make_sequence_impl<T, params...>::type;

    /**
     * @brief 创建指定索引序列
     * @details
     * 当 params 为空时，返回空的索引序列；
     * 当 params 为一个参数 size 时，返回 [0, size) 的索引序列；
     * 当 params 为两个参数 start, end 时，返回 [start, end) 的索引序列
     */
    template <xyu::size_t... params>
    using make_isequence_t = make_sequence_t<xyu::size_t, params...>;

    /// 创建 [0, size) 的 整数序列
    template <typename T, T size>
    constexpr auto make_sequence() noexcept { return make_sequence_t<T, size>{}; }

    /// 创建 [0, size) 的 索引序列
    template <xyu::size_t size>
    constexpr auto make_isequence() noexcept { return make_sequence<xyu::size_t, size>(); }

    /// 创建 [start, end) 的 整数序列
    template <typename T, T start, T end>
    constexpr auto make_sequence() noexcept { return make_sequence_t<T, start, end>{}; }

    /// 创建 [start, end) 的 索引序列
    template <xyu::size_t start, xyu::size_t end>
    constexpr auto make_isequence() noexcept { return make_sequence<xyu::size_t, start, end>(); }
}

/** 模板序列操作 */
namespace xylu::xytraits
{
    namespace __
    {
        // 其他类型
        template <typename SI>
        struct seq_park_impl_impl { static_assert(t_is_void<SI>, "not supported sequence type"); };
        // 异构序列
        template <template<auto...>class ST, auto... es>
        struct seq_park_impl_impl<ST<es...>>
        {
            using T = decltype((..., es));
            static constexpr bool is_homogeneous = false;
            static constexpr xyu::size_t count = sizeof...(es);
            using ori = ST<es...>;
            using std = sequence_c<T, T(es)...>;
            template <auto... nes> using unpark = ST<nes...>;
        };
        // 其他类型
        template <typename SI>
        struct seq_park_impl : seq_park_impl_impl<SI> {};
        // 提供类型参数的同构序列
        template <template<typename T,T...>class ST, typename T, T... es>
        struct seq_park_impl<ST<T, es...>>
        {
            static constexpr bool is_homogeneous = true;
            static constexpr xyu::size_t count = sizeof...(es);
            using ori = ST<T, es...>;
            using std = sequence_c<T, es...>;
            template <auto... nes> using unpark = ST<T, nes...>;
        };
        // 未提供类型参数的同构序列
        template <typename T, template<T...>class ST, T... es>
        struct seq_park_impl<ST<es...>>
        {
            static constexpr bool is_homogeneous = true;
            static constexpr xyu::size_t count = sizeof...(es);
            using ori = ST<es...>;
            using std = sequence_c<T, es...>;
            template <auto... nes> using unpark = ST<nes...>;
        };
        // 未提供类型参数的空序列
        template <template<auto...>class ST>
        struct seq_park_impl<ST<>>
        {
            static constexpr bool is_homogeneous = true;
            static constexpr xyu::size_t count = 0;
            using ori = ST<>;
            using std = isequence_c<>;
            template <auto... nes> using unpark = ST<nes...>;
        };

        // 打包类型
        template <typename SI>
        using seq_park = seq_park_impl<t_rem_cr<SI>>;

        // 数组
        template <typename T, xyu::size_t count>
        struct cArray { T data[count]; };

        // 可变数组
        template <typename T, xyu::size_t limit>
        struct cList
        {
            T data[limit] {};
            xyu::size_t count = 0;
        };
    }

    /* 获取 */

    /// 获取序列的长度
    template <typename SI>
    constexpr xyu::size_t t_seq_count = __::seq_park<SI>::count;

    namespace __
    {
        // 同构
        template <typename SP, bool = SP::is_homogeneous>
        struct seq_get
        {
            template <typename SI> struct seq_get_impl;
            template <typename T, T... es>
            struct seq_get_impl<sequence_c<T, es...>>
            {
                static constexpr T values[sizeof...(es)] = {es...};
                template <xyu::size_t index>
                static constexpr T get()
                {
                    static_assert(index < sizeof...(es), "index out of range");
                    return values[index];
                }
            };

            template <xyu::size_t index>
            static constexpr auto get() { return seq_get_impl<typename SP::std>::template get<index>(); }
        };
        // 异构
        template <typename SP>
        struct seq_get<SP, false>
        {
#if !__has_builtin(__type_pack_element)
            template <xyu::size_t index, typename = void, auto... es> struct seq_get_impl;

        #define XY_seq_get_impl_gen( n ) \
            template <XY_PP_TAKE(n, XY_PP_PARAM_INDEX_20( auto e ) ), auto... es> \
            struct seq_get_impl<n-1, void, XY_PP_TAKE(n, XY_PP_PARAM_INDEX_20( e ) ), es...> \
            { static constexpr auto value = e##n; };
        XY_PP_FORWARD( XY_seq_get_impl_gen, 1, XY_PP_PARAM_INDEX_20() )
        #undef XY_seq_get_impl_gen
            template <xyu::size_t index, XY_PP_PARAM_INDEX_20( auto e ), auto... es>
            struct seq_get_impl<index, xyu::t_enable<(index >= 20)>, XY_PP_PARAM_INDEX_20( e ), es...>
            { static constexpr auto value = seq_get_impl<index-20, void, es...>::value; };
#endif

            template <typename, xyu::size_t> struct seq_get_check;
            template <xyu::size_t index, template<auto...>class ST, auto... es>
            struct seq_get_check<ST<es...>, index>
            {
                static_assert(index < sizeof...(es), "index out of range");

                static constexpr auto value =
#if __has_builtin(__type_pack_element)
                    __type_pack_element<index, constant_c<decltype(es), es>...>::value;
#else
                    seq_get_impl<index, void, es...>::value;
#endif
            };

            template <xyu::size_t index>
            static constexpr auto get() { return seq_get_check<typename SP::ori, index>::value; }
        };
    }
    /// 获取 序列 中位于 index 索引处的元素
    template <typename SI, xyu::size_t index>
    constexpr auto t_seq_get = __::seq_get<__::seq_park<SI>>::template get<index>();

    namespace __
    {
        template <typename SP, typename SI, typename Is> struct seq_sel_homog;
        template <typename SP, typename T, T... es, xyu::size_t... is>
        struct seq_sel_homog<SP, sequence_c<T, es...>, isequence_c<is...>>
        {
            static constexpr T values[sizeof...(es)] = {es...};
            using type = typename SP::template unpark<values[is]...>;
        };

#if __has_builtin(__type_pack_element)
        template <typename SI, typename Is_> struct seq_sel_heterog;
        template <template<auto...>class ST, auto... es, xyu::size_t... is>
        struct seq_sel_heterog<ST<es...>, isequence_c<is...>>
        { using type = ST<__type_pack_element<is, constant_c<decltype(es), es>...>::value...>; };
#endif

        template <xyu::size_t count, xyu::size_t... is>
        struct seq_sel_fix
        {
            static constexpr auto lis = []
            {
                cList<xyu::size_t, count> lis;
                ((is < count ? lis.data[lis.count++] = is : is), ...);
                return lis;
            }();

            template <xyu::size_t... ids>
            static isequence_c<lis.data[ids]...> seq_sel_impl(isequence_c<ids...>);
            using type = decltype(seq_sel_impl(make_isequence_t<lis.count>{}));
        };

        // 同构
        template <typename SP, bool = SP::is_homogeneous>
        struct seq_sel
        {
            template <xyu::size_t... is>
            using type = typename seq_sel_homog<SP, typename SP::std,
                    typename seq_sel_fix<SP::count, is...>::type>::type;
        };

        // 异构
        template <typename SP>
        struct seq_sel<SP, false>
        {
            template <xyu::size_t... is>
#if __has_builtin(__type_pack_element)
            using type = typename seq_sel_heterog<typename SP::ori, 
                    typename seq_sel_fix<SP::count, is...>::type>::type;
#else
            using type = typename SP::template unpark<__::seq_get<SP>::template get<is>()...>;
#endif

        };
    }
    /**
     * @brief 获取 序列中 指定索引组 的 元素序列
     * @bar 示例
     * @code
     * t_seq_sel˂isequence_c˂2,3,4,5,6˃, 0,2,4˃ == isequence_c˂2,4,6˃
     * @endcode
     */
    template <typename SI, xyu::size_t... indexs>
    using t_seq_sel = typename __::seq_sel<__::seq_park<SI>>::template type<indexs...>;

    namespace __
    {
        // 同构
        template <typename SP, bool = SP::is_homogeneous>
        struct seq_sub
        {
            static constexpr xyu::size_t count = SP::count;
            template <xyu::size_t start, xyu::size_t len>
            using type = typename seq_sel_homog<SP, typename SP::std, make_isequence_t<
                    (start < count) ? start : count,
                    (start + len < start || start + len > count) ? count : start + len
            >>::type;
        };

        // 异构
        template <typename SP>
        struct seq_sub<SP, false>
        {
            static constexpr xyu::size_t count = SP::count;

#if __has_builtin(__type_pack_element)
            template <xyu::size_t start, xyu::size_t len>
            using type = typename seq_sel_heterog<typename SP::ori, make_isequence_t<
                    (start < count) ? start : count,
                    (start + len < start || start + len > count) ? count : start + len
            >>::type;
#else
            template <xyu::size_t start, xyu::size_t len, typename SI, typename = void, auto... les>
            struct seq_sub_impl;
            template <xyu::size_t start, xyu::size_t len, template<auto...>class ST, auto... es, auto... les>
            struct seq_sub_impl<start, len, ST<es...>, t_enable<len == 0 || sizeof...(es) == 0>, les...>
            { using type = ST<les...>; };

            template <xyu::size_t start, xyu::size_t len, template<auto...>class ST, auto... es>
            struct seq_sub_impl<start, len, ST<es...>, t_enable<(len && start && start >= sizeof...(es))>>
            { using type = ST<>; };
        #define XY_seq_sub_impl_gen( n ) \
            template <xyu::size_t start, xyu::size_t len, template<auto...>class ST, XY_PP_TAKE(n, XY_PP_PARAM_INDEX_20( auto e ) ), auto... es> \
            struct seq_sub_impl<start, len, ST<XY_PP_TAKE(n, XY_PP_PARAM_INDEX_20( e ) ), es...>, t_enable<(start == n && len)>> \
            { using type = typename seq_sub_impl<start-n, len, ST<es...>, void>::type; };
        XY_PP_FORWARD( XY_seq_sub_impl_gen, 1, XY_PP_PARAM_INDEX_20() )
        #undef XY_seq_sub_impl_gen
            template <xyu::size_t start, xyu::size_t len, template<auto...>class ST, XY_PP_PARAM_INDEX_20( auto e ), auto... es>
            struct seq_sub_impl<start, len, ST<XY_PP_PARAM_INDEX_20( e ), es...>, t_enable<(start > 20 && len && start < 20 + sizeof...(es))>>
            { using type = typename seq_sub_impl<start - 20, len, ST<es...>, void>::type; };

            template <xyu::size_t start, xyu::size_t len, template<auto...>class ST, auto... es, auto... les>
            struct seq_sub_impl<start, len, ST<es...>, t_enable<(start == 0 && len && len >= sizeof...(es))>, les...>
            { using type = ST<les..., es...>; };
        #define XY_seq_sub_impl_gen( n ) \
            template <xyu::size_t start, xyu::size_t len, template<auto...>class ST, XY_PP_TAKE(n, XY_PP_PARAM_INDEX_20( auto e ) ), auto... es, auto... les> \
            struct seq_sub_impl<start, len, ST<XY_PP_TAKE(n, XY_PP_PARAM_INDEX_20( e ) ), es...>, t_enable<(start == 0 && len == n)>, les...> \
            { using type = ST<les..., XY_PP_TAKE(n, XY_PP_PARAM_INDEX_20( e ) )>; };
        XY_PP_FORWARD( XY_seq_sub_impl_gen, 1, XY_PP_PARAM_INDEX_20() )
        #undef XY_seq_sub_impl_gen
            template <xyu::size_t start, xyu::size_t len, template<auto...>class ST, XY_PP_PARAM_INDEX_20( auto e ), auto... es, auto... les>
            struct seq_sub_impl<start, len, ST<XY_PP_PARAM_INDEX_20( e ), es...>, t_enable<(start == 0 && len > 20 && len < 20 + sizeof...(es))>, les...>
            { using type = typename seq_sub_impl<start, len - 20, ST<es...>, void, les..., XY_PP_PARAM_INDEX_20( e )>::type; };

            template <xyu::size_t start, xyu::size_t len>
            using type = typename seq_sub_impl<
                    start,
                    (start >= count ? 0 : (start + len < start || start + len > count ? count - start : len)),
                    typename SP::ori>::type;
#endif
        };
    }
    /**
     * @brief 获取 序列中 [start, start + len) 范围的子序列
     * @bar 示例
     * @code
     * t_seq_sub˂isequence_c˂2,3,4,5˃, 0,2˃ == isequence_c˂2,3˃
     * @endcode
     */
    template <typename SI, xyu::size_t start, xyu::size_t len = xyu::size_t(-1)>
    using t_seq_sub = typename __::seq_sub<__::seq_park<SI>>::template type<start, len>;

    /* 顺序 */

    namespace __
    {
        template <xyu::size_t count>
        struct seq_rev_index
        {
            template <typename SI> struct seq_rev_index_impl;
            template <xyu::size_t... is>
            struct seq_rev_index_impl<isequence_c<is...>>
            { using type = isequence_c<(count - 1 - is)...>; };

            using type = typename seq_rev_index_impl<make_isequence_t<count>>::type;
        };

        // 同构
        template <typename SP, bool = SP::is_homogeneous>
        struct seq_rev { using type = typename seq_sel_homog<SP, typename SP::std, typename seq_rev_index<SP::count>::type>::type; };

        // 异构
        template <typename SP>
        struct seq_rev<SP, false>
        {
#if __has_builtin(__type_pack_element)
            using type = typename seq_sel_heterog<typename SP::ori, typename seq_rev_index<SP::count>::type>::type;
#else
            template <typename SI, auto... les> struct seq_rev_impl;
            template <template<auto...>class ST, auto... les>
            struct seq_rev_impl<ST<>, les...> { using type = ST<les...>; };
            template <template<auto...>class ST, auto e, auto... es, auto... les>
            struct seq_rev_impl<ST<e, es...>, les...>
            { using type = typename seq_rev_impl<ST<es...>, e, les...>::type; };

            using type = typename seq_rev_impl<typename SP::ori>::type;
#endif
        };
    }
    /// 反转序列
    template <typename SI>
    using t_seq_rev = typename __::seq_rev<__::seq_park<SI>>::type;

    namespace __
    {
        // 升序检测
        template <typename SI> struct seq_is_sorted;
        template <typename T, T... es>
        struct seq_is_sorted<sequence_c<T, es...>>
        {
            static constexpr T arr[sizeof...(es)] = {es...};
            static constexpr bool value = []
            {
                for (xyu::size_t i = 1; i < sizeof...(es); ++i)
                    if (arr[i-1] > arr[i]) return false;
                return true;
            }();
        };

        // 插入排序 (编译期对递归、分支的执行较差)
        template <typename T, xyu::size_t count>
        constexpr auto insertionSort(const cArray<T,count>& arr)
        {
            auto newarr = arr;
            for (xyu::size_t i = 1; i < count; i++)
            {
                T key = newarr.data[i];
                xyu::size_t j = i - 1;
                while (j < count && newarr.data[j] > key)
                {
                    newarr.data[j + 1] = newarr.data[j];
                    j--;
                }
                newarr.data[j + 1] = key;
            }
            return newarr;
        }

        template <typename SP, typename SI, bool ase, bool is_sorted = seq_is_sorted<SI>::value, typename = void>
        struct seq_sort { using type = typename SP::ori; };
        template <typename SP, typename SI, bool is_sorted>
        struct seq_sort<SP, SI, false, is_sorted> { using type = t_seq_rev<typename seq_sort<SP, SI, true, is_sorted>::type>; };
        template <typename SP, typename T, T... es>
        struct seq_sort<SP, sequence_c<T,es...>, true, false, t_enable<sizeof...(es) >= 2>>
        {
            static constexpr cArray<T,sizeof...(es)> arr = {es...};
            static constexpr auto newarr = insertionSort(arr);

            template <xyu::size_t... is>
            static typename SP::template unpark<newarr.data[is]...> seq_sort_impl(isequence_c<is...>);

            using type = decltype(seq_sort_impl(make_isequence<sizeof...(es)>()));
        };

        template <typename SP, bool ase>
        struct seq_sort_check
        {
            static_assert(SP::is_homogeneous, "sort only support homogeneous sequence");
            using type = typename seq_sort<SP, typename SP::std, ase>::type;
        };
    }
    /**
     * @brief 对序列进行排序 (默认升序)
     * @note 不支持异构序列
     */
    template <typename SI, bool ase = true>
    using t_seq_sort = typename __::seq_sort_check<__::seq_park<SI>, ase>::type;

    /* 增加 */

    namespace __
    {
        template <typename SI, auto... eles> struct seq_app;
        template <template<typename T, T...>class ST, typename T, T... es, auto... eles>
        struct seq_app<ST<T, es...>, eles...> { using type = ST<T, es..., eles...>; };
        template <template<auto...>class ST, auto... es, auto... eles>
        struct seq_app<ST<es...>, eles...> { using type = ST<es..., eles...>; };
    }
    /// 在序列的 末尾 追加 eles...
    template <typename SI, auto... eles>
    using t_seq_app = typename __::seq_app<t_rem_cr<SI>, eles...>::type;

    namespace __
    {
        template <typename LSI, typename RSI, auto... eles>
        struct seq_cat { static_assert(t_is_void<LSI>, "unsupported sequence type"); };
        template <template<typename T, T...>class ST, typename T, T... les, T... res, auto... eles>
        struct seq_cat<ST<T,les...>, ST<T,res...>, eles...> { using type = ST<T, les..., eles..., res...>; };
        template <template<auto...>class ST1, template<typename T, T...>class ST2, typename T, auto... les, T... res, auto... eles>
        struct seq_cat<ST1<les...>, ST2<T,res...>, eles...> { using type = ST1<les..., eles..., res...>; };
        template <template<typename T, T...>class ST1, template<auto...>class ST2, typename T, T... les, auto... res, auto... eles>
        struct seq_cat<ST1<T,les...>, ST2<res...>, eles...> { using type = ST2<les..., eles..., res...>; };
        template <template<auto...>class ST1, template<auto...>class ST2, auto... les, auto... res, auto... eles>
        struct seq_cat<ST1<les...>, ST2<res...>, eles...> { using type = ST1<les..., eles..., res...>; };
    }
    /**
     * @brief 拼接两个序列并用 eles 连接 (les..., eles..., res...)
     * @details 若为同构序列，必须为相同类型；如果存在异构序列，则最终结果为异构类型
     */
    template <typename LSI, typename RSI, auto... eles>
    using t_seq_cat = typename __::seq_cat<t_rem_cr<LSI>, t_rem_cr<RSI>, eles...>::type;

    /// 在序列的 index 索引之前插入 eles...
    template <typename SI, xyu::size_t index, auto... eles>
    using t_seq_ins = t_seq_cat<t_seq_sub<SI, 0, index>, t_seq_sub<SI, index>, eles...>;

    /* 删除 */

    /// 删除 SI 中从 start 索引开始的 len 个元素
    template <typename SI, xyu::size_t start, xyu::size_t len = 1>
    using t_seq_del = t_seq_cat<t_seq_sub<SI, 0, start>, t_seq_sub<SI, start+len>>;

    namespace __
    {
        // 过滤索引
        template <xyu::size_t... ois, xyu::size_t... fis>
        constexpr auto seq_filter_index(isequence_c<ois...>, isequence_c<fis...>)
        {
            cList<xyu::size_t, sizeof...(ois)> target;
            xyu::size_t Ori[sizeof...(ois)] = {ois...}, Filt[sizeof...(fis)] = {fis...};
            xyu::size_t oi = 0, fi = 0, oe = sizeof...(ois), fe = sizeof...(fis);
            while (oi < oe && fi < fe)
            {
                if (Ori[oi] == Filt[fi]) ++oi;
                else if (Ori[oi] < Filt[fi])
                {
                    target.data[target.count++] = Ori[oi];
                    ++oi;
                }
                else ++fi;
            }
            while (oi < oe) target.data[target.count++] = Ori[oi++];
            return target;
        }

        template <typename SP, xyu::size_t... indexs>
        struct seq_ers
        {
            static constexpr auto ids = seq_filter_index(make_isequence<SP::count>(),
                                                         t_seq_sort<isequence_c<indexs...>>{});

            template <xyu::size_t... is>
            static t_seq_sel<typename SP::ori, ids.data[is]...> seq_ers_impl(isequence_c<is...>);
            using type = decltype(seq_ers_impl(make_isequence<ids.count>()));
        };
    }
    /// 删除 SI 中指定的 索引组
    template <typename SI, xyu::size_t... indexs>
    using t_seq_ers = typename __::seq_ers<__::seq_park<SI>, indexs...>::type;

    namespace __
    {
        // 同构
        template <typename SI> struct seq_rem_homog;
        template <typename T, T... es>
        struct seq_rem_homog<sequence_c<T, es...>>
        {
            template <T target, xyu::size_t limit, xyu::size_t start, xyu::size_t end>
            static constexpr auto lis = []
            {
                T arr[sizeof...(es)] = {es...};
                cList<xyu::size_t, sizeof...(es)> lis;
                xyu::size_t index = start;
                for (xyu::size_t count = 0; index < end && count < limit; index++)
                {
                    if (arr[index] != target) lis.data[lis.count++] = index;
                    else ++count;
                }
                for (; index < sizeof...(es); index++) lis.data[lis.count++] = index;
                return lis;
            }();
        };

        // 异构
        template <typename SI> struct seq_rem_heterog;
        template <template<auto...>class ST, auto... es>
        struct seq_rem_heterog<ST<es...>>
        {
            template <auto target, xyu::size_t limit, xyu::size_t start, xyu::size_t end>
            static constexpr auto lis = []
            {
                cList<xyu::size_t, sizeof...(es)> lis;
                xyu::size_t index = 0, count = 0;
                (((index >= start && index < end && count < limit &&
                  t_is_same<decltype(es), decltype(target)> && es == target ?
                  count++ : lis.data[lis.count++] = index), index++), ...);
                return lis;
            }();
        };

        template <typename SP, auto target, xyu::size_t limit, xyu::size_t start, xyu::size_t end, typename = void>
        struct seq_rem_check
        {
            using impl = t_cond<SP::is_homogeneous, seq_rem_homog<typename SP::std>, seq_rem_heterog<typename SP::ori>>;
            static constexpr auto& lis = impl::template lis<target, limit, start, end>;

            template <xyu::size_t... is>
            static t_seq_sel<typename SP::ori, lis.data[is]...> seq_rem_impl(isequence_c<is...>);
            using type = decltype(seq_rem_impl(make_isequence<lis.count>()));
        };
        template <typename SP, auto target, xyu::size_t limit, xyu::size_t start, xyu::size_t end>
        struct seq_rem_check<SP, target, limit, start, end, t_enable<(limit == 0 || start > SP::count || start >= end)>>
        { using type = typename SP::ori; };
    }
    /// 删除 SI 的子序列 [start, end) 中等于 target 的元素，最多删除 limit 个
    template <typename SI, auto target, xyu::size_t limit = static_cast<xyu::size_t>(-1), xyu::size_t start = 0, xyu::size_t end = xyu::size_t(-1)>
    using t_seq_rem = typename __::seq_rem_check<__::seq_park<SI>, target, limit, start, end>::type;

    /* 查找 */

    namespace __
    {
        template <typename SI> struct seq_find_homog;
        template <typename T, T... es>
        struct seq_find_homog<sequence_c<T, es...>>
        {
            static constexpr xyu::size_t get(T target, xyu::size_t start, xyu::size_t end)
            {
                T arr[sizeof...(es)] = {es...};
                for (xyu::size_t i = start; i < end; i++)
                    if (arr[i] == target) return i;
                return xyu::size_t(-1);
            };
        };

        template <typename SI> struct seq_find_heterog;
        template <template<auto...>class ST, auto... es>
        struct seq_find_heterog<ST<es...>>
        {
            template <auto target>
            static constexpr xyu::size_t get(xyu::size_t start, xyu::size_t end)
            {
                xyu::size_t ret = xyu::size_t(-1), cur = 0;
                ((cur >= start && cur < end && ret == -1 && t_is_same<decltype(es), decltype(target)> && es == target ? ret = cur : cur++), ...);
                return ret;
            }
        };

        // 同构
        template <auto target, xyu::size_t start, xyu::size_t end, typename SP, bool = SP::is_homogeneous>
        struct seq_find
        { static constexpr xyu::size_t value = seq_find_homog<typename SP::std>::get(target, start, end); };

        // 异构
        template <auto target, xyu::size_t start, xyu::size_t end, typename SP>
        struct seq_find<target, start, end, SP, false>
        { static constexpr xyu::size_t value = seq_find_heterog<typename SP::ori>::template get<target>(start, end); };

        template <auto target, xyu::size_t start, xyu::size_t end, typename SI>
        struct seq_find_check
        {
            static constexpr xyu::size_t value = []
            {
                constexpr xyu::size_t count = t_seq_count<SI>;
                if constexpr (start >= count || start >= end) return xyu::size_t(-1);
                else return seq_find<target, start, (end > count ? count : end), seq_park<SI>>::value;
            }();
        };
    }
    /// 在 SI 的子序列 [start, end) 中查找 target，并返回其首次出现的索引，不存在则返回 -1
    template <typename SI, auto target, xyu::size_t start = 0, xyu::size_t end = xyu::size_t(-1)>
    constexpr xyu::size_t t_seq_find = __::seq_find_check<target, start, end, t_rem_cr<SI>>::value;

    /* 修改 */

    namespace __
    {
        template <typename SI, xyu::size_t index, auto change>
        struct seq_set_check
        {
            static_assert(index < t_seq_count<SI>, "index out of range");
            using type = t_seq_cat<t_seq_sub<SI, 0, index>, t_seq_sub<SI, index+1>, change>;
        };
    }
    /**
     * @brief 将序列中 index 索引处的元素替换为 change
     * @bar 示例
     * @code
     * t_seq_set˂isequence_c˂2, 3, 4˃, 1, 0˃ == isequence_c˂2, 0, 4˃
     * @endcode
     */
    template <typename SI, xyu::size_t index, auto change>
    using t_seq_set = typename __::seq_set_check<t_rem_cr<SI>, index, change>::type;

    namespace __
    {
        template <typename SI, typename Transform> struct t_seq_map;
        template <template<typename T,T...>class ST, typename T, T... es, typename Transform>
        struct t_seq_map<ST<T, es...>, Transform> { using type = ST<T, Transform{}(es)...>; };
        template <template<auto...>class ST, auto... es, typename Transform>
        struct t_seq_map<ST<es...>, Transform> { using type = ST<Transform{}(es)...>; };
    }
    /**
     * @brief 将序列中的每个元素应用 Transform 变换
     * @details Transform 必须有 constexpr 的 operator() 成员函数
     */
    template <typename SI, typename Transform>
    using t_seq_map = typename __::t_seq_map<t_rem_cr<SI>, Transform>::type;

    namespace __
    {
        // 同构
        template <typename SI> struct seq_uniq_homog;
        template <typename T, T... es>
        struct seq_uniq_homog<sequence_c<T, es...>>
        {
            constexpr static auto lis = []
            {
                cList<T, sizeof...(es)> ret, set;
                T arr[sizeof...(es)] = {es...};
                for (xyu::size_t i = 0; i < sizeof...(es); i++)
                {
                    T& e = arr[i];
                    // 查找重复或插入位置
                    xyu::size_t left = 0, right = set.count;
                    bool found = false;
                    while (left < right)
                    {
                        xyu::size_t mid = left + (right - left) / 2;

                        if (set.data[mid] == e) { found = true; break; }
                        if (set.data[mid] < e) left = mid + 1;
                        else right = mid;
                    }
                    // 如果未找到，则插入到集合和结果中
                    if (!found)
                    {
                        xyu::size_t ins = left;
                        for (xyu::size_t j = set.count; j > ins; j--) set.data[j] = set.data[j - 1];
                        set.data[ins] = e;
                        set.count++;

                        ret.data[ret.count] = e;
                        ret.count++;
                    }
                }
                return ret;
            }();
        };

        template <typename SP, bool = SP::is_homogeneous>
        struct seq_uniq
        {
            static constexpr auto& lis = seq_uniq_homog<typename SP::std>::lis;

            template <xyu::size_t... is>
            static typename SP::template unpark<lis.data[is]...> seq_uniq_impl(isequence_c<is...>);
            using type = decltype(seq_uniq_impl(make_isequence<lis.count>()));
        };

        // 异构
        template <typename SP>
        struct seq_uniq<SP, false>
        {
            template <typename SI, typename = void, auto... les> struct seq_uniq_impl;
            template <template<auto...>class ST, auto... les>
            struct seq_uniq_impl<ST<>, void, les...> { using type = ST<les...>; };
            template <template<auto...>class ST, auto e, auto... es, auto... les>
            struct seq_uniq_impl<ST<e, es...>, t_enable<t_seq_find<ST<les...>, e> == -1>, les...>
            { using type = typename seq_uniq_impl<ST<es...>, void, les..., e>::type; };
            template <template<auto...>class ST, auto e, auto... es, auto... les>
            struct seq_uniq_impl<ST<e, es...>, t_enable<t_seq_find<ST<les...>, e> != -1>, les...>
            { using type = typename seq_uniq_impl<ST<es...>, void, les...>::type; };

            using type = typename seq_uniq_impl<typename SP::ori>::type;
        };
    }
    /// 序列去重
    template <typename SI>
    using t_seq_uniq = typename __::seq_uniq<__::seq_park<SI>>::type;
}
