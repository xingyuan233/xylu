#pragma once

#include <iostream>
#include <iomanip>
#include <vector>
#include <random>
#include <string>
#include <utility>
#include <algorithm>
#include <cassert>
#include <map>

#include "../detail/xycoll/btree.h"
#include "../detail/xysystem/time.h"
#include "../detail/xycoll/queue.h"


constexpr int N_preload = 1000000;      // 初始数据量
constexpr int N_ops     =  500000;      // 混合操作次数

constexpr int M = 32;                     // B树阶数
constexpr bool cross_thread = false;

// ===================== 性能测试 =====================
void benchmark_btree_shrink_factor() {
    using namespace std;

    using Tree = xyu::BTreeMap<int, int, M, cross_thread>;

    // 测试的收缩因子列表 (标准算法即 >= 0.5)
    // const vector<double> factors = {0.2, 0.3, 0.4, 0.5, 0.6, 0.8, 1.0};
    // const vector<double> factors = {0.2, 0.3, 0.32, 0.34, 0.36, 0.38, 0.40, 0.42, 0.44, 0.46, 0.48, 0.50, 1.0};
    // const vector<double> factors = {0.4, 0.5};
    const vector<double> factors = {0.4};

    mt19937 rng(42);                       // 固定种子，可复现

    cout << "=== BTree Shrink Factor Benchmark (M=" << M << ") ===" << endl;
    cout << "preload " << N_preload << " elements, then " << N_ops << " operations" << endl;
    cout << "---------------------------------------------------------" << endl;

    int times = 0;
    for (double factor : factors) {
        // Tree::K_shrink_factor = factor;

        // ========== 1. 纯删除测试 ==========
        {
            Tree tree;
            for (int i = 0; i < N_preload; ++i) tree.append(i, i);  // 有序插入

            xyu::Clock clk;
            clk.start();
            for (int i = 0; i < N_preload; ++i) tree.erase(i);      // 顺序全部删除
            auto t = clk.stop();

            cout << "[Factor=" << factor << "] Pure delete : "
                 << t.count << " ns (" << t.count / 1000000.0 << " ms)" << endl;
        }

        // ========== 2. 有序交替删除/插入测试 ==========
        {
            Tree tree;
            for (int i = 0; i < N_preload; ++i) tree.append(i, i);
            int next_key = N_preload;

            xyu::Clock clk;
            clk.start();
            for (int i = 0; i < N_ops; ++i) {
                tree.erase(i);                    // 删除最小键
                tree.append(next_key++, 0);       // 插入新的大键
            }
            auto t = clk.stop();

            cout << "[Factor=" << factor << "] Ordered mix: "
                 << t.count << " ns (" << t.count / 1000000.0 << " ms)" << endl;
        }

        // ========== 3. 随机删除/插入测试 ==========
        {
            Tree tree;
            // 预加载有序键，但后续操作随机
            for (int i = 0; i < N_preload; ++i) tree.append(i, i);

            vector<int> live;                       // 当前存在的键
            live.reserve(N_preload + N_ops);
            for (int i = 0; i < N_preload; ++i) live.push_back(i);

            int next_key = N_preload;
            uniform_int_distribution<int> op_dist(0, 1);     // 0:删除, 1:插入
            uniform_int_distribution<size_t> idx_dist;       // 用于随机选删除对象

            xyu::Clock clk;
            clk.start();
            for (int i = 0; i < N_ops; ++i) {
                if (!live.empty() && op_dist(rng) == 0) {    // 随机删除
                    idx_dist.param(uniform_int_distribution<size_t>::param_type(0, live.size() - 1));
                    size_t pos = idx_dist(rng);
                    int key = live[pos];
                    tree.erase(key);
                    live[pos] = live.back();
                    live.pop_back();
                } else {                                     // 随机插入
                    int key = next_key++;
                    tree.append(key, 0);
                    live.push_back(key);
                }
            }
            auto t = clk.stop();

            cout << "[Factor=" << factor << "] Random mix : "
                 << t.count << " ns (" << t.count / 1000000.0 << " ms)" << endl;
        }

        cout << endl;
    }

     using namespace std;

    using Tree2 = std::map<int, int>;

    cout << "=== std::map Benchmark ===" << endl;
    cout << "preload " << N_preload << " elements, then " << N_ops << " operations" << endl;
    cout << "---------------------------------------------------------" << endl;

    // 1. 纯删除
    {
        Tree2 tree;
        for (int i = 0; i < N_preload; ++i) tree.emplace(i, i);

        xyu::Clock clk;
        clk.start();
        for (int i = 0; i < N_preload; ++i) tree.erase(i);
        auto t = clk.stop();

        cout << "[std::map] Pure delete : "
             << t.count << " ns (" << t.count / 1000000.0 << " ms)" << endl;
    }

    // 2. 有序交替
    {
        Tree2 tree;
        for (int i = 0; i < N_preload; ++i) tree.emplace(i, i);
        int next_key = N_preload;

        xyu::Clock clk;
        clk.start();
        for (int i = 0; i < N_ops; ++i) {
            tree.erase(i);
            tree.emplace(next_key++, 0);
        }
        auto t = clk.stop();

        cout << "[std::map] Ordered mix: "
             << t.count << " ns (" << t.count / 1000000.0 << " ms)" << endl;
    }

    // 3. 随机混合
    {
        Tree2 tree;
        for (int i = 0; i < N_preload; ++i) tree.emplace(i, i);

        vector<int> live;
        live.reserve(N_preload + N_ops);
        for (int i = 0; i < N_preload; ++i) live.push_back(i);

        int next_key = N_preload;
        uniform_int_distribution<int> op_dist(0, 1);
        uniform_int_distribution<size_t> idx_dist;

        xyu::Clock clk;
        clk.start();
        for (int i = 0; i < N_ops; ++i) {
            if (!live.empty() && op_dist(rng) == 0) {
                idx_dist.param(uniform_int_distribution<size_t>::param_type(0, live.size() - 1));
                size_t pos = idx_dist(rng);
                int key = live[pos];
                tree.erase(key);
                live[pos] = live.back();
                live.pop_back();
            } else {
                int key = next_key++;
                tree.emplace(key, 0);
                live.push_back(key);
            }
        }
        auto t = clk.stop();

        cout << "[std::map] Random mix : "
             << t.count << " ns (" << t.count / 1000000.0 << " ms)" << endl;
    }
}

