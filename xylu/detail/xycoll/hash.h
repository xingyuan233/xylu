#pragma once

/**
 * @file    hash.h
 * @brief   哈希表
 * @date    2026-06-20
 */

#include "./__/tool.h"
#include "./__/node.h"
#include "../xycore/log.h"
#include "../xyrange/cmp.h"

/// 哈希表基类
namespace xylu::xycoll::__
{
    // 获取控制块中每个元素据的最高有效位
    xyu::uint32 simd_msb(const xyu::uint8* ctrl) noexcept;
    // 获取控制块每个字节与val的是否相等
    xyu::uint32 simd_eq(const xyu::uint8* ctrl, xyu::uint8 val) noexcept;

    // 哈希表基类
    template <typename Key, typename Val, bool cross_thread = false>
    struct HashTableBase
    {
        static_assert(xyu::t_can_hash<Key>);

        /// 负载因子
        static constexpr double K_load_factor = 0.875;
        /// 缩容比例限制
        static constexpr double K_shrink_factor = 0.5;

    protected:
        using Node = KVNode<Key, Val>;

        // 控制块大小
#ifdef __AVX2__
        static constexpr xyu::size_t BlockSize = 32;
        static constexpr xyu::uint32 FuncMask = 0xFFFFFFFF;
#else
        static constexpr xyu::size_t BlockSize = 16;
        static constexpr xyu::uint16 FuncMask = 0xFFFF;
#endif
        // 控制块位数
        static constexpr xyu::size_t BlockBits = xyu::bit_count_0_low(BlockSize);

        /// 标记常量
        enum Tag : xyu::uint8
        {
            EMPTY   = 0x00,     // 空标记   0b 0000 0000 (此位置 可以插入 并且 可以结束探测)
            DELETED = 0x7f,     // 删除标记 0b 0111 1111 (此位置 可以插入 但 不可以结束探测)
            USED    = 0x80,     // 占用位   0b 1xxx xxxx (低7位为 hash 值的低位)
        };

    protected:
        xyu::uint8* data;       // 数据
        xyu::size_t n;          // 元素数量
        xyu::size_t capa;       // 容量        (容量 = 总容量 * 负载因子)
        xyu::size_t groups_;    // 控制块总数-1 (总容量 = 控制块数量 * BlockSize)

    public:
        /* 构造析构 */

        /// 默认构造
        HashTableBase(xyu::uint8* data = nullptr, xyu::size_t n = 0, xyu::size_t capa = 0, xyu::size_t groups = 0) noexcept
        : data{data}, n{n}, capa{capa}, groups_{groups} {}

        /// 预分配空间
        explicit HashTableBase(xyu::size_t mincapa) noexcept : HashTableBase{}
        {
            // 计算容量
            xyu::size_t newtotal = calc_new_total(mincapa);
            xyu::size_t total_size = calc_size(newtotal);
            // 分配内存
            if constexpr (!cross_thread) data = xyu::alloc<xyu::uint8>(total_size, calc_align());
            else data = xyu::alloc<xyu::uint8>(xyu::native_v, total_size, calc_align());
            // 内存结构      [ctrl_bytes, ......, data_bytes，......]
            xyu::mem_set(data, EMPTY, newtotal);
            groups_ = (newtotal >> BlockBits) - 1;
            static_assert(xyu::size_t(K_load_factor * BlockSize) >= 1);
            capa = newtotal * K_load_factor;
        }

        /// 析构函数
        ~HashTableBase() noexcept { release(); }

        /// 移动
        void swap(HashTableBase& other) noexcept
        {
            xyu::swap(data, other.data);
            xyu::swap(n, other.n);
            xyu::swap(capa, other.capa);
            xyu::swap(groups_, other.groups_);
        }

        /* 数据容量 */

        /// 获取最大容量
        constexpr static xyu::size_t limit() noexcept
        {
            // 防止加法溢出
            xyu::size_t lim1 = xyu::size_t(-1) / 2;
            // 内存分配限制
            constexpr
            xyu::size_t lim2 = (xyu::size_t(-1) - calc_align() + 1) / (sizeof(Node) + 1) - calc_align();
            // 位操作 2^n 限制
            xyu::size_t lim3 = xyu::bit_floor(xyu::min(lim1, lim2));
            // 确保对任意 capa <= limit(), size_t(capa / factor) <= lim3
            // 计算 K_load_factor 误差上界
            double ulp = xyu::t_number<double>::ulp(lim3);
            double up = xyu::calc_floor(ulp / K_load_factor) + 1.0;
            if (up > lim3) return 0;
            xyu::size_t lim4 = lim3 - xyu::size_t(up);
            return lim4 * K_load_factor;

            /*
               证明：对任意 capa <= limit(), 有 size_t((double)capa / K_load_factor) <= lim3.

               设 L = lim3（正整数，无需是 2 的幂），K = K_load_factor（0 < K <= 1），
                   u = ulp((double)L)，
                   up = floor(u / K) + 1，
                   lim4 = L - up.
               由 limit() 实现知 capa <= floor(lim4 * K) <= lim4 * K.

               对任意满足上述条件的 capa：
               1. 精确商 t = capa / K <= lim4 = L - up.
               2. 浮点计算 v = (double)capa / K 的误差 Δ = v - t：
                  - 整数 → double 舍入误差 ≤ 0.5u（因 capa ≤ L，ulp(capa) ≤ u）；
                  - 除法舍入误差 ≤ 0.5u（因精确商 t < L，故 ulp(t) ≤ u，除法正确舍入）。
                  故正向最大误差 Δ ≤ 0.5u/K + 0.5u ≤ u/K（因 K ≤ 1）。
               3. 由 up 定义知 up > u/K，于是 Δ < up.
               4. 因此 v = t + Δ < (L - up) + up = L.

               由 v < L 且 L 为整数，得 size_t(v) ≤ L - 1 < L = lim3. 证毕。
            */
        }

