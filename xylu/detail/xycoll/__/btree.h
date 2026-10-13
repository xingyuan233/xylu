#pragma once

/**
 * @file    xycoll/__/btree.h
 * @brief   B树基类
 * @date    2026-06-26
 */

#include "./tool.h"
#include "./node.h"
#include "../nearlist.h"
#include "../queue.h"
#include "../hash.h"
#include "../../xymath/func.h"
#include "../../xyrange/cmp.h"

/// B树基类
namespace xylu::xycoll::__
{
    template <typename Key, typename Val, xyu::uint32 M>
    struct BTreeMidNode;

    // B树节点基类
    template <typename Key, typename Val, xyu::uint32 M>
    struct BTreeNode
    {
        BTreeMidNode<Key, Val, M>* parent;  // 父节点
        bool is_leaf;                       // 是否为叶子节点
        xyu::uint32 count;                  // 当前节点元素数
        xyu::uint32 index;                  // 该节点在父节点 children 中的索引
        Key keys[M - 1];                    // 键
        Val vals[M - 1];                    // 值
    };

    // B树中间节点
    template <typename Key, typename Val, xyu::uint32 M>
    struct BTreeMidNode : BTreeNode<Key, Val, M>
    {
        BTreeNode<Key, Val, M>* children[M];
    };

    // B树基类
    template <typename Key, typename Val, xyu::uint32 M, bool cross_thread>
    struct BTreeBase
    {
        static_assert(M >= 4 && M <= xyu::t_number<int>::max);
        static_assert(M * (sizeof(Key) + sizeof(Val)) <= xyu::t_number<xyu::size_t>::max / 2);

        /// 借出比例 (仅限移动安全类型)
        constexpr static double K_lent_factor = 0.8;
        /// 缩容比例
        constexpr static double K_shrink_factor = 0.4;

    private:
        using Node = BTreeNode<Key, Val, M>;
        using Mid = BTreeMidNode<Key, Val, M>;
    protected:
        xyu::size_t n;  // 元素数量
        Node* root;     // 根节点
        Node* ln;       // 最左节点
        Node* rn;       // 最右节点
        xyu::size_t m;  // 节点数量

    public:
        /* 构造析构 */

        /// 默认构造
        BTreeBase(xyu::size_t n = 0, xyu::size_t m = 0, Node* root = nullptr, Node* ln = nullptr, Node* rn = nullptr) noexcept
            : n(n), root(root), ln(ln), rn(rn), m(m) {}

        /// 析构函数
        ~BTreeBase() noexcept { release(); }

        /// 移动
        void swap(BTreeBase& other) noexcept
        {
            xyu::swap(n, other.n);
            xyu::swap(root, other.root);
            xyu::swap(ln, other.ln);
            xyu::swap(rn, other.rn);
            xyu::swap(m, other.m);
        }

        /* 数据容量 */

        /// 获取最大容量
        constexpr static xyu::size_t limit() noexcept { return xyu::t_number<xyu::size_t>::max / 2; }

        /// 获取元素数量
        xyu::size_t count() const noexcept { return n; }
        /// 是否为空
        bool empty() const noexcept { return n == 0; }

        /* 数据管理 */

        /// 释放内存
        void release() noexcept
        {
            if (!root) return;
            constexpr xyu::uint32 mM = (M - 1) / 2;
            auto count = xyu::calc_log(mM, 1 + (m - 1) * (mM - 1) / 2) * (M -1) + 1;
            NearList<Node*> list(xyu::min(count, 512));
            list.append(root);
            release_tree<false>( list);
            n = m = 0;
            root = ln = rn = nullptr;
        }

    private:
        // 分配节点
        static Node* alloc_node(Mid* parent, bool is_leaf) noexcept
        {
            Node* node;
            if (is_leaf) node = Common::alloc<Node, cross_thread>();
            else node = Common::alloc<Mid, cross_thread>();
            node->parent = parent;
            node->is_leaf = is_leaf;
            return node;
        }

        // 释放节点
        static void dealloc_part_node(Node* node, xyu::uint32 key_count, xyu::uint32 val_count) noexcept
        {
            xyu::object_destroy(node->keys, key_count);
            xyu::object_destroy(node->vals, val_count);
            if (node->is_leaf) Common::dealloc<cross_thread>(node);
            else Common::dealloc<cross_thread>((Mid*)node);
        }
        static void dealloc_node(Node* node) noexcept
        { dealloc_part_node(node, node->count, node->count); }

        // 删除节点
        template <bool change_param>
        void release_tree(NearList<Node*>& list) noexcept
        {
            while (!list.empty())
            {
                Node* node = list[list.count() - 1];
                list.reduce(list.count() - 1);
                if (!node->is_leaf)
                {
                    Mid* mid = (Mid*)node;
                    for (xyu::uint32 i = 0; i <= node->count; ++i)
                        list.append(mid->children[i]);
                }
                if constexpr (change_param) n -= node->count, m -= 1;
                dealloc_node(node);
            }
        }

        // 异常检查
        template <typename... T>
        static constexpr bool Nmc() noexcept { return xyu::t_can_nothrow_mvconstr<T...>; }
        template <typename... T>
        static constexpr bool Nma() noexcept { return xyu::t_can_nothrow_mvassign<T...>; }
        template <typename... T>
        static constexpr bool Ncc() noexcept { return xyu::t_can_nothrow_cpconstr<T...>; }

        static constexpr bool NV = sizeof(Val) > 0;

        // 创建根节点
        template <bool empty, int iter = 2, typename K, typename... Args>
        auto create_root(K&& key, Args&&... args)
        {
            Node* node = alloc_node(nullptr, empty);
            if constexpr (xyu::t_can_nothrow_construct<Key, K>)
                xyu::place_init(node->keys, xyu::forward<K, xyu::t_can_nothrow_construct<Val, Args...>>(key));
            else try { xyu::place_init(node->keys, key); } catch (...) { dealloc_part_node(node, 0, 0); throw; }
            if constexpr (xyu::t_can_nothrow_construct<Val, Args...>)
                xyu::place_init(node->vals, xyu::forward<Args>(args)...);
            else try { xyu::place_init(node->vals, args...); } catch (...) { dealloc_part_node(node, 1, 0); throw; }
            node->count = 1;
            node->index = 0;
            root = node;
            if constexpr (empty)
            {
                ln = rn = node;
                n = 1;
                m = 1;
            }
            return Iter<iter>{node, 0};
        }

        // 复制到新分配的节点
        template <bool nothrow = false, typename Callback, typename ErrorHandle = void(*)()>
        Node* copy_node(Node* oldn, Callback callback, ErrorHandle ehandle = []{})
        {
            Node* newn = alloc_node(oldn->parent, oldn->is_leaf);
            newn->count = oldn->count;
            newn->index = oldn->index;
            xyu::uint32 newk = 0, newv = 0;
            if constexpr (nothrow) callback(newn, newk, newv);
            else try { callback(newn, newk, newv); }
                catch (...)
                {
                    dealloc_part_node(newn, newk, newv);
                    ehandle();
                    throw;
                }
            return newn;
        }

        // 右旋 x 个键值
        void right_rotate(Mid* parent, Node* left, Node* right, xyu::uint32 x)
        {
            xyu::uint32 index = left->index;
            xyu::uint32 lcount = left->count;
            xyu::uint32 rcount = right->count;

            // ---------- 移动键值 ----------
            auto move_kv = [=](auto* parr, auto* larr, auto* rarr)
            {
                for (xyu::int32 i = xyu::max<xyu::int32>(x - rcount, 0); i < (xyu::int32)x; ++i)
                    xyu::place_init(rarr + i + rcount, xyu::move(rarr[i + rcount - x]));
                for (xyu::int32 i = rcount - 1; i >= (xyu::int32)x; --i)
                    rarr[i] = xyu::move(rarr[i - x]);
                xyu::object_destroy(rarr, xyu::min(rcount, x));

                for (xyu::uint32 i = 0; i < x - 1; ++i)
                    xyu::place_init(rarr + i, xyu::move(larr[lcount - x + i + 1]));
                xyu::place_init(rarr + x - 1, xyu::move(parr[index]));
                parr[index] = xyu::move(larr[lcount - x]);
                xyu::object_destroy(larr + lcount - x, x);
            };

            move_kv(parent->keys, left->keys, right->keys);
            if constexpr (NV) move_kv(parent->vals, left->vals, right->vals);

            // ---------- 移动子节点 ----------
            if (!right->is_leaf)
            {
                Mid *ml = (Mid*)left, *mr = (Mid*)right;

                for (int i = rcount; i >= 0; --i)
                    mr->children[i + x] = mr->children[i];
                for (xyu::uint32 i = 0; i < x; ++i)
                    mr->children[i] = ml->children[lcount + 1 - x + i];
                for (xyu::uint32 i = 0; i <= rcount + x; ++i)
                {
                    mr->children[i]->parent = mr;
                    mr->children[i]->index  = i;
                }
            }

            // ---------- 更新元素个数 ----------
            right->count += x;
            left->count -= x;
        }

        // 左旋 x 个键值
        void left_rotate(Mid* parent, Node* left, Node* right, xyu::uint32 x)
        {
            xyu::uint32 index = left->index;
            xyu::uint32 lcount = left->count;
            xyu::uint32 rcount = right->count;

            // ---------- 移动键值 ----------
            auto move_kv = [=](auto* parr, auto* larr, auto* rarr)
            {
                xyu::place_init(larr + lcount, xyu::move(parr[index]));
                for (xyu::uint32 i = 0; i < x - 1; ++i)
                    xyu::place_init(larr + lcount + 1 + i, xyu::move(rarr[i]));

                parr[index] = xyu::move(rarr[x - 1]);

                for (xyu::uint32 i = 0; i < rcount - x; ++i)
                    rarr[i] = xyu::move(rarr[i + x]);
                xyu::object_destroy(rarr + rcount - x, x);
            };

            move_kv(parent->keys, left->keys, right->keys);
            if constexpr (NV) move_kv(parent->vals, left->vals, right->vals);

            // ---------- 移动子节点 ----------
            if (!left->is_leaf)
            {
                Mid *ml = (Mid*)left, *mr = (Mid*)right;

                for (xyu::uint32 i = 0; i < x; ++i)
                {
                    ml->children[lcount + 1 + i] = mr->children[i];
                    ml->children[lcount + 1 + i]->parent = ml;
                    ml->children[lcount + 1 + i]->index  = lcount + 1 + i;
                }
                for (xyu::uint32 i = 0; i <= rcount - x; ++i)
                {
                    mr->children[i] = mr->children[i + x];
                    mr->children[i]->index = i;
                }
            }

            // ---------- 更新元素个数 ----------
            left->count  += x;
            right->count -= x;
        }

