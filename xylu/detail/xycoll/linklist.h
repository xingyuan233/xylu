#pragma once

/**
 * @file    xycoll/linklist.h
 * @brief   链表
 * @date    2026-04-25
 */

#include "./heapval.h"

namespace xylu::xycoll
{
    namespace __
    {
        /// 链表节点基类
        struct LinkNodeBase
        {
            LinkNodeBase* next;     // 下一个节点
            LinkNodeBase* prev;     // 上一个节点
        };

        /// 链表节点类
        template <typename T>
        struct LinkNode : LinkNodeBase
        {
            T val;    // 数据

            template <typename... Args, typename Test = T, xyu::t_enable<xyu::t_is_aggregate<Test>, bool> = true>
            explicit LinkNode(LinkNodeBase* next, LinkNodeBase* prev, Args&&... args) noexcept(xyu::t_can_nothrow_listinit<T, Args...>)
                    : LinkNodeBase{next, prev}, val{xyu::forward<Args>(args)...} {}

            template <typename... Args, typename Test = T, xyu::t_enable<!xyu::t_is_aggregate<Test>, bool> = false>
            explicit LinkNode(LinkNodeBase* next, LinkNodeBase* prev, Args&&... args) noexcept(xyu::t_can_nothrow_construct<T, Args...>)
                    : LinkNodeBase{next, prev}, val(xyu::forward<Args>(args)...) {}
        };
    }

    /**
     * @brief 双向链表
     * @tparam T  元素类型
     * @tparam cross_thread  是否允许跨线程内存管理。
     *         - false（默认）: 使用线程局部内存池，性能更好；
     *         - true : 使用全局内存分配，对象可跨线程重分配和释放。
     *
     * @details
     *   存储方式：
     *   - 堆分配节点，指针连接，!cross_thread 下通常有更好的缓存局部性
     *   - 通过哨兵节点存储 第一个节点 与 最后一个节点
     *
     *   方法简述：
     *   - 支持 `get`O(n/2)、`head`O(1)、`tail`O(1) 访问元素
     *   - 支持 `append`O(1)、`prepend`O(1)、`insert` 通过 索引/迭代器 插入 多元素/范围/单元素，`erase` 删除元素，
     *     其中 `insert`、`erase` 节点操作为 O(1)，索引定位为 O(n/2)，此类操作均会返回第一个操作位置的新迭代器
     *   - 支持 `take` 移出节点或子链表（返回 `HeapVal` / `LinkList`），`put` 移入 O(1) 拆分与拼接，定位为 O(n/2)
     *
     *   注意事项：
     *   - 容量限制：增加元素超过 `limit` 时会触发 abort
     *   - 范围限制：传入非有限范围在 !XY_RELEASE 下检查并触发 abort
     *   - 索引越界：`get`、`op[]`、`head`、`tail`、`take`单节点定位 会在 !XY_RELEASE 下检查并触发 abort，其余方法无害
     *   - 迭代器所属：迭代器操作传入的非所属迭代器时 会在 XY_DEBUG 下检查归属并触发 abort
     *   - 哨兵保护：`erase`、`take` 传入哨兵节点时 会在 !XY_RELEASE 下检查并触发 abort
     *
     *   异常安全：
     *   - 复制赋值、`insert`：抛出异常时回滚至操作前状态
     *   - `append`、`prepend`：若元素构造抛出异常，已成功构造的元素保留，需要手动回滚或继续尝试
     */
    template <typename T, bool cross_thread = false>
    class LinkList
    {
        static_assert(xyu::t_can_nothrow_destruct<T>, "LinkList don't support C4");

        using Iter = xyu::Iterator_Node<__::LinkNode<T>, LinkList>;
        using rIter = xyu::Iterator_Reverse_Direct<Iter>;
    private:
        __::LinkNodeBase lead;      // 哨兵节点
        xyu::size_t n;              // 节点数量
        
    public:
        /* 构造赋值 */

        /// 析构
        ~LinkList() noexcept { release(); }

        /// 默认构造
        LinkList() noexcept : lead{&lead, &lead}, n{0} {}

        /// 范围构造
        template <typename Rg, typename = xyu::t_enable<xyu::t_can_init<T, xyu::t_access<Rg>> && !xyu::t_can_init<T, Rg>>>
        explicit LinkList(Rg&& range) noexcept(noexcept(append(xyu::forward<Rg>(range)))) : LinkList{}
        {
            insert(park_iter(static_cast<__::LinkNode<T>*>(&lead)), xyu::forward<Rg>(range));
        }