void benchmark_btree_lent_factor() {
    using namespace std;

    using Tree = xyu::BTreeMap<int, int, M, cross_thread>;

    // 测试的借出因子列表（0.0 表示永不借出，1.0 表示最大借出）
    const vector<double> factors = {0.0, 0.5, 0.6, 0.7, 0.8, 0.9, 1.0};
    // const vector<double> factors = {0.8, 0.82, 0.84, 0.86, 0.88, 0.9, 0.92,0.94,0.96,0.98,1.0};

    mt19937 rng(42);

    cout << "=== BTree Lend Factor Benchmark (M=" << M << ") ===" << endl;
    cout << "Focus on insertion performance." << endl;
    cout << "---------------------------------------------------------" << endl;

    for (double factor : factors) {
        // Tree::K_lent_factor = factor;         // 设置静态借出因子

        // ========== 1. 纯顺序插入 ==========
        {
            Tree tree;
            xyu::Clock clk;
            clk.start();
            for (int i = 0; i < N_preload; ++i)
                tree.append(i, i);
            auto t = clk.stop();

            cout << "[Lend=" << factor << "] Seq insert : "
                 << t.count / 1'000'000.0 << " ms" << endl;
        }

        // ========== 2. 纯随机插入 ==========
        {
            Tree tree;
            // 先生成随机键（避免生成开销影响测量）
            vector<int> keys(N_preload);
            for (int i = 0; i < N_preload; ++i) keys[i] = i;
            shuffle(keys.begin(), keys.end(), rng);

            xyu::Clock clk;
            clk.start();
            for (int key : keys)
                tree.append(key, key);
            auto t = clk.stop();

            cout << "[Lend=" << factor << "] Rand insert: "
                 << t.count / 1'000'000.0 << " ms" << endl;
        }

        // ========== 3. 顺序插入 + 随机删除（插入量多于删除） ==========
        {
            Tree tree;
            // 预加载少量数据
            for (int i = 0; i < N_preload / 10; ++i)
                tree.append(i, i);

            vector<int> live;                 // 当前存在的键
            for (int i = 0; i < N_preload / 10; ++i) live.push_back(i);
            int next_key = N_preload / 10;

            uniform_int_distribution<int> op_dist(0, 4);   // 0~4，0删除, 1~4插入 → 20%删除
            uniform_int_distribution<size_t> idx_dist;

            xyu::Clock clk;
            clk.start();
            for (int i = 0; i < N_ops; ++i) {
                if (!live.empty() && op_dist(rng) == 0) {  // 随机删除
                    idx_dist.param(uniform_int_distribution<size_t>::param_type(0, live.size() - 1));
                    size_t pos = idx_dist(rng);
                    tree.erase(live[pos]);
                    live[pos] = live.back();
                    live.pop_back();
                } else {                                     // 插入新键
                    tree.append(next_key, 0);
                    live.push_back(next_key++);
                }
            }
            auto t = clk.stop();

            cout << "[Lend=" << factor << "] Insert-heavy mix: "
                 << t.count / 1'000'000.0 << " ms" << endl;
        }

        cout << endl;
    }

    cout << "--- std::map<int, int> comparison ---" << endl;

    {
        map<int, int> m;
        xyu::Clock clk;
        clk.start();
        for (int i = 0; i < N_preload; ++i)
            m.emplace(i, i);
        auto t = clk.stop();
        cout << "[std::map] Seq insert : "
             << t.count / 1'000'000.0 << " ms" << endl;
    }

    {
        map<int, int> m;
        vector<int> keys(N_preload);
        for (int i = 0; i < N_preload; ++i) keys[i] = i;
        shuffle(keys.begin(), keys.end(), rng);

        xyu::Clock clk;
        clk.start();
        for (int key : keys)
            m.emplace(key, key);
        auto t = clk.stop();
        cout << "[std::map] Rand insert: "
             << t.count / 1'000'000.0 << " ms" << endl;
    }

    {
        map<int, int> m;
        for (int i = 0; i < N_preload / 10; ++i)
            m.emplace(i, i);

        vector<int> live;
        for (int i = 0; i < N_preload / 10; ++i) live.push_back(i);
        int next_key = N_preload / 10;

        uniform_int_distribution<int> op_dist(0, 4);
        uniform_int_distribution<size_t> idx_dist;

        xyu::Clock clk;
        clk.start();
        for (int i = 0; i < N_ops; ++i) {
            if (!live.empty() && op_dist(rng) == 0) {
                idx_dist.param(uniform_int_distribution<size_t>::param_type(0, live.size() - 1));
                size_t pos = idx_dist(rng);
                m.erase(live[pos]);
                live[pos] = live.back();
                live.pop_back();
            } else {
                m.emplace(next_key, 0);
                live.push_back(next_key++);
            }
        }
        auto t = clk.stop();
        cout << "[std::map] Insert-heavy mix: "
             << t.count / 1'000'000.0 << " ms" << endl;
    }
}


