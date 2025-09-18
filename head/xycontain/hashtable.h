#pragma clang diagnostic push
#pragma ide diagnostic ignored "UnusedParameter"
#pragma ide diagnostic ignored "OCUnusedStructInspection"
#pragma ide diagnostic ignored "misc-unconventional-assign-operator"
#pragma ide diagnostic ignored "OCUnusedTemplateParameterInspection"
#pragma ide diagnostic ignored "OCUnusedGlobalDeclarationInspection"
#pragma ide diagnostic ignored "hicpp-exception-baseclass"
#pragma ide diagnostic ignored "modernize-use-nodiscard"
#pragma once

#include "./impl/kv.h"
#include "../../link/log"

// 辅助工具
namespace xylu::xycontain::__
{
    /// 控制组
    struct alignas(16) ControlGroup
    {
        xyu::uint8 metas[16];   // 控制组元数据
    };

    /// 获取控制组中每个元素据的最高有效位 - sse2
    xyu::uint16 msb(ControlGroup& ctrl) noexcept;

    /// 获取控制组每个元素与val的相等比较结果 - sse2
    xyu::uint16 cmpeq(ControlGroup& ctrl, xyu::uint8 val) noexcept;
}

/// 生成器
namespace xylu::xyrange
{
    // 生成器 - 存储类 - 哈希表
    template <typename HT>
    struct RangeGenerator_Storage_HashTable
    {
        HT& ht;             // 哈希表
        xyu::size_t index;  // 索引
        xyu::uint16 mask;   // 掩码 (控制位掩码)

        // 构造函数 (初始化 index 为 -16，构造时直接复用 increment 即可)
        constexpr explicit RangeGenerator_Storage_HashTable(HT& ht) noexcept : ht{ht}, index(-16), mask{0} {}
    };

    // 生成器 - 有效判断 - 哈希表
    struct RangeGenerator_Valid_HashTable
    {
        template <typename Generator>
        constexpr bool operator()(Generator& gen) noexcept
        { return gen.index < gen.ht.total; }
    };

    // 生成器 - 解引用 - 哈希表
    template <int kind>
    struct RangeGenerator_Dereference_HashTable
    {
        template <typename Generator>
        constexpr decltype(auto) operator()(Generator& gen) noexcept
        {
            auto& kv = gen.ht.get_kv(gen.index + xyu::bit_count_0_back(gen.mask));
            if constexpr (kind == 0) return kv;
            else if constexpr (kind == 1) return kv.key;
            else if constexpr (kind == 2) return kv.val;
            else static_assert(kind == 404, "Invalid kind");
        }
    };

    // 生成器 - 自增 - 哈希表
    struct RangeGenerator_Increment_HashTable
    {
        template <typename Generator>
        constexpr void operator()(Generator& gen) noexcept
        {
            using namespace xylu::xycontain::__;
            if (gen.mask) gen.mask &= gen.mask - 1;
            while (gen.mask == 0) {
                if ((gen.index += 16) >= gen.ht.total) return;
                gen.mask = ~msb(static_cast<ControlGroup*>(gen.ht.data)[gen.index / 16]);
            }
        }
    };

    /**
     * @brief 创建一个遍历哈希表的生成器。
     * @tparam HT 哈希表类型。
     * @tparam kind 哈希表元素获取器。
     * 0: 键值对；1: 键；2: 值
     */
    template <typename Ht, int kind>
    using RangeGenerator_HashTable = RangeGenerator<RangeGenerator_Storage_HashTable<Ht>,
            RangeGenerator_Valid_HashTable, RangeGenerator_Dereference_HashTable<kind>, RangeGenerator_Increment_HashTable>;
}

