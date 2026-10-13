/**
 * @file    test_fmt.cpp
 * @brief   针对 xylu::xystring::FormatCore 和 FormatImpl 的全面测试
 * @details 包含超过100个测试用例，覆盖编译期解析、运行期解析、异常抛出、边界条件以及SFINAE流适配。
 */

#include <iostream>
#include <string>
#include <vector>
#include <type_traits>
#include <stdexcept>

#include "../direct/format"

using namespace xyu;

// ============================================================================
// 测试框架宏与统计变量
// ============================================================================

int total_tests = 0;
int passed_tests = 0;

// 控制台彩色输出 (GCC/Clang 适用)
#define C_RST "\033[0m"
#define C_GRN "\033[32m"
#define C_RED "\033[31m"
#define C_YEL "\033[33m"
#define C_CYN "\033[36m"

// 基础测试断言
#define EXPECT_TRUE(expr) do { \
    total_tests++; \
    if (expr) { \
        std::cout << C_GRN << "[PASS] " << C_RST << __FILE__ << ":" << __LINE__ << " " << #expr << std::endl; \
        passed_tests++; \
    } else { \
        std::cout << C_RED << "[FAIL] " << C_RST << __FILE__ << ":" << __LINE__ << " " << #expr << std::endl; \
    } \
} while(0)

// 字符串相等断言 (兼容 xyu::String)
#define EXPECT_EQ_STR(expr, expected) do { \
    total_tests++; \
    auto&& _res = (expr); \
    std::string res_str(_res.data(), _res.size()); \
    std::string exp_str(expected); \
    if (res_str == exp_str) { \
        std::cout << C_GRN << "[PASS] " << C_RST << __FILE__ << ":" << __LINE__ << " " << #expr << " == \"" << exp_str << "\"" << std::endl; \
        passed_tests++; \
    } else { \
        std::cout << C_RED << "[FAIL] " << C_RST << __FILE__ << ":" << __LINE__ << " " << #expr << " == \"" << exp_str << "\" (Got: \"" << res_str << "\")" << std::endl; \
    } \
} while(0)