        // 分裂节点
        Node* split_node(Node* node, xyu::uint32& ins) noexcept(Nmc<Key,Val>() && Nma<Key,Val>())
        {
            Mid* pa = node->parent;
            bool is_root = XY_UNLIKELY(pa == nullptr);

            //------ 1.借出 -------
            if constexpr (Nmc<Key,Val>() && Nma<Key,Val>())
            {
                Node *left = nullptr, *right = nullptr;
                xyu::uint32 lcount = -1, rcount = -1;
                xyu::uint32 count = node->count, index = node->index;
                if (!is_root && index > 0) lcount = (left = pa->children[index - 1])->count;
                if (!is_root && index < pa->count) rcount = (right = pa->children[index + 1])->count;

                xyu::uint32 mM = xyu::min<xyu::uint32>(2 * (M - 1) * K_lent_factor, 2 * (M - 2));
                if (!is_root && xyu::min(lcount, rcount) + count <= mM)
                {
                    //------ 1.1 左借出 ------
                    if (left && lcount <= rcount)
                    {
                        xyu::uint32 sub = (count - lcount) / 2;
                        left_rotate(pa, left, node, sub);
                        if (ins < sub) { ins = lcount + 1 + ins; return left; }
                        ins -= sub; return node;
                    }
                    //------ 1.2 右借出 ------
                    else
                    {
                        xyu::uint32 sub = (count - rcount) / 2;
                        right_rotate(pa, node, right, sub);
                        if (ins <= count - sub) return node;
                        ins = ins - count + sub - 1; return right;
                    }
                }
            }

            //------ 2.分裂 ------
            {
                // 计算上移键
                xyu::uint32 spl;
                if constexpr ((M - 1) & 1) spl = (M - 1) / 2;
                else spl = ins < M / 2 ? (M - 2) / 2 : (M - 1) / 2;

                // 分配父节点
                Mid* pa_old;
                if (is_root)
                {
                    create_root<false>(xyu::move<Nmc<Key,Val>()>(node->keys[spl]), xyu::move<Nmc<Key,Val>()>(node->vals[spl]));
                    pa = (Mid*)root;
                    pa->children[0] = node;
                }
                else if constexpr (!Nma<Key,Val>())
                {
                    pa_old = pa;
                    pa = (Mid*)copy_node(pa, [=](Node* newn, xyu::uint32& ki, xyu::uint32& vi)
                    {
                        newn->count = pa->count + 1;
                        for (; ki < node->index; ++ki) xyu::place_init(newn->keys + ki, pa->keys[ki]);
                        xyu::place_init(newn->keys + node->index, node->keys[spl]), ++ki;
                        for (; ki < newn->count; ++ki) xyu::place_init(newn->keys + ki, pa->keys[ki - 1]);
                        for (; vi < node->index; ++vi) xyu::place_init(newn->vals + vi, pa->vals[vi]);
                        xyu::place_init(newn->vals + node->index, node->vals[spl]), ++vi;
                        for (; vi < newn->count; ++vi) xyu::place_init(newn->vals + vi, pa->vals[vi - 1]);
                    });
                    for (xyu::uint32 i = 0; i <= node->index; ++i) pa->children[i] = pa_old->children[i];
                    for (xyu::uint32 i = node->index + 1; i <= pa_old->count; ++i) pa->children[i + 1] = pa_old->children[i];
                }

                // 分配新节点
                xyu::uint32 move_count = M - 1 - spl - 1;
                Node* bro = copy_node<Nmc<Key,Val>()>(node, [=](Node* newn, xyu::uint32& ki, xyu::uint32& vi)
                {
                    for (; ki < move_count; ++ki)
                        xyu::place_init(newn->keys + ki, xyu::move<Nmc<Key,Val>()>(node->keys[ki + spl + 1]));
                    for (; vi < move_count; ++vi)
                        xyu::place_init(newn->vals + vi, xyu::move<Nmc<Key,Val>()>(node->vals[vi + spl + 1]));
                }, [=,this]
                {
                    if (is_root) dealloc_node(this->root), this->root = node;
                    else if constexpr (!Nma<Key,Val>()) dealloc_node(pa);
                });
                bro->parent = pa;
                bro->count = move_count;
                bro->index = node->index + 1;
                if (rn == node) rn = bro;

                // 更新子节点
                if (!bro->is_leaf)
                {
                    auto* bn = (Mid*)bro;
                    for (xyu::uint32 i = 0; i <= move_count; ++i)
                    {
                        bn->children[i] = ((Mid*)node)->children[i + spl + 1];
                        bn->children[i]->parent = bn;
                        bn->children[i]->index = i;
                    }
                }

                // 更新父节点
                if (is_root) pa->children[bro->index] = bro, ++m;
                else
                {
                    if constexpr (Nma<Key,Val>())
                    {
                        if (node->index != pa->count)
                        {
                            xyu::place_init(pa->keys + pa->count, xyu::move(pa->keys[pa->count - 1]));
                            for (xyu::int32 i = pa->count - 1; i > (xyu::int32)node->index; --i) pa->keys[i] = xyu::move(pa->keys[i - 1]);
                            pa->keys[node->index] = xyu::move(node->keys[spl]);
                        }
                        else xyu::place_init(pa->keys + node->index, xyu::move(node->keys[spl]));

                        if constexpr (NV)
                        {
                            if (node->index != pa->count)
                            {
                                xyu::place_init(pa->vals + pa->count, xyu::move(pa->vals[pa->count - 1]));
                                for (xyu::int32 i = pa->count - 1; i > (xyu::int32)node->index; --i) pa->vals[i] = xyu::move(pa->vals[i - 1]);
                                pa->vals[node->index] = xyu::move(node->vals[spl]);
                            }
                            else xyu::place_init(pa->vals + node->index, xyu::move(node->vals[spl]));
                        }

                        for (xyu::uint32 i = pa->count; i > node->index; --i) pa->children[i + 1] = pa->children[i];
                        pa->children[bro->index] = bro;
                        ++pa->count;
                        for (xyu::uint32 i = bro->index + 1; i <= pa->count; ++i) ++pa->children[i]->index;
                    }
                    else
                    {
                        pa->children[bro->index] = bro;
                        for (xyu::uint32 i = 0; i <= pa->count; ++i)
                        {
                            pa->children[i]->parent = pa;
                            pa->children[i]->index = i;
                        }
                        if (pa->parent) pa->parent->children[pa->index] = pa;
                        else root = pa;
                        dealloc_node(pa_old);
                    }
                }

                // 更新原节点
                node->parent = pa;
                node->count = spl;
                xyu::object_destroy(node->keys + spl, node->keys + M - 1);
                xyu::object_destroy(node->vals + spl, node->vals + M - 1);
                ++m;

                // 更新插入位置
                if (ins <= spl) return node;
                ins = ins - spl - 1;
                return bro;
            }
        }

        // 插入叶子节点
        template <int iter, typename K, typename... Args>
        auto insert_leaf(Node* node, xyu::uint32 ins, K&& key, Args&&... args)
        noexcept((Nma<Key,Val>() || Ncc<Key,Val>()) && xyu::t_can_nothrow_init<Key,K> && xyu::t_can_nothrow_init<Val,Args...>)
        {
            if constexpr (Nma<Key,Val>())
            {
                bool app = ins == node->count;

                if (!app && node->count)
                {
                    xyu::place_init(node->keys + node->count, xyu::move(node->keys[node->count - 1]));
                    for (xyu::uint32 i = node->count - 1; i > ins; --i) node->keys[i] = xyu::move(node->keys[i - 1]);
                }
                if constexpr (xyu::t_can_nothrow_init<Key,K>)
                {
                    if (app) xyu::place_init(node->keys + ins, xyu::forward<K,xyu::t_can_nothrow_init<Val,Args...>>(key));
                    else if constexpr (xyu::t_can_nothrow_assign<Key, K>) node->keys[ins] = xyu::forward<K,xyu::t_can_nothrow_init<Val,Args...>>(key);
                    else node->keys[ins] = xyu::object_create<Key>(xyu::forward<K,xyu::t_can_nothrow_init<Val,Args...>>(key));
                }
                else try
                {
                    if (app) xyu::place_init(node->keys + ins, key);
                    else node->keys[ins] = xyu::object_create<Key>(key);
                }
                catch (...)
                {
                    for (xyu::uint32 i = ins; i < node->count; ++i) node->keys[i] = xyu::move(node->keys[i + 1]);
                    xyu::object_destroy(node->keys + node->count);
                    throw;
                }

                if constexpr (NV)
                {
                    if (!app && node->count)
                    {
                        xyu::place_init(node->vals + node->count, xyu::move(node->vals[node->count - 1]));
                        for (xyu::uint32 i = node->count - 1; i > ins; --i) node->vals[i] = xyu::move(node->vals[i - 1]);
                    }
                     if constexpr (xyu::t_can_nothrow_init<Val,Args...>)
                    {
                        if (app) xyu::place_init(node->vals + ins, xyu::forward<Args>(args)...);
                        else if constexpr (sizeof...(Args) == 1 && xyu::t_can_nothrow_assign<Val, Args...>) node->vals[ins] = (..., xyu::forward<Args>(args));
                        else node->vals[ins] = xyu::object_create<Val>(xyu::forward<Args>(args)...);
                    }
                    else try
                    {
                        if (app) xyu::place_init(node->vals + ins, args...);
                        else node->vals[ins] = xyu::object_create<Val>(args...);
                    }
                    catch (...)
                    {
                        for (xyu::uint32 i = ins; i < node->count; ++i) node->vals[i] = xyu::move(node->vals[i + 1]);
                        xyu::object_destroy(node->vals + node->count);
                        for (xyu::uint32 i = ins; i < node->count; ++i) node->keys[i] = xyu::move(node->keys[i + 1]);
                        xyu::object_destroy(node->keys + node->count);
                        throw;
                    }
                }

                ++node->count;
            }
            else
            {
                Node* nn = copy_node(node, [&](Node* newn, xyu::uint32& ki, xyu::uint32& vi)
                {
                    newn->count = node->count + 1;
                    for (; ki < ins; ++ki) xyu::place_init(newn->keys + ki, node->keys[ki]);
                    xyu::place_init(newn->keys + ins, key), ++ki;
                    for (; ki < newn->count; ++ki) xyu::place_init(newn->keys + ki, node->keys[ki - 1]);
                    for (; vi < ins; ++vi) xyu::place_init(newn->vals + vi, node->vals[vi]);
                    xyu::place_init(newn->vals + ins, args...), ++vi;
                    for (; vi < newn->count; ++vi) xyu::place_init(newn->vals + vi, node->vals[vi - 1]);
                });
                if (node->parent) node->parent->children[node->index] = nn;
                if (root == node) root = nn;
                if (ln == node) ln = nn;
                if (rn == node) rn = nn;
                dealloc_node(node);
                node = nn;
            }
            ++n;
            return Iter<iter>{node, ins};
        }

        // 下溢修正
        template <int mode = 0>
        void fix_underflow(Node* node, void* p = nullptr)
        {
            struct Ref { Node*& node; xyu::uint32& idx; };

            /* ---------- 处理下溢 ---------- */
            xyu::uint32 mcount = m * (M - 1) * K_shrink_factor;
            xyu::uint32 mM = n <= mcount ? (M - 1) / 2 : 1;
            while (node != root && node->count < mM)
            {
                xyu::uint32 index = node->index;
                Mid* parent = node->parent;
                xyu::uint32 count = node->count;
                Node *left = nullptr, *right = nullptr;
                xyu::uint32 lcount, rcount;

                // --- 1. 从左兄弟借 ---
                if (index > 0 && ((lcount = (left = parent->children[index - 1])->count) + count >= 2*mM))
                {
                    xyu::size_t add = (lcount - count) / 2;
                    right_rotate(parent, left, node, add);
                    if constexpr (mode == 2)
                        if ((*(Ref*)p).node == node) (*(Ref*)p).idx += add;
                    break;
                }
                // --- 2. 从右兄弟借 ---
                if (index < parent->count && ((rcount = (right = parent->children[index + 1])->count) + count >= 2*mM))
                {
                    xyu::size_t add = (rcount - count) / 2;
                    left_rotate(parent, node, right, add);
                    if constexpr (mode == 2)
                        if ((*(Ref*)p).node == right)
                        {
                            if ((*(Ref*)p).idx < add)
                            {
                                (*(Ref*)p).node = node;
                                (*(Ref*)p).idx += count + 1;
                            }
                           else  (*(Ref*)p).idx -= add;
                        }

                    break;
                }

                // --- 3. 合并兄弟 ---
                {
                    bool concat_left = left && (!right || lcount <= rcount);
                    Node* from = concat_left ? node : right;
                    Node* to = concat_left ? left : node;

                    xyu::place_init(to->keys + to->count, xyu::move(parent->keys[to->index]));
                    for (xyu::uint32 i = 0; i < from->count; ++i)
                        xyu::place_init(to->keys + to->count + i + 1, xyu::move(from->keys[i]));
                    xyu::place_init(to->vals + to->count, xyu::move(parent->vals[to->index]));
                    for (xyu::uint32 i = 0; i < from->count; ++i)
                        xyu::place_init(to->vals + to->count + i + 1, xyu::move(from->vals[i]));
                    if (!to->is_leaf)
                    {
                        for (xyu::uint32 i = 0; i <= from->count; ++i)
                        {
                            ((Mid*)to)->children[to->count + i + 1] = ((Mid*)from)->children[i];
                            ((Mid*)to)->children[to->count + i + 1]->parent = (Mid*)to;
                            ((Mid*)to)->children[to->count + i + 1]->index = to->count + 1 + i;
                        }
                    }
                    to->count += from->count + 1;

                    for (xyu::uint32 i = from->index; i < parent->count; ++i)
                        parent->keys[i - 1] = xyu::move(parent->keys[i]);
                    parent->keys[parent->count - 1].~Key();
                    if constexpr (NV) for (xyu::uint32 i = from->index; i < parent->count; ++i)
                        parent->vals[i - 1] = xyu::move(parent->vals[i]);
                    xyu::object_destroy(parent->vals + parent->count - 1);
                    for (xyu::uint32 i = from->index; i < parent->count; ++i)
                    {
                        parent->children[i] = parent->children[i + 1];
                        parent->children[i]->index = i;
                    }
                    --parent->count;

                    if constexpr (mode == 1) if (*(Node**)p == from) *(Node**)p = to;
                    if constexpr (mode == 2) if ((*(Ref*)p).node == from)
                        (*(Ref*)p).node = to, (*(Ref*)p).idx += to->count - from->count;

                    if (rn == from) rn = to;
                    dealloc_node(from);
                    --m;
                    node = parent;
                }
            }

            /* ---------- 根节点缩并 ---------- */
            if (root->count == 0)
            {
                if (root->is_leaf)
                {
                    dealloc_node(root);
                    root = ln = rn = nullptr;
                    m = 0;
                    if constexpr (mode == 2) (*(Ref*)p).node = nullptr;
                }
                else
                {
                    root = ((Mid*)root)->children[0];
                    dealloc_node(root->parent);
                    root->parent = nullptr;
                    root->index = 0;
                    --m;
                }
                if constexpr (mode == 1) *(Node**)p = root;
            }
        }

