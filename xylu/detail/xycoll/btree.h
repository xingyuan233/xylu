#pragma once

/**
 * @file    xycoll/btree.h
 * @brief   B树
 * @date    2026-06-26
 */

#include "./__/tool.h"
#include "./__/btree.h"

/// B树映射
namespace xylu::xycoll
{
    /**
     * @brief B树映射
     * @tparam Key  键类型
     * @tparam Val  值类型
     * @tparam M    阶数 (每个节点最多有 M-1 个键值，最多有 M 个孩子)
     * @tparam cross_thread  是否允许跨线程内存管理。
     *         - false（默认）: 使用线程局部内存池，性能更好；
     *         - true : 使用全局内存分配，对象可跨线程重分配和释放。
     *
     * @details
     *   存储方式：
     *   - 堆分配，父子指针连接，闭区间存储，即 k_left <= k_parent <= k_right
     *   - K_shrink_factor 控制缩容阈值 (决定下溢标准)
     *
     *   方法简述：
     *   - 支持 `select` 查找单个、`select_all` 查找范围
     *   - 支持 `append` 尾插，`prepend` 头插，`insert` 唯一键插入，`update` 唯一键插入或更新
     *   - 支持 `erase` 删除单个、`erase_all` 删除所有相同键
     *   - 上述所有操作均约为 O(logₘn + k)，其中 Fast:k=0，First/Last:K/2，all:K
     *   - 通过 `range`、`krange`、`vrange` 以及原生反向范围 进行随机访问，均摊 O(1)
     *
     *   注意事项：
     *   - 若只使用 `insert`/`update` 则为唯一键，否则可能为重复键，取决于调用方
     *   - 容量限制：增加元素超过 `limit` 时会触发 abort
     *   - 范围限制：传入非有限范围在 !XY_RELEASE 下检查并触发 abort
     *   - 自引用：禁止将容器自身的范围传给自身操作
     *   - 迭代器失效：插入操作触发分裂或删除操作触发借用合并会使部分旧迭代器失效
     *
     *   异常安全：
     *   - 复制赋值、所有非范围操作：抛出异常时回滚至操作前状态
     *   - 所有范围操作（包括初始化列表）：若抛出异常，已成功的元素保留，不会回滚
     */
    template <typename Key, typename Val, xyu::uint M = 32, bool cross_thread = false>
    struct BTreeMap : __::BTreeBase<Key, Val, M, cross_thread>
    {
        static_assert(!xyu::t_is_void<Val>);
        static_assert(xyu::t_can_nothrow_destruct<Key, Val>);
    private:
        using Base = __::BTreeBase<Key, Val, M, cross_thread>;
        using Node = __::KVNode<Key, Val>;

    public:
        /* 构造赋值 */

        /// 默认构造
        BTreeMap() noexcept = default;
        
        /// 范围构造
        template <typename kvRg, typename = xyu::t_enable<Node::template can_kv_range<kvRg>>>
        explicit BTreeMap(kvRg&& rg) noexcept(noexcept(append(xyu::forward<kvRg>(rg))))
        {
            BTreeMap tmp;
            tmp.append(xyu::forward<kvRg>(rg));
            swap(tmp);
        }
        /// 范围构造
        template <typename kRg, typename vRg, typename = xyu::t_enable<Node::template can_kv_2range<kRg, vRg>>>
        explicit BTreeMap(kRg&& krg, vRg&& vrg) noexcept(noexcept(append(xyu::forward<kRg>(krg), xyu::forward<vRg>(vrg))))
        {
            BTreeMap tmp;
            tmp.append(xyu::forward<kRg>(krg), xyu::forward<vRg>(vrg));
            swap(tmp);
        }
        
        /// 复制构造
        BTreeMap(const BTreeMap& other) noexcept(noexcept(BTreeMap(other.range()))) : BTreeMap(other.range()) {}

        /// 复制赋值
        BTreeMap& operator=(const BTreeMap& other) noexcept(noexcept(BTreeMap(other.range())))
        {
            if (XY_UNLIKELY(this == &other)) return *this;
            BTreeMap tmp(other.count());
            tmp.append(other.range());
            swap(tmp);
            return *this;
        }

        /* 移动交换 */

        /// 移动构造
        BTreeMap(BTreeMap&& other) noexcept : Base{other.n, other.m, other.root, other.ln, other.rn}
        {
            other.n = other.m = 0;
            other.root = other.ln = other.rn = nullptr;
        }

        /// 移动赋值
        BTreeMap& operator=(BTreeMap&& other) noexcept { return swap(other); }

        /// 交换
        BTreeMap& swap(BTreeMap& other) noexcept { Base::swap(other); return *this; }

        /* 查找 */

