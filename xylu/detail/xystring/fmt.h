#pragma once

/**
 * @file    xystring/fmt.h
 * @brief   一个功能强大、类型安全、可扩展的格式化库
 * @date    2026-03-05
 *
 * @details
 *   提供编译期和运行期两种格式化路径，兼顾性能与灵活性。
 *   通过特化 `xylu::xystring::FormatImpl˂T˃` 可轻松扩展对自定义类型的支持。
 *   可将格式化结果输出到任何能够适配 `xyu::OStream` 的对象中。
 */

//==================================================================================================
// ## 核心用法 (Quick Start) ##
//==================================================================================================
//
// 1. 编译期格式化 (推荐，性能最高，有编译期检查):
//    使用 xyfmt(fmt, ...) 宏来生成一个 xyu::String 对象
//    使用 xyfmtto(stream, fmt, ...) 宏来将结果输出到指定的流
//
//    auto str = xyfmt("Hello, {}! The answer is {}.", "world", 42);
//    // str == "Hello, world! The answer is 42."
//
//    xyu::File file("log.txt", xyu::File::WRITE);
//    xyfmtto(file, "Log entry: {}", str);
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
// 替换字段的基本形式: { [env] [|lay] [:rep] [?ex] }
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
// --- [:rep] (Represent Specifier) ---
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
//     - 若提供 rep 格式部分，则作为整数解析 (即支持所有的整数格式化选项)
//   - 指针:
//     - 默认为十六进制表示("#x")rep 部分会透传给底层的整数格式化器
//     - 传递的 rep 部分可以覆盖标志，如: "d" 可以将其作为十进制输出
//
// --- [?ex] (Expand Specifier) ---
// 用户自定义扩展格式，由 '?' 引导
//   - 对基本类型无任何影响，仅对自定义类型有效
//   - 支持除了 `{}`之外的任意字符，包括`|`,`:`等。
//
// --- 转义 ---
//   - `{{` 和 `}}` 用于在最终输出中表示 `{` 和 `}`
//
//==================================================================================================
// ## 动态格式化参数 (Dynamic Formatting) ##
//==================================================================================================
//
// `lay` (布局), `rep` (模式), `ex` (扩展) 均支持使用动态参数
// `env` (环境) 不支持
//
// - **语法:** 在格式规范的任意位置，使用 `{}` 或 `{n}` 来引用一个格式化参数
//   - `{}`:      自动使用下一个未使用的参数
//   - `{n}`:     使用索引为 n 的参数
//
// - **参数分配顺序:**
//   - 参数会优先分配给格式规范内部的动态参数，然后再分配给主替换字段 (或在`env`显式指定)
//   - 分配顺序严格按照从左到右扫描整个替换字段 `{...}`
//   - 示例: `xyfmt("{|{} :{}}", '>', 8, "hi")` 等同于 `xyfmt("{2|{0}:{1}}", '>', 8, "hi")`
//
// - **示例:**
//   - **动态宽度和精度:** `xyfmt("Result: {|{}:.{}f}", "|-8", 3, 3.14159)` -> `"Result:||||3.142"`
//   - **动态填充和对齐:** `xyfmt("{|{}{}{}}", '=', '^', 10, "str")` -> `"===str===="`
//
// - **注意事项:**
//   - 动态参数本身不会作为引导格式符解析
//   - 动态参数值本身不能再嵌套动态参数 (e.g., `"{|{{}}}"` 是禁止的)
//   - 仅支持整数、字符和字符串类型作为动态参数的值
//
//==================================================================================================
// ## 扩展自定义类型 (Extending for Custom Types) ##
//==================================================================================================
//
// 通过为自定义类型 `MyType` 特化 `xylu::xystring::FormatImpl<MyType>` 即可
//
// template <>
// struct xylu::xystring::FormatImpl<MyType>
// {
//     // 0. (可选) 是否仅使用运行期解析 (不定义则为 false)
//     static constexpr bool runtime = true; // 若被定义且为 true，则 xyfmt/xyfmtto 使用运行期解析，否则使用编译期解析
//
//     // 1. (可选) `parse_compile` - 编译期解析格式字符串，检查错误并并预估长度
//     //    如果缺失，会使用一个默认大小 min(sizeof(T)*2, 64)
//     static constexpr xyu::size_t parse_compile(Format_Layout fl, StringView represent, StringView expand);
//
//     // 2. (可选) `parse_remain` - 在运行期处理 `parse_compile` 分离出的动态参数部分
//     //    即使在 xyfmt / xyfmtto 中也需要在运行期解析的参数，大部分情况下等同于 parse()
//     static xyu::size_t parse_remain(const MyType& val, Format_Layout fl, StringView represent, StringView expand);
//
//     // 3. (可选) `parse_runtime` - 运行期完整地解析格式字符串，预估总长度
//     //    如果缺失，会回退到调用 prepare()
//     static xyu::size_t parse_runtime(const MyType& val, Format_Layout fl, StringView represent, StringView expand);
//
//     // 4. (必须) `format` - 实际的格式化逻辑
//     template <typename Stream>
//     static void format(Stream& out, const MyType& val, Format_Layout fl, StringView represent, StringView expand)
//     {
//         // ... 将格式化后的 val 写入 out ...
//         // out 会自动封装为 OStream 对象，可以直接使用 << 或 .write() 等
//     }
// };
//
// - **`parse_compile`/`parse_remain`/`parse_runtime`/`format` 的可选参数:**
//   `FormatImpl` 的实现可以根据需要省略部分参数，格式化系统会自动匹配最合适的重载
//   支持的签名 (按优先级):
//   1. `(..., Format_Layout, StringView represent, StringView expand)`
//   2. `(..., StringView represent, StringView expand)`
//   3. `(..., Format_Layout, StringView represent)`
//   4. `(..., StringView represent)`
//   5. `(...)`
//
//   当不接收 Format_Layout 时，底层format会自动处理对齐 (仅填充，不会截断)
//
//