    protected:
        struct Location { Node* node; bool find; xyu::uint32 idx; };

        // 查找键 (0:表示任意匹配，-1:第一个匹配，1:最后一个匹配)
        template <int kind = 0, typename K>
        void find_key(const K& key, Node*& node, xyu::uint32& idx, xyu::size_t& count) noexcept
        {
            Storage<0> iter{node, idx};
            for (;;)
            {
                if constexpr (kind == -1)
                {
                    Decrement{}(iter);
                    if (XY_UNLIKELY(iter.p == ln && iter.index == -1)) break;
                }
                else if constexpr (kind == 1)
                {
                    Increment{}(iter);
                    if (XY_UNLIKELY(iter.p == rn && iter.index == rn->count)) break;
                }
                else break;

                if (xyu::compare(iter.p->keys[iter.index], key) != 0) break;
                node = iter.p;
                idx = iter.index;
                ++count;
            }
        }

        // locate
        template <bool need_split = true, int kind = 0, typename K>
        Location locate(const K& key, Node* start = nullptr)
        {
            if constexpr (!need_split) if (XY_UNLIKELY(!root))
                return {nullptr, false};

            Node* node = start ? start : root;
            xyu::uint32 idx;
            for (;;)
            {
                if constexpr (xyu::t_is_number<Key> && M <= 8)
                {
                    idx = 0;
                    while (idx < node->count)
                    {
                        int r = xyu::compare(node->keys[idx], key);
                        if (r == 0) goto fix_pos;
                        if (r > 0) break;
                        ++idx;
                    }
                }
                else
                {
                    xyu::uint32 left = 0, right = node->count;
                    while (left < right)
                    {
                        idx = (left + right) / 2;
                        int r = xyu::compare(node->keys[idx], key);
                        if (r == 0) goto fix_pos;
                        if (r > 0) right = idx;
                        else left = idx + 1;
                    }
                    idx = left;
                }

                if constexpr (need_split)
                    if (node->count == M - 1) node = split_node(node, idx);

                if (node->is_leaf) break;
                node = ((Mid*)node)->children[idx];
            }
            if constexpr (need_split) return {node, false, idx};
            else return {nullptr, false};

        fix_pos:
            xyu::size_t tmp;
            find_key<kind>(key, node, idx, tmp);
            return {node, true, idx};
        }
        
        // select 辅助
        template <typename K>
        static constexpr bool select_impl_noexcept() noexcept
        {
            return xyu::t_can_compare<Key,K> || xyu::t_can_nothrow_init<Key,K>;
        }
        template <int kind = 0, int iter = 0, typename K>
        auto select_impl(K&& key) noexcept(select_impl_noexcept<K>())
        {
            if constexpr (!xyu::t_can_compare<Key, K>)
                return select_impl<kind>(xyu::object_create<Key>(xyu::forward<K>(key)));
            else
            {
                Location location = locate<false, kind>(key);
                return Iter<iter>{location.node, location.idx};
            }
        }

        // select_all 辅助
        template <int iter = 2, typename K>
        auto select_all_impl(K&& key) noexcept(select_impl_noexcept<K>())
        {
            if constexpr (!xyu::t_can_compare<Key, K>)
                return select_all_impl<2>(xyu::object_create<Key>(xyu::forward<K>(key)));
            else
            {
                struct Ret { xyu::size_t count; Iter<iter> start; Iter<iter> end; };

                Location location = locate<false, 0>(key);
                if (!location.find) return xyu::Range<Iter<2>>{0, {}, {}};

                xyu::size_t count = 1;
                Iter<iter> start = {location.node, location.idx}, end = {location.node, location.idx};
                find_key<-1>(key, start.p, start.index, count);
                find_key<1>(key, end.p, end.index, count);
                ++end;
                return xyu::Range<Iter<iter>>{count, start, end};
            }
        }
        
        // append 辅助
        template <typename K, typename... Args>
        static constexpr bool append_impl_noexcept() noexcept
        {
            return Nmc<Key,Val>() && (Nma<Key,Val>() || Ncc<Key,Val>()) &&
                   xyu::t_can_nothrow_init<Key,K> && xyu::t_can_nothrow_init<Val,Args...>;
        }
        template <int iter = 2, typename K, typename... Args, bool nothrow = append_impl_noexcept<K,Args...>()>
        auto append_impl(K&& key, Args&&... args) noexcept(nothrow)
        {
            if constexpr (!xyu::t_can_compare<Key, K>)
                return append_impl(xyu::object_create<Key>(xyu::forward<K,nothrow>(key)), xyu::forward<Args>(args)...);
            else if (XY_UNLIKELY(!root))
                return create_root<true, iter>(xyu::forward<K>(key), xyu::forward<Args>(args)...);
            else
            {
                Node* node = root;
                xyu::uint32 ins;
                for (;;)
                {
                    if constexpr (xyu::t_is_number<Key> && M <= 8)
                    {
                        ins = 0;
                        while (ins < node->count && xyu::compare(node->keys[ins], key) <= 0) ++ins;
                    }
                    else
                    {
                        xyu::uint32 left = 0, right = node->count;
                        while (left < right)
                        {
                            xyu::uint32 mid = (left + right) / 2;
                            if (xyu::compare(node->keys[mid], key) > 0) right = mid;
                            else left = mid + 1;
                        }
                        ins = left;
                    }

                    if (node->count == M - 1) node = split_node(node, ins);

                    if (node->is_leaf) break;
                    node = ((Mid*)node)->children[ins];
                }
                return insert_leaf<iter>(node, ins, xyu::forward<K>(key), xyu::forward<Args>(args)...);
            }
        }

        // prepend 辅助
        template <int iter = 2, typename K, typename... Args, bool nothrow = append_impl_noexcept<K, Args...>()>
        auto prepend_impl(K&& key, Args&&... args) noexcept(nothrow)
        {
            if constexpr (!xyu::t_can_compare<Key, K>)
                return prepend_impl(xyu::object_create<Key>(xyu::forward<K, nothrow>(key)), xyu::forward<Args>(args)...);
            else if (XY_UNLIKELY(!root))
                return create_root<true, iter>(xyu::forward<K>(key), xyu::forward<Args>(args)...);
            else
            {
                Node* node = root;
                xyu::uint32 ins;
                for (;;)
                {
                    if constexpr (xyu::t_is_number<Key> && M <= 8)
                    {
                        ins = 0;
                        while (ins < node->count && xyu::compare(node->keys[ins], key) < 0) ++ins;
                    }
                    else
                    {
                        xyu::uint32 left = 0, right = node->count;
                        while (left < right)
                        {
                            xyu::uint32 mid = (left + right) / 2;
                            if (xyu::compare(node->keys[mid], key) >= 0) right = mid;
                            else left = mid + 1;
                        }
                        ins = left;
                    }

                    if (node->count == M - 1) node = split_node(node, ins);

                    if (node->is_leaf) break;
                    node = ((Mid*)node)->children[ins];
                }
                return insert_leaf<iter>(node, ins, xyu::forward<K>(key), xyu::forward<Args>(args)...);
            }
        }

        // insert 辅助
        template <int kind = 0, int iter = 2, typename K, typename... Args, bool nothrow = append_impl_noexcept<K,Args...>()>
        auto insert_impl(K&& key, Args&&... args) noexcept(nothrow)
        {
            if constexpr (!xyu::t_can_compare<Key, K>)
                return insert_impl<kind>(xyu::object_create<Key>(xyu::forward<K,nothrow>(key)), xyu::forward<Args>(args)...);
            else if (XY_UNLIKELY(!root))
                return create_root<true, iter>(xyu::forward<K>(key), xyu::forward<Args>(args)...);
            else
            {
                Location location = locate<true, kind>(key);
                if (location.find) return Iter<iter>{location.node, location.idx};
                return insert_leaf<iter>(location.node, location.idx, xyu::forward<K>(key), xyu::forward<Args>(args)...);
            }
        }

        // insert iter 辅助
        template <typename It, typename K, typename... Args, bool nothrow = append_impl_noexcept<K,Args...>()>
        auto insert_iter_impl(It it, K&& key, Args&&... args) noexcept(nothrow)
        {
            constexpr int iter = It::Kind_t::value;

            if (XY_UNLIKELY(!root))
                return create_root<true, iter>(xyu::forward<K>(key), xyu::forward<Args>(args)...);
            if constexpr (!XY_RELEASE) if (XY_UNLIKELY(!it || (it.index >= it.p->count && it.p != rn)))
                xyloge3("can't pass invalid iter for insert"), xyu::abort();
            if constexpr (XY_DEBUG)
            {
                if (XY_LIKELY(it.index < it.p->count))
                {
                    if (XY_UNLIKELY(xyu::compare(it.p->keys[it.index], xyu::object_create<Key>(key)) < 0))
                        xyloge3("key is out of order"), xyu::abort();
                }
                if (XY_LIKELY(it.p != ln || it.index > 0))
                {
                    It prev = it; --prev;
                    if (XY_UNLIKELY(xyu::compare(prev.p->keys[prev.index], xyu::object_create<Key>(key)) > 0))
                        xyloge3("key is out of order"), xyu::abort();
                }
            }

            Node* node = it.p;
            xyu::uint32 ins = it.index;
            for (;;)
            {
                if (node->count == M - 1) node = split_node(node, ins);
                if (node->is_leaf) break;
                node = ((Mid*)node)->children[ins];
                ins = node->count;
            }
            return insert_leaf<iter>(node, ins, xyu::forward<K>(key), xyu::forward<Args>(args)...);
        }

        // update 辅助
        template <typename K, typename... Args>
        static constexpr bool update_impl_noexcept() noexcept
        {
            return append_impl_noexcept<K, Args...>() &&
                ((sizeof...(Args) == 1 && xyu::t_can_assign<Val, Args...>) ?
                  xyu::t_can_nothrow_assign<Val, Args...> : Nma<Val>());
        }
        template <int kind = 0, int iter = 2, typename K, typename... Args, bool nothrow = update_impl_noexcept<K,Args...>()>
        auto update_impl(K&& key, Args&&... args) noexcept(nothrow)
        {
            if constexpr (!xyu::t_can_compare<Key, K>)
                return update_impl<kind>(xyu::object_create<Key>(xyu::forward<K,nothrow>(key)), xyu::forward<Args>(args)...);
            else if (XY_UNLIKELY(!root))
                return create_root<true, iter>(xyu::forward<K>(key), xyu::forward<Args>(args)...);
            else
            {
                Location location = locate<true, kind>(key);
                if (!location.find) return insert_leaf<iter>(location.node, location.idx, xyu::forward<K>(key), xyu::forward<Args>(args)...);
                auto& val = location.node->vals[location.idx];
                if constexpr (sizeof...(Args) == 1 && xyu::t_can_assign<Val, Args...>)
                    val = (..., xyu::forward<Args, xyu::t_can_nothrow_assign<Val, Args...>>(args));
                else
                    val = xyu::object_create<Val>(xyu::forward<Args,nothrow>(args)...);
                return Iter<iter>{location.node, location.idx};
            }
        }