        /**
         * @brief 查找方式
         * @details
         *   Faster: 查找离根最近的元素；
         *   First: 查找排在最左边的元素；
         *   Last: 查找排在最右边的元素；
         */
        enum _LocateType : int { Fast = 0, First = -1, Last = 1 };

        /**
         * @brief 迭代器类型
         * @details
         *   KeyVal: 解引用后得到键值，(*it).key, (*it).val；
         *   OnlyKey: 解引用后得到键，*it == key；
         *   OnlyVal: 解引用后得到值，*it == val；
         */
        enum _IterType : int { KeyVal = 0, OnlyKey = 1, OnlyVal = 2 };

        /// 查找键值返回迭代器，不存在返回 空迭代器
        template <_LocateType locate = Fast, _IterType iter = OnlyVal, typename K, typename = xyu::t_enable<xyu::t_can_compare<Key,K> || xyu::t_can_init<Key,K>>>
        auto select(K&& key) noexcept(noexcept(this->template select_impl<locate, iter>(xyu::forward<K>(key))))
        { return this->template select_impl<locate, iter>(xyu::forward<K>(key)); }

        /// 查找键值返回常量迭代器，不存在返回 空迭代器
        template <_LocateType locate = Fast, _IterType iter = OnlyVal, typename K, typename = xyu::t_enable<xyu::t_can_compare<Key,K> || xyu::t_can_init<Key,K>>>
        auto select(K&& key) const noexcept(noexcept(const_cast<BTreeMap*>(this)->template select<locate, _IterType{iter + 3}>(xyu::forward<K>(key))))
        { return const_cast<BTreeMap*>(this)->template select<locate, _IterType{iter + 3}>(xyu::forward<K>(key)); }

        /// 查找所有键值返回范围
        template <_IterType iter = OnlyVal, typename K, typename = xyu::t_enable<xyu::t_can_compare<Key,K> || xyu::t_can_init<Key,K>>>
        auto select_all(K&& key) noexcept(noexcept(this->template select_all_impl<iter>(xyu::forward<K>(key))))
        { return this->template select_all_impl<iter>(xyu::forward<K>(key)); }

        /// 查找所有键值返回常量范围
        template <_IterType iter = OnlyVal, typename K, typename = xyu::t_enable<xyu::t_can_compare<Key,K> || xyu::t_can_init<Key,K>>>
        auto select_all(K&& key) const noexcept(noexcept(const_cast<BTreeMap*>(this)->template select_all_impl<_IterType{iter + 3}>(xyu::forward<K>(key))))
        { return const_cast<BTreeMap*>(this)->template select_all_impl<_IterType{iter + 3}>(xyu::forward<K>(key)); }

        /* 增加 */

        /// 插入键值，如果有相同键追加到末尾并返回迭代器
        template <_IterType iter = OnlyVal, typename K, typename... Args, typename = xyu::t_enable<xyu::t_can_init<Key,K> && xyu::t_can_init<Val,Args...>>>
        auto append(K&& key, Args&&... args) noexcept(noexcept(this->template append_impl<iter>(xyu::forward<K>(key), xyu::forward<Args>(args)...)))
        {
            __::Checker::check_capa<Base::limit()>(this->n, 1, XY_CurPos("BTreeMap::append"));
            return this->template append_impl<iter>(xyu::forward<K>(key), xyu::forward<Args>(args)...);
        }

        /// 插入键值范围，如果有相同键追加到末尾
        template <typename kvRg, typename = xyu::t_enable<Node::template can_kv_range<kvRg>>>
        BTreeMap& append(kvRg&& rg) noexcept(noexcept(this->append_impl(Node::template get_key_ne<kvRg>(), Node::template get_val_ne<kvRg>())))
        {
            auto range = xyu::make_range(xyu::forward<kvRg>(rg));
            __::Checker::check_range(range, XY_CurPos("BTreeMap::append"));
            __::Checker::check_capa<Base::limit()>(this->n, range.count(), XY_CurPos("BTreeMap::append"));
            for (auto&& kv : range) this->append_impl(xyu::forward<decltype(kv)>(kv).key, xyu::forward<decltype(kv)>(kv).val);
            return *this;
        }

        /// 插入键值范围，如果有相同键追加到末尾
        template <typename kRg, typename vRg, typename = xyu::t_enable<Node::template can_kv_2range<kRg, vRg>>>
        BTreeMap& append(kRg&& krg, vRg&& vrg) noexcept(noexcept(this->append_impl(Node::template get_ne<kRg>(), Node::template get_ne<vRg>())))
        {
            auto&& krange = xyu::make_range(xyu::forward<kRg>(krg));
            auto&& vrange = xyu::make_range(xyu::forward<vRg>(vrg));
            __::Checker::check_range(krange, vrange, XY_CurPos("BTreeMap::append"));
            __::Checker::check_capa<Base::limit()>(this->n, krange.count(), XY_CurPos("BTreeMap::append"));
            auto ki = krange.begin(); auto ko = krange.end();
            auto vi = vrange.begin();
            while (ki != ko) this->append_impl(*ki, *vi), ++ki, ++vi;
            return *this;
        }