/// 哈希表
namespace xylu::xycontain
{
    /**
     * @brief 一个实现了 Google's Swiss Table 核心思想的高性能哈希表容器。
     *
     * @tparam Key 键的类型。
     * @tparam Value 值的类型。如果为 `void`，则容器表现为哈希集合 (HashSet)。
     *
     * @details
     * `xyu::HashTable` 采用开放寻址法，并通过SIMD指令集（SSE2）优化来加速元素
     * 的查找、插入和删除操作，旨在提供业界顶级的性能表现。
     *
     * ### 核心亮点 (Key Features):
     *
     * 1.  **SIMD优化的控制组 (SIMD-Optimized Control Group):**
     *     - 借鉴了 Abseil Swiss Table 的设计，将哈希值的元数据（H2哈希）存储在
     *       独立的“控制字节”数组中。
     *     - 通过单条SIMD指令，可以并行地探测16个槽位的状态（空、已删除、或匹配的H2哈希），
     *       极大地减少了探测链的长度，显著提升了缓存局部性和查找性能。
     *
     * 2.  **与范围/生成器生态的深度融合 (Deep Range/Generator Integration):**
     *     - 迭代是通过一个专门设计的、惰性的**哈希表生成器** (`RangeGenerator_HashTable`)
     *       来实现的，它能高效地跳过空槽位，只遍历有效元素。
     *     - 提供了 `range()` (键值对), `krange()` (键), `vrange()` (值) 三种视图，
     *       返回 `xyu::Range` 对象，与 `xylu` 的所有范围工具无缝协作。
     *
     * 3.  **高度泛化与智能的插入接口 (Highly Generic & Smart Insertion):**
     *     - `insert` 和 `update` 方法通过强大的模板元编程，能够在编译期自动识别
     *       各种输入范围（例如，`std::vector<std::pair<K, V>>`, `List<Tuple<K,V>>`等）
     *       的元素类型，并以最高效的方式从中提取键值对，提供了无与伦比的灵活性。
     *
     * 4.  **与 `xylu` 库生态的完全集成:**
     *     - **内存管理:** 所有节点的内存都由 `xylu` 的高性能内存池管理。
     *     - **格式化:** 提供了强大的 `Formatter` 特化，支持复杂的嵌套哈希表的递归格式化，
     *       并能通过格式字符串精确控制输出。
     *     - **异常与日志:** 所有的错误情况（如键不存在）都会通过 `xylu` 的日志和异常系统
     *       进行详细的报告。
     */
    template <typename Key, typename Value = void>
    class HashTable
    {
        using Data = __::KVData<Key, Value>;
        static_assert(xyu::t_can_nothrow_destruct<Data>);
    public:
        /* 静态常量 */
        /// 负载因子
        static constexpr double K_load_factor = 0.875;
        /// 释放空间条件比例
        static constexpr double K_shrink_factor = 0.5;

    private:
        /// 标记常量
        enum Tag : xyu::uint8
        {
            EMPTY = 0x80,       // 空标记   0b 1000 0000 (此位置 可以插入 并且 可以结束线性探测)
            DELETED = 0xff,     // 删除标记 0b 1111 1111 (此位置 可以插入 但 不可以结束线性探测)
            USED = 0x00,        // 占用位   0b 0hhh hhhh
        };

    private:
        void* data;         // 数据
        xyu::size_t n;      // 元素数量
        xyu::size_t capa;   // 容量 (=总容量*负载因子，超过后扩容)
        xyu::size_t total;  // 总容量

    public:
        /* 构造析构 */

        /// 默认构造
        HashTable() noexcept : data{nullptr}, n{0}, capa{0}, total{0} {}
        /// 复制构造
        HashTable(const HashTable& other) : HashTable{other.n} { insert(other.range()); }
        /// 移动构造
        HashTable(HashTable&& other) noexcept : data{other.data}, n{other.n}, capa{other.capa}, total{other.total}
        {
            other.data = nullptr;
            other.n = other.capa = other.total = 0;
        }

        /// 预分配空间
        explicit HashTable(xyu::size_t mincapa) : HashTable{}
        {
            xyu::size_t newtotal = calc_new_total(mincapa);
            // 计算字段大小
            xyu::size_t ctrl_size = newtotal;
            if constexpr (alignof(Data) > 16) ctrl_size = (ctrl_size + alignof(Data) - 1) & -alignof(Data);
            xyu::size_t data_size = ctrl_size * sizeof(Data);
            // 分配内存
            data = xyu::alloc(ctrl_size + data_size, xyu::max(alignof(Data), 16));
            xyu::mem_set(data, newtotal, EMPTY);
            total = newtotal;
            capa = static_cast<xyu::size_t>(static_cast<double>(total) * K_load_factor);
        }

