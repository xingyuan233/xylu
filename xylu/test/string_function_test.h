#pragma once

/**
 * @file    string_test.h
 * @brief   String基础功能测试
 * @date    2025-12-29
 */

#include <iostream>
#include "../deprecated/strsf.h"

using namespace xyu;

using String = FString;

// -----------------------------------------------------------------------------
// 测试辅助工具
// -----------------------------------------------------------------------------

#define RESET   "\033[0m"
#define RED     "\033[31m"
#define GREEN   "\033[32m"
#define YELLOW  "\033[33m"
#define BLUE    "\033[34m"

int g_pass = 0;
int g_fail = 0;

#define TEST_CASE(name) \
    std::cout << "\n" << BLUE << "[TEST CASE] " << name << RESET << std::endl;

void check(bool condition, const char* desc, const char* file, int line) {
    if (condition) {
        // std::cout << GREEN << "[PASS] " << desc << RESET << std::endl;
        g_pass++;
    } else {
        std::cout << RED << "[FAIL] " << desc << " at " << file << ":" << line << RESET << std::endl;
        g_fail++;
        exit(1); // 遇到错误直接停止，方便调试
    }
}

#define EXPECT_TRUE(expr) check((expr), #expr, __FILE__, __LINE__)
#define EXPECT_EQ(a, b) check((a) == (b), #a " == " #b, __FILE__, __LINE__)
#define EXPECT_STR(str_obj, expect) check(str_obj.view().equals(expect), "String Content Check", __FILE__, __LINE__)

// 探针函数：打印 String 的内部状态
// 假设 String 内部的 union { s, l, f } 和 KIND 已经是 public
void inspect(const String& str, const char* name) {
    std::cout << YELLOW << "  Inspecting " << name << ": ";

    // 获取 kind (假设 kind 在所有结构体的相同偏移处，读取 s.kind 即可)
    auto kind = str.s.kind;

    std::string kind_str;
    const void* ptr = nullptr;
    size_t capa = 0;

    switch (kind) {
        case String::Small:
            kind_str = "Small (SSO)";
            ptr = str.s.data;
            capa = sizeof(str.s.data) - 1;
            break;
        case String::Large:
            kind_str = "Large (Heap)";
            ptr = str.l.data;
            capa = str.l.capa;
            break;
        case String::Fixed:
            kind_str = "Fixed (Const)";
            ptr = str.f.data;
            capa = 0;
            break;
        default: kind_str = "Unknown"; break;
    }

    std::cout << "[" << kind_str << "] Size=" << str.size()
              << ", Capa=" << capa
              << ", Ptr=" << ptr
              << ", Content=\"" << str.view().data() << "\"" << RESET << std::endl;
}

// -----------------------------------------------------------------------------
// 测试用例
// -----------------------------------------------------------------------------

void test_construction_and_state() {
    TEST_CASE("Construction & Internal States");

    // 1. 默认构造 -> Small
    String s1;
    EXPECT_TRUE(s1.empty());
    EXPECT_EQ(s1.size(), 0);
    EXPECT_EQ(s1.s.kind, String::Small);
    inspect(s1, "s1 (Default)");

    // 2. 字面量构造 -> Fixed
    String s2 = "Hello Fixed"; // 模板推导 T(&)[N]
    EXPECT_EQ(s2.size(), 11);
    EXPECT_EQ(s2.s.kind, String::Fixed);
    // 验证指针是否直接指向代码段（通过比较地址）
    const char* literal_ptr = "Hello Fixed";
    // 注意：编译器可能会合并相同的字符串常量，这里不做强指针检查，只检查类型
    inspect(s2, "s2 (Literal)");

    // 3. 短字符串构造 (运行时) -> Small
    const char* runtime_short = "Hello Small";
    String s3 = String(runtime_short); // 显式调用构造函数，视作 const char*
    EXPECT_EQ(s3.size(), 11);
    EXPECT_EQ(s3.s.kind, String::Small);
    inspect(s3, "s3 (Runtime Short)");

    // 4. 长字符串构造 -> Large
    // 假设 SSO 阈值大概是 23 左右
    const char* runtime_long = "This is a very long string that should bypass SSO optimization";
    String s4 = String(runtime_long);
    EXPECT_TRUE(s4.size() > 24);
    EXPECT_EQ(s4.s.kind, String::Large);
    inspect(s4, "s4 (Runtime Large)");
}

void test_view_base_methods() {
    TEST_CASE("Methods Inherited from StringViewBase");

    String s = "Hello World";

    // Starts/Ends with
    EXPECT_TRUE(s.starts_with('H'));
    EXPECT_TRUE(s.starts_with("Hello"));
    EXPECT_TRUE(s.ends_with('d'));
    EXPECT_TRUE(s.ends_with("World"));
    EXPECT_TRUE(!s.starts_with("X"));

    // Find
    EXPECT_EQ(s.find('W'), 6);
    EXPECT_EQ(s.find("or"), 7);
    EXPECT_EQ(s.find("XYZ"), -1);

    // Subview -> String
    String sub = s.subview(0, 5); // "Hello"
    EXPECT_STR(sub, "Hello");

    // Compare
    String s2 = "Hello World";
    EXPECT_TRUE(s.equals(s2));
    EXPECT_EQ(s.compare(s2), 0);

    String s3 = "Hello Aorld";
    EXPECT_TRUE(s.compare(s3) > 0); // 'W' > 'A'
}