        /// 获取当前容量
        xyu::size_t capacity() const noexcept { return capa; }

        /// 获取元素数量
        xyu::size_t count() const noexcept { return n; }
        /// 是否为空
        bool empty() const noexcept { return n == 0; }

        /* 数据管理 */

        /// 清空元素
        void clear() noexcept
        {
            if (XY_UNLIKELY(n == 0)) return;
            clear_help<true>();
        }

        /// 释放内存
        void release() noexcept
        {
            if (XY_UNLIKELY(data == nullptr)) return;
            // 情况元素
            clear_help<false>();
            // 释放内存
            if constexpr (!cross_thread)
            {
                xyu::size_t total_size = calc_size((groups_ + 1) << BlockBits);
                xyu::dealloc(data, total_size, calc_align());
            }
            else xyu::dealloc(xyu::native_v, data, calc_align());
            // 重置状态
            data = nullptr;
            n = capa = groups_ = 0;
        }

    protected:
        // 计算新容量
        xyu::size_t calc_new_total(xyu::size_t mincapa) const noexcept
        {
            if (XY_UNLIKELY(mincapa > limit()))
                xyu::E_Logic::abort_over_limit(XY_CurPos("HashTable::HashTable"), mincapa, 0, limit(), "newcapa");
            xyu::size_t acctual_capa = mincapa / K_load_factor;
            xyu::size_t fixed_capa = xyu::bit_ceil(acctual_capa);
            if (XY_UNLIKELY(fixed_capa < BlockSize)) fixed_capa = BlockSize;
            return fixed_capa;
        }

        // 计算总空间
        static xyu::size_t calc_size(xyu::size_t count) noexcept
        {
            xyu::size_t ctrl_size = count;
            if constexpr (alignof(Node) > BlockSize) ctrl_size = (ctrl_size + alignof(Node) - 1) & -alignof(Node);
            xyu::size_t data_size = count * sizeof(Node);
            if constexpr (alignof(Node) < BlockSize) data_size = (data_size + BlockSize - 1) & -BlockSize;
            return  ctrl_size + data_size;
        }

        // 计算对齐
        static constexpr xyu::size_t calc_align() noexcept
        {
            return xyu::max(alignof(Node), BlockSize);
        }

        // 获取节点
        Node& get_node(xyu::size_t index) const noexcept
        {
            xyu::size_t ctrl_size = (groups_ + 1) << BlockBits;
            if constexpr (alignof(Node) > BlockSize) ctrl_size = (ctrl_size + alignof(Node) - 1) & -alignof(Node);
            return ((Node*)(data + ctrl_size))[index];
        }

        // 释放节点
        template <bool reset>
        void clear_help() noexcept
        {
            for (xyu::size_t index = 0; index <= groups_ * BlockSize; index += BlockSize)
            {
                if (xyu::t_rem_cr<decltype(FuncMask)> mask = simd_msb(data + index))
                {
                    do {
                        xyu::uint32 offset = xyu::bit_count_0_low(mask);
                        auto& node = get_node(index + offset);
                        node.~Node();
                        mask &= mask - 1;
                    } while (mask);
                    if constexpr (reset) xyu::mem_set(data + index, EMPTY, BlockSize);
                }
            }
            n = 0;
        }

    protected:
        // 异常检测
        template <typename K, typename... Args>
        static constexpr bool Nc() noexcept { return xyu::t_can_nothrow_init<Node, K, Args...>; }

        // 根据 Val 返回
        static decltype(auto) return_by_node(Node& node) noexcept
        {
            if constexpr (!xyu::t_is_void<Val>) return (node.val);
        }

        struct Location { Node* node; bool find; xyu::uint8 tag; xyu::uint32 offset; xyu::size_t index; };
        // locate
        template <bool need_insert, bool need_find = true, typename K>
        Location locate(const K& key) noexcept
        {
            xyu::size_t hash = xyu::make_hash(key);
            xyu::uint8 tag = (hash & 0x7f) | USED;
            xyu::size_t block = (hash >> 7) & groups_;
            xyu::size_t start = block;

            Location location{nullptr, false};
            if (XY_UNLIKELY(!data)) return location;
            for(;;)
            {
                xyu::size_t index = block << BlockBits;
                xyu::t_rem_cr<decltype(FuncMask)> mask;
                // 查找是否已存在
                if constexpr (need_find)
                {
                    mask = simd_eq(data + index, tag);
                    while (mask)
                    {
                        xyu::uint32 offset = xyu::bit_count_0_low(mask);
                        auto& node = get_node(index + offset);
                        if (xyu::equals(node.key, key)) return {&node, true, tag, offset, index};
                        mask &= mask - 1;
                    }
                }
                // 查找空位
                if constexpr (need_insert)
                {
                    if (!location.node)
                    {
                        mask = simd_msb(data + index);
                        if (mask == FuncMask) goto skip_eq;
                        xyu::uint32 offset = xyu::bit_count_1_low(mask);
                        location = {&get_node(index + offset), false, tag, offset, index};
                        if (data[index + offset] == EMPTY) return location;
                        if ((mask | 1u << offset) == FuncMask) goto skip_eq;
                    }
                    if (simd_eq(data + index, EMPTY)) return location;
                skip_eq:;
                }
                else if (simd_eq(data + index, EMPTY)) break;
                block = (block + 1) & groups_;

                if constexpr (!need_insert) if (block == start) break;
            }
            if constexpr (need_insert) XY_UNREACHABLE;
            return location;
        }

