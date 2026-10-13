#pragma once

/**
 * @file    xycoll/stack.h
 * @brief   栈
 * @date    2026-06-16
 */

#include "./array.h"
#include "./nearlist.h"
#include "./slinklist.h"

/// 静态数组栈
namespace xylu::xycoll
{
    /**
     * @brief 静态数组
     * @tparam T    元素类型
     * @tparam Capa 最大容量
     *
     * @details
     *   存储方式：栈数组，编译期固定容量，连续存储，基于 @see ArrayList
     *
     *   方法简述：
     *   - 支持 `reduce`、缩减，`clear` 清空元素
     *   - 支持 `get`O(1) 获取元素，栈顶索引为 0
     *   - 支持 `push`O(1) 插入 多元素/范围/单元素，`pop`O(1) 出栈
     *
     *   注意事项：
     *   - 容量限制：增加元素超过 Capa 时会触发 abort
     *   - 索引越界：`get`、`op[]` 会在 !XY_RELEASE 下检查并触发 abort
     *
     *   异常安全：
     *   - 复制/移动赋值：若元素赋值可能抛出异常，先清空再插入，异常时容器为空；否则就地移动或构造
     *   - `push`：若元素构造抛出异常，已成功构造的元素保留，需要手动回滚或继续尝试
     */
    template <typename T, xyu::size_t Capa>
    struct ArrayStack
    {
        static_assert(xyu::t_can_nothrow_destruct<T>);
    private:
        ArrayList<T, Capa> list;

    public:
        /* 构造 */

        /// 默认构造
        ArrayStack() noexcept : list{} {}

        /// 范围构造
        template <typename Rg, typename = xyu::t_enable<xyu::t_can_range<Rg>>>
        explicit ArrayStack(Rg&& range) noexcept(noexcept(ArrayList<T, Capa>{xyu::forward<Rg>(range)}))
        : list{xyu::make_range(xyu::forward<Rg>(range))} {}

        /* 数据容量 */

        /// 获取容量
        static constexpr xyu::size_t capacity() noexcept { return Capa; }

        /// 获取元素个数
        xyu::size_t count() const noexcept { return list.count(); }
        /// 是否为空
        bool empty() const noexcept { return list.empty(); }

        /* 数据获取 */

        /// 获取元素 (0为栈顶)
        T& get(xyu::size_t index) noexcept { return list.get(count() - index - 1); }
        /// 获取元素 (const) (0为栈顶)
        xyu::t_add_const<T>& get(xyu::size_t index) const noexcept { return list.get(count() - index - 1); }

        /* 数据管理 */

        /// 清空元素
        void clear() noexcept { list.clear(); }

        /// 缩减栈元素数量
        void reduce(xyu::size_t newn) noexcept { list.reduce(newn); }

        /* 入栈 */

        /**
         * @brief 入栈
         * @details 支持多元素 | 范围 | xyu::special_v + 单元素
         */
        template <typename... Args, typename = decltype(list.append(xyu::t_val<Args>()...))>
        ArrayStack& push(Args&&... args) noexcept(noexcept(list.append(xyu::forward<Args>(args)...)))
        {
            list.append(xyu::forward<Args>(args)...);
            return *this;
        }

        /// 入栈 (初始化列表)
        ArrayStack& push(std::initializer_list<T> il) noexcept(xyu::t_can_nothrow_cpconstr<T>) { return push(xyu::make_range(il)); }

        /* 出栈 */

        /// 弹出一个元素
        T pop() noexcept(xyu::t_can_nothrow_mvconstr<T>)
        {
            T tmp = xyu::move(get(0));
            list.reduce(count() - 1);
            return tmp;
        }

        /// 弹出多个元素
        void pop(xyu::size_t count) noexcept
        {
            if (XY_LIKELY(count < this->count())) reduce(this->count() - count);
            else clear();
        }

        /* 范围 */

        /// 创建范围
        auto range() noexcept { return list.range().rrange(); }
        /// 创建范围 (const)
        auto range() const noexcept { return list.range().rrange(); }

        /* 运算符 */

        /// 索引访问
        T& operator[](xyu::size_t index) noexcept { return get(index); }
        /// 索引访问 (const)
        xyu::t_add_const<T>& operator[](xyu::size_t index) const noexcept { return get(index); }