        /// 插入键值范围，如果有相同键追加到末尾
        BTreeMap& append(std::initializer_list<Node> il) noexcept(noexcept(append(xyu::make_range(il)))) { return append(xyu::make_range(il)); }


        /// 插入键值，如果有相同键插入到开头并返回迭代器
        template <_IterType iter = OnlyVal, typename K, typename... Args, typename = xyu::t_enable<xyu::t_can_init<Key,K> && xyu::t_can_init<Val,Args...>>>
        auto prepend(K&& key, Args&&... args) noexcept(noexcept(this->template prepend_impl<iter>(xyu::forward<K>(key), xyu::forward<Args>(args)...)))
        {
            __::Checker::check_capa<Base::limit()>(this->n, 1, XY_CurPos("BTreeMap::prepend"));
            return this->template prepend_impl<iter>(xyu::forward<K>(key), xyu::forward<Args>(args)...);
        }

        /// 插入键值范围，如果有相同键插入到开头
        template <typename kvRg, typename = xyu::t_enable<Node::template can_kv_range<kvRg>>>
        BTreeMap& prepend(kvRg&& rg) noexcept(noexcept(this->prepend_impl(Node::template get_key_ne<kvRg>(), Node::template get_val_ne<kvRg>())))
        {
            auto range = xyu::make_range(xyu::forward<kvRg>(rg));
            __::Checker::check_range(range, XY_CurPos("BTreeMap::prepend"));
            __::Checker::check_capa<Base::limit()>(this->n, range.count(), XY_CurPos("BTreeMap::prepend"));
            for (auto&& kv : range) this->prepend_impl(xyu::forward<decltype(kv)>(kv).key, xyu::forward<decltype(kv)>(kv).val);
            return *this;
        }

        /// 插入键值范围，如果有相同键插入到开头
        template <typename kRg, typename vRg, typename = xyu::t_enable<Node::template can_kv_2range<kRg, vRg>>>
        BTreeMap& prepend(kRg&& krg, vRg&& vrg) noexcept(noexcept(this->prepend_impl(Node::template get_ne<kRg>(), Node::template get_ne<vRg>())))
        {
            auto&& krange = xyu::make_range(xyu::forward<kRg>(krg));
            auto&& vrange = xyu::make_range(xyu::forward<vRg>(vrg));
            __::Checker::check_range(krange, vrange, XY_CurPos("BTreeMap::prepend"));
            __::Checker::check_capa<Base::limit()>(this->n, krange.count(), XY_CurPos("BTreeMap::prepend"));
            auto ki = krange.begin(); auto ko = krange.end();
            auto vi = vrange.begin();
            while (ki != ko) this->prepend_impl(*ki, *vi), ++ki, ++vi;
            return *this;
        }

        /// 插入键值范围，如果有相同键插入到开头
        BTreeMap& prepend(std::initializer_list<Node> il) noexcept(noexcept(prepend(xyu::make_range(il)))) { return prepend(xyu::make_range(il)); }


        /// 插入键值，如果有相同键则直接返回迭代器
        template <_LocateType locate = Fast, _IterType iter = OnlyVal, typename K, typename... Args, typename = xyu::t_enable<xyu::t_can_init<Key,K> && xyu::t_can_init<Val,Args...>>>
        auto insert(K&& key, Args&&... args) noexcept(noexcept(this->template insert_impl<locate, iter>(xyu::forward<K>(key), xyu::forward<Args>(args)...)))
        {
            __::Checker::check_capa<Base::limit()>(this->n, 1, XY_CurPos("BTreeMap::insert"));
            return this->template insert_impl<locate, iter>(xyu::forward<K>(key), xyu::forward<Args>(args)...);
        }

        /// 插入键值范围，如果有相同键则直接返回
        template <_LocateType locate = Fast, typename kvRg, typename = xyu::t_enable<Node::template can_kv_range<kvRg>>>
        BTreeMap& insert(kvRg&& rg) noexcept(noexcept(this->template insert_impl<locate>(Node::template get_key_ne<kvRg>(), Node::template get_val_ne<kvRg>())))
        {
            auto range = xyu::make_range(xyu::forward<kvRg>(rg));
            __::Checker::check_range(range, XY_CurPos("BTreeMap::insert"));
            __::Checker::check_capa<Base::limit()>(this->n, range.count(), XY_CurPos("BTreeMap::insert"));
            for (auto&& kv : range) this->template insert_impl<locate>(xyu::forward<decltype(kv)>(kv).key, xyu::forward<decltype(kv)>(kv).val);
            return *this;
        }