        // select 辅助
        template <typename K, bool nothrow = xyu::t_is_same_hash<Key,K> || xyu::t_can_nothrow_init<Key,K>>
        decltype(auto) select_impl(K&& key) noexcept(nothrow)
        {
            if constexpr (!xyu::t_is_same_hash<Key, K>)
                return select_impl(xyu::object_create<Key>(xyu::forward<K,nothrow>(key)));
            else
            {
                Location location = locate<false, true>(key);
                if constexpr (xyu::t_is_void<Val>) return location.find;
                else return location.find ? &location.node->val : nullptr;
            }
        }

        // append 辅助
        template <typename K, typename... Args>
        static constexpr bool append_impl_noexcept() noexcept
        {
            if constexpr (!xyu::t_is_same_hash<Key, K>)
                return xyu::t_can_nothrow_init<Key,K> && append_impl_noexcept<Key,Args...>();
            else return xyu::t_can_nothrow_init<Node, K, Args...>;
        }
        template <typename K, typename... Args, bool nothrow = append_impl_noexcept<K,Args...>()>
        decltype(auto) append_impl(K&& key, Args&&... args) noexcept(nothrow)
        {
            if constexpr (!xyu::t_is_same_hash<Key, K>)
                return append_impl(xyu::object_create<Key>(xyu::forward<K,nothrow>(key)), xyu::forward<Args>(args)...);
            else
            {
                Location location = locate<true, false>(key);
                ::new (location.node) Node{xyu::forward<K,Nc<K,Args...>()>(key), xyu::forward<Args,Nc<K,Args...>()>(args)...};
                data[location.index + location.offset] = location.tag;
                ++n;
                return return_by_node(*location.node);
            }
        }

        // insert 辅助
        template <typename K, typename... Args, bool nothrow = append_impl_noexcept<K,Args...>()>
        decltype(auto) insert_impl(K&& key, Args&&... args) noexcept(nothrow)
        {
            if constexpr (!xyu::t_is_same_hash<Key, K>)
                return insert_impl(xyu::object_create<Key>(xyu::forward<K,nothrow>(key)), xyu::forward<Args>(args)...);
            else
            {
                Location location = locate<true, true>(key);
                if (location.find) return return_by_node(*location.node);
                ::new (location.node) Node{xyu::forward<K,Nc<K,Args...>()>(key), xyu::forward<Args,Nc<K,Args...>()>(args)...};
                data[location.index + location.offset] = location.tag;
                ++n;
                return return_by_node(*location.node);
            }
        }

        // update 辅助
        template <typename K, typename... Args>
        static constexpr bool update_impl_noexcept() noexcept
        {
            if constexpr (!xyu::t_is_same_hash<Key, K>)
                return xyu::t_can_nothrow_init<Key,K> && update_impl_noexcept<Key,Args...>();
            else if constexpr (!xyu::t_can_nothrow_init<Node, K, Args...>)
                return false;
            else if constexpr (sizeof...(Args) == 1 && xyu::t_can_assign<Val, Args...>)
                return xyu::t_can_nothrow_assign<Val, Args...>;
            else
                return xyu::t_can_nothrow_mvassign<Val>;
        }
        template <typename K, typename... Args, bool nothrow = update_impl_noexcept<K,Args...>()>
        decltype(auto) update_impl(K&& key, Args&&... args) noexcept(nothrow)
        {
            if constexpr (!xyu::t_is_same_hash<Key, K>)
                return update_impl(xyu::object_create<Key>(xyu::forward<K,nothrow>(key)), xyu::forward<Args>(args)...);
            else
            {
                Location location = locate<true, true>(key);
                // 更新已存在节点的值
                if (location.find)
                {
                    if constexpr (sizeof...(Args) == 1 && xyu::t_can_assign<Val, Args...>)
                        location.node->val = (..., xyu::forward<Args,xyu::t_can_nothrow_assign<Val, Args...>>(args));
                    else
                        location.node->val = xyu::object_create<Val>(xyu::forward<Args,nothrow>(args)...);
                    return return_by_node(*location.node);
                }
                // 插入新节点
                ::new (location.node) Node{xyu::forward<K,Nc<K,Args...>()>(key), xyu::forward<Args,Nc<K,Args...>()>(args)...};
                data[location.index + location.offset] = location.tag;
                ++n;
                return return_by_node(*location.node);
            }
        }

        // erase 辅助
        template <typename K>
        bool erase_impl(K&& key) noexcept(xyu::t_is_same_hash<Key, K> || xyu::t_can_nothrow_init<Key,K>)
        {
            if constexpr (!xyu::t_is_same_hash<Key, K>)
                return erase_impl(xyu::object_create<Key>(xyu::forward<K,xyu::t_can_nothrow_init<Key,K>>(key)));
            else
            {
                auto [node, find, tag, offset, index] = locate<false, true>(key);
                if (find)
                {
                    node->~Node();
                    --n;

                    xyu::size_t pos = index + offset;
                    // 检查下一个位置是否存在且为 EMPTY
                    if (pos + 1 < ((groups_ + 1) << BlockBits) && data[pos + 1] == EMPTY)
                    {
                        // 将紧随的连续 DELETED 变为 EMPTY
                        xyu::uint32 mask_deleted = simd_eq(data + index, DELETED);
                        xyu::uint32 mask_fix = mask_deleted << (31u - offset) << 1u;
                        xyu::uint32 count = xyu::bit_count_1_high(mask_fix) + 1;
                        xyu::mem_set(data + pos - count + 1, EMPTY, count);

                        // 清理前控制块的墓碑
                        if (XY_UNLIKELY(offset == count - 1))
                        {
                            for (;;)
                            {
                                if (XY_UNLIKELY(index == 0)) index = groups_ << BlockBits;
                                else index -= BlockSize;
                                // 整块探测
                                mask_deleted = simd_eq(data + index, DELETED);
                                count = xyu::bit_count_1_high(mask_deleted);
                                xyu::mem_set(data + index + BlockSize - count, (char)EMPTY, count);
                                if (XY_LIKELY(count != BlockSize)) break;
                            }
                        }
                    }
                    else data[pos] = DELETED;
                    return true;
                }
                return false;
            }
        }