#include "./fmtbase.h"
#include "../xycore/log.h"
#include "../xysystem/file.h"
#include "../xysystem/timefmt.h"

/// 格式化函数
namespace xylu::xystring
{
    /**
     * @brief 格式化函数
     * @param fmt 格式化字符串
     * @param args 格式化参数
     * @note 编译期解析并预分配内存
     */
#define xyfmt(fmt, ...) \
    [&](auto&&... ___args) { \
        using namespace xylu::xystring; \
        using namespace xylu::xystring::__; \
        constexpr const char ___sfmt[] = fmt; \
        constexpr auto ___ret = FormatCore::parse_compile<sizeof(___sfmt), decltype(___args)...>(___sfmt); \
        parser_constant_error<___ret.error>(); \
        xyu::size_t ___len = ___ret.len;  \
        if constexpr (___ret.last) ___len += FormatCore::parse_remain<___ret.dynamic>(StringView{___ret.buf, ___ret.last}, ___ret.reused, ___args...); \
        String ___str(___len); \
        FormatCore::format<(___ret.last > 0)>(OStream(___str), ___sfmt, ___args...); \
        return ___str; \
    }(__VA_ARGS__)

    /**
     * @brief 格式化函数
     * @param stream 输出流
     * @param fmt 格式化字符串
     * @param args 格式化参数
     * @note 编译期解析并预分配内存
     */
#define xyfmtto(stream, fmt, ...) \
    [&](auto&&... ___args) -> decltype((stream)) { \
        using namespace xylu::xystring; \
        using namespace xylu::xystring::__; \
        constexpr const char ___sfmt[] = fmt; \
        constexpr auto ___ret = FormatCore::parse_compile<sizeof(___sfmt), decltype(___args)...>(___sfmt); \
        parser_constant_error<___ret.error>(); \
        xyu::size_t ___len = ___ret.len;  \
        if constexpr (___ret.last) ___len += FormatCore::parse_remain<___ret.dynamic>(StringView{___ret.buf, ___ret.last}, ___ret.reused, ___args...); \
        auto&& ___s = stream; \
        auto ___sw = OStream(___s); \
        ___sw.expand(___len);   \
        FormatCore::format<(___ret.last > 0)>(___sw, ___sfmt, ___args...); \
        return xyu::forward<decltype((stream))>(___s); \
    }(__VA_ARGS__)

