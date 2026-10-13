#pragma once

#include <cassert>
#include <string>
#include <utility>
#include <vector>
#include <iostream>
#include "../detail/xycoll/hash.h"

using namespace xyu;

// 辅助：构造一个 pair 列表方便传参
template<typename K, typename V>
std::vector<std::pair<K, V>> make_pairs(std::initializer_list<std::pair<K, V>> il) {
    return il;
}

// ------------------------------------------------------------
// 测试 HashMap<int, std::string>
// ------------------------------------------------------------
void test_hashmap_basic_operations() {
    // 1. 默认构造
    HashMap<int, String> m0;
    assert(m0.empty());
    assert(m0.count() == 0);
    assert(m0.capacity() == 0);

    // 2. 预分配构造
    HashMap<int, String> m1(10);
    assert(m1.capacity() >= 10);
    assert(m1.empty());

    // 3. reserve（扩容）
    m1.reserve(50);
    assert(m1.capacity() >= 50);

    // 4. append（自行保证键不重复）
    m1.append(1, "one");
    m1.append(2, "two");
    m1.append(3, "three");
    assert(m1.count() == 3);

    // 5. select（查找）
    auto* p = m1.select(2);
    assert(p != nullptr && *p == "two");
    p = m1.select(10);
    assert(p == nullptr);

    const auto& cm1 = m1;
    const auto* cp = cm1.select(1);
    assert(cp != nullptr && *cp == "one");

    // 6. insert（不存在则插入，存在则返回原值）
    auto& v4 = m1.insert(4, "four");
    assert(v4 == "four");
    assert(m1.count() == 4);

    auto& v2_exist = m1.insert(2, "TWO");   // 键已存在，返回旧值
    assert(v2_exist == "two");
    assert(m1.count() == 4);                // 数量不变

    // 7. update（存在则更新，否则插入）
    auto& v3 = m1.update(3, "THREE");        // 更新
    assert(v3 == "THREE");
    auto& v5 = m1.update(5, "five");         // 插入
    assert(v5 == "five");
    assert(m1.count() == 5);

    // 8. operator[]（访问，不存在则插入默认值）
    auto& v6 = m1[6];
    assert(v6 == "");                        // 默认构造的 string
    v6 = "six";
    assert(*m1.select(6) == "six");
    assert(m1.count() == 6);

    // 9. 范围 insert（使用初始化列表）
    m1.insert({{7, "seven"}, {8, "eight"}}); // 全是新键
    assert(m1.count() == 8);
    assert(*m1.select(7) == "seven");

    // 键 7,8 已存在，再次 insert 不会改变
    m1.insert({{7, "SEVEN"}, {8, "EIGHT"}});
    assert(*m1.select(7) == "seven");
    assert(m1.count() == 8);

    // 10. 范围 update（存在即更新，否则插入）
    m1.update({{7, "SEVEN"}, {9, "nine"}});
    assert(*m1.select(7) == "SEVEN");
    assert(*m1.select(9) == "nine");
    assert(m1.count() == 9);

    // 11. append 初始化列表（自行保证无重复）
    m1.append({{10, "ten"}, {11, "eleven"}});
    assert(m1.count() == 11);
    assert(*m1.select(10) == "ten");

    // 12. 双范围操作（键范围 + 值范围）
    std::vector<int> keys = {12, 13};
    std::vector<std::string> vals = {"twelve", "thirteen"};
    m1.append(keys, vals);           // append 双范围
    assert(m1.count() == 13);
    assert(*m1.select(12) == "twelve");

    m1.insert(keys, vals);           // insert 双范围（键已存在，无影响）
    assert(m1.count() == 13);

    m1.update(keys, vals);           // update 双范围（更新值）
    assert(*m1.select(12) == "twelve");

    // 13. erase 单键
    bool erased = m1.erase(13);
    assert(erased);
    assert(m1.count() == 12);
    erased = m1.erase(100);          // 不存在的键
    assert(!erased);

    // erase 键范围（vector）
    m1.erase(std::vector<int>{10, 11});
    assert(m1.count() == 10);

    // erase 键范围（初始化列表）
    m1.erase({8, 9});
    assert(m1.count() == 8);

    // 14. 遍历
    // range() 返回节点范围，节点有 key 和 val
    int sum_keys = 0;
    for (auto& node : m1.range()) {
        sum_keys += node.key;
    }
    // 当前剩余键：1,2,3,4,5,6,7,12  (注意3已改为THREE，但不影响求和)
    assert(sum_keys == 1+2+3+4+5+6+7+12); // 40

    sum_keys = 0;
    for (const auto& node : std::as_const(m1).range()) {
        sum_keys += node.key;
    }
    assert(sum_keys == 40);

    // krange() 返回键范围
    sum_keys = 0;
    for (auto k : m1.krange()) {
        sum_keys += k;
    }
    assert(sum_keys == 40);

    // vrange() 返回值范围
    size_t total_len = 0;
    for (auto& v : m1.vrange()) {
        total_len += v.size();
    }
    for (const auto& v : std::as_const(m1).vrange()) {
        total_len += v.size();
    }
    (void)total_len; // 仅验证编译

    // 15. clear / release
    m1.clear();
    assert(m1.empty());
    assert(m1.capacity() != 0);   // 内存未释放
    m1.release();
    assert(m1.capacity() == 0);

    // 16. 拷贝构造与赋值
    HashMap<int, std::string> m2(5);
    m2.insert(100, "hundred");
    HashMap<int, std::string> m3(m2);
    assert(m3.count() == 1 && *m3.select(100) == "hundred");

    HashMap<int, std::string> m4 = m2;
    assert(m4.count() == 1);

    // 17. 移动构造与赋值
    HashMap<int, std::string> m5(std::move(m2));
    assert(m5.count() == 1 && m2.empty());

    HashMap<int, std::string> m6;
    m6 = std::move(m5);
    assert(m6.count() == 1 && m5.empty());

    // 18. swap
    HashMap<int, std::string> m7;
    m7.insert(200, "two hundred");
    m6.swap(m7);
    assert(m6.select(200) && m7.select(100));

    // 19. reduce（缩容）
    m6.erase(200);
    m6.reduce();
    assert(m6.count() == 0);

    // 20. 范围构造（双范围）
    std::vector<int> ks = {30, 31};
    std::vector<std::string> vs = {"thirty", "thirtyone"};
    HashMap<int, std::string> m8(ks, vs);          // 双范围构造
    assert(m8.count() == 2 && *m8.select(30) == "thirty");

    // 21. limit
    [[maybe_unused]] auto lim = HashMap<int, std::string>::limit();
    assert(lim > 0);
}