        /// 入栈
        template <typename Arg>
        ArrayStack& operator<<(Arg&& arg) noexcept(noexcept(push(xyu::forward<Arg>(arg)))) { return push(xyu::forward<Arg>(arg)); }
    };

}

/// 动态数组栈
namespace xylu::xycoll
{
    /**
     * @brief 动态数组栈
     * @tparam T  元素类型
     * @tparam cross_thread  是否允许跨线程内存管理。
     *         - false（默认）: 使用线程局部内存池，性能更好；
     *         - true : 使用全局内存分配，对象可跨线程重分配和释放。
     *
     * @details
     *   存储方式：堆数组，连续存储，基于 @see NearList
     *
     *   方法简述：
     *   - 支持 `reserve` 预分配，`reduce` 条件缩容，`release` 释放内存
     *   - 支持 `reduce` 整体缩减，`clear` 清空元素
     *   - 支持 `get`O(1) 获取元素，栈顶索引为 0
     *   - 支持 `push`O(1) 插入 多元素/范围/单元素，`pop`O(1) 出栈
     *
     *   注意事项：
     *   - 容量限制：增加元素超过 `limit` 时会触发 abort
     *   - 范围限制：传入非有限范围在 !XY_RELEASE 下检查并触发 abort
     *   - 索引越界：`get`、`op[]` 会在 !XY_RELEASE 下检查并触发 abort
     *   - 迭代器失效：`pop` 在移动赋值可能抛出异常时会分配新内存，操作完毕后所有旧迭代器失效
     *
     *   异常安全：
     *   - 复制赋值、`reserve`、`reduce`、`resize`、`pop`：抛出异常时回滚至操作前状态
     *   - `push`：若元素构造抛出异常，已成功构造的元素保留，需要手动回滚或继续尝试
     */
    template <typename T, bool cross_thread = false>
    struct NearStack
    {
    private:
        NearList<T, cross_thread> list;

    public:
        /* 构造 */

        /// 默认构造
        NearStack() noexcept = default;

        /// 预分配
        explicit NearStack(xyu::size_t capa) noexcept : list(capa) {}

        /// 范围构造
        template <typename Rg, typename = xyu::t_enable<xyu::t_can_range<Rg>>>
        explicit NearStack(Rg&& range) noexcept(noexcept(NearList<T, cross_thread>{xyu::forward<Rg>(range)}))
        : list{xyu::make_range(xyu::forward<Rg>(range))} {}

        /// 交换
        NearStack& swap(NearStack& other) noexcept { list.swap(other.list); return *this; }

        /* 数据容量 */

        /// 获取最大容量
        constexpr static xyu::size_t limit() noexcept { return NearList<T, cross_thread>::limit(); }
        /// 获取容量
        xyu::size_t capacity() noexcept { return list.capacity(); }

        /// 获取元素个数
        xyu::size_t count() const noexcept { return list.count(); }
        /// 是否为空
        bool empty() const noexcept { return list.empty(); }

        /* 数据获取 */

        /// 获取元素 (0为栈顶)
        T& get(xyu::size_t index) noexcept { return list.get(count() - index - 1); }
        /// 获取元素 (const) (0为栈顶)
        xyu::t_add_const<T>& get(xyu::size_t index) const noexcept { return list.get(count() - index - 1); }

        /* 数据管理 */

        /// 预分配空间
        void reserve(xyu::size_t mincapa) noexcept(noexcept(list.reserve(mincapa))) { list.reserve(mincapa); }
        /// 缩减容量
        void reduce() noexcept(noexcept(list.reduce())) { list.reduce(); }

        /// 清空元素
        void clear() noexcept { list.clear(); }
        /// 释放内存
        void release() noexcept { list.release(); }

        /// 缩减栈元素数量
        void reduce(xyu::size_t newn) noexcept { list.reduce(newn); }

        /* 入栈 */

        /**
         * @brief 入栈
         * @details 支持多元素 | 范围 | xyu::special_v + 单元素
         */
        template <typename... Args, typename = decltype(list.append(xyu::t_val<Args>()...))>
        NearStack& push(Args&&... args) noexcept(noexcept(list.append(xyu::forward<Args>(args)...)))
        {
            list.append(xyu::forward<Args>(args)...);
            return *this;
        }