        // erase 辅助
        template <bool iter = false>
        void erase_help(Node*& node_ref, xyu::uint32& idx_ref)
        {
            Node* node = node_ref;
            xyu::uint32 idx = idx_ref;

            if constexpr (Nma<Key, Val>() && Nmc<Key, Val>())
            {
                /* ---------- 内部节点：叶子前驱替换 ---------- */
                if (!node->is_leaf)
                {
                    // 找到前驱
                    Node* pred = ((Mid*)node)->children[idx];
                    while (!pred->is_leaf) pred = ((Mid*)pred)->children[pred->count];

                    if constexpr (iter)
                    {
                        Node* next = ((Mid*)node)->children[idx + 1];
                        while (!next->is_leaf) next = ((Mid*)next)->children[0];
                        node_ref = next;
                        idx_ref = 0;
                    }

                    xyu::uint32 last = pred->count - 1;
                    node->keys[idx] = xyu::move(pred->keys[last]);
                    node->vals[idx] = xyu::move(pred->vals[last]);

                    node = pred;
                    idx  = last;
                }

                /* ---------- 删除元素 ---------- */
                {
                    for (xyu::uint32 i = idx + 1; i < node->count; ++i)
                        node->keys[i - 1] = xyu::move(node->keys[i]);
                    node->keys[node->count - 1].~Key();
                    for (xyu::uint32 i = idx + 1; i < node->count; ++i)
                        node->vals[i - 1] = xyu::move(node->vals[i]);
                    node->vals[node->count - 1].~Val();
                    --node->count;
                    --n;
                }

                /* ---------- 处理下溢 ---------- */
                if constexpr (iter)
                {
                    struct { Node*& node; xyu::uint32& idx; } ref{node_ref, idx_ref};
                    fix_underflow<2>(node, &ref);
                }
                else fix_underflow(node);
            }
            else
            {
                // 存储临时节点，等所有操作成功再进行整体替换
                NearList<Node*> list{8};

                try
                {
                    /* ---------- 内部节点：前驱替换 ---------- */
                    Node* ori_node = nullptr;
                    if (!node->is_leaf)
                    {
                        ori_node = node;

                        // 找到前驱
                        Node* pred = ((Mid*)node)->children[idx];
                        while (!pred->is_leaf) pred = ((Mid*)pred)->children[pred->count];

                        xyu::uint32 last = pred->count - 1;
                        list.append(copy_node(node, [=](Node* newn, xyu::uint32& ki, xyu::uint32& vi)
                        {
                            for (; ki < idx; ++ki) xyu::place_init(newn->keys + ki, node->keys[ki]);
                            xyu::place_init(newn->keys + idx, pred->keys[last]), ++ki;
                            for (; ki < node->count; ++ki) xyu::place_init(newn->keys + ki, node->keys[ki]);
                            for (; vi < idx; ++vi) xyu::place_init(newn->vals + vi, node->vals[vi]);
                            xyu::place_init(newn->vals + idx, pred->vals[last]), ++vi;
                            for (; vi < node->count; ++vi) xyu::place_init(newn->vals + vi, node->vals[vi]);
                        }));

                        if constexpr (iter)
                        {
                            Node* next = ((Mid*)node)->children[idx + 1];
                            while (!next->is_leaf) next = ((Mid*)next)->children[0];
                            node_ref = next;
                            idx_ref = 0;
                        }

                        node = pred;
                        idx  = last;
                    }
                    else list.append(nullptr);

                    /* ---------- 叶子节点：删除 ---------- */
                    list.append(copy_node(node, [=](Node* newn, xyu::uint32& ki, xyu::uint32& vi)
                    {
                        newn->count = node->count - 1;
                        for (; ki < idx; ++ki) xyu::place_init(newn->keys + ki, node->keys[ki]);
                        for (; ki < newn->count; ++ki) xyu::place_init(newn->keys + ki, node->keys[ki + 1]);
                        for (; vi < idx; ++vi) xyu::place_init(newn->vals + vi, node->vals[vi]);
                        for (; vi < newn->count; ++vi) xyu::place_init(newn->vals + vi, node->vals[vi + 1]);
                    }));
                    if constexpr (iter) if (node_ref == node) node_ref = list[list.count() - 1];

                    /* ---------- 处理下溢 ---------- */
                    xyu::uint32 mcount = m * (M - 1) * K_shrink_factor;
                    xyu::uint32 mM = n <= mcount ? (M - 1) / 2 : 1;
                    for (;;)
                    {
                        node = list[list.count() - 1];
                        Mid* parent = node->parent;
                        if (!parent || node->count >= mM) break;
                        xyu::uint32 index = node->index;
                        xyu::uint32 count = node->count;
                        bool need_fix = parent == ori_node;
                        Node *left = nullptr, *right = nullptr;
                        xyu::uint32 lcount, rcount;

                        auto front_insert = [index, count, &lcount]
                        (xyu::uint32 add, auto* arr, auto* larr, auto* parr, xyu::uint32& sc, xyu::uint32& nc)
                        {
                            nc = count;
                            sc = 0;
                            for (xyu::uint32 i = xyu::max(add, count); i < add + count; ++i)
                                xyu::place_init(arr + i, xyu::move(arr[i - add])), ++sc;
                            for (int i = count - 1; i >= (int)add; --i)
                                arr[i] = xyu::move(arr[i - add]), ++sc, --nc;
                            xyu::object_destroy(arr, xyu::min(count, add));
                            for (nc = 0; nc < add - 1; ++nc)
                                xyu::place_init(arr + nc, larr[lcount - add + nc + 1]);
                            xyu::place_init(arr + add - 1, parr[index - 1]);
                            nc = count + add;
                            sc = 0;
                        };

                        auto back_insert = [index, count]
                        (xyu::uint32 add, auto* arr, auto* rarr, auto* parr, xyu::uint32& nc)
                        {
                            nc = count;
                            xyu::place_init(arr + count, parr[index]), ++nc;
                            for (xyu::uint32 i = 0; i < add - 1; ++i)
                                xyu::place_init(arr + count + 1 + i, rarr[i]), ++nc;
                        };

                        // --- 1. 从左兄弟借 ---
                        if (index > 0 && ((lcount = (left = parent->children[index - 1])->count) + count >= 2*mM))
                        {
                            if (need_fix) parent = (Mid*)list[0], node->parent = parent;

                            xyu::uint32 add = (lcount - count) / 2;
                            xyu::uint32 nk = count, nv = count;
                            xyu::uint32 sk = 0, sv = 0;
                            try
                            {
                                front_insert(add, node->keys, left->keys, parent->keys, sk, nk);
                                if constexpr (NV) front_insert(add, node->vals, left->vals, parent->vals, sv, nv);
                            }
                            catch (...)
                            {
                                xyu::uint32 sinit = xyu::max(add, count);
                                xyu::uint32 ninit = xyu::min(add, count);
                                if (sk <= ninit) xyu::object_destroy(node->keys + sinit, sk);
                                else xyu::object_destroy(node->keys + count + add - sk, sk);
                                if (sv <= ninit) xyu::object_destroy(node->vals + sinit, sv);
                                else xyu::object_destroy(node->vals + count + add - sv, sv);
                                dealloc_part_node(node, nk, nv);
                                list.reduce(list.count() - 1);
                                throw;
                            }
                            node->count += add;
                            if constexpr (iter) if (node_ref == node) idx_ref += add;

                            list.append(copy_node(left, [=](Node* newn, xyu::uint32& ki, xyu::uint32& vi)
                            {
                                newn->parent = parent;
                                newn->count = lcount - add;
                                for (; ki < newn->count; ++ki) newn->keys[ki] = left->keys[ki];
                                if constexpr (NV) for (; vi < newn->count; ++vi) newn->vals[vi] = left->vals[vi];
                            }));

                            list.append(copy_node(parent, [=](Node* newn, xyu::uint32& ki, xyu::uint32& vi)
                            {
                                for (; ki < index - 1; ++ki) newn->keys[ki] = parent->keys[ki];
                                newn->keys[ki] = left->keys[lcount - add], ++ki;
                                for (; ki < parent->count; ++ki) newn->keys[ki] = parent->keys[ki];

                                if constexpr (NV)
                                {
                                    for (; vi < index - 1; ++vi) newn->vals[vi] = parent->vals[vi];
                                    newn->vals[vi] = left->vals[lcount - add], ++vi;
                                    for (; vi < parent->count; ++vi) newn->vals[vi] = parent->vals[vi];
                                }
                            }));

                            break;
                        }

                        // --- 2. 从右兄弟借 ---
                        if (index < parent->count && ((rcount = (right = parent->children[index + 1])->count) + count >= 2*mM))
                        {
                            if (need_fix) parent = (Mid*)list[0], node->parent = parent;

                            xyu::uint32 add = (rcount - count) / 2;
                            xyu::uint32 nk = count, nv = count;
                            try
                            {
                                back_insert(add, node->keys, right->keys, parent->keys, nk);
                                back_insert(add, node->vals, right->vals, parent->vals, nv);
                            }
                            catch (...)
                            {
                                dealloc_part_node(node, nk, nv);
                                list.reduce(list.count() - 1);
                                throw;
                            }
                            node->count += add;

                            list.append(copy_node(right, [=](Node* newn, xyu::uint32& ki, xyu::uint32& vi)
                            {
                                newn->parent = parent;
                                newn->count = rcount - add;
                                for (; ki < newn->count; ++ki) xyu::place_init(newn->keys + ki, right->keys[ki + add]);
                                for (; vi < newn->count; ++vi) xyu::place_init(newn->vals + vi, right->vals[vi + add]);
                            }));
                            if constexpr (iter)
                                if (node_ref == right)
                                {
                                    if (idx_ref < add)
                                    {
                                        node_ref = node;
                                        idx_ref += count + 1;
                                    }
                                    else
                                    {
                                        node_ref = list[list.count() - 1];
                                        idx_ref -= add;
                                    }
                                }

                            list.append(copy_node(parent, [=](Node* newn, xyu::uint32& ki, xyu::uint32& vi)
                            {
                                for (; ki < index; ++ki) xyu::place_init(newn->keys + ki, parent->keys[ki]);
                                xyu::place_init(newn->keys + ki, right->keys[add - 1]), ++ki;
                                for (; ki < parent->count; ++ki) xyu::place_init(newn->keys + ki, parent->keys[ki]);

                                for (; vi < index; ++vi) xyu::place_init(newn->vals + vi, parent->vals[vi]);
                                xyu::place_init(newn->vals + vi, right->vals[add - 1]), ++vi;
                                for (; vi < parent->count; ++vi) xyu::place_init(newn->vals + vi, parent->vals[vi]);
                            }));

                            break;
                        }

                        // --- 3. 合并兄弟 ---
                        {
                            if (need_fix) parent = (Mid*)list[0], node->parent = parent;
                            bool concat_left = left && (!right || lcount <= rcount);

                            if (concat_left)
                            {
                                xyu::uint32 add = lcount + 1;
                                xyu::uint32 nk = count, nv = count;
                                xyu::uint32 sk = 0, sv = 0;
                                try
                                {
                                    front_insert(add, node->keys, left->keys, parent->keys, sk, nk);
                                    if constexpr (NV) front_insert(add, node->vals, left->vals, parent->vals, sv, nv);
                                }
                                catch (...)
                                {
                                    xyu::uint32 sinit = xyu::max(add, count);
                                    xyu::uint32 ninit = xyu::min(add, count);
                                    if (sk <= ninit) xyu::object_destroy(node->keys + sinit, sk);
                                    else xyu::object_destroy(node->keys + count + add - sk, sk);
                                    if (sv <= ninit) xyu::object_destroy(node->vals + sinit, sv);
                                    else xyu::object_destroy(node->vals + count + add - sv, sv);
                                    dealloc_part_node(node, nk, nv);
                                    list.reduce(list.count() - 1);
                                    throw;
                                }
                                node->count += add;
                                node->index = index - 1;

                                if constexpr (iter) if (node_ref == node) idx_ref += add;
                            }
                            else
                            {
                                xyu::uint32 add = rcount + 1;
                                xyu::uint32 nk = count, nv = count;
                                try
                                {
                                    back_insert(add, node->keys, right->keys, parent->keys, nk);
                                    back_insert(add, node->vals, right->vals, parent->vals, nv);
                                }
                                catch (...)
                                {
                                    dealloc_part_node(node, nk, nv);
                                    list.reduce(list.count() - 1);
                                    throw;
                                }
                                node->count += add;
                            }
                            list.append(nullptr);

                            xyu::uint32 del_idx = concat_left ? (index - 1) : index;
                            list.append(copy_node(parent, [=](Node* newn, xyu::uint32& ki, xyu::uint32& vi)
                            {
                                newn->count = parent->count - 1;
                                for (; ki < del_idx; ++ki)
                                    xyu::place_init(newn->keys + ki, parent->keys[ki]);
                                for (; ki < newn->count; ++ki)
                                    xyu::place_init(newn->keys + ki, parent->keys[ki + 1]);

                                for (; vi < del_idx; ++vi)
                                    xyu::place_init(newn->vals + vi, parent->vals[vi]);
                                for (; vi < newn->count; ++vi)
                                    xyu::place_init(newn->vals + vi, parent->vals[vi + 1]);
                            }));
                        }

                        if (need_fix) ori_node = nullptr;
                    }

                    /* ---------- 应用新节点 ---------- */
                    auto apply_newn = [=](Node* newn, bool copy_child)
                    {
                        Node* oldn = newn->parent->children[newn->index];
                        if (copy_child && !newn->is_leaf)
                        {
                            for (xyu::uint32 i = 0; i <= newn->count; ++i)
                            {
                                ((Mid*)newn)->children[i] = ((Mid*)oldn)->children[i];
                                ((Mid*)newn)->children[i]->parent = (Mid*)newn;
                            }
                        }
                        dealloc_node(oldn);
                        newn->parent->children[newn->index] = newn;
                    };

                    auto apply_root = [this](Node* newn)
                    {
                        if (!newn->is_leaf) for (xyu::uint32 i = 0; i <= newn->count; ++i)
                        {
                            ((Mid*)newn)->children[i] = ((Mid*)this->root)->children[i];
                            ((Mid*)newn)->children[i]->parent = (Mid*)newn;
                        }
                        if (this->rn == this->root) this->ln = this->rn = newn;
                        dealloc_node(this->root);
                        this->root = newn;
                    };

                    if (list[0]) list[0]->parent ? apply_newn(list[0], true) : apply_root(list[0]);
                    for (xyu::size_t i = 1; i < list.count() - 1; i += 2)
                    {
                        Node *newn = list[i], *newb = list[i + 1];

                        // 借用
                        if (newb)
                        {
                            Node* oldn = newn->parent->children[newn->index];
                            Node* oldb = newb->parent->children[newb->index];
                            xyu::uint32 count = newn->count - oldn->count;

                            // 借用左兄弟
                            if (newn->index > newb->index)
                            {
                                if (!newn->is_leaf)
                                {
                                    for (xyu::uint32 j = 0; j < count; ++j)
                                        ((Mid*)newn)->children[j] = ((Mid*)oldb)->children[oldb->count + 1 - count + j];
                                    for (xyu::uint32 j = 0; j <= oldn->count; ++j)
                                        ((Mid*)newn)->children[j + count] = ((Mid*)oldn)->children[j];
                                    for (xyu::uint32 j = 0; j <= newn->count; ++j)
                                    {
                                        ((Mid*)newn)->children[j]->parent = (Mid*)newn;
                                        ((Mid*)newn)->children[j]->index = j;
                                    }
                                }
                                if (oldb == ln) ln = newb;
                                if (oldn == rn) rn = newn;
                                apply_newn(newb, true);
                            }
                            // 借用右兄弟
                            else
                            {
                                if (!newn->is_leaf)
                                {
                                    for (xyu::uint32 j = 0; j <= oldn->count; ++j)
                                        ((Mid*)newn)->children[j] = ((Mid*)oldn)->children[j];
                                    for (xyu::uint32 j = 0; j < count; ++j)
                                        ((Mid*)newn)->children[oldn->count + 1 + j] = ((Mid*)oldb)->children[j];
                                    for (xyu::uint32 j = 0; j <= newn->count; ++j)
                                    {
                                        ((Mid*)newn)->children[j]->parent = (Mid*)newn;
                                        ((Mid*)newn)->children[j]->index = j;
                                    }
                                    for (xyu::uint32 j = 0; j <= newb->count; ++j)
                                    {
                                        ((Mid*)newb)->children[j] = ((Mid*)oldb)->children[j + count];
                                        ((Mid*)newb)->children[j]->parent = (Mid*)newb;
                                        ((Mid*)newb)->children[j]->index = j;
                                    }
                                }
                                if (oldn == ln) ln = newn;
                                if (oldb == rn) rn = newb;
                                apply_newn(newb, false);
                            }
                            apply_newn(newn, false);
                        }

                        // 合并
                        else
                        {
                            Mid* pa = newn->parent;
                            Node* old_to = pa->children[newn->index];
                            Node* old_from = pa->children[newn->index + 1];

                            if (!newn->is_leaf)
                            {
                                for (xyu::uint32 j = 0; j <= old_to->count; ++j)
                                    ((Mid*)newn)->children[j] = ((Mid*)old_to)->children[j];
                                for (xyu::uint32 j = 0; j <= old_from->count; ++j)
                                    ((Mid*)newn)->children[old_to->count + 1 + j] = ((Mid*)old_from)->children[j];
                                for (xyu::uint32 j = 0; j <= newn->count; ++j)
                                {
                                    ((Mid*)newn)->children[j]->parent = (Mid*)newn;
                                    ((Mid*)newn)->children[j]->index = j;
                                }
                            }

                            if (old_to == ln) ln = newn;
                            if (old_from == rn) rn = newn;

                            dealloc_node(old_from);
                            apply_newn(newn, false);

                            for (xyu::uint32 j = newn->index + 1; j < pa->count; ++j)
                            {
                                pa->children[j] = pa->children[j + 1];
                                pa->children[j]->index = j;
                            }
                            --pa->count;
                            --m;
                        }
                    }
                    {
                        node = list[list.count() - 1];

                        /* ---------- 根节点缩并 ---------- */
                        if (node->parent)
                        {
                            Node* oldn = node->parent->children[node->index];
                            if (node->is_leaf)
                            {
                                if (ln == oldn) ln = node;
                                if (rn == oldn) rn = node;
                            }
                            if (node->parent == ori_node) node->parent = (Mid*)list[0];
                            apply_newn(node, true);
                        }
                        else if (node->count == 0)
                        {
                            dealloc_node(node);
                            node = root;
                            if (node->is_leaf)
                            {
                                root = ln = rn = nullptr;
                                m = 0;
                                if constexpr (iter) node_ref = nullptr;
                            }
                            else
                            {
                                root = ((Mid*)root)->children[0];
                                root->parent = nullptr;
                                root->index = 0;
                                --m;
                            }
                            dealloc_node(node);
                        }
                        else apply_root(node);
                    }

                    --n;
                }
                catch (...)
                {
                    for (auto newn : list.range()) if (newn) dealloc_node(newn);
                    throw;
                }
            }
        }

