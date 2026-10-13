#pragma once

/**
 * @file    xycoll/__/node.h
 * @brief   节点类
 * @date    2026-06-22
 */

#include "../_.h"
#include "../../xystring/fmtimpl.h"

/// 辅助节点
namespace xylu::xycoll::__
{
    template <typename Key, typename Val>
    struct KVNode
    {
        const Key key;
        Val val;

    private:
        static constexpr bool a1 = xyu::t_is_aggregate<Key>;
        static constexpr bool a2 = xyu::t_is_aggregate<Val>;

        template <typename K, typename... V>
        static constexpr bool can_init = xyu::t_can_init<Key,K> && xyu::t_can_init<Val,V...>;
        template <typename K, typename... V>
        static constexpr bool can_noexcept_init = xyu::t_can_init<Key,K> && xyu::t_can_init<Val,V...>;

    public:
        template <typename K, typename... V, xyu::t_enable<can_init<K,V...> && (!a1&&!a2), int> = 1>
        constexpr KVNode(K&& k, V&&... vs) noexcept(can_noexcept_init<K,V...>)  : key(xyu::forward<K>(k)), val(xyu::forward<V>(vs)...) {}
        template <typename K, typename... V, xyu::t_enable<can_init<K,V...> && (a1&&!a2), int> = 2>
        constexpr KVNode(K&& k, V&&... vs) noexcept(can_noexcept_init<K,V...>)  : key{xyu::forward<K>(k)}, val(xyu::forward<V>(vs)...) {}
        template <typename K, typename... V, xyu::t_enable<can_init<K,V...> && (!a1&&a2), int> = 3>
        constexpr KVNode(K&& k, V&&... vs) noexcept(can_noexcept_init<K,V...>)  : key(xyu::forward<K>(k)), val{xyu::forward<V>(vs)...} {}
        template <typename K, typename... V, xyu::t_enable<can_init<K,V...> && (a1&&a2), int> = 4>
        constexpr KVNode(K&& k, V&&... vs) noexcept(can_noexcept_init<K,V...>)  : key{xyu::forward<K>(k)}, val{xyu::forward<V>(vs)...} {}

    public:
        template <typename Rg,
                  typename K = decltype(xyu::t_val<xyu::t_access<Rg>>().key),
                  typename V = decltype(xyu::t_val<xyu::t_access<Rg>>().val)>
        static constexpr bool can_kv_range = !xyu::t_can_init<Key,Rg> &&
            xyu::t_can_init<Key,K> && xyu::t_can_init<Val,V>;

        template <typename kRg, typename vRg>
        static constexpr bool can_kv_2range = !xyu::t_can_init<Key,kRg> &&
            xyu::t_can_init<Key,xyu::t_access<kRg>> &&
            xyu::t_can_init<Val,xyu::t_access<vRg>>;

        template <typename Rg>
        constexpr static decltype(auto) get_key_ne() noexcept { return (xyu::t_val<xyu::t_noexcept_access<Rg>>().key); }
        template <typename Rg>
        constexpr static decltype(auto) get_val_ne() noexcept { return (xyu::t_val<xyu::t_noexcept_access<Rg>>().val); }
        template <typename Rg>
        constexpr static decltype(auto) get_ne() noexcept { return xyu::t_val<xyu::t_noexcept_access<Rg>>(); }
    };

    template <typename Key>
    struct KVNode<Key, void>
    {
        const Key key;

        template <typename... K, xyu::t_enable<xyu::t_can_init<Key,K...> && !xyu::t_is_aggregate<Key>, int> = 1>
        constexpr KVNode(K&&... ks) noexcept(xyu::t_can_nothrow_init<Key,K...>) : key(xyu::forward<K>(ks)...) {}
        template <typename... K, xyu::t_enable<xyu::t_can_init<Key,K...> && xyu::t_is_aggregate<Key>, int> = 2>
        constexpr KVNode(K&&... ks) noexcept(xyu::t_can_nothrow_init<Key,K...>) : key{xyu::forward<K>(ks)...} {}
    };

    template <typename Key, typename Val>
    KVNode(Key, Val) -> KVNode<Key, Val>;
}

/// 格式化
namespace xylu::xystring
{
    template <typename Key, typename Val>
    struct FormatImpl<xycoll::__::KVNode<Key, Val>>
    {
        using T = xycoll::__::KVNode<Key, Val>;

