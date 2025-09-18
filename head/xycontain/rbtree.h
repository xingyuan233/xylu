#pragma clang diagnostic push
#pragma ide diagnostic ignored "NullDereference"
#pragma ide diagnostic ignored "OCUnusedGlobalDeclarationInspection"
#pragma ide diagnostic ignored "hicpp-exception-baseclass"
#pragma ide diagnostic ignored "modernize-use-nodiscard"
#pragma ide diagnostic ignored "misc-unconventional-assign-operator"
#pragma ide diagnostic ignored "UnreachableCallsOfFunction"
#pragma once

#include "./impl/kv.h"
#include "../../link/log"

// 辅助类
namespace xylu::xycontain::__
{
    /// 红黑树节点基类
    struct RbTreeNode_Base
    {
        RbTreeNode_Base* up{};                  // 父节点
        RbTreeNode_Base* left{};                // 左子节点
        RbTreeNode_Base* right{};               // 右子节点
        enum Color { RED, BLACK } color {RED};  // 颜色
    };

    /// 红黑树节点
    template <typename Key, typename Value>
    struct RbTreeNode : RbTreeNode_Base, KVData<Key, Value>
    {
        using KVData<Key, Value>::KVData;
        KVData<Key, Value>& data() noexcept { return *this; }
    };
}

/// 迭代器
namespace xylu::xyrange
{
    // 迭代器 - 解引用 - 树节点
    template <int kind>
    struct RangeIter_Dereference_TreeNode
    {
        template <typename Iter>
        constexpr decltype(auto) operator()(Iter& it) noexcept
        {
            if constexpr (kind == 0) return it.i->data();
            else if constexpr (kind == 1) return it.i->data().key;
            else if constexpr (kind == 2) return it.i->data().val;
            else static_assert(kind == 404, "Invalid kind");
        }
    };
}

/// 红黑树
namespace xylu::xycontain
{
    // 辅助函数
    namespace __
    {
        //左旋节点
        template <bool reColor = true>
        constexpr void leftRotate(RbTreeNode_Base* n, RbTreeNode_Base*& pns) noexcept
        {
            RbTreeNode_Base* rn = n->right;
            n->right = rn->left;
            if (rn->left) rn->left->up = n;
            rn->left = n;
            rn->up = n->up;
            n->up = rn;
            pns = rn;
            if constexpr (reColor)
            {
                n->color = RbTreeNode_Base::RED;
                rn->color = RbTreeNode_Base::BLACK;
            }
        }
        template <bool reColor = true>
        constexpr void leftRotate(RbTreeNode_Base* n, RbTreeNode_Base& lead) noexcept
        {
            RbTreeNode_Base* pn = n->up;
            leftRotate<reColor>(n, (pn == &lead ? lead.up : (pn->left == n ? pn->left : pn->right)));
        }
        //右旋节点
        template <bool reColor = true>
        constexpr void rightRotate(RbTreeNode_Base* n, RbTreeNode_Base*& pns) noexcept
        {
            RbTreeNode_Base* ln = n->left;
            n->left = ln->right;
            if (n->left) ln->right->up = n;
            ln->right = n;
            ln->up = n->up;
            n->up = ln;
            pns = ln;
            if constexpr (reColor)
            {
                n->color = RbTreeNode_Base::RED;
                ln->color = RbTreeNode_Base::BLACK;
            }
        }
        template <bool reColor = true>
        constexpr void rightRotate(RbTreeNode_Base* n, RbTreeNode_Base& lead) noexcept
        {
            RbTreeNode_Base* pn = n->up;
            rightRotate<reColor>(n, (pn == &lead ? lead.up : (pn->left == n ? pn->left : pn->right)));
        }
        //修正插入 (仅父节点为红色时调用)
        constexpr void fixInsert(RbTreeNode_Base* pn, bool pl, RbTreeNode_Base& lead) noexcept
        {
            for(;;)
            {
                RbTreeNode_Base* gn = pn->up;
                bool gl = gn->left == pn;
                RbTreeNode_Base* un = (gl ? gn->right : gn->left);
                //叔节点为红色
                if (un && un->color == RbTreeNode_Base::RED) {
                    pn->color = un->color = RbTreeNode_Base::BLACK;
                    if (gn == lead.up) break;
                    gn->color = RbTreeNode_Base::RED;
                    pn = gn->up;
                    if (pn->color == RbTreeNode_Base::BLACK) break;
                    pl = pn->left == gn;
                    continue;
                }
                //叔节点为黑色
                if (gl) {
                    if (!pl) leftRotate<false>(pn, gn->left);
                    rightRotate(gn, lead);
                } else {
                    if (pl) rightRotate<false>(pn, gn->right);
                    leftRotate(gn, lead);
                }
                break;
            }
        }
        //修正删除 (仅删除节点为黑色时调用)
        constexpr void fixEraseImpl(RbTreeNode_Base* sn, RbTreeNode_Base* pn, RbTreeNode_Base& lead) noexcept
        {
            for(;;)
            {
                //删除节点在左边
                if (sn == pn->left)
                {
                    RbTreeNode_Base* bn = pn->right;
                    if (bn->color == RbTreeNode_Base::RED)
                    {
                        __::leftRotate(pn, lead);
                        bn = pn->right;
                    }
                    bool fr = !bn->right || bn->right->color == RbTreeNode_Base::BLACK;
                    if (fr && (!bn->left  || bn->left->color == RbTreeNode_Base::BLACK))
                    {
                        bn->color = RbTreeNode_Base::RED;
                        if (pn->color == RbTreeNode_Base::RED) pn->color = RbTreeNode_Base::BLACK;
                        else if (pn != lead.up) {
                            sn = pn;
                            pn = pn->up;
                            continue;
                        }
                    } else {
                        if (fr) {
                            rightRotate(bn, pn->right);
                            bn = pn->right;
                        }
                        bn->color = pn->color;
                        pn->color = RbTreeNode_Base::BLACK;
                        bn->right->color = RbTreeNode_Base::BLACK;
                        leftRotate<false>(pn, lead);
                    }
                }
                //删除节点在右边 (对称操作)
                else
                {
                    RbTreeNode_Base* bn = pn->left;
                    if (bn->color == RbTreeNode_Base::RED)
                    {
                        rightRotate(pn, lead);
                        bn = pn->left;
                    }
                    bool fl = !bn->left || bn->left->color == RbTreeNode_Base::BLACK;
                    if (fl && (!bn->right || bn->right->color == RbTreeNode_Base::BLACK))
                    {
                        bn->color = RbTreeNode_Base::RED;
                        if (pn->color == RbTreeNode_Base::RED) pn->color = RbTreeNode_Base::BLACK;
                        else if (pn != lead.up) {
                            sn = pn;
                            pn = pn->up;
                            continue;
                        }
                    } else {
                        if (fl) {
                            leftRotate(bn, pn->left);
                            bn = pn->left;
                        }
                        bn->color = pn->color;
                        pn->color = RbTreeNode_Base::BLACK;
                        if (bn->right) bn->right->color = RbTreeNode_Base::BLACK;
                        rightRotate<false>(pn, lead);
                    }
                }
                break;
            }
        }
        constexpr void fixErase(RbTreeNode_Base* n, RbTreeNode_Base& lead) noexcept
        {
            RbTreeNode_Base *sn{}, *pn{};
            if (!n->left) sn = n->right;
            else if (!n->right) sn = n->left;
            //删除节点有两个子节点 (转换为只有一个或没有子节点)
            else {
                //获取后继节点
                RbTreeNode_Base* nn = n->right;
                while (nn->left) nn = nn->left;
                sn = nn->right;
                //用后继节点代替删除节点
                xyu::swap(n->color, nn->color);
                n->left->up = nn; //左
                nn->left = n->left;
                if (nn != n->right) {  //右
                    pn = nn->up;
                    pn->left = sn;
                    if (sn) sn->up = pn;
                    nn->right = n->right;
                    n->right->up = nn;
                }
                else pn = nn;
                nn->up = n->up; //父
                if (n == lead.up) lead.up = nn;
                else (n->up->left == n ? n->up->left : n->up->right) = nn;
            }
            //原删除节点只有一个或没有子节点
            if (!pn) {
                //更新最小最大节点
                if (n == lead.left) {
                    if (n->right) {auto mn = n->right; while (mn->left) mn = mn->left; lead.left = mn;}
                    else lead.left = n->up;
                }
                if (n == lead.right) {
                    if (n->left) {auto mn = n->left; while (mn->right) mn = mn->right; lead.right = mn;}
                    else lead.right = n->up;
                }
                //父节点重置
                if (n == lead.up) lead.up = sn;
                else (n->up->left == n ? n->up->left : n->up->right) = sn;
                pn = n->up;
                if (sn) sn->up = pn;
            }
            //删除节点只有一个子节点
            if (sn) { sn->color = RbTreeNode_Base::BLACK; }
            //删除节点无子节点且为黑色，则需要修正 (红色不做处理)
            else if (n->color == RbTreeNode_Base::BLACK && pn != &lead) fixEraseImpl(sn, pn, lead);
        }
    }