// ------------------------------------------------------------
// 测试 HashSet<int>
// ------------------------------------------------------------
void test_hashset_basic_operations() {
    // 默认构造
    HashSet<int> s0;
    assert(s0.empty());

    // 预分配构造
    HashSet<int> s1(10);
    assert(s1.capacity() >= 10);
    assert(s1.empty());

    // reserve
    s1.reserve(20);
    assert(s1.capacity() >= 20);

    // append 单元素（自行保证不重复）
    s1.append(1);
    s1.append(2);
    s1.append(3);
    assert(s1.count() == 3);

    // select（返回 bool）
    assert(s1.select(2) == true);
    assert(s1.select(5) == false);
    assert(std::as_const(s1).select(1) == true);

    // insert（不存在则插入）
    s1.insert(4);
    assert(s1.count() == 4);
    s1.insert(2);   // 已存在，无效果
    assert(s1.count() == 4);

    // 范围 insert（初始化列表）
    s1.insert({5, 6});
    assert(s1.count() == 6);
    s1.insert({2, 3, 5});   // 全部已存在
    assert(s1.count() == 6);

    // 范围 append（自行保证不重复）
    std::vector<int> new_keys = {7, 8};
    s1.append(new_keys);
    assert(s1.count() == 8);

    // erase 单键
    assert(s1.erase(8) == true);
    assert(s1.count() == 7);
    assert(s1.erase(100) == false);

    // erase 范围
    s1.erase(std::vector<int>{6, 7});
    assert(s1.count() == 5);
    s1.erase({5});
    assert(s1.count() == 4);

    // 遍历
    int sum = 0;
    for (int k : s1.range()) {
        sum += k;
    }
    // 剩余：1,2,3,4  => 10
    assert(sum == 10);

    // clear / release
    s1.clear();
    assert(s1.empty());
    s1.release();
    assert(s1.capacity() == 0);

    // 拷贝 / 移动
    HashSet<int> s2(5);
    s2.insert(10);
    HashSet<int> s3(s2);
    assert(s3.select(10));

    HashSet<int> s4 = s2;
    assert(s4.select(10));

    HashSet<int> s5(std::move(s2));
    assert(s5.select(10) && s2.empty());

    HashSet<int> s6;
    s6 = std::move(s5);
    assert(s6.select(10) && s5.empty());

    // swap
    HashSet<int> s7;
    s7.insert(20);
    s6.swap(s7);
    assert(s6.select(20) && s7.select(10));

    // 范围构造（非初始化列表）
    std::vector<int> init = {100, 200, 300};
    HashSet<int> s8(init);
    assert(s8.count() == 3 && s8.select(200));

    // reduce
    s8.erase(100);
    s8.erase(200);
    s8.reduce();
    assert(s8.count() == 1);
}