void benchmark_btree_vs_map() {
    using namespace std;
    using namespace xyu;
    using Key = int;
    using Val = int;
    constexpr int N = 1'000'000;          // 测试规模，可根据需求调整
    constexpr int K =   500'000;           // 查找/删除次数（采样）

    // 生成顺序键 [0, N)
    vector<Key> sequential_keys(N);
    iota(sequential_keys.begin(), sequential_keys.end(), 0);

    // 生成随机键（打乱顺序键）
    vector<Key> random_keys = sequential_keys;
    mt19937 rng(42);  // 固定种子保证可复现
    shuffle(random_keys.begin(), random_keys.end(), rng);

    // 生成用于查找和删除的采样键（均匀分布）
    vector<Key> sample_keys(K);
    uniform_int_distribution<Key> dist(0, N-1);
    for (auto& k : sample_keys) k = dist(rng);

    // 辅助打印
    auto print_header = [](const string& title) {
        cout << "\n========== " << title << " (N=" << N << ") ==========" << endl;
        cout << left << setw(20) << "Operation"
             << right << setw(15) << "BTreeMap(us)"
             << setw(15) << "std::map(us)"
             << setw(12) << "Speedup" << endl;
        cout << string(62, '-') << endl;
    };

    auto print_row = [](const string& op, long long bt_ns, long long stl_ns) {
        double bt_us = bt_ns / 1000.0;
        double stl_us = stl_ns / 1000.0;
        double speedup = (bt_ns > 0 && stl_ns > 0) ? (double)stl_ns / bt_ns : 0.0;
        cout << left << setw(20) << op
             << right << setw(15) << bt_us
             << setw(15) << stl_us;
        if (speedup > 0)
            cout << setw(11) << speedup << "x";
        else
            cout << setw(11) << "N/A";
        cout << endl;
    };

    // 用于避免编译器优化掉计算结果
    volatile long long sink = 0;

    // -------------------------------------------------------
    // 1. 顺序插入 (升序键)
    // -------------------------------------------------------
    print_header("Sequential Insertion");
    {
        // BTreeMap: 使用 append 逐元素插入
        BTreeMap<Key, Val, M, cross_thread> bmap;
        Clock clk;
        clk.start();
        for (auto k : sequential_keys)
            bmap.append(k, k * 2);   // 值 = key*2
        auto bt_ns = clk.stop().count;
        assert(bmap.count() == N);
        sink = bmap.range().count(); // 防止优化

        // std::map
        map<Key, Val> smap;
        clk.reset();
        clk.start();
        for (auto k : sequential_keys)
            smap.insert({k, k * 2});
        auto stl_ns = clk.stop().count;
        sink = smap.size();

        print_row("insert (seq)", bt_ns, stl_ns);
    }

    // -------------------------------------------------------
    // 2. 随机插入
    // -------------------------------------------------------
    print_header("Random Insertion");
    {
        BTreeMap<Key, Val, M, cross_thread> bmap;
        Clock clk;
        clk.start();
        for (auto k : random_keys)
            bmap.insert(k, k * 2);   // 唯一键插入（默认 Fast）
        auto bt_ns = clk.stop().count;
        assert(bmap.count() == N);

        map<Key, Val> smap;
        clk.reset();
        clk.start();
        for (auto k : random_keys)
            smap.insert({k, k * 2});
        auto stl_ns = clk.stop().count;
        assert(smap.size() == N);

        print_row("insert (rand)", bt_ns, stl_ns);
    }

    // -------------------------------------------------------
    // 3. 查找 (现有键，采样)
    // -------------------------------------------------------
    print_header("Lookup (existing keys)");
    {
        // 先构建两个容器（随机顺序，保证结构类似）
        BTreeMap<Key, Val, M, cross_thread> bmap;
        for (auto k : random_keys) bmap.insert(k, k * 2);
        map<Key, Val> smap;
        for (auto k : random_keys) smap.insert({k, k * 2});

        // BTreeMap::select
        Clock clk;
        clk.start();
        for (auto k : sample_keys) {
            auto p = bmap.select(k);
            if (p) sink += *p;
        }
        auto bt_ns = clk.stop().count;

        // std::map::find
        clk.reset();
        clk.start();
        for (auto k : sample_keys) {
            auto it = smap.find(k);
            if (it != smap.end()) sink += it->second;
        }
        auto stl_ns = clk.stop().count;

        print_row("find (sample)", bt_ns, stl_ns);
    }

    // -------------------------------------------------------
    // 4. 范围遍历 (全量读取值)
    // -------------------------------------------------------
    print_header("Full Traversal");
    {
        BTreeMap<Key, Val, M, cross_thread> bmap;
        for (auto k : sequential_keys) bmap.append(k, k * 2);
        map<Key, Val> smap;
        for (auto k : sequential_keys) smap.insert({k, k * 2});

        // BTreeMap: 遍历 vrange (返回值范围)
        Clock clk;
        clk.start();
        for (auto& val : bmap.range<bmap.OnlyVal>())
            sink += val;
        auto bt_ns = clk.stop().count;

        // std::map: 迭代器遍历
        clk.reset();
        clk.start();
        for (const auto& [k, v] : smap)
            sink += v;
        auto stl_ns = clk.stop().count;

        print_row("iterate values", bt_ns, stl_ns);
    }

    // -------------------------------------------------------
    // 5. 更新 (对所有键执行 update)
    // -------------------------------------------------------
    print_header("Update (all keys)");
    {
        BTreeMap<Key, Val, M, cross_thread> bmap;
        for (auto k : sequential_keys) bmap.append(k, k * 2);
        map<Key, Val> smap;
        for (auto k : sequential_keys) smap.insert({k, k * 2});

        // BTreeMap::update (键存在，执行修改)
        Clock clk;
        clk.start();
        for (auto k : sequential_keys)
            bmap.update(k, k * 3);   // 值改为 k*3
        auto bt_ns = clk.stop().count;

        // std::map: operator[] 或者 insert_or_assign
        clk.reset();
        clk.start();
        for (auto k : sequential_keys)
            smap.insert_or_assign(k, k + 3);
        auto stl_ns = clk.stop().count;

        print_row("update (seq)", bt_ns, stl_ns);
    }

    // 5. 更新 (对所有键执行 update)
    // -------------------------------------------------------
    print_header("random Update (all keys)");
    {
        BTreeMap<Key, Val, M, cross_thread> bmap;
        for (auto k : sequential_keys) bmap.append(k, k * 2);
        map<Key, Val> smap;
        for (auto k : sequential_keys) smap.insert({k, k * 2});

        // BTreeMap::update (键存在，执行修改)
        Clock clk;
        clk.start();
        for (auto k : random_keys)
            bmap.update(k, k * 3);   // 值改为 k*3
        auto bt_ns = clk.stop().count;

        // std::map: operator[] 或者 insert_or_assign
        clk.reset();
        clk.start();
        for (auto k : random_keys)
            smap.insert_or_assign(k, k + 3);
        auto stl_ns = clk.stop().count;

        print_row("random update (seq)", bt_ns, stl_ns);
    }

    // -------------------------------------------------------
    // 6. 删除 (采样键)
    // -------------------------------------------------------
    print_header("Deletion (sample keys)");
    {
        BTreeMap<Key, Val, M, cross_thread> bmap;
        for (auto k : random_keys) bmap.insert(k, k * 2);
        map<Key, Val> smap;
        for (auto k : random_keys) smap.insert({k, k * 2});

        // BTreeMap::erase (Fast 模式)
        Clock clk;
        clk.start();
        for (auto k : sample_keys)
            bmap.erase(k);
        auto bt_ns = clk.stop().count;

        // std::map::erase
        clk.reset();
        clk.start();
        for (auto k : sample_keys)
            smap.erase(k);
        auto stl_ns = clk.stop().count;

        print_row("erase (sample)", bt_ns, stl_ns);
    }

    // 防止 sink 被优化
    cout << "\n[volatile sink = " << sink << "]" << endl;
}