        /// 复制构造
        LinkList(const LinkList& other) noexcept(noexcept(append(other))) : LinkList{}
        {
            insert(park_iter(static_cast<__::LinkNode<T>*>(&lead)), other.range());
        }

        /// 复制赋值
        LinkList& operator=(const LinkList& other) noexcept(xyu::t_can_nothrow_cpconstr<T>)
        {
            if (XY_UNLIKELY(this == &other)) return *this;
            if constexpr (xyu::t_can_nothrow_cpconstr<T> && xyu::t_can_nothrow_cpassign<T>)
            {
                // 复制一部分
                auto it = range().begin();
                auto it2 = other.range().begin();
                for (xyu::size_t i = 0, j = xyu::min(n, other.n); i < j; ++i, ++it, ++it2) *it = *it2;
                // 追加剩余元素
                if (other.n > n) append(xyu::make_range(other.n - n, it2, other.range().end()));
                // 释放多余内存
                else erase(it, n - other.n);
            }
            else
            {
                LinkList tmp = other;
                *this = xyu::move(tmp);
            }
            return *this;
        }

        /* 移动交换 */

        /// 移动构造
        LinkList(LinkList&& other) noexcept : n{other.n}, lead{other.n ? other.lead.next : &lead, other.n ? other.lead.prev : &lead}
        {
            lead.next->prev = lead.prev->next = &lead;
            other.lead.next = other.lead.prev = &other.lead;
            other.n = 0;
        }

        /// 移动赋值
        LinkList& operator=(LinkList&& other) noexcept
        {
            if (XY_UNLIKELY(this == &other)) return *this;
            release();
            if (!other.empty())
            {
                link_list(&lead, other.lead.next, other.lead.prev, other.count());
                other.lead.next = other.lead.prev = &other.lead;
                other.n = 0;
            }
            return *this;
        }

        /// 交换
        LinkList& swap(LinkList& other) noexcept
        {
            // 缓存 this
            xyu::size_t tmp = n;
            __::LinkNodeBase tmp_lead = lead;
            // this << other
            n = other.n;
            if (n == 0) lead.next = lead.prev = &lead;
            else {
                lead = other.lead;
                lead.next->prev = lead.prev->next = &lead;
            }
            // other << this
            other.n = tmp;
            if (tmp == 0) other.lead.next = other.lead.prev = &other.lead;
            else {
                other.lead = tmp_lead;
                tmp_lead.next->prev = tmp_lead.prev->next = &other.lead;
            }
            return *this;
        }

        /* 数据容量 */

        /// 获取最大容量
        constexpr static xyu::size_t limit() noexcept { return xyu::t_number<xyu::size_t>::max / 2; }
        
        /// 获取元素数量
        xyu::size_t count() const noexcept { return n; }
        /// 是否为空
        bool empty() const noexcept { return n == 0; }

        /* 数据获取 */

        /// 获取元素
        T& get(xyu::size_t index) noexcept
        {
            __::Checker::check_index(index, n, XY_CurPos("LinkList::get"));
            return get_node<false>(index)->val;
        }
        /// 获取元素 (const)
        xyu::t_add_const<T>& get(xyu::size_t index) const noexcept
        {
            __::Checker::check_index(index, n, XY_CurPos("LinkList::get"));
            return get_node<false>(index)->val;
        }

        /// 获取第一个元素
        T& head() noexcept
        {
            __::Checker::check_index(0, n, XY_CurPos("LinkList::head"));
            return static_cast<__::LinkNode<T>*>(lead.next)->val;
        }
        /// 获取第一个元素 (const)
        xyu::t_add_const<T>& head() const noexcept
        {
            __::Checker::check_index(0, n, XY_CurPos("LinkList::head"));
            return static_cast<const __::LinkNode<T>*>(lead.next)->val;
        }

        /// 获取最后一个元素
        T& tail() noexcept
        {
            __::Checker::check_index(n - 1, n, XY_CurPos("LinkList::tail"));
            return static_cast<__::LinkNode<T>*>(lead.prev)->val;
        }
        /// 获取最后一个元素 (const)
        xyu::t_add_const<T>& tail() const noexcept
        {
            __::Checker::check_index(n - 1, n, XY_CurPos("LinkList::tail"));
            return static_cast<const __::LinkNode<T>*>(lead.prev)->val;
        }

        /* 数据管理 */