        // erase 辅助
        template <int kind = 0, typename K, bool nothrow = Nma<Key, Val>() && Nmc<Key, Val>() && select_impl_noexcept<K>()>
        bool erase_impl(K&& key) noexcept(nothrow)
        {
            if constexpr (!xyu::t_can_compare<Key, K>)
                return erase_impl<kind>(xyu::object_create<Key>(xyu::forward<K,nothrow>(key)));
            else
            {
                Location location = locate<false, kind>(key);
                if (!location.find) return false;
                Node* node = location.node;
                xyu::uint32 idx = location.idx;
                erase_help(node, idx);
                return true;
            }
        }

        // erase iter 辅助
        template <typename It>
        It erase_iter_impl(It it) noexcept(Nma<Key, Val>() && Nmc<Key, Val>())
        {
            if constexpr (!XY_RELEASE) if (XY_UNLIKELY(!it || (it.index >= it.p->count && n > 0)))
                xyloge3("can't pass invalid iter for erase"), xyu::abort();
            Node* node = it.p;
            xyu::uint32 ins = it.index;
            erase_help<true>(node, ins);
            return It{node, ins};
        }

        // erase_all 辅助
        template <typename K, bool nothrow = Nma<Key, Val>() && Nmc<Key, Val>() && select_impl_noexcept<K>()>
        xyu::size_t erase_all_impl(K&& key) noexcept(nothrow)
        {
            if constexpr (!xyu::t_can_compare<Key, K>)
                return erase_all_impl(xyu::object_create<Key>(xyu::forward<K,nothrow>(key)));
            else if constexpr (Nma<Key, Val>() && Nmc<Key, Val>())
            {
                xyu::size_t total = 0;
                Node* high = root;
                NearList<Node*> list;

                for (;;)
                {
                    /* ---------- 查找最顶层的匹配键 ---------- */
                    Location location = locate<false, 0>(key, high);
                    if (!location.find) return total;

                    Node* node = location.node;
                    xyu::uint32 idx = location.idx;
                    xyu::uint32 count = 1;

                    high = node;

                    /* ---------- 内部节点处理 ---------- */
                    if (!node->is_leaf)
                    {
                        // 找到相同键范围
                        xyu::uint32 start = idx, end = idx + 1;
                        while (start > 0 && xyu::compare(node->keys[start - 1], key) == 0) --start;
                        while (end < node->count && xyu::compare(node->keys[end], key) == 0) ++end;

                        auto remove_inner = [this, &list, &total]
                        (Mid* node, xyu::uint32 s, xyu::uint32 e)
                        {
                            // 递归删除中间的子节点
                            xyu::uint32 mcount = e - s;
                            list.reserve(mcount);
                            for (xyu::uint32 i = s; i < e; ++i)
                                list.append(node->children[i]);
                            xyu::size_t tmp = this->n;
                            release_tree<true>(list);
                            total += tmp - this->n;

                            // 删除中间键
                            for (xyu::uint32 i = e; i < node->count; ++i)
                                node->keys[i - mcount] = xyu::move(node->keys[i]);
                            for (xyu::uint32 i = e; i < node->count; ++i)
                                node->vals[i - mcount] = xyu::move(node->vals[i]);
                            n -= mcount;
                            total += mcount;

                            // 移动剩余子节点
                            for (xyu::uint32 i = e; i <= node->count; ++i)
                                node->children[i - mcount] = node->children[i];
                            node->count -= mcount;
                            for (xyu::uint32 i = s; i <= node->count; ++i)
                                node->children[i]->index = i;
                        };

                        if (start + 1 < end) remove_inner((Mid*)node, start + 1, end);

                        // 寻找前驱
                        Node* pred = ((Mid*)node)->children[start];
                        while (!pred->is_leaf)
                        {
                            xyu::uint32 index = pred->count;
                            xyu::int32 rend = pred->count - 1;
                            while (rend >= 0 && xyu::compare(pred->keys[rend], key) == 0) --rend;
                            if (rend + 2 < pred->count)
                            {
                                remove_inner((Mid*)pred, rend + 2, pred->count);
                                index = rend + 2;
                            }
                            pred = ((Mid*)pred)->children[index];
                        }

                        idx = pred->count - 1;
                        while (idx > 0 && xyu::compare(pred->keys[idx], key) == 0) --idx, ++count;

                        node->keys[start] = xyu::move(pred->keys[idx]);
                        node->vals[start] = xyu::move(pred->vals[idx]);
                        node = pred;
                    }
                    /* ---------- 叶子节点处理 ---------- */
                    else
                    {
                        while (idx + count < node->count && xyu::compare(node->keys[idx + count], key) == 0) ++count;
                        while (idx > 0 && xyu::compare(node->keys[idx - 1], key) == 0) --idx, ++count;
                    }

                    /* ---------- 删除元素 ---------- */
                    {
                        for (xyu::uint32 i = idx + count; i < node->count; ++i)
                            node->keys[i - count] = xyu::move(node->keys[i]);
                        xyu::object_destroy(node->keys + node->count - count, count);
                        for (xyu::uint32 i = idx + count; i < node->count; ++i)
                            node->vals[i - count] = xyu::move(node->vals[i]);
                        xyu::object_destroy(node->vals + node->count - count, count);
                        node->count -= count;
                        n -= count;
                        total += count;
                    }

                    /* ---------- 处理下溢 ---------- */
                    fix_underflow<1>(node, &high);

                    if (high->is_leaf) break;
                }

                return total;
            }
            else
            {
                if (XY_UNLIKELY(!root)) return 0;

                // 旧节点 -> 新节点
                HashMap<Node*,Node*> replaces;
                // 新节点
                HashSet<Node*> newnodes;
                // 需要删除的旧节点 (递归删除 & 直接删除)
                NearList<Node*> removes, deletes;
                // 全局状态
                Node *new_root = root, *new_ln = ln, *new_rn = rn;
                xyu::size_t new_n = n, new_m = m;

                xyu::size_t total = 0;

                try
                {
                    // 获取最新节点
                    auto visible_node = [&](Node*& node, bool& native)
                    {
                        if (native)
                        {
                            if (Node** newn = replaces.select(node))
                                node = *newn, native = false;
                        }
                        else if (!newnodes.select(node)) native = true;
                    };

                    // 子节点变为最新节点
                    auto visible_children = [&](Mid* node, xyu::uint32 start = 0)
                    {
                        for (xyu::uint32 i = start; i <= node->count; ++i)
                        {
                            Node* child = node->children[i];
                            if (!newnodes.select(child)) continue;
                            child->parent = node;
                            child->index = i;
                        }
                    };

                    Location location = locate<false, 0>(key);
                    if (!location.find) return 0;
                    Node* high = location.node;
                    bool native_high = true;
                    for (;;)
                    {
                        /* ---------- 查找最顶层的匹配键 ---------- */
                        Node* leaf_pa = nullptr;
                        xyu::uint32 pos;
                        Node* node = high;
                        bool native = native_high;
                        xyu::uint32 idx;
                        for (;;)
                        {
                            if constexpr (xyu::t_is_number<Key> && M <= 8)
                            {
                                idx = 0;
                                while (idx < node->count)
                                {
                                    int r = xyu::compare(node->keys[idx], key);
                                    if (r == 0) goto has_key;
                                    if (r > 0) break;
                                    ++idx;
                                }
                            }
                            else
                            {
                                xyu::uint32 left = 0, right = node->count;
                                while (left < right)
                                {
                                    idx = (left + right) / 2;
                                    int r = xyu::compare(node->keys[idx], key);
                                    if (r == 0) goto has_key;
                                    if (r > 0) right = idx;
                                    else left = idx + 1;
                                }
                                idx = left;
                            }

                            if (node->is_leaf) break;

                            leaf_pa = node;
                            pos = idx;

                            node = ((Mid*)node)->children[idx];
                            visible_node(node, native);
                        }
                        break;

                    has_key:
                        high = node;
                        native_high = native;

                        xyu::uint32 kcount = 1;

                        /* ---------- 内部节点处理 ---------- */
                        if (!node->is_leaf)
                        {
                            // 找到相同键范围
                            xyu::uint32 start = idx, end = idx + 1;
                            while (start > 0 && xyu::compare(node->keys[start - 1], key) == 0) --start;
                            while (end < node->count && xyu::compare(node->keys[end], key) == 0) ++end;

                            // 保存需要删除中间的子节点
                            removes.reserve(removes.count() + end - start - 1);
                            for (xyu::uint32 i = start + 1; i < end; ++i)
                                removes.append(((Mid*)node)->children[i]);

                            // 寻找前驱
                            leaf_pa = node;
                            pos = start;
                            bool native_pred = native;
                            Node* pred = ((Mid*)node)->children[start];
                            for (;;)
                            {
                                visible_node(pred, native_pred);
                                if (pred->is_leaf) break;

                                xyu::uint32 index = pred->count;
                                if (native_pred)
                                {
                                    xyu::int32 rend = pred->count - 1;
                                    while (rend >= 0 && xyu::compare(pred->keys[rend], key) == 0) --rend;
                                    if (rend + 2 < pred->count)
                                    {
                                        xyu::uint32 s = rend + 2, e = pred->count;
                                        xyu::uint32 mcount = e - s;
                                        Mid* newp = (Mid*)copy_node(pred, [&](Node* newn, xyu::uint32& ki, xyu::uint32 vi)
                                        {
                                            newn->parent = (Mid*)leaf_pa;
                                            newn->index = pos;
                                            newn->count = pred->count - mcount;

                                            // 保存需要删除中间的子节点
                                            removes.reserve(removes.count() + mcount);
                                            for (xyu::uint32 i = s; i < e; ++i)
                                                removes.append(((Mid*)pred)->children[i]);

                                            // 复制键值
                                            for (; ki < s; ++ki)
                                                xyu::place_init(newn->keys + ki, pred->keys[ki]);
                                            for (; ki < newn->count; ++ki)
                                                xyu::place_init(newn->keys + ki, pred->keys[ki + mcount]);
                                            for (; vi < s; ++vi)
                                                xyu::place_init(newn->vals + vi, pred->vals[vi]);
                                            for (; vi < newn->count; ++vi)
                                                xyu::place_init(newn->vals + vi, pred->vals[vi + mcount]);
                                            total += mcount;
                                        });

                                        for (xyu::uint32 i = 0; i < s; ++i)
                                            newp->children[i] = ((Mid*)pred)->children[i];
                                        for (xyu::uint32 i = s; i <= newp->count; ++i)
                                            newp->children[i] = ((Mid*)pred)->children[i + mcount];
                                        visible_children(newp);

                                        if (leaf_pa)
                                        {
                                            newp->parent = (Mid*)leaf_pa;
                                            ((Mid*)leaf_pa)->children[pos] = newp;
                                        }

                                        replaces[pred] = newp;
                                        deletes.append(pred);
                                        newnodes.insert(newp);

                                        pred = newp;
                                        index = rend + 2;
                                        native_pred = false;
                                    }
                                }

                                if (native_pred) leaf_pa = nullptr;
                                else leaf_pa = pred, pos = index;
                                pred = ((Mid*)pred)->children[index];
                            }

                            idx = pred->count - 1;
                            while (idx > 0 && xyu::compare(pred->keys[idx], key) == 0) --idx, ++kcount;

                            // 构建新节点
                            xyu::uint32 mcount = end - start - 1;
                            if (native)
                            {
                                Node* nn = copy_node(node, [=](Node* newn, xyu::uint32& ki, xyu::uint32& vi)
                                {
                                    newn->count = node->count - mcount;

                                    for (; ki < start; ++ki)
                                        xyu::place_init(newn->keys + ki, node->keys[ki]);
                                    if (native_pred) xyu::place_init(newn->keys + ki, xyu::move(pred->keys[idx]));
                                    else xyu::place_init(newn->keys + ki, pred->keys[idx]);
                                    for (++ki; ki < newn->count; ++ki)
                                        xyu::place_init(newn->keys + ki, node->keys[ki + mcount]);

                                    for (; vi < start; ++vi)
                                        xyu::place_init(newn->vals + vi, node->vals[vi]);
                                    if (native_pred) xyu::place_init(newn->vals + vi, xyu::move(pred->vals[idx]));
                                    else xyu::place_init(newn->vals + vi, pred->vals[idx]);
                                    for (++vi; vi < newn->count; ++vi)
                                        xyu::place_init(newn->vals + vi, node->vals[vi + mcount]);

                                    for (xyu::uint32 i = 0; i <= start; ++i)
                                        ((Mid*)newn)->children[i] = ((Mid*)node)->children[i];
                                    for (xyu::uint32 i = end; i <= node->count; ++i)
                                        ((Mid*)newn)->children[i - mcount] = ((Mid*)node)->children[i];
                                    visible_children((Mid*)newn);
                                });

                                replaces[node] = nn;
                                deletes.append(node);
                                newnodes.insert(nn);

                                if (new_root == node) new_root = nn;
                                if (leaf_pa == node) leaf_pa = nn;
                                high = nn;
                                native_high = false;
                            }
                            else
                            {
                                if (start + 1 < end)
                                {
                                    // 删除中间键
                                    for (xyu::uint32 i = end; i < node->count; ++i)
                                        node->keys[i - mcount] = xyu::move(node->keys[i]);
                                    xyu::object_destroy(node->keys + node->count - mcount, mcount);
                                    if constexpr (NV) for (xyu::uint32 i = end; i < node->count; ++i)
                                        node->vals[i - mcount] = xyu::move(node->vals[i]);
                                    xyu::object_destroy(node->vals + node->count - mcount, mcount);

                                    // 移动剩余子节点
                                    for (xyu::uint32 i = end; i <= node->count; ++i)
                                        ((Mid*)node)->children[i - mcount] = ((Mid*)node)->children[i];
                                    node->count -= mcount;
                                    for (xyu::uint32 i = start + 1; i <= node->count; ++i)
                                        ((Mid*)node)->children[i]->index = i;
                                }

                                if (native_pred)
                                {
                                    xyu::place_init(node->keys + start, xyu::move(pred->keys[idx]));
                                    xyu::place_init(node->vals + start, xyu::move(pred->vals[idx]));
                                }
                                else
                                {
                                    xyu::place_init(node->keys + start, pred->keys[idx]);
                                    xyu::place_init(node->vals + start, pred->vals[idx]);
                                }
                            }

                            new_n -= mcount;
                            total += mcount;

                            node = pred;
                            native = native_pred;
                        }
                        /* ---------- 叶子节点处理 ---------- */
                        else
                        {
                            while (idx + kcount < node->count && xyu::compare(node->keys[idx + kcount], key) == 0) ++kcount;
                            while (idx > 0 && xyu::compare(node->keys[idx - 1], key) == 0) --idx, ++kcount;
                        }

                        /* ---------- 删除元素 ---------- */
                        if (native)
                        {
                            Node* nn = copy_node(node, [=](Node* newn, xyu::uint32& ki, xyu::uint32& vi)
                            {
                                newn->count = node->count - kcount;
                                for (; ki < idx; ++ki)
                                    xyu::place_init(newn->keys + ki, node->keys[ki]);
                                for (; ki < newn->count; ++ki)
                                    xyu::place_init(newn->keys + ki, node->keys[ki + kcount]);
                                if constexpr (NV)
                                {
                                    for (; vi < idx; ++vi)
                                        xyu::place_init(newn->vals + vi, node->vals[vi]);
                                    for (; vi < newn->count; ++vi)
                                        xyu::place_init(newn->vals + vi, node->vals[vi + kcount]);
                                }
                            });

                            // 原父节点可能为已删除的旧节点
                            if (leaf_pa)
                            {
                                nn->parent = (Mid*)leaf_pa;
                                nn->index = pos;
                                ((Mid*)leaf_pa)->children[pos] = nn;
                            }

                            replaces[node] = nn;
                            deletes.append(node);
                            newnodes.insert(nn);

                            if (high == node) high = nn;
                            if (new_root == node) new_root = nn;
                            if (new_ln == node) new_ln = nn;
                            if (new_rn == node) new_rn = nn;
                            node = nn;
                        }
                        else
                        {
                            for (xyu::uint32 i = idx + kcount; i < node->count; ++i)
                                node->keys[i - kcount] = xyu::move(node->keys[i]);
                            xyu::object_destroy(node->keys + node->count - kcount, kcount);
                            if constexpr (NV) for (xyu::uint32 i = idx + kcount; i < node->count; ++i)
                                node->vals[i - kcount] = xyu::move(node->vals[i]);
                            xyu::object_destroy(node->vals + node->count - kcount, kcount);
                            node->count -= kcount;
                        }
                        new_n -= kcount;
                        total += kcount;

                        /* ---------- 处理下溢 ---------- */
                        xyu::uint32 mcount = m * (M - 1) * K_shrink_factor;
                        xyu::uint32 mM = n <= mcount ? (M - 1) / 2 : 1;
                        for (;node->count < mM;)
                        {
                            Mid* parent = node->parent;
                            if (!parent) break;
                            if (newnodes.select(parent)) native = false;
                            else if (auto p = replaces.select(parent))
                            {
                                parent = (Mid*)*p;
                                node->parent = parent;
                                parent->children[node->index] = node;
                                native = false;
                            }
                            else native = true;

                            xyu::uint32 index = node->index;
                            xyu::uint32 count = node->count;
                            Node *left = nullptr, *right = nullptr;
                            xyu::uint32 lcount, rcount;

                            bool native_left = native;
                            bool native_right = native;

                            auto front_insert = [index, count, &lcount, &native_left, native]
                            (xyu::uint32 add, auto* arr, auto* larr, auto* parr, xyu::uint32& sc, xyu::uint32& nc)
                            {
                                nc = count;
                                sc = 0;
                                for (xyu::uint32 i = xyu::max(add, count); i < add + count; ++i)
                                    xyu::place_init(arr + i, xyu::move(arr[i - add])), ++sc;
                                for (int i = count - 1; i >= (int)add; --i)
                                    arr[i] = xyu::move(arr[i - add]), ++sc, --nc;
                                xyu::object_destroy(arr, xyu::min(count, add));
                                for (nc = 0; nc < add - 1; ++nc)
                                {
                                    if (native_left) xyu::place_init(arr + nc, larr[lcount - add + nc + 1]);
                                    else xyu::place_init(arr + nc, xyu::move(larr[lcount - add + nc + 1]));
                                }
                                if (native) xyu::place_init(arr + add - 1, parr[index - 1]);
                                else xyu::place_init(arr + add - 1, xyu::move(parr[index - 1]));
                                nc = count + add;
                                sc = 0;
                            };

                            auto back_insert = [index, count, &native_right, native]
                            (xyu::uint32 add, auto* arr, auto* rarr, auto* parr, xyu::uint32& nc)
                            {
                                nc = count;
                                if (native) xyu::place_init(arr + count, parr[index]);
                                else xyu::place_init(arr + count, xyu::move(parr[index]));
                                ++nc;
                                for (xyu::uint32 i = 0; i < add - 1; ++i, ++nc)
                                {
                                    if (native_right) xyu::place_init(arr + count + 1 + i, rarr[i]);
                                    else xyu::place_init(arr + count + 1 + i, xyu::move(rarr[i]));
                                }
                            };

                            // --- 1. 从左兄弟借 ---
                            if (index > 0 && (visible_node((left = parent->children[index - 1]), native_left),
                                (lcount = left->count) + count >= 2*mM))
                            {
                                xyu::uint32 add = (lcount - count) / 2;
                                xyu::uint32 nk = count, nv = count;
                                xyu::uint32 sk = 0, sv = 0;
                                try
                                {
                                    front_insert(add, node->keys, left->keys, parent->keys, sk, nk);
                                    if constexpr (NV) front_insert(add, node->vals, left->vals, parent->vals, sv, nv);
                                }
                                catch (...)
                                {
                                    xyu::uint32 sinit = xyu::max(add, count);
                                    xyu::uint32 ninit = xyu::min(add, count);
                                    if (sk <= ninit) xyu::object_destroy(node->keys + sinit, sk);
                                    else xyu::object_destroy(node->keys + count + add - sk, sk);
                                    if (sv <= ninit) xyu::object_destroy(node->vals + sinit, sv);
                                    else xyu::object_destroy(node->vals + count + add - sv, sv);
                                    dealloc_part_node(node, nk, nv);
                                    newnodes.erase(node);
                                    throw;
                                }
                                node->count += add;

                                if (!node->is_leaf)
                                {
                                    for (int i = count; i >= 0; --i)
                                        ((Mid*)node)->children[i + add] = ((Mid*)node)->children[i];
                                    for (xyu::uint32 i = 0; i < add; ++i)
                                        ((Mid*)node)->children[i] = ((Mid*)left)->children[lcount + 1 - add + i];
                                    visible_children((Mid*)node);
                                }

                                if (native)
                                {
                                    Mid* newp = (Mid*)copy_node(parent, [=](Node* newn, xyu::uint32& ki, xyu::uint32& vi)
                                    {
                                        for (; ki < index - 1; ++ki)
                                            xyu::place_init(newn->keys + ki, parent->keys[ki]);
                                        if (native_left) xyu::place_init(newn->keys + ki, left->keys[lcount - add]);
                                        else xyu::place_init(newn->keys + ki, xyu::move(left->keys[lcount - add]));
                                        for (++ki; ki < parent->count; ++ki)
                                            xyu::place_init(newn->keys + ki, parent->keys[ki]);

                                        if constexpr (NV)
                                        {
                                            for (; vi < index - 1; ++vi)
                                                xyu::place_init(newn->vals + vi, parent->vals[vi]);
                                            if (native_left) xyu::place_init(newn->vals + vi, left->vals[lcount - add]);
                                            else xyu::place_init(newn->vals + vi, xyu::move(left->vals[lcount - add]));
                                            for (++vi; vi < parent->count; ++vi)
                                                xyu::place_init(newn->vals + vi, parent->vals[vi]);
                                        }
                                    });

                                    for (xyu::uint32 i = 0; i <= parent->count; ++i)
                                        newp->children[i] = parent->children[i];
                                    visible_children(newp);

                                    replaces[parent] = newp;
                                    deletes.append(parent);
                                    newnodes.insert(newp);
                                    if (high == parent) high = newp, native_high = false;
                                    if (new_root == parent) new_root = newp;
                                    parent = newp;

                                    node->parent = parent;
                                    parent->children[node->index] = node;
                                }
                                else
                                {
                                    if (native_left)
                                    {
                                        parent->keys[index - 1] = xyu::move(left->keys[lcount - add]);
                                        if constexpr (NV) parent->vals[index - 1] = xyu::move(left->vals[lcount - add]);
                                    }
                                    else
                                    {
                                        parent->keys[index - 1] = left->keys[lcount - add];
                                        if constexpr (NV) parent->vals[index - 1] = left->vals[lcount - add];
                                    }
                                }

                                if (native_left)
                                {
                                    Node* newl = copy_node(left, [=](Node* newn, xyu::uint32& ki, xyu::uint32& vi)
                                    {
                                        newn->count = lcount - add;
                                        for (; ki < newn->count; ++ki)
                                            xyu::place_init(newn->keys + ki, left->keys[ki]);
                                        if constexpr (NV) for (; vi < newn->count; ++vi)
                                            xyu::place_init(newn->vals + vi, left->vals[vi]);
                                    });

                                    if (!left->is_leaf)
                                    {
                                        for (xyu::uint32 i = 0; i <= newl->count; ++i)
                                            ((Mid*)newl)->children[i] = ((Mid*)left)->children[i];
                                        visible_children((Mid*)newl);
                                    }
                                    else if (new_ln == left) new_ln = newl;

                                    replaces[left] = newl;
                                    deletes.append(left);
                                    newnodes.insert(newl);
                                    left = newl;
                                }
                                else
                                {
                                    xyu::object_destroy(left->keys + lcount - add, add);
                                    left->count -= add;
                                }
                                left->parent = parent;
                                parent->children[index - 1] = left;

                                node = parent;
                                break;
                            }

                            // --- 2. 从右兄弟借 ---
                            if (index < parent->count && (visible_node((right = parent->children[index + 1]), native_right),
                                (rcount = right->count) + count >= 2*mM))
                            {
                                xyu::uint32 add = (rcount - count) / 2;
                                xyu::uint32 nk = count, nv = count;
                                try
                                {
                                    back_insert(add, node->keys, right->keys, parent->keys, nk);
                                    back_insert(add, node->vals, right->vals, parent->vals, nv);
                                }
                                catch (...)
                                {
                                    dealloc_part_node(node, nk, nv);
                                    newnodes.erase(node);
                                    throw;
                                }
                                node->count += add;

                                if (!node->is_leaf)
                                {
                                    for (xyu::uint32 i = 0; i < add; ++i)
                                        ((Mid*)node)->children[count + 1 + i] = ((Mid*)right)->children[i];
                                    visible_children((Mid*)node);
                                }

                                if (native)
                                {
                                    Mid* newp = (Mid*)copy_node(parent, [=](Node* newn, xyu::uint32& ki, xyu::uint32& vi)
                                    {
                                        for (; ki < index; ++ki) xyu::place_init(newn->keys + ki, parent->keys[ki]);
                                        if (native_right) xyu::place_init(newn->keys + ki, right->keys[add - 1]);
                                        else xyu::place_init(newn->keys + ki, xyu::move(right->keys[add - 1]));
                                        for (++ki; ki < parent->count; ++ki) xyu::place_init(newn->keys + ki, parent->keys[ki]);

                                        for (; vi < index; ++vi) xyu::place_init(newn->vals + vi, parent->vals[vi]);
                                        if (native_right) xyu::place_init(newn->vals + vi, right->vals[add - 1]);
                                        else xyu::place_init(newn->vals + vi, xyu::move(right->vals[add - 1]));
                                        for (++vi; vi < parent->count; ++vi) xyu::place_init(newn->vals + vi, parent->vals[vi]);
                                    });

                                    for (xyu::uint32 i = 0; i <= parent->count; ++i)
                                        newp->children[i] = parent->children[i];
                                    visible_children(newp);

                                    replaces[parent] = newp;
                                    deletes.append(parent);
                                    newnodes.insert(newp);
                                    if (high == parent) high = newp, native_high = false;
                                    if (new_root == parent) new_root = newp;
                                    parent = newp;

                                    node->parent = parent;
                                    parent->children[node->index] = node;
                                }
                                else
                                {
                                    if (native_right)
                                    {
                                        parent->keys[index] = right->keys[add - 1];
                                        if constexpr (NV) parent->vals[index] = right->vals[add - 1];
                                    }
                                    else
                                    {
                                        parent->keys[index] = xyu::move(right->keys[add - 1]);
                                        if constexpr (NV) parent->vals[index] = xyu::move(right->vals[add - 1]);
                                    }
                                }

                                if (native_right)
                                {
                                    Node* newr = copy_node(right, [=](Node* newn, xyu::uint32& ki, xyu::uint32& vi)
                                    {
                                        newn->count = rcount - add;
                                        for (; ki < newn->count; ++ki) xyu::place_init(newn->keys + ki, right->keys[ki + add]);
                                        for (; vi < newn->count; ++vi) xyu::place_init(newn->vals + vi, right->vals[vi + add]);
                                    });

                                    if (!right->is_leaf)
                                        for (xyu::uint32 i = 0; i <= newr->count; ++i)
                                            ((Mid*)newr)->children[i] = ((Mid*)right)->children[i + add];
                                    else if (new_rn == right) new_rn = newr;

                                    replaces[right] = newr;
                                    deletes.append(right);
                                    newnodes.insert(newr);
                                    right = newr;
                                }
                                else
                                {
                                    right->count -= add;
                                    for (xyu::uint32 i = 0; i < right->count; ++i)
                                        right->keys[i] = xyu::move(right->keys[i + add]);
                                    xyu::object_destroy(right->keys + right->count, add);
                                    if constexpr (NV) for (xyu::uint32 i = 0; i < right->count; ++i)
                                        right->vals[i] = xyu::move(right->vals[i + add]);
                                    xyu::object_destroy(right->vals + right->count, add);

                                    if (!right->is_leaf)
                                        for (xyu::uint32 i = 0; i <= right->count; ++i)
                                            ((Mid*)right)->children[i] = ((Mid*)right)->children[i + add];
                                }
                                if (!right->is_leaf) visible_children((Mid*)right);
                                right->parent = parent;
                                parent->children[index + 1] = right;

                                node = parent;
                                break;
                            }

                            // --- 3. 合并兄弟 ---
                            {
                                bool concat_left = left && (!right || lcount <= rcount);

                                if (concat_left)
                                {
                                    xyu::uint32 add = lcount + 1;
                                    xyu::uint32 nk = count, nv = count;
                                    xyu::uint32 sk = 0, sv = 0;
                                    try
                                    {
                                        front_insert(add, node->keys, left->keys, parent->keys, sk, nk);
                                        if constexpr (NV) front_insert(add, node->vals, left->vals, parent->vals, sv, nv);
                                    }
                                    catch (...)
                                    {
                                        xyu::uint32 sinit = xyu::max(add, count);
                                        xyu::uint32 ninit = xyu::min(add, count);
                                        if (sk <= ninit) xyu::object_destroy(node->keys + sinit, sk);
                                        else xyu::object_destroy(node->keys + count + add - sk, sk);
                                        if (sv <= ninit) xyu::object_destroy(node->vals + sinit, sv);
                                        else xyu::object_destroy(node->vals + count + add - sv, sv);
                                        dealloc_part_node(node, nk, nv);
                                        newnodes.erase(node);
                                        throw;
                                    }
                                    node->count += add;
                                    node->index = index - 1;

                                    if (!node->is_leaf)
                                    {
                                        for (xyu::uint32 i = node->count; i >= add; --i)
                                            ((Mid*)node)->children[i] = ((Mid*)node)->children[i - add];
                                        for (xyu::uint32 i = 0; i < add; ++i)
                                            ((Mid*)node)->children[i] = ((Mid*)left)->children[i];
                                    }

                                    if (new_ln == left) new_ln = node;
                                    if (!native_left) newnodes.erase(left);
                                    deletes.append(left);
                                }
                                else
                                {
                                    xyu::uint32 add = rcount + 1;
                                    xyu::uint32 nk = count, nv = count;
                                    try
                                    {
                                        back_insert(add, node->keys, right->keys, parent->keys, nk);
                                        back_insert(add, node->vals, right->vals, parent->vals, nv);
                                    }
                                    catch (...)
                                    {
                                        dealloc_part_node(node, nk, nv);
                                        newnodes.erase(node);
                                        throw;
                                    }

                                    if (!node->is_leaf)
                                        for (xyu::uint32 i = 0; i < add; ++i)
                                            ((Mid*)node)->children[count + 1 + i] = ((Mid*)right)->children[i];
                                    node->count += add;

                                    if (new_rn == right) new_rn = node;
                                    if (!native_right) newnodes.erase(right);
                                    deletes.append(right);
                                }
                                if (!node->is_leaf) visible_children((Mid*)node);
                                --new_m;

                                xyu::uint32 del_kv = concat_left ? (index - 1) : index;
                                xyu::uint32 del_child = concat_left ? (index - 1) : (index + 1);
                                if (native)
                                {
                                    Mid* newp = (Mid*)copy_node(parent, [=](Node* newn, xyu::uint32& ki, xyu::uint32& vi)
                                    {
                                        newn->count = parent->count - 1;
                                        for (; ki < del_kv; ++ki)
                                            xyu::place_init(newn->keys + ki, parent->keys[ki]);
                                        for (; ki < newn->count; ++ki)
                                            xyu::place_init(newn->keys + ki, parent->keys[ki + 1]);

                                        for (; vi < del_kv; ++vi)
                                            xyu::place_init(newn->vals + vi, parent->vals[vi]);
                                        for (; vi < newn->count; ++vi)
                                            xyu::place_init(newn->vals + vi, parent->vals[vi + 1]);
                                    });

                                    for (xyu::uint32 i = 0; i < del_child; ++i)
                                        newp->children[i] = parent->children[i];
                                    for (xyu::uint32 i = del_child; i <= newp->count; ++i)
                                        newp->children[i] = parent->children[i + 1];
                                    visible_children(newp);

                                    replaces[parent] = newp;
                                    deletes.append(parent);
                                    newnodes.insert(newp);

                                    if (high == parent) high = newp, native_high = false;
                                    if (new_root == parent) new_root = newp;
                                    parent = newp;

                                    node->parent = parent;
                                    parent->children[node->index] = node;
                                }
                                else
                                {
                                    --parent->count;
                                    for (xyu::uint32 i = del_kv; i < parent->count; ++i)
                                        parent->keys[i] = xyu::move(parent->keys[i + 1]);
                                    xyu::object_destroy(parent->keys + parent->count);
                                    if constexpr (NV) for (xyu::uint32 i = del_kv; i < parent->count; ++i)
                                        parent->vals[i] = xyu::move(parent->vals[i + 1]);
                                    xyu::object_destroy(parent->vals + parent->count);
                                    for (xyu::uint32 i = del_child; i <= parent->count; ++i)
                                        parent->children[i] = parent->children[i + 1];
                                    visible_children(parent, del_child);
                                }

                                node = parent;
                            }
                        }

                        // 保证内部节点修改后的可见性
                        if (Mid* parent = node->parent)
                        {
                            if (auto p = replaces.select(parent))
                            {
                                parent = (Mid*)*p;
                                node->parent = parent;
                                parent->children[node->index] = node;
                            }
                        }

                        /* ---------- 根节点缩并 ---------- */
                        if (new_root->count == 0)
                        {
                            if (new_root->is_leaf)
                            {
                                total = n;
                                for (auto val : replaces.vrange()) dealloc_node(val);
                                release();
                                return total;
                            }
                            else
                            {
                                native = new_root == root;
                                Node* child = ((Mid*)new_root)->children[0];
                                visible_node(child, native);

                                if (new_root != root) newnodes.erase(new_root);
                                deletes.append(new_root);

                                if (native)
                                {
                                    Node* newc = copy_node(child, [=](Node* newn, xyu::uint32& ki, xyu::uint32& vi)
                                    {
                                        for (; ki < child->count; ++ki) newn->keys[ki] = child->keys[ki];
                                        if constexpr (NV) for (; vi < child->count; ++vi) newn->vals[vi] = child->vals[vi];
                                    });

                                    if (!child->is_leaf)
                                    {
                                        for (xyu::uint32 i = 0; i <= newc->count; ++i)
                                            ((Mid*)newc)->children[i] = ((Mid*)child)->children[i];
                                        visible_children((Mid*)newc);
                                    }

                                    replaces[child] = newc;
                                    deletes.append(child);
                                    newnodes.insert(newc);
                                    if (new_ln == child) new_ln = newc;
                                    if (new_rn == child) new_rn = newc;

                                    child = newc;
                                }

                                child->parent = nullptr;
                                child->index = 0;

                                --new_m;
                                new_root = child;
                            }
                        }

                        if (high->is_leaf) break;
                    }

                    /* ---------- 应用修改 ---------- */

                    for (auto node : newnodes.range())
                    {
                        if (node->parent && !newnodes.select(node->parent))
                            node->parent->children[node->index] = (Node*)node;
                        if (node->is_leaf) continue;
                        for (xyu::uint32 i = 0; i <= node->count; ++i)
                        {
                            Node* child = ((Mid*)node)->children[i];
                            if (!newnodes.select(child))
                            {
                                child->parent = (Mid*)node;
                                child->index = i;
                            }
                        }    
                    }

                    // 更新状态
                    root = new_root, ln = new_ln, rn = new_rn;
                    n = new_n, m = new_m;

                    // 释放旧节点
                    for (auto oldn : deletes.range()) dealloc_node(oldn);
                    release_tree<true>(removes);

                    total += new_n - n;
                }
                catch (...)
                {
                    for (auto node : replaces.vrange()) dealloc_node(node);
                    throw;
                }

                return total;
            }
        }