    private:
        static_assert(K_load_factor <= 1 && K_load_factor > 0.1);
        static_assert(K_shrink_factor <= 1 && K_shrink_factor >= 0);

    private:
        // 存储类
        struct Storage
        {
            HashTableBase& ht;      // 哈希表
            xyu::size_t block;      // 控制块索引
            xyu::uint32 mask;         // 控制位掩码

            constexpr Storage(HashTableBase& ht) noexcept : ht{ht}, block(-1), mask{0} {}
        };

        // 有效判断
        struct Valid
        {
            template <typename Generator>
            constexpr bool operator()(Generator& gen) noexcept
            { return gen.block <= gen.ht.groups_; }
        };

        // 解引用
        template <int kind>
        struct Dereference
        {
            template <typename Generator>
            constexpr decltype(auto) operator()(Generator& gen) noexcept
            {
                auto& node = gen.ht.get_node((gen.block << BlockBits) + xyu::bit_count_0_low(gen.mask));
                if constexpr (kind == 0) return node;
                else if constexpr (kind == 1 || kind == 4) return (node.key);
                else if constexpr (kind == 2) return (node.val);
                else if constexpr (kind == 3) return KVNode<const Key&, const Val&>{node.key, node.val};
                else if constexpr (kind == 5) return xyu::make_const(node.val);
                else static_assert(kind == 404, "Invalid kind");
            }
        };

        // 自增
        struct Increment
        {
            template <typename Generator>
            constexpr void operator()(Generator& gen) noexcept
            {
                if (gen.mask) gen.mask &= gen.mask - 1;
                while (gen.mask == 0)
                {
                    if (++gen.block > gen.ht.groups_) return;
                    gen.mask = simd_msb(gen.ht.data + (gen.block << BlockBits));
                }
            }
        };

        template <int kind>
        using Gen = xyu::Generator<Storage, Valid, Dereference<kind>, Increment>;

    protected:
        // 获取范围
        template <int kind>
        auto get_range() noexcept
        {
            if (XY_UNLIKELY(n == 0)) return xyu::Range<Gen<kind>>{0, Gen<kind>{*this}};
            return xyu::Range<Gen<kind>>{n, ++Gen<kind>{*this}};
        }
    };

}

/// 哈希映射
namespace xylu::xycoll
{
    /**
     * @brief 哈希映射
     * @tparam Key  键类型
     * @tparam Val  值类型
     * @tparam cross_thread  是否允许跨线程内存管理。
     *         - false（默认）: 使用线程局部内存池，性能更好；
     *         - true : 使用全局内存分配，对象可跨线程重分配和释放。
     *
     * @details
     *   存储方式：
     *   - 堆分配，控制字节与数据整体连续存储，通过 simd 快速进行开放寻址
     *   - K_load_factor 控制扩容，K_shrink_factor 控制缩容阈值
     *
     *   方法简述：
     *   - 支持 `reserve` 预分配，`reduce` 条件缩容，`release` 释放内存，`clear` 清空元素
     *   - 支持 `select`查找、`append`直接插入、`insert`检查插入、`update`检查更新、`erase`删除，所有操作均为 O(1)
     *   - 支持 通过 `range`、`krange`、`vrange` 进行单向遍历
     *
     *   注意事项：
     *   - 唯一性：Key 禁止重复，使用 `append` 需自行保证
     *   - 容量限制：增加元素超过 `limit` 时会触发 abort
     *   - 范围限制：传入非有限范围在 !XY_RELEASE 下检查并触发 abort
     *   - 容量检查：所有插入或更新操作都会先通过 `reserve` 确保容量充足
     *   - 迭代器失效：任意操作触发扩容后会使所有旧迭代器失效，插入点失效
     *
     *   异常安全：
     *   - 复制赋值、`reserve`（及所有间接调用）、`reduce`：抛出异常时回滚至操作前状态
     *   - 所有范围操作（包括初始化列表）：若元素构造或赋值抛出异常，已成功的元素保留，不会回滚
     */
    template <typename Key, typename Val, bool cross_thread = false>
    struct HashMap : __::HashTableBase<Key, Val, cross_thread>
    {
        static_assert(!xyu::t_is_void<Val>);
        static_assert(xyu::t_can_nothrow_destruct<Key, Val>);
    private:
        using Base = __::HashTableBase<Key, Val, cross_thread>;
        using Node = __::KVNode<Key, Val>;

    public:
        /* 构造赋值 */

        /// 默认构造
        HashMap() noexcept = default;

        /// 预分配空间
        explicit HashMap(xyu::size_t mincapa) noexcept : Base{mincapa} {}

        /// 范围构造
        template <typename kvRg, typename = xyu::t_enable<Node::template can_kv_range<kvRg>>>
        explicit HashMap(kvRg&& rg) noexcept(noexcept(insert(xyu::forward<kvRg>(rg))))
        {
            auto&& range = xyu::make_range(rg);
            HashMap tmp(range.count());
            tmp.insert(xyu::move(range));
            swap(tmp);
        }
        /// 范围构造
        template <typename kRg, typename vRg, typename = xyu::t_enable<Node::template can_kv_2range<kRg, vRg>>>
        explicit HashMap(kRg&& krg, vRg&& vrg) noexcept(noexcept(insert(xyu::forward<kRg>(krg), xyu::t_val<vRg>())))
        {
            auto&& krange = xyu::make_range(krg);
            HashMap tmp(krange.count());
            tmp.insert(xyu::move(krange), xyu::forward<vRg>(vrg));
            swap(tmp);
        }