        /// 初始化列表构造
        template <typename Test = Value, xyu::t_enable<!xyu::t_is_void<Test>, bool> = true>
        HashTable(std::initializer_list<Tuple<Key, Value>> il) : HashTable(il.size()) { insert(il); }
        /// 初始化列表构造
        template <typename Test = Value, xyu::t_enable<xyu::t_is_void<Test>, bool> = false>
        HashTable(std::initializer_list<Tuple<Key>> il) : HashTable(il.size()) { insert(il); }

        /// 析构
        ~HashTable() noexcept { release(); }

        /* 赋值交换 */

        /// 复制赋值
        HashTable& operator=(const HashTable& other)
        {
            if (XY_UNLIKELY(this == &other)) return *this;
            // 复用空间
            if (other.n >= capa * K_shrink_factor && other.n <= capa) {
                clear();
                insert(other.range());
            }
            // 重新分配
            else {
                HashTable tmp(other.n);
                tmp.insert(other.range());
                swap(tmp);
            }
            return *this;
        }
        /// 移动赋值
        HashTable& operator=(HashTable&& other) noexcept { return swap(other); }

        /// 交换
        HashTable& swap(HashTable&& other) noexcept { return swap(other); }
        /// 交换
        HashTable& swap(HashTable& other) noexcept
        {
            xyu::swap(data, other.data);
            xyu::swap(n, other.n);
            xyu::swap(capa, other.capa);
            xyu::swap(total, other.total);
            return *this;
        }

        /* 数据容量 */

        /// 获取最大容量
        constexpr static xyu::size_t limit() noexcept
        {
            return static_cast<double>(xyu::number_traits<xyu::size_t>::max) * K_load_factor;
        }
        /// 获取当前容量
        xyu::size_t capacity() const noexcept { return capa; }

        /// 获取元素数量
        xyu::size_t count() const noexcept { return n; }
        /// 是否为空
        bool empty() const noexcept { return n == 0; }

        /* 数据管理 */

        /// 预分配空间
        void reserve(xyu::size_t mincapa)
        {
           if (mincapa <= capa) return;
           HashTable tmp(mincapa);
           tmp.insert(range().mrange());
           swap(tmp);
        }
        /// 缩减容量 (仅当 元素数量 < 容量 * K_shrink_factor)
        void reduce()
        {
            if (n >= capa * K_shrink_factor) return;
            HashTable tmp(n);
            tmp.insert(range().mrange());
            swap(tmp);
        }

        /// 清空元素
        void clear() noexcept
        {
            if (XY_UNLIKELY(n == 0)) return;
            for (xyu::size_t ci = 0; ci < total; ci += 16)
            {
                __::ControlGroup& ctrl = get_ctrl(ci / 16);
                xyu::uint16 mask = ~__::msb(ctrl);
                if (mask)
                {
                    do {
                        xyu::uint offset = xyu::bit_count_0_back(mask);
                        Data& kv = get_kv(ci + offset);
                        kv.~Data();
                        mask &= mask - 1;
                    } while (mask);
                    xyu::mem_set(&ctrl, 16, EMPTY);
                }
            }
            n = 0;
        }
        /// 释放内存
        void release() noexcept
        {
            if (XY_UNLIKELY(total == 0)) return;
            clear();
            // 计算字段大小
            xyu::size_t ctrl_size = total;
            if constexpr (alignof(Data) > 16) ctrl_size = (ctrl_size + alignof(Data) - 1) & -alignof(Data);
            xyu::size_t data_size = ctrl_size * sizeof(Data);
            // 释放内存
            xyu::dealloc(data, ctrl_size + data_size, xyu::max(alignof(Data), 16));
            data = nullptr;
            capa = total = 0;
        }
        
        /* 查找 */

        /// 查找键是否存在
        bool contains(const Key& key) const noexcept
        {
            xyu::size_t hash = xyu::make_hash(key);
            xyu::size_t start = (hash >> 7) & (total - 1);
            xyu::size_t ci = start;
            do {
                // 查找是否已存在
                xyu::uint16 mask = __::cmpeq(get_ctrl(ci), hash & 0x7f);
                while (mask) {
                    xyu::uint offset = xyu::bit_count_0_back(mask);
                    Data& kv = get_kv(ci * 16 + offset);
                    if (kv.key == key) return true;
                    mask &= mask - 1;
                }
                // 查找是否结束
                if (__::cmpeq(get_ctrl(ci), EMPTY)) break;
                ci = (ci + 1) & (total - 1);
            } while (ci != start);
            // 查找失败
            return false;
        }