        /// 插入键值范围，如果有相同键则直接返回
        template <_LocateType locate = Fast, typename kRg, typename vRg, typename = xyu::t_enable<Node::template can_kv_2range<kRg, vRg>>>
        BTreeMap& insert(kRg&& krg, vRg&& vrg) noexcept(noexcept(this->template insert_impl<locate>(Node::template get_ne<kRg>(), Node::template get_ne<vRg>())))
        {
            auto&& krange = xyu::make_range(xyu::forward<kRg>(krg));
            auto&& vrange = xyu::make_range(xyu::forward<vRg>(vrg));
            __::Checker::check_range(krange, vrange, XY_CurPos("BTreeMap::insert"));
            __::Checker::check_capa<Base::limit()>(this->n, krange.count(), XY_CurPos("BTreeMap::insert"));
            auto ki = krange.begin(); auto ko = krange.end();
            auto vi = vrange.begin();
            while (ki != ko) this->template insert_impl<locate>(*ki, *vi), ++ki, ++vi;
            return *this;
        }

        /// 插入键值范围，如果有相同键则直接返回
        template <_LocateType locate = Fast>
        BTreeMap& insert(std::initializer_list<Node> il) noexcept(noexcept(insert<locate>(xyu::make_range(il)))) { return insert<locate>(xyu::make_range(il)); }

        /* 修改 */

        /// 插入键值，如果有相同键则修改并返回迭代器
        template <_LocateType locate = Fast, _IterType iter = OnlyVal, typename K, typename... Args, typename = xyu::t_enable<xyu::t_can_init<Key,K> && xyu::t_can_init<Val,Args...> && (sizeof...(Args) != 1 || xyu::t_can_assign<Val,Args...>)>>
        auto update(K&& key, Args&&... args) noexcept(noexcept(this->template update_impl<locate, iter>(xyu::forward<K>(key), xyu::forward<Args>(args)...)))
        {
            __::Checker::check_capa<Base::limit()>(this->n, 1, XY_CurPos("BTreeMap::update"));
            return this->template update_impl<locate, iter>(xyu::forward<K>(key), xyu::forward<Args>(args)...);
        }

        /// 插入键值范围，如果有相同键则修改
        template <_LocateType locate = Fast, typename kvRg, typename = xyu::t_enable<Node::template can_kv_range<kvRg> && xyu::t_can_assign<Val, decltype(xyu::t_val<xyu::t_access<kvRg>>().val)>>>
        BTreeMap& update(kvRg&& rg) noexcept(noexcept(this->template update_impl<locate>(Node::template get_key_ne<kvRg>(), Node::template get_val_ne<kvRg>())))
        {
            auto range = xyu::make_range(xyu::forward<kvRg>(rg));
            __::Checker::check_range(range, XY_CurPos("BTreeMap::update"));
            __::Checker::check_capa<Base::limit()>(this->n, range.count(), XY_CurPos("BTreeMap::update"));
            for (auto&& kv : range) this->template update_impl<locate>(xyu::forward<decltype(kv)>(kv).key, xyu::forward<decltype(kv)>(kv).val);
            return *this;
        }

        /// 插入键值范围，如果有相同键则修改
        template <_LocateType locate = Fast, typename kRg, typename vRg, typename = xyu::t_enable<Node::template can_kv_2range<kRg, vRg> && xyu::t_can_assign<Val, xyu::t_access<vRg>>>>
        BTreeMap& update(kRg&& krg, vRg&& vrg) noexcept(noexcept(this->template update_impl<locate>(Node::template get_ne<kRg>(), Node::template get_ne<vRg>())))
        {
            auto&& krange = xyu::make_range(xyu::forward<kRg>(krg));
            auto&& vrange = xyu::make_range(xyu::forward<vRg>(vrg));
            __::Checker::check_range(krange, vrange, XY_CurPos("BTreeMap::update"));
            __::Checker::check_capa<Base::limit()>(this->n, krange.count(), XY_CurPos("BTreeMap::update"));
            auto ki = krange.begin(); auto ko = krange.end();
            auto vi = vrange.begin();
            while (ki != ko) this->template update_impl<locate>(*ki, *vi), ++ki, ++vi;
            return *this;
        }

        /// 插入键值范围，如果有相同键则修改
        template <_LocateType locate = Fast>
        BTreeMap& update(std::initializer_list<Node> il) noexcept(noexcept(update<locate>(xyu::make_range(il)))) { return update<locate>(xyu::make_range(il)); }

        /* 删除 */