// ------------------------------------------------------------
// 边界测试：大量插入/删除/缩容
// ------------------------------------------------------------
void test_edge_cases() {
    HashMap<int, int> m;
    for (int i = 0; i < 1000; ++i) {
        m.insert(i, i * 2);
    }
    assert(m.count() == 1000);
    for (int i = 0; i < 1000; ++i) {
        assert(*m.select(i) == i * 2);
    }

    for (int i = 0; i < 500; ++i) {
        m.erase(i);
    }
    assert(m.count() == 500);
    m.reduce();
    for (int i = 500; i < 1000; ++i) {
        assert(*m.select(i) == i * 2);
    }

    HashSet<int> s;
    for (int i = 0; i < 1000; ++i) {
        s.insert(i);
    }
    assert(s.count() == 1000);
    for (int i = 0; i < 500; ++i) {
        s.erase(i);
    }
    s.reduce();
    assert(s.count() == 500);
    for (int i = 500; i < 1000; ++i) {
        assert(s.select(i));
    }
}

// ------------------------------------------------------------
// 4. 测试 string 键
// ------------------------------------------------------------
void test_string_keys() {
    HashMap<String, double> m;
    m.insert("apple", 1.5);
    m.insert("banana", 2.3);
    m.insert("cherry", 3.14);
    assert(m.count() == 3);

    assert(std::abs(*m.select("banana") - 2.3) < 0.001);
    assert(m.select("durian") == nullptr);

    m.update("banana", 2.5);
    assert(std::abs(*m.select("banana") - 2.5) < 0.001);

    // erase
    m.erase("apple");
    assert(m.count() == 2);

    // 范围
    for (auto& node : m.range()) {
        // 简单验证 key 和 value 存在
        assert(node.key == "banana" || node.key == "cherry");
    }

    HashSet<String> s;
    s.insert("red");
    s.insert("green");
    s.insert("blue");
    assert(s.count() == 3);
    assert(s.select("green"));
    assert(!s.select("yellow"));
}

// ------------------------------------------------------------
// 5. 测试 void* 指针键（任意地址）
// ------------------------------------------------------------
void test_voidptr_keys() {
    int a = 1, b = 2, c = 3;
    void* p1 = &a;
    void* p2 = &b;
    void* p3 = &c;

    HashMap<void*, int> m;
    m.insert(p1, 100);
    m.insert(p2, 200);
    m.insert(p3, 300);
    assert(m.count() == 3);

    assert(*m.select(p2) == 200);
    assert(m.select(nullptr) == nullptr);  // 未插入 nullptr

    // 更新
    m.update(p2, 250);
    assert(*m.select(p2) == 250);

    // HashSet<void*>
    HashSet<void*> s;
    s.insert(p1);
    s.insert(p2);
    assert(s.count() == 2);
    assert(s.select(p1));
    assert(!s.select(nullptr));
}

// ---------- 自定义碰撞键 ----------
struct CollisionKey {
    int val;
    bool operator==(const CollisionKey& other) const { return val == other.val; }
};

// 特化 Hasher，使所有键的哈希值相同，制造极致碰撞
namespace xylu::xymath {
    template <>
    struct Hasher<CollisionKey> {
        constexpr xyu::size_t operator()(const CollisionKey&) const noexcept {
            return 0;   // 全部键落入同一桶
        }
    };
}