        /* 获取 */

        /**
         * @brief 获取值
         * @details 若键不存在，则通过默认构造插入元素
         * @param key 键
         * @note 仅 Value 不为 void 时可用
         */
        template <typename Test = Value, typename = xyu::t_enable<!xyu::t_is_void<Test>>>
        decltype(auto) get(const Key& key)
        {
            reserve(n + 1); // 保证有空间插入
            xyu::size_t hash = xyu::make_hash(key);
            xyu::size_t start = (hash >> 7) & (total - 1);
            xyu::size_t ii = -1;
            xyu::size_t ci = start;
            do {
                // 查找是否已存在
                xyu::uint16 mask = __::cmpeq(get_ctrl(ci), hash & 0x7f);
                while (mask) {
                    xyu::uint offset = xyu::bit_count_0_back(mask);
                    Data& kv = get_kv(ci * 16 + offset);
                    if (xyu::equals(kv.key, key)) return kv.val;
                    mask &= mask - 1;
                }
                // 若没有插入指针
                if (ii == -1) {
                    mask = __::msb(get_ctrl(ci));
                    if (mask) ii = ci * 16 + xyu::bit_count_0_back(mask);
                    else if (__::cmpeq(get_ctrl(ci), EMPTY)) break;
                }
                    // 判断是否结束
                else if (__::cmpeq(get_ctrl(ci), EMPTY)) break;
                ci = (ci + 1) & (total - 1);
            } while (ci != start);
            // 插入元素
            Data& kv = get_kv(ii);
            ::new (&kv) Data{key};
            get_ctrl(ii >> 4).metas[ii & (16 - 1)] = hash & 0x7f;
            ++n;
            return kv.val;
        }

        /**
         * @brief 获取值
         * @details 若键不存在，则通过默认构造插入元素
         * @param key 键
         * @note 仅 Value 不为 void 时可用
         */
        template <typename Test = Value, typename = xyu::t_enable<!xyu::t_is_void<Test>>>
        decltype(auto) get(const Key& key) const { return xyu::make_const(const_cast<HashTable*>(this)->get(key)); }

        /**
         * @brief 获取值
         * @details 若键不存在，则抛出异常
         * @param key 键
         * @note 仅 Value 不为 void 时可用
         */
        template <typename Test = Value, typename = xyu::t_enable<!xyu::t_is_void<Test>>>
        decltype(auto) at(const Key& key)
        {
            xyu::size_t hash = xyu::make_hash(key);
            xyu::size_t start = (hash >> 7) & (total - 1);
            xyu::size_t ci = start;
            do {
                // 查找是否已存在
                xyu::uint16 mask = __::cmpeq(get_ctrl(ci), hash & 0x7f);
                while (mask) {
                    xyu::uint offset = xyu::bit_count_0_back(mask);
                    Data& kv = get_kv(ci * 16 + offset);
                    if (kv.key == key) return kv.val;
                    mask &= mask - 1;
                }
                // 查找是否结束
                if (__::cmpeq(get_ctrl(ci), EMPTY)) break;
                ci = (ci + 1) & (total - 1);
            } while (ci != start);
            // 查找失败
            xyloge(false, "E_Logic_Key_Not_Found: key {} is not found in the table", key);
            throw xyu::E_Logic_Key_Not_Found{};
        }

        /**
         * @brief 获取值
         * @details 若键不存在，则抛出异常
         * @param key 键
         * @note 仅 Value 不为 void 时可用
         */
        template <typename Test = Value, typename = xyu::t_enable<!xyu::t_is_void<Test>>>
        decltype(auto) at(const Key& key) const { return xyu::make_const(const_cast<HashTable*>(this)->at(key)); }

        /* 插入 */

