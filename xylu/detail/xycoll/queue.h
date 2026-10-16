#pragma once

/**
 * @file    xycoll/queue.h
 * @brief   队列
 * @date    2026-06-18
 */

#include "./slinklist.h"

/// 静态数组队列
namespace xylu::xycoll
{
    /**
     * @brief 静态循环队列
     * @tparam T    元素类型
     * @tparam Capa 最大容量
     *
     * @details
     *   存储方式：栈数组，循环缓冲区，编译期固定容量
     *
     *   方法简述：
     *   - 支持 `clear` 清空元素
     *   - 支持 `head`O(1)、`tail`O(1)、`get`O(1)、`op[]`O(1) 访问元素
     *   - 支持 `push`O(1) 入队 多元素/范围/单元素，`pop`O(1) 出队
     *
     *   注意事项：
     *   - 容量限制：入队元素总数超过 Capa 时会触发 abort
     *   - 索引越界：`head`、`tail`、`get`、`op[]` 会在 !XY_RELEASE 下检查并触发 abort，其余方法无害
     *   - 循环定位：当 `Capa` 为 2 的幂时，索引回绕使用位运算实现，否则需要分支判断
     *
     *   异常安全：
     *   - 复制/移动赋值：先清空再插入，异常时容器为空
     *   - `push`：若元素构造抛出异常，已成功构造的元素保留，需要手动回滚或继续尝试
     */
    template <typename T, xyu::size_t Capa>
    struct ArrayQueue
    {
        static_assert(Capa <= xyu::t_number<xyu::size_t>::max / xyu::max(2, sizeof(T)));
        static_assert(xyu::t_can_nothrow_destruct<T>);
    private:
        xyu::size_t n = 0;          // 元素个数
        xyu::size_t h = 0;          // 头指针
        xyu::size_t t = Capa - 1;   // 尾指针
        alignas(alignof(T)) char buf[sizeof(T) * Capa];

    public:
        /* 构造析构 */

        /// 析构
        ~ArrayQueue() noexcept { clear(); }

        /// 默认构造
        ArrayQueue() noexcept {}

        /// 范围构造
        template <typename Rg, typename = xyu::t_enable<xyu::t_can_range<Rg>>>
        explicit ArrayQueue(Rg&& range) noexcept(noexcept(push(xyu::forward<Rg>(range))))
        {
            if constexpr (noexcept(push(xyu::forward<Rg>(range))))
                push(xyu::forward<Rg>(range));
            else try { push(xyu::forward<Rg>(range)); }
                catch(...) { clear(); throw; }
        }

        /// 初始化列表构造
        ArrayQueue(std::initializer_list<T> il) noexcept(noexcept(push(il)))
        {
            if constexpr (noexcept(push(il))) push(il);
            else try { push(il); }
                catch(...) { clear(); throw; }
        }

        /* 复制移动 */

        /// 复制构造
        ArrayQueue(const ArrayQueue& other) noexcept(noexcept(push(other.range())))
        {
            if constexpr (noexcept(push(other.range()))) push(other.range());
            else try { push(other.range()); }
                catch(...) { clear(); throw; }
        }
        /// 移动构造
        ArrayQueue(ArrayQueue&& other) noexcept(noexcept(push(other.range().mrange())))
        {
            if constexpr (noexcept(push(other.range().mrange()))) push(other.range().mrange());
            else try { push(other.range().mrange()); }
                catch(...) { clear(); throw; }
        }

        /// 复制赋值
        ArrayQueue& operator=(const ArrayQueue& other) noexcept(noexcept(push(other.range())))
        {
            if (XY_UNLIKELY(this == &other)) return *this;
            clear();
            return push(other.range());
        }
        /// 移动赋值
        ArrayQueue& operator=(ArrayQueue&& other) noexcept(noexcept(push(other.range().mrange())))
        {
            if (XY_UNLIKELY(this == &other)) return *this;
            clear();
            return push(other.range().mrange());
        }

        /* 数据容量 */

        /// 获取容量
        static constexpr xyu::size_t capacity() noexcept { return Capa; }

        /// 获取元素个数
        xyu::size_t count() const noexcept { return n; }
        /// 是否为空
        bool empty() const noexcept { return n == 0; }

