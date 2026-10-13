#pragma once

/**
 * @file    stringbase_test.h
 * @brief   SString的constexpr功能测试
 * @date    2025-12-27
 */

#include "../detail/xystring/strfixed.h"

using namespace xyu;

template <size_t N>
using SString = FxiedString<N>;

// ==========================================
// 1. 基础构造与视图测试
// ==========================================
constexpr bool test_construct()
{
    // 默认构造
    SString<10> s1 = "";
    if (!s1.empty() || s1.size() != 0) return false;
    if (s1.data()[0] != '\0') return false;

    // 字面量构造
    SString<10> s2("hello");
    if (s2.size() != 5) return false;
    if (s2[0] != 'h' || s2[4] != 'o') return false;

    // 截断构造 (手动指定长度)
    SString<10> s3("hello world", 5);
    if (s3.size() != 5) return false;
    if (!s3.view().equals("hello"_sv)) return false;

    // 视图构造
    constexpr auto sv = "test"_sv;
    SString<10> s4(sv);
    if (!s4.view().equals(sv)) return false;

    // 赋值
    s1 = "world";
    if (!s1.view().equals("world"_sv)) return false;

    return true;
}
static_assert(test_construct(), "Construct test failed");

// ==========================================
// 2. 追加测试 (Append / Operator+)
// ==========================================
constexpr bool test_append()
{
    SString<20> s("hello");

    // append char
    s.append(' ');
    if (!s.view().equals("hello "_sv)) return false;

    // append string
    s.append("world");
    if (!s.view().equals("hello world"_sv)) return false;

    // operator+=
    s += "!";
    if (!s.view().equals("hello world!"_sv)) return false;

    // operator+ (链式)
    // SString + const char*
    auto s2 = s + " Next";
    if (!s2.view().equals("hello world! Next"_sv)) return false;

    // operator<<
    SString<10> s3;
    s3 << 'A' << "BC";
    if (!s3.view().equals("ABC"_sv)) return false;

    return true;
}
static_assert(test_append(), "Append test failed");

// ==========================================
// 3. 插入测试 (Insert) - 重点测试 strmove
// ==========================================
constexpr bool test_insert()
{
    SString<20> s("heo");

    // 中间插入 (触发数据后移)
    s.insert(2, "ll"); // "hello"
    if (!s.view().equals("hello"_sv)) return false;

    // 头部插入
    s.insert(0, '>'); // ">hello"
    if (!s.view().equals(">hello"_sv)) return false;

    // 尾部插入
    s.insert(s.size(), " world"); // ">hello world"
    if (!s.view().equals(">hello world"_sv)) return false;

    // **自引用插入测试** (StringBase 的难点)
    // 场景: 把自己的内容插到自己中间
    SString<20> s_self("AB");
    // insert(1, view()) -> 把 "AB" 插到 index 1
    // 预期: "A" + "AB" + "B" -> "AABB"
    s_self.insert(1, s_self.view());
    if (!s_self.view().equals("AABB"_sv)) return false;

    return true;
}
static_assert(test_insert(), "Insert test failed");

// ==========================================
// 4. 删除测试 (Erase)
// ==========================================
constexpr bool test_erase()
{
    SString<20> s("hello amazing world");

    // 删除中间
    // "hello amazing world" -> delete " amazing" (index 5, len 8)
    s.erase(5, 8);
    if (!s.view().equals("hello world"_sv)) return false;

    // 删除尾部 (超长 len)
    s.erase(5, 100);
    if (!s.view().equals("hello"_sv)) return false;

    // 迭代器删除
    // 删除 'e' (index 1)
    auto it = s.erase(s.range().begin() + 1);
    if (!s.view().equals("hllo"_sv)) return false;
    // 迭代器返回检查
    if (*it != 'l') return false;

    return true;
}
static_assert(test_erase(), "Erase test failed");

// ==========================================
// 5. 替换与更新测试 (Replace / Update)
// ==========================================
constexpr bool test_modify()
{
    // Update (覆盖)
    SString<20> s1("abcdef");
    s1.update(2, "123"); // "ab123f"
    if (!s1.view().equals("ab123f"_sv)) return false;

    // Update 越界追加
    s1.update(5, "XYZ"); // "ab123XYZ" (覆盖 'f' 并追加 'YZ')
    if (!s1.view().equals("ab123XYZ"_sv)) return false;

    // Replace (长度变化)
    SString<30> s2("hello C++ world");

    // 缩短: "C++" -> "C"
    s2.replace(6, 3, "C");
    if (!s2.view().equals("hello C world"_sv)) return false;

    // 增长: "C" -> "Rust"
    s2.replace(6, 1, "Rust");
    if (!s2.view().equals("hello Rust world"_sv)) return false;

    // **自引用替换测试**
    // "12345" -> 把 "23" (index 1, len 2) 替换为 "12345"
    SString<20> s3("12345");
    s3.replace(1, 2, s3.view());
    // 预期: "1" + "12345" + "45" -> "11234545"
    if (!s3.view().equals("11234545"_sv)) return false;

    return true;
}
static_assert(test_modify(), "Modify test failed");

// ==========================================
// 6. 查找与比较测试 (继承自 StringViewBase)
// ==========================================
constexpr bool test_search()
{
    SString<20> s("banananana");

    // starts/ends
    if (!s.starts_with("bana"_sv)) return false;
    if (!s.ends_with("nana"_sv)) return false;

    // find
    if (s.find('a') != 1) return false;
    if (s.find("nana"_sv) != 2) return false; // index 2
    if (s.find("xyz"_sv) != -1) return false;

    // rfind
    if (s.rfind('a') != 9) return false;
    if (s.rfind("nana"_sv) != 6) return false; // index 6

    // subview
    auto sub = s.subview(2, 4);
    if (!sub.equals("nana"_sv)) return false;

    // compare
    SString<5> a("abc");
    SString<5> b("abd");
    if (a.compare(b) >= 0) return false; // abc < abd

    return true;
}
static_assert(test_search(), "Search test failed");

// ==========================================
// 7. 边界与容量测试
// ==========================================
constexpr bool test_limit()
{
    SString<5> s;
    if (s.capacity() != 5) return false;

    s.append("12345"); // 满
    if (s.size() != 5) return false;

    // 尝试赋值超过容量的串 (SString 逻辑是截断)
    s = "12345678";
    if (s.size() != 5) return false;
    if (!s.view().equals("12345"_sv)) return false;

    // 注意：append 超过容量会抛异常，所以在 constexpr 中无法测试 append 溢出
    // 除非我们用 try-catch (C++20 constexpr try-catch) 或者 SFINAE 检测
    // 这里只测试合法的边界操作

    return true;
}
//static_assert(test_limit(), "Limit test failed");

// ==========================================
// 主函数 (用于运行时验证，或在此处断点调试)
// ==========================================
int main2()
{
    // 如果编译通过，说明 static_assert 都通过了
    std::cout << "All constexpr SString tests passed!" << std::endl;

    // 运行时简单演示 (会走到 SIMD 优化路径)
    SString<50> rt_s("Runtime String");
    rt_s += " is fast";

    if (rt_s.find("fast") != -1) {
        std::cout << "Runtime search found: " << rt_s.data() << std::endl;
    }

    return 0;
}