        /**
         * @brief 插入元素，并返回节点
         * @details 若键已存在，则不做处理，直接返回原值引用
         * @param key 键
         * @param args 值 (或用于构造 Value 的参数)
         * @note 当 Value 为 void 时，args 必须为空
         */
        template <typename... Args>
        Data& insert(const Key& key, Args&&... args)
        {
            static_assert(!(xyu::t_is_void<Value> && sizeof...(Args) > 0));
            return increment_help(1, [&](HashTable& ht){ return ht.insert_impl(key, xyu::forward<Args>(args)...); });
        }

        /**
         * @brief 依次插入范围中的每个键
         * @details 若键已存在，则不做处理
         * @param range 键范围
         */
        template <typename Rg, xyu::t_enable<!xyu::t_can_icast<Rg, Key> && xyu::t_is_range<Rg> && (__::get_range_kv_type<Rg, Key> < 0), char> = 's'>
        HashTable& insert(Rg&& range)
        {
            increment_help(n + range.count(), [&](HashTable& ht)
            {
                constexpr int kind = __::get_range_kv_type<Rg, Key>;
                for (auto&& e : range)
                {
                    if constexpr (kind == -1) ht.insert_impl(xyu::forward<decltype(e)>(e));
                    else if constexpr (kind == -2) ht.insert_impl(xyu::forward<decltype(e)>(e).key);
                    else if constexpr (kind == -3) ht.insert_impl(xyu::forward<decltype(e)>(e).template get<0>());
                    else { auto&& [key] = xyu::forward<decltype(e)>(e); ht.insert_impl(xyu::forward<decltype(key)>(key)); }
                }
            });
            return *this;
        }

        /**
         * @brief 依次插入范围中的每个键值对
         * @details 若键已存在，则不做处理
         * @param range 键值对范围
         * @note 仅 Value 不为 void 时可用
         */
        template <typename Rg, typename Test = Value, xyu::t_enable<!xyu::t_is_void<Test> && !xyu::t_can_icast<Rg, Key> && xyu::t_is_range<Rg> && (__::get_range_kv_type<Rg> > 0), char> = 'd'>
        HashTable& insert(Rg&& range)
        {
            static_assert(!xyu::t_is_void<Value>);
            increment_help(n + range.count(), [&](HashTable& ht)
            {
                constexpr int kind = __::get_range_kv_type<Rg>;
                for (auto&& e : range)
                {
                    if constexpr (kind == 2) ht.insert_impl(xyu::forward<decltype(e)>(e).key, xyu::forward<decltype(e)>(e).val);
                    else if constexpr (kind == 3) ht.insert_impl(xyu::forward<decltype(e)>(e).template get<0>(), xyu::forward<decltype(e)>(e).template get<1>());
                    else { auto&& [key, val] = xyu::forward<decltype(e)>(e); ht.insert_impl(xyu::forward<decltype(key)>(key), xyu::forward<decltype(val)>(val)); }
                }
            });
            return *this;
        }

        /**
         * @brief 将参数构造为范围后，依次插入范围中的每个元素
         * @details 若键已存在，则不做处理
         * @param arg 用于构造范围的参数
         */
        template <typename Arg, xyu::t_enable<!xyu::t_can_icast<Arg, Key> && !xyu::t_is_range<Arg>, char> = 'x'>
        HashTable& insert(Arg&& arg) { return insert(xyu::make_range(arg)); }

        /**
         * @brief 将键值对通过初始化列表进行插入
         * @details 若键已存在，则不做处理
         * @param il 键值对列表
         * @note 仅 Value 不为 void 时可用
         */
        template <typename Test = Value, xyu::t_enable<!xyu::t_is_void<Test>, bool> = true>
        HashTable& insert(std::initializer_list<Tuple<Key, Value>> il) { return insert(xyu::make_range(il)); }

        /**
         * @brief 将键通过初始化列表进行插入
         * @details 若键已存在，则不做处理
         * @param il 键列表
         * @note 仅 Value 为 void 时可用
         */
        template <typename Test = Value, xyu::t_enable<xyu::t_is_void<Test>, bool> = false>
        HashTable& insert(std::initializer_list<Tuple<Key>> il) { return insert(xyu::make_range(il)); }

        /* 更新 */