        /* 数据获取 */

        /// 获取队首元素
        T& head() noexcept
        {
            __::Checker::check_index(0, n, XY_CurPos("ArrayQueue::head"));
            return data()[h];
        }
        /// 获取队首元素 (const)
        xyu::t_add_const<T>& head() const noexcept
        {
            __::Checker::check_index(0, n, XY_CurPos("ArrayQueue::head"));
            return data()[h];
        }

        /// 获取队尾元素
        T& tail() noexcept
        {
            __::Checker::check_index(n - 1, n, XY_CurPos("ArrayQueue::tail"));
            return data()[t];
        }
        /// 获取队尾元素 (const)
        xyu::t_add_const<T>& tail() const noexcept
        {
            __::Checker::check_index(n - 1, n, XY_CurPos("ArrayQueue::tail"));
            return data()[t];
        }

        /// 获取任意元素
        T& get(xyu::size_t index) noexcept
        {
            __::Checker::check_index(index, n, XY_CurPos("ArrayQueue::get"));
            return data()[fix_index(h + index)];
        }
        /// 获取队尾元素
        xyu::t_add_const<T>& get(xyu::size_t index) const noexcept
        {
            __::Checker::check_index(index, n, XY_CurPos("ArrayQueue::get"));
            return data()[fix_index(h + index)];
        }

        /* 数据管理 */

        /// 清空元素
        void clear() noexcept
        {
            if (n == 0) return;
            if (h > t) { xyu::object_destroy(data() + h, data() + Capa); h = 0; }
            xyu::object_destroy(data() + h, data() + t + 1);
            h = n = 0;
            t = Capa - 1;
        }

        /* 入队 */

        /**
         * @brief 入队 (多元素)
         * @details 将每一个 args 作为一个元素逐个入队
         */
        template <typename... Args, typename = xyu::t_enable<sizeof...(Args) && (... && xyu::t_can_init<T, Args>)>>
        ArrayQueue& push(Args&&... args) noexcept((... && xyu::t_can_nothrow_init<T, Args>))
        {
            static_assert(sizeof...(args) <= capacity());
            __::Checker::check_capa<Capa>(n, sizeof...(args), XY_CurPos("ArrayQueue::push"));
            xyu::size_t index;
            (..., (
                index = fix_index(t + 1),
                xyu::place_init(data() + index, xyu::forward<Args, xyu::t_can_nothrow_init<T, Args>>(args)),
                t = index,
                ++n
            ));
            return *this;
        }

        /**
         * @brief 入队 (范围)
         * @details 将 range 中的每一个元素逐个入队
         */
        template <typename Rg, typename = xyu::t_enable<xyu::t_can_init<T, xyu::t_access<Rg>> && !xyu::t_can_init<T, Rg>>>
        ArrayQueue& push(Rg&& rg) noexcept(xyu::t_can_nothrow_init<T, xyu::t_noexcept_access<Rg>>)
        {
            auto range = xyu::make_range(xyu::forward<Rg>(rg));
            __::Checker::check_range(range, XY_CurPos("ArrayQueue::push"));
            __::Checker::check_capa<Capa>(n, range.count(), XY_CurPos("ArrayQueue::push"));
            xyu::size_t index;
            for (auto&& v : range)
            {
                index = fix_index(t + 1);
                xyu::place_init(data() + index, xyu::forward<decltype(v), xyu::t_can_nothrow_init<T, xyu::t_noexcept_access<Rg>>>(v)),
                t = index;
                ++n;
            }
            return *this;
        }

        /**
         * @brief 入队 (单元素)
         * @details 将 args 作为一个元素的构造参数入队
         */
        template <typename... Args, typename = xyu::t_enable<xyu::t_can_init<T, Args...>>>
        ArrayQueue& push(xyu::special_t = {}, Args&&... args) noexcept(xyu::t_can_nothrow_init<T, Args...>)
        {
            __::Checker::check_capa<Capa>(n, 1, XY_CurPos("ArrayQueue::push"));
            xyu::size_t index = fix_index(t + 1);
            xyu::place_init(data() + index, xyu::forward<Args, xyu::t_can_nothrow_init<T, Args...>>(args)...);
            t = index;
            ++n;
            return *this;
        }

