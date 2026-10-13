#pragma once

/**
 * @file    xycoll/slinklist.h
 * @brief   单向链表
 * @date    2026-06-16
 */

#include "./heapval.h"

namespace xylu::xycoll
{
    namespace __
    {
        /// 链表节点基类
        struct SLinkNodeBase
        {
            SLinkNodeBase* next;    // 下一个节点
        };

        /// 链表哨兵节点
        struct SLinkLeadNode : SLinkNodeBase
        {
            SLinkNodeBase* tail;    // 尾节点
        };

        /// 链表节点类
        template <typename T>
        struct SLinkNode : SLinkNodeBase
        {
            T val;    // 数据

            template <typename... Args, typename Test = T, xyu::t_enable<xyu::t_is_aggregate<Test>, bool> = true>
            explicit SLinkNode(SLinkNodeBase* next, Args&&... args) noexcept(xyu::t_can_nothrow_listinit<T, Args...>)
                    : SLinkNodeBase{next}, val{xyu::forward<Args>(args)...} {}

            template <typename... Args, typename Test = T, xyu::t_enable<!xyu::t_is_aggregate<Test>, bool> = false>
            explicit SLinkNode(SLinkNodeBase* next, Args&&... args) noexcept(xyu::t_can_nothrow_construct<T, Args...>)
                    : SLinkNodeBase{next}, val(xyu::forward<Args>(args)...) {}
        };
    }

    /**
     * @brief 单向链表
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
     *   - 支持 `get`O(n)、`head`O(1)、`tail`O(1) 访问元素
     *   - 支持 `append`O(1)、`prepend`O(1) 在头尾追加 多元素/范围/单元素
     *   - 支持索引操作：`insert` 插入、`erase` 删除、`take` 移出，`put` 移入，均为 O(n)
     *   - 支持迭代器操作：`insert_after` 插入、`erase_after` 删除、`take_after` 移出、`put_after` 移入，均为 O(1)
     *   - 上述两种操作均会返回第一个操作位置的前驱迭代器
     *
     *   注意事项：
     *   - 容量限制：增加元素超过 `limit` 时会触发 abort
     *   - 范围限制：传入非有限范围在 !XY_RELEASE 下检查并触发 abort
     *   - 索引越界：`get`、`op[]`、`head`、`tail`、`take`单节点定位 会在 !XY_RELEASE 下检查并触发 abort，其余方法无害
     *   - 迭代器所属：迭代器操作传入的非所属迭代器时 会在 XY_DEBUG 下检查归属并触发 abort
     *   - 哨兵保护：`erase_after`、`take_after` 传入尾节点 会在 !XY_RELEASE 下检查并触发 abort
     *   - 头部操作：`xx_after` 系列传入 哨兵迭代器 时会进行头部操作
     *
     *   异常安全：
     *   - 复制赋值、`insert`、`insert_after`：抛出异常时回滚至操作前状态
     *   - `append`、`prepend`：若元素构造抛出异常，已成功构造的元素保留，需要手动回滚或继续尝试
     */
    template <typename T, bool cross_thread = false>
    class SLinkList
    {
        static_assert(xyu::t_can_nothrow_destruct<T>, "SLinkList don't support C4");

        using Iter = xyu::Iterator_Node<__::SLinkNode<T>, SLinkList>;
    private:
        __::SLinkLeadNode lead;     // 哨兵节点
        xyu::size_t n;              // 节点数量
        
    public:
        /* 构造赋值 */

        /// 析构
        ~SLinkList() noexcept { release(); }

        /// 默认构造
        SLinkList() noexcept : lead{&lead,&lead}, n{0} {}

        /// 范围构造
        template <typename Rg, typename = xyu::t_enable<xyu::t_can_init<T, xyu::t_access<Rg>> && !xyu::t_can_init<T, Rg>>>
        explicit SLinkList(Rg&& range) noexcept(noexcept(append(xyu::forward<Rg>(range)))) : SLinkList()
        {
            insert_after(park_iter((__::SLinkNode<T>*)&lead), xyu::forward<Rg>(range));
        }

        /// 复制构造
        SLinkList(const SLinkList& other) noexcept(noexcept(append(other))) : SLinkList{}
        {
            insert_after(park_iter((__::SLinkNode<T>*)&lead), other);
        }