        /// 释放内存
        void release() noexcept
        {
            if (XY_UNLIKELY(n == 0)) return;
            auto* node = static_cast<__::LinkNode<T>*>(lead.next);
            do {
                auto* next = static_cast<__::LinkNode<T>*>(node->next);
                node->~LinkNode();
                free_node(node);
                node = next;
            } while (node != &lead);
            lead.next = lead.prev = &lead;
            n = 0;
        }

        /* 尾增 */

        /**
         * @brief 尾部增加元素 (多元素)，返回第一个添加的元素位置
         * @details 将每一个 args 作为一个元素添加到尾部
         */
        template <typename... Args, typename = xyu::t_enable<sizeof...(Args) && (... && xyu::t_can_init<T, Args>)>,
                  bool nothrow_init = (... && xyu::t_can_nothrow_init<T, Args>)>
        Iter append(Args&&... args) noexcept(nothrow_init)
        {
            static_assert(sizeof...(Args) <= limit());
            auto prev = lead.prev;
            append_help<nothrow_init>(
                    sizeof...(Args),
                    [&](auto& node) { (..., (node = alloc_link_node(node, xyu::forward<Args, xyu::t_can_nothrow_init<T, Args>>(args)), ++n)); }
            );
            return Iter{static_cast<__::LinkNode<T>*>(prev->next)};
        }

        /**
         * @brief 尾部增加元素 (范围)，返回第一个添加的元素位置
         * @details 将 range 中的每一个元素添加到尾部
         */
        template <typename Rg, typename = xyu::t_enable<xyu::t_can_init<T, xyu::t_access<Rg>> && !xyu::t_can_init<T, Rg>>,
                  bool nothrow_init = xyu::t_can_nothrow_init<T, xyu::t_noexcept_access<Rg>>>
        Iter append(Rg&& rg) noexcept(nothrow_init)
        {
            auto range = xyu::make_range(xyu::forward<Rg>(rg));
            __::Checker::check_range(range, XY_CurPos("LinkList::append"));
            auto prev = lead.prev;
            append_help<nothrow_init>(
                    range.count(),
                    [&](auto& node) { for (auto&& v : range) node = alloc_link_node(node, xyu::forward<decltype(v), nothrow_init>(v)), ++n; }
            );
            return Iter{static_cast<__::LinkNode<T>*>(prev->next)};
        }

        /**
         * @brief 尾部增加元素 (单元素)，返回第一个添加的元素位置
         * @details 将 args 作为一个元素的构造参数添加到尾部
         */
        template <typename... Args, typename = xyu::t_enable<xyu::t_can_init<T, Args...>>,
                  bool nothrow_init = xyu::t_can_nothrow_init<T, Args...>>
        Iter append(xyu::special_t = {}, Args&&... args) noexcept(nothrow_init)
        {
            __::Checker::check_capa<limit()>(n, 1, XY_CurPos("LinkList::append"));
            lead.prev = alloc_link_node(lead.prev, xyu::forward<Args, nothrow_init>(args)...);
            ++n;
            return Iter{static_cast<__::LinkNode<T>*>(lead.prev)};
        }

        /// 尾部增加元素 (初始化列表)，返回第一个添加的元素位置
        Iter append(std::initializer_list<T> il) noexcept(noexcept(append(xyu::make_range(il))))
        { return append(xyu::make_range(il)); }

        /* 头增 */

        /**
         * @brief 头部增加元素 (多元素)，返回第一个添加的元素位置
         * @details 将每一个 args 作为一个元素添加到头部，保持参数顺序
         */
        template <typename... Args, typename = xyu::t_enable<sizeof...(Args) && (... && xyu::t_can_init<T, Args>)>,
                  bool nothrow_init = (... && xyu::t_can_nothrow_init<T, Args>)>
        Iter prepend(Args&&... args) noexcept(nothrow_init)
        {
            static_assert(sizeof...(Args) <= limit());
            auto prev = &lead;
            prepend_help<nothrow_init>(
                sizeof...(Args),
                [&](auto& node) { (..., (node = alloc_link_node(node, xyu::forward<Args, nothrow_init>(args)), ++n)); }
            );
            return Iter{static_cast<__::LinkNode<T>*>(prev->next)};
        }

