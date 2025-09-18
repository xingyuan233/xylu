#pragma clang diagnostic push
#pragma ide diagnostic ignored "misc-unconventional-assign-operator"
#pragma ide diagnostic ignored "UnreachableCallsOfFunction"
#pragma ide diagnostic ignored "hicpp-exception-baseclass"
#pragma ide diagnostic ignored "OCUnusedGlobalDeclarationInspection"
#pragma ide diagnostic ignored "modernize-use-nodiscard"
#pragma ide diagnostic ignored "OCUnusedStructInspection"
#pragma once

#include "../../link/log"

namespace xylu::xycontain
{
    namespace __
    {
        /// 链表节点基类
        struct ListNode_Base
        {
            ListNode_Base* next;    // 后一个节点
            ListNode_Base* prev;    // 前一个节点
        };

        /// 链表节点类
        template <typename T>
        struct ListNode : ListNode_Base
        {
            T val;    // 数据

            template <typename... Args, typename Test = T, xyu::t_enable<xyu::t_is_aggregate<Test>, bool> = true>
            explicit ListNode(ListNode_Base* next, ListNode_Base* prev, Args&&... args) noexcept(xyu::t_can_nothrow_listinit<T, Args...>)
                : ListNode_Base{next, prev}, val{xyu::forward<Args>(args)...} {}

            template <typename... Args, typename Test = T, xyu::t_enable<!xyu::t_is_aggregate<Test>, bool> = false>
            explicit ListNode(ListNode_Base* next, ListNode_Base* prev, Args&&... args) noexcept(xyu::t_can_nothrow_construct<T, Args...>)
                : ListNode_Base{next, prev}, val(xyu::forward<Args>(args)...) {}
        };
    }

    /**
     * @brief 一个功能丰富、高度优化且异常安全的双向链表容器。
     *
     * @tparam T 存储的元素类型。T 必须满足可无异常析构的要求 (is_nothrow_destructible)。
     *
     * @details
     * `xyu::List` 是对经典双向链表的现代化C++实现，它在提供所有核心功能的同时，
     * 深度集成了 `xylu` 库的生态系统，展现了卓越的性能和易用性。
     *
     * ### 核心亮点 (Key Features):
     *
     * 1.  **哨兵节点设计 (Sentinel Node Design):**
     *     - 内部采用单个哨兵节点来代表链表的头和尾，极大地简化了插入和删除操作的
     *       逻辑，避免了对空链表和边界节点的特殊处理，使代码更高效、更健壮。
     *
     * 2.  **高性能索引 (High-Performance Indexing):**
     *     - 通过索引访问元素时 (`at()`, `get()`)，会自动判断目标索引位于链表的前半部分
     *       还是后半部分，并选择从头或从尾开始遍历，将最坏情况下的查找复杂度从 O(n)
     *       优化到 O(n/2)。
     *
     * 3.  **统一且强大的修改接口 (Unified & Powerful Modification API):**
     *     - 与 `xyu::Vector` 共享一套设计哲学，`append()` 和 `insert()` 方法通过模板元编程
     *       被高度泛化，能够智能地处理单元素构造、多元素添加以及范围插入等多种情况，
     *       提供了高度一致的API体验。
     *
     * 4.  **与范围生态无缝融合 (Seamless Range Integration):**
     *     - `range()` 方法返回一个 `xyu::Range` 对象，该对象内部使用为节点优化的
     *       `RangeIter_NodePtr` 迭代器。这使得 `List` 可以直接、高效地用于范围for循环，
     *       并与所有 `xylu` 范围算法无缝协作。
     *
     * 5.  **异常安全与底层集成 (Exception Safety & Deep Integration):**
     *     - 节点的内存分配与构造过程提供了强大的异常安全保证，确保在元素构造失败时
     *       能正确回滚并释放内存，杜绝泄露。
     *     - 所有节点的内存都通过 `xylu` 的线程局部内存池进行管理，享受其带来的高性能和
     *       自动回收的特性。
     */
    template <typename T>
    class List
    {
        static_assert(xyu::t_can_nothrow_destruct<T>);