        /// 复制赋值
        SLinkList& operator=(const SLinkList& other) noexcept(xyu::t_can_nothrow_cpconstr<T>)
        {
            if (XY_UNLIKELY(this == &other)) return *this;
            if constexpr (xyu::t_can_nothrow_cpconstr<T> && xyu::t_can_nothrow_cpassign<T>)
            {
                // 复制一部分
                auto it = Iter{(__::SLinkNode<T>*)&lead};
                auto it2 = other.range().begin();
                for (xyu::size_t i = 0, j = xyu::min(n, other.n); i < j; ++i, ++it2) *++it = *it2;
                // 追加剩余元素
                if (other.n > n) append(xyu::make_range(other.n - n, it2, other.range().end()));
                // 释放多余内存
                else erase_after(it, n - other.n);
            }
            else
            {
                SLinkList tmp = other;
                *this = xyu::move(tmp);
            }
            return *this;
        }

        /* 移动交换 */

        /// 移动构造
        SLinkList(SLinkList&& other) noexcept : n{other.n}, lead{other.n ? other.lead.next : &lead, other.n ? other.lead.tail : &lead}
        {
            lead.tail->next = &lead;
            other.lead.next = other.lead.tail = &other.lead;
            other.n = 0;
        }

        /// 移动赋值
        SLinkList& operator=(SLinkList&& other) noexcept
        {
            if (XY_UNLIKELY(this == &other)) return *this;
            release();
            if (!other.empty())
            {
                link_list(&lead, other.lead.next, other.lead.tail, other.count());
                other.lead.next = other.lead.tail = &other.lead;
                other.n = 0;
            }
            return *this;
        }

