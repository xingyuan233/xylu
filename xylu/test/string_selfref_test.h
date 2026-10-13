#pragma once

/**
 * @file    string_selfref_test.h
 * @brief   String自引用测试
 * @date    2025-12-29
 */

#include <iostream>
#include <cassert>
#include "../deprecated/strsf.h"

using namespace xyu;

using String = FString;

#define RESET   "\033[0m"
#define RED     "\033[31m"
#define GREEN   "\033[32m"
#define YELLOW  "\033[33m"
#define BLUE    "\033[34m"

// 简化测试宏
#define TEST(condition) \
    do { \
        if (condition) { \
            std::cout << GREEN << "[PASS]" << RESET << " " << __FUNCTION__; \
            if (test_name) std::cout << " - " << test_name; \
            std::cout << std::endl; \
            passed++; \
        } else { \
            std::cout << RED << "[FAIL]" << RESET << " " << __FUNCTION__; \
            if (test_name) std::cout << " - " << test_name; \
            std::cout << " at line " << __LINE__ << std::endl; \
            failed++; \
        } \
        total++; \
    } while(0)

static int total = 0;
static int passed = 0;
static int failed = 0;
static const char* test_name = nullptr;

// 测试报告
void print_test_report() {
    std::cout << "\n" << BLUE << "=== 测试报告 ===" << RESET << std::endl;
    std::cout << "总测试数: " << total << std::endl;
    std::cout << GREEN << "通过: " << passed << RESET << std::endl;
    if (failed > 0) {
        std::cout << RED << "失败: " << failed << RESET << std::endl;
    } else {
        std::cout << GREEN << "所有测试通过!" << RESET << std::endl;
    }
}

// 测试自引用的辅助函数
void test_self_append() {
    test_name = "test_self_append";

    // 测试1: 追加自身的开头部分
    {
        String str = "HelloWorld";
        String expected = "HelloWorldHello";
        str.append(str.subview(0, 5));  // 追加 "Hello"
        TEST(str.equals(expected.view()));
    }

    // 测试2: 追加自身的中间部分
    {
        String str = "HelloWorld";
        String expected = "HelloWorldWorld";
        str.append(str.subview(5, 5));  // 追加 "World"
        TEST(str.equals(expected.view()));
    }

    // 测试3: 追加自身的结尾部分
    {
        String str = "HelloWorld";
        String expected = "HelloWorldWorld";
        str.append(str.subview(5));  // 追加 "World"
        TEST(str.equals(expected.view()));
    }

    // 测试4: 追加整个自身（应该翻倍）
    {
        String str = "Hello";
        String expected = "HelloHello";
        str.append(str.view());  // 追加整个字符串
        TEST(str.equals(expected.view()));
    }
}

void test_self_insert() {
    test_name = "test_self_insert";

    // 测试1: 在开头插入自身的开头部分
    {
        String str = "HelloWorld";
        String expected = "HelloHelloWorld";
        str.insert(0, str.subview(0, 5));  // 在开头插入 "Hello"
        TEST(str.equals(expected.view()));
    }

    // 测试2: 在中间插入自身的开头部分（左边引用）
    {
        String str = "HelloWorld";
        String expected = "HelloHelloWorld";
        str.insert(5, str.subview(0, 5));  // 在中间插入 "Hello"
        TEST(str.equals(expected.view()));
    }

    // 测试3: 在中间插入自身的结尾部分（右边引用）
    {
        String str = "HelloWorld";
        String expected = "HelloWorldWorld";
        str.insert(5, str.subview(5, 5));  // 在中间插入 "World"
        TEST(str.equals(expected.view()));
    }

    // 测试4: 在结尾插入自身的部分
    {
        String str = "HelloWorld";
        String expected = "HelloWorldHello";
        str.insert(str.size(), str.subview(0, 5));  // 在结尾插入 "Hello"
        TEST(str.equals(expected.view()));
    }

    // 测试5: 插入点在被引用部分内部（复杂情况）
    {
        String str = "ABCDEFGH";
        // 在位置2插入从位置1开始的3个字符 "BCD"
        str.insert(2, str.subview(1, 3));
        String expected = "ABBCDCDEFGH";
        TEST(str.equals(expected.view()));
    }

    // 测试6: 插入点在被引用部分开头（边界情况）
    {
        String str = "ABCDEFGH";
        // 在位置1插入从位置0开始的3个字符 "ABC"
        str.insert(1, str.subview(0, 3));
        String expected = "AABCBCDEFGH";
        TEST(str.equals(expected.view()));
    }

    // 测试7: 插入点在被引用部分结尾（边界情况）
    {
        String str = "ABCDEFGH";
        // 在位置7插入从位置5开始的3个字符 "FGH"
        str.insert(7, str.subview(5, 3));
        String expected = "ABCDEFGFGHH";
        TEST(str.equals(expected.view()));
    }
}