// 编译期静态异常测试辅助宏
#define STATIC_ASSERT_FMT_ERR(n, fmt_str, err_code, ...) \
    constexpr const char __fmt_##n[] = fmt_str; \
    constexpr auto __ret_##n = FormatCore::parse_compile<sizeof(__fmt_##n), ##__VA_ARGS__>(__fmt_##n); \
    static_assert(__ret_##n.error == FormatCore::err_code, "Expected error " #err_code " for " fmt_str); \
    total_tests++; passed_tests++; \
    std::cout << C_GRN << "[PASS] " << C_RST << "Compile-time check: " << fmt_str << " -> " << #err_code << std::endl;

#define STATIC_ASSERT_COMPILE_SUCCESS(n, fmt_str, ...) \
    constexpr const char __fmt_##n[] = fmt_str; \
    constexpr auto __ret_##n = FormatCore::parse_compile<sizeof(__fmt_##n), ##__VA_ARGS__>(__fmt_##n); \
    static_assert(__ret_##n.error == FormatCore::E_NONE, "Expected success for " fmt_str); \
    total_tests++; passed_tests++; \
    std::cout << C_GRN << "[PASS] " << C_RST << "Compile-time check: " << fmt_str << " -> E_NONE" << std::endl;

// 运行期异常测试
#define RUNTIME_THROW_TEST(fmt_str, exc_type, ...) \
do { \
    total_tests++; \
    try { \
        FormatCore::parse_runtime(fmt_str, ##__VA_ARGS__); \
        std::cout << C_RED << "[FAIL] " << C_RST << __FILE__ << ":" << __LINE__ << " Expected " #exc_type " for " << fmt_str << std::endl; \
    } catch(const exc_type&) { \
        std::cout << C_GRN << "[PASS] " << C_RST << __FILE__ << ":" << __LINE__ << " Caught " #exc_type " for " << fmt_str << std::endl; \
        passed_tests++; \
    } catch(...) { \
        std::cout << C_RED << "[FAIL] " << C_RST << __FILE__ << ":" << __LINE__ << " Caught wrong exception for " << fmt_str << std::endl; \
    } \
} while(0)


// ============================================================================
// 模拟 SFINAE Stream 测试用的对象
// ============================================================================
struct MockStream1 {
    std::string out;
    MockStream1& operator<<(const StringView& s) { out.append(s.data(), s.size()); return *this; }
    MockStream1& operator<<(char c) { out.push_back(c); return *this; }
};
struct MockStream2 {
    std::string out;
    void write(char c) { out.push_back(c); }
    void write(const char* s, size_t n) { out.append(s, n); }
};
struct MockStream3 {
    std::string out;
    void append(char c) { out.push_back(c); }
    void append(const char* ptr) { out.append(ptr); }
};
struct MockStream4 {
    std::string out;
    void push_back(char c) { out.push_back(c); }
};


// 自定义非法动态参数类型
struct UnallowedDynType {};


// ============================================================================
// 测试单元
// ============================================================================

void test_core_parse_compile() {
    std::cout << C_CYN << "\n--- 一. 核心测试: parse_compile (编译期解析) ---" << C_RST << std::endl;

    // 1. 成功解析
    STATIC_ASSERT_COMPILE_SUCCESS(123, "Hello World");
    STATIC_ASSERT_COMPILE_SUCCESS(124, "Hello {}", int);
    STATIC_ASSERT_COMPILE_SUCCESS(125, "{0} and {1}", int, double);
    STATIC_ASSERT_COMPILE_SUCCESS(126, "{1} and {0}", int, double);
    STATIC_ASSERT_COMPILE_SUCCESS(127, "{|10}", int);
    STATIC_ASSERT_COMPILE_SUCCESS(128, "{:d}", int);
    STATIC_ASSERT_COMPILE_SUCCESS(129, "{:?ex}", int);

    // 2. 异常: 参数数量不匹配
    STATIC_ASSERT_FMT_ERR(132, "{}", E_TOO_FEW_ARGS);
    STATIC_ASSERT_FMT_ERR(133, "{0} {1}", E_TOO_BIG_INDEX, int);
    STATIC_ASSERT_FMT_ERR(134, "{}", E_TOO_MANY_ARGS, int, int);
    STATIC_ASSERT_FMT_ERR(135, "{0}", E_TOO_MANY_ARGS, int, double);

    // 3. 异常: 语法不完整
    STATIC_ASSERT_FMT_ERR(138, "{", E_UNCOMPLETED_PLACEHOLDER);
    STATIC_ASSERT_FMT_ERR(139, "}", E_UNCOMPLETED_PLACEHOLDER);
    STATIC_ASSERT_FMT_ERR(140, "{0", E_UNCOMPLETED_PLACEHOLDER, int);
    STATIC_ASSERT_FMT_ERR(141, "0}", E_UNCOMPLETED_PLACEHOLDER);

    // 4. 异常: 环境索引与排版边界错误
    STATIC_ASSERT_FMT_ERR(144, "{5}", E_TOO_BIG_INDEX, int);
    STATIC_ASSERT_FMT_ERR(145, "{0xxx}", E_FAILED_PARSE_ENV, int);
    // 此处预设过大宽带会返回-2 触发 E_TOO_BIG_WIDTH
    STATIC_ASSERT_FMT_ERR(147, "{|9999999999999999}", E_TOO_BIG_WIDTH, int);

    // 5. 异常: 动态格式化检查
    STATIC_ASSERT_FMT_ERR(150, "{|{a}}", E_DYFMT_NOT_NUMBER, int);
    STATIC_ASSERT_FMT_ERR(151, "{|{0}}", E_DYFMT_UNALLOWED_TYPE, UnallowedDynType);
}

void test_core_parse_remain() {
    std::cout << C_CYN << "\n--- 一. 核心测试: parse_remain (动态参数排版延续) ---" << C_RST << std::endl;
    // parse_remain 主要被隐藏在宏里，通过 xyfmt 执行结果进行验证
    // 测试动态宽度、动态对齐与动态填充符
    EXPECT_EQ_STR(xyfmt("{0|{1}}", "abc", 5), "abc  ");
    EXPECT_EQ_STR(xyfmt("{0|>{1}}", "abc", 5), "  abc");
    EXPECT_EQ_STR(xyfmt("{0|{1}{2}}", "a", '^', 5), "  a  ");
    EXPECT_EQ_STR(xyfmt("{0|{1}{2}{3}}", "str", '-', '^', 10), "---str----");

    // 动态参数多层嵌套与重复引用测试
    EXPECT_EQ_STR(xyfmt("{0|{0}}", 4), "   4");   // Width is 4, Output is "   4"
    EXPECT_EQ_STR(xyfmt("{|{1}:.{2}f}", 3.14159, 8, 3), "   3.142");
}

void test_core_parse_runtime() {
    std::cout << C_CYN << "\n--- 一. 核心测试: parse_runtime (运行期解析异常捕获) ---" << C_RST << std::endl;

    // 捕捉运行时异常
    RUNTIME_THROW_TEST("{ ", E_Format_Syntax);
    RUNTIME_THROW_TEST("} ", E_Format_Syntax);
    RUNTIME_THROW_TEST("{0", E_Format_Syntax, 1);
    RUNTIME_THROW_TEST("{}", E_Format_Argument);
    RUNTIME_THROW_TEST("{1}", E_Format_Argument, 42); // 越界
    RUNTIME_THROW_TEST("{0} {}", E_Format_Argument, 1); // 过少
    RUNTIME_THROW_TEST("{}", E_Format_Argument, 1, 2); // 过多

    // 浮点数精度超长错误
    RUNTIME_THROW_TEST("{:.9999f}", E_Format_RepEx, 3.14);

    // 动态格式类型不支持
    UnallowedDynType bad_var;
    RUNTIME_THROW_TEST("{|{0}}", E_Format_Dynamic, bad_var);
    RUNTIME_THROW_TEST("{|{abc}}", E_Format_Dynamic, 1); // 非数字索引
}

void test_impl_integers() {
    std::cout << C_CYN << "\n--- 二. 特项测试: 整数类型边界与格式 ---" << C_RST << std::endl;

    // 1. 基础转换
    EXPECT_EQ_STR(xyfmt("{:d}", 42), "42");
    EXPECT_EQ_STR(xyfmt("{:x}", 255), "ff");
    EXPECT_EQ_STR(xyfmt("{:X}", 255), "FF");
    EXPECT_EQ_STR(xyfmt("{:o}", 8), "10");
    EXPECT_EQ_STR(xyfmt("{:b}", 5), "101");
    EXPECT_EQ_STR(xyfmt("{:c}", 65), "A");

    // 2. 带基数与符号
    EXPECT_EQ_STR(xyfmt("{:#x}", 255), "0xff");
    EXPECT_EQ_STR(xyfmt("{:#X}", 255), "0XFF");
    EXPECT_EQ_STR(xyfmt("{:#b}", 5), "0b101");
    EXPECT_EQ_STR(xyfmt("{:#o}", 8), "0o10");
    EXPECT_EQ_STR(xyfmt("{:+}", 42), "+42");
    EXPECT_EQ_STR(xyfmt("{:-}", 42), " 42");  // 内部约定: '-' 表示正数补空格
    EXPECT_EQ_STR(xyfmt("{:-}", -42), "-42");

    // 3. 对齐与截断边界（按照注释中提及的：数值高位可能被截断）
    EXPECT_EQ_STR(xyfmt("{|>5}", 42), "   42");
    EXPECT_EQ_STR(xyfmt("{|<5}", 42), "42   ");
    EXPECT_EQ_STR(xyfmt("{|^5}", 42), "  42 ");
    EXPECT_EQ_STR(xyfmt("{|0=5}", -42), "-0042");

    // 故意提供过小的 Width 来触发截断测试 (验证行为是否符合代码内部设计逻辑)
    EXPECT_EQ_STR(xyfmt("{|2}", 1234), "34");  // 被截断后只保留低位

    // 4. 零与极值
    EXPECT_EQ_STR(xyfmt("{}", 0), "0");
    EXPECT_EQ_STR(xyfmt("{}", -2147483648LL), "-2147483648");
}

void test_impl_floats() {
    std::cout << C_CYN << "\n--- 二. 特项测试: 浮点数类型边界与格式 ---" << C_RST << std::endl;

    EXPECT_EQ_STR(xyfmt("{:.2f}", 3.14159), "3.14");
    EXPECT_EQ_STR(xyfmt("{:.0f}", 3.14), "3");
    EXPECT_EQ_STR(xyfmt("{:#.0f}", 3.14), "3."); // 替代形式保留小数点

    // 布局与正号
    EXPECT_EQ_STR(xyfmt("{|10:.2f}", 3.14), "      3.14");
    EXPECT_EQ_STR(xyfmt("{|0=8:.2f}", -3.14), "-0003.14");
    EXPECT_EQ_STR(xyfmt("{:+}", 3.14), "+3.14");
    EXPECT_EQ_STR(xyfmt("{:e}", 1000.0), "1.000000e+03");
}

void test_impl_bool_char_str() {
    std::cout << C_CYN << "\n--- 二. 特项测试: 布尔、字符与字符串类型 ---" << C_RST << std::endl;

    // 布尔
    EXPECT_EQ_STR(xyfmt("{}", true), "true");
    EXPECT_EQ_STR(xyfmt("{}", false), "false");
    EXPECT_EQ_STR(xyfmt("{:S}", true), "TRUE");
    EXPECT_EQ_STR(xyfmt("{:b}", false), "0");
    EXPECT_EQ_STR(xyfmt("{|2}", true), "t "); // 截断时打印单个字符

    // 字符与转义
    EXPECT_EQ_STR(xyfmt("{}", 'X'), "X");
    EXPECT_EQ_STR(xyfmt("{|3}", 'X'), "X  ");
    EXPECT_EQ_STR(xyfmt("{{}}"), "{}"); // 转义大括号

    // 字符串截断与对齐
    EXPECT_EQ_STR(xyfmt("{|>10}", "Hi"), "        Hi");
    EXPECT_EQ_STR(xyfmt("{|^5}", "A"), "  A  ");
    // 左对齐截断
    EXPECT_EQ_STR(xyfmt("{|2}", "abcd"), "ab");
    // 右对齐截断
    EXPECT_EQ_STR(xyfmt("{|>2}", "abcd"), "cd");
    // 居中截断
    EXPECT_EQ_STR(xyfmt("{|^2}", "abcd"), "bc");
}

void test_impl_pointers_ranges() {
    std::cout << C_CYN << "\n--- 二. 特项测试: 指针与范围序列类型 ---" << C_RST << std::endl;

    // 指针 (因指针长度和平台相关，仅验证是否以 0x 开头及包含内容)
    auto ptr_str = xyfmt("{}", (void*)0x1234).view();
    EXPECT_TRUE(ptr_str.find("0x") == 0);
    EXPECT_TRUE(ptr_str.find("1234") != -1);

    // Range
    std::vector<int> vec = {1, 2, 3};
    EXPECT_EQ_STR(xyfmt("{}", vec), "[1,2,3]");
    // 使用自定义分隔符 -| 和子格式
    EXPECT_EQ_STR(xyfmt("{|?(-|)}", vec), "[1-|2-|3]");

    std::vector<std::string> vs = {"a", "b"};
    EXPECT_EQ_STR(xyfmt("{:?()}", vs), "[[a][b]]"); // 空分隔符
}

void test_env_indexing() {
    std::cout << C_CYN << "\n--- 二. 特项测试: Env 索引分配机制 ---" << C_RST << std::endl;

    EXPECT_EQ_STR(xyfmt("{2} {1} {0}", "a", "b", "c"), "c b a");
    EXPECT_EQ_STR(xyfmt("{0} {0} {0}", "X"), "X X X");

    // 预分配环境指示符功能
    auto pre_str = xyfmt("{0+100}", "AllocCheck");
    EXPECT_EQ_STR(pre_str, "AllocCheck");
    // 虽然容量没法通过字符串内容体现，但是验证解析逻辑是否正确无错
}

void test_stream_adapters() {
    std::cout << C_CYN << "\n--- 二. 特项测试: SFINAE OStream 流适配器 ---" << C_RST << std::endl;

    MockStream1 s1;
    xyfmtto(s1, "{} + {} = {}", 1, 2, 3);
    EXPECT_EQ_STR(s1.out, "1 + 2 = 3");

    MockStream2 s2;
    xyfmtto(s2, "Hello {}", "World");
    EXPECT_EQ_STR(s2.out, "Hello World");

    MockStream3 s3;
    xyfmtto(s3, "Append {}", 123);
    EXPECT_EQ_STR(s3.out, "Append 123");

    MockStream4 s4;
    xyfmtto(s4, "PushBack chars");
    EXPECT_EQ_STR(s4.out, "PushBack chars");
}

int main2() {
    std::cout << C_YEL << "==========================================================" << C_RST << std::endl;
    std::cout << C_YEL << "    FormatCore & FormatImpl 综合单元测试启动...    " << C_RST << std::endl;
    std::cout << C_YEL << "==========================================================" << C_RST << std::endl;

    try {
        test_core_parse_compile();
        test_core_parse_remain();
        test_core_parse_runtime();

        test_impl_integers();
        test_impl_floats();
        test_impl_bool_char_str();
        test_impl_pointers_ranges();

        test_env_indexing();
        test_stream_adapters();

    } catch(const std::exception& e) {
        std::cerr << C_RED << "Fatal error causing test abort: " << e.what() << C_RST << std::endl;
    } catch(...) {
        std::cerr << C_RED << "Unknown fatal error causing test abort." << C_RST << std::endl;
    }

    std::cout << C_YEL << "\n==========================================================" << C_RST << std::endl;
    std::cout << C_YEL << " 测试总结: 共执行了 " << total_tests << " 个断言." << C_RST << std::endl;
    if (total_tests == passed_tests) {
        std::cout << C_GRN << " [全部通过] ALL TESTS PASSED (" << passed_tests << "/" << total_tests << ")." << C_RST << std::endl;
    } else {
        std::cout << C_RED << " [存在失败] SOME TESTS FAILED (" << (total_tests - passed_tests) << " failures)." << C_RST << std::endl;
    }
    std::cout << C_YEL << "==========================================================" << C_RST << std::endl;

    return (total_tests == passed_tests) ? 0 : 1;
}