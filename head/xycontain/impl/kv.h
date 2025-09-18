#pragma clang diagnostic push
#pragma ide diagnostic ignored "OCUnusedGlobalDeclarationInspection"
#pragma ide diagnostic ignored "bugprone-forwarding-reference-overload"
#pragma ide diagnostic ignored "OCUnusedTemplateParameterInspection"
#pragma once

#include "../../../link/tuple"

// 键值对辅助工具

namespace xylu::xycontain::__
{
    /// 检测范围的迭代器类型
    template <typename Rg, typename Key, typename = xyu::t_enable<xyu::t_is_same_nocvref<Key, decltype(*(xyu::t_val<Rg>().begin()))>>>
    xyu::int_c<-1> range_kv_type(char&&);
    template <typename Rg, typename Key, typename = decltype((*(xyu::t_val<Rg>().begin())).key, (*(xyu::t_val<Rg>().begin())).val)>
    xyu::int_c<2> range_kv_type(const char&&);
    template <typename Rg, typename Key, typename = decltype((*(xyu::t_val<Rg>().begin())).key)>
    xyu::int_c<-2> range_kv_type(const char&);
    template <typename Rg, typename Key, typename = decltype((*(xyu::t_val<Rg>().begin())).template get<0>(), (*(xyu::t_val<Rg>().begin())).template get<1>())>
    xyu::int_c<3> range_kv_type(int&&);
    template <typename Rg, typename Key, typename = decltype((*(xyu::t_val<Rg>().begin())).template get<0>())>
    xyu::int_c<-3> range_kv_type(const int&&);
    template <typename Rg, typename Key, typename T = xyu::t_remove_refer<decltype(*(xyu::t_val<Rg>().begin()))>, typename = xyu::t_enable<xyu::t_is_aggregate<T> && xyu::t_get_member_count<T> == 2>>
    xyu::int_c<4> range_kv_type(const int&);
    template <typename Rg, typename Key, typename T = xyu::t_remove_refer<decltype(*(xyu::t_val<Rg>().begin()))>, typename = xyu::t_enable<xyu::t_is_aggregate<T> && xyu::t_get_member_count<T> == 1>>
    xyu::int_c<-4> range_kv_type(double&&);
    template <typename Rg, typename Key, typename T = xyu::t_remove_refer<decltype(*(xyu::t_val<Rg>().begin()))>, typename = xyu::t_enable<std::tuple_size<T>::value == 2>>
    xyu::int_c<4> range_kv_type(const double&&);
    template <typename Rg, typename Key, typename T = xyu::t_remove_refer<decltype(*(xyu::t_val<Rg>().begin()))>, typename = xyu::t_enable<std::tuple_size<T>::value == 1>>
    xyu::int_c<-4> range_kv_type(const double&);
    template <typename Rg, typename Key>
    xyu::int_c<0> range_kv_type(...);
    template <typename Rg, typename Key = void>
    constexpr int get_range_kv_type = decltype(range_kv_type<Rg, Key>('0'))::value;

    /// 键值数据
    template <typename Key, typename Value>
    struct KVData
    {
        Key key;        // 键
        Value val;      // 值

        constexpr KVData() = default;

        template <typename K, typename... Args, typename Test = Value, xyu::t_enable<xyu::t_is_aggregate<Test>, bool> = true>
        constexpr explicit KVData(K&& key, Args&&... args) noexcept(xyu::t_can_nothrow_cpconstr<Key> && xyu::t_can_nothrow_listinit<Value, Args...>)
                : key{xyu::forward<K>(key)}, val{xyu::forward<Args>(args)...} {}

        template <typename K, typename... Args, typename Test = Value, xyu::t_enable<!xyu::t_is_aggregate<Test>, bool> = false>
        constexpr explicit KVData(K&& key, Args&&... args) noexcept(xyu::t_can_nothrow_cpconstr<Key> && xyu::t_can_nothrow_construct<Value, Args...>)
                : key{xyu::forward<K>(key)}, val(xyu::forward<Args>(args)...) {}
    };
    //仅健特化
    template <typename Key>
    struct KVData<Key, void>
    {
        Key key;        // 键

        constexpr KVData() = default;

        template <typename K>
        constexpr explicit KVData(K&& key) noexcept(xyu::t_can_nothrow_construct<Key, K>) : key{xyu::forward<K>(key)} {}
    };
}

namespace xylu::xystring
{
    /// 哈希表内部键类型
    template <typename Key>
    struct Formatter<xylu::xycontain::__::KVData<Key, void>>
            : Formatter<xyu::typelist_c<xylu::xycontain::__::KVData<Key, void>, Key>>
    {
        // 类型转发
        static const Key& transform(const xylu::xycontain::__::KVData<Key, void>& data) noexcept
        { return data.key; }
    };

    /// 哈希表内部键值类型
    template <typename Key, typename Value>
    struct Formatter<xylu::xycontain::__::KVData<Key, Value>>
    {
        using T = xylu::xycontain::__::KVData<Key, Value>;

        // 运行时解析
        static constexpr bool runtime = __::is_formatter_runtime<Key> || __::is_formatter_runtime<Value>;

        /// 预解析
        static constexpr xyu::size_t prepare(const StringView& pattern, const StringView& expand) noexcept
        {
            return 1 + __::call_formatter_prepare<Key>(Format_Layout{}, pattern, expand)
                   + __::call_formatter_prepare<Value>(Format_Layout{}, pattern, expand);
        }

        /// 解析
        static xyu::size_t parse(const T& kv, const StringView& pattern, const StringView& expand)
        {
            return 1 + __::call_formatter_parse<Key>(&kv.key, Format_Layout{}, pattern, expand)
                   + __::call_formatter_parse<Value>(&kv.val, Format_Layout{}, pattern, expand);
        }

        /// 运行期预解析
        static constexpr xyu::size_t preparse(const T& kv, const StringView& pattern, const StringView& expand) noexcept
        {
            return 1 + __::call_formatter_preparse<Key>(&kv.key, Format_Layout{}, pattern, expand)
                   + __::call_formatter_preparse<Value>(&kv.val, Format_Layout{}, pattern, expand);
        }

        /// 格式化
        template <typename Stream>
        static void format(Stream& out, const T& kv, const StringView& pattern, const StringView& expand)
        {
            __::call_formatter_format<Key>(out, &kv.key, Format_Layout{}, pattern, expand);
            out << ':';
            __::call_formatter_format<Value>(out, &kv.val, Format_Layout{}, pattern, expand);
        }
    };
}

#pragma clang diagnostic pop