    private:
        __::ListNode_Base lead;     // 哨兵节点
        xyu::size_t n;              // 节点数量

    public:
        /* 构造析构 */

        /// 默认构造
        List() noexcept : lead{&lead, &lead}, n{0} {}
        /// 移动构造
        List(List&& other) noexcept : n{other.n}, lead{other.n ? other.lead.next : &lead, other.n ? other.lead.prev : &lead}
        {
            lead.next->prev = lead.prev->next = &lead;
            other.lead.next = other.lead.prev = &other.lead;
            other.n = 0;
        }
        /// 复制构造
        List(const List& other) : List{} { append(other.range()); }

        /// 分配元素初始化
        List(xyu::size_t n, const T& value) : List{} { append(xyu::make_range_repeat(value, n)); }
        /// 初始化列表构造
        List(std::initializer_list<T> il) : List{} { append(xyu::make_range(il)); }

        /// 析构
        ~List() noexcept { release(); }

        /* 赋值交换 */

        /// 移动赋值
        List& operator=(List&& other) noexcept { return swap(other); }
        /// 复制赋值
        List& operator=(const List& other)
        {
            if (XY_UNLIKELY(this == &other)) return *this;
            // 复制一部分
            auto it = range().begin();
            auto it2 = other.range().begin();
            for (xyu::size_t i = 0, j = xyu::min(n, other.n); i < j; ++i, ++it, ++it2) *it = *it2;
            // 追加剩余元素
            if (other.n > n) append(decltype(other.range())(other.n - n, it2, other.range().end()));
            // 释放多余内存
            else erase(it, n - other.n);
            return *this;
        }

        /// 交换
        List& swap(List&& other) noexcept { return swap(other); }
        /// 交换
        List& swap(List& other) noexcept
        {
            // 缓存 this
            xyu::size_t tmp = n;
            __::ListNode_Base *first = lead.next, *last = lead.prev;
            // other -> this
            n = other.n;
            if (n == 0) lead.next = lead.prev = &lead;
            else {
                lead.next = other.lead.next;
                lead.prev = other.lead.prev;
                lead.next->prev = lead.prev->next = &lead;
            }
            // this -> other
            other.n = tmp;
            if (tmp == 0) other.lead.next = other.lead.prev = &other.lead;
            else {
                other.lead.next = first;
                other.lead.prev = last;
                first->prev = last->next = &other.lead;
            }
            return *this;
        }

        /* 数据容量 */

        /// 获取最大容量
        constexpr static xyu::size_t limit() noexcept { return xyu::number_traits<xyu::size_t>::max / 2; }
        /// 获取元素数量
        xyu::size_t count() const noexcept { return n; }
        /// 是否为空
        bool empty() const noexcept { return n == 0; }

        /* 元素获取 */

        /// 获取元素
        T& get(xyu::size_t index) XY_NOEXCEPT_NDEBUG
        {
#if XY_DEBUG
            return at(index);
#else
            return static_cast<__::ListNode<T>*>(get_node(index))->val;
#endif
        }
        /// 获取元素 (const)
        const T& get(xyu::size_t index) const XY_NOEXCEPT_NDEBUG { return const_cast<List*>(this)->get(index); }

        /// 获取元素 [检测范围]
        T& at(xyu::size_t index)
        {
            if (XY_UNLIKELY(index >= n)) {
                xyloge(false, "E_Logic_Out_Of_Range: index {} out of range [0, {})", index, n);
                throw xyu::E_Logic_Out_Of_Range{};
            }
            return static_cast<__::ListNode<T>*>(get_node(index))->val;
        }
        /// 获取元素 (const) [检测范围]
        const T& at(xyu::size_t index) const { return const_cast<List*>(this)->at(index); }

        /* 数据管理 */