        /// 入栈 (初始化列表)
        NearStack& push(std::initializer_list<T> il) noexcept(xyu::t_can_nothrow_cpassign<T>) { return push(xyu::make_range(il)); }

        /* 出栈 */

        /// 弹出一个元素
        T pop() noexcept(xyu::t_can_nothrow_mvconstr<T>)
        {
            T tmp = xyu::move(get(0));
            list.reduce(count() - 1);
            return tmp;
        }

        /// 弹出多个元素
        void pop(xyu::size_t count) noexcept
        {
            if (XY_LIKELY(count < this->count())) list.reduce(this->count() - count);
            else list.clear();
        }

        /* 范围 */

        /// 创建范围
        auto range() noexcept { return list.range().rrange(); }
        /// 创建范围 (const)
        auto range() const noexcept { return list.range().rrange(); }

        /* 运算符 */

        /// 索引访问
        T& operator[](xyu::size_t index) noexcept { return get(index); }
        /// 索引访问 (const)
        xyu::t_add_const<T>& operator[](xyu::size_t index) const noexcept { return get(index); }

        /// 入栈
        template <typename Arg>
        NearStack& operator<<(Arg&& arg) noexcept(noexcept(push(xyu::forward<Arg>(arg)))) { return push(xyu::forward<Arg>(arg)); }
    };

    // 推导指引
    template <typename Rg, typename = xyu::t_enable<xyu::t_can_range<Rg>>>
    NearStack(Rg&&) -> NearStack<xyu::t_rem_cr<xyu::t_access<Rg>>>;
}

/// 单向链表栈
namespace xylu::xycoll
{
    /**
     * @brief 单向链表栈
     * @tparam T  元素类型
     * @tparam cross_thread  是否允许跨线程内存管理。
     *         - false（默认）: 使用线程局部内存池，性能更好；
     *         - true : 使用全局内存分配，对象可跨线程重分配和释放。
     *
     * @details
     *   存储方式：堆分配节点，指针连接，基于 @see SLinkList
     *
     *   方法简述：
     *   - 支持 `release` 释放内存
     *   - 支持 `get`O(n) 获取元素，`get(0)`O(1) 获取栈顶元素
     *   - 支持 `push`O(1) 插入 多元素/范围/单元素，`pop`O(1) 出栈
     *   - 支持 `take`O(1) 与 `put`O(1) 实现节点级转移
     *
     *   注意事项：
     *   - 容量限制：增加元素超过 `limit` 时会触发 abort
     *   - 范围限制：传入非有限范围在 !XY_RELEASE 下检查并触发 abort
     *   - 索引越界：`get`、`op[]`、`take`单节点定位 会在 !XY_RELEASE 下检查并触发 abort，其余方法无害
     *
     *   异常安全：
     *   - 复制赋值：抛出异常时回滚至操作前状态
     *   - `push`：若元素构造抛出异常，已成功构造的元素保留，需要手动回滚或继续尝试
     */
    template <typename T, bool cross_thread = false>
    struct SLinkStack
    {
    private:
        SLinkList<T, cross_thread> list;
        
    public:
        /* 构造 */

        /// 默认构造
        SLinkStack() noexcept = default;

        /// 范围构造
        template <typename Rg, typename = xyu::t_enable<xyu::t_can_range<Rg>>>
        explicit SLinkStack(Rg&& range) noexcept(noexcept(SLinkList<T, cross_thread>{xyu::forward<Rg>(range)}))
        : list{xyu::make_range(xyu::forward<Rg>(range))} {}

        /// 交换
        SLinkStack& swap(SLinkStack& other) noexcept { list.swap(other.list); return *this; }

        /* 数据容量 */

        /// 获取最大容量
        constexpr static xyu::size_t limit() noexcept { return SLinkList<T, cross_thread>::limit(); }

        /// 获取元素个数
        xyu::size_t count() const noexcept { return list.count(); }
        /// 是否为空
        bool empty() const noexcept { return list.empty(); }

        /* 数据获取 */

