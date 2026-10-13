#pragma once

/**
 * @file    xycoll/nearlist.h
 * @brief   数组列表
 * @date    2026-04-22
 */

#include "./__/tool.h"

namespace xylu::xycoll
{
    /**
     * @brief 动态数组
     * @tparam T  元素类型
     * @tparam cross_thread  是否允许跨线程内存管理。
     *         - false（默认）: 使用线程局部内存池，性能更好；
     *         - true : 使用全局内存分配，对象可跨线程重分配和释放。
     *
     * @details
     *   存储方式：
     *   - 堆分配数组，连续存储
     *   - K_grow_factor 控制扩容倍数，K_shrink_factor 控制缩容阈值
     *
     *   方法简述：
     *   - 支持 `reserve` 预分配，`reduce` 条件缩容，`release` 释放内存
     *   - 支持 `reduce`、`resize` 整体 缩减 或 扩展，`clear` 清空元素
     *   - 支持 `append`O(1)、`insert`O(n) 插入 多元素/范围/单元素，`erase`O(n) 删除元素
     *
     *   注意事项：
     *   - 容量限制：增加元素超过 `limit` 时会触发 abort
     *   - 范围限制：传入非有限范围在 !XY_RELEASE 下检查并触发 abort
     *   - 索引越界：`get`、`op[]` 会在 !XY_RELEASE 下检查并触发 abort，其余方法无害
     *   - 自引用：`insert` 待插入元素为插入点之后的元素引用时 XY_DEBUG 下会检查并触发 abort
     *   - 迭代器失效：`insert` 插入后会使 插入位置及之后的迭代器失效，
     *               `reserve`、`resize`扩展、`append`、`insert` 等操作触发扩容后会使所有旧迭代器失效，
     *               `erase` 在移动赋值可能抛出异常时会分配新内存，操作完毕后所有旧迭代器失效。
     *
     *   异常安全：
     *   - 复制赋值、`reserve`、`reduce`、`resize`、`insert`、`erase`：抛出异常时回滚至操作前状态
     *   - `append`、`resize`扩展：若元素构造抛出异常，已成功构造的元素保留，需要手动回滚或继续尝试
     */
    template <typename T, bool cross_thread = false>
    class NearList
    {
        static_assert(xyu::t_can_nothrow_destruct<T>, "do your type catch a bomb!??");

    public:
        /* 静态常量 */

        /// 释放空间条件比例
        static constexpr double K_shrink_factor = 0.5;
        /// 扩容比例
        static constexpr double K_grow_factor = 2;

    private:
        T* data;            // 数组指针
        xyu::size_t n;      // 使用的元素数量
        xyu::size_t capa;   // 数组容量

    public:
        /* 构造赋值 */

        /// 析构
        ~NearList() noexcept { release(); }

        /// 默认构造
        NearList() noexcept : data{nullptr}, n{0}, capa{0} {}

        /// 预分配空间构造
        explicit NearList(xyu::size_t capacity) noexcept : NearList{}
        {
            data = __::Common::alloc<T, cross_thread>(capacity);
            capa = capacity;
        }
        /// 分配空间并初始化
        NearList(xyu::size_t n, const T& value) noexcept(xyu::t_can_nothrow_cpconstr<T>) : NearList{} { insert(0, xyu::make_range_repeat(value, n)); }

        /// 范围构造
        template <typename Rg, typename = xyu::t_enable<xyu::t_can_init<T, xyu::t_access<Rg>> && !xyu::t_can_init<T, Rg>>>
        explicit NearList(Rg&& range) noexcept(noexcept(insert(0, xyu::forward<Rg>(range)))) : NearList(xyu::make_range(xyu::forward<Rg>(range)).count()) { insert(0, range); }

        /// 复制构造
        NearList(const NearList& other) noexcept(noexcept(insert(0, other))) : NearList{} { insert(0, other); }