void test_self_replace() {
    test_name = "test_self_replace";

    // 测试1: 用开头替换开头（左边引用）
    {
        String str = "HelloWorld";
        // 用前5个字符替换前5个字符（实际上不变）
        str.update(0, 5, str.subview(0, 5));
        String expected = "HelloWorld";
        TEST(str.equals(expected.view()));
    }

    // 测试2: 用结尾替换开头（右边引用）
    {
        String str = "HelloWorld";
        // 用后5个字符替换前5个字符
        str.update(0, 5, str.subview(5, 5));
        String expected = "WorldWorld";
        TEST(str.equals(expected.view()));
    }

    // 测试3: 用开头替换结尾（左边引用）
    {
        String str = "HelloWorld";
        // 用前5个字符替换后5个字符
        str.update(5, 5, str.subview(0, 5));
        String expected = "HelloHello";
        TEST(str.equals(expected.view()));
    }

    // 测试4: 用中间替换开头（中间引用）
    {
        String str = "ABCDEFGHIJ";
        // 用位置2-4的字符替换位置0-2的字符
        str.update(0, 3, str.subview(2, 3));  // 用 "CDE" 替换 "ABC"
        String expected = "CDEDEFGHIJ";
        TEST(str.equals(expected.view()));
    }

    // 测试5: 被替换区域与源区域有重叠（复杂情况）
    {
        String str = "ABCDEFGH";
        // 在位置2替换长度为3，用从位置1开始的3个字符
        // 被替换: "CDE" (位置2-4)
        // 源: "BCD" (位置1-3)
        str.update(2, 3, str.subview(1, 3));
        // 期望: AB + BCD + FGH = ABBCDFGH
        String expected = "ABBCDFGH";
        TEST(str.equals(expected.view()));
    }

    // 测试6: 被替换区域包含源区域（重叠包含）
    {
        String str = "ABCDEFGH";
        // 在位置1替换长度为5，用从位置2开始的3个字符
        str.update(1, 5, str.subview(2, 3));  // 用 "CDE" 替换 "BCDEF"
        String expected = "ACDEFGH";  // 注意: 实际上应该是 A + CDE + GH = ACDEEFGH? 不对
        // 让我们手动计算:
        // 原始: A B C D E F G H
        // 替换位置1开始5个字符: B C D E F
        // 用位置2开始的3个字符: C D E
        // 结果: A + CDE + GH = ACDEEFGH
        // 等一等，让我重新思考...
        // 实际上: A + (CDE) + (G H) = A C D E G H
        expected = "ACDEGH";
        TEST(str.equals(expected.view()));
    }

    // 测试7: 源区域在被替换区域左边（左边引用）
    {
        String str = "ABCDEFGHIJ";
        // 被替换: 位置5开始的3个字符 "FGH"
        // 源: 位置2开始的3个字符 "CDE" (在左边)
        str.update(5, 3, str.subview(2, 3));
        String expected = "ABCDECDEIJ";
        // 原始: A B C D E F G H I J
        // 替换后: A B C D E + CDE + I J = A B C D E C D E I J
        TEST(str.equals(expected.view()));
    }

    // 测试8: 源区域在被替换区域右边（右边引用）
    {
        String str = "ABCDEFGHIJ";
        // 被替换: 位置2开始的3个字符 "CDE"
        // 源: 位置5开始的3个字符 "FGH" (在右边)
        str.update(2, 3, str.subview(5, 3));
        String expected = "ABFGHFGHIJ";  // A B + FGH + FGH I J
        TEST(str.equals(expected.view()));
    }
}