        /// 入队 (初始化列表)
        ArrayQueue& push(std::initializer_list<T> il) noexcept(xyu::t_can_nothrow_cpconstr<T>) { return push(xyu::make_range(il)); }

        /* 出队 */

        /// 弹出一个元素
        T pop() noexcept(xyu::t_can_nothrow_mvconstr<T>)
        {
            T& v = head();
            T tmp = xyu::move(v);
            v.~T();
            h = fix_index(h + 1);
            --n;
            return tmp;
        }

        /// 弹出多个元素
        void pop(xyu::size_t count) noexcept
        {
            xyu::size_t num = xyu::min(count, n);
            for (xyu::size_t i = 0; i < num; ++i)
            {
                data()[h].~T();
                h = fix_index(h + 1);
            }
            n -= num;
        }

        /* 范围 */

        /// 获取范围
        auto range() noexcept
        {
            using Iter = xyu::t_chg_iter<xyu::Iterator_Ptr<T>, xyu::IterTag::Storage, Storage,
                xyu::IterTag::Dereference, Dereference, xyu::IterTag::Address, Address>;
            Iter begin{data()+h, data()+Capa};
            if (n == 0) return xyu::make_range(0, begin, begin);
            Iter end{h > t ? data()+t+Capa+1 : data()+t+1, data()+Capa};
            return xyu::make_range(n, begin, end);
        }
        /// 获取范围 (const)
        auto range() const noexcept { return const_cast<ArrayQueue*>(this)->range().crange(); }

        /* 运算符 */

        /// 索引访问
        T& operator[](xyu::size_t index) noexcept { return get(index); }
        /// 索引访问 (const)
        xyu::t_add_const<T>& operator[](xyu::size_t index) const noexcept { return get(index); }

        /// 入队
        template <typename Arg>
        ArrayQueue& operator<<(Arg&& arg) noexcept(noexcept(push(xyu::forward<Arg>(arg)))) { return push(xyu::forward<Arg>(arg)); }

    private:
        // 获取数组
        T* data() noexcept { return (T*)buf; }
        // 获取数组 (const)
        xyu::t_add_const<T>* data() const noexcept { return (T*)buf; }

        // 修正索引
        static xyu::size_t fix_index(xyu::size_t index) noexcept
        {
            if constexpr (xyu::bit_count_1_total(Capa) == 1) return index & (Capa - 1);
            else if (XY_UNLIKELY(index >= Capa)) return index - Capa;
            else return index;
        }

    private:
        // 迭代器组件
        struct Storage
        {
            using Owner_t = ArrayQueue;

            T* p;   // 数据指针
            T* rp;  // 右边界

            constexpr Storage(T* p, T* rp) : p(p), rp(rp) {}
        };

        struct Dereference
        {
            template <typename Iter>
            constexpr T& operator()(Iter& it) noexcept
            {
                if (it.p < it.rp) return *it.p;
                return *(it.p - Capa);
            }
        };

        struct Address
        {
            template <typename Iter>
            constexpr T* operator()(Iter& it) noexcept
            {
                if (it.p < it.rp) return it.p;
                return it.p - Capa;
            }
        };
    };
}

/// 动态数组队列
namespace xylu::xycoll
{
    /**
     * @brief 动态循环队列
     * @tparam T  元素类型
     * @tparam cross_thread  是否允许跨线程内存管理。
     *         - false（默认）: 使用线程局部内存池，性能更好；
     *         - true : 使用全局内存分配，对象可跨线程重分配和释放。
     *
     * @details
     *   存储方式：堆分配数组，循环缓冲区，容量始终为 2 的幂
     *
     *   方法简述：
     *   - 支持 `reserve` 预分配，`reduce` 条件缩容，`release` 释放内存，`clear` 清空元素
     *   - 支持 `head`O(1)、`tail`O(1)、`get`O(1)、`op[]`O(1) 访问元素
     *   - 支持 `push`O(1) 入队 多元素/范围/单元素，`pop`O(1) 出队
     *
     *   注意事项：
     *   - 容量限制：增加元素超过 `limit` 时会触发 abort
     *   - 范围限制：传入非有限范围在 !XY_RELEASE 下检查并触发 abort
     *   - 索引越界：`get`、`op[]` 会在 !XY_RELEASE 下检查并触发 abort
     *   - 迭代器失效：`reserve`、`reduce`、`push` 触发扩容后会使所有旧迭代器失效
     *
     *   异常安全：
     *   - 复制赋值：先清空再 push，抛出异常后队列为空
     *   - `reserve`、`reduce`、`resize`、`pop`：抛出异常时回滚至操作前状态
     *   - `push`：若元素构造抛出异常，已成功构造的元素保留，需要手动回滚或继续尝试
     */
    template <typename T, bool cross_thread = false>
    struct NearQueue
    {
        static_assert(xyu::t_can_nothrow_destruct<T>);
    private:
        T* data = nullptr;      // 数据指针
        xyu::size_t n = 0;      // 元素个数
        xyu::size_t h = 0;      // 头指针
        xyu::size_t t = -1;     // 尾指针
        xyu::size_t capa = 0;   // 容量