void test_modification_and_cow() {
    TEST_CASE("Modification & Fixed String COW");

    // 初始化为 Fixed
    String s = "Fixed String";
    EXPECT_EQ(s.s.kind, String::Fixed);
    const void* original_ptr = make_const(s).data(); // 获取只读指针
    inspect(s, "s (Fixed)");

    // 1. 修改触发 COW (Fixed -> Small/Large)
    // append 一个字符
    s.append('!');
    inspect(s, "s (After Append)");

    EXPECT_STR(s, "Fixed String!");
    EXPECT_TRUE(s.s.kind != String::Fixed); // 应该变成了 Small 或 Large
    EXPECT_TRUE(s.data() != original_ptr);  // 指针应该变了

    // 2. Insert
    s.insert(0, "Start: ");
    EXPECT_STR(s, "Start: Fixed String!");
    inspect(s, "s (After Insert)");

    // 3. update
    // "Start: Fixed String!" -> update "Fixed" with "Mutable"
    // "Start: " len=7. index=7
    s.update(7, 5, "Mutable");
    EXPECT_STR(s, "Start: Mutable String!");
    inspect(s, "s (After Replace)");

    // 4. Erase
    // "Start: Mutable String!" -> erase "Start: "
    s.erase(0, 7);
    EXPECT_STR(s, "Mutable String!");
    inspect(s, "s (After Erase)");
}

void test_capacity_control() {
    TEST_CASE("Capacity Control (Reserve/Shrink)");

    String s;
    // 初始 Small
    EXPECT_EQ(s.s.kind, String::Small);

    // 1. Reserve 扩容 -> Large
    s.reserve(1024);
    EXPECT_EQ(s.s.kind, String::Large);
    EXPECT_TRUE(s.capacity() >= 1024);
    inspect(s, "s (After Reserve 1024)");

    // 填入数据
    s.append("Small Data");
    EXPECT_EQ(s.size(), 10);

    // 2. Reduce (Shrink) -> Small
    // 数据量很小，应该能缩回 Small
    s.reduce();
    EXPECT_EQ(s.s.kind, String::Small); // 假设 Reduce 逻辑允许回退到 Small
    EXPECT_STR(s, "Small Data");
    inspect(s, "s (After Reduce)");

    // 3. Resize
    s.resize(50, 'A');
    EXPECT_EQ(s.size(), 50);
    EXPECT_EQ(s.s.kind, String::Large); // 50 > 23, 肯定是 Large
    EXPECT_EQ(s[10], 'A');
    EXPECT_EQ(s[49], 'A');
    inspect(s, "s (After Resize 50)");
}

void test_operators() {
    TEST_CASE("Operators (+, +=, <<)");

    String s1 = "Hello";
    String s2 = " ";
    String s3 = "World";

    // Operator +
    String s4 = s1 + s2 + s3;
    EXPECT_STR(s4, "Hello World");

    // Operator +=
    s1 += s2;
    s1 += s3;
    EXPECT_STR(s1, "Hello World");

    // Operator <<
    String s5;
    s5 << "A" << "B" << 'C';
    EXPECT_STR(s5, "ABC");
}

void test_exceptions() {
    TEST_CASE("Exception Handling");

    String s = "Test";

    // 2. Capacity Error (Simulation)
    // 很难直接模拟 E_Memory_Alloc 除非 mock 内存池
    // 但可以测试 limit()
    // try {
    //     s.resize(String::limit() + 1);
    // } catch (const xyu::E_Memory_Capacity&) { ... }
}

void test_self_assignment_and_aliasing() {
    TEST_CASE("Self Assignment & Aliasing");

    String s = "12345";

    // 1. 自赋值
    s = s;
    EXPECT_STR(s, "12345");

    // 2. 内部 insert (Small)
    s.insert(2, s.subview(0, 2)); // 在 '3' 前面插入 "12" -> "1212345"
    EXPECT_STR(s, "1212345");

    // 3. 构造 Large 字符串测试自引用
    String l_str;
    l_str.reserve(100);
    l_str = "ABCDEFGHIJKLMNOPQRSTUVWXYZ"; // 26 chars

    // 替换中间部分为自身的一部分
    // "ABC..." update [1, 3) "BC" with "ABCDEFGHIJKLMNOPQRSTUVWXYZ"
    l_str.update(1, 2, l_str);
    // 预期: A + ABC...Z + DE...Z

    EXPECT_EQ(l_str[0], 'A');
    EXPECT_EQ(l_str[1], 'A');
    EXPECT_EQ(l_str[26], 'Z');
    EXPECT_EQ(l_str[27], 'D');

    std::cout << "  Aliasing update check passed." << std::endl;
}

// -----------------------------------------------------------------------------
// 主函数
// -----------------------------------------------------------------------------

int main41() {
    std::cout << "==========================================" << std::endl;
    std::cout << "      XYSTRING UNIT TEST SUITE            " << std::endl;
    std::cout << "==========================================" << std::endl;

    try {
        test_construction_and_state();
        test_view_base_methods();
        test_modification_and_cow();
        test_capacity_control();
        test_operators();
        test_self_assignment_and_aliasing();
        test_exceptions();
    } catch (const xyu::Error& e) {
        std::cout << RED << "\n[FATAL] Uncaught xyu::Error!" << RESET << std::endl;
        g_fail++;
    } catch (const std::exception& e) {
        std::cout << RED << "\n[FATAL] Uncaught std::exception: " << e.what() << RESET << std::endl;
        g_fail++;
    } catch (...) {
        std::cout << RED << "\n[FATAL] Uncaught unknown exception!" << RESET << std::endl;
        g_fail++;
    }

    std::cout << "\n==========================================" << std::endl;
    std::cout << "Summary: ";
    if (g_fail == 0) {
        std::cout << GREEN << "ALL " << g_pass << " TESTS PASSED." << RESET << std::endl;
    } else {
        std::cout << RED << g_fail << " TESTS FAILED." << RESET << " (" << g_pass << " passed)" << std::endl;
    }
    std::cout << "==========================================" << std::endl;

    return g_fail;
}
