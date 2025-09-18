#pragma clang diagnostic push
#pragma ide diagnostic ignored "OCUnusedGlobalDeclarationInspection"
#pragma once

#include "./fmtbase.h"

/* 格式化 */

/**
 * @brief 一个功能强大、类型安全、可扩展的格式化库
 * @note 提供编译期和运行期两种格式化路径，兼顾性能与灵活性
 * @note 通过特化 `xylu::xystring::Formatter<T>` 可轻松扩展对自定义类型的支持
 * @note 可将格式化结果输出到任何能够适配 `xyu::Stream_Out` 的对象中 (如 `xyu::String`, `xyu::File` 等)
 */

//==================================================================================================
// ## 核心用法 (Quick Start) ##
//==================================================================================================
//
// 1. 编译期格式化 (推荐，性能最高，有编译期检查):
//    使用 xyfmt(fmt, ...) 宏来生成一个 xyu::String 对象
//    使用 xyfmtt(stream, fmt, ...) 宏来将结果输出到指定的流
//
//    auto str = xyfmt("Hello, {}! The answer is {}.", "world", 42);
//    // str == "Hello, world! The answer is 42."
//
//    xyu::File file("log.txt", xyu::File::WRITE);
//    xyfmtt(file, "Log entry: {}", str);
//
//
// 2. 运行期格式化 (当格式字符串不是编译期常量时使用):
//    使用 xyu::format(fmt, ...) 函数来生成 String
//    使用 xyu::format_to(stream, fmt, ...) 函数来输出到流
//
//    xyu::StringView user_input = "{:c}";
//    int ch = 'A';
//    auto result = xyu::format(user_input, ch); // result == "A"
//
//==================================================================================================
// ## 格式字符串语法 (Format String Syntax) ##
//==================================================================================================
//
// 格式字符串由普通文本和替换字段 `{}` 组成
// 替换字段的基本形式: { [env] [|lay] [:ptn] [?ex] }
// 各部分均可选，但顺序固定
//
// --- [env] (Environment Specifier) ---
// 控制参数索引和解析器行为
//   - `{}`:      自动使用下一个未使用的最小的参数 (即 "{},{2},{}" 等同于 "{0},{2},{1}")
//   - `{n}`:     使用索引为 n 的参数 (可以复用)
//   - `{n+val}`: 使用索引为 n 的参数，并为输出流额外预留 val 字节的空间
//
//
// --- [|lay] (Layout Specifier) ---
// 控制对齐、宽度和填充，由 '|' 引导
// 语法: [fill] [align] [width]
//   - fill:  单个字符，用于填充空白区域 (默认为 ' ')
//       - 注意: 禁止直接使用 '{','}','|',':','?' 会影响解析的字符，可以使用动态参数实现
//   - align: 对齐方式:
//       '-' : 使用默认对齐方式 (字符串左对齐，数值右对齐)
//       '<' : 左对齐
//       '>' : 右对齐
//       '^' : 居中对齐
//       '=' : 数字对齐 (符号在最左，数值在最右，填充在中间)
//   - width: 字段宽度 (为0或省略时表示自动宽度，字符串、整数、布尔都会被截断，但浮点数不会)
//
//
// --- [:ptn] (Pattern Specifier) ---
// 控制特定类型的输出格式，由 ':' 引导
//   - 整数 (d/D, b/B, o/O, x/X, c/C, +, -, #):
//     - d/D: 十进制 (默认)
//     - b/B: 二进制 (0b/0B 前缀需与 '#' 配合)
//     - o/O: 八进制 (0o/0O 前缀需与 '#' 配合)
//     - x/X: 十六进制 (0x/0X 前缀需与 '#' 配合)
//     - c/C: 解释为字符，仅占1位 (仅不受符号进制等影响，但仍受 layout 影响)
//     - +: 正数前显示 '+'
//     - -: 正数前显示 ' '
//     - #: 显示进制前缀 (0b, 0o, 0x)
//     - ~: 取消'#'标志
//   - 浮点数 (f, e, g, a, ., +, #):
//     - f/F: 固定点表示
//     - e/E: 科学计数法
//     - g/G: 通用格式 (自动选择 f 或 e) (默认)
//     - a/A: 十六进制浮点
//     - .n:  精度 (f/e/a下小数点后n位，g下总有效数字n位)
//     - +/-: 正数前显示 '+'/' '
//     - #: 显示进制前缀 0x (仅a/A下有效)
//     - ~: 取消'#'标志
//   - 布尔 (s, S, b, B, #):
//     - s/S/#: 输出 "true"/"false"(默认) 或 "TRUE"/"FALSE"
//     - b/B/~: 输出 "1"/"0"
//   - 字符:
//     - 默认输出字符
//     - 若提供 ptn 格式部分，则作为整数解析 (即支持所有的整数格式化选项)
//   - 指针:
//     - 默认为十六进制表示("#x")ptn 部分会透传给底层的整数格式化器
//     - 传递的 ptn 部分可以覆盖标志，如: "d" 可以将其作为十进制输出
//
// --- [?ex] (Expand Specifier) ---
// 用户自定义扩展格式，由 '?' 引导
//   - 对基本类型无任何影响，仅对自定义类型有效
//   - 支持除了 `{}`之外的任意字符。
//
// --- 转义 ---
//   - `{{` 和 `}}` 用于在最终输出中表示 `{` 和 `}`
//
//==================================================================================================
// ## 动态格式化参数 (Dynamic Formatting) ##
//==================================================================================================
//
// `lay` (布局), `ptn` (模式), `ex` (扩展) 部分都支持使用动态值
// `env` (环境) 不支持
//
// - **语法:** 在格式规范的任意位置，使用 `{}` 或 `{n}` 来引用一个格式化参数
//   - `{}`:      自动使用下一个未使用的参数
//   - `{n}`:     使用索引为 n 的参数
//
// - **参数分配顺序:**
//   - 参数会优先分配给格式规范内部的动态参数，然后再分配给主替换字段(可在`env`显式指定)
//   - 分配顺序严格按照从左到右扫描整个替换字段 `{...}`
//   - 示例: `xyfmt("{|{}:{}}", '>', 8, "hi")` 等同于 `xyfmt("{2|{0}:{1}}", '>', 8, "hi")`
//
// - **示例:**
//   - **动态宽度和精度:** `xyfmt("Result: {0|{1}:.{2}f}", 3.14159, 8, 3)` -> `"Result:    3.142"`
//   - **动态填充和对齐:** `xyfmt("{0|{1}{2}{3}}", "str", '^', '=', 10)` -> `"===str===="`
//
// - **限制:**
//   - 动态参数值本身不能再嵌套动态参数 (e.g., `"{|{{}}}"` 是禁止的)
//   - 仅支持整数、字符和字符串类型作为动态参数的值
//
//==================================================================================================
// ## 扩展自定义类型 (Extending for Custom Types) ##
//==================================================================================================
//
// 通过为你的类型 `MyType` 特化 `xylu::xystring::Formatter<MyType>` 即可
//
// template <>
// struct xylu::xystring::Formatter<MyType>
// {
//     // 0. (可选) 是否仅使用运行期解析 (不定义则为 0)
//     static constexpr int dynamic = 1; // 若被定义且为 true，则 xyfmt/xyfmtt 使用运行期解析，否则使用编译期解析
//
//     // 1. (可选) `prepare` - 编译期预估长度
//     //    如果缺失，会使用一个默认大小 min(sizeof(T)*2, 64)
//     static constexpr xyu::size_t prepare(const Format_Layout& fl, const StringView& pattern, const StringView& expand) {
//         // ... 返回一个预估的长度 ...
//     }
//
//     // 2. (可选) `parse` - 运行期解析长度
//     //    如果缺失，会回退到调用 prepare()
//     static xyu::size_t parse(const MyType& val, const Format_Layout& fl, const StringView& pattern, const StringView& expand) {
//         // ... 返回精确的长度 ...
//     }
//
//     // 3. (可选) `preparse` - 运行期预解析参数
//     //    即使在 xyfmt / xyfmtt 中也会在运行期解析的参数，大部分情况下等同于 parse()
//     static void preparse(const MyType& val, const Format_Layout& fl, const StringView& pattern, const StringView& expand) {
//         // ... 返回可能精确的长度 ...
//     }
//
//     // 4. (必须) `format` - 实际的格式化逻辑
//     template <typename Stream>
//     static void format(Stream& out, const MyType& val, const Format_Layout& fl, const StringView& pattern, const StringView& expand) {
//         // ... 将格式化后的 val 写入 out ...
//         // out 会自动封装为 StreamWrapper 对象，可以直接使用 << 或 .write()
//         out << "Value is " << val.to_string();
//     }
// };
//
// - **`prepare`/`parse`/`preparse`/`format` 的可选参数:**
//   你的 `Formatter` 实现可以根据需要省略部分参数，格式化系统会自动匹配最合适的重载
//   支持的签名 (按优先级):
//   1. `(..., const Format_Layout&, const StringView& pattern, const StringView& expand)`
//   2. `(..., const StringView& pattern, const StringView& expand)`
//   3. `(..., const Format_Layout&, const StringView& pattern)`
//   4. `(..., const StringView& pattern)`
//   5. `(...)`
//
//   当不接收 Format_Layout 时，底层format会自动处理对齐 (只会填充，不会截断)
//
//