        /**
         * @brief 头部增加元素 (范围)，返回第一个添加的元素位置
         * @details 将 range 中的每一个元素添加到头部，保持原有顺序
         */
        template <typename Rg, typename = xyu::t_enable<xyu::t_can_init<T, xyu::t_access<Rg>> && !xyu::t_can_init<T, Rg>>,
                  bool nothrow_init = xyu::t_can_nothrow_init<T, xyu::t_noexcept_access<Rg>>>
        Iter prepend(Rg&& rg) noexcept(nothrow_init)
        {
            auto range = xyu::make_range(xyu::forward<Rg>(rg));
            __::Checker::check_range(range, XY_CurPos("LinkList::prepend"));
            auto prev = &lead;
            prepend_help<nothrow_init>(
                range.count(),
                [&](auto& node) { for (auto&& v : range) node = alloc_link_node(node, xyu::forward<decltype(v), nothrow_init>(v)), ++n; }
            );
            return Iter{static_cast<__::LinkNode<T>*>(prev->next)};
        }

        /**
         * @brief 头部增加元素 (单元素构造)，返回新元素的位置
         * @details 将 args 作为一个元素的构造参数添加到头部
         */
        template <typename... Args, typename = xyu::t_enable<xyu::t_can_init<T, Args...>>,
                  bool nothrow_init = xyu::t_can_nothrow_init<T, Args...>>
        Iter prepend(xyu::special_t = {}, Args&&... args) noexcept(nothrow_init)
        {
            __::Checker::check_capa<limit()>(n, 1, XY_CurPos("LinkList::prepend"));
            auto* newn = alloc_link_node(&lead, xyu::forward<Args, nothrow_init>(args)...);
            newn->next->prev = newn;
            ++n;
            return Iter{static_cast<__::LinkNode<T>*>(newn)};
        }

        /// 头部增加元素 (初始化列表)，返回第一个添加的元素位置
        Iter prepend(std::initializer_list<T> il) noexcept(noexcept(prepend(xyu::make_range(il))))
        { return prepend(xyu::make_range(il)); }

        /* 插入 */

        /**
         * @brief 中间插入元素 (多元素)，返回原位置的新迭代器
         * @details 将每一个 args 作为一个元素依次插入到 index 位置 (若 index 越界，则在尾部插入)
         */
        template <typename... Args, typename = xyu::t_enable<sizeof...(Args) && (... && xyu::t_can_init<T, Args>)>>
        Iter insert(xyu::size_t index, Args&&... args) noexcept((... && xyu::t_can_nothrow_init<T, Args>))
        { return insert(park_iter(get_node(index)), xyu::forward<Args>(args)...); }

        /**
         * @brief 中间插入元素 (范围)，返回原位置的新迭代器
         * @details 将 range 中的每一个元素依次插入到 index 位置 (若 index 越界，则在尾部插入)
         */
        template <typename Rg, typename = xyu::t_enable<xyu::t_can_init<T, xyu::t_access<Rg>> && !xyu::t_can_init<T, Rg>>>
        Iter insert(xyu::size_t index, Rg&& rg) noexcept(xyu::t_can_nothrow_init<T, xyu::t_noexcept_access<Rg>>)
        { return insert(park_iter(get_node(index)), xyu::forward<Rg>(rg)); }

        /**
         * @brief 中间插入元素 (单元素)，返回原位置的新迭代器
         * @details 将 args 作为一个元素的构造参数依次插入到 index 位置 (若 index 越界，则在尾部插入)
         */
        template <typename... Args, typename = xyu::t_enable<xyu::t_can_init<T, Args...>>>
        Iter insert(xyu::size_t index, xyu::special_t = {}, Args&&... args) noexcept(xyu::t_can_nothrow_init<T, Args...>)
        { return insert(park_iter(get_node(index)), xyu::special_v, xyu::forward<Args>(args)...); }

        /// 中间插入元素 (初始化列表)，返回原位置的新迭代器
        Iter insert(xyu::size_t index, std::initializer_list<T> il) noexcept(noexcept(insert(index, xyu::make_range(il))))
        { return insert(index, xyu::make_range(il)); }

        /* 删除 */

        /// 删除索引开始的 count(默认为1) 个元素，并返回下一个位置的迭代器
        Iter erase(xyu::size_t index, xyu::size_t count = 1) noexcept
        {
            if (XY_UNLIKELY(index >= n || count == 0)) return Iter{static_cast<__::LinkNode<T>*>(&lead)};
            if (n - index < count) count = n - index;
            auto* prev = get_node<false>(index)->prev;
            remove_node<true>(prev, count);
            return Iter{static_cast<__::LinkNode<T>*>(prev->next)};
        }

        /* 迭代器 - 插入 */
        