    /**
     * @brief 原始格式化函数
     * @param stream 输出流
     * @param fmt 格式化字符串
     * @param args 格式化参数
     * @note 不预分配内存直接格式化
     */
    #define xyfmtraw(stream, fmt, ...) \
    [&](auto&&... ___args) -> decltype((stream)) { \
        using namespace xylu::xystring; \
        using namespace xylu::xystring::__; \
        constexpr const char ___sfmt[] = fmt; \
        constexpr auto ___ret = FormatCore::parse_compile<sizeof(___sfmt), decltype(___args)...>(___sfmt); \
        parser_constant_error<___ret.error>(); \
        auto&& ___s = stream; \
        FormatCore::format<(___ret.last > 0)>(OStream(___s), ___sfmt, ___args...); \
        return xyu::forward<decltype((stream))>(___s); \
    }(__VA_ARGS__)

    /**
     * @brief 格式化函数
     * @param fmt 格式化字符串
     * @param args 格式化参数
     * @return 格式化后的字符串
     * @note 运行期解析并预分配内存
     */
    template <typename... Args>
    String format(StringView fmt, const Args&... args)
    noexcept(noexcept(FormatCore::parse_runtime(fmt, args...), FormatCore::format(xyu::t_val<OStream<String>&>(), fmt, args...)))
    {
        xyu::size_t len = FormatCore::parse_runtime(fmt, args...);
        String str(len);
        auto sw = OStream(str);
        FormatCore::format(sw, fmt, args...);
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
    Stream&& format_to(Stream&& stream, StringView fmt, const Args&... args)
    noexcept(noexcept(FormatCore::parse_runtime(fmt, args...), FormatCore::format(xyu::t_val<OStream<Stream>&>(), fmt, args...)))
    {
        xyu::size_t len = FormatCore::parse_runtime(fmt, args...);
        auto sw = OStream(stream);
        sw.expand(len);
        FormatCore::format(sw, fmt, args...);
        return xyu::forward<Stream>(stream);
    }
}

// 后置实现
namespace xylu::xystring
{
    // 字符串 或 视图，可以直接调用 str.format()
    template <typename ST>
    template <typename... Args>
    auto StringViewBase<ST>::format(const Args&... args) const noexcept(is_noexcept_format<Args...>())
    { return xystring::format(data(), args...); }

    template <typename ST>
    template <typename... Args>
    constexpr bool StringViewBase<ST>::is_noexcept_format() noexcept
    { return noexcept(xystring::format({}, xyu::t_val<Args>()...)); }

    // 输出流格式化
    template <typename S>
    template <typename T, xyu::t_enable<!xyu::t_can_init<StringView, const T&>, bool>>
    OStream<S>& OStream<S>::write(const T& v) noexcept(is_noexcept_write<T>())
    { return write(xyfmt("{}", v)); }

    template <typename S>
    template <typename T>
    constexpr bool OStream<S>::is_noexcept_write() noexcept
    {
        return noexcept(xyu::t_val<OStream&>().write(StringView{})) &&
        noexcept(FormatCore::format(xyu::t_val<OStream<String>&>(), "{}", xyu::t_val<const T&>()));
    }
}
namespace xylu::xysystem
{
    // 文件输出格式化
    template <typename T, typename>
    xyu::size_t File::write(T&& data) const { return write(xyfmt("{}", data)); }