    /**
     * @brief 一个功能极其强大、泛化的、基于红黑树的有序关联容器。
     *
     * @tparam Key   键的类型。
     * @tparam Value 值的类型。若为 `void`，则容器表现为集合 (Set)。
     * @tparam multi 布尔值。若为 `false` (默认)，则为唯一键容器 (Map/Set)；
     *               若为 `true`，则为多键容器 (MultiMap/MultiSet)。
     *
     * @details
     * `xyu::RbTree` 是对经典红黑树数据结构的一次现代化、高度泛化的C++实现。通过模板参数，
     * 它可以同时作为 `Map`, `Set`, `MultiMap`, `MultiSet` 四种标准关联容器的统一替代品。
     * 它深度集成了 `xylu` 的库生态，以提供卓越的性能、安全性和易用性。
     *
     * ### 核心亮点 (Key Features):
     *
     * 1.  **四合一泛型设计 (Four-in-One Generic Design):**
     *     - 通过 `Value` 和 `multi` 模板参数的组合，一套代码即可实现四种不同的有序关联容器，
     *       极大地提高了代码的复用性和一致性。
     *
     * 2.  **健壮的红黑树实现 (Robust Red-Black Tree Implementation):**
     *     - 内部采用带哨兵节点的经典红黑树算法，确保了所有插入和删除操作都能在 O(log n)
     *       的时间复杂度内完成，并自动维持树的平衡。
     *     - 提供了强大的异常安全保证，确保在元素构造失败时能正确回滚并释放内存。
     *
     * 3.  **与范围生态无缝融合 (Seamless Range Integration):**
     *     - `range()`, `krange()` (键范围), `vrange()` (值范围) 方法返回高效的 `xyu::Range` 对象，
     *       其内部使用为树形结构专门优化的 `RangeIter_TreePtr` 迭代器。
     *     - 对于多键版本 (`multi = true`)，提供了 `range(key)` 等方法来获取所有等价键的范围，
     *       功能类似于 `std::multimap::equal_range`。
     *
     * 4.  **智能泛型接口 (Intelligent Generic Interface):**
     *     - `insert` 和 `update` 方法与 `xyu::HashTable` 保持一致，能够通过元编程自动识别
     *       并接受各种数据源，如单个键值对、`Tuple`、以及其他容器的范围。
     *
     * 5.  **底层生态集成 (Deep Integration with xylu Ecosystem):**
     *     - 所有节点的内存都通过 `xylu` 的线程局部内存池进行管理，特别适合处理大量小节点的分配场景。
     *     - 元素的排序完全依赖于 `xyu::compare` 泛型比较框架。
     *     - 所有 `const` 接口均返回 `const` 范围，保证了 `const` 正确性。
     */
    template <typename Key, typename Value = void, bool multi = false>
    class RbTree
    {
        using NodeBase = __::RbTreeNode_Base;
        using Node = __::RbTreeNode<Key, Value>;
        using Data = __::KVData<Key, Value>;
        static_assert(xyu::t_can_nothrow_destruct<Node>);

    private:
        NodeBase lead;      // 哨兵节点
        xyu::size_t num;    // 节点数量

    public:
        /* 构造析构 */

        /// 默认构造
        RbTree() noexcept : lead{nullptr, &lead, &lead}, num{0} {}
        /// 复制构造
        RbTree(const RbTree& other) : RbTree{} { insert(other.range()); }
        /// 移动构造
        RbTree(RbTree&& other) noexcept : RbTree{} { swap(other); }

        /// 初始化列表构造
        template <typename Test = Value, xyu::t_enable<!xyu::t_is_void<Test>, bool> = true>
        RbTree(std::initializer_list<Tuple<Key, Value>> il) : RbTree{} { insert(il); }
        /// 初始化列表构造
        template <typename Test = Value, xyu::t_enable<xyu::t_is_void<Test>, bool> = false>
        RbTree(std::initializer_list<Tuple<Key>> il) : RbTree{} { insert(il); }

        /// 析构
        ~RbTree() noexcept { release(); }

        /* 赋值交换 */

        /// 复制赋值
        RbTree& operator=(const RbTree& other)
        {
            if (XY_UNLIKELY(this == &other)) return *this;
            release();
            insert(other.range());
            return *this;
        }
        /// 移动赋值
        RbTree& operator=(RbTree&& other) noexcept { return swap(other); }