        /**
         * @brief 更新元素，并返回节点
         * @details 若键不存在，则插入构造元素；
         * 若键存在，仅一个元素时优先尝试赋值，否则或其他数量的元素个数时构造值后移动赋值
         * @param key 键
         * @param args 值 (或用于构造 Value 的参数)
         * @note 仅 Value 不为 void 时可用
         */
        template <typename... Args, typename Test = Value, typename = xyu::t_enable<!xyu::t_is_void<Test>>>
        Data& update(const Key& key, Args&&... args)
        {
            return increment_help(1, [&](HashTable& ht){ return ht.update_impl(key, xyu::forward<Args>(args)...); });
        }

        /**
         * @brief 依次更新范围中的每个键
         * @details 若键不存在，则插入构造元素；若键存在，进行赋值
         * @param range 键范围
         * @note 仅 Value 不为 void 时可用
         */
        template <typename Rg, typename Test = Value, xyu::t_enable<!xyu::t_is_void<Test> && !xyu::t_can_icast<Rg, Key> && xyu::t_is_range<Rg>, char> = 'd'>
        HashTable& update(Rg&& range)
        {
            static_assert(!xyu::t_is_void<Value>);
            static_assert(__::get_range_kv_type<Rg> > 0);
            increment_help(n + range.count(), [&](HashTable& ht)
            {
                constexpr int kind = __::get_range_kv_type<Rg>;
                for (auto&& e : range)
                {
                    if constexpr (kind == 2) ht.update_impl(xyu::forward<decltype(e)>(e).key, xyu::forward<decltype(e)>(e).val);
                    else if constexpr (kind == 3) ht.update_impl(xyu::forward<decltype(e)>(e).template get<0>(), xyu::forward<decltype(e)>(e).template get<1>());
                    else { auto&& [key, val] = xyu::forward<decltype(e)>(e); ht.update_impl(xyu::forward<decltype(key)>(key), xyu::forward<decltype(val)>(val)); }
                }
            });
            return *this;
        }

        /**
         * @brief 将参数构造为范围后，依次更新范围中的每个元素
         * @details 若键不存在，则插入构造元素；若键存在，进行赋值
         * @param arg 用于构造范围的参数
         * @note 仅 Value 不为 void 时可用
         */
        template <typename Arg, typename Test = Value, xyu::t_enable<!xyu::t_is_void<Test> && !xyu::t_can_icast<Arg, Key> && !xyu::t_is_range<Arg>, char> = 'x'>
        HashTable& update(Arg&& arg) { return update(xyu::make_range(arg)); }

        /**
         * @brief 将键值对通过初始化列表进行更新
         * @details 若键不存在，则插入构造元素；若键存在，进行赋值
         * @param il 键值对列表
         * @note 仅 Value 不为 void 时可用
         */
        template <typename Test = Value, xyu::t_enable<!xyu::t_is_void<Test>, bool> = true>
        HashTable& update(std::initializer_list<Tuple<Key, Value>> il) { return update(xyu::make_range(il)); }

        /**
         * @brief 将键通过初始化列表进行更新
         * @details 若键不存在，则插入构造元素；若键存在，进行赋值
         * @param il 键列表
         * @note 仅 Value 为 void 时可用
         */
        template <typename Test = Value, xyu::t_enable<xyu::t_is_void<Test>, bool> = false>
        HashTable& update(std::initializer_list<Tuple<Key>> il) { return update(xyu::make_range(il)); }

        /* 删除 */

        /**
         * @brief 删除元素，返回是否成功删除
         * @details 若键不存在，则不做处理
         * @param key 键
         */
        bool erase(const Key& key) noexcept
        {
            xyu::size_t hash = xyu::make_hash(key);
            xyu::size_t start = (hash >> 7) & (total - 1);
            xyu::size_t ci = start;
            do {
                // 查找是否已存在
                xyu::uint16 mask = __::cmpeq(get_ctrl(ci), hash & 0x7f);
                while (mask) {
                    xyu::uint offset = xyu::bit_count_0_back(mask);
                    Data& kv = get_kv(ci * 16 + offset);
                    if (kv.key == key)
                    {
                        // 删除元素
                        kv.~Data();
                        get_ctrl(ci).metas[offset] = DELETED;
                        --n;
                        return true;
                    }
                    mask &= mask - 1;
                }
                // 查找是否结束
                if (__::cmpeq(get_ctrl(ci), EMPTY)) break;
                ci = (ci + 1) & (total - 1);
            } while (ci != start);
            // 查找失败
            return false;
        }