namespace xylu::xystring
{
    /**
     * @brief 格式化函数
     * @param fmt 格式化字符串
     * @param args 格式化参数
     * @return 格式化后的字符串
     * @note 编译期解析并预分配内存
     */
#define xyfmt(fmt, ...) \
    [&](auto Tp) { \
        using namespace xylu::xystring::__; \
        constexpr const char __sfmt[] = fmt;  \
        constexpr auto __ret = FmtManager::prepare_tp<sizeof(__sfmt), decltype(Tp)>(__sfmt); \
        parser_constant_error<__ret.len>(); \
        xyu::size_t __len = __ret.len; \
        if constexpr (__ret.last) __len += FmtManager::preparse<__ret.dynamic>(xyu::StringView{__ret.buf, __ret.last}, ##__VA_ARGS__); \
        xyu::String __str(__len); \
        auto __sw = xyu::Stream_Out(__str); \
        FmtManager::format<__ret.dynamic>(__sw, __sfmt, ##__VA_ARGS__); \
        return __str; \
    }(xyu::make_typelist(__VA_ARGS__)) // 调用时别忘了逗号

    /**
     * @brief 格式化函数
     * @param stream 输出流
     * @param fmt 格式化字符串
     * @param args 格式化参数
     * @return 输出流
     * @note 编译期解析并预分配内存
     */
#define xyfmtt(stream, fmt, ...) \
    [&](auto Tp) -> decltype((stream)) {  \
        using namespace xylu::xystring::__; \
        constexpr const char __sfmt[] = fmt; \
        constexpr auto __ret = FmtManager::prepare_tp<sizeof(__sfmt), decltype(Tp)>(__sfmt); \
        parser_constant_error<__ret.len>(); \
        xyu::size_t __len = __ret.len; \
        if constexpr (__ret.last) __len += FmtManager::preparse<__ret.dynamic>(xyu::StringView{__ret.buf, __ret.last}, ##__VA_ARGS__); \
        auto&& __s = stream; \
        auto __sw = xyu::Stream_Out(__s); \
        __sw.expand(__ret.len);   \
        FmtManager::format<__ret.dynamic>(__sw, __sfmt, ##__VA_ARGS__); \
        return xyu::forward<decltype((stream))>(__s); \
    }(xyu::make_typelist(__VA_ARGS__)) // 调用时别忘了逗号

    /**
     * @brief 格式化函数
     * @param fmt 格式化字符串
     * @param args 格式化参数
     * @return 格式化后的字符串
     * @note 运行期解析并预分配内存
     */
    template <typename... Args>
    String format(const StringView& fmt, const Args&... args)
    {
        xyu::size_t len = __::FmtManager::parse(fmt, args...);
        String str(len);
        auto sw = Stream_Out(str);
        __::FmtManager::format(sw, fmt, args...);
        return str;
    }

    /**
     * @brief 格式化函数
     * @param stream 输出流
     * @param fmt 格式化字符串
     * @param args 格式化参数
     * @return 输出流
     * @note 运行期解析并预分配内存
     */
    template <typename Stream, typename... Args>
    Stream&& format_to(Stream&& stream, const StringView& fmt, const Args&... args)
    {
        xyu::size_t len = __::FmtManager::parse(fmt, args...);
        auto sw = Stream_Out(stream);
        sw.expand(len);
        __::FmtManager::format(sw, fmt, args...);
        return xyu::forward<Stream>(stream);
    }
}

namespace xylu::xystring
{
    namespace __
    {
        XY_COLD void inner_output_error(xyu::N_LOG_LEVEL level, const StringView& mess);

        template <xyu::size_t N, typename... Args>
        XY_COLD void output_error(bool is_fatal, const char(&fmt)[N], Args&&... args)
        {
            inner_output_error(is_fatal ? xyu::N_LOG_FATAL : xyu::N_LOG_ERROR, format(fmt, args...));
        }
    }

    // 字符串 或 视图，直接调用 str.format()
    template <typename ST>
    template <typename... Args>
    auto StringBase<ST>::format(const Args&... args) const
    { return xylu::xystring::format(data(), args...); }

    // 输出流的格式化
    template <typename S>
    template <typename T, xyu::t_enable<!xyu::t_can_icast<T, StringView>, bool>>
    Stream_Out<S>& Stream_Out<S>::write(const T& v) noexcept(noexcept(write(StringView{})))
    { return write(xyfmt("{}", v)); }
}

#pragma clang diagnostic pop