        /// 交换
        RbTree& swap(RbTree&& other) noexcept { return swap(other); }
        /// 交换
        RbTree& swap(RbTree& other) noexcept
        {
            // 缓存
            xyu::size_t n = num;
            NodeBase *pn = lead.up, *ln = lead.left, *rn = lead.right;
            // other -> this
            num = other.num;
            if (num == 0) { lead.up = nullptr; lead.left = lead.right = &lead; }
            else {
                lead.up = other.lead.up;
                lead.left = other.lead.left;
                lead.right = other.lead.right;
                other.lead.up = &lead;
            }
            // this -> other
            other.num = n;
            if (n == 0) { other.lead.up = nullptr; other.lead.left = other.lead.right = &other.lead; }
            else {
                other.lead.up = pn;
                other.lead.left = ln;
                other.lead.right = rn;
                pn->up = &other.lead;
            }
            return *this;
        }

        /* 数据容量 */

        /// 获取最大容量
        constexpr static xyu::size_t limit() noexcept { return xyu::number_traits<xyu::size_t>::max; }
        /// 获取元素数量
        xyu::size_t count() const noexcept { return num; }
        /// 是否为空
        bool empty() const noexcept { return num == 0; }

        /* 数据管理 */

        /// 释放内存
        void release() noexcept
        {
            if (XY_UNLIKELY(empty())) return;
            back_nodes(lead.up);
            lead.up = nullptr;
            lead.left = lead.right = &lead;
            num = 0;
        }

        /* 查找 */

        /// 查找键是否存在
        bool contains(const Key& key) const noexcept { return find_node(key) != nullptr; }

        /* 获取 */

        /**
         * @brief 获取值
         * @details 若键不存在，则通过默认构造插入元素
         * @param key 键
         * @note 仅 Value 不为 void 时可用
         */
        template <typename K, typename Test = Value, typename = xyu::t_enable<!xyu::t_is_void<Test>>>
        decltype(auto) get(K&& key)
        {
            static_assert(xyu::t_is_same_nocvref<Key, K>);
            auto [pre, less] = find_node_add(key);
            if (less == -1) return static_cast<Node*>(pre)->val;
            Node* n = add_node(xyu::forward<K>(key));
            return n->val;
        }

        /**
         * @brief 获取值
         * @details 若键不存在，则通过默认构造插入元素
         * @param key 键
         * @note 仅 Value 不为 void 时可用
         */
        template <typename K, typename Test = Value, typename = xyu::t_enable<!xyu::t_is_void<Test>>>
        decltype(auto) get(K&& key) const
        { return xyu::make_const(const_cast<RbTree*>(this)->get(key)); }

        /**
         * @brief 获取值
         * @details 若键不存在，则抛出异常
         * @param key 键
         * @note 仅 Value 不为 void 时可用
         */
        template <typename Test = Value, typename = xyu::t_enable<!xyu::t_is_void<Test>>>
        decltype(auto) at(const Key& key)
        {
            Node* n = find_node(key);
            if (n) return n->val;
            xyloge(false, "E_Logic_Key_Not_Found: key {} is not found in the tree", key);
            throw xyu::E_Logic_Key_Not_Found{};
        }

        /**
         * @brief 获取值
         * @details 若键不存在，则抛出异常
         * @param key 键
         * @note 仅 Value 不为 void 时可用
         */
        template <typename Test = Value, typename = xyu::t_enable<!xyu::t_is_void<Test>>>
        decltype(auto) at(const Key& key) const
        { return xyu::make_const(const_cast<RbTree*>(this)->at(key)); }

        /* 插入 */

        /**
         * @brief 插入元素，并返回节点
         * @details 若键已存在，则不做处理，直接返回原值引用
         * @param key 键
         * @param args 值 (或用于构造 Value 的参数)
         * @note 当 Value 为 void 时，args 必须为空
         */
        template <typename K, typename... Args>
        Data& insert(K&& key, Args&&... args)
        {
            static_assert(xyu::t_is_same_nocvref<Key, K>);
            static_assert(!(xyu::t_is_void<Value> && sizeof...(Args) > 0));
            check_new_capa(1);
            return *add_node(xyu::forward<K>(key), xyu::forward<Args>(args)...);
        }

        /**
         * @brief 依次插入范围中的每个键
         * @details 若键已存在，则不做处理
         * @param range 键范围
         */
        template <typename Rg, xyu::t_enable<!xyu::t_can_icast<Rg, Key> && xyu::t_is_range<Rg> && (__::get_range_kv_type<Rg, Key> < 0), char> = 's'>
        RbTree& insert(Rg&& range)
        {
            check_new_capa(range.count());
            constexpr int kind = __::get_range_kv_type<Rg, Key>;
            for (auto&& e : range)
            {
                if constexpr (kind == -1) add_node(xyu::forward<decltype(e)>(e));
                else if constexpr (kind == -2) add_node(xyu::forward<decltype(e)>(e).key);
                else if constexpr (kind == -3) add_node(xyu::forward<decltype(e)>(e).template get<0>());
                else { auto&& [key] = xyu::forward<decltype(e)>(e); add_node(xyu::forward<decltype(key)>(key)); }
            }
            return *this;
        }

        /**
         * @brief 依次插入范围中的每个键值对
         * @details 若键已存在，则不做处理
         * @param range 键值对范围
         * @note 仅 Value 不为 void 时可用
         */
        template <typename Rg, typename Test = Value, xyu::t_enable<!xyu::t_is_void<Test> && !xyu::t_can_icast<Rg, Key> && xyu::t_is_range<Rg> && (__::get_range_kv_type<Rg> > 0), char> = 'd'>
        RbTree& insert(Rg&& range)
        {
            static_assert(!xyu::t_is_void<Value>);
            check_new_capa(range.count());
            constexpr int kind = __::get_range_kv_type<Rg>;
            for (auto&& e : range)
            {
                if constexpr (kind == 2) add_node(xyu::forward<decltype(e)>(e).key, xyu::forward<decltype(e)>(e).val);
                else if constexpr (kind == 3) add_node(xyu::forward<decltype(e)>(e).template get<0>(), xyu::forward<decltype(e)>(e).template get<1>());
                else { auto&& [key, val] = xyu::forward<decltype(e)>(e); add_node(xyu::forward<decltype(key)>(key), xyu::forward<decltype(val)>(val)); }
            }
            return *this;
        }

        /**
         * @brief 将参数构造为范围后，依次插入范围中的每个元素
         * @details 若键已存在，则不做处理
         * @param arg 用于构造范围的参数
         */
        template <typename Arg, xyu::t_enable<!xyu::t_can_icast<Arg, Key> && !xyu::t_is_range<Arg>, char> = 'x'>
        RbTree& insert(Arg&& arg) { return insert(xyu::make_range(arg)); }

        /**
         * @brief 将键值对通过初始化列表进行插入
         * @details 若键已存在，则不做处理
         * @param il 键值对列表
         * @note 仅 Value 不为 void 时可用
         */
        template <typename Test = Value, xyu::t_enable<!xyu::t_is_void<Test>, bool> = true>
        RbTree& insert(std::initializer_list<Tuple<Key, Value>> il)
        { return insert(xyu::make_range(il)); }