    public:
        /* 构造析构 */

        /// 析构
        ~NearQueue() noexcept { release(); }

        /// 默认构造
        NearQueue() noexcept {}

        /// 预分配空间构造
        explicit NearQueue(xyu::size_t capacity) noexcept
        : data(__::Common::alloc<T, cross_thread>(capacity = calc_new_capa(capacity))),
          t(capacity - 1), capa(capacity) {}

        /// 范围构造
        template <typename Rg, typename = xyu::t_enable<xyu::t_can_range<Rg>>>
        explicit NearQueue(Rg&& range) noexcept(noexcept(push(xyu::forward<Rg>(range))))
        {
            if constexpr (noexcept(push(xyu::forward<Rg>(range))))
                push(xyu::forward<Rg>(range));
            else try { push(xyu::forward<Rg>(range)); }
                catch(...) { release(); throw; }
        }

        /// 复制构造
        NearQueue(const NearQueue& other) noexcept(noexcept(push(other.range())))
        {
            if constexpr (noexcept(push(other.range()))) push(other.range());
            else try { push(other.range()); }
                catch(...) { release(); throw; }
        }

        /// 复制赋值
        NearQueue& operator=(const NearQueue& other) noexcept(noexcept(push(other.range())))
        {
            if (XY_UNLIKELY(this == &other)) return *this;
            clear();
            return push(other.range());
        }

        /* 移动交换 */

        /// 移动构造
        NearQueue(NearQueue&& other) noexcept : data{other.data}, n{other.n}, h{other.h}, t{other.t}, capa{other.capa}
        {
            other.data = nullptr;
            other.n = other.capa = other.h = 0;
            other.t = -1;
        }

        /// 移动赋值
        NearQueue& operator=(NearQueue&& other) noexcept { return swap(other); }

        /// 交换
        NearQueue& swap(NearQueue& other) noexcept
        {
            xyu::swap(data, other.data);
            xyu::swap(n, other.n);
            xyu::swap(h, other.h);
            xyu::swap(t, other.t);
            xyu::swap(capa, other.capa);
            return *this;
        }

        /* 数据容量 */

        /// 获取最大容量
        constexpr static xyu::size_t limit() noexcept { return xyu::bit_floor(xyu::t_number<xyu::size_t>::max / xyu::max(2, sizeof(T))); }
        /// 获取容量
        xyu::size_t capacity() noexcept { return capa; }

        /// 获取元素个数
        xyu::size_t count() const noexcept { return n; }
        /// 是否为空
        bool empty() const noexcept { return n == 0; }

        /* 数据获取 */

        /// 获取队首元素
        T& head() noexcept
        {
            __::Checker::check_index(0, n, XY_CurPos("NearQueue::head"));
            return data[h];
        }
        /// 获取队首元素 (const)
        xyu::t_add_const<T>& head() const noexcept
        {
            __::Checker::check_index(0, n, XY_CurPos("NearQueue::head"));
            return data[h];
        }

        /// 获取队尾元素
        T& tail() noexcept
        {
            __::Checker::check_index(n - 1, n, XY_CurPos("NearQueue::tail"));
            return data[t];
        }
        /// 获取队尾元素 (const)
        xyu::t_add_const<T>& tail() const noexcept
        {
            __::Checker::check_index(n - 1, n, XY_CurPos("NearQueue::tail"));
            return data[t];
        }