        /**
        * @brief 迭代器之前插入元素 (多元素)，并返回原位置的新迭代器
        * @details 将每一个 args 作为一个元素依次插入到迭代器之前
        */
        template <typename It, typename... Args, typename = xyu::t_enable<xyu::t_is_same<LinkList, typename It::Owner_t> && sizeof...(Args) && (... && xyu::t_can_init<T, Args>)>,
                  bool nothrow_init = (... && xyu::t_can_nothrow_init<T, Args>)>
        Iter insert(It it, Args&&... args) noexcept(nothrow_init)
        {
            static_assert(sizeof...(args) <= limit());
            check_owned(it, __LINE__, "LinkList::insert");
            auto prev = it.p->prev;
            insert_help<nothrow_init>(
                    sizeof...(Args), prev,
                    [&](auto& node){ (..., (node = alloc_link_node(node, xyu::forward<Args, nothrow_init>(args)))); }
            );
            return Iter{static_cast<__::LinkNode<T>*>(prev->next)};
        }

        /**
         * @brief 迭代器之前插入元素 (范围)，并返回原位置的新迭代器
         * @details 将 range 中的每一个元素依次插入到迭代器之前
         */
        template <typename It, typename Rg, typename = xyu::t_enable<xyu::t_is_same<LinkList, typename It::Owner_t> && xyu::t_can_init<T, xyu::t_access<Rg>> && !xyu::t_can_init<T, Rg>>,
                  bool nothrow_init = xyu::t_can_nothrow_init<T, xyu::t_noexcept_access<Rg>>>
        Iter insert(It it, Rg&& rg) noexcept(nothrow_init)
        {
            auto range = xyu::make_range(xyu::forward<Rg>(rg));
            check_owned(it, __LINE__, "LinkList::insert");
            __::Checker::check_range(range, XY_CurPos("LinkList::insert"));
            auto prev = it.p->prev;
            insert_help<nothrow_init>(
                    range.count(), prev,
                    [&](auto& node){ for (auto&& v : range) node = alloc_link_node(node, xyu::forward<decltype(v), nothrow_init>(v)); }
            );
            return Iter{static_cast<__::LinkNode<T>*>(prev->next)};
        }

        /**
         * @brief 迭代器之前插入元素 (单元素)，并返回原位置的新迭代器
         * @details 将 args 作为一个元素的构造参数依次插入到迭代器之前
         */
        template <typename It, typename... Args, typename = xyu::t_enable<xyu::t_is_same<LinkList, typename It::Owner_t> && xyu::t_can_init<T, Args...>>,
                  bool nothrow_init = xyu::t_can_nothrow_init<T, Args...>>
        Iter insert(It it, xyu::special_t = {}, Args&&... args) noexcept(nothrow_init)
        {
            check_owned(it, __LINE__, "LinkList::insert");
            __::Checker::check_capa<limit()>(n, 1, XY_CurPos("LinkList::insert"));
            it.p->prev = alloc_link_node(it.p->prev, xyu::forward<Args, nothrow_init>(args)...);
            ++n;
            return Iter{static_cast<__::LinkNode<T>*>(it.p->prev)};
        }

        /// 迭代器之前插入元素 (初始化列表)，并返回原位置的新迭代器
        template <typename It, typename = xyu::t_enable<xyu::t_is_same<LinkList, typename It::Owner_t>>>
        Iter insert(It it, std::initializer_list<T> il) noexcept(noexcept(insert(it, xyu::make_range(il))))
        { return insert(it, xyu::make_range(il)); }

        /* 迭代器 - 删除 */

        /// 删除迭代器位置，并返回下一个位置的迭代器
        template <typename It, typename = xyu::t_enable<xyu::t_is_same<LinkList, typename It::Owner_t>>>
        Iter erase(It it, xyu::size_t count = 1) noexcept
        {
            check_owned(it, __LINE__, "LinkList::erase");
            auto* prev = it.p->prev;
            remove_node<false>(prev, count);
            return Iter{static_cast<__::LinkNode<T>*>(prev->next)};
        }

        /* 转移 - 移出 */

        /// 移出索引位置节点
        HeapVal<__::LinkNode<T>, cross_thread> take(xyu::size_t index) noexcept
        {
            __::Checker::check_index(index, n, XY_CurPos("LinkList::take"));
            return take(park_iter(get_node<false>(index)));
        }