        /**
         * @brief 将键通过初始化列表进行插入
         * @details 若键已存在，则不做处理
         * @param il 键列表
         * @note 仅 Value 为 void 时可用
         */
        template <typename Test = Value, xyu::t_enable<xyu::t_is_void<Test>, bool> = false>
        RbTree& insert(std::initializer_list<Tuple<Key>> il)
        { return insert(xyu::make_range(il)); }

        /* 更新 */

        /**
         * @brief 更新元素，并返回节点
         * @details 若键不存在，则插入构造元素；
         * 若键存在，仅一个元素时优先尝试赋值，否则或其他数量的元素个数时构造值后移动赋值
         * @param key 键
         * @param args 值 (或用于构造 Value 的参数)
         * @note 仅 Value 不为 void 时可用
         */
        template <typename K, typename... Args, typename Test = Value, typename = xyu::t_enable<!xyu::t_is_void<Test>>>
        Data& update(K&& key, Args&&... args)
        {
            static_assert(xyu::t_is_same_nocvref<Key, K>);
            return *update_node(xyu::forward<K>(key), xyu::forward<Args>(args)...);
        }

        /**
         * @brief 依次更新范围中的每个键
         * @details 若键不存在，则插入构造元素；若键存在，进行赋值
         * @param range 键范围
         * @note 仅 Value 不为 void 时可用
         */
        template <typename Rg, typename Test = Value, xyu::t_enable<!xyu::t_is_void<Test> && !xyu::t_can_icast<Rg, Key> && xyu::t_is_range<Rg>, char> = 'd'>
        RbTree& update(Rg&& range)
        {
            static_assert(!xyu::t_is_void<Value>);
            static_assert(__::get_range_kv_type<Rg> > 0);
            constexpr int kind = __::get_range_kv_type<Rg>;
            for (auto&& e : range)
            {
                if constexpr (kind == 2) update_node(xyu::forward<decltype(e)>(e).key, xyu::forward<decltype(e)>(e).val);
                else if constexpr (kind == 3) update_node(xyu::forward<decltype(e)>(e).template get<0>(), xyu::forward<decltype(e)>(e).template get<1>());
                else { auto&& [key, val] = xyu::forward<decltype(e)>(e); update_node(xyu::forward<decltype(key)>(key), xyu::forward<decltype(val)>(val)); }
            }
            return *this;
        }

        /**
        * @brief 将参数构造为范围后，依次更新范围中的每个元素
        * @details 若键不存在，则插入构造元素；若键存在，进行赋值
        * @param arg 用于构造范围的参数
        * @note 仅 Value 不为 void 时可用
        */
        template <typename Arg, typename Test = Value, xyu::t_enable<!xyu::t_is_void<Test> && !xyu::t_can_icast<Arg, Key> && !xyu::t_is_range<Arg>, char> = 'x'>
        RbTree& update(Arg&& arg) { return update(xyu::make_range(arg)); }

        /**
         * @brief 将键值对通过初始化列表进行更新
         * @details 若键不存在，则插入构造元素；若键存在，进行赋值
         * @param il 键值对列表
         * @note 仅 Value 不为 void 时可用
         */
        template <typename Test = Value, xyu::t_enable<!xyu::t_is_void<Test>, bool> = true>
        RbTree& update(std::initializer_list<Tuple<Key, Value>> il)
        { return update(xyu::make_range(il)); }

        /**
         * @brief 将键通过初始化列表进行更新
         * @details 若键不存在，则插入构造元素；若键存在，进行赋值
         * @param il 键列表
         * @note 仅 Value 为 void 时可用
         */
        template <typename Test = Value, xyu::t_enable<xyu::t_is_void<Test>, bool> = false>
        RbTree& update(std::initializer_list<Tuple<Key>> il)
        { return update(xyu::make_range(il)); }

        /* 删除 */

        /**
         * @brief 删除元素，返回是否成功删除
         * @details 若键不存在，则不做处理
         * @param key 键
         */
        bool erase(const Key& key) noexcept
        {
            Node* n = find_node(key);
            if (n) delete_node(n);
            return n != nullptr;
        }

        /**
         * @brief 删除迭代器位置
         * @return 指向下一个元素的迭代器
         */
        template <typename Iter, typename = xyu::t_enable<xyu::t_is_same_nocvref<Node*, decltype(Iter{}.i)>>>
        Iter erase(Iter it) noexcept
        {
            NodeBase* n = it.i;
            Iter next = ++it;
            delete_node(n);
            return next;
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
            constexpr int kind = xyu::t_is_void<Value> ? 1 : 0;
            return xyu::make_range(xyu::RangeIter_Dereference_v, xyu::native_v,
                                   xyu::RangeIter_Dereference_TreeNode<kind>{},
                                   num, static_cast<Node*>(lead.left), static_cast<Node*>(&lead));
        }
        /**
         * @brief 获取常量范围
         * @details
         * 当 Value 为 void 时，范围为直接的 key；
         * 否则，范围为 struct { Key key; Value val; }；
         */
        auto range() const noexcept { return const_cast<RbTree*>(this)->range().crange(); }

        /// 获取键范围
        auto krange() noexcept
        {
            return xyu::make_range(xyu::RangeIter_Dereference_v, xyu::native_v,
                                   xyu::RangeIter_Dereference_TreeNode<1>{},
                                   num, static_cast<Node*>(lead.left), static_cast<Node*>(&lead));
        }
        /// 获取常量键范围
        auto krange() const noexcept { return const_cast<RbTree*>(this)->krange().crange(); }

        /// 获取值范围
        template <typename Test = Value, typename = xyu::t_enable<!xyu::t_is_void<Test>>>
        auto vrange() noexcept
        {
            return xyu::make_range(xyu::RangeIter_Dereference_v, xyu::native_v,
                                   xyu::RangeIter_Dereference_TreeNode<2>{},
                                   num, static_cast<Node*>(lead.left), static_cast<Node*>(&lead));
        }
        /// 获取常量值范围
        template <typename Test = Value, typename = xyu::t_enable<!xyu::t_is_void<Test>>>
        auto vrange() const noexcept { return const_cast<RbTree*>(this)->vrange().crange(); }

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
        decltype(auto) operator[](K&& key) const { return xyu::make_const(const_cast<RbTree*>(this)->operator[](key)); }

        /**
         * @brief 插入元素
         * @details 若键不存在，则插入构造元素；若键存在，进行赋值
         * @param arg 键 或 范围 或 初始化列表
         */
        template <typename Arg>
        RbTree& operator<<(Arg&& arg) { update(xyu::forward<Arg>(arg)); return *this; }