        /// 复制赋值
        NearList& operator=(const NearList& other) noexcept(xyu::t_can_nothrow_cpconstr<T>)
        {
            if (XY_UNLIKELY(this == &other)) return *this;
            if constexpr (xyu::t_can_nothrow_cpconstr<T> && xyu::t_can_nothrow_cpassign<T>)
            {
                // 需要扩容
                if (capa < other.n)
                {
                    clear();
                    append(other.range());
                }
                // 容量足够
                else
                {
                    bool app = n < other.n;
                    for (xyu::size_t i = 0, j = app ? n : other.n; i < j; ++i) data[i] = other.data[i];
                    if (app) for (;n < other.n; ++n) xyu::place_init(data + n, other.data[n]);
                    else { for (auto i = other.n; i < n; ++i) data[i].~T(); n = other.n; }
                }
            }
            else
            {
                NearList tmp = other;
                *this = xyu::move(tmp);
            }
            return *this;
        }

        /* 移动交换 */

        /// 移动构造
        NearList(NearList&& other) noexcept : data{other.data}, n{other.n}, capa{other.capa}
        {
            other.data = nullptr;
            other.n = other.capa = 0;
        }

        /// 移动赋值
        NearList& operator=(NearList&& other) noexcept { return swap(other); }

        /// 交换
        NearList& swap(NearList& other) noexcept
        {
            xyu::swap(data, other.data);
            xyu::swap(n, other.n);
            xyu::swap(capa, other.capa);
            return *this;
        }

        /* 数据容量 */

        /// 获取最大容量
        constexpr static xyu::size_t limit() noexcept { return xyu::t_number<xyu::size_t>::max / xyu::max(2, sizeof(T)); }
        /// 获取当前容量
        xyu::size_t capacity() const noexcept { return capa; }

        /// 获取元素数量
        xyu::size_t count() const noexcept { return n; }
        /// 是否为空
        bool empty() const noexcept { return n == 0; }

        /* 数据获取 */

        /// 获取元素
        T& get(xyu::size_t index) noexcept
        {
            __::Checker::check_index(index, n, XY_CurPos("NearList::get"));
            return data[index];
        }
        /// 获取元素 (const)
        xyu::t_add_const<T>& get(xyu::size_t index) const noexcept
        {
            __::Checker::check_index(index, n, XY_CurPos("NearList::get"));
            return data[index];
        }

        /* 数据管理 */

        /// 预分配空间
        void reserve(xyu::size_t mincapa) noexcept(Nmc())
        {
            if (mincapa > capa) realloc_capa(calc_new_capa(mincapa));
        }
        /// 缩减容量 (仅当 元素数量 < 容量 * K_shrink_factor)
        void reduce() noexcept(Nmc())
        {
            if (n < capa * K_shrink_factor)
            {
                if (n == 0) release();
                else realloc_capa(n);
            }
        }
        
        /// 缩减元素数量
        void reduce(xyu::size_t newn) noexcept
        {
            if (XY_UNLIKELY(newn >= n)) return;
            for (xyu::size_t i=newn; i<n; ++i) data[i].~T();
            n = newn;
        }

        /// 调整元素数量 (可以缩减或扩容)
        void resize(xyu::size_t newn, const T& value) noexcept(xyu::t_can_nothrow_mvconstr<T> && xyu::t_can_nothrow_cpconstr<T>)
        {
            if (newn <= n) {
                for (xyu::size_t i=newn; i<n; ++i) data[i].~T();
                n = newn;
            } else {
                xyu::size_t pos = 0;
                bool alias = &value >= data && &value < data + n;
                if (XY_UNLIKELY(alias)) pos = &value - data;
                reserve(newn);
                if (XY_LIKELY(!alias)) while (n < newn) ::new (data + n) T{value}, ++n;
                else while (n < newn) ::new (data + n) T{data[pos]}, ++n;
            }
        }