void test_self_assignment() {
    test_name = "test_self_assignment";

    // 测试1: 自赋值
    {
        String str = "HelloWorld";
        String original = str;
        str = str;  // 自赋值
        TEST(str.equals(original.view()));
    }

    // 测试2: 赋值为自身的子串（左边部分）
    {
        String str = "HelloWorld";
        str = str.subview(0, 5);  // 赋值为 "Hello"
        String expected = "Hello";
        TEST(str.equals(expected.view()));
    }

    // 测试3: 赋值为自身的子串（右边部分）
    {
        String str = "HelloWorld";
        str = str.subview(5);  // 赋值为 "World"
        String expected = "World";
        TEST(str.equals(expected.view()));
    }

    // 测试4: 赋值为自身的子串（中间部分）
    {
        String str = "HelloWorld";
        str = str.subview(2, 5);  // 赋值为 "lloWo"
        String expected = "lloWo";
        TEST(str.equals(expected.view()));
    }

    // 测试5: 先修改再赋值为子串（复杂情况）
    {
        String str = "ABCD";
        str.append("EFGH");  // 变为 "ABCDEFGH"
        str = str.subview(2, 4);  // 赋值为 "CDEF"
        String expected = "CDEF";
        TEST(str.equals(expected.view()));
    }

    // 测试6: 赋值为自身的 view()
    {
        String str = "HelloWorld";
        str = str.view();  // 赋值为整个视图
        String expected = "HelloWorld";
        TEST(str.equals(expected.view()));
    }

    // 测试7: 测试 Fixed 字符串赋值为自身子串
    {
        String str("HelloWorld", 10);  // 创建 Fixed 字符串
        str = str.subview(0, 5);  // 赋值为 "Hello"
        // 这里应该触发 COW，从 Fixed 变为 Small
        String expected = "Hello";
        TEST(str.equals(expected.view()));
    }
}

void test_self_append_char() {
    test_name = "test_self_append_char";

    // 测试: 追加字符串，其中包含当前字符串的内容
    {
        String str = "Hello";
        str.append("Hello");  // 追加相同的字符串
        String expected = "HelloHello";
        TEST(str.equals(expected.view()));
    }
}

void test_self_update() {
    test_name = "test_self_update";

    // 测试1: 更新为自身的子串（左边引用）
    {
        String str = "ABCDEFGH";
        str.set(0, str.subview(0, 4));  // 用 "ABCD" 更新开头
        String expected = "ABCDEFGH";  // 应该不变
        TEST(str.equals(expected.view()));
    }

    // 测试2: 更新为自身的子串（右边引用）
    {
        String str = "ABCDEFGH";
        str.set(0, str.subview(4, 4));  // 用 "EFGH" 更新开头
        String expected = "EFGHEFGH";  // 前4个变为 EFGH，后面不变
        TEST(str.equals(expected.view()));
    }

    // 测试3: 更新区域与源区域重叠
    {
        String str = "ABCDEFGH";
        // 在位置2更新长度为3，用从位置1开始的3个字符
        str.set(2, str.subview(1, 3));  // 用 "BCD" 更新 "CDE"
        String expected = "ABBCDFGH";
        TEST(str.equals(expected.view()));
    }
}

void test_edge_cases() {
    test_name = "test_edge_cases";

    // 测试1: 空字符串的自引用操作
    {
        String str;
        str.append(str.view());  // 追加空字符串
        TEST(str.empty());
    }

    // 测试2: 单个字符的自引用
    {
        String str = "A";
        str.append(str.view());  // 追加自身
        String expected = "AA";
        TEST(str.equals(expected.view()));
    }

    // 测试3: 大字符串的自引用（测试 Large 模式）
    {
        // 创建一个大字符串（超过 SSO 大小）
        String str;
        for (int i = 0; i < 100; i++) {
            str.append("Hello");
        }
        String copy = str;
        str.append(str.subview(0, 50));  // 追加自身的前50个字符
        TEST(str.size() == copy.size() + 50);
    }

    // 测试4: 循环自引用（多次操作）
    {
        String str = "AB";
        for (int i = 0; i < 5; i++) {
            str.append(str.subview(0, 1));  // 每次都追加第一个字符
        }
        // 期望: AB + A + A + A + A + A = ABAAAAA
        String expected = "ABAAAAA";
        TEST(str.equals(expected.view()));
    }
}

void test_self_reference_comprehensive() {
    std::cout << YELLOW << "\n=== 开始自引用测试 ===" << RESET << std::endl;

    test_self_append();
    test_self_insert();
    test_self_replace();
    test_self_assignment();
    test_self_append_char();
    test_self_update();
    test_edge_cases();

    std::cout << YELLOW << "=== 自引用测试结束 ===" << RESET << std::endl;
}

int main42() {
    std::cout << BLUE << "开始 String 自引用测试..." << RESET << std::endl;

    test_self_reference_comprehensive();

    print_test_report();

    if (failed == 0) {
        std::cout << GREEN << "\n✅ 所有自引用测试通过！" << RESET << std::endl;
        return 0;
    } else {
        std::cout << RED << "\n❌ 有测试失败！" << RESET << std::endl;
        return 1;
    }
}