        /// 移出迭代器位置节点
        template <typename It, typename = xyu::t_enable<xyu::t_is_same<LinkList, typename It::Owner_t>>>
        HeapVal<__::LinkNode<T>, cross_thread> take(It it) noexcept
        {
            check_owned<true>(it, __LINE__, "LinkList::take");
            auto node = it.p;
            node->prev->next = node->next;
            node->next->prev = node->prev;
            --n;
            return xyu::HeapVal<__::LinkNode<T>>{xyu::special_v, node};
        }

        /**
         * @brief 移出索引范围节点
         * @param index 起始索引，若越界则返回空链表
         * @param count 移动数量，若越界则为后续全部节点
         */
        LinkList take(xyu::size_t index, xyu::size_t count) noexcept
        {
            if (XY_UNLIKELY(count == 0 || index >= n)) return LinkList{};
            __::LinkNode<T>* on;
            if (n - index < count) { count = n - index; on = static_cast<__::LinkNode<T>*>(&lead); }
            else on = get_node<false>(index + count);
            return take(park_iter(get_node<false>(index)), park_iter(on), count);
        }

        /**
         * @brief 移出索引范围节点
         * @param start 起始迭代器位置，若为超尾位置则返回空链表
         * @param count 移动数量，若越界则为后续全部节点
         */
        template <typename It, typename = xyu::t_enable<xyu::t_is_same<LinkList, typename It::Owner_t>>>
        LinkList take(It start, xyu::size_t count) noexcept
        {
            check_owned(start, __LINE__, "LinkList::take");
            auto over = start.p;
            for (xyu::size_t i = 0; i < count; ++i)
            {
                if (over == &lead) { count = i; break; }
                over = static_cast<__::LinkNode<T>*>(over->next);
            }
            return take(park_iter(start.p), park_iter(over), count);
        }

        /**
         * @brief 移出索引范围节点
         * @param start 起始迭代器位置，若为超尾位置则返回空链表
         * @param over 结束迭代器位置（不包括），必须在 start 之后位置
         * @param count 移动数量，若不提供则自动计算
         */
        template <typename It, typename = xyu::t_enable<xyu::t_is_same<LinkList, typename It::Owner_t>>>
        LinkList take(It start, It over, xyu::size_t count = -1) noexcept
        {
            if (check_iters(start, over, count, __LINE__, "LinkList::take")) return LinkList{};
            auto ln = over.p->prev;
            start.p->prev->next = over.p;
            over.p->prev = start.p->prev;
            n -= count;
            LinkList ret;
            ret.lead = __::LinkNodeBase{start.p, ln};
            start.p->prev = ln->next = &ret.lead;
            ret.n = count;
            return ret;
        }

        /* 转移 - 移入 */

        /// 将节点移入索引位置，并返回新迭代器
        Iter put(xyu::size_t index, HeapVal<__::LinkNode<T>, cross_thread>&& node) noexcept
        {
            return put(park_iter(get_node(index)), xyu::move(node));
        }

        /// 将节点移入迭代器位置，并返回新迭代器
        template <typename It, typename = xyu::t_enable<xyu::t_is_same<LinkList, typename It::Owner_t>>>
        Iter put(It it, HeapVal<__::LinkNode<T>, cross_thread>&& node) noexcept
        {
            check_owned(it, __LINE__, "LinkList::put");
            auto newn = node.take();
            newn->next = it.p;
            newn->prev = it.p->prev;
            it.p->prev->next = newn;
            it.p->prev = newn;
            ++n;
            return Iter{newn};
        }

        /// 将链表移入索引位置，并返回原位置的新迭代器 (若 index 越界，则在尾部插入)
        Iter put(xyu::size_t index, LinkList&& list) noexcept { return put(park_iter(get_node(index)), xyu::move(list)); }

        /// 将链表移入迭代器位置，并返回原位置的新迭代器 (若 index 越界，则在尾部插入)
        template <typename It, typename = xyu::t_enable<xyu::t_is_same<LinkList, typename It::Owner_t>>>
        Iter put(It it, LinkList&& list) noexcept
        {
            check_owned(it, __LINE__, "LinkList::put");
            if (XY_UNLIKELY(&list == this)) return insert(it, list.range());
            if (list.empty()) return Iter{it.p};
            auto* prev = it.p->prev;
            link_list(prev, list.lead.next, list.lead.prev, list.count());
            list.lead = __::LinkNodeBase{&list.lead, &list.lead};
            list.n = 0;
            return Iter{static_cast<__::LinkNode<T>*>(prev->next)};
        }

        /* 范围 */