        /* 范围 */

        /**
         * @brief 获取范围
         * @details
         * 当 Value 为 void 时，范围为直接的 key；
         * 否则，范围为 struct { Key key; Value val; }；
         */
        auto range() noexcept
        {
            using Gen = xyu::t_cond<xyu::t_is_void<Value>,
                    xyu::RangeGenerator_HashTable<HashTable, 1>,
                    xyu::RangeGenerator_HashTable<HashTable, 0>>;
            return xyu::Range<Gen>(n, ++Gen{*this});
        }
        /**
         * @brief 获取常量范围
         * @details
         * 当 Value 为 void 时，范围为直接的 key；
         * 否则，范围为 struct { Key key; Value val; }；
         */
        auto range() const noexcept { return const_cast<HashTable*>(this)->range().crange(); }

        /// 获取键范围
        auto krange() noexcept
        {
            using Gen = xyu::RangeGenerator_HashTable<HashTable, 1>;
            return xyu::Range<Gen>(n, ++Gen{*this});
        }
        /// 获取常量键范围
        auto krange() const noexcept { return const_cast<HashTable*>(this)->krange().crange(); }

        /// 获取值范围
        template <typename Test = Value, typename = xyu::t_enable<!xyu::t_is_void<Test>>>
        auto vrange() noexcept
        {
            using Gen = xyu::RangeGenerator_HashTable<HashTable, 2>;
            return xyu::Range<Gen>(n, ++Gen{*this});
        }
        /// 获取常量值范围
        template <typename Test = Value, typename = xyu::t_enable<!xyu::t_is_void<Test>>>
        auto vrange() const noexcept { return const_cast<HashTable*>(this)->vrange().crange(); }

        /* 运算符 */

        /**
         * @brief 获取值
         * @details 若键不存在，则通过默认构造插入元素
         * @param key 键
         * @note 仅 Value 不为 void 时可用
         */
        template <typename K, typename Test = Value, typename = xyu::t_enable<!xyu::t_is_void<Test>>>
        decltype(auto) operator[](K&& key)
        {
            if constexpr (xyu::t_can_icast<K, Key>) return get(key);
            else if constexpr (xyu::t_is_aggregate<Key>) return get(Key{key});
            else return get(Key(key));
        }
        /**
         * @brief 获取值
         * @details 若键不存在，则通过默认构造插入元素
         * @param key 键
         * @note 仅 Value 不为 void 时可用
         */
        template <typename K, typename Test = Value, typename = xyu::t_enable<!xyu::t_is_void<Test>>>
        decltype(auto) operator[](K&& key) const { return xyu::make_const(const_cast<HashTable*>(this)->operator[](key)); }

        /**
         * @brief 插入元素
         * @details 若键不存在，则插入构造元素；若键存在，进行赋值
         * @param arg 键 或 范围 或 初始化列表
         */
        template <typename Arg>
        HashTable& operator<<(Arg&& arg) { update(xyu::forward<Arg>(arg)); return *this; }

    private:
        // 获取控制字段
        __::ControlGroup& get_ctrl(xyu::size_t index) noexcept
        {
            return reinterpret_cast<__::ControlGroup*>(data)[index];
        }
        // 获取键值
        Data& get_kv(xyu::size_t index) noexcept
        {
            xyu::size_t ctrl_size = total;
            if constexpr (alignof(Data) > 16) ctrl_size = (ctrl_size + alignof(Data) - 1) & -alignof(Data);
            return *reinterpret_cast<Data*>(reinterpret_cast<char*>(data) + ctrl_size + index * sizeof(Data));
        }

        // 计算新容量
        xyu::size_t calc_new_total(xyu::size_t mincapa) const
        {
            if (XY_UNLIKELY(mincapa > limit())) {
                xyloge(false, "E_Memory_Capacity: capacity {} over limit {}", mincapa, limit());
                throw xyu::E_Memory_Capacity{};
            }
            auto acctual_capa = static_cast<xyu::size_t>(static_cast<double>(mincapa) / K_load_factor);
            xyu::size_t fixed_capa = xyu::bit_get_2ceil(acctual_capa);
            if (XY_UNLIKELY(fixed_capa < 16)) fixed_capa = 16;
            return xyu::max(fixed_capa, static_cast<xyu::size_t>(total * 2));
        }