        /// 释放内存
        void release() noexcept
        {
            if (XY_UNLIKELY(n == 0)) return;
            auto* node = static_cast<__::ListNode<T>*>(lead.next);
            do {
                auto* next = static_cast<__::ListNode<T>*>(node->next);
                node->~ListNode();
                xyu::dealloc<__::ListNode<T>>(node, 1);
                node = next;
            } while (node != &lead);
            lead.next = lead.prev = &lead;
            n = 0;
        }

        /* 尾增 */

        /**
         * @brief 尾部增加元素 (多元素)
         * @note 将每一个 args 作为一个元素添加到尾部
         */
        template <typename Tag = void, typename... Args, xyu::t_enable<!xyu::t_is_same<Tag, xyu::special_t> && xyu::t_can_init<T, xyu::t_args_get<xyu::typelist_c<Args..., void>, 0>>, bool> = true>
        List& append(Args&&... args)
        {
            static_assert(xyu::t_is_same<Tag, void>);
            static_assert((... && xyu::t_can_init<T, Args>));
            static_assert(sizeof...(Args) <= limit());
            check_new_capa(n + sizeof...(Args));
            (..., alloc_init(lead.prev, xyu::forward<Args>(args)));
            return *this;
        }

        /**
         * @brief 尾部增加元素 (范围)
         * @note 将 range 中的每一个元素添加到尾部
         */
        template <typename Tag = void, typename Rg, xyu::t_enable<!xyu::t_is_same<Tag, xyu::special_t> && !xyu::t_can_init<T, Rg> && xyu::t_is_range<Rg>, bool> = true>
        List& append(Rg&& range)
        {
            static_assert(xyu::t_is_same<Tag, void>);
            check_add(range.count());
            check_new_capa(n + range.count());
            for (auto&& v : range) alloc_init(lead.prev, xyu::forward<decltype(v)>(v));
            return *this;
        }

        /**
         * @brief 尾部增加元素 (构造范围)
         * @note 将 args 构造为一个 xyu::range 后，逐元素添加到尾部
         */
        template <typename Tag = void, typename Arg, xyu::t_enable<!xyu::t_is_same<Tag, xyu::special_t> && !xyu::t_can_init<T, Arg> && !xyu::t_is_range<Arg>, bool> = true>
        List& append(Arg&& arg)
        {
            static_assert(xyu::t_is_same<Tag, void>);
            static_assert(xyu::t_can_make_range<Arg>);
            return append<Tag>(xyu::make_range(arg));
        }

        /**
         * @brief 尾部增加元素 (单元素)
         * @tparam Tag 显式指定为 xyu::special_t
         * @note 将 args 作为一个元素的构造参数添加到尾部
         */
        template <typename Tag, typename... Args, xyu::t_enable<xyu::t_is_same<Tag, xyu::special_t>, bool> = false>
        List& append(Args&&... args)
        {
            static_assert(xyu::t_can_init<T, Args...>);
            check_new_capa(n + 1);
            alloc_init(lead.prev, xyu::forward<Args>(args)...);
            return *this;
        }

        /// 尾部增加元素 (初始化列表)
        List& append(std::initializer_list<T> il) { return append(xyu::make_range(il)); }

        /* 插入 */

        /**
         * @brief 中间插入元素 (多元素)
         * @note 将每一个 args 作为一个元素依次插入到 index 位置
         * @note 若 index 越界，则在尾部插入
         */
        template <typename Tag = void, typename... Args, xyu::t_enable<sizeof...(Args) && !xyu::t_is_same<Tag, xyu::special_t> && xyu::t_can_init<T, xyu::t_args_get<xyu::typelist_c<Args..., void>, 0>>, bool> = true>
        List& insert(xyu::size_t index, Args&&... args)
        {
            static_assert(xyu::t_is_same<Tag, void>);
            static_assert((... && xyu::t_can_init<T, Args>));
            static_assert(sizeof...(args) <= limit());
            if (index >= n) return append(xyu::forward<Args>(args)...);
            check_new_capa(n + sizeof...(Args));
            __::ListNode_Base* prev = get_prev_node(index);
            (..., (prev = alloc_init(prev, xyu::forward<Args>(args))));
            return *this;
        }