void benchmark_btree_vs_multimap() {
    using namespace std;
    using namespace xyu;
    using Key = int;
    using Val = int;

    constexpr int N_DISTINCT = 100'000;   // 不同键的数量
    constexpr int REPEATS = 10;          // 每个键重复插入次数
    constexpr int TOTAL = N_DISTINCT * REPEATS;  // 总元素数
    constexpr int SAMPLE = 500'000;        // 查找/删除采样键数

    // 生成顺序重复键序列：0,0,0,... 1,1,1,...  ...
    vector<Key> sequential_keys;
    sequential_keys.reserve(TOTAL);
    for (int k = 0; k < N_DISTINCT; ++k) {
        for (int r = 0; r < REPEATS; ++r) {
            sequential_keys.push_back(k);
        }
    }

    // 生成随机重复键序列（随机选择键，但保持总元素数相同）
    vector<Key> random_keys;
    random_keys.reserve(TOTAL);
    mt19937 rng(42);
    uniform_int_distribution<Key> dist_key(0, N_DISTINCT - 1);
    for (int i = 0; i < TOTAL; ++i) {
        random_keys.push_back(dist_key(rng));
    }

    // 采样键用于查找和删除
    vector<Key> sample_keys(SAMPLE);
    for (auto& k : sample_keys) k = dist_key(rng);

    // 格式化输出辅助
    auto print_header = [](const string& title) {
        cout << "\n========== " << title << " (total=" << TOTAL << ") ==========" << endl;
        cout << left << setw(28) << "Operation"
             << right << setw(15) << "BTreeMap(us)"
             << setw(15) << "std::multimap(us)"
             << setw(12) << "Speedup" << endl;
        cout << string(70, '-') << endl;
    };

    auto print_row = [](const string& op, long long bt_ns, long long stl_ns) {
        double bt_us = bt_ns / 1000.0;
        double stl_us = stl_ns / 1000.0;
        double speedup = (bt_ns > 0 && stl_ns > 0) ? (double)stl_ns / bt_ns : 0.0;
        cout << left << setw(28) << op
             << right << setw(15) << bt_us
             << setw(15) << stl_us;
        if (speedup > 0)
            cout << setw(11) << speedup << "x";
        else
            cout << setw(11) << "N/A";
        cout << endl;
    };

    volatile long long sink = 0;

    // -------------------------------------------------------
    // 1. 顺序插入重复键 (升序键，每个键重复多次)
    // -------------------------------------------------------
    print_header("Sequential Insertion (duplicates)");
    {
        // BTreeMap: 使用 append 保留重复键（插入顺序为追加到末尾）
        BTreeMap<Key, Val, M, cross_thread> bmap;
        Clock clk;
        clk.start();
        for (auto k : sequential_keys)
            bmap.append(k, k * 2);   // 值 = key*2
        auto bt_ns = clk.stop().count;
        assert(bmap.count() == TOTAL);

        // std::multimap
        multimap<Key, Val> mmap;
        clk.reset();
        clk.start();
        for (auto k : sequential_keys)
            mmap.insert({k, k * 2});
        auto stl_ns = clk.stop().count;
        assert(mmap.size() == TOTAL);

        print_row("append / insert (seq dup)", bt_ns, stl_ns);
    }

    // -------------------------------------------------------
    // 2. 随机插入重复键
    // -------------------------------------------------------
    print_header("Random Insertion (duplicates)");
    {
        BTreeMap<Key, Val, M, cross_thread> bmap;
        Clock clk;
        clk.start();
        for (auto k : random_keys)
            bmap.append(k, k * 2);   // 重复键，使用 append 保留所有副本
        auto bt_ns = clk.stop().count;
        assert(bmap.count() == TOTAL);

        multimap<Key, Val> mmap;
        clk.reset();
        clk.start();
        for (auto k : random_keys)
            mmap.insert({k, k * 2});
        auto stl_ns = clk.stop().count;
        assert(mmap.size() == TOTAL);

        print_row("append / insert (rand dup)", bt_ns, stl_ns);
    }

    // -------------------------------------------------------
    // 3. 查找某个键的所有值 (select_all vs equal_range)
    // -------------------------------------------------------
    print_header("Lookup all values by key");
    {
        // 预建容器（使用随机重复键构造，结构多样）
        BTreeMap<Key, Val, M, cross_thread> bmap;
        for (auto k : random_keys) bmap.append(k, k * 2);
        multimap<Key, Val> mmap;
        for (auto k : random_keys) mmap.insert({k, k * 2});

        // BTreeMap::select_all 返回一个范围，遍历它
        Clock clk;
        clk.start();
        for (auto key : sample_keys) {
            auto rg = bmap.select_all(key);
            for (auto& val : rg) sink += val;
        }
        auto bt_ns = clk.stop().count;

        // std::multimap::equal_range
        clk.reset();
        clk.start();
        for (auto key : sample_keys) {
            auto [begin, end] = mmap.equal_range(key);
            for (auto it = begin; it != end; ++it)
                sink += it->second;
        }
        auto stl_ns = clk.stop().count;

        print_row("select_all / equal_range", bt_ns, stl_ns);
    }

    // -------------------------------------------------------
    // 4. 全量遍历所有值
    // -------------------------------------------------------
    print_header("Full traversal of all values");
    {
        BTreeMap<Key, Val, M, cross_thread> bmap;
        for (auto k : sequential_keys) bmap.append(k, k * 2);
        multimap<Key, Val> mmap;
        for (auto k : sequential_keys) mmap.insert({k, k * 2});

        // BTreeMap: 遍历值范围
        Clock clk;
        clk.start();
        for (auto& val : bmap.range<bmap.OnlyVal>())
            sink += val;
        auto bt_ns = clk.stop().count;

        // multimap: 迭代器遍历
        clk.reset();
        clk.start();
        for (const auto& [k, v] : mmap)
            sink += v;
        auto stl_ns = clk.stop().count;

        print_row("iterate values", bt_ns, stl_ns);
    }

    // -------------------------------------------------------
    // 5. 删除某个键的所有副本 (erase_all vs erase(key))
    // -------------------------------------------------------
    print_header("Erase all copies of a key");
    {
        BTreeMap<Key, Val, M, cross_thread> bmap;
        for (auto k : random_keys) bmap.append(k, k * 2);
        multimap<Key, Val> mmap;
        for (auto k : random_keys) mmap.insert({k, k * 2});

        // BTreeMap::erase_all 删除所有匹配键
        Clock clk;
        clk.start();
        for (auto key : sample_keys)
            bmap.erase_all(key);
        auto bt_ns = clk.stop().count;

        // multimap::erase(key) 同样删除所有匹配键
        clk.reset();
        clk.start();
        for (auto key : sample_keys)
            mmap.erase(key);
        auto stl_ns = clk.stop().count;

        print_row("erase_all / erase(key)", bt_ns, stl_ns);
    }

    cout << "\n[volatile sink = " << sink << "]" << endl;
}