        /// 删除键值，返回是否删除成功
        template <_LocateType locate = Fast, typename K, typename = xyu::t_enable<xyu::t_can_compare<Key,K> || xyu::t_can_init<Key,K>>>
        bool erase(K&& key) noexcept(noexcept(this->template erase_impl<locate>(xyu::forward<K>(key))))
        { return this->template erase_impl<locate>(xyu::forward<K>(key)); }

        /// 删除键值范围，返回成功删除的个数
        template <_LocateType locate = Fast, typename kRg, typename = xyu::t_enable<xyu::t_can_range<kRg> && !xyu::t_can_compare<Key,kRg> && !xyu::t_can_init<Key,kRg> && (xyu::t_can_compare<Key,xyu::t_access<kRg>> || xyu::t_can_init<Key,xyu::t_access<kRg>>)>>
        xyu::size_t erase(kRg&& rg) noexcept(noexcept(this->template erase_impl<locate>(Node::template get_ne<kRg>())))
        {
            auto range = xyu::make_range(xyu::forward<kRg>(rg));
            __::Checker::check_range(range, XY_CurPos("BTreeMap::erase"));
            xyu::size_t count = 0;
            for (auto&& key : range)
                count += this->template erase_impl<locate>(xyu::forward<decltype(key)>(key));
            return count;
        }
        
        /// 删除键值范围，返回成功删除的个数
        template <_LocateType locate = Fast>
        xyu::size_t erase(std::initializer_list<Key> il) noexcept(noexcept(erase<locate>(xyu::make_range(il)))) { return erase<locate>(xyu::make_range(il)); }
        
        
        /// 删除所有匹配键值，返回删除个数
        template <typename K, typename = xyu::t_enable<xyu::t_can_compare<Key,K> || xyu::t_can_init<Key,K>>>
        xyu::size_t erase_all(K&& key) noexcept(noexcept(this->erase_all_impl(xyu::forward<K>(key))))
        { return this->erase_all_impl(xyu::forward<K>(key)); }

        /// 删除所有匹配键值范围，返回删除个数
        template <typename kRg, typename = void, typename = xyu::t_enable<!xyu::t_can_compare<Key,kRg> && !xyu::t_can_init<Key,kRg> && (xyu::t_can_compare<Key,xyu::t_access<kRg>> || xyu::t_can_init<Key,xyu::t_access<kRg>>)>>
        xyu::size_t erase_all(kRg&& rg) noexcept(noexcept(this->erase_all_impl(Node::template get_ne<kRg>())))
        {
            auto range = xyu::make_range(xyu::forward<kRg>(rg));
            __::Checker::check_range(range, XY_CurPos("BTreeMap::erase_all"));
            xyu::size_t count = 0;
            for (auto&& key : range)
                count += this->erase_all_impl(xyu::forward<decltype(key)>(key));
            return count;
        }

        /// 删除所有匹配键值范围，返回删除个数
        xyu::size_t erase_all(std::initializer_list<Key> il) noexcept(noexcept(erase_all(xyu::make_range(il)))) { return erase_all(xyu::make_range(il)); }

        /* 插入 - 迭代器 */

        /**
         * @brief 迭代器位置插入键值，并返回新迭代器
         * @attention 自行确保 key 的大小适合此位置
         */
        template <typename It, typename K, typename... Args, typename = xyu::t_enable<xyu::t_can_init<Key,K> && xyu::t_can_init<Val,Args...> && xyu::t_is_same<Base, typename It::Owner_t>>>
        It insert(It iter, K&& k, Args&&... args) noexcept(noexcept(this->insert_iter_impl(iter, xyu::forward<K>(k), xyu::forward<Args>(args)...)))
        { return this->insert_iter_impl(iter, xyu::forward<K>(k), xyu::forward<Args>(args)...); }

        /* 删除- 迭代器 */

        /**
         * @brief 删除迭代器位置键值，并返回下一个位置迭代器
         * @details 空迭代器会返回原状
         */
        template <typename It, typename = xyu::t_enable<xyu::t_is_same<Base, typename It::Owner_t>>>
        It erase(It iter) noexcept(noexcept(this->erase_iter_impl(iter))){ return this->erase_iter_impl(iter); }

        /* 范围 */

        /// 获取升序范围
        template <_IterType type = KeyVal>
        auto range() noexcept { return this->template get_range<type>(); }
        /// 获取升序常量范围
        template <_IterType type = KeyVal>
        auto range() const noexcept { return const_cast<BTreeMap*>(this)->template get_range<_IterType{type + 3}>(); }

        /// 获取降序范围
        template <_IterType type = KeyVal>
        auto rrange() noexcept { return this->template get_rrange<type>(); }
        /// 获取降序常量范围
        template <_IterType type = KeyVal>
        auto rrange() const noexcept { return const_cast<BTreeMap*>(this)->template get_rrange<_IterType{type + 3}>(); }

        /* 运算符 */