        /// 交换
        SLinkList& swap(SLinkList& other) noexcept
        {
            // 缓存 this
            xyu::size_t tmp = n;
            __::SLinkLeadNode tmp_lead = lead;
            // this << other
            n = other.n;
            if (n == 0) lead.next = lead.tail = &lead;
            else {
                lead = other.lead;
                lead.tail->next = &lead;
            }
            // other << this
            other.n = tmp;
            if (tmp == 0) other.lead.next = other.lead.tail = &other.lead;
            else {
                other.lead = tmp_lead;
                tmp_lead.tail->next = &other.lead;
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
            __::Checker::check_index(index, n, XY_CurPos("SLinkList::get"));
            return static_cast<__::SLinkNode<T>*>(get_prev_node<false>(index)->next)->val;
        }
        /// 获取元素 (const)
        xyu::t_add_const<T>& get(xyu::size_t index) const noexcept
        {
            __::Checker::check_index(index, n, XY_CurPos("SLinkList::get"));
            return static_cast<__::SLinkNode<T>*>(get_prev_node<false>(index)->next)->val;
        }

        /// 获取第一个元素
        T& head() noexcept
        {
            __::Checker::check_index(0, n, XY_CurPos("SLinkList::head"));
            return static_cast<__::SLinkNode<T>*>(lead.next)->val;
        }
        /// 获取第一个元素 (const)
        xyu::t_add_const<T>& head() const noexcept
        {
            __::Checker::check_index(0, n, XY_CurPos("SLinkList::head"));
            return static_cast<const __::SLinkNode<T>*>(lead.next)->val;
        }

        /// 获取最后一个元素
        T& tail() noexcept
        {
            __::Checker::check_index(n - 1, n, XY_CurPos("SLinkList::tail"));
            return static_cast<__::SLinkNode<T>*>(lead.tail)->val;
        }
        /// 获取最后一个元素 (const)
        xyu::t_add_const<T>& tail() const noexcept
        {
            __::Checker::check_index(n - 1, n, XY_CurPos("SLinkList::tail"));
            return static_cast<const __::SLinkNode<T>*>(lead.tail)->val;
        }

        /* 数据管理 */

        /// 释放内存
        void release() noexcept
        {
            if (XY_UNLIKELY(n == 0)) return;
            auto* node = lead.next;
            do {
                auto* next = node->next;
                static_cast<__::SLinkNode<T>*>(node)->~SLinkNode();
                free_node(static_cast<__::SLinkNode<T>*>(node));
                node = next;
            } while (node != &lead);
            lead.next = lead.tail = &lead;
            n = 0;
        }
        
        /* 尾增 */

        /**
         * @brief 尾部增加元素 (多元素)，返回旧尾部迭代器
         * @details 将每一个 args 作为一个元素添加到尾部
         */
        template <typename... Args, typename = xyu::t_enable<sizeof...(Args) && (... && xyu::t_can_init<T, Args>)>,
                  bool nothrow_init = (... && xyu::t_can_nothrow_init<T, Args>)>
        Iter append(Args&&... args) noexcept(nothrow_init)
        {
            static_assert(sizeof...(Args) <= limit());
            auto prev = lead.tail;
            append_help<nothrow_init>(
                    sizeof...(Args),
                    [&](auto& node) { (..., (node = alloc_link_node(node, xyu::forward<Args, xyu::t_can_nothrow_init<T, Args>>(args)), ++n)); }
            );
            return Iter{static_cast<__::SLinkNode<T>*>(prev)};
        }

        /**
         * @brief 尾部增加元素 (范围)，返回旧尾部迭代器
         * @details 将 range 中的每一个元素添加到尾部
         */
        template <typename Rg, typename = xyu::t_enable<xyu::t_can_init<T, xyu::t_access<Rg>> && !xyu::t_can_init<T, Rg>>,
                  bool nothrow_init = xyu::t_can_nothrow_init<T, xyu::t_noexcept_access<Rg>>>
        Iter append(Rg&& rg) noexcept(nothrow_init)
        {
            auto range = xyu::make_range(xyu::forward<Rg>(rg));
            __::Checker::check_range(range, XY_CurPos("SLinkList::append"));
            auto prev = lead.tail;
            append_help<nothrow_init>(
                    range.count(),
                    [&](auto& node) { for (auto&& v : range) node = alloc_link_node(node, xyu::forward<decltype(v), nothrow_init>(v)), ++n; }
            );
            return Iter{static_cast<__::SLinkNode<T>*>(prev)};
        }

        /**
         * @brief 尾部增加元素 (单元素)，返回旧尾部迭代器
         * @details 将 args 作为一个元素的构造参数添加到尾部
         */
        template <typename... Args, typename = xyu::t_enable<xyu::t_can_init<T, Args...>>,
                  bool nothrow_init = xyu::t_can_nothrow_init<T, Args...>>
        Iter append(xyu::special_t = {}, Args&&... args) noexcept(nothrow_init)
        {
            __::Checker::check_capa<limit()>(n, 1, XY_CurPos("SLinkList::append"));
            auto prev = lead.tail;
            lead.tail = alloc_link_node(prev, xyu::forward<Args, nothrow_init>(args)...);
            ++n;
            return Iter{static_cast<__::SLinkNode<T>*>(prev)};
        }

        /// 尾部增加元素 (初始化列表)，返回旧尾部迭代器
        Iter append(std::initializer_list<T> il) noexcept(noexcept(append(xyu::make_range(il))))
        { return append(xyu::make_range(il)); }

        /* 头增 */

        /**
         * @brief 头部增加元素 (多元素)，返回哨兵迭代器
         * @details 将每一个 args 作为一个元素添加到头部，保持参数顺序
         */
        template <typename... Args, typename = xyu::t_enable<sizeof...(Args) && (... && xyu::t_can_init<T, Args>)>,
                  bool nothrow_init = (... && xyu::t_can_nothrow_init<T, Args>)>
        Iter prepend(Args&&... args) noexcept(nothrow_init)
        {
            static_assert(sizeof...(Args) <= limit());
            prepend_help<nothrow_init>(
                sizeof...(Args),
                [&](auto& node) { (..., (node = alloc_link_node(node, xyu::forward<Args, nothrow_init>(args)), ++n)); }
            );
            return Iter{(__::SLinkNode<T>*)&lead};
        }

        /**
         * @brief 头部增加元素 (范围)，返回旧哨兵迭代器
         * @details 将 range 中的每一个元素添加到头部，保持原有顺序
         */
        template <typename Rg, typename = xyu::t_enable<xyu::t_can_init<T, xyu::t_access<Rg>> && !xyu::t_can_init<T, Rg>>,
                  bool nothrow_init = xyu::t_can_nothrow_init<T, xyu::t_noexcept_access<Rg>>>
        Iter prepend(Rg&& rg) noexcept(nothrow_init)
        {
            auto range = xyu::make_range(xyu::forward<Rg>(rg));
            __::Checker::check_range(range, XY_CurPos("SLinkList::prepend"));
            prepend_help<nothrow_init>(
                range.count(),
                [&](auto& node) { for (auto&& v : range) node = alloc_link_node(node, xyu::forward<decltype(v), nothrow_init>(v)), ++n; }
            );
            return Iter{(__::SLinkNode<T>*)&lead};
        }

        /**
         * @brief 头部增加元素 (单元素构造)，返回哨兵迭代器
         * @details 将 args 作为一个元素的构造参数添加到头部
         */
        template <typename... Args, typename = xyu::t_enable<xyu::t_can_init<T, Args...>>,
                  bool nothrow_init = xyu::t_can_nothrow_init<T, Args...>>
        Iter prepend(xyu::special_t = {}, Args&&... args) noexcept(nothrow_init)
        {
            __::Checker::check_capa<limit()>(n, 1, XY_CurPos("SLinkList::prepend"));
            bool retail = empty();
            auto* newn = alloc_link_node(&lead, xyu::forward<Args, nothrow_init>(args)...);
            if (retail) lead.tail = newn;
            ++n;
            return Iter{(__::SLinkNode<T>*)&lead};
        }

        /// 头部增加元素 (初始化列表)，返回哨兵迭代器
        Iter prepend(std::initializer_list<T> il) noexcept(noexcept(prepend(xyu::make_range(il))))
        { return prepend(xyu::make_range(il)); }

        /* 插入 */

        /**
         * @brief 中间插入元素 (多元素)，返回插入位置的前一个位置的迭代器
         * @details 将每一个 args 作为一个元素依次插入到 index 位置 (若 index 越界，则在尾部插入)
         */
        template <typename... Args, typename = xyu::t_enable<sizeof...(Args) && (... && xyu::t_can_init<T, Args>)>>
        Iter insert(xyu::size_t index, Args&&... args) noexcept((... && xyu::t_can_nothrow_init<T, Args>))
        { return insert_after(park_iter(get_prev_node(index)), xyu::forward<Args>(args)...); }

        /**
         * @brief 中间插入元素 (范围)，返回插入位置的前一个位置的迭代器
         * @details 将 range 中的每一个元素依次插入到 index 位置 (若 index 越界，则在尾部插入)
         */
        template <typename Rg, typename = xyu::t_enable<xyu::t_can_init<T, xyu::t_access<Rg>> && !xyu::t_can_init<T, Rg>>>
        Iter insert(xyu::size_t index, Rg&& rg) noexcept(xyu::t_can_nothrow_init<T, xyu::t_noexcept_access<Rg>>)
        { return insert_after(park_iter(get_prev_node(index)), xyu::forward<Rg>(rg)); }

        /**
         * @brief 中间插入元素 (单元素)，返回插入位置的前一个位置的迭代器
         * @details 将 args 作为一个元素的构造参数依次插入到 index 位置 (若 index 越界，则在尾部插入)
         */
        template <typename... Args, typename = xyu::t_enable<xyu::t_can_init<T, Args...>>>
        Iter insert(xyu::size_t index, xyu::special_t = {}, Args&&... args) noexcept(xyu::t_can_nothrow_init<T, Args...>)
        { return insert_after(park_iter(get_prev_node(index)), xyu::special_v, xyu::forward<Args>(args)...); }

        /// 中间插入元素 (初始化列表)，返回插入位置的前一个位置的迭代器
        Iter insert(xyu::size_t index, std::initializer_list<T> il) noexcept(noexcept(insert(index, xyu::make_range(il))))
        { return insert(index, xyu::make_range(il)); }

        /* 删除 */

        /// 删除索引开始的 count(默认为1) 个元素，返回删除位置的前一个位置的迭代器
        Iter erase(xyu::size_t index, xyu::size_t count = 1) noexcept
        {
            if (XY_UNLIKELY(index >= n || count == 0)) return Iter{(__::SLinkNode<T>*)&lead};
            if (n - index < count) count = n - index;
            auto* prev = get_prev_node(index);
            remove_node<true>(prev, count);
            return Iter{static_cast<__::SLinkNode<T>*>(prev)};
        }
        
        /* 迭代器 - 插入 */
        
        /**
        * @brief 迭代器之后插入元素 (多元素)，并返回传入位置的迭代器
        * @details 将每一个 args 作为一个元素依次插入到迭代器之前
        */
        template <typename It, typename... Args, typename = xyu::t_enable<xyu::t_is_same<SLinkList, typename It::Owner_t> && sizeof...(Args) && (... && xyu::t_can_init<T, Args>)>,
                  bool nothrow_init = (... && xyu::t_can_nothrow_init<T, Args>)>
        Iter insert_after(It prev_it, Args&&... args) noexcept(nothrow_init)
        {
            static_assert(sizeof...(args) <= limit());
            check_owned(prev_it, __LINE__, "SLinkList::insert_after");
            auto prev = prev_it.p;
            insert_help<nothrow_init>(
                    sizeof...(Args), prev,
                    [&](auto& node){ (..., (node = alloc_link_node(node, xyu::forward<Args, nothrow_init>(args)))); }
            );
            return Iter{static_cast<__::SLinkNode<T>*>(prev)};
        }

        /**
         * @brief 迭代器之后插入元素 (范围)，并返回传入位置的迭代器
         * @details 将 range 中的每一个元素依次插入到迭代器之前
         */
        template <typename It, typename Rg, typename = xyu::t_enable<xyu::t_is_same<SLinkList, typename It::Owner_t> && xyu::t_can_init<T, xyu::t_access<Rg>> && !xyu::t_can_init<T, Rg>>,
                  bool nothrow_init = xyu::t_can_nothrow_init<T, xyu::t_noexcept_access<Rg>>>
        Iter insert_after(It prev_it, Rg&& rg) noexcept(nothrow_init)
        {
            auto range = xyu::make_range(xyu::forward<Rg>(rg));
            check_owned(prev_it, __LINE__, "SLinkList::insert_after");
            __::Checker::check_range(range, XY_CurPos("SLinkList::insert_after"));
            auto prev = prev_it.p;
            insert_help<nothrow_init>(
                    range.count(), prev,
                    [&](auto& node){ for (auto&& v : range) node = alloc_link_node(node, xyu::forward<decltype(v), nothrow_init>(v)); }
            );
            return Iter{static_cast<__::SLinkNode<T>*>(prev)};
        }

        /**
         * @brief 迭代器之后插入元素 (单元素)，并返回传入位置的迭代器
         * @details 将 args 作为一个元素的构造参数依次插入到迭代器之前
         */
        template <typename It, typename... Args, typename = xyu::t_enable<xyu::t_is_same<SLinkList, typename It::Owner_t> && xyu::t_can_init<T, Args...>>,
                  bool nothrow_init = xyu::t_can_nothrow_init<T, Args...>>
        Iter insert_after(It prev_it, xyu::special_t = {}, Args&&... args) noexcept(nothrow_init)
        {
            check_owned(prev_it, __LINE__, "SLinkList::insert_after");
            __::Checker::check_capa<limit()>(n, 1, XY_CurPos("SLinkList::insert_after"));
            auto prev = prev_it.p;
            alloc_link_node(prev, xyu::forward<Args, nothrow_init>(args)...);
            ++n;
            return Iter{static_cast<__::SLinkNode<T>*>(prev)};
        }

        /// 迭代器之后插入元素 (初始化列表)，并返回传入位置的迭代器
        template <typename It, typename = xyu::t_enable<xyu::t_is_base<__::SLinkNodeBase, xyu::t_rem_pointer<decltype(xyu::t_val<It>().p)>>>>
        Iter insert_after(It prev_it, std::initializer_list<T> il) noexcept(noexcept(insert_after(prev_it, xyu::make_range(il))))
        { return insert_after(prev_it, xyu::make_range(il)); }

        /* 迭代器 - 删除 */

        /// 删除迭代器之后的位置，并返回传入位置的迭代器
        template <typename It, typename = xyu::t_enable<xyu::t_is_base<__::SLinkNodeBase, xyu::t_rem_pointer<decltype(xyu::t_val<It>().p)>>>>
        Iter erase_after(It prev_it, xyu::size_t count = 1) noexcept
        {
            check_owned<false,true>(prev_it, __LINE__, "SLinkList::erase_after");
            auto prev = prev_it.p;
            remove_node<false>(prev, count);
            return Iter{static_cast<__::SLinkNode<T>*>(prev)};
        }

        /* 转移 - 移出 */

        /// 移出索引位置节点
        HeapVal<__::SLinkNode<T>, cross_thread> take(xyu::size_t index) noexcept
        {
            __::Checker::check_index(index, n, XY_CurPos("SLinkList::take"));
            return take_after(park_iter(get_prev_node<false>(index)));
        }

        /// 移出迭代器位置的后一个节点
        template <typename It, typename = xyu::t_enable<xyu::t_is_base<__::SLinkNodeBase, xyu::t_rem_pointer<decltype(xyu::t_val<It>().p)>>>>
        HeapVal<__::SLinkNode<T>, cross_thread> take_after(It prev_it) noexcept
        {
            check_owned<true,true>(prev_it, __LINE__, "SLinkList::take_after");
            auto prev = prev_it.p;
            auto node = prev->next;
            prev->next = node->next;
            if (node == lead.tail) lead.tail = prev;
            --n;
            return xyu::HeapVal<__::SLinkNode<T>>{xyu::special_v, static_cast<__::SLinkNode<T>*>(node)};
        }

        /**
         * @brief 移出索引范围节点
         * @param index 起始索引，若越界则返回空链表
         * @param count 移动数量，若越界则为后续全部节点
         */
        SLinkList take(xyu::size_t index, xyu::size_t count) noexcept
        {
            return take_after(park_iter(get_prev_node(index)), count);
        }

        /**
         * @brief 移出索引范围节点
         * @param prev_start 从迭代器的下一个位置开始移出
         * @param count 移动数量，若越界则为后续全部节点
         */
        template <typename It, typename = xyu::t_enable<xyu::t_is_base<__::SLinkNodeBase, xyu::t_rem_pointer<decltype(xyu::t_val<It>().p)>>>>
        SLinkList take_after(It prev_start, xyu::size_t count) noexcept
        {
            check_owned(prev_start, __LINE__, "SLinkList::take_after");
            auto last = prev_start.p;
            for (xyu::size_t i = 0; i < count; ++i)
            {
                if (auto next = last->next; next == &lead) { count = i; break; }
                else last = static_cast<__::SLinkNode<T>*>(next);
            }
            return take_after(park_iter(prev_start.p), park_iter(last), count);
        }

        /**
         * @brief 移出索引范围节点
         * @param prev_start 从迭代器的下一个位置开始移出
         * @param prev_end 迭代器的下一个位置不再移动（即最后一个移动的位置）
         * @param count 移动数量，若不提供则自动计算
         */
        template <typename It, typename = xyu::t_enable<xyu::t_is_base<__::SLinkNodeBase, xyu::t_rem_pointer<decltype(xyu::t_val<It>().p)>>>>
        SLinkList take_after(It prev_start, It prev_end, xyu::size_t count = -1) noexcept
        {
            if (check_iters(prev_start, prev_end, count, __LINE__, "SLinkList::take_after")) return SLinkList{};
            SLinkList ret;
            ret.lead = {prev_start->next, prev_end.p};
            prev_start->next = prev_end->next;
            prev_end->next = &ret.lead;
            ret.n = count;
            if (prev_end.p == lead.tail) lead.tail = prev_start.p;
            n -= count;
            return ret;
        }

        /* 转移 - 移入 */

        /// 将节点移入索引位置，并返回移入位置的前一个位置的迭代器
        Iter put(xyu::size_t index, HeapVal<__::SLinkNode<T>, cross_thread>&& node) noexcept
        {
            return put_after(park_iter(get_prev_node(index)), xyu::move(node));
        }

        /// 将节点移入迭代器之后，并返回移入位置的前一个位置的迭代器
        template <typename It, typename = xyu::t_enable<xyu::t_is_base<__::SLinkNodeBase, xyu::t_rem_pointer<decltype(xyu::t_val<It>().p)>>>>
        Iter put_after(It prev_it, HeapVal<__::SLinkNode<T>, cross_thread>&& node) noexcept
        {
            check_owned(prev_it, __LINE__, "SLinkList::put_after");
            auto prev = prev_it.p;
            auto newn = node.take();
            newn->next = prev->next;
            prev->next = newn;
            if (prev == lead.tail) lead.tail = newn;
            ++n;
            return Iter{prev};
        }

        /// 将链表移入索引位置，并返回移入位置的前一个位置的迭代器
        Iter put(xyu::size_t index, SLinkList&& list) noexcept
        {
            return put_after(park_iter(get_prev_node(index)), xyu::move(list));
        }

        /// 将链表移入迭代器之后，并返回移入位置的前一个位置的迭代器
        template <typename It, typename = xyu::t_enable<xyu::t_is_base<__::SLinkNodeBase, xyu::t_rem_pointer<decltype(xyu::t_val<It>().p)>>>>
        Iter put_after(It prev_it, SLinkList&& list) noexcept
        {
            check_owned(prev_it, __LINE__, "SLinkList::put_after");
            if (XY_UNLIKELY(&list == this)) return insert_after(prev_it, list.range());
            auto* prev = prev_it.p;
            if (list.empty()) return Iter{prev};
            link_list(prev, list.lead.next, list.lead.tail, list.count());
            list.lead = {&list.lead, &list.lead};
            list.n = 0;
            return Iter{prev};
        }

        /* 范围 */

        /// 创建范围
        auto range() noexcept { return xyu::Range<Iter>(n, static_cast<__::SLinkNode<T>*>(lead.next), (__::SLinkNode<T>*)(&lead)); }
        /// 创建范围 (const)
        auto range() const noexcept { return const_cast<SLinkList*>(this)->range().crange(); }

        /* 运算符 */

        /// 索引访问
        T& operator[](xyu::size_t index) noexcept { return get(index); }
        /// 索引访问 (const)
        xyu::t_add_const<T>& operator[](xyu::size_t index) const noexcept { return get(index); }

        /// 尾部添加元素
        template <typename Arg>
        SLinkList& operator<<(Arg&& arg) noexcept(noexcept(append(xyu::forward<Arg>(arg))))
        { append(xyu::forward<Arg>(arg)); return *this; }
        /// 头部添加元素
        template <typename Arg>
        friend SLinkList& operator>>(Arg&& arg, SLinkList& list) noexcept(noexcept(list.prepend(xyu::forward<Arg>(arg))))
        { list.prepend(xyu::forward<Arg>(arg)); return list; }

    private:
        // 生成标志迭代器
        struct self_iter_tag {};
        static auto park_iter(__::SLinkNodeBase* node) noexcept
        {
            struct SIter : Iter, self_iter_tag { using Iter::Iter; };
            return SIter{static_cast<__::SLinkNode<T>*>(node)};
        }
        // 检查是否为标志迭代器
        template <typename It>
        static constexpr bool check_park_iter(It) noexcept { return xyu::t_is_base<self_iter_tag, It>; }
        // 检查所属
        template <bool check_empty = false, bool no_tail = false, typename It>
        void check_owned(It&& iter, int line, const char* func) noexcept
        {
            if constexpr (!XY_RELEASE)
            {
                if constexpr (check_empty) if (XY_UNLIKELY(empty()))
                { xyloge2("can't get node from an empty SLinkList", line, func); xyu::abort(); }
                if constexpr (no_tail) if (XY_UNLIKELY(iter.p == lead.tail))
                { xyloge2("can't provide tail node as prev nod", line, func); xyu::abort(); }
                if constexpr (!xyu::t_is_base<self_iter_tag, It> && XY_DEBUG)
                {
                    const __::SLinkNodeBase* node = iter.p;
                    if (node == &lead) return;
                    __::SLinkNodeBase* cur = lead.next;
                    for(;;)
                    {
                        if (cur == node) break;
                        if (cur == &lead)
                        {
                            xyloge2("SLinkNode doesn't belong to this SLinkList", line, func);
                            xyu::abort();
                        }
                        cur = cur->next;
                    }
                }
            }
        }
        // 检查迭代器范围
        template <typename It>
        bool check_iters(It&& prev_start, It&& prev_end, xyu::size_t& count, int line, const char* func) noexcept
        {
            if constexpr (!xyu::t_is_base<self_iter_tag, It>)
            {
                if constexpr (!XY_RELEASE)
                {
                    const __::SLinkNodeBase *sn = prev_start.p, *en = prev_end.p;
                    __::SLinkNodeBase* cur = &lead;
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
                            if (en == cur) { of = true; if (sf) break; }
                            else ++j;
                        }
                        cur = cur->next;
                        if (cur == &lead)
                        {
                            if (!sf) xyloge2("SLinkNode prev_start doesn't belong to this SLinkList", line, func);
                            if (!of) xyloge2("SLinkNode prev_end doesn't belong to this SLinkList", line, func);
                            xyu::abort();
                        }
                    }
                    if (XY_UNLIKELY(i > j))
                    {
                        xyloge2("SLinkNode prev_start can't larger than prev_end", line, func);
                        xyu::abort();
                    }
                    if (count == -1) count = j - i;
                    else if (count != j - i)
                    {
                        xyloge2("SLinkNode [{},{}) dosen't match with {}", line, func, i, j, count);
                        xyu::abort();
                    }
                }
                else if (count == -1)
                {
                    count = 0;
                    __::SLinkNodeBase *sn = prev_start.p, *en = prev_end.p;
                    while (sn != en) { sn = sn->next; ++count; }
                }
            }
            if (XY_UNLIKELY(count == 0)) return true;
            return false;
        }