// ---------- 哈希碰撞测试 ----------
void test_hash_collision() {
    // --- HashMap 碰撞测试 ---
    HashMap<CollisionKey, int> m;
    constexpr int N = 200;

    // 批量插入
    for (int i = 0; i < N; ++i) {
        m.insert(CollisionKey{i}, i * 10);
    }
    assert(m.count() == N);

    // 查找所有键
    for (int i = 0; i < N; ++i) {
        int* p = m.select(CollisionKey{i});
        assert(p != nullptr && *p == i * 10);
    }

    // 更新中间某个键
    m.update(CollisionKey{50}, 999);
    assert(*m.select(CollisionKey{50}) == 999);

    // 删除前半部分
    for (int i = 0; i < N / 2; ++i) {
        assert(m.erase(CollisionKey{i}));
    }
    assert(m.count() == N - N / 2);

    // 剩余键仍存在
    for (int i = N / 2; i < N; ++i) {
        assert(m.select(CollisionKey{i}) != nullptr);
    }

    // 删除一个不存在的键
    assert(!m.erase(CollisionKey{-1}));

    // 范围遍历
    int sum = 0;
    for (auto& node : m.range()) {
        sum += node.key.val;
        // 值应为 10*val，但50被改为999
        if (node.key.val == 50) assert(node.val == 999);
        else assert(node.val == node.key.val * 10);
    }
    // 计算应有总和（0..99 去掉0..99 的前一半：0-99 总 = 99*100/2 = 4950，去掉0..99的前一半：0..99 total=4950，去掉0..99 前一半？？实际删除0..99的一半，N=200，删除0..99，剩余100..199。总和100..199 = (100+199)*100/2 = 14950。但由于key.val从0开始，删除0..99，剩余100..199，总和 = 100+101+...+199 = (100+199)*100/2 = 14950。但要注意我们构造时key.val = i, 0..199，删除0..99，剩余100..199。所以sum应等于14950。)
    assert(sum == (100 + 199) * 100 / 2);

    // 缩容
    m.reduce();
    assert(m.count() == N - N / 2);

    // 清空
    m.clear();
    assert(m.empty());

    // --- HashSet 碰撞测试 ---
    HashSet<CollisionKey> s;
    for (int i = 0; i < N; ++i) {
        s.insert(CollisionKey{i});
    }
    assert(s.count() == N);

    for (int i = 0; i < N; ++i) {
        assert(s.select(CollisionKey{i}));
    }

    s.erase(CollisionKey{0});
    assert(s.count() == N - 1);
    assert(!s.select(CollisionKey{0}));

    // 遍历
    int cnt = 0;
    for (const auto& key : s.range()) {
        (void)key;
        ++cnt;
    }
    assert(cnt == N - 1);
}

// ---------- 析构计数对象 ----------
struct Tracked {
    static inline int alive = 0;   // 当前存活对象数
    static inline int total_ctor = 0;  // 总构造次数
    static inline int total_dtor = 0;  // 总析构次数

    int value;

    Tracked(int v = 0) : value(v) {
        ++alive;
        ++total_ctor;
    }

    Tracked(const Tracked& other) : value(other.value) {
        ++alive;
        ++total_ctor;
    }

    Tracked(Tracked&& other) noexcept : value(other.value) {
        other.value = 0;
        ++alive;
        ++total_ctor;
    }

    Tracked& operator=(const Tracked& other) {
        if (this != &other) value = other.value;
        return *this;
    }

    Tracked& operator=(Tracked&& other) noexcept {
        value = other.value;
        other.value = 0;
        return *this;
    }

    ~Tracked() {
        --alive;
        ++total_dtor;
    }

    bool operator==(const Tracked& other) const { return value == other.value; }

    static void reset() {
        alive = 0;
        total_ctor = 0;
        total_dtor = 0;
    }
};

namespace xylu::xymath {
    template <>
    struct Hasher<Tracked> {
        constexpr xyu::size_t operator()(const Tracked&) const noexcept { return 0; }
    };
}

struct TrackedInt {
    static inline int alive = 0;
    static inline int ctor = 0;
    static inline int dtor = 0;

    int val;
    TrackedInt(int v = 0) : val(v) { ++alive; ++ctor; }
    TrackedInt(const TrackedInt& o) : val(o.val) { ++alive; ++ctor; }
    TrackedInt(TrackedInt&& o) noexcept : val(o.val) { o.val = 0; ++alive; ++ctor; }
    ~TrackedInt() { --alive; ++dtor; }
    bool operator==(const TrackedInt& other) const { return val == other.val; }

    static void reset() { alive = ctor = dtor = 0; }
};