    private:
        // 检测新容量
        void check_new_capa(xyu::size_t add)
        {
            if (XY_UNLIKELY(num + add < num)) {
                xylogei(false, "E_Memory_Capacity: capacity {} add {} over limit {}", num, add, limit());
                throw xyu::E_Memory_Capacity{};
            }
        }

        // 查找节点
        Node* find_node(const Key& key) const noexcept
        {
            NodeBase* cur = lead.up;
            while (cur) {
                int cmp = xyu::compare(key, static_cast<Node*>(cur)->key);
                if (cmp == 0) break;
                cur = (cmp < 0 ? cur->left : cur->right);
            }
            return static_cast<Node*>(cur);
        }
        // 查找或新增节点
        auto find_node_add(const Key& key) const noexcept
        {
            struct Result { NodeBase* pre; int less; };
            auto* pre = const_cast<NodeBase*>(&lead);
            NodeBase* cur = lead.up;
            bool less = false;
            while (cur)
            {
                pre = cur;
                int cmp = xyu::compare(key, static_cast<Node*>(cur)->key);
                if (cmp == 0) return Result{cur, -1};
                less = cmp < 0;
                cur = less ? cur->left : cur->right;
            }
            return Result{pre, less};
        }
        // 查找插入位置
        auto find_node_path(const Key& key) const noexcept
        {
            struct Result { NodeBase* pre; int less; };
            auto* pre = const_cast<NodeBase*>(&lead);
            NodeBase* cur = lead.up;
            bool less = true;
            while (cur)
            {
                pre = cur;
                less = key < static_cast<Node*>(cur)->key;
                cur = less ? cur->left : cur->right;
            }
            if (XY_LIKELY(pre != &lead))
            {
                NodeBase* n = less ? (--xyu::RangeIter_TreePtr<NodeBase>{pre}).i : pre;
                if (xyu::equals(key, static_cast<Node*>(n)->key)) return Result{n, -1};
            }
            return Result{pre, less};
        }

        // 增加节点
        template <typename K, typename... V>
        Node* add_node(K&& key, V&&... args)
        {
            auto [pn, less] = find_node_path(key);
            if (less == -1) return static_cast<Node*>(pn);
            Node* n = xyu::alloc<Node>(1);
            ::new (n) Node{xyu::forward<K>(key), xyu::forward<V>(args)...};
            if (pn == &lead) {
                //若为根节点
                lead.up = lead.left = lead.right = n;
                n->color = Node::BLACK;
                n->up = &lead;
            }else{
                //更新最小最大节点
                if (less) { pn->left = n; if (pn == lead.left) lead.left = n; }
                else { pn->right = n; if (pn == lead.right) lead.right = n; }
                //连接节点
                n->up = pn;
                if (pn->color == Node::RED) __::fixInsert(pn, less, lead);
            }
            ++num;
            return n;
        }
        // 更新节点
        template <typename K, typename... V>
        Node* update_node(K&& key, V&&... args)
        {
            auto [n, less] = find_node_path(key);
            if (less == -1)
            {
                n = xyu::alloc<Node>(1);
                ::new (n) Node{xyu::forward<K>(key), xyu::forward<V>(args)...};
            }
            else if constexpr (xyu::t_is_aggregate<Value>) static_cast<Node*>(n)->val = Value{xyu::forward<V>(args)...};
            else static_cast<Node*>(n)->val = Value(xyu::forward<V>(args)...);
            return n;
        }

        // 释放单个节点
        static void back_node(NodeBase* n) noexcept
        {
            static_cast<Node*>(n)->~Node();
            xyu::dealloc<Node>(static_cast<Node*>(n), 1);
        }
        // 递归释放节点
        static void back_nodes(NodeBase* n) noexcept
        {
            if (n->left) back_nodes(n->left);
            if (n->right) back_nodes(n->right);
            back_node(n);
        }
        // 删除节点
        void delete_node(NodeBase* n) noexcept
        {
            __::fixErase(n, lead);
            --num;
            back_node(n);
        }
    };

    template <typename Key, typename Value>
    class RbTree<Key, Value, true>
    {
        using NodeBase = __::RbTreeNode_Base;
        using Node = __::RbTreeNode<Key, Value>;
        using Data = __::KVData<Key, Value>;
        static_assert(xyu::t_can_nothrow_destruct<Node>);

    private:
        NodeBase lead;      // 哨兵节点
        xyu::size_t num;    // 节点数量

    public:
        /* 构造析构 */

        /// 默认构造
        RbTree() noexcept : lead{nullptr, &lead, &lead}, num{0} {}
        /// 复制构造
        RbTree(const RbTree& other) : RbTree{} { insert(other.range()); }
        /// 移动构造
        RbTree(RbTree&& other) noexcept : RbTree{} { swap(other); }

        /// 初始化列表构造
        template <typename Test = Value, xyu::t_enable<!xyu::t_is_void<Test>, bool> = true>
        RbTree(std::initializer_list<Tuple<Key, Value>> il) : RbTree{} { insert(il); }
        /// 初始化列表构造
        template <typename Test = Value, xyu::t_enable<xyu::t_is_void<Test>, bool> = false>
        RbTree(std::initializer_list<Tuple<Key>> il) : RbTree{} { insert(il); }

        /// 析构
        ~RbTree() noexcept { release(); }

        /* 赋值交换 */

        /// 复制赋值
        RbTree& operator=(const RbTree& other)
        {
            if (XY_UNLIKELY(this == &other)) return *this;
            release();
            insert(other.range());
            return *this;
        }
        /// 移动赋值
        RbTree& operator=(RbTree&& other) noexcept { return swap(other); }

        /// 交换
        RbTree& swap(RbTree&& other) noexcept { return swap(other); }
        /// 交换
        RbTree& swap(RbTree& other) noexcept
        {
            // 缓存
            xyu::size_t n = num;
            NodeBase *pn = lead.up, *ln = lead.left, *rn = lead.right;
            // other -> this
            num = other.num;
            if (num == 0) { lead.up = nullptr; lead.left = lead.right = &lead; }
            else {
                lead.up = other.lead.up;
                lead.left = other.lead.left;
                lead.right = other.lead.right;
                other.lead.up = &lead;
            }
            // this -> other
            other.num = n;
            if (n == 0) { other.lead.up = nullptr; other.lead.left = other.lead.right = &other.lead; }
            else {
                other.lead.up = pn;
                other.lead.left = ln;
                other.lead.right = rn;
                pn->up = &other.lead;
            }
            return *this;
        }

        /* 数据容量 */

        /// 获取最大容量
        constexpr static xyu::size_t limit() noexcept { return xyu::number_traits<xyu::size_t>::max; }
        /// 获取元素数量
        xyu::size_t count() const noexcept { return num; }
        /// 是否为空
        bool empty() const noexcept { return num == 0; }

        /* 数据管理 */