        /// 创建范围
        auto range() noexcept { return xyu::Range<Iter>(n, static_cast<__::LinkNode<T>*>(lead.next), static_cast<__::LinkNode<T>*>(&lead)); }
        /// 创建范围 (const)
        auto range() const noexcept { return const_cast<LinkList*>(this)->range().crange(); }

        /// 创建反向范围
        auto rrange() noexcept { return xyu::Range<rIter>(n, static_cast<__::LinkNode<T>*>(lead.prev), static_cast<__::LinkNode<T>*>(&lead)); }
        /// 创建反向范围 (const)
        auto rrange() const noexcept { return const_cast<LinkList*>(this)->rrange().crange(); }

        /* 运算符 */

        /// 索引访问
        T& operator[](xyu::size_t index) noexcept { return get(index); }
        /// 索引访问 (const)
        xyu::t_add_const<T>& operator[](xyu::size_t index) const noexcept { return get(index); }

        /// 尾部添加元素
        template <typename Arg>
        LinkList& operator<<(Arg&& arg) noexcept(noexcept(append(xyu::forward<Arg>(arg))))
        { append(xyu::forward<Arg>(arg)); return *this; }
        /// 头部添加元素
        template <typename Arg>
        friend LinkList& operator>>(Arg&& arg, LinkList& list) noexcept(noexcept(list.prepend(xyu::forward<Arg>(arg))))
        { list.prepend(xyu::forward<Arg>(arg)); return list; }

    private:
        // 生成标志迭代器
        struct self_iter_tag {};
        static auto park_iter(__::LinkNode<T>* node) noexcept
        {
            struct SIter : Iter, self_iter_tag { using Iter::Iter; };
            return SIter{node};
        }
        // 检查是否为标志迭代器
        template <typename It>
        static constexpr bool check_park_iter(It) noexcept { return xyu::t_is_base<self_iter_tag, It>; }
        // 检查所属
        template <bool check_lead = false, typename It>
        void check_owned(It&& iter, int line, const char* func) noexcept
        {
            if constexpr (!XY_RELEASE && check_lead) if (XY_UNLIKELY(iter.p == &lead))
                xyloge2("can't change lead node", line, func);
            if constexpr (!xyu::t_is_base<self_iter_tag, It> && XY_DEBUG)
            {
                const __::LinkNodeBase* node = iter.p;
                __::LinkNodeBase* cur = lead.next;
                for(;;)
                {
                    if (cur == node) break;
                    if (cur == &lead)
                    {
                        xyloge2("LinkNode doesn't belong to this LinkList", line, func);
                        xyu::abort();
                    }
                    cur = cur->next;
                }
            }
        }
        // 检查迭代器范围
        template <typename It>
        bool check_iters(It&& start, It&& over, xyu::size_t& count, int line, const char* func) noexcept
        {
            if constexpr (!xyu::t_is_base<self_iter_tag, It>)
            {
                if constexpr (!XY_RELEASE)
                {
                    const __::LinkNodeBase *sn = start.p, *on = over.p;
                    __::LinkNodeBase* cur = lead.next;
                    xyu::size_t i = 0, j = 0;
                    bool sf = false, of = false;
                    for(;;)
                    {
                        if (!sf)
                        {
                            if (sn == cur) { sf = true; if (of) break; }
                            else ++i;
                        }
                        if (!of)
                        {
                            if (on == cur) { of = true; if (sf) break; }
                            else ++j;
                        }
                        if (cur == &lead)
                        {
                            if (!sf) xyloge2("LinkNode start doesn't belong to this LinkList", line, func);
                            if (!of) xyloge2("LinkNode over doesn't belong to this LinkList", line, func);
                            xyu::abort();
                        }
                        cur = cur->next;
                    }
                    if (XY_UNLIKELY(i > j))
                    {
                        xyloge2("LinkNode start can't larger than over", line, func);
                        xyu::abort();
                    }
                    if (count == -1) count = j - i;
                    else if (count != j - i)
                    {
                        xyloge2("LinkNode [{},{}) dosen't match with {}", line, func, i, j, count);
                        xyu::abort();
                    }
                }
                else if (count == -1)
                {
                    count = 0;
                    __::LinkNodeBase *sn = start.p, *on = over.p;
                    while (sn != on) { sn = sn->next; ++count; }
                }
            }
            if (XY_UNLIKELY(count == 0)) return true;
            return false;
        }