// 为 TrackedInt 特化 Hasher 制造碰撞
namespace xylu::xymath {
    template <>
    struct Hasher<TrackedInt> {
        constexpr xyu::size_t operator()(const TrackedInt&) const noexcept { return 0; }
    };
}

// ---------- 插删交替 + 析构计数测试 ----------
void test_mixed_insert_delete_and_destruction_tracking() {
    // --- 1. HashMap 测试 ---
    Tracked::reset();
    {
        HashMap<CollisionKey, Tracked> m;
        constexpr int N = 500;

        // 阶段1: 插入 0..N-1
        for (int i = 0; i < N; ++i) {
            m.insert(CollisionKey{i}, Tracked{i * 10});
        }
        assert(m.count() == N);
        assert(Tracked::alive == N);

        // 阶段2: 删除偶数键
        for (int i = 0; i < N; i += 2) {
            bool erased = m.erase(CollisionKey{i});
            assert(erased);
        }
        assert(m.count() == N - N/2);  // 奇数键保留
        assert(Tracked::alive == N - N/2);

        // 阶段3: 重新插入偶数键（但值改为 i*100）
        for (int i = 0; i < N; i += 2) {
            m.insert(CollisionKey{i}, Tracked{i * 100});
        }
        assert(m.count() == N);
        assert(Tracked::alive == N);

        // 阶段4: 随机删除再插入（交替）
        for (int i = 0; i < N; i += 10) {
            m.erase(CollisionKey{i});
            m.insert(CollisionKey{i}, Tracked{i * 1000});
        }
        assert(m.count() == N);

        // 阶段5: 强制扩容后再删除所有
        m.reserve(N * 4);
        for (int i = 0; i < N; ++i) {
            assert(m.select(CollisionKey{i}) != nullptr);
            m.erase(CollisionKey{i});
        }
        assert(m.empty());
        assert(m.count() == 0);
        assert(Tracked::alive == 0);          // 全部析构，无泄漏
    }
    // HashMap 析构后检查全局平衡
    assert(Tracked::total_ctor == Tracked::total_dtor);


    TrackedInt::reset();
    {
        HashSet<TrackedInt> s;
        constexpr int M = 300;

        // 插入
        for (int i = 0; i < M; ++i) s.insert(TrackedInt{i});
        assert(s.count() == M);
        assert(TrackedInt::alive == M);

        // 删除一半
        for (int i = 0; i < M; i += 2) s.erase(TrackedInt{i});
        assert(s.count() == M - M/2);
        assert(TrackedInt::alive == M - M/2);

        // 重新插入奇数（已经存在，insert 无操作）和新的偶数
        for (int i = 0; i < M; i += 2) s.insert(TrackedInt{i});
        assert(s.count() == M);
        assert(TrackedInt::alive == M);

        // 随机交替
        for (int i = 0; i < M; i += 7) {
            s.erase(TrackedInt{i});
            s.insert(TrackedInt{i + 1000}); // 新键
        }

        // 清空
        s.clear();
        assert(s.empty());
        assert(TrackedInt::alive == 0);   // 全部析构

        // 再次插入一些元素，然后销毁容器
        for (int i = 0; i < 100; ++i) s.insert(TrackedInt{i});
        assert(s.count() == 100);
    }
    assert(TrackedInt::alive == 0);
    assert(TrackedInt::ctor == TrackedInt::dtor);

    // 额外测试：operator[] 使用非平凡值类型
    Tracked::reset();
    {
        HashMap<int, Tracked> m2;
        m2[1] = Tracked{100};   // operator[] 先默认构造，再赋值，注意这里会多一次默认构造，析构计数也要匹配
        // 为了简化，我们不测细节，但最终析构应平衡
        m2.insert(2, Tracked{200});
        m2.erase(1);
        m2.clear();
    }
    assert(Tracked::alive == 0);
    assert(Tracked::total_ctor == Tracked::total_dtor);

    std::cout << "Mixed insert/delete and destruction tracking passed!" << std::endl;
}

int main30() {
#define Test( name ) xyout << #name << '\n'; name();

    Test(test_hashmap_basic_operations)
    Test(test_hashset_basic_operations)
    Test(test_edge_cases)
    Test(test_string_keys)
    Test(test_voidptr_keys)
    Test(test_hash_collision)
    Test(test_mixed_insert_delete_and_destruction_tracking)

    std::cout << "All hash table tests passed!" << std::endl;
    return 0;
}