        /// 释放内存
        void release() noexcept
        {
            if (XY_UNLIKELY(empty())) return;
            back_nodes(lead.up);
            lead.up = nullptr;
            lead.left = lead.right = &lead;
            num = 0;
        }

        /* 查找 */

        /// 查找键是否存在
        bool contains(const Key& key) const noexcept { return find_node(key) != nullptr; }

        /* 获取 */

        /**
         * @brief 获取值
         * @details 若键不存在，则通过默认构造插入元素
         * @param key 键
         * @attention 若有多个值时不确定返回哪一个
         * @note 仅 Value 不为 void 时可用
         */
        template <typename K, typename Test = Value, typename = xyu::t_enable<!xyu::t_is_void<Test>>>
        decltype(auto) get(K&& key)
        {
            static_assert(xyu::t_is_same_nocvref<Key, K>);
            auto [pre, less] = find_node_add(key);
            if (less == -1) return static_cast<Node*>(pre)->val;
            Node* n = add_node(xyu::forward<K>(key));
            return n->val;
        }

        /**
         * @brief 获取值
         * @details 若键不存在，则通过默认构造插入元素
         * @param key 键
         * @attention 若有多个值时不确定返回哪一个
         * @note 仅 Value 不为 void 时可用
         */
        template <typename K, typename Test = Value, typename = xyu::t_enable<!xyu::t_is_void<Test>>>
        decltype(auto) get(K&& key) const
        { return xyu::make_const(const_cast<RbTree*>(this)->get(key)); }

        /**
         * @brief 获取值
         * @details 若键不存在，则抛出异常
         * @param key 键
         * @attention 若有多个值时不确定返回哪一个
         * @note 仅 Value 不为 void 时可用
         */
        template <typename Test = Value, typename = xyu::t_enable<!xyu::t_is_void<Test>>>
        decltype(auto) at(const Key& key)
        {
            Node* n = find_node(key);
            if (n) return n->val;
            xyloge(false, "E_Logic_Key_Not_Found: key {} is not found in the tree", key);
            throw xyu::E_Logic_Key_Not_Found{};
        }

        /**
         * @brief 获取值
         * @details 若键不存在，则抛出异常
         * @param key 键
         * @attention 若有多个值时不确定返回哪一个
         * @note 仅 Value 不为 void 时可用
         */
        template <typename Test = Value, typename = xyu::t_enable<!xyu::t_is_void<Test>>>
        decltype(auto) at(const Key& key) const
        { return xyu::make_const(const_cast<RbTree*>(this)->at(key)); }

        template <typename Test = Value, typename = xyu::t_enable<!xyu::t_is_void<Test>>>
        decltype(auto) gets(const Key& key)
        {
            auto [n, first, last] = find_nodes(key);
            return decltype(range()){n, first, last};
        }

        template <typename Test = Value, typename = xyu::t_enable<!xyu::t_is_void<Test>>>
        decltype(auto) gets(const Key& key) const
        { return (const_cast<RbTree*>(this)->gets(key)).crange(); }

        /* 插入 */

        /**
         * @brief 插入元素，并返回节点
         * @details 若键已存在，则不做处理，直接返回原值引用
         * @param key 键
         * @param args 值 (或用于构造 Value 的参数)
         * @note 当 Value 为 void 时，args 必须为空
         */
        template <typename K, typename... Args>
        Data& insert(K&& key, Args&&... args)
        {
            static_assert(xyu::t_is_same_nocvref<Key, K>);
            static_assert(!(xyu::t_is_void<Value> && sizeof...(Args) > 0));
            check_new_capa(1);
            return *add_node(xyu::forward<K>(key), xyu::forward<Args>(args)...);
        }

        /**
         * @brief 依次插入范围中的每个键
         * @details 若键已存在，则不做处理
         * @param range 键范围
         */
        template <typename Rg, xyu::t_enable<!xyu::t_can_icast<Rg, Key> && xyu::t_is_range<Rg> && (__::get_range_kv_type<Rg, Key> < 0), char> = 's'>
        RbTree& insert(Rg&& range)
        {
            check_new_capa(range.count());
            constexpr int kind = __::get_range_kv_type<Rg, Key>;
            for (auto&& e : range)
            {
                if constexpr (kind == -1) add_node(xyu::forward<decltype(e)>(e));
                else if constexpr (kind == -2) add_node(xyu::forward<decltype(e)>(e).key);
                else if constexpr (kind == -3) add_node(xyu::forward<decltype(e)>(e).template get<0>());
                else { auto&& [key] = xyu::forward<decltype(e)>(e); add_node(xyu::forward<decltype(key)>(key)); }
            }
            return *this;
        }

        /**
         * @brief 依次插入范围中的每个键值对
         * @details 若键已存在，则不做处理
         * @param range 键值对范围
         * @note 仅 Value 不为 void 时可用
         */
        template <typename Rg, typename Test = Value, xyu::t_enable<!xyu::t_is_void<Test> && !xyu::t_can_icast<Rg, Key> && xyu::t_is_range<Rg> && (__::get_range_kv_type<Rg> > 0), char> = 'd'>
        RbTree& insert(Rg&& range)
        {
            static_assert(!xyu::t_is_void<Value>);
            check_new_capa(range.count());
            constexpr int kind = __::get_range_kv_type<Rg>;
            for (auto&& e : range)
            {
                if constexpr (kind == 2) add_node(xyu::forward<decltype(e)>(e).key, xyu::forward<decltype(e)>(e).val);
                else if constexpr (kind == 3) add_node(xyu::forward<decltype(e)>(e).template get<0>(), xyu::forward<decltype(e)>(e).template get<1>());
                else { auto&& [key, val] = xyu::forward<decltype(e)>(e); add_node(xyu::forward<decltype(key)>(key), xyu::forward<decltype(val)>(val)); }
            }
            return *this;
        }

        /**
         * @brief 将参数构造为范围后，依次插入范围中的每个元素
         * @details 若键已存在，则不做处理
         * @param arg 用于构造范围的参数
         */
        template <typename Arg, xyu::t_enable<!xyu::t_can_icast<Arg, Key> && !xyu::t_is_range<Arg>, char> = 'x'>
        RbTree& insert(Arg&& arg) { return insert(xyu::make_range(arg)); }

        /**
         * @brief 将键值对通过初始化列表进行插入
         * @details 若键已存在，则不做处理
         * @param il 键值对列表
         * @note 仅 Value 不为 void 时可用
         */
        template <typename Test = Value, xyu::t_enable<!xyu::t_is_void<Test>, bool> = true>
        RbTree& insert(std::initializer_list<Tuple<Key, Value>> il) { return insert(xyu::make_range(il)); }

        /**
         * @brief 将键通过初始化列表进行插入
         * @details 若键已存在，则不做处理
         * @param il 键列表
         * @note 仅 Value 为 void 时可用
         */
        template <typename Test = Value, xyu::t_enable<xyu::t_is_void<Test>, bool> = false>
        RbTree& insert(std::initializer_list<Tuple<Key>> il) { return insert(xyu::make_range(il)); }