        /// 复制构造
        HashMap(const HashMap& other) noexcept(xyu::t_can_nothrow_cpconstr<Key, Val>) { operator=(other); }

        /// 复制赋值
        HashMap& operator=(const HashMap& other) noexcept(xyu::t_can_nothrow_cpconstr<Key, Val>)
        {
            if (XY_UNLIKELY(this == &other)) return *this;
            HashMap tmp(other.count());
            tmp.append(other.range());
            swap(tmp);
            return *this;
        }

        /* 移动交换 */

        /// 移动构造
        HashMap(HashMap&& other) noexcept : Base{other.data, other.n, other.capa, other.groups_}
        {
            other.data = nullptr;
            other.n = other.capa = other.groups_ = 0;
        }

        /// 移动赋值
        HashMap& operator=(HashMap&& other) noexcept { return swap(other); }

        /// 交换
        HashMap& swap(HashMap& other) noexcept { Base::swap(other); return *this; }

        /* 数据管理 */

        /// 预分配空间
        void reserve(xyu::size_t mincapa) noexcept(xyu::t_can_nothrow_mvconstr<Key, Val>)
        {
            if (mincapa <= this->capa) return;
            HashMap tmp(mincapa);
            if constexpr (xyu::t_can_nothrow_mvconstr<Key, Val>)
                for (auto& node : range()) tmp.append_impl((Key&&)(node.key), xyu::move(node.val));
            else tmp.append(range());
            swap(tmp);
        }

        /// 缩减容量 (仅当 元素数量 < 容量 * K_shrink_factor)
        void reduce() noexcept(xyu::t_can_nothrow_mvconstr<Key, Val>)
        {
            if (this->n >= this->capa * this->K_shrink_factor) return;
            HashMap tmp(this->n);
            if constexpr (xyu::t_can_nothrow_mvconstr<Key, Val>)
                for (auto& node : range()) tmp.append_impl((Key&&)(node.key), xyu::move(node.val));
            else tmp.append(range());
            swap(tmp);
        }

        /* 获取 */

        /// 查找键值，返回 Val*，不存在返回 nullptr
        template <typename K, typename = xyu::t_enable<xyu::t_is_same_hash<Key,K> || xyu::t_can_init<Key,K>>>
        Val* select(K&& key) noexcept(noexcept(Base::select_impl(xyu::forward<Key>(key))))
        { return Base::select_impl(xyu::forward<Key>(key)); }

        /// 查找键值，返回 const Val*，不存在返回 nullptr
        template <typename K, typename = xyu::t_enable<xyu::t_is_same_hash<Key,K> || xyu::t_can_init<Key,K>>>
        const Val* select(K&& key) const noexcept(noexcept(const_cast<HashMap*>(this)->select_impl(xyu::forward<Key>(key))))
        { return const_cast<HashMap*>(this)->select_impl(xyu::forward<Key>(key)); }

        /* 插入 */

        /// 插入键值，不做检查，自行确保键不存在
        template <typename K, typename... Args, typename = xyu::t_enable<xyu::t_can_init<Key,K> && xyu::t_can_init<Val, Args...>>>
        Val& append(K&& key, Args&&... args) noexcept(Base::template append_impl_noexcept<K, Args...>() && noexcept(reserve(0)))
        {
            reserve(this->n + 1);
            return Base::append_impl(xyu::forward<K>(key), xyu::forward<Args>(args)...);
        }

        /// 插入键值范围，不做检查，自行确保键不存在
        template <typename kvRg, typename = xyu::t_enable<Node::template can_kv_range<kvRg>>>
        HashMap& append(kvRg&& rg) noexcept(noexcept(Base::append_impl(Node::template get_key_ne<kvRg>(), Node::template get_val_ne<kvRg>())) && noexcept(reserve(0)))
        {
            auto&& range = xyu::make_range(rg);
            __::Checker::check_range(range, XY_CurPos("HashMap::append"));
            reserve(this->n + range.count());
            for (auto&& v : range) Base::append_impl(xyu::forward<decltype(v)>(v).key, xyu::forward<decltype(v)>(v).val);
            return *this;
        }

        /// 插入键值范围，不做检查，自行确保键不存在
        template <typename kRg, typename vRg, typename = xyu::t_enable<Node::template can_kv_2range<kRg, vRg>>>
        HashMap& append(kRg&& krg, vRg&& vrg) noexcept(noexcept(Base::append_impl(Node::template get_ne<kRg>(), Node::template get_ne<vRg>())) && noexcept(reserve(0)))
        {
            auto&& krange = xyu::make_range(krg);
            auto&& vrange = xyu::make_range(vrg);
            __::Checker::check_range(krange, vrange, XY_CurPos("HashMap::append"));
            reserve(this->n + krange.count());
            auto ki = krange.begin();
            auto ko = krange.end();
            auto vi = vrange.begin();
            while (ki != ko) Base::append_impl(*ki, *vi), ++ki, ++vi;
            return *this;
        }

        /// 插入键值范围，不做检查，自行确保键不存在
        HashMap& append(std::initializer_list<Node> il) noexcept(noexcept(append(xyu::make_range(il)))) { return append(xyu::make_range(il)); }


        /// 插入键值，若键不存在，就地构造并返回；若已存在，返回原值
        template <typename K, typename... Args, typename = xyu::t_enable<xyu::t_can_init<Key,K> && xyu::t_can_init<Val, Args...>>>
        Val& insert(K&& key, Args&&... args) noexcept(Base::template append_impl_noexcept<K, Args...>() && noexcept(reserve(0)))
        {
            reserve(this->n + 1);
            return Base::insert_impl(xyu::forward<K>(key), xyu::forward<Args>(args)...);
        }