        /// 获取任意元素
        T& get(xyu::size_t index) noexcept
        {
            __::Checker::check_index(index, n, XY_CurPos("NearQueue::get"));
            return data[fix_index(h + index)];
        }
        /// 获取队尾元素
        xyu::t_add_const<T>& get(xyu::size_t index) const noexcept
        {
            __::Checker::check_index(index, n, XY_CurPos("NearQueue::get"));
            return data[fix_index(h + index)];
        }

        /* 数据管理 */

        /// 预分配空间
        void reserve(xyu::size_t mincapa) noexcept(xyu::t_can_nothrow_mvconstr<T> || xyu::t_can_nothrow_cpconstr<T>)
        {
            if (mincapa <= capa) return;
            NearQueue q(mincapa);
            if constexpr (xyu::t_can_nothrow_mvconstr<T>) q.push(range().mrange());
            else q.push(range());
            swap(q);
        }
        /// 缩减容量
        void reduce() noexcept(xyu::t_can_nothrow_mvconstr<T> || xyu::t_can_nothrow_cpconstr<T>)
        {
            if (n == 0) return release();
            if (calc_new_capa(n) == capa) return;
            NearQueue q(n);
            if constexpr (xyu::t_can_nothrow_mvconstr<T>) q.push(range().mrange());
            else q.push(range());
            swap(q);
        }

        /// 清空元素
        void clear() noexcept
        {
            if (n == 0) return;
            if (h > t) { xyu::object_destroy(data + h, data + capa); h = 0; }
            xyu::object_destroy(data + h, data + t + 1);
            h = n = 0;
            t = capa - 1;
        }
        /// 释放内存
        void release() noexcept
        {
            if (XY_UNLIKELY(capa == 0)) return;
            clear();
            __::Common::dealloc<cross_thread>(data, capa);
            data = nullptr;
            capa = 0;
        }

        /* 入队 */

        /**
         * @brief 入队 (多元素)
         * @details 将每一个 args 作为一个元素逐个入队
         */
        template <typename... Args, typename = xyu::t_enable<sizeof...(Args) && (... && xyu::t_can_init<T, Args>)>>
        NearQueue& push(Args&&... args) noexcept((... && xyu::t_can_nothrow_init<T, Args>))
        {
            static_assert(sizeof...(args) <= limit());
            reserve(n + sizeof...(args));
            xyu::size_t index;
            (..., (
                index = fix_index(t + 1),
                xyu::place_init(data + index, xyu::forward<Args, xyu::t_can_nothrow_init<T, Args>>(args)),
                t = index,
                ++n
            ));
            return *this;
        }

        /**
         * @brief 入队 (范围)
         * @details 将 range 中的每一个元素逐个入队
         */
        template <typename Rg, typename = xyu::t_enable<xyu::t_can_init<T, xyu::t_access<Rg>> && !xyu::t_can_init<T, Rg>>>
        NearQueue& push(Rg&& rg) noexcept(xyu::t_can_nothrow_init<T, xyu::t_noexcept_access<Rg>>)
        {
            auto range = xyu::make_range(xyu::forward<Rg>(rg));
            __::Checker::check_range(range, XY_CurPos("NearQueue::push"));
            reserve(n + range.count());
            for (auto&& v : range)
            {
                xyu::size_t index = fix_index(t + 1);
                xyu::place_init(data + index, xyu::forward<decltype(v), xyu::t_can_nothrow_init<T, xyu::t_noexcept_access<Rg>>>(v)),
                t = index;
                ++n;
            }
            return *this;
        }

        /**
         * @brief 入队 (单元素)
         * @details 将 args 作为一个元素的构造参数入队
         */
        template <typename... Args, typename = xyu::t_enable<xyu::t_can_init<T, Args...>>>
        NearQueue& push(xyu::special_t = {}, Args&&... args) noexcept(xyu::t_can_nothrow_init<T, Args...>)
        {
            reserve(n + 1);
            xyu::size_t index = fix_index(t + 1);
            xyu::place_init(data + index, xyu::forward<Args, xyu::t_can_nothrow_init<T, Args...>>(args)...);
            t = index;
            ++n;
            return *this;
        }