        // 获取上一个节点
        template <bool check = true>
        __::SLinkNodeBase* get_prev_node(xyu::size_t index) const noexcept
        {
            if constexpr (check)
                if (XY_UNLIKELY(index >= n)) return lead.tail;
            auto* node = (__::SLinkNodeBase*)(&lead);
            while (index > 0) node = node->next, --index;
            return node;
        }

        // 创建新节点
        template <typename... Args>
        static __::SLinkNodeBase* alloc_node(__::SLinkNodeBase* prev, Args&&... args) noexcept(xyu::t_can_nothrow_init<T, Args...>)
        {
            __::SLinkNode<T>* node = __::Common::alloc<__::SLinkNode<T>, cross_thread>();
            if constexpr (xyu::t_can_nothrow_init<T, Args...>)
                ::new (node) __::SLinkNode<T>(prev->next, xyu::forward<Args>(args)...);
            else {
                try { ::new (node) __::SLinkNode<T>(prev->next, xyu::forward<Args>(args)...); }
                catch (...) { free_node(node); throw; }
            }
            return node;
        }
        // 释放节点
        static void free_node(__::SLinkNode<T>* ptr) noexcept { __::Common::dealloc<cross_thread>(ptr); }

        // 创建新节点并单向连接
        template <typename... Args>
        static __::SLinkNodeBase* alloc_link_node(__::SLinkNodeBase* prev, Args&&... args) noexcept(noexcept(alloc_node(prev, xyu::forward<Args>(args)...)))
        {
            auto* node = alloc_node(prev, xyu::forward<Args>(args)...);
            prev->next = node;
            return node;
        }