        // 获取当前节点
        template <bool check = true>
        __::LinkNode<T>* get_node(xyu::size_t index) const noexcept
        {
            auto* node = const_cast<__::LinkNodeBase*>(&lead);
            if constexpr (check)
                if (XY_UNLIKELY(index >= n)) return static_cast<__::LinkNode<T>*>(node);
            if (index < (n + 1) / 2) do node = node->next; while (index-- > 0);
            else do node = node->prev; while (++index < n);
            return static_cast<__::LinkNode<T>*>(node);
        }

        // 创建新节点
        template <typename... Args>
        static __::LinkNodeBase* alloc_node(__::LinkNodeBase* prev, Args&&... args) noexcept(xyu::t_can_nothrow_init<T, Args...>)
        {
            __::LinkNode<T>* node = __::Common::alloc<__::LinkNode<T>, cross_thread>();
            if constexpr (xyu::t_can_nothrow_init<T, Args...>)
                ::new (node) __::LinkNode<T>(prev->next, prev, xyu::forward<Args>(args)...);
            else {
                try { ::new (node) __::LinkNode<T>(prev->next, prev, xyu::forward<Args>(args)...); }
                catch (...) { free_node(node); throw; }
            }
            return node;
        }
        // 释放节点
        static void free_node(__::LinkNode<T>* ptr) noexcept { __::Common::dealloc<cross_thread>(ptr); }

        // 创建新节点并单向连接
        template <typename... Args>
        static __::LinkNodeBase* alloc_link_node(__::LinkNodeBase* prev, Args&&... args) noexcept(noexcept(alloc_node(prev, xyu::forward<Args>(args)...)))
        {
            auto* node = alloc_node(prev, xyu::forward<Args>(args)...);
            prev->next = node;
            return node;
        }

        // 将节点链接入链表
        void link_list(__::LinkNodeBase* prev, __::LinkNodeBase* start, __::LinkNodeBase* last, xyu::size_t add) noexcept
        {
            start->prev = prev;
            last->next = prev->next;
            prev->next->prev = last;
            prev->next = start;
            n += add;
        }

        // 删除节点
        template <bool count_safe>
        void remove_node(__::LinkNodeBase* prev, xyu::size_t count) noexcept
        {
            for (xyu::size_t i = 0; i < count; ++i)
            {
                auto* node = static_cast<__::LinkNode<T>*>(prev->next);
                if constexpr (!count_safe) if (node == &lead) break;
                node->next->prev = prev;
                prev->next = node->next;
                node->~LinkNode();
                free_node(node);
                --n;
            }
        }

        // append 辅助
        template <bool nothrow_init, typename AppFun>
        void append_help(xyu::size_t add, AppFun&& af)
        {
            __::Checker::check_capa<limit()>(n, add, XY_CurPos("LinkList::append"));
            __::LinkNodeBase* node = lead.prev;
            if constexpr (nothrow_init) af(node);
            else try { af(node); }
                catch (...) {
                    lead.prev = node;
                    throw;
                }
            lead.prev = node;
        }

        // prepend 辅助
        template <bool nothrow_init, typename PreFun>
        void prepend_help(xyu::size_t add, PreFun&& pf)
        {
            __::Checker::check_capa<limit()>(n, add, XY_CurPos("LinkList::prepend"));
            __::LinkNodeBase* node = &lead;
            if constexpr (nothrow_init) pf(node);
            else try { pf(node); }
                catch (...) {
                    node->next->prev = node;
                    throw;
                }
            node->next->prev = node;
        }

        // insert 辅助
        template <bool nothrow_init, typename AddFun>
        void insert_help(xyu::size_t add, __::LinkNodeBase* prev, AddFun&& af)
        {
            __::Checker::check_capa<limit()>(n, add, XY_CurPos("LinkList::insert"));
            if (XY_UNLIKELY(add == 0)) return;
            __::LinkNodeBase head{&head,&head}, *node = &head;
            if constexpr (nothrow_init) af(node);
            else try { af(node); }
                catch (...) {
                    auto* cur = static_cast<__::LinkNode<T>*>(head.next);
                    while (cur != &head)
                    {
                        auto* next = static_cast<__::LinkNode<T>*>(cur->next);
                        cur->~LinkNode();
                        free_node(cur);
                        cur = next;
                    }
                    throw;
                }
            link_list(prev, head.next, node, add);
        }
    };

    // 推导指引
    template <typename Rg, typename = xyu::t_enable<xyu::t_can_range<Rg>>>
    LinkList(Rg&&) -> LinkList<xyu::t_rem_cr<xyu::t_access<Rg>>>;
}