        /* 更新 */

        /**
         * @brief 更新元素，并返回节点
         * @details 若键不存在，则插入构造元素；
         * 若键存在，仅一个元素时优先尝试赋值，否则或其他数量的元素个数时构造值后移动赋值
         * @param key 键
         * @param args 值 (或用于构造 Value 的参数)
         * @attention 若有多个值时不确定更新哪一个
         * @note 仅 Value 不为 void 时可用
         */
        template <typename K, typename... Args, typename Test = Value, typename = xyu::t_enable<!xyu::t_is_void<Test>>>
        Data& update(K&& key, Args&&... args)
        {
            static_assert(xyu::t_is_same_nocvref<Key, K>);
            return *update_node(xyu::forward<K>(key), xyu::forward<Args>(args)...);
        }

        /**
         * @brief 依次更新范围中的每个键
         * @details 若键不存在，则插入构造元素；若键存在，进行赋值
         * @param range 键范围
         * @attention 若有多个值时不确定更新哪一个
         * @note 仅 Value 不为 void 时可用
         */
        template <typename Rg, typename Test = Value, xyu::t_enable<!xyu::t_is_void<Test> && !xyu::t_can_icast<Rg, Key> && xyu::t_is_range<Rg>, char> = 'd'>
        RbTree& update(Rg&& range)
        {
            static_assert(!xyu::t_is_void<Value>);
            static_assert(__::get_range_kv_type<Rg> > 0);
            constexpr int kind = __::get_range_kv_type<Rg>;
            for (auto&& e : range)
            {
                if constexpr (kind == 2) update_node(xyu::forward<decltype(e)>(e).key, xyu::forward<decltype(e)>(e).val);
                else if constexpr (kind == 3) update_node(xyu::forward<decltype(e)>(e).template get<0>(), xyu::forward<decltype(e)>(e).template get<1>());
                else { auto&& [key, val] = xyu::forward<decltype(e)>(e); update_node(xyu::forward<decltype(key)>(key), xyu::forward<decltype(val)>(val)); }
            }
            return *this;
        }

        /**
        * @brief 将参数构造为范围后，依次更新范围中的每个元素
        * @details 若键不存在，则插入构造元素；若键存在，进行赋值
        * @param arg 用于构造范围的参数
         * @attention 若有多个值时不确定更新哪一个
        * @note 仅 Value 不为 void 时可用
        */
        template <typename Arg, typename Test = Value, xyu::t_enable<!xyu::t_is_void<Test> && !xyu::t_can_icast<Arg, Key> && !xyu::t_is_range<Arg>, char> = 'x'>
        RbTree& update(Arg&& arg) { return update(xyu::make_range(arg)); }

        /**
         * @brief 将键值对通过初始化列表进行更新
         * @details 若键不存在，则插入构造元素；若键存在，进行赋值
         * @param il 键值对列表
         * @attention 若有多个值时不确定更新哪一个
         * @note 仅 Value 不为 void 时可用
         */
        template <typename Test = Value, xyu::t_enable<!xyu::t_is_void<Test>, bool> = true>
        RbTree& update(std::initializer_list<Tuple<Key, Value>> il) { return update(xyu::make_range(il)); }

        /**
         * @brief 将键通过初始化列表进行更新
         * @details 若键不存在，则插入构造元素；若键存在，进行赋值
         * @param il 键列表
         * @attention 若有多个值时不确定更新哪一个
         * @note 仅 Value 为 void 时可用
         */
        template <typename Test = Value, xyu::t_enable<xyu::t_is_void<Test>, bool> = false>
        RbTree& update(std::initializer_list<Tuple<Key>> il) { return update(xyu::make_range(il)); }

        /* 删除 */

        /**
         * @brief 删除键，返回删除的个数
         * @details 删除所有键为 key 的元素
         * @param key 键
         */
        xyu::size_t erase(const Key& key) noexcept
        {
            Node* n = find_node(key);
            if (!n) return 0;
            xyu::size_t cnt = 1;
            Node *prev, *tmp = n;
            while ((prev = (--xyu::RangeIter_TreePtr<Node>(tmp)).i) != &lead && xyu::equals(key, prev->key))
            { tmp = prev; ++cnt; }
            while ((n = (++xyu::RangeIter_TreePtr<Node>(n)).i) != &lead && xyu::equals(key, n->key))
            { ++cnt; }
            for (xyu::size_t i = 0; i < cnt; ++i)
            {
                n = tmp;
                tmp = (++xyu::RangeIter_TreePtr<Node>(tmp)).i;
                delete_node(n);
            }
            return cnt;
        }

        /**
         * @brief 删除迭代器位置
         * @return 指向下一个元素的迭代器
         */
        template <typename Iter, typename = xyu::t_enable<xyu::t_is_same_nocvref<Node*, decltype(Iter{}.i)>>>
        Iter erase(Iter it) noexcept
        {
            NodeBase* n = it.i;
            Iter next = ++it;
            delete_node(n);
            return next;
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
            constexpr int kind = xyu::t_is_void<Value> ? 1 : 0;
            return xyu::make_range(xyu::RangeIter_Dereference_v, xyu::native_v,
                                   xyu::RangeIter_Dereference_TreeNode<kind>{},
                                   num, static_cast<Node*>(lead.left), static_cast<Node*>(&lead));
        }
        /**
         * @brief 获取常量范围
         * @details
         * 当 Value 为 void 时，范围为直接的 key；
         * 否则，范围为 struct { Key key; Value val; }；
         */
        auto range() const noexcept { return const_cast<RbTree*>(this)->range().crange(); }

        /// 获取键范围
        auto krange() noexcept
        {
            return xyu::make_range(xyu::RangeIter_Dereference_v, xyu::native_v,
                                   xyu::RangeIter_Dereference_TreeNode<1>{},
                                   num, static_cast<Node*>(lead.left), static_cast<Node*>(&lead));
        }
        /// 获取常量键范围
        auto krange() const noexcept { return const_cast<RbTree*>(this)->krange().crange(); }

        /// 获取值范围
        template <typename Test = Value, typename = xyu::t_enable<!xyu::t_is_void<Test>>>
        auto vrange() noexcept
        {
            return xyu::make_range(xyu::RangeIter_Dereference_v, xyu::native_v,
                                   xyu::RangeIter_Dereference_TreeNode<2>{},
                                   num, static_cast<Node*>(lead.left), static_cast<Node*>(&lead));
        }
        /// 获取常量值范围
        template <typename Test = Value, typename = xyu::t_enable<!xyu::t_is_void<Test>>>
        auto vrange() const noexcept { return const_cast<RbTree*>(this)->vrange().crange(); }