    template <typename T, typename>
    xyu::size_t File::write(T&& data, xyu::ES_File& state) const noexcept { return write(xyfmt("{}", data), state); }
}

/// 格式化日志
namespace xylu::xycore
{
#if XY_THREAD
    /**
     * @brief 全局日志输出函数
     * @param level 日志等级
     * @param fmt 格式化字符串
     * @param args 格式化参数 (指定索引从 4 开始)
     */
#define xylog(level, fmt, ...) \
    [&](auto&&... ___args) noexcept { \
        using namespace xylu::xycore::__; \
        if (level > xyu::K_Log_Level) return; \
        LogString ___info; \
        auto ___d = xyu::Duration_utc(); \
        auto ___c = xyu::Calendar{}.from_epoch_duration(___d).to_sys(); \
        auto& ___glogger = glogger(); \
        xyfmtto(___info, "[{:%C}.{|0-6}] [0x{|0-8:~}] [{}]: " fmt "\n", \
                ___c, ___d.us(), xyu::Thread::current(), \
                ___glogger.slevel[static_cast<int>(level)], ___args...); \
        ___glogger.log_native(level, xyu::move(___info));   \
    }(__VA_ARGS__)
#else
    /**
     * @brief 全局日志输出函数
     * @param level 日志等级
     * @param fmt 格式化字符串
     * @param args 格式化参数 (指定索引从 3 开始)
     */
#define xylog(level, fmt, ...) \
    [&](auto&&... ___args) noexcept { \
        using namespace xylu::xycore::__; \
        if (level > xyu::K_Log_Level) return; \
        auto& ___glogger = glogger(); \
        auto& ___info = ___glogger.tmp(); \
        ___info.clear(); \
        auto ___d = xyu::Duration_utc(); \
        auto ___c = xyu::Calendar{}.from_epoch_duration(___d).to_sys(); \
        xyfmtto(___info, "[{:%C}.{|0-6}] [{}]: " fmt "\n", \
                ___c, ___d.us(), ___glogger.slevel[static_cast<int>(level)], ___args...); \
        ___glogger.log_native(level, ___info); \
    }(__VA_ARGS__)
#endif

    /**
     * @brief 全局日志条件输出函数
     * @param level 日志等级
     * @param limit 输出日志等级限制
     * @param fmt 格式化字符串
     * @param args 格式化参数
     */
#define xylog_if(level, limit, fmt, ...) \
    [&](auto&&... ___args) noexcept { \
       if (level > limit) return; \
       xylog(level, fmt, xyu::forward<decltype(___args)>(___args)...); \
    }(__VA_ARGS__)

    /// 全局日志输出函数 - Trace
#define xylogt(fmt, ...) xylog(xyu::N_Log_Level::Trace, fmt, ##__VA_ARGS__)
    /// 全局日志输出函数 - DEBUG
#define xylogd(fmt, ...) xylog(xyu::N_Log_Level::Debug, fmt, ##__VA_ARGS__)
    /// 全局日志输出函数 - Info
#define xylogi(fmt, ...) xylog(xyu::N_Log_Level::Info, fmt, ##__VA_ARGS__)
    /// 全局日志输出函数 - Warn
#define xylogw(fmt, ...) xylog(xyu::N_Log_Level::Warn, fmt, ##__VA_ARGS__)
    /// 全局日志输出函数 - Error
#define xyloge(fmt, ...) xylog(xyu::N_Log_Level::Error, fmt, ##__VA_ARGS__)
    /// 全局日志输出函数 - Fatal
#define xylogf(fmt, ...) xylog(xyu::N_Log_Level::Fatal, fmt, ##__VA_ARGS__)

    /// 全局日志条件输出函数 - Trace
#define xylogt_if(limit, fmt, ...) xylog_if(xyu::N_Log_Level::Trace, limit, fmt, ##__VA_ARGS__)
    /// 全局日志条件输出函数 - DEBUG
#define xylogd_if(limit, fmt, ...) xylog_if(xyu::N_Log_Level::Debug, limit, fmt, ##__VA_ARGS__)
    /// 全局日志条件输出函数 - Info
#define xylogi_if(limit, fmt, ...) xylog_if(xyu::N_Log_Level::Info, limit, fmt, ##__VA_ARGS__)
    /// 全局日志条件输出函数 - Warn
#define xylogw_if(limit, fmt, ...) xylog_if(xyu::N_Log_Level::Warn, limit, fmt, ##__VA_ARGS__)
    /// 全局日志条件输出函数 - Error
#define xyloge_if(limit, fmt, ...) xylog_if(xyu::N_Log_Level::Error, limit, fmt, ##__VA_ARGS__)
    /// 全局日志条件输出函数 - Fatal
#define xylogf_if(limit, fmt, ...) xylog_if(xyu::N_Log_Level::Fatal, limit, fmt, ##__VA_ARGS__)