inline std::string make_key(int i) {
    char buf[32];
    snprintf(buf, sizeof(buf), "key%06d", i);
    return std::string(buf);
}

// 生成对应的值："val_" + 原键
inline std::string make_val(const std::string& key) {
    return "val_" + key;
}

// 预先构造 N 个字符串键和值，避免构造开销干扰计时
static std::vector<std::string> build_str_keys(int N) {
    std::vector<std::string> keys;
    keys.reserve(N);
    for (int i = 0; i < N; ++i)
        keys.push_back(make_key(i));
    return keys;
}

static std::vector<std::string> build_str_vals(const std::vector<std::string>& keys) {
    std::vector<std::string> vals;
    vals.reserve(keys.size());
    for (const auto& k : keys)
        vals.push_back(make_val(k));
    return vals;
}

/* ==================== BTreeMap vs std::map（字符串） ==================== */
void benchmark_btree_vs_map_string() {
    using namespace std;
    using namespace xyu;

    constexpr int N = 500'000;        // 不同键的数量
    constexpr int SAMPLE = 25'000;    // 查找/删除采样键数

    // 预先生成键和值
    auto sequential_keys = build_str_keys(N);
    auto sequential_vals = build_str_vals(sequential_keys);

    // 随机打乱键序，用于随机插入测试
    vector<string> random_keys = sequential_keys;
    {
        mt19937 rng(42);
        shuffle(random_keys.begin(), random_keys.end(), rng);
    }

    // 构造采样键（随机选择现有键）
    vector<string> sample_keys(SAMPLE);
    {
        mt19937 rng(42);
        uniform_int_distribution<int> dist(0, N - 1);
        for (auto& k : sample_keys)
            k = sequential_keys[dist(rng)];
    }

    volatile long long sink = 0;  // 防优化

    auto print_header = [](const string& title) {
        cout << "\n========== " << title << " (N=" << N << ") ==========" << endl;
        cout << left << setw(28) << "Operation"
             << right << setw(15) << "BTreeMap(us)"
             << setw(15) << "std::map(us)"
             << setw(12) << "Speedup" << endl;
        cout << string(70, '-') << endl;
    };

    auto print_row = [](const string& op, long long bt_ns, long long stl_ns) {
        double bt_us = bt_ns / 1000.0;
        double stl_us = stl_ns / 1000.0;
        double speedup = (bt_ns > 0 && stl_ns > 0) ? (double)stl_ns / bt_ns : 0.0;
        cout << left << setw(28) << op
             << right << setw(15) << bt_us
             << setw(15) << stl_us
             << setw(11) << speedup << (speedup > 0 ? "x" : "") << endl;
    };

    // ---- 1. 顺序插入 ----
    print_header("Sequential Insertion (string)");
    {
        BTreeMap<string, string> bmap;
        Clock clk;
        clk.start();
        for (int i = 0; i < N; ++i)
            bmap.append(sequential_keys[i], sequential_vals[i]);
        auto bt_ns = clk.stop().count;
        assert(bmap.count() == N);

        map<string, string> smap;
        clk.reset();
        clk.start();
        for (int i = 0; i < N; ++i)
            smap.insert({sequential_keys[i], sequential_vals[i]});
        auto stl_ns = clk.stop().count;

        print_row("append / insert (seq)", bt_ns, stl_ns);
    }

    // ---- 2. 随机插入 ----
    print_header("Random Insertion (string)");
    {
        BTreeMap<string, string> bmap;
        Clock clk;
        clk.start();
        for (const auto& k : random_keys)
            bmap.insert(k, make_val(k));   // 值统一构造
        auto bt_ns = clk.stop().count;
        assert(bmap.count() == N);

        map<string, string> smap;
        clk.reset();
        clk.start();
        for (const auto& k : random_keys)
            smap.insert({k, make_val(k)});
        auto stl_ns = clk.stop().count;

        print_row("insert (rand)", bt_ns, stl_ns);
    }

    // ---- 3. 查找 (采样) ----
    print_header("Lookup (existing keys, string)");
    {
        // 构建相同结构容器
        BTreeMap<string, string> bmap;
        for (const auto& k : random_keys) bmap.insert(k, make_val(k));
        map<string, string> smap;
        for (const auto& k : random_keys) smap.insert({k, make_val(k)});

        Clock clk;
        clk.start();
        for (const auto& k : sample_keys) {
            auto p = bmap.select(k);
            if (p) sink += (*p).size();  // 使用值防止优化
        }
        auto bt_ns = clk.stop().count;

        clk.reset();
        clk.start();
        for (const auto& k : sample_keys) {
            auto it = smap.find(k);
            if (it != smap.end()) sink += it->second.size();
        }
        auto stl_ns = clk.stop().count;

        print_row("select / find", bt_ns, stl_ns);
    }

    // ---- 4. 全量遍历 ----
    print_header("Full traversal (string)");
    {
        BTreeMap<string, string> bmap;
        for (int i = 0; i < N; ++i) bmap.append(sequential_keys[i], sequential_vals[i]);
        map<string, string> smap;
        for (int i = 0; i < N; ++i) smap.insert({sequential_keys[i], sequential_vals[i]});

        Clock clk;
        clk.start();
        for (auto& val : bmap.range<bmap.OnlyVal>())
            sink += val.size();
        auto bt_ns = clk.stop().count;

        clk.reset();
        clk.start();
        for (const auto& [k, v] : smap)
            sink += v.size();
        auto stl_ns = clk.stop().count;

        print_row("iterate values", bt_ns, stl_ns);
    }

    // ---- 5. 更新 (覆盖值) ----
    print_header("Update (all keys, string)");
    {
        BTreeMap<string, string> bmap;
        for (int i = 0; i < N; ++i) bmap.append(sequential_keys[i], sequential_vals[i]);
        map<string, string> smap;
        for (int i = 0; i < N; ++i) smap.insert({sequential_keys[i], sequential_vals[i]});

        // 准备新值
        vector<string> new_vals;
        for (const auto& k : sequential_keys)
            new_vals.push_back("new_" + k);

        Clock clk;
        clk.start();
        for (int i = 0; i < N; ++i)
            bmap.update(sequential_keys[i], new_vals[i]);  // update 允许键不存在时插入，这里都存在
        auto bt_ns = clk.stop().count;

        clk.reset();
        clk.start();
        for (int i = 0; i < N; ++i)
            smap[sequential_keys[i]] = new_vals[i];        // operator[] 会查找或插入，这里都存在
        auto stl_ns = clk.stop().count;

        print_row("update / operator[]", bt_ns, stl_ns);
    }

    // ---- 6. 删除 (采样键) ----
    print_header("Deletion (sample keys, string)");
    {
        BTreeMap<string, string> bmap;
        for (const auto& k : random_keys) bmap.insert(k, make_val(k));
        map<string, string> smap;
        for (const auto& k : random_keys) smap.insert({k, make_val(k)});

        Clock clk;
        clk.start();
        for (const auto& k : sample_keys)
            bmap.erase(k);
        auto bt_ns = clk.stop().count;

        clk.reset();
        clk.start();
        for (const auto& k : sample_keys)
            smap.erase(k);
        auto stl_ns = clk.stop().count;

        print_row("erase (sample)", bt_ns, stl_ns);
    }

    cout << "\n[volatile sink = " << sink << "]" << endl;
}