        /// 插入键值范围，若键不存在，就地构造；若已存在，不做处理
        template <typename kvRg, typename = xyu::t_enable<Node::template can_kv_range<kvRg>>>
        HashMap& insert(kvRg&& rg) noexcept(noexcept(Base::append_impl(Node::template get_key_ne<kvRg>(), Node::template get_val_ne<kvRg>())) && noexcept(reserve(0)))
        {
            auto&& range = xyu::make_range(rg);
            __::Checker::check_range(range, XY_CurPos("HashMap::insert"));
            reserve(this->n + range.count());
            for (auto&& v : range) Base::insert_impl(xyu::forward<decltype(v)>(v).key, xyu::forward<decltype(v)>(v).val);
            return *this;
        }

        /// 插入键值范围，若键不存在，就地构造；若已存在，不做处理
        template <typename kRg, typename vRg, typename = xyu::t_enable<Node::template can_kv_2range<kRg, vRg>>>
        HashMap& insert(kRg&& krg, vRg&& vrg) noexcept(noexcept(Base::append_impl(Node::template get_ne<kRg>(), Node::template get_ne<vRg>())) && noexcept(reserve(0)))
        {
            auto&& krange = xyu::make_range(krg);
            auto&& vrange = xyu::make_range(vrg);
            __::Checker::check_range(krange, vrange, XY_CurPos("HashMap::insert"));
            reserve(this->n + krange.count());
            auto ki = krange.begin();
            auto ko = krange.end();
            auto vi = vrange.begin();
            while (ki != ko) Base::insert_impl(*ki, *vi), ++ki, ++vi;
            return *this;
        }

        /// 插入键值范围，若键不存在，就地构造；若已存在，不做处理
        HashMap& insert(std::initializer_list<Node> il) noexcept(noexcept(insert(xyu::make_range(il)))) { return insert(xyu::make_range(il)); }

        /* 修改 */

        /// 修改键值，若键不存在，就地构造并返回；若已存在，修改后返回
        template <typename K, typename... Args, typename = xyu::t_enable<xyu::t_can_init<Key,K> && (sizeof...(Args) == 1 && xyu::t_can_assign<Val,Args...> || xyu::t_can_init<Val,Args...> && xyu::t_can_mvassign<Val>)>>
        Val& update(K&& key, Args&&... args) noexcept(Base::template update_impl_noexcept<K, Args...>() && noexcept(reserve(0)))
        {
            reserve(this->n + 1);
            return Base::update_impl(xyu::forward<K>(key), xyu::forward<Args>(args)...);
        }

        /// 修改键值范围，若键不存在，就地构造；若已存在则修改
        template <typename kvRg, typename = xyu::t_enable<Node::template can_kv_range<kvRg> && xyu::t_can_assign<Val,decltype(xyu::t_val<xyu::t_access<kvRg>>().val)>>>
        HashMap& update(kvRg&& rg) noexcept(noexcept(Base::update_impl(Node::template get_key_ne<kvRg>(), Node::template get_val_ne<kvRg>())) && noexcept(reserve(0)))
        {
            auto&& range = xyu::make_range(rg);
            __::Checker::check_range(range, XY_CurPos("HashMap::update"));
            reserve(this->n + range.count());
            for (auto&& v : range) Base::update_impl(xyu::forward<decltype(v)>(v).key, xyu::forward<decltype(v)>(v).val);
            return *this;
        }

        /// 修改键值范围，若键不存在，就地构造；若已存在则修改
        template <typename kRg, typename vRg, typename = xyu::t_enable<Node::template can_kv_2range<kRg, vRg> && xyu::t_can_assign<Val,xyu::t_access<vRg>>>>
        HashMap& update(kRg&& krg, vRg&& vrg) noexcept(noexcept(Base::update_impl(Node::template get_ne<kRg>(), Node::template get_ne<vRg>())) && noexcept(reserve(0)))
        {
            auto&& krange = xyu::make_range(krg);
            auto&& vrange = xyu::make_range(vrg);
            __::Checker::check_range(krange, vrange, XY_CurPos("HashMap::update"));
            reserve(this->n + krange.count());
            auto ki = krange.begin();
            auto ko = krange.end();
            auto vi = vrange.begin();
            while (ki != ko) Base::update_impl(*ki, *vi), ++ki, ++vi;
            return *this;
        }

        /// 修改键值范围，若键不存在，就地构造；若已存在则修改
        HashMap& update(std::initializer_list<Node> il) noexcept(noexcept(update(xyu::make_range(il)))) { return update(xyu::make_range(il)); }

        /* 删除 */

        /// 删除键，返回是否删除成功（即键是否存在）
        template <typename K, typename = xyu::t_enable<xyu::t_can_init<Key,K>>>
        bool erase(K&& key) noexcept(noexcept(Base::erase_impl(xyu::forward<K>(key))))
        { return Base::erase_impl(xyu::forward<K>(key)); }

        /// 删除键范围
        template <typename kRg, typename = xyu::t_enable<!xyu::t_can_init<Key,kRg>>>
        HashMap& erase(kRg&& krg) noexcept(noexcept(Base::erase_impl(xyu::t_val<xyu::t_noexcept_access<kRg&>>())))
        {
            auto&& krange = xyu::make_range(krg);
            for (auto&& v : krange) Base::erase_impl(xyu::forward<decltype(v)>(v));
            return *this;
        }

        /// 删除键范围
        HashMap& erase(std::initializer_list<Key> il) noexcept(noexcept(erase(xyu::make_range(il)))) { return erase(xyu::make_range(il)); }

        /* 范围 */

        /**
         * @brief 迭代器类型
         * @details
         *   KeyVal: 解引用后得到键值，*it == {key, val}；
         *   OnlyKey: 解引用后得到键，*it == key；
         *   OnlyVal: 解引用后得到值，*it == val；
         */
        enum _IterType : int { KeyVal = 0, OnlyKey = 1, OnlyVal = 2 };