    private:
        static_assert(K_lent_factor <= 1 && K_lent_factor >= 0);
        static_assert(K_shrink_factor <= 1 && K_shrink_factor >= 0);

    private:
        // 存储类
        template <int kind>
        struct Storage
        {
            using Owner_t = BTreeBase;
            using Kind_t = xyu::size_c<kind>;

            Node* p;            // 节点
            xyu::uint32 index;  // 数据索引

            constexpr Storage(Node* node = nullptr, xyu::uint32 index = 0) noexcept : p{node}, index {index} {}
        };

        // 解引用
        template <int kind>
        struct Dereference
        {
            template <typename Iter>
            constexpr decltype(auto) operator()(Iter& iter) noexcept
            {
                if constexpr (kind == 0)
                {
                    using KV = KVNode<const Key&, Val&>;
                    return KV{iter.p->keys[iter.index], iter.p->vals[iter.index]};
                }
                else if constexpr (kind == 1 || kind == 4) return xyu::make_const(iter.p->keys[iter.index]);
                else if constexpr (kind == 2) return (iter.p->vals[iter.index]);
                else if constexpr (kind == 3)
                {
                    using KV = KVNode<const Key&, const Val&>;
                    return KV{iter.p->keys[iter.index], iter.p->vals[iter.index]};
                }
                else if constexpr (kind == 5) return xyu::make_const(iter.p->vals[iter.index]);
                else static_assert(kind == 404, "Invalid kind");
            }
        };