        /// 入队 (初始化列表)
        NearQueue& push(std::initializer_list<T> il) noexcept(xyu::t_can_nothrow_cpconstr<T>) { return push(xyu::make_range(il)); }

        /* 出队 */

        /// 弹出一个元素
        T pop() noexcept(xyu::t_can_nothrow_mvconstr<T>)
        {
            T& v = head();
            T tmp = xyu::move(v);
            v.~T();
            h = fix_index(h + 1);
            --n;
            return tmp;
        }

        /// 弹出多个元素
        void pop(xyu::size_t count) noexcept
        {
            xyu::size_t num = xyu::min(count, n);
            for (xyu::size_t i = 0; i < num; ++i)
            {
                data[h].~T();
                h = fix_index(h + 1);
            }
            n -= num;
        }

        /* 范围 */

        /// 获取范围
        auto range() noexcept
        {
            using Iter = xyu::t_chg_iter<xyu::Iterator_Ptr<T>, xyu::IterTag::Storage, Storage,
                xyu::IterTag::Dereference, Dereference, xyu::IterTag::Address, Address>;
            Iter begin{data+h, data+capa, capa};
            if (n == 0) return xyu::make_range(0, begin, begin);
            Iter end{h > t ? data+t+capa+1 : data+t+1, data+capa, capa};
            return xyu::make_range(n, begin, end);
        }
        /// 获取范围 (const)
        auto range() const noexcept { return const_cast<NearQueue*>(this)->range().crange(); }

        /* 运算符 */

        /// 索引访问
        T& operator[](xyu::size_t index) noexcept { return get(index); }
        /// 索引访问 (const)
        xyu::t_add_const<T>& operator[](xyu::size_t index) const noexcept { return get(index); }

        /// 入队
        template <typename Arg>
        NearQueue& operator<<(Arg&& arg) noexcept(noexcept(push(xyu::forward<Arg>(arg)))) { return push(xyu::forward<Arg>(arg)); }

    private:
        // 修正索引
        xyu::size_t fix_index(xyu::size_t index) noexcept { return index & (capa - 1); }

        // 计算新容量
        static xyu::size_t calc_new_capa(xyu::size_t mincapa) noexcept
        {
            __::Checker::check_capa<limit()>(mincapa, 0, XY_CurPos("NearQueue::reserve"));
            return xyu::bit_ceil(mincapa);
        }

    private:
        // 迭代器组件
        struct Storage
        {
            using Owner_t = NearQueue;

            T* p;               // 数据指针
            T* rp;              // 右边界
            xyu::size_t capa;   // 容量

            constexpr Storage(T* p, T* rp, xyu::size_t capa) noexcept : p(p), rp(rp), capa(capa) {}
        };

        struct Dereference
        {
            template <typename Iter>
            constexpr T& operator()(Iter& it) noexcept
            {
                if (it.p < it.rp) return *it.p;
                return *(it.p - it.capa);
            }
        };

        struct Address
        {
            template <typename Iter>
            constexpr T* operator()(Iter& it) noexcept
            {
                if (it.p < it.rp) return it.p;
                return it.p - it.capa;
            }
        };
    };

    // 推导指引
    template <typename Rg, typename = xyu::t_enable<xyu::t_can_range<Rg>>>
    NearQueue(Rg&&) -> NearQueue<xyu::t_rem_cr<xyu::t_access<Rg>>>;
}

/// 单向链表队列
namespace xylu::xycoll
{
    /**
     * @brief 单向链表队列
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
     *   - 支持 `get`O(n) 获取元素，`head`O(1)，`tail`O(1) 获取队列首尾元素
     *   - 支持 `push`O(1) 插入 多元素/范围/单元素，`pop`O(1) 出队
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
    struct SLinkQueue
    {
    private:
        SLinkList<T, cross_thread> list;
        
    public:
        /* 构造 */
        
        /// 默认构造
        SLinkQueue() noexcept {}

        /// 范围构造
        template <typename Rg, typename = xyu::t_enable<xyu::t_can_range<Rg>>>
        explicit SLinkQueue(Rg&& range) noexcept(noexcept(SLinkList<T, cross_thread>(xyu::forward<Rg>(range))))
        : list(xyu::forward<Rg>(range)) {}