        /// 获取元素 (0为栈顶)
        T& get(xyu::size_t index) noexcept { return list.get(index); }
        /// 获取元素 (const) (0为栈顶)
        xyu::t_add_const<T>& get(xyu::size_t index) const noexcept { return list.get(index); }

        /* 数据管理 */

        /// 释放内存
        void release() noexcept { list.release(); }

        /* 入栈 */

        /// 入栈 (多元素)
        template <typename... Args, typename = xyu::t_enable<(... && xyu::t_can_init<T, Args>)>>
        SLinkStack& push(Args&&... args) noexcept(noexcept(list.prepend(xyu::special_v, xyu::forward<Args>(args)...)))
        {
            (..., list.prepend(xyu::special_v, xyu::forward<Args>(args)));
            return *this;
        }

        /// 入栈 (范围)
        template <typename Rg, typename = xyu::t_enable<xyu::t_can_init<T, xyu::t_access<Rg>> && !xyu::t_can_init<T, Rg>>>
        SLinkStack& push(Rg&& range) noexcept(noexcept(list.prepend(xyu::special_v, *xyu::make_range(range).begin())))
        {
            for (auto&& v : xyu::make_range(xyu::forward<Rg>(range))) list.prepend(xyu::special_v, xyu::forward<decltype(v)>(v));
            return *this;
        }

        /// 入栈 (单元素)
        template <typename... Args, typename = xyu::t_enable<xyu::t_can_init<T, Args...>>>
        SLinkStack& push(xyu::special_t = {}, Args&&... args) noexcept(noexcept(list.prepend(xyu::special_v, xyu::forward<Args>(args)...)))
        {
            list.prepend(xyu::special_v, xyu::forward<Args>(args)...);
            return *this;
        }

        /// 入栈 (初始化列表)
        SLinkStack& push(std::initializer_list<T> il) noexcept(noexcept(push(xyu::make_range(il)))) { return push(xyu::make_range(il)); }

        /* 出栈 */

        /// 弹出一个元素
        T pop() noexcept(xyu::t_can_nothrow_mvconstr<T>)
        {
            T tmp = xyu::move(get(0));
            list.erase(0);
            return tmp;
        }

        /// 弹出多个元素
        void pop(xyu::size_t count) noexcept
        {
            list.erase(0, count);
        }

        /* 转移 */

        /// 入栈 (节点)
        SLinkStack& put(HeapVal<__::SLinkNode<T>, cross_thread>&& node) noexcept(noexcept(list.put(0, xyu::move(node))))
        {
            list.put(0, xyu::move(node));
            return *this;
        }
        /// 入栈 (多个节点)
        SLinkStack& put(SLinkStack&& stack) noexcept(noexcept(list.put(0, xyu::move(stack.list))))
        {
            list.put(0, xyu::move(stack.list));
            return *this;
        }

        /// 弹出节点
        HeapVal<__::SLinkNode<T>, cross_thread> take() noexcept(noexcept(list.take(0))) { return list.take(0); }
        /// 弹出多个节点
        SLinkStack take(xyu::size_t count) noexcept(noexcept(list.take(0, count))) { return list.take(0, count); }

        /* 范围 */

        /// 创建范围
        auto range() noexcept { return list.range(); }
        /// 创建范围 (const)
        auto range() const noexcept { return list.range(); }

        /* 运算符 */

        /// 索引访问
        T& operator[](xyu::size_t index) noexcept { return get(index); }
        /// 索引访问 (const)
        xyu::t_add_const<T>& operator[](xyu::size_t index) const noexcept { return get(index); }

        /// 入栈
        template <typename Arg>
        SLinkStack& operator<<(Arg&& arg) noexcept(noexcept(push(xyu::forward<Arg>(arg)))) { return push(xyu::forward<Arg>(arg)); }

    private:
        SLinkStack(SLinkList<T, cross_thread>&& list) noexcept : list{xyu::move(list)} {}
    };

    // 推导指引
    template <typename Rg, typename = xyu::t_enable<xyu::t_can_range<Rg>>>
    SLinkStack(Rg&&) -> SLinkStack<xyu::t_rem_cr<xyu::t_access<Rg>>>;
}