    /**
     * @brief 全局日志输出函数
     * @param level 日志等级
     * @param fmt 格式化字符串
     * @param line 行数
     * @param func 函数名
     * @param args 格式化参数
     * @details 增加 "file:line func" 前缀
     */
#define xylog1(level, fmt, file, line, func, ...) \
    xylog(level, "{}:{} {}: " fmt, file, line, func, ##__VA_ARGS__)

    /// 全局日志输出函数 - Warn (增加 "file:line:func" 前缀)
#define xylogw1(fmt, file, line, func, ...) xylog1(xyu::N_Log_Level::Warn, fmt, file, line, func, ##__VA_ARGS__)
    /// 全局日志输出函数 - Error (增加 "file:line:func" 前缀)
#define xyloge1(fmt, file, line, func, ...) xylog1(xyu::N_Log_Level::Error, fmt, file, line, func, ##__VA_ARGS__)
    /// 全局日志输出函数 - Fatal (增加 "file:line:func" 前缀)
#define xylogf1(fmt, file, line, func, ...) xylog1(xyu::N_Log_Level::Fatal, fmt, file, line, func, ##__VA_ARGS__)

    /**
     * @brief 全局日志输出函数
     * @param level 日志等级
     * @param fmt 格式化字符串
     * @param line 行数
     * @param func 函数名
     * @param args 格式化参数
     * @details 增加 "__FILE__:line func" 前缀
     */
#define xylog2(level, fmt, line, func, ...) \
    xylog(level, __FILE__ ":{} {}: " fmt, line, func, ##__VA_ARGS__)

    /// 全局日志输出函数 - Warn (增加 "__FILE__:line:func" 前缀)
#define xylogw2(fmt, line, func, ...) xylog2(xyu::N_Log_Level::Warn, fmt, line, func, ##__VA_ARGS__)
    /// 全局日志输出函数 - Error (增加 "__FILE__:line:func" 前缀)
#define xyloge2(fmt, line, func, ...) xylog2(xyu::N_Log_Level::Error, fmt, line, func, ##__VA_ARGS__)
    /// 全局日志输出函数 - Fatal (增加 "__FILE__:line:func" 前缀)
#define xylogf2(fmt, line, func, ...) xylog2(xyu::N_Log_Level::Fatal, fmt, line, func, ##__VA_ARGS__)

    /**
     * @brief 全局日志输出函数
     * @param level 日志等级
     * @param fmt 格式化字符串
     * @param args 格式化参数
     * @details 增加 "__FILE__:__LINE__ __func__" 前缀
     */
#define xylog3(level, fmt, ...) \
    xylog(level, __FILE__ ":{} {}: " fmt, __LINE__, __func__, ##__VA_ARGS__)

    /// 全局日志输出函数 - Warn (增加 "__FILE__:__LINE__:__func__" 前缀)
#define xylogw3(fmt, ...) xylog3(xyu::N_Log_Level::Warn, fmt, ##__VA_ARGS__)
    /// 全局日志输出函数 - Error (增加 "__FILE__:__LINE__:__func__" 前缀)
#define xyloge3(fmt, ...) xylog3(xyu::N_Log_Level::Error, fmt, ##__VA_ARGS__)
    /// 全局日志输出函数 - Fatal (增加 "__FILE__:__LINE__:__func__" 前缀)
#define xylogf3(fmt, ...) xylog3(xyu::N_Log_Level::Fatal, fmt, ##__VA_ARGS__)
}