        /// 交换
        SLinkQueue& swap(SLinkQueue& other) noexcept { list.swap(other.list); return *this; }
        
        /* 数据容量 */

        /// 获取最大容量
        static constexpr xyu::size_t limit() noexcept { return SLinkList<T, cross_thread>::limit(); }

        /// 获取元素个数
        xyu::size_t count() const noexcept { return list.count(); }
        /// 是否为空
        bool empty() const noexcept { return list.empty(); }

        /* 数据获取 */

        /// 获取队首元素
        T& head() noexcept { return list.head(); }
        /// 获取队首元素 (const)
        xyu::t_add_const<T>& head() const noexcept { return list.head(); }

        /// 获取队尾元素
        T& tail() noexcept { return list.tail(); }
        /// 获取队尾元素 (const)
        xyu::t_add_const<T>& tail() const noexcept { return list.tail(); }

        /// 获取任意元素
        T& get(xyu::size_t index) noexcept { return list.get(index); }
        /// 获取队尾元素
        xyu::t_add_const<T>& get(xyu::size_t index) const noexcept { return list.get(index); }
        
        /* 数据管理 */

        /// 释放内存
        void release() noexcept { list.release(); } 

        /* 入队 */

        /**
         * @brief 入队
         * @details 多元素 | 范围 | xyu::special_v + 单元素
         */
        template <typename... Args, typename = decltype(list.append(xyu::t_val<Args>()...))>
        SLinkQueue& push(Args&&... args) noexcept(noexcept(list.append(xyu::forward<Args>(args)...)))
        {
            list.append(xyu::forward<Args>(args)...);
            return *this;
        }

        /// 入队 (初始化列表)
        SLinkQueue& push(std::initializer_list<T> il) noexcept(xyu::t_can_nothrow_cpconstr<T>) { return push(xyu::make_range(il)); }

        /* 出队 */

        /// 弹出一个元素
        T pop() noexcept(xyu::t_can_nothrow_mvconstr<T>)
        {
            T tmp = xyu::move(head());
            list.erase(0, 1);
            return tmp;
        }

        /// 弹出多个元素
        void pop(xyu::size_t count) noexcept { list.erase(0, count); }

        /* 转移 */

        /// 入队 (节点)
        SLinkQueue& put(HeapVal<__::SLinkNode<T>, cross_thread>&& node) noexcept(noexcept(list.put(-1, xyu::move(node))))
        {
            list.put(-1, xyu::move(node));
            return *this;
        }
        /// 入队 (多个节点)
        SLinkQueue& put(SLinkQueue&& queue) noexcept(noexcept(list.put(-1, xyu::move(queue.list))))
        {
            list.put(-1, xyu::move(queue.list));
            return *this;
        }

        /// 弹出节点
        HeapVal<__::SLinkNode<T>, cross_thread> take() noexcept(noexcept(list.take(0))) { return list.take(0); }
        /// 弹出多个节点
        SLinkQueue take(xyu::size_t count) noexcept(noexcept(list.take(0, count))) { return list.take(0, count); }

        /* 范围 */

        /// 获取范围
        auto range() noexcept { return list.range(); }
        /// 获取范围 (const)
        auto range() const noexcept { return list.range(); }

        /* 运算符 */

        /// 索引访问
        T& operator[](xyu::size_t index) noexcept { return get(index); }
        /// 索引访问 (const)
        xyu::t_add_const<T>& operator[](xyu::size_t index) const noexcept { return get(index); }

        /// 入队
        template <typename Arg>
        SLinkQueue& operator<<(Arg&& arg) noexcept(noexcept(push(xyu::forward<Arg>(arg)))) { return push(xyu::forward<Arg>(arg)); }

    private:
        SLinkQueue(SLinkList<T, cross_thread>&& list) noexcept : list{xyu::move(list)} {}
    };

    // 推导指引
    template <typename Rg, typename = xyu::t_enable<xyu::t_can_range<Rg>>>
    SLinkQueue(Rg&&) -> SLinkQueue<xyu::t_rem_cr<xyu::t_access<Rg>>>;
}