        // 强制运行时解析类型
        static constexpr bool runtime = FormatCore::is_runtime<Key> || FormatCore::is_runtime<Val>;

        /// 编译期解析
        template <bool R = runtime, typename = xyu::t_enable<!R>>
        constexpr static xyu::size_t parse_compile(StringView represent, StringView expand) noexcept
        {
            auto repex = parse_repex(represent, expand);
            return parse_forward(repex,
                FormatCore::call_parse_compile<Key>,
                FormatCore::call_parse_compile<Val>);
        }

        /// 运行期解析
        static xyu::size_t parse_runtime(const T& node, StringView represent, StringView expand) noexcept(noexcept(FormatCore::call_parse_runtime<Key>({},{},{},{})) && noexcept(FormatCore::call_parse_runtime<Val>({},{},{},{})))
        {
            auto repex = parse_repex(represent, expand);
            return parse_forward(repex,
                [&node](auto&&...args){return FormatCore::call_parse_runtime<Key>(&node.key, args...);},
                [&node](auto&&...args){return FormatCore::call_parse_runtime<Val>(&node.val, args...);});
        }

        /// 运行期解析剩余
        static xyu::size_t parse_remain(const T& node, StringView represent, StringView expand) noexcept(noexcept(FormatCore::call_parse_remain<Key>({},{},{},{})) && noexcept(FormatCore::call_parse_remain<Val>({},{},{},{})))
        {
            auto repex = parse_repex(represent, expand);
            return parse_forward(repex,
                [&node](auto&&...args)
                {
                    if constexpr (!FormatCore::is_runtime<Key>) return FormatCore::call_parse_compile<Key>(args...);
                    else return FormatCore::call_parse_remain<Key>(&node.key, args...);
                },
                [&node](auto&&...args)
                {
                    if constexpr (!FormatCore::is_runtime<Val>) return FormatCore::call_parse_compile<Val>(args...);
                    else return FormatCore::call_parse_remain<Val>(&node.val, args...);
                });
        }

        /// 格式化
        template <typename Stream>
        static void format(Stream& out, const T& node, StringView represent, StringView expand) noexcept(noexcept(FormatCore::call_format<Key>(out,{},{},{},{})) && noexcept(FormatCore::call_format<Val>(out,{},{},{},{})))
        {
            auto repex = parse_repex(represent, expand);
            FormatCore::call_format<Key>(out, &node.key, Format_Layout{}, repex.rep1, repex.ex1);
            out << ':';
            FormatCore::call_format<Val>(out, &node.val, Format_Layout{}, repex.rep2, repex.ex2);
        }

    private:
        template <typename Func1, typename Func2, typename RepEx, typename... Args>
        constexpr static xyu::size_t parse_forward(RepEx& repex, Func1&& f1, Func2&& f2, Args&&... args)
        {
            xyu::size_t count = repex.split.size();
            xyu::size_t add = f1(xyu::forward<Args>(args)..., Format_Layout{}, repex.rep1, repex.ex1);
            if (XY_UNLIKELY(add > FormatCore::limit)) return -1;
            count += add;
            add = f2(xyu::forward<Args>(args)..., Format_Layout{}, repex.rep2, repex.ex2);
            if (XY_UNLIKELY(add > FormatCore::limit)) return -1;
            count += add;
            return count;
        }

        // 解析所有格式符
        constexpr static auto parse_repex(StringView rep, StringView ex) noexcept
        {
            struct Ret { StringView split, rep1, rep2, ex1, ex2; } ret;
            if (!rep.empty())
            {
                auto sl = parse_split(rep);
                ret.rep1 = sl.split;
                ret.rep2 = sl.last;
            }
            {
                auto sl = parse_split(ex);
                ret.split = sl.split;
                if (!sl.last.empty())
                {
                    sl = parse_split(sl.last);
                    ret.ex1 = sl.split;
                    ret.ex2 = sl.last;
                }
            }
            return ret;
        }

        // 通过 , 将字符串分开，() 内的跳过
        constexpr static auto parse_split(StringView re) noexcept
        {
            auto sl = FormatImpl<int>::parse_split(re);
            if (re.empty()) sl.split = ":";
            return sl;
        }
    };

    template <typename Key>
    struct FormatImpl<xycoll::__::KVNode<Key, void>> :
        FormatImpl<xyu::typelist_c<xycoll::__::KVNode<Key, void>, Key>>
    {
        static auto transform(const xycoll::__::KVNode<Key, void>& node) noexcept { return node.key; }
    };
}