        // 自增
        struct Increment
        {
            template <typename Iter>
            constexpr void operator()(Iter& iter) noexcept
            {
                if (iter.p->is_leaf)
                {
                    if (++iter.index < iter.p->count);
                    else for (auto node = iter.p;;)
                    {
                        auto pos = node->index;
                        node = node->parent;
                        if (XY_UNLIKELY(!node)) return;
                        if (pos < node->count) { iter = {node, pos}; return; }
                    }
                }
                else
                {
                    auto node = ((Mid*)iter.p)->children[iter.index + 1];
                    while (!node->is_leaf) node = ((Mid*)node)->children[0];
                    iter = {node, 0};
                }
            }
        };

        // 自减
        struct Decrement
        {
            template <typename Iter>
            constexpr void operator()(Iter& iter) noexcept
            {
                if (iter.p->is_leaf)
                {
                    if (iter.index-- > 0);
                    else for (auto node = iter.p;;)
                    {
                        auto pos = node->index;
                        node = node->parent;
                        if (XY_UNLIKELY(!node)) return;
                        if (pos) { iter = {node, pos - 1}; return; }
                    }
                }
                else
                {
                    auto node = ((Mid*)iter.p)->children[iter.index];
                    while (!node->is_leaf) node = ((Mid*)node)->children[node->count];
                    iter = {node, node->count - 1};
                }
            }
        };