/* ==================== BTreeMap vs std::multimap（字符串） ==================== */
void benchmark_btree_vs_multimap_string() {
    using namespace std;
    using namespace xyu;

    constexpr int N_DISTINCT = 50'000;   // 不同键数
    constexpr int REPEATS = 10;          // 每个键重复次数
    constexpr int TOTAL = N_DISTINCT * REPEATS;
    constexpr int SAMPLE = 25'000;        // 查找/删除采样键数

    // 基础键（不同字符串）
    auto base_keys = build_str_keys(N_DISTINCT);
    vector<string> base_vals;
    for (const auto& k : base_keys)
        base_vals.push_back(make_val(k));

    // 构造顺序重复键序列：每个键重复 REPEATS 次
    vector<string> sequential_keys;
    sequential_keys.reserve(TOTAL);
    for (int i = 0; i < N_DISTINCT; ++i) {
        for (int r = 0; r < REPEATS; ++r) {
            sequential_keys.push_back(base_keys[i]);
        }
    }

    // 随机重复键序列（随机挑选键，总元素数相同）
    vector<string> random_keys;
    random_keys.reserve(TOTAL);
    {
        mt19937 rng(42);
        uniform_int_distribution<int> dist(0, N_DISTINCT - 1);
        for (int i = 0; i < TOTAL; ++i)
            random_keys.push_back(base_keys[dist(rng)]);
    }

    // 采样键
    vector<string> sample_keys(SAMPLE);
    {
        mt19937 rng(42);
        uniform_int_distribution<int> dist(0, N_DISTINCT - 1);
        for (auto& k : sample_keys)
            k = base_keys[dist(rng)];
    }

    volatile long long sink = 0;

    auto print_header = [](const string& title) {
        cout << "\n========== " << title << " (total=" << TOTAL << ") ==========" << endl;
        cout << left << setw(28) << "Operation"
             << right << setw(15) << "BTreeMap(us)"
             << setw(15) << "std::multimap(us)"
             << setw(12) << "Speedup" << endl;
        cout << string(70, '-') << endl;
    };

    auto print_row = [](const string& op, long long bt_ns, long long stl_ns) {
        double bt_us = bt_ns / 1000.0;
        double stl_us = stl_ns / 1000.0;
        double speedup = (bt_ns > 0 && stl_ns > 0) ? (double)stl_ns / bt_ns : 0.0;
        cout << left << setw(28) << op
             << right << setw(15) << bt_us
             << setw(15) << stl_us
             << setw(11) << speedup << (speedup > 0 ? "x" : "") << endl;
    };

    // ---- 1. 顺序插入重复键 ----
    print_header("Sequential Insertion (dup string)");
    {
        BTreeMap<string, string> bmap;
        Clock clk;
        clk.start();
        for (const auto& k : sequential_keys)
            bmap.append(k, make_val(k));   // 重复键，append 保留所有
        auto bt_ns = clk.stop().count;
        assert(bmap.count() == TOTAL);

        multimap<string, string> mmap;
        clk.reset();
        clk.start();
        for (const auto& k : sequential_keys)
            mmap.insert({k, make_val(k)});
        auto stl_ns = clk.stop().count;
        assert(mmap.size() == TOTAL);

        print_row("append / insert (seq dup)", bt_ns, stl_ns);
    }

    // ---- 2. 随机插入重复键 ----
    print_header("Random Insertion (dup string)");
    {
        BTreeMap<string, string> bmap;
        Clock clk;
        clk.start();
        for (const auto& k : random_keys)
            bmap.append(k, make_val(k));
        auto bt_ns = clk.stop().count;
        assert(bmap.count() == TOTAL);

        multimap<string, string> mmap;
        clk.reset();
        clk.start();
        for (const auto& k : random_keys)
            mmap.insert({k, make_val(k)});
        auto stl_ns = clk.stop().count;
        assert(mmap.size() == TOTAL);

        print_row("append / insert (rand dup)", bt_ns, stl_ns);
    }

    // ---- 3. 查找某个键的所有值 ----
    print_header("Lookup all values by key (string)");
    {
        BTreeMap<string, string> bmap;
        for (const auto& k : random_keys) bmap.append(k, make_val(k));
        multimap<string, string> mmap;
        for (const auto& k : random_keys) mmap.insert({k, make_val(k)});

        Clock clk;
        clk.start();
        for (const auto& key : sample_keys) {
            auto rg = bmap.select_all(key);
            for (const auto& val : rg)
                sink += val.size();
        }
        auto bt_ns = clk.stop().count;

        clk.reset();
        clk.start();
        for (const auto& key : sample_keys) {
            auto [beg, end] = mmap.equal_range(key);
            for (auto it = beg; it != end; ++it)
                sink += it->second.size();
        }
        auto stl_ns = clk.stop().count;

        print_row("select_all / equal_range", bt_ns, stl_ns);
    }

    // ---- 4. 全量遍历值 ----
    print_header("Full traversal (dup string)");
    {
        BTreeMap<string, string> bmap;
        for (const auto& k : sequential_keys) bmap.append(k, make_val(k));
        multimap<string, string> mmap;
        for (const auto& k : sequential_keys) mmap.insert({k, make_val(k)});

        Clock clk;
        clk.start();
        for (const auto& val : bmap.range<bmap.OnlyVal>())
            sink += val.size();
        auto bt_ns = clk.stop().count;

        clk.reset();
        clk.start();
        for (const auto& [k, v] : mmap)
            sink += v.size();
        auto stl_ns = clk.stop().count;

        print_row("iterate values", bt_ns, stl_ns);
    }

    // ---- 5. 删除某个键的所有副本 ----
    print_header("Erase all copies of a key (string)");
    {
        BTreeMap<string, string> bmap;
        for (const auto& k : random_keys) bmap.append(k, make_val(k));
        multimap<string, string> mmap;
        for (const auto& k : random_keys) mmap.insert({k, make_val(k)});

        Clock clk;
        clk.start();
        for (const auto& key : sample_keys)
            bmap.erase_all(key);
        auto bt_ns = clk.stop().count;

        clk.reset();
        clk.start();
        for (const auto& key : sample_keys)
            mmap.erase(key);
        auto stl_ns = clk.stop().count;

        print_row("erase_all / erase(key)", bt_ns, stl_ns);
    }

    cout << "\n[volatile sink = " << sink << "]" << endl;
}