        /// 清空元素
        void clear() noexcept
        {
            xyu::object_destroy(data, n);
            n = 0;
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

        /* 尾增 */

        /**
         * @brief 尾部增加元素 (多元素)
         * @details 将每一个 args 作为一个元素添加到尾部
         */
        template <typename... Args, typename = xyu::t_enable<sizeof...(Args) && (... && xyu::t_can_init<T, Args>)>,
                  bool nothrow_init = (... && xyu::t_can_nothrow_init<T, Args>)>
        NearList& append(Args&&... args) noexcept(nothrow_init && Nmc())
        {
            static_assert(sizeof...(args) <= limit());
            append_help<nothrow_init>(
                    n + sizeof...(args),
                    [&]{ (..., (xyu::place_init(data + n, xyu::forward<Args, xyu::t_can_nothrow_init<T, Args>>(args)), ++n)); },
                    [&](T* newdata){ (..., (xyu::place_init(newdata + n, xyu::forward<Args, nothrow_init && Nmc()>(args)), ++n)); });
            return *this;
        }

        /**
         * @brief 尾部增加元素 (范围)
         * @details 将 range 中的每一个元素添加到尾部
         */
        template <typename Rg, typename = xyu::t_enable<xyu::t_can_init<T, xyu::t_access<Rg>> && !xyu::t_can_init<T, Rg>>,
                  bool nothrow_init = xyu::t_can_nothrow_init<T, xyu::t_noexcept_access<Rg>>>
        NearList& append(Rg&& rg) noexcept(nothrow_init && Nmc())
        {
            auto range = xyu::make_range(xyu::forward<Rg>(rg));
            __::Checker::check_range(range, XY_CurPos("NearList::append"));
            append_help<nothrow_init>(
                    n + range.count(),
                    [&]{ for (auto&& v : range) xyu::place_init(data + n, xyu::forward<decltype(v), nothrow_init>(v)), ++n; },
                    [&](T* newdata){ for (auto&& v : range) xyu::place_init(newdata + n, xyu::forward<decltype(v), nothrow_init && Nmc()>(v)), ++n; });
            return *this;
        }

        /**
         * @brief 尾部增加元素 (单元素)
         * @details 将 args 作为一个元素的构造参数添加到尾部
         */
        template <typename... Args, typename = xyu::t_enable<xyu::t_can_init<T, Args...>>,
                  bool nothrow_init = xyu::t_can_nothrow_init<T, Args...>>
        NearList& append(xyu::special_t = {}, Args&&... args) noexcept(nothrow_init && Nmc())
        {
            append_help<nothrow_init>(
                    n + 1,
                    [&]{ xyu::place_init(data + n, xyu::forward<Args, nothrow_init>(args)...), ++n; },
                    [&](T* newdata){ xyu::place_init(newdata + n, xyu::forward<Args, nothrow_init && Nmc()>(args)...), ++n; });
            return *this;
        }

        /// 尾部增加元素 (初始化列表)
        NearList& append(std::initializer_list<T> il) noexcept(noexcept(append(xyu::make_range(il))))
        { return append(xyu::make_range(il)); }

        /* 插入 */

        /**
         * @brief 中间插入元素 (多元素)
         * @details 将每一个 args 作为一个元素依次插入到 index 位置 (若 index 越界，则在尾部插入)
         * @attention 禁止插入 index 位置及之后的自引用
         */
        template <typename... Args, typename = xyu::t_enable<sizeof...(Args) && (... && xyu::t_can_init<T, Args>)>,
                  bool nothrow_init = (... && (xyu::t_can_nothrow_init<T, Args>)), bool nothrow_assign = (... && (xyu::t_can_nothrow_assign<T, Args>))>
        NearList& insert(xyu::size_t index, Args&&... args) noexcept(nothrow_init && Nmc() && nothrow_assign && Nma())
        {
            static_assert(sizeof...(args) <= limit());
            auto func = "NearList::insert";
            auto ordi = index;
            insert_help<nothrow_init, nothrow_assign>(
                    index, sizeof...(Args),
                    [&]{ (..., (data[index++] = __::Checker::check_alias(xyu::forward<Args, nothrow_assign>(args), ordi, data, n, XY_CurPos(func)))); },
                    [&]{ (..., (xyu::place_init(data + n, xyu::forward<Args, nothrow_init>(args)), ++n)); },
                    [&](T* newdata, xyu::size_t& i){ (..., (xyu::place_init(newdata + i, __::Checker::check_alias(xyu::forward<Args, nothrow_init && Nmc()>(args), ordi, data, n, XY_CurPos(func))), ++i)); });
            return *this;
        }

        /**
         * @brief 中间插入元素 (范围)
         * @details 将 range 中的每一个元素依次插入到 index 位置 (若 index 越界，则在尾部插入)
         * @attention 禁止插入 index 位置及之后的自引用
         */
        template <typename Rg, typename = xyu::t_enable<xyu::t_can_init<T, xyu::t_access<Rg>> && !xyu::t_can_init<T, Rg>>,
                  bool nothrow_init = xyu::t_can_nothrow_init<T, xyu::t_noexcept_access<Rg>>, bool nothrow_assign = xyu::t_can_nothrow_assign<T&, xyu::t_noexcept_access<Rg>>>
        NearList& insert(xyu::size_t index, Rg&& rg) noexcept(nothrow_init && Nmc() && nothrow_assign && Nma())
        {
            auto range = xyu::make_range(xyu::forward<Rg>(rg));
            auto func = "NearList::insert";
            __::Checker::check_range(range, XY_CurPos(func));
            auto ordi = index;
            insert_help<nothrow_init, nothrow_assign>(
                    index, range.count(),
                    [&]{ for (auto&& v : range) data[index++] = __::Checker::check_alias(xyu::forward<decltype(v), nothrow_assign>(v), ordi, data, n, xyu::ErrorPos{XY_CurPos(func)}); },
                    [&]{ for (auto&& v : range) xyu::place_init(data + n, xyu::forward<decltype(v), nothrow_init>(v)), ++n; },
                    [&](T* newdata, xyu::size_t& i){ for (auto&& v : range) xyu::place_init(newdata + i, __::Checker::check_alias(xyu::forward<decltype(v), nothrow_init && Nmc()>(v), ordi, data, n, xyu::ErrorPos{XY_CurPos(func)})), ++i; });
            return *this;
        }

        /**
         * @brief 中间插入元素 (单元素)
         * @details 将 args 作为一个元素的构造参数依次插入到 index 位置 (若 index 越界，则在尾部插入)
         * @attention 禁止插入 index 位置及之后的自引用
         */
        template <typename... Args, typename = xyu::t_enable<xyu::t_can_init<T, Args...>>,
                  bool nothrow_init = xyu::t_can_nothrow_init<T, Args...>, bool nothrow_assign = (sizeof...(Args) == 1 && xyu::t_can_assign<T, Args...>) ? xyu::t_can_nothrow_assign<T, Args...> : (xyu::t_can_nothrow_init<T, Args...> && xyu::t_can_nothrow_mvassign<T>)>
        NearList& insert(xyu::size_t index, xyu::special_t = {}, Args&&... args) noexcept(nothrow_init && Nmc() && nothrow_assign && Nma())
        {
            auto func = "NearList::insert";
            auto ordi = index;
            insert_help<nothrow_init, nothrow_assign>(
                    index, 1,
                    [&]{
                        if constexpr (sizeof...(Args) == 1 && xyu::t_can_assign<T, Args...>) data[index] = (..., __::Checker::check_alias(xyu::forward<Args, nothrow_assign>(args), ordi, data, n, XY_CurPos(func)));
                        else data[index] = xyu::object_create<T>(__::Checker::check_alias(xyu::forward<Args, nothrow_assign>(args), ordi, data, n, XY_CurPos(func))...);
                    },
                    [&]{ xyu::place_init(data + n, xyu::forward<Args, nothrow_init>(args)...), ++n; },
                    [&](T* newdata, xyu::size_t& i){ xyu::place_init(newdata + i, __::Checker::check_alias(xyu::forward<Args, nothrow_init && Nmc()>(args), ordi, data, n, XY_CurPos(func))...), ++i; });
            return *this;
        }

        /**
         * @brief 中间插入元素 (初始化列表)
         * @attention 禁止插入 index 位置及之后的自引用
         */
        NearList& insert(xyu::size_t index, std::initializer_list<T> il) noexcept(noexcept(insert(index, xyu::make_range(il))))
        { return insert(index, xyu::make_range(il)); }

        /* 删除 */

        /// 删除索引开始的 count(默认为1) 个元素
        NearList& erase(xyu::size_t index, xyu::size_t count = 1) noexcept(xyu::t_can_nothrow_mvassign<T> || xyu::t_can_nothrow_cpconstr<T>)
        {
            if (XY_UNLIKELY(index >= n || count == 0)) return *this;
            if (n - index <= count)
            {
                xyu::object_destroy(data + index, data + n);
                n = index;
            }
            else if constexpr (xyu::t_can_nothrow_mvassign<T>)
            {
                xyu::size_t i;
                for (i = index; i < n - count; ++i) data[i] = xyu::move(data[i + count]);
                xyu::object_destroy(data + i, data + n);
                n -= count;
            }
            else
            {
                NearList tmp(n - count);
                using Range = xyu::Range<xyu::Iterator_Ptr<T>>;
                if (index) tmp.append(Range{index, data, data + index});
                tmp.append(Range{n - count - index, data + index + count, data + n});
                swap(tmp);
            }
            return *this;
        }

        /* 范围 */

        /// 创建范围
        auto range() noexcept { return xyu::Range<xyu::Iterator_Ptr<T, NearList>>(n, data, data + n); }
        /// 创建范围 (const)
        auto range() const noexcept { return const_cast<NearList*>(this)->range().crange(); }

        /* 运算符 */

        /// 索引访问
        T& operator[](xyu::size_t index) noexcept { return get(index); }
        /// 索引访问 (const)
        xyu::t_add_const<T>& operator[](xyu::size_t index) const noexcept { return get(index); }

        /// 尾部添加元素
        template <typename Arg>
        NearList& operator<<(Arg&& arg) noexcept(noexcept(append(xyu::forward<Arg>(arg))))
        { return append(xyu::forward<Arg>(arg)); }

    private:
        // 无异常移动构造
        static constexpr bool Nmc() noexcept { return xyu::t_can_nothrow_mvconstr<T>; }
        // 无异常移动赋值
        static constexpr bool Nma() noexcept { return xyu::t_can_nothrow_mvassign<T>; }

        // 计算新容量
        xyu::size_t calc_new_capa(xyu::size_t mincapa) const noexcept
        {
            __::Checker::check_capa<limit()>(mincapa, 0, XY_CurPos("NearList::reserve"));
            xyu::size_t newcapa = capa * K_grow_factor >= capa ? capa * K_grow_factor : limit();
            if (XY_UNLIKELY(newcapa > limit())) newcapa = limit();
            return xyu::max(mincapa, newcapa);
        }

        // 重新分配内存
        void realloc_capa(xyu::size_t newcapa) noexcept(xyu::t_can_nothrow_mvconstr<T>)
        {
            // 重新分配内存
            T* newdata = __::Common::alloc<T, cross_thread>(newcapa);
            // 资源转移
            if (XY_LIKELY(capa > 0))
            {
                if constexpr (xyu::t_can_nothrow_mvconstr<T> || xyu::t_can_nothrow_cpconstr<T>) {
                    for (xyu::size_t i = 0; i < n; ++i)
                    {
                        ::new (newdata + i) T{xyu::move<xyu::t_can_nothrow_mvconstr<T>>(data[i])};
                        data[i].~T();
                    }
                } else {
                    xyu::size_t i = 0;
                    try { for (; i < n; ++i) ::new (newdata + i) T{data[i]}; }
                    catch (...) {
                        for (xyu::size_t j = 0; j < i; ++j) newdata[j].~T();
                        __::Common::dealloc<cross_thread>(newdata, newcapa);
                        throw;
                    }
                    for (i = 0; i < n; ++i) data[i].~T();
                }
                // 释放旧内存
                __::Common::dealloc<cross_thread>(data, capa);
            }
            // 更新状态
            data = newdata;
            capa = newcapa;
        }

        // append 辅助函数
        // 扩容时，先构造新元素，再释放旧元素，保证插入容器内元素时不会因扩容而失效
        template <bool nothrow_init, typename AppFun, typename ReFun>
        void append_help(xyu::size_t newn, AppFun af, ReFun rf) noexcept(nothrow_init && Nmc())
        {
            if (newn <= capa) af();
            else {
                xyu::size_t newcapa = calc_new_capa(newn);
                T* newdata = __::Common::alloc<T, cross_thread>(newcapa);
                xyu::size_t tmp = n;
                // 构造新元素
                if constexpr (nothrow_init) rf(newdata);
                else try { rf(newdata); }
                    catch (...) {
                        while (n > tmp) newdata[--n].~T();
                        __::Common::dealloc<cross_thread>(newdata, newcapa);
                        throw;
                    }
                // 移动旧元素
                if constexpr (Nmc())
                    for (xyu::size_t i = 0; i < tmp; ++i) {
                        ::new (newdata + i) T{xyu::move(data[i])};
                        data[i].~T();
                    }
                else {
                    xyu::size_t i;
                    try { for (i = 0; i < tmp; ++i) ::new (newdata + i) T{data[i]}; }
                    catch (...) {
                        while (n > tmp) newdata[--n].~T();
                        while (i > 0) newdata[--i].~T();
                        __::Common::dealloc<cross_thread>(newdata, newcapa);
                        throw;
                    }
                    for (i = 0; i < tmp; ++i) data[i].~T();
                }
                // 释放旧内存
                if (XY_LIKELY(capa > 0)) __::Common::dealloc<cross_thread>(data, capa);
                // 更新状态
                data = newdata;
                capa = newcapa;
            }
        }

        // insert 辅助函数
        template <bool nothrow_init, bool nothrow_assign, typename InsFun, typename AppFun, typename ReFun>
        void insert_help(xyu::size_t index, xyu::size_t count, InsFun sf, AppFun af, ReFun rf)
        noexcept(nothrow_init && Nmc() && nothrow_assign && Nma())
        {
            if (XY_UNLIKELY(index >= n))
            {
                if constexpr (nothrow_init && Nmc())
                    append_help<nothrow_init>(n + count, af, [=](T* newdata){ rf(newdata, this->n); });
                else
                {
                    xyu::size_t tmp = n;
                    try { append_help<nothrow_init>(n + count, af, [=](T* newdata){ rf(newdata, this->n); }); }
                    catch (...) { while (n > tmp) data[--n].~T(); throw; }
                }
                return;
            }

            xyu::size_t newn = n + count;
            // 容量足够
            if (newn <= capa && Nma())
            {
                // 移动构造
                xyu::size_t i, l = n - xyu::min(count, n - index);
                if constexpr (Nmc())
                    for (i = l; i < n; ++i) ::new (data + i + count) T{xyu::move(data[i])};
                else try { for (i = l; i < n; ++i) ::new (data + i + count) T{data[i]}; }
                    catch (...) {
                        for (; l < i; ++l) data[l + count].~T();
                        throw;
                    }
                // 移动赋值
                for (i = l; i > index;) --i, data[i + count] = xyu::move(data[i]);
                n = newn;
                // 新元素赋值
                if constexpr (nothrow_assign) sf();
                else try { sf(); }
                    catch (...) {
                        n -= count;
                        for (i = index; i < n; ++i) data[i] = xyu::move(data[i + count]);
                        for (i = l; i < n; ++i) data[i + count].~T();
                        throw;
                    }
            }
            // 容量不足
            else
            {
                // 重新分配内存
                xyu::size_t newcapa = calc_new_capa(newn);
                T* newdata = __::Common::alloc<T, cross_thread>(newcapa);
                // 构造新元素
                xyu::size_t i = index;
                if constexpr (nothrow_init) rf(newdata, i);
                else try { rf(newdata, i); }
                    catch (...) {
                        for (xyu::size_t j = index; j < i; ++j) newdata[j].~T();
                        __::Common::dealloc<cross_thread>(newdata, newcapa);
                        throw;
                    }
                // 移动旧元素
                i = 0;
                if constexpr (Nmc())
                {
                    for (; i < index; ++i) ::new (newdata + i) T{xyu::move(data[i])}, data[i].~T();
                    for (; i < n; ++i) ::new (newdata + i + count) T{xyu::move(data[i])}, data[i].~T();
                }
                else
                {
                    try {
                        for (; i < index; ++i) ::new (newdata + i) T{data[i]};
                        for (; i < n; ++i) ::new (newdata + i + count) T{data[i]};
                    } catch (...) {
                        if (i >= index) for (xyu::size_t j = 0; j < i + count; ++j) newdata[j].~T();
                        else {
                            for (xyu::size_t j = 0; j < i; ++j) newdata[j].~T();
                            for (xyu::size_t j = index; j < index + count; ++j) newdata[j].~T();
                        }
                        __::Common::dealloc<cross_thread>(newdata, newcapa);
                        throw;
                    }
                    for (i = 0; i < n; ++i) data[i].~T();
                }
                // 释放旧内存
                if (XY_LIKELY(capa > 0)) __::Common::dealloc<cross_thread>(data, capa);
                // 更新状态
                n = newn;
                data = newdata;
                capa = newcapa;
            }
        }

    private:
        static_assert(K_grow_factor > 1);
        static_assert(K_shrink_factor <= 1 && K_shrink_factor >= 0);
    };

    // 推导指引
    template <typename Rg, typename = xyu::t_enable<xyu::t_can_range<Rg>>>
    NearList(Rg&&) -> NearList<xyu::t_rem_cr<xyu::t_access<Rg>>>;
}