        /// 值访问，不存在时默认构造返回
        template <typename K, typename = xyu::t_enable<xyu::t_can_init<Key,K> && xyu::t_can_init<Val>>>
        Val& operator[](K&& key) noexcept(noexcept(insert(xyu::forward<K>(key)))) { return *insert(xyu::forward<K>(key)); }

        /// 插入键值，如果有相同键追加到末尾
        template <typename Arg>
        BTreeMap& operator<<(Arg&& arg) noexcept(noexcept(append(xyu::forward<Arg>(arg))))
        { append(xyu::forward<Arg>(arg)); return *this; }

        /// 插入键值，如果有相同键插入到开头
        template <typename Arg>
        friend BTreeMap& operator>>(Arg&& arg, BTreeMap& map) noexcept(noexcept(map.prepend(xyu::forward<Arg>(arg))))
        { map.prepend(xyu::forward<Arg>(arg)); return map; }
    };

    // 推导指引
    template <typename Rg, typename = xyu::t_enable<xyu::t_can_range<Rg>>>
    BTreeMap(Rg&&) -> BTreeMap<xyu::t_rem_cr<decltype(xyu::t_val<xyu::t_access<Rg>>().key)>, xyu::t_rem_cr<decltype(xyu::t_val<xyu::t_access<Rg>>().val)>>;
    template <typename kRg, typename vRg, typename = xyu::t_enable<xyu::t_can_range<kRg, vRg>>>
    BTreeMap(kRg&&, vRg&&) -> BTreeMap<xyu::t_rem_cr<xyu::t_access<kRg>>, xyu::t_rem_cr<xyu::t_access<vRg>>>;
}

/// B树集
namespace xylu::xycoll
{
    /**
     * @brief B树集
     * @tparam Key  键类型
     * @tparam M    阶数 (每个节点最多有 M-1 个键值，最多有 M 个孩子)
     * @tparam cross_thread  是否允许跨线程内存管理。
     *         - false（默认）: 使用线程局部内存池，性能更好；
     *         - true : 使用全局内存分配，对象可跨线程重分配和释放。
     *
     * @details
     *   存储方式：
     *   - 堆分配，父子指针连接
     *   - K_shrink_factor 控制缩容阈值 (决定下溢标准)
     *
     *   方法简述：
     *   - 支持 `select` 查找是否存在、`select_all` 查找存在数量
     *   - 支持 `append` 重复插入，`insert` 唯一插入
     *   - 支持 `erase` 删除单个、`erase_all` 删除所有相同键
     *   - 上述所有操作均约为 O(logₘn + k)，其中 all:K，其余k为0
     *   - 通过 `range`、`rrange` 进行随机访问，均摊 O(1)
     *
     *   注意事项：
     *   - 若只使用 `insert` 则为唯一键，否则可能为重复键，取决于调用方
     *   - 容量限制：增加元素超过 `limit` 时会触发 abort
     *   - 范围限制：传入非有限范围在 !XY_RELEASE 下检查并触发 abort
     *   - 自引用：禁止将容器自身的范围传给自身操作
     *   - 迭代器失效：插入操作触发分裂或删除操作触发借用合并会使部分旧迭代器失效
     *
     *   异常安全：
     *   - 复制赋值、所有非范围操作：抛出异常时回滚至操作前状态
     *   - 所有范围操作（包括初始化列表）：若抛出异常，已成功的元素保留，不会回滚
     */
    template <typename Key, xyu::uint M = 64, bool cross_thread = false>
    struct BTreeSet : __::BTreeBase<Key, int[0], M, cross_thread>
    {
        static_assert(xyu::t_can_nothrow_destruct<Key>);
    private:
        using Base = __::BTreeBase<Key, int[0], M, cross_thread>;
        using Node = __::KVNode<Key, int[0]>;

    public:
        /* 构造赋值 */

        /// 默认构造
        BTreeSet() noexcept = default;

        /// 范围构造
        template <typename Rg, typename = xyu::t_enable<!xyu::t_can_init<Key,Rg> && xyu::t_can_init<Key,xyu::t_access<Rg>>>>
        explicit BTreeSet(Rg&& rg) noexcept(noexcept(append(xyu::forward<Rg>(rg))))
        {
            BTreeSet tmp;
            tmp.append(xyu::forward<Rg>(rg));
            swap(tmp);
        }

        /// 复制构造
        BTreeSet(const BTreeSet& other) noexcept(noexcept(BTreeSet(other.range()))) : BTreeSet(other.range()) {}

        /// 复制赋值
        BTreeSet& operator=(const BTreeSet& other) noexcept(noexcept(BTreeSet(other.range())))
        {
            if (XY_UNLIKELY(this == &other)) return *this;
            BTreeSet tmp(other.count());
            tmp.append(other.range());
            swap(tmp);
            return *this;
        }