        /// 获取范围
        template <_IterType type = KeyVal>
        auto range() noexcept { return this->template get_range<type>(); }
        /// 获取常量范围
        template <_IterType type = KeyVal>
        auto range() const noexcept { return const_cast<HashMap*>(this)->template get_range<_IterType{type + 3}>(); }

        /* 运算符 */

        /// 值访问，不存在时默认构造返回
        template <typename K, typename = xyu::t_enable<xyu::t_can_init<Key,K> && xyu::t_can_init<Val>>>
        Val& operator[](K&& key) noexcept { return insert(xyu::forward<K>(key)); }

        /// 插入键值
        template <typename Arg>
        HashMap& operator<<(Arg&& arg) noexcept(noexcept(append(xyu::forward<Arg>(arg))))
        { update(xyu::forward<Arg>(arg)); return *this; }
    };

    // 推导指引
    template <typename Rg, typename = xyu::t_enable<xyu::t_can_range<Rg>>>
    HashMap(Rg&&) -> HashMap<xyu::t_rem_cr<decltype(xyu::t_val<xyu::t_access<Rg>>().key)>, xyu::t_rem_cr<decltype(xyu::t_val<xyu::t_access<Rg>>().val)>>;
    template <typename kRg, typename vRg, typename = xyu::t_enable<xyu::t_can_range<kRg, vRg>>>
    HashMap(kRg&&, vRg&&) -> HashMap<xyu::t_rem_cr<xyu::t_access<kRg>>, xyu::t_rem_cr<xyu::t_access<vRg>>>;
}

/// 哈希集
namespace xylu::xycoll
{
    /**
     * @brief 哈希集
     * @tparam Key  键类型
     * @tparam cross_thread  是否允许跨线程内存管理。
     *         - false（默认）: 使用线程局部内存池，性能更好；
     *         - true : 使用全局内存分配，对象可跨线程重分配和释放。
     *
     * @details
     *   存储方式：
     *   - 堆分配，控制字节与数据整体连续存储，通过 simd 快速进行开放寻址
     *   - K_load_factor 控制扩容，K_shrink_factor 控制缩容阈值
     *
     *   方法简述：
     *   - 支持 `reserve` 预分配，`reduce` 条件缩容，`release` 释放内存，`clear` 清空元素
     *   - 支持 `select`查找、`append`直接插入、`insert`检查插入、`erase`删除，所有操作均为 O(1)
     *   - 支持 通过 `range` 进行单向遍历
     *
     *   注意事项：
     *   - 唯一性：Key 禁止重复，使用 `append` 需自行保证
     *   - 容量限制：增加元素超过 `limit` 时会触发 abort
     *   - 范围限制：传入非有限范围在 !XY_RELEASE 下检查并触发 abort
     *   - 容量检查：除 `select`仅查找键 外，其余所有操作都会先通过 `reserve` 确保容量充足
     *   - 迭代器失效：任意操作触发扩容后会使所有旧迭代器失效，插入点失效
     *
    *   异常安全：
     *   - 复制赋值、`reserve`（及所有间接调用）、`reduce`：抛出异常时回滚至操作前状态
     *   - 所有范围操作（包括初始化列表）：若元素构造或赋值抛出异常，已成功的元素保留，不会回滚
     */
    template <typename Key, bool cross_thread = false>
    struct HashSet : __::HashTableBase<Key, void, cross_thread>
    {
    private:
        using Base = __::HashTableBase<Key, void, cross_thread>;
        using Node = __::KVNode<Key, void>;

    public:
        /* 构造赋值 */

        /// 默认构造
        HashSet() noexcept = default;

        /// 预分配空间
        explicit HashSet(xyu::size_t mincapa) noexcept : Base{mincapa} {}

        /// 范围构造
        template <typename Rg, typename = xyu::t_enable<!xyu::t_can_init<Key,Rg> && xyu::t_can_init<Key,xyu::t_access<Rg&>>>>
        explicit HashSet(Rg&& rg) noexcept(noexcept(insert(xyu::forward<Rg>(rg))))
        {
            auto&& range = xyu::make_range(rg);
            HashSet tmp(range.count());
            tmp.insert(xyu::move(range));
            swap(tmp);
        }

        /// 复制构造
        HashSet(const HashSet& other) noexcept(xyu::t_can_nothrow_cpconstr<Key>) { operator=(other); }

        /// 复制赋值
        HashSet& operator=(const HashSet& other) noexcept(xyu::t_can_nothrow_cpconstr<Key>)
        {
            if (XY_UNLIKELY(this == &other)) return *this;
            HashSet tmp(other.count());
            tmp.append(other.range());
            swap(tmp);
            return *this;
        }

        /* 移动交换 */

        /// 移动构造
        HashSet(HashSet&& other) noexcept : Base{other.data, other.n, other.capa, other.groups_}
        {
            other.data = nullptr;
            other.n = other.capa = other.groups_ = 0;
        }

        /// 移动赋值
        HashSet& operator=(HashSet&& other) noexcept { return swap(other); }

        /// 交换
        HashSet& swap(HashSet& other) noexcept { Base::swap(other); return *this; }

        /* 数据管理 */

        /// 预分配空间
        void reserve(xyu::size_t mincapa) noexcept(xyu::t_can_nothrow_mvconstr<Key>)
        {
            if (mincapa <= this->capa) return;
            HashSet tmp(mincapa);
            if constexpr (xyu::t_can_nothrow_mvconstr<Key>)
                for (auto& key : range()) tmp.append_impl((Key&&)key);
            else tmp.append(range());
            swap(tmp);
        }