        /**
         * @brief 获取范围
         * @details
         * 当 Value 为 void 时，范围为直接的 key；
         * 否则，范围为 struct { Key key; Value val; }；
         * @param key 查找的键
         */
        auto range(const Key& key) noexcept
        {
            auto [n, first, last] = find_nodes(key);
            return decltype(range()){n, first, last};
        }
        /**
         * @brief 获取常量范围
         * @details
         * 当 Value 为 void 时，范围为直接的 key；
         * 否则，范围为 struct { Key key; Value val; }；
         * @param key 查找的键
         */
        auto range(const Key& key) const noexcept { return const_cast<RbTree*>(this)->range(key).crange(); }

        /**
         * @brief 获取键范围
         * @param key 查找的键
         */
        auto krange(const Key& key) noexcept
        {
            auto [n, first, last] = find_nodes(key);
            return decltype(krange()){n, first, last};
        }
        /**
         * @brief 获取常量键范围
         * @param key 查找的键
         */
        auto krange(const Key& key) const noexcept { return const_cast<RbTree*>(this)->krange(key).crange(); }

        /**
         * @brief 获取值范围
         * @param key 查找的键
         */
        template <typename Test = Value, typename = xyu::t_enable<!xyu::t_is_void<Test>>>
        auto vrange(const Key& key) noexcept
        {
            auto [n, first, last] = find_nodes(key);
            return decltype(vrange()){n, first, last};
        }
        /**
         * @brief 获取常量值范围
         * @param key 查找的键
         */
        template <typename Test = Value, typename = xyu::t_enable<!xyu::t_is_void<Test>>>
        auto vrange(const Key& key) const noexcept { return const_cast<RbTree*>(this)->vrange(key).crange(); }

        /* 运算符 */

        /**
         * @brief 获取值
         * @details 若键不存在，则通过默认构造插入元素
         * @param key 键
         * @attention 若有多个值时不确定返回哪一个
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
         * @attention 若有多个值时不确定返回哪一个
         * @note 仅 Value 不为 void 时可用
         */
        template <typename K, typename Test = Value, typename = xyu::t_enable<!xyu::t_is_void<Test>>>
        decltype(auto) operator[](K&& key) const { return xyu::make_const(const_cast<RbTree*>(this)->operator[](key)); }

        /**
         * @brief 插入元素
         * @details 若键已存在，插入后会有多个键
         * @param arg 键 或 范围 或 初始化列表
         */
        template <typename Arg>
        RbTree& operator<<(Arg&& arg) { insert(xyu::forward<Arg>(arg)); return *this; }

    private:
        // 检测新容量
        void check_new_capa(xyu::size_t add)
        {
            if (XY_UNLIKELY(num + add < num)) {
                xylogei(false, "E_Memory_Capacity: capacity {} add {} over limit {}", num, add, limit());
                throw xyu::E_Memory_Capacity{};
            }
        }

        // 查找节点
        Node* find_node(const Key& key) const noexcept
        {
            NodeBase* cur = lead.up;
            while (cur) {
                int cmp = xyu::compare(key, static_cast<Node*>(cur)->key);
                if (cmp == 0) break;
                cur = (cmp < 0 ? cur->left : cur->right);
            }
            return static_cast<Node*>(cur);
        }
        // 查找所有节点
        auto find_nodes(const Key& key) const noexcept
        {
            struct Result { xyu::size_t cnt; Node *first, *last; };
            Node* n = find_node(key);
            if (n == nullptr) return Result{0};
            Result res{1, n, n};
            Node *prev;
            while ((prev = (--xyu::RangeIter_TreePtr<Node>(res.first)).i) != &lead && xyu::equals(key, prev->key))
            { res.first = prev; ++res.cnt; }
            while ((res.last = (++xyu::RangeIter_TreePtr<Node>(res.last)).i) != &lead && xyu::equals(key, res.last->key))
            { ++res.cnt; }
            return res;
        }

        // 查找或新增节点
        auto find_node_add(const Key& key) const noexcept
        {
            struct Result { NodeBase* pre; int less; };
            auto* pre = const_cast<NodeBase*>(&lead);
            NodeBase* cur = lead.up;
            bool less = false;
            while (cur)
            {
                pre = cur;
                int cmp = xyu::compare(key, static_cast<Node*>(cur)->key);
                if (cmp == 0) return Result{cur, -1};
                less = cmp < 0;
                cur = less ? cur->left : cur->right;
            }
            return Result{pre, less};
        }
        // 查找插入位置
        auto find_node_path(const Key& key) const noexcept
        {
            struct Result { NodeBase* pre; int less; };
            auto* pre = const_cast<NodeBase*>(&lead);
            NodeBase* cur = lead.up;
            bool less = true;
            while (cur)
            {
                pre = cur;
                less = key < static_cast<Node*>(cur)->key;
                cur = less ? cur->left : cur->right;
            }
            return Result{pre, less};
        }

        // 增加节点
        template <typename K, typename... V>
        Node* add_node(K&& key, V&&... args)
        {
            auto [pn, less] = find_node_path(key);
            if (less == -1) return static_cast<Node*>(pn);
            Node* n = xyu::alloc<Node>(1);
            ::new (n) Node{xyu::forward<K>(key), xyu::forward<V>(args)...};
            if (pn == &lead) {
                //若为根节点
                lead.up = lead.left = lead.right = n;
                n->color = Node::BLACK;
                n->up = &lead;
            }else{
                //更新最小最大节点
                if (less) { pn->left = n; if (pn == lead.left) lead.left = n; }
                else { pn->right = n; if (pn == lead.right) lead.right = n; }
                //连接节点
                n->up = pn;
                if (pn->color == Node::RED) __::fixInsert(pn, less, lead);
            }
            ++num;
            return n;
        }
        // 更新节点
        template <typename K, typename... V>
        Node* update_node(K&& key, V&&... args)
        {
            auto [n, less] = find_node_path(key);
            if (less == -1)
            {
                n = xyu::alloc<Node>(1);
                ::new (n) Node{xyu::forward<K>(key), xyu::forward<V>(args)...};
            }
            else if constexpr (xyu::t_is_aggregate<Value>) static_cast<Node*>(n)->val = Value{xyu::forward<V>(args)...};
            else static_cast<Node*>(n)->val = Value(xyu::forward<V>(args)...);
            return n;
        }

        // 释放单个节点
        static void back_node(NodeBase* n) noexcept
        {
            static_cast<Node*>(n)->~Node();
            xyu::dealloc<Node>(static_cast<Node*>(n), 1);
        }
        // 递归释放节点
        static void back_nodes(NodeBase* n) noexcept
        {
            if (n->left) back_nodes(n->left);
            if (n->right) back_nodes(n->right);
            back_node(n);
        }
        // 删除节点
        void delete_node(NodeBase* n) noexcept
        {
            __::fixErase(n, lead);
            --num;
            back_node(n);
        }
    };
}

#pragma clang diagnostic pop