        /**
         * @brief 中间插入元素 (范围)
         * @note 将 range 中的每一个元素依次插入到 index 位置
         * @note 若 index 越界，则在尾部插入
         */
        template <typename Tag = void, typename Rg, xyu::t_enable<!xyu::t_is_same<Tag, xyu::special_t> && !xyu::t_can_init<T, Rg> && xyu::t_is_range<Rg>, bool> = true>
        List& insert(xyu::size_t index, Rg&& range)
        {
            static_assert(xyu::t_is_same<Tag, void>);
            if (index >= n) return append(xyu::forward<Rg>(range));
            check_add(range.count());
            __::ListNode_Base* prev = get_prev_node(index);
            for (auto&& v : range) prev = alloc_init(prev, xyu::forward<decltype(v)>(v));
            return *this;
        }

        /**
         * @brief 中间插入元素 (构造范围)
         * @note 将 args 构造为一个 xyu::range 后，逐元素插入到 index 位置
         * @note 若 index 越界，则在尾部插入
         */
        template <typename Tag = void, typename Arg, xyu::t_enable<!xyu::t_is_same<Tag, xyu::special_t> && !xyu::t_can_init<T, Arg> && !xyu::t_is_range<Arg>, bool> = true>
        List& insert(xyu::size_t index, Arg&& arg)
        {
            static_assert(xyu::t_is_same<Tag, void>);
            static_assert(xyu::t_can_make_range<Arg>);
            return insert<Tag>(index, xyu::make_range(arg));
        }

        /**
         * @brief 中间插入元素 (单元素)
         * @tparam Tag 显式指定为 xyu::special_t
         * @note 将 args 作为一个元素的构造参数依次插入到 index 位置
         * @note 若 index 越界，则在尾部插入
         */
        template <typename Tag, typename... Args, xyu::t_enable<xyu::t_is_same<Tag, xyu::special_t>, bool> = false>
        List& insert(xyu::size_t index, Args&&... args)
        {
            static_assert(xyu::t_can_init<T, Args...>);
            if (index >= n) return append(xyu::forward<Args>(args)...);
            check_new_capa(n + 1);
            __::ListNode_Base* prev = get_prev_node(index);
            alloc_init(prev, xyu::forward<Args>(args)...);
            return *this;
        }

        /// 中间插入元素 (初始化列表)
        List& insert(xyu::size_t index, std::initializer_list<T> il) { return insert(index, xyu::make_range(il)); }

        /// 迭代器之前插入元素，并返回新位置的迭代器 (单元素)
        template <typename Iter, typename... Args, typename = xyu::t_enable<xyu::t_is_same_nocvref<decltype(*xyu::t_val<Iter>()), T>>>
        Iter insert(Iter it, Args&&... args)
        {
            alloc_init(it.i->prev, xyu::forward<Args>(args)...);
            return Iter{static_cast<__::ListNode<T>*>(it.i->prev)};
        }

        /* 删除 */

        /// 删除索引开始的 count(默认为1) 个元素
        List& erase(xyu::size_t index, xyu::size_t count = 1)
        {
            if (XY_UNLIKELY(index >= n)) return *this;
            if (n - index < count) count = n - index;
            remove_node(get_prev_node(index), count);
            return *this;
        }

        /// 删除迭代器位置，并返回下一个位置的迭代器
        template <typename Iter, typename = xyu::t_enable<xyu::t_is_same_nocvref<decltype(*xyu::t_val<Iter>()), T>>>
        Iter erase(Iter it, xyu::size_t count = 1)
        {
            auto* prev = static_cast<__::ListNode_Base*>(it.i->prev);
            remove_node(prev, count);
            return Iter{static_cast<__::ListNode<T>*>(prev->next)};
        }