        /// 缩减容量 (仅当 元素数量 < 容量 * K_shrink_factor)
        void reduce() noexcept(xyu::t_can_nothrow_mvconstr<Key>)
        {
            if (this->n >= this->capa * this->K_shrink_factor) return;
            HashSet tmp(this->n);
            if constexpr (xyu::t_can_nothrow_mvconstr<Key>)
                for (auto& key : range()) tmp.append_impl((Key&&)key);
            else tmp.append(range());
            swap(tmp);
        }

        /* 获取 */

        /// 查找键是否存在
        template <typename K, typename = xyu::t_enable<xyu::t_is_same_hash<Key,K> || xyu::t_can_init<Key,K>>>
        bool select(K&& key) const noexcept(noexcept(const_cast<HashSet*>(this)->select_impl(xyu::forward<Key>(key))))
        { return const_cast<HashSet*>(this)->select_impl(xyu::forward<Key>(key)); }

        /* 插入 */

        /// 插入键，不做检查，自行确保键不存在
        template <typename... Args, typename = xyu::t_enable<xyu::t_can_init<Key, Args...>>, bool direct = (sizeof...(Args) == 1), typename Arg = xyu::t_cond<direct, xyu::t_tps_get<xyu::typelist_c<Args..., void>, 0>, Key&&>>
        HashSet& append(Args&&... args) noexcept(Base::template append_impl_noexcept<Arg>() && (direct || xyu::t_can_nothrow_init<Key,Args...>) && noexcept(reserve(0)))
        {
            reserve(this->n + 1);
            if constexpr (direct) Base::append_impl(xyu::forward<Args>(args)...);
            else Base::append_impl(xyu::object_create<Key>(xyu::forward<Args>(args)...));
            return *this;
        }

        /// 插入键值范围，不做检查，自行确保键不存在
        template <typename Rg, typename = xyu::t_enable<!xyu::t_can_init<Key,Rg> && xyu::t_can_init<Key,xyu::t_access<Rg&>>>>
        HashSet& append(Rg&& rg) noexcept(noexcept(Base::append_impl(xyu::t_val<xyu::t_noexcept_access<Rg&>>())) && noexcept(reserve(0)))
        {
            auto&& range = xyu::make_range(rg);
            __::Checker::check_range(range, XY_CurPos("HashSet::append"));
            reserve(this->n + range.count());
            for (auto&& v : range) Base::append_impl(xyu::forward<decltype(v)>(v));
            return *this;
        }

        /// 插入键值范围，不做检查，自行确保键不存在
        HashSet& append(std::initializer_list<Key> il) noexcept(noexcept(append(xyu::make_range(il)))) { return append(xyu::make_range(il)); }


        /// 插入键值，若键不存在，就地构造并返回；若已存在，返回原值
        template <typename... Args, typename = xyu::t_enable<xyu::t_can_init<Key, Args...>>, bool direct = (sizeof...(Args) == 1), typename Arg = xyu::t_cond<direct, xyu::t_tps_get<xyu::typelist_c<Args..., void>, 0>, Key&&>>
        HashSet& insert(Args&&... args) noexcept(Base::template append_impl_noexcept<Arg>() && (direct || xyu::t_can_nothrow_init<Key,Args...>) && noexcept(reserve(0)))
        {
            reserve(this->n + 1);
            if constexpr (direct) Base::insert_impl(xyu::forward<Args>(args)...);
            else Base::insert_impl(xyu::object_create<Key>(xyu::forward<Args>(args)...));
            return *this;
        }

        /// 插入键值范围，若键不存在，就地构造；若已存在，不做处理
        template <typename Rg, typename = xyu::t_enable<!xyu::t_can_init<Key,Rg> && xyu::t_can_init<Key,xyu::t_access<Rg&>>>>
        HashSet& insert(Rg&& rg) noexcept(noexcept(Base::insert_impl(xyu::t_val<xyu::t_noexcept_access<Rg&>>())) && noexcept(reserve(0)))
        {
            auto&& range = xyu::make_range(rg);
            __::Checker::check_range(range, XY_CurPos("HashSet::insert"));
            reserve(this->n + range.count());
            for (auto&& v : range) Base::insert_impl(xyu::forward<decltype(v)>(v));
            return *this;
        }

        /// 插入键值范围，若键不存在，就地构造；若已存在，不做处理
        HashSet& insert(std::initializer_list<Key> il) noexcept(noexcept(insert(xyu::make_range(il)))) { return insert(xyu::make_range(il)); }

        /* 删除 */

        /// 删除键，返回是否删除成功（即键是否存在）
        template <typename K, typename = xyu::t_enable<xyu::t_can_init<Key,K>>>
        bool erase(K&& key) noexcept(noexcept(Base::erase_impl(xyu::forward<K>(key))))
        { return Base::erase_impl(xyu::forward<K>(key)); }

        /// 删除键范围
        template <typename Rg, typename = xyu::t_enable<!xyu::t_can_init<Key,Rg>>>
        HashSet& erase(Rg&& krg) noexcept(noexcept(Base::erase_impl(xyu::t_val<xyu::t_noexcept_access<Rg&>>())))
        {
            auto&& krange = xyu::make_range(krg);
            for (auto&& v : krange) Base::erase_impl(xyu::forward<decltype(v)>(v));
            return *this;
        }

        /// 删除键范围
        HashSet& erase(std::initializer_list<Key> il) noexcept(noexcept(erase(xyu::make_range(il)))) { return erase(xyu::make_range(il)); }

        /* 范围 */

        /// 获取范围
        auto range() const noexcept { return const_cast<HashSet*>(this)->template get_range<1>(); }
    };

    // 推导指引
    template <typename Rg, typename = xyu::t_enable<xyu::t_can_range<Rg>>>
    HashSet(Rg&&) -> HashSet<xyu::t_rem_cr<xyu::t_access<Rg>>>;
}