        // 将节点链接入链表
        void link_list(__::SLinkNodeBase* prev, __::SLinkNodeBase* start, __::SLinkNodeBase* last, xyu::size_t add) noexcept
        {
            last->next = prev->next;
            prev->next = start;
            if (lead.tail == prev) lead.tail = last;
            n += add;
        }

        // 删除节点
        template <bool count_safe>
        void remove_node(__::SLinkNodeBase* prev, xyu::size_t count) noexcept
        {
            for (xyu::size_t i = 0; i < count; ++i)
            {
                auto* node = static_cast<__::SLinkNode<T>*>(prev->next);
                if constexpr (!count_safe) if ((__::SLinkNodeBase*)node == &lead) break;
                prev->next = node->next;
                node->~SLinkNode();
                free_node(node);
                --n;
            }
            if (prev->next == &lead) lead.tail = prev;
        }

        // append 辅助
        template <bool nothrow_init, typename AppFun>
        void append_help(xyu::size_t add, AppFun&& af)
        {
            __::Checker::check_capa<limit()>(n, add, XY_CurPos("SLinkList::append"));
            __::SLinkNodeBase* node = lead.tail;
            if constexpr (nothrow_init) af(node);
            else try { af(node); }
            catch (...) {
                lead.tail = node;
                throw;
            }
            lead.tail = node;
        }