        /* 移动交换 */

        /// 移动构造
        BTreeSet(BTreeSet&& other) noexcept : Base{other.n, other.m, other.root, other.ln, other.rn}
        {
            other.n = other.m = 0;
            other.root = other.ln = other.rn = nullptr;
        }

        /// 移动赋值
        BTreeSet& operator=(BTreeSet&& other) noexcept { return swap(other); }

        /// 交换
        BTreeSet& swap(BTreeSet& other) noexcept { Base::swap(other); return *this; }

        /* 查找 */

        /// 查找键，返回迭代器指示是否找到
        template <typename K, typename = xyu::t_enable<xyu::t_can_compare<Key,K> || xyu::t_can_init<Key,K>>>
        auto select(K&& key) const noexcept(noexcept(const_cast<BTreeSet*>(this)->template select_impl<0,1>(xyu::forward<K>(key))))
        { return const_cast<BTreeSet*>(this)->template select_impl<0,1>(xyu::forward<K>(key)); }

        /// 查找所有键，返回找到个数
        template <typename K, typename = xyu::t_enable<xyu::t_can_compare<Key,K> || xyu::t_can_init<Key,K>>>
        xyu::size_t select_all(K&& key) const noexcept(noexcept(const_cast<BTreeSet*>(this)->template select_all_impl<1>(xyu::forward<K>(key))))
        { return const_cast<BTreeSet*>(this)->template select_all_impl<1>(xyu::forward<K>(key)).count(); }

        /* 增加 */

        /// 插入键，如果有相同键追加到末尾
        template <typename K, typename = xyu::t_enable<xyu::t_can_init<Key,K>>>
        auto append(K&& key) noexcept(noexcept(this->template append_impl<1>(xyu::forward<K>(key))))
        {
            __::Checker::check_capa<Base::limit()>(this->n, 1, XY_CurPos("BTreeSet::append"));
            return this->template append_impl<1>(xyu::forward<K>(key));
        }

        /// 插入键值范围，如果有相同键追加到末尾
        template <typename Rg, typename=void, typename = xyu::t_enable<!xyu::t_can_init<Key,Rg> && xyu::t_can_init<Key,xyu::t_access<Rg>>>>
        BTreeSet& append(Rg&& rg) noexcept(noexcept(this->append_impl(Node::template get_ne<Rg>())))
        {
            auto range = xyu::make_range(xyu::forward<Rg>(rg));
            __::Checker::check_range(range, XY_CurPos("BTreeSet::append"));
            __::Checker::check_capa<Base::limit()>(this->n, range.count(), XY_CurPos("BTreeSet::append"));
            for (auto&& key : range) this->append_impl(xyu::forward<decltype(key)>(key));
            return *this;
        }

        /// 插入键值范围，如果有相同键追加到末尾
        BTreeSet& append(std::initializer_list<Key> il) noexcept(noexcept(append(xyu::make_range(il)))) { return append(xyu::make_range(il)); }


        /// 插入键，如果有相同键则忽略
        template <typename K, typename = xyu::t_enable<xyu::t_can_init<Key,K>>>
        auto insert(K&& key) noexcept(noexcept(this->template insert_impl<0,1>(xyu::forward<K>(key))))
        {
            __::Checker::check_capa<Base::limit()>(this->n, 1, XY_CurPos("BTreeSet::insert"));
            return this->template insert_impl<0,1>(xyu::forward<K>(key));
        }

        /// 插入键范围，如果有相同键则忽略
        template <typename Rg, typename=void, typename = xyu::t_enable<xyu::t_can_range<Rg> && !xyu::t_can_init<Key,Rg> && xyu::t_can_init<Key,xyu::t_access<Rg>>>>
        BTreeSet& insert(Rg&& rg) noexcept(noexcept(this->insert_impl(Node::template get_ne<Rg>())))
        {
            auto range = xyu::make_range(xyu::forward<Rg>(rg));
            __::Checker::check_range(range, XY_CurPos("BTreeSet::insert"));
            __::Checker::check_capa<Base::limit()>(this->n, range.count(), XY_CurPos("BTreeSet::insert"));
            for (auto&& key : range) this->insert_impl(xyu::forward<decltype(key)>(key));
            return *this;
        }

        /// 插入键范围，如果有相同键则忽略
        BTreeSet& insert(std::initializer_list<Key> il) noexcept(noexcept(insert(xyu::make_range(il)))) { return insert(xyu::make_range(il)); }

        /* 删除 */

        /// 删除键，返回是否删除成功
        template <typename K, typename = xyu::t_enable<xyu::t_can_compare<Key,K> || xyu::t_can_init<Key,K>>>
        bool erase(K&& key) noexcept(noexcept(this->erase_impl(xyu::forward<K>(key))))
        { return this->erase_impl(xyu::forward<K>(key)); }