        // 前进
        struct Forward
        {
            template <typename Iter, typename Diff>
            constexpr void operator()(Iter& iter, Diff step) noexcept
            {
                auto node = iter.p;
                auto index  = iter.index;

                while (step > 0)
                {
                    if (node->is_leaf)
                    {
                        auto remain = node->count - index;
                        if (remain > step) { index += step; break; }
                        step -= remain;
                        iter = {node, node->count};
                        for (;;)
                        {
                            auto pos = node->index;
                            node = node->parent;
                            if (XY_UNLIKELY(!node)) return;
                            if (pos < node->count) { index = pos; break; }
                        }
                    }
                    if (step > 0)
                    {
                        --step;
                        node = ((Mid*)node)->children[index + 1];
                        while (!node->is_leaf) node = ((Mid*)node)->children[0];
                        index = 0;
                    }
                }
                iter = {node, index};
            }
        };

        // 后退
        struct Backward
        {
            template <typename Iter, typename Diff>
            constexpr void operator()(Iter& iter, Diff step) noexcept
            {
                auto node = iter.p;
                auto index  = iter.index;

                while (step > 0)
                {
                    if (node->is_leaf)
                    {
                        auto remain = index + 1;
                        if (remain > step) { index -= step; break; }
                        step -= remain;
                        iter = {node, -1u};
                        for (;;)
                        {
                            auto pos = node->index;
                            node = node->parent;
                            if (XY_UNLIKELY(!node)) return;
                            if (pos > 0) { index = pos - 1; break; }
                        }
                    }
                    if (step > 0)
                    {
                        --step;
                        node = ((Mid*)node)->children[index];
                        while (!node->is_leaf) node = ((Mid*)node)->children[node->count];
                        index = node->count - 1;
                    }
                }

                iter = {node, index};
            }
        };

        // 比较
        struct Equal
        {
            template <typename Iter>
            constexpr bool operator()(const Iter& it1, const Iter& it2) noexcept
            { return it1.p == it2.p && it1.index == it2.index; }
        };

        template <int kind>
        using Iter = xyu::Iterator<Storage<kind>, xyu::IterPolicies::Default::Valid, Dereference<kind>, void,
                                   Increment, Decrement, Forward, Backward, void, Equal, void>;
        template <int kind>
        using rIter = xyu::Iterator_Reverse_Direct<Iter<kind>>;

    public:
        // 获取范围
        template <int kind>
        auto get_range() noexcept
        {
            if (XY_UNLIKELY(n == 0)) return xyu::Range<Iter<kind>>{0,{},{}};
            return xyu::Range{n, Iter<kind>{ln, 0}, Iter<kind>{rn, rn->count}};
        }
        // 获取反向范围
        template <int kind>
        auto get_rrange() noexcept
        {
            if (XY_UNLIKELY(n == 0)) return xyu::Range<rIter<kind>>{0,{},{}};
            return xyu::Range{n, rIter<kind>{rn, rn->count-1}, rIter<kind>{ln, -1u}};
        }

    protected:
        void print_bfs()
        {
            auto& os = xyu::xyout;

            if (!root) { os << "(empty tree)\n"; return; }
            os << "tree: (" << m << "," << n << ")\n";

            // 用队列做层序遍历
            struct Item { Node* node; int level; };
            NearQueue<Item> queue{64};
            queue.push(xyu::special_v, root, 0);

            int cur_level = -1;
            while (queue.count() > 0)
            {
                Item item = queue.pop();
                auto* node = item.node;
                int level = item.level;

                if (level != cur_level) {
                    if (cur_level != -1) os << "\n";
                    cur_level = level;
                    os << "Level " << level << ": \n";
                }

                // 节点类型和基本信息
                os << "  [" << (void*)node << " ";
                if (!node) {
                    os << "NULL] ";
                    continue;
                }
                os << (node->is_leaf ? "L" : "I") << " ";
                os << "cnt=" << node->count << " ";
                os << "idx=" << node->index << " ";
                if (node->parent)
                    os << "p=" << node->parent << " ";
                else
                    os << "p=none ";

                // 键值
                os << "kv=[";
                for (xyu::uint32 i = 0; i < node->count; ++i) {
                    if (i) os << ",";
                    os << node->keys[i];
                    if constexpr (sizeof(Val)) os << ':';
                    if constexpr (sizeof(Val)) os << node->vals[i];
                }
                os << "]";

                // 如果不是叶子，打印子节点指针（地址）并加入队列
                if (!node->is_leaf) {
                    Mid* mid = (Mid*)node;
                    os << " children=[";
                    for (xyu::uint32 i = 0; i <= node->count; ++i) {
                        if (i) os << ",";
                        Node* child = mid->children[i];
                        os << (child ? (void*)child : "(null)");
                        queue.push(xyu::special_v, child, level + 1);
                    }
                    os << "]";
                }
                os << "] \n";

                os.flush();
            }
            os << "\n";

            // 额外打印 ln/rn 地址供参考
            os << "ln = " << (void*)ln << "\n";
            os << "rn = " << (void*)rn << "\n";
            os << "\n";
            os << "\n";
        }
    };
}
