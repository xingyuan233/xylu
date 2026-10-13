#pragma once

/**
 * @file    xytraits/tmpl.h
 * @brief   模板高阶工具：模板的一等公民化、延迟实例化与参数提取
 * @date    2025-12-20
 */

#include "./tplis.h"

/** 打包与解包 */
namespace xylu::xytraits
{
    namespace __
    {
        template <template<typename...>class TT, typename... ExArgs>
        struct pack_tmpl
        {
            using extra = typelist_c<ExArgs...>;

            template <typename... Args>
            using instance = TT<Args...>;
        };
    }
    /**
     * @brief 将 模板 打包 成一个类型
     * @tparam TT       打包的模板
     * @tparam ExArgs   额外参数，可通过 extra 访问
     */
    template <template<typename...>class TT, typename... ExArgs>
    using t_pack_tmpl = __::pack_tmpl<TT, ExArgs...>;

    namespace __
    {
        template <typename T>
        struct pack_instance { static_assert(t_is_void<T>, "not a template instance"); };
        template <template<typename...>class TT, typename... Args>
        struct pack_instance<TT<Args...>> { using type = pack_tmpl<TT, Args...>; };
        template <template<typename...>class TT, typename... ExArgs>
        struct pack_instance<pack_tmpl<TT, ExArgs...>> { using type = pack_tmpl<TT, ExArgs...>; };
    }
    /**
     * @brief 从 模板实例 中 提取并打包 原始模板
     * @details 同时会保留原始参数，可通过 extra 访问
     * @note 对于展开后会丢失信息的别名模板，此工具无法正常工作
     */
    template <typename TI>
    using t_pack_instance = typename __::pack_instance<TI>::type;

    namespace __
    {
        template <typename TI, typename... Args>
        struct unpack_tmpl { using type = TI; };
        template <template<typename...>class TT, typename... IArgs, typename... Args>
        struct unpack_tmpl<TT<IArgs...>, Args...> { using type = TT<Args...>; };
        template <template<typename...>class TT, typename... IArgs, typename... Args>
        struct unpack_tmpl<TT<IArgs...>, typelist_c<Args...>> : unpack_tmpl<pack_tmpl<TT>, typelist_c<Args...>> {};
        template <template<typename...>class TT, typename... ExArgs, typename... Args>
        struct unpack_tmpl<pack_tmpl<TT, ExArgs...>, Args...> { using type = TT<Args...>; };
        template <template<typename...>class TT, typename... ExArgs, typename... Args>
        struct unpack_tmpl<pack_tmpl<TT, ExArgs...>, typelist_c<Args...>>
        {
            template <template<typename...>class Tm, typename... Ags>
            static Tm<Ags...> test(int);
            template <template<typename...>class Tm, typename... Ags>
            static Tm<typelist_c<Ags...>> test(void*);
            using type = decltype(test<TT, Args...>(0));
        };
    }
    /**
     * @brief 将打包后的模板 用新的参数包 解包并创建新的实例
     * @tparam TI   打包后的模板
     * @tparam Args 用于创建新实例的参数包，若仅提供一个 typelist_c，则先尝试用其包含的参数解包，失败则直接作为参数
     * @bar 示例
     * @code
     * t_unpack_tmpl˂t_pack_tmpl˂xyu::Tuple˃, int, char˃ == xyu::Tuple˂int, char˃
     * t_unpack_tmpl˂t_pack_tmpl˂xyu::Tuple˃, typelist_c˂int, char˃ == xyu::Tuple˂int, char˃
     * @endcode
     */
    template <typename TI, typename... Args>
    using t_unpack_tmpl = typename __::unpack_tmpl<TI, Args...>::type;

    namespace __
    {
        template <typename TI, template<typename...>class nTT>
        struct unpack_args { using type = nTT<>; };
        template <template<typename...>class TT, typename... Args, template<typename...>class nTT>
        struct unpack_args<TT<Args...>, nTT> { using type = nTT<Args...>; };
        template <template<typename...>class TT, typename... ExArgs, template<typename...>class nTT>
        struct unpack_args<pack_tmpl<TT, ExArgs...>, nTT> { using type = nTT<ExArgs...>; };
    }
    /**
     * @brief 将打包模板的额外参数 通过新模板解包并创建实例
     * @tparam TI   打包后的模板
     * @tparam nTT  用于创建新实例的模板
     * @bar 示例
     * @code
     * t_unpack_args˂t_pack_instance˂xyu::Tuple˂int˃˃, xyu::Vector˃ == xyu::Vector˂int˃
     * @endcode
     */
    template <typename TI, template<typename...>class nTT>
    using t_unpack_args = typename __::unpack_args<TI, nTT>::type;
}

/** 实例化 */
namespace xylu::xytraits
{
    namespace __
    {
        template <typename, typename if_fail_t, template<typename...>class TT, typename... Args>
        struct tmpl_instance_or :false_c { using type = if_fail_t; };
        template <typename if_fail_t, template<typename...>class TT, typename... Args>
        struct tmpl_instance_or<t_void<TT<Args...>>, if_fail_t, TT, Args...> : true_c { using type = TT<Args...>; };
    }
    /// 检查模板 TT 是否能用参数 Args... 成功实例化
    template <template<typename...>class TT, typename... Args>
    constexpr bool t_can_instance = __::tmpl_instance_or<void, false_c, TT, Args...>::value;

    /**
     * @brief 尝试用 Args... 实例化模板 TT
     * @details 如果实例化成功，则返回 TT˂Args...˃；如果失败，则返回 if_fail_t
     */
    template <typename if_fail_t, template<typename...>class TT, typename... Args>
    using t_instance_or = typename __::tmpl_instance_or<void, if_fail_t, TT, Args...>::type;
}

/** 静态测试 */
namespace xylu::xytraits
{
    /**
     * @brief 万能模板，可转换为任何类型
     * @attention 仅供测试
     */
    struct t_universal
    {
        template <typename T> operator T() const;
    };

    namespace __
    {
        template <typename T, typename = void, typename... Args>
        struct member_count_test : size_c<sizeof...(Args) - 1> {};
        template <typename T, typename... Args>
        struct member_count_test<T, t_void<decltype(T{{t_val<Args>()}...})>, Args...>
        : size_c<member_count_test<T, void, Args..., t_universal>::value> {};
    }
    /**
     * @brief 获取模板 T 的成员数量
     * @attention 仅限聚合类型
     */
    template <typename T>
    constexpr xyu::size_t t_get_member_count = __::member_count_test<T>::value;
}