        // prepend 辅助
        template <bool nothrow_init, typename PreFun>
        void prepend_help(xyu::size_t add, PreFun&& pf)
        {
            __::Checker::check_capa<limit()>(n, add, XY_CurPos("SLinkList::prepend"));
            bool retail = empty();
            __::SLinkNodeBase* node = &lead;
            if constexpr (nothrow_init) pf(node);
            else try { pf(node); }
                catch (...) {
                    if (retail) lead.tail = node;
                    throw;
                }
            if (retail) lead.tail = node;
        }
        
        // insert 辅助
        template <bool nothrow_init, typename AddFun>
        void insert_help(xyu::size_t add, __::SLinkNodeBase* prev, AddFun&& af)
        {
            __::Checker::check_capa<limit()>(n, add, XY_CurPos("SLinkList::insert"));
            if (XY_UNLIKELY(add == 0)) return;
            __::SLinkNodeBase head{&head}, *node = &head;
            if constexpr (nothrow_init) af(node);
            else try { af(node); }
            catch (...) {
                auto* cur = static_cast<__::SLinkNode<T>*>(head.next);
                while (cur != &head)
                {
                    auto* next = static_cast<__::SLinkNode<T>*>(cur->next);
                    cur->~SLinkNode();
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
    SLinkList(Rg&&) -> SLinkList<xyu::t_rem_cr<xyu::t_access<Rg>>>;
}