        /// 删除键范围，返回成功删除的个数
        template <typename Rg, typename=void, typename = xyu::t_enable<xyu::t_can_range<Rg> && !xyu::t_can_compare<Key,Rg> && !xyu::t_can_init<Key,Rg> && (xyu::t_can_compare<Key,xyu::t_access<Rg>> || xyu::t_can_init<Key,xyu::t_access<Rg>>)>>
        xyu::size_t erase(Rg&& rg) noexcept(noexcept(this->erase_impl(Node::template get_ne<Rg>())))
        {
            auto range = xyu::make_range(xyu::forward<Rg>(rg));
            __::Checker::check_range(range, XY_CurPos("BTreeSet::erase"));
            xyu::size_t count = 0;
            for (auto&& key : range)
                count += this->erase_impl(xyu::forward<decltype(key)>(key));
            return count;
        }

        /// 删除键范围，返回成功删除的个数
        xyu::size_t erase(std::initializer_list<Key> il) noexcept(noexcept(erase(xyu::make_range(il)))) { return erase(xyu::make_range(il)); }


        /// 删除所有匹配键，返回删除个数
        template <typename K, typename = xyu::t_enable<xyu::t_can_compare<Key,K> || xyu::t_can_init<Key,K>>>
        xyu::size_t erase_all(K&& key) noexcept(noexcept(this->erase_all_impl(xyu::forward<K>(key))))
        { return this->erase_all_impl(xyu::forward<K>(key)); }

        /// 删除所有匹配键范围，返回删除个数
        template <typename Rg, typename=void, typename = xyu::t_enable<!xyu::t_can_compare<Key,Rg> && !xyu::t_can_init<Key,Rg> && (xyu::t_can_compare<Key,xyu::t_access<Rg>> || xyu::t_can_init<Key,xyu::t_access<Rg>>)>>
        xyu::size_t erase_all(Rg&& rg) noexcept(noexcept(this->erase_all_impl(Node::template get_ne<Rg>())))
        {
            auto range = xyu::make_range(xyu::forward<Rg>(rg));
            __::Checker::check_range(range, XY_CurPos("BTreeSet::erase_all"));
            xyu::size_t count = 0;
            for (auto&& key : range)
                count += this->erase_all_impl(xyu::forward<decltype(key)>(key));
            return count;
        }

        /// 删除所有匹配键范围，返回删除个数
        xyu::size_t erase_all(std::initializer_list<Key> il) noexcept(noexcept(erase_all(xyu::make_range(il)))) { return erase_all(xyu::make_range(il)); }

        /* 插入 - 迭代器 */

        /**
         * @brief 迭代器位置插入键值，并返回新迭代器
         * @attention 自行确保 key 的大小适合此位置
         */
        template <typename It, typename K, typename = xyu::t_enable<xyu::t_can_init<Key,K> && xyu::t_is_same<Base, typename It::Owner_t>>>
        It insert(It iter, K&& k) noexcept(noexcept(this->insert_iter_impl(iter, xyu::forward<K>(k))))
        { return this->insert_iter_impl(iter, xyu::forward<K>(k)); }

        /* 删除- 迭代器 */

        /**
         * @brief 删除迭代器位置键值，并返回下一个位置迭代器
         * @details 空迭代器会返回原状
         */
        template <typename It, typename = xyu::t_enable<xyu::t_is_same<Base, typename It::Owner_t>>>
        It erase(It iter) noexcept(noexcept(this->erase_iter_impl(iter))){ return this->erase_iter_impl(iter); }

        /* 范围 */

        /// 获取升序范围
        auto range() const noexcept { return const_cast<BTreeSet*>(this)->template get_range<1>(); }
        /// 获取降序范围
        auto rrange() const noexcept { return const_cast<BTreeSet*>(this)->template get_rrange<1>(); }

        /* 运算符 */

        /// 插入键值，如果有相同键追加到末尾
        template <typename Arg>
        BTreeSet& operator<<(Arg&& arg) noexcept(noexcept(append(xyu::forward<Arg>(arg))))
        { append(xyu::forward<Arg>(arg)); return *this; }

        /// 插入键值，如果有相同键插入到开头
        template <typename Arg>
        friend BTreeSet& operator>>(Arg&& arg, BTreeSet& map) noexcept(noexcept(map.prepend(xyu::forward<Arg>(arg))))
        { map.prepend(xyu::forward<Arg>(arg)); return map; }
    };

    // 推导指引
    template <typename Rg, typename = xyu::t_enable<xyu::t_can_range<Rg>>>
    BTreeSet(Rg&&) -> BTreeSet<xyu::t_rem_cr<xyu::t_access<Rg>>>;
}