        /* 范围 */

        /// 创建范围
        auto range() noexcept { return xyu::make_range(n, static_cast<__::ListNode<T>*>(lead.next), static_cast<__::ListNode<T>*>(&lead)); }
        /// 创建范围 (const)
        auto range() const noexcept { return xyu::make_range(n, static_cast<const __::ListNode<T>*>(lead.next), static_cast<const __::ListNode<T>*>(&lead)); }

        /* 运算符 */

        /**
         * @brief 索引访问 (可修改)
         * @note 支持负索引，即 [-count,-1] 映射到 [0,count-1]
         * @note (-∞,-count) 与 [count,+∞) 则 映射到 两端 即 0 和 count-1
         */
        T& operator[](xyu::diff_t index) XY_NOEXCEPT_NDEBUG
        {
            auto i = static_cast<xyu::size_t>(index);
            if (index >= 0) return get(i >= n ? n - 1 : i);
            else return get(XY_UNLIKELY((i += n) >= n) ? 0 : i);
        }
        /**
         * @brief 索引访问 (不可修改)
         * @note 支持负索引，即 [-count,-1] 映射到 [0,count-1]
         * @note (-∞,-count) 与 [count,+∞) 则 映射到 两端 即 0 和 count-1
         */
        const T& operator[](xyu::diff_t index) const XY_NOEXCEPT_NDEBUG
        { return const_cast<List*>(this)->operator[](index); }

        /// 尾部添加元素
        template <typename Arg>
        List& operator<<(Arg&& arg) { return append(xyu::forward<Arg>(arg)); }

    private:
        // 就地构造新元素
        template <typename... Args>
        __::ListNode_Base* alloc_init(__::ListNode_Base* prev, Args&&... args)
        {
            __::ListNode<T>* node = xyu::alloc<__::ListNode<T>>(1);
            if constexpr (xyu::t_can_nothrow_init<T, Args...>)
                ::new (node) __::ListNode<T>(prev->next, prev, xyu::forward<Args>(args)...);
            else {
                try { ::new (node) __::ListNode<T>(prev->next, prev, xyu::forward<Args>(args)...); }
                catch (...) { xyu::dealloc<__::ListNode<T>>(node, 1); throw; }
            }
            prev->next->prev = node;
            prev->next = node;
            ++n;
            return node;
        }

        // 检查增加的元素数量
        static void check_add(xyu::size_t count)
        {
            if (XY_UNLIKELY(count > limit())) {
                xyloge(false, "E_Memory_Capacity: add {} over limit {}", count, limit());
                throw xyu::E_Memory_Capacity{};
            }
        }
        // 检测新容量
        static void check_new_capa(xyu::size_t newcapa)
        {
            if (XY_UNLIKELY(newcapa > limit())) {
                xylogei(false, "E_Memory_Capacity: capacity {} over limit {}", newcapa, limit());
                throw xyu::E_Memory_Capacity{};
            }
        }

        // 获取上一个节点
        __::ListNode_Base* get_prev_node(xyu::size_t index) noexcept
        {
            auto* node = const_cast<__::ListNode_Base*>(&lead);
            if (index <= n / 2) for (xyu::size_t i = 0; i < index; ++i) node = node->next;
            else for (xyu::size_t i = 0; i < n - index + 1; ++i) node = node->prev;
            return node;
        }
        // 获取当前节点
        __::ListNode_Base* get_node(xyu::size_t index) noexcept
        {
            return get_prev_node(index)->next;
        }

        // 删除节点
        void remove_node(__::ListNode_Base* prev, xyu::size_t count) noexcept
        {
            for (xyu::size_t i = 0; i < count; ++i)
            {
                auto* node = static_cast<__::ListNode<T>*>(prev->next);
                node->next->prev = prev;
                prev->next = node->next;
                node->~ListNode();
                xyu::dealloc<__::ListNode<T>>(node, 1);
                --n;
            }
        }
    };
}

#pragma clang diagnostic pop