inline auto print_structure = [](auto& tree, auto& os) {
    if (!tree.root) {
        os << "(empty tree)\n";
        return;
    }
    os << "tree: \n";

    // 用队列做层序遍历
    using Tree = xyu::t_rem_cr<decltype(tree)>;
    using Node = typename Tree::Base::Node;
    using Mid = typename Tree::Base::Mid;
    struct Item { Node* node; int level; };
    xyu::NearQueue<Item> queue{64};
    queue.push(xyu::special_v, tree.root, 0);

    int cur_level = -1;
    while (queue.count() > 0) {
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
            os << ':';
            os << node->vals[i];
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
    os << "ln = " << (void*)tree.ln << "\n";
    os << "rn = " << (void*)tree.rn << "\n";
    os << "\n";
    os << "\n";
};

inline auto print_structure2 = [](auto& tree, auto& os) {
    if (!tree.root) {
        os << "(empty tree)\n";
        return;
    }
    os << "tree: \n";

    // 用队列做层序遍历
    using Tree = xyu::t_rem_cr<decltype(tree)>;
    using Node = typename Tree::Base::Node;
    using Mid = typename Tree::Base::Mid;
    struct Item { Node* node; int level; };
    xyu::NearQueue<Item> queue{64};
    queue.push(xyu::special_v, tree.root, 0);

    int cur_level = -1;
    while (queue.count() > 0) {
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
        os << "k=[";
        for (xyu::uint32 i = 0; i < node->count; ++i) {
            if (i) os << ",";
            os << node->keys[i];
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
    os << "ln = " << (void*)tree.ln << "\n";
    os << "rn = " << (void*)tree.rn << "\n";
    os << "\n";
    os << "\n";
};