        // 增加元素辅助函数
        template <typename Fun>
        decltype(auto) increment_help(xyu::size_t count, Fun f)
        {
            if (n + count <= capa) return f(*this);
            else {
                HashTable tmp(n + count);
                tmp.insert(range().mrange());
                if constexpr (xyu::t_is_same<xyu::t_get_ret<Fun, HashTable&>, Data&>)
                {
                    Data& kv = f(tmp);
                    swap(tmp);
                    return kv;
                }
                else
                {
                    f(tmp);
                    swap(tmp);
                }
            }
        }

        // insert 细节
        template <typename... Args>
        Data& insert_impl(const Key& key, Args&&... args)
        {
            xyu::size_t hash = xyu::make_hash(key);
            xyu::size_t ci = (hash >> 7) & (total - 1);
            for(;;)
            {
                // 查找是否已存在
                xyu::uint16 mask = __::cmpeq(get_ctrl(ci), hash & 0x7f);
                if (mask) {
                    xyu::uint offset = xyu::bit_count_0_back(mask);
                    return get_kv(ci * 16 + offset);
                }
                // 查找空位
                mask = __::msb(get_ctrl(ci));
                if (mask) {
                    xyu::uint offset = xyu::bit_count_0_back(mask);
                    Data& kv = get_kv(ci * 16 + offset);
                    ::new (&kv) Data{key, xyu::forward<Args>(args)...};
                    get_ctrl(ci).metas[offset] = hash & 0x7f;
                    ++n;
                    return kv;
                }
                ci = (ci + 1) & (total - 1);
            }
        }

        // update 辅助
        template <typename... Args>
        Data& update_impl(const Key& key, Args&&... args)
        {
            xyu::size_t hash = xyu::make_hash(key);
            xyu::size_t start = (hash >> 7) & (total - 1);
            xyu::size_t ii = -1;
            xyu::size_t ci = start;
            do {
                // 查找是否已存在
                xyu::uint16 mask = __::cmpeq(get_ctrl(ci), hash & 0x7f);
                while (mask) {
                    xyu::uint offset = xyu::bit_count_0_back(mask);
                    Data& kv = get_kv(ci * 16 + offset);
                    if (xyu::equals(kv.key, key))
                    {
                        // 更新值
                        if constexpr (sizeof...(Args) != 1 || !xyu::t_can_assign<Value&, Args...>)
                        {
                            if constexpr (xyu::t_is_aggregate<Value>) kv.val = Value{xyu::forward<Args>(args)...};
                            else kv.val = Value(xyu::forward<Args>(args)...);
                        }
                        else (..., (kv.val = xyu::forward<Args>(args)));
                        return kv.val;
                    }
                    mask &= mask - 1;
                }
                // 若没有插入指针
                if (ii == -1) {
                    mask = __::msb(get_ctrl(ci));
                    if (mask) ii = ci * 16 + xyu::bit_count_0_back(mask);
                    else if (__::cmpeq(get_ctrl(ci), EMPTY)) break;
                }
                // 判断是否结束
                else if (__::cmpeq(get_ctrl(ci), EMPTY)) break;
                ci = (ci + 1) & (total - 1);
            } while (ci != start);
            // 插入元素
            Data& kv = get_kv(ii);
            ::new (&kv) Data{key, xyu::forward<Args>(args)...};
            get_ctrl(ii >> 4).metas[ii & (16 - 1)] = hash & 0x7f;
            ++n;
            return kv.val;
        }

    private:
        friend class xylu::xyrange::RangeGenerator_Valid_HashTable;
        friend class xylu::xyrange::RangeGenerator_Increment_HashTable;
        template <int> friend class xylu::xyrange::RangeGenerator_Dereference_HashTable;
        static_assert(K_load_factor >= (1./16.) && K_load_factor <= 1 && limit() >= 16);
    };
}

#pragma clang diagnostic pop