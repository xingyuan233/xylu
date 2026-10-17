#pragma once

/**
 * @file    xystring/fmtbase.h
 * @brief   定义了Format格式化的基础组件
 * @date    2025-12-29
 *
 * @details
 *   本文件定义了 `FormatImpl˂T˃` 的概念和接口约定，
 *   提供了用于探测 `FormatImpl`特化版本的 SFINAE 工具，
 *   并包含了核心的格式字符串解析器 `FormatCore`。
 */

#include "./stmout.h"
#include "./strsso.h"

/// 格式化异常
namespace xylu::xycore
{
    /// 格式化异常基类
    struct E_Format : Error
    {
        // syntax错误：大括号未闭合
        [[noreturn]] static void abort_syntax_uncompleted_placeholder(ErrorPos) noexcept XY_COLD;

        // args错误：参数过多
        [[noreturn]] static void abort_args_too_many(ErrorPos) noexcept XY_COLD;

        // args错误：参数过少
        [[noreturn]] static void abort_args_too_few(ErrorPos) noexcept XY_COLD;

        // args错误：索引非数字
        [[noreturn]] static void abort_dyn_not_number(ErrorPos) noexcept XY_COLD;

        // args错误: 索引过大
        [[noreturn]] static void abort_dyn_index_over_limit(xyu::StringView index, ErrorPos) noexcept XY_COLD;

        // args错误: 类型不允许
        [[noreturn]] static void abort_dyn_unallowed_type(ErrorPos) noexcept XY_COLD;

        // env错误: 索引过大
        [[noreturn]] static void abort_env_index_over_limit(xyu::StringView index, ErrorPos) noexcept XY_COLD;

        // env错误: 不支持的参数
        [[noreturn]] static void abort_env_invalid_param(xyu::StringView env, ErrorPos) noexcept XY_COLD;

        // lay错误: 未指定宽度
        [[noreturn]] static void abort_lay_no_width(ErrorPos) noexcept XY_COLD;

        // lay错误: 未知的对齐方式
        [[noreturn]] static void abort_lay_unknown_align(ErrorPos) noexcept XY_COLD;

        // lay错误: 宽度非数字
        [[noreturn]] static void abort_lay_width_not_number(ErrorPos) noexcept XY_COLD;

        // lay错误: 宽度超出上限
        [[noreturn]] static void abort_lay_width_over_limit(xyu::StringView width, ErrorPos) noexcept XY_COLD;

        // rep/ex错误: 解析失败
        [[noreturn]] static void abort_repex_failed(ErrorPos) noexcept XY_COLD;

        // 预分配溢出
        [[noreturn]] static void abort_reserve_over_limit(size_t value, ErrorPos) noexcept XY_COLD;
    };
}

/// 格式化辅助
namespace xylu::xystring
{
    // 前置声明
    template <typename T, typename = void>
    struct FormatImpl;

    /// layout 格式
    struct Format_Layout
    {
        enum Align { DEFAULT, LEFT, RIGHT, CENTER, NUMERIC, DYNAMIC, UNKNOWN };

        xyu::uint width;    // 宽度 [默认为0] (0表示自动或动态)
        Align align;        // 对齐方式 [默认为'-'] ('\0'表示动态)
        char fill;          // 填充字符 [默认为' '] ('\0'表示动态)

        // 构造函数
        constexpr Format_Layout(xyu::uint w = 0, char f = ' ', char a = '-') noexcept
                : width(w), align(parse_align(a)), fill(f) {}
        // 构造函数
        constexpr Format_Layout(xyu::uint w, char f, Align a) noexcept : width(w), align(a), fill(f) {}
        // 构造函数
        constexpr Format_Layout(xyu::uint w, Align a) noexcept : width(w), align(a), fill(' ') {}

        // 解析 align
        static constexpr Align parse_align(char c) noexcept
        {
            switch (c)
            {
                case '<': return LEFT;
                case '>': return RIGHT;
                case '^': return CENTER;
                case '=': return NUMERIC;
                case '\0': return DYNAMIC;
                case '-': return DEFAULT;
                default: return UNKNOWN;
            }
        }
    };

    // 辅助测试
    namespace __
    {
    #define XY_deal_param( isend, ... ) XY_deal_param_impl( XY_PP_PARAM_EMPTY(__VA_ARGS__), isend, __VA_ARGS__ )
    #define XY_deal_param_impl( hasparam, isend, ... ) XY_deal_param_impl2( hasparam, isend, __VA_ARGS__ )
    #define XY_deal_param_impl2( hasparam, isend, ... ) XY_deal_param_##hasparam##_##isend( __VA_ARGS__ )
    #define XY_deal_param_0_0( ... ) __VA_ARGS__ ,
    #define XY_deal_param_0_1( ... ) __VA_ARGS__
    #define XY_deal_param_1_0( ... ) __VA_ARGS__
    #define XY_deal_param_1_1( ... ) __VA_ARGS__

        // 测试 FormatImpl<T>::name 接受的参数类型  Format_Layout, [represent], [ex]
    #define XY_gen_fmt_test( name, ... ) \
        template <typename T, typename Stream = void, typename = decltype(FormatImpl<T>::name(XY_deal_param(0, __VA_ARGS__) Format_Layout{}, StringView{}, StringView{}))> \
        xyu::size_c<6> test_formatimpl_##name(char&&); \
        template <typename T, typename Stream = void, typename = decltype(FormatImpl<T>::name(XY_deal_param(0, __VA_ARGS__) StringView{}, StringView{}))> \
        xyu::size_c<5> test_formatimpl_##name(const char&&); \
        template <typename T, typename Stream = void, typename = decltype(FormatImpl<T>::name(XY_deal_param(0, __VA_ARGS__) Format_Layout{}, StringView{}))> \
        xyu::size_c<4> test_formatimpl_##name(const char&); \
        template <typename T, typename Stream = void, typename = decltype(FormatImpl<T>::name(XY_deal_param(0, __VA_ARGS__) StringView{}))> \
        xyu::size_c<3> test_formatimpl_##name(int&&); \
        template <typename T, typename Stream = void, typename = decltype(FormatImpl<T>::name(XY_deal_param(0, __VA_ARGS__) Format_Layout{}))> \
        xyu::size_c<2> test_formatimpl_##name(const int&&); \
        template <typename T, typename Stream = void, typename = decltype(FormatImpl<T>::name(XY_deal_param(1, __VA_ARGS__)))> \
        xyu::size_c<1> test_formatimpl_##name(const int&); \
        template <typename T, typename Stream = void> \
        xyu::size_c<0> test_formatimpl_##name(...); \
        template <typename T, typename Stream = void> \
        constexpr auto formatimpl_##name##_type = decltype(test_formatimpl_##name<T, Stream>('0'))::value; \
        template <typename T, typename Stream = void> \
        constexpr bool formatimpl_##name##_layout = !(formatimpl_##name##_type<T, Stream> & 1);

        XY_gen_fmt_test( parse_compile )
        XY_gen_fmt_test( parse_remain, xyu::t_val<const T&>() )
        XY_gen_fmt_test( parse_runtime, xyu::t_val<const T&>() )
        XY_gen_fmt_test( format, xyu::t_val<Stream&>(), xyu::t_val<const T&>() )

    #undef XY_gen_fmt_test
    #undef XY_deal_param_1_1
    #undef XY_deal_param_1_0
    #undef XY_deal_param_0_1
    #undef XY_deal_param_0_0
    #undef XY_deal_param_impl2
    #undef XY_deal_param_impl
    #undef XY_deal_param
    
        // 异常检测 FormatImpl<T>::parse_runtime 
        template <typename U>
        constexpr bool noexcept_parse_runtime() noexcept
        {
            using T = xyu::t_rem_cr<U>;
            constexpr auto type = formatimpl_parse_runtime_type<T>;
            if      constexpr (type == 6) return noexcept(FormatImpl<T>::parse_runtime(xyu::t_val<const T&>(), Format_Layout{}, StringView{}, StringView{}));
            else if constexpr (type == 5) return noexcept(FormatImpl<T>::parse_runtime(xyu::t_val<const T&>(), StringView{}, StringView{}));
            else if constexpr (type == 4) return noexcept(FormatImpl<T>::parse_runtime(xyu::t_val<const T&>(), Format_Layout{}, StringView{}));
            else if constexpr (type == 3) return noexcept(FormatImpl<T>::parse_runtime(xyu::t_val<const T&>(), StringView{}));
            else if constexpr (type == 2) return noexcept(FormatImpl<T>::parse_runtime(xyu::t_val<const T&>(), Format_Layout{}));
            else if constexpr (type == 1) return noexcept(FormatImpl<T>::parse_runtime(xyu::t_val<const T&>()));
            else return true;
        }

        // 异常检测 FormatImpl<T>::parse_remain
        template <typename U>
        constexpr bool noexcept_parse_remain() noexcept
        {
            using T = xyu::t_rem_cr<U>;
            constexpr auto type = formatimpl_parse_remain_type<T>;
            if      constexpr (type == 6) return noexcept(FormatImpl<T>::parse_remain(xyu::t_val<const T&>(), Format_Layout{}, StringView{}, StringView{}));
            else if constexpr (type == 5) return noexcept(FormatImpl<T>::parse_remain(xyu::t_val<const T&>(), StringView{}, StringView{}));
            else if constexpr (type == 4) return noexcept(FormatImpl<T>::parse_remain(xyu::t_val<const T&>(), Format_Layout{}, StringView{}));
            else if constexpr (type == 3) return noexcept(FormatImpl<T>::parse_remain(xyu::t_val<const T&>(), StringView{}));
            else if constexpr (type == 2) return noexcept(FormatImpl<T>::parse_remain(xyu::t_val<const T&>(), Format_Layout{}));
            else if constexpr (type == 1) return noexcept(FormatImpl<T>::parse_remain(xyu::t_val<const T&>()));
            else return noexcept_parse_runtime<U>();
        }
        
        // 异常检测 FormatImpl<T>::format
        template <typename U, typename Stream>
        constexpr bool noexcept_format() noexcept
        {
            using T = xyu::t_rem_cr<U>;
            constexpr auto type = formatimpl_format_type<T>;
            if constexpr (type == 0) return true;
            else if constexpr (type == 6) return noexcept(FormatImpl<T>::format(xyu::t_val<Stream&>(), xyu::t_val<const T&>(), Format_Layout{}, StringView{}, StringView{}));
            else if constexpr (type == 5) return noexcept(FormatImpl<T>::format(xyu::t_val<Stream&>(), xyu::t_val<const T&>(), StringView{}, StringView{}));
            else if constexpr (type == 4) return noexcept(FormatImpl<T>::format(xyu::t_val<Stream&>(), xyu::t_val<const T&>(), Format_Layout{}, StringView{}));
            else if constexpr (type == 3) return noexcept(FormatImpl<T>::format(xyu::t_val<Stream&>(), xyu::t_val<const T&>(), StringView{}));
            else if constexpr (type == 2) return noexcept(FormatImpl<T>::format(xyu::t_val<Stream&>(), xyu::t_val<const T&>(), Format_Layout{}));
            else if constexpr (type == 1) return noexcept(FormatImpl<T>::format(xyu::t_val<Stream&>(), xyu::t_val<const T&>()));
        }

        // 测试 FormatImpl<T>::runtime
        template <typename T>
        xyu::bool_c<FormatImpl<T>::runtime> test_formatimpl_runtime(int);
        template <typename T>
        xyu::false_c test_formatimpl_runtime(...);
    }

    // 辅助工具
    namespace __
    {
        // 检测是否为数字
        constexpr bool is_digit(char c) noexcept { return c >= '0' && c <= '9'; }

        // 动态参数类型是否允许
        template <typename... T>
        constexpr bool is_dyn_allowed[] = { (xyu::t_is_char<T> || xyu::t_can_icast<T, StringView> || xyu::t_is_integer<T>)... };

        // 动态参数格式化
        template <typename T>
        void format_dyn(String& buf, const void* p) noexcept
        {
            auto& arg = *static_cast<const T*>(p);
            // 字符 or 字符串
            if constexpr (xyu::t_is_same<T, char> || xyu::t_can_icast<T, StringView>) buf << arg;
            // 整数
            else if constexpr (xyu::t_is_integer<T>)
            {
                constexpr char digits[201] =
                        "0001020304050607080910111213141516171819"
                        "2021222324252627282930313233343536373839"
                        "4041424344454647484950515253545556575859"
                        "6061626364656667686970717273747576777879"
                        "8081828384858687888990919293949596979899";
                constexpr xyu::size_t count = xyu::t_number<T>::digits10 + 2;
                char tmp[count];
                char* r = tmp + count;
                xyu::size_t i;
                xyu::t_chg_sign2un<T> v = arg;
                if constexpr (xyu::t_number<T>::is_signed) if (arg < 0) v = -v;
                while (v >= 100) i = (v % 100) * 2, *--r = digits[i + 1], *--r = digits[i], v /= 100;
                if (v >= 10) i = v * 2, *--r = digits[i + 1], *--r = digits[i];
                else *--r = static_cast<char>('0' + v);
                if (XY_UNLIKELY(arg < 0)) *--r = '-';
                buf << StringView(r, tmp + count - r);
            }
        }
    }
}

/// 格式化核心
namespace xylu::xystring
{
    // 辅助类型
    namespace __
    {
        /// 编译期错误
        namespace FormatError
        {
            enum FormatErrorEnum
            {
                E_NONE,
                E_TOO_MANY_ARGS,
                E_TOO_FEW_ARGS,
                E_TOO_BIG_INDEX,
                E_TOO_BIG_WIDTH,
                E_UNCOMPLETED_PLACEHOLDER,
                E_UNCOMPLETED_BRACKET,
                E_RESERVE_OVERFLOW,
                E_FAILED_PARSE_ENV,
                E_FAILED_PARSE_LAY,
                E_FAILED_PARSE_PTNEX,
                E_DYFMT_NOT_NUMBER,
                E_DYFMT_INDEX_OVER_LIMIT,
                E_DYFMT_UNALLOWED_TYPE,
            };
        }

        // 编译期错误解析
        template <FormatError::FormatErrorEnum E>
        constexpr static void parser_constant_error() noexcept
        {
            using namespace FormatError;
            static_assert(E != E_TOO_MANY_ARGS, "too many arguments");
            static_assert(E != E_TOO_FEW_ARGS, "too few arguments");
            static_assert(E != E_TOO_BIG_INDEX, "argument's index out of range");
            static_assert(E != E_TOO_BIG_WIDTH, "width out of range");
            static_assert(E != E_UNCOMPLETED_PLACEHOLDER, "uncompleted {{}} placeholder");
            static_assert(E != E_UNCOMPLETED_BRACKET, "uncompleted () bracket");
            static_assert(E != E_RESERVE_OVERFLOW, "reserve count overflow");
            static_assert(E != E_FAILED_PARSE_ENV, "failed to parse environment-fmt");
            static_assert(E != E_FAILED_PARSE_LAY, "failed to parse layout-fmt");
            static_assert(E != E_FAILED_PARSE_PTNEX, "failed to parse represent-fmt or expand-fmt");
            static_assert(E != E_DYFMT_NOT_NUMBER, "dynamic-fmt's index is not a number");
            static_assert(E != E_DYFMT_INDEX_OVER_LIMIT, "dynamic-fmt's index is over limit");
            static_assert(E != E_DYFMT_UNALLOWED_TYPE, "dynamic-fmt's type is not allowed"); // 字符/字符串/整数
        }

        /// 编译期解析结果
        template <xyu::size_t N, xyu::size_t S>
        struct ParseCompileResult
        {
            FormatError::FormatErrorEnum error = FormatError::E_NONE; // 错误类型
            xyu::size_t len;            // 编译期预留长度
            xyu::size_t last;           // 需运行期或动态解析的参数长度
            bool dynamic = false;       // 是否存在动态参数
            bool reused[S];             // 传递给运行期的参数使用表 (需重新使用的被设为false)
            char buf[N];                // 动态参数缓存
        };
    }
    
    /**
     * @brief 格式化核心
     * @internal
     * @brief 核心的格式化字符串解析与分派管理器。
     * @details
     *   这个内部类包含了四个核心的静态方法：
     *   - `parse_compile`: 在编译期解析格式字符串，预计算长度，检查基本语法错误，并分离出需要运行期处理的部分。
     *   - `parse_remain`: 在运行期处理 `parse_compile` 分离出的动态参数部分，计算其长度。
     *   - `parse_runtime`: 在运行期完整地解析格式字符串，计算总长度并检查错误。
     *   - `format`: 在运行期解析格式字符串，并将格式化结果写入输出流。
     */
    struct FormatCore
    {
        /* 编译期运行期混合解析 */

        /**
         * @brief 格式化字符串 - 编译期解析
         * @note fmt 组成: { [env] [|lay] [:rep] [?ex] } 每个部分可省略，但顺序固定
         */
        template <xyu::size_t N, typename... Args>
        constexpr static __::ParseCompileResult<N, sizeof...(Args)> parse_compile(StringView fmt) noexcept
        {
            using namespace __::FormatError;
            if (XY_UNLIKELY(fmt.empty())) return {sizeof...(Args) == 0 ? E_NONE : E_TOO_MANY_ARGS};

            // 预解析器
            using parser_t = xyu::size_t(*const)(Format_Layout, StringView, StringView);
            constexpr parser_t parsers[] = { call_parse_compile<Args>... };
            // 预解析器是否已经使用过
            bool used[sizeof...(Args)] = {};
            // 更新参数
            auto update_used = [&](xyu::size_t cur, xyu::size_t& index) {
                if (!used[cur]) {
                    if (cur == index) do ++index; while (index < sizeof...(Args) && used[index]);
                    used[cur] = true;
                }
            };

            __::ParseCompileResult<N, sizeof...(Args)> result{};
            // 检测动态参数类型是否允许
            constexpr auto& dyn_allowed = __::is_dyn_allowed<Args...>;
            // 是否需要运行期解析
            constexpr bool need_runtime[] = { is_runtime<Args>... };

            /// 循环解析
            auto& buf = result.buf;             // 动态解析参数
            xyu::size_t& total = result.len;    // 总可能长度
            xyu::size_t& last = result.last;    // 运行期或动态参数长度
            auto& reused = result.reused;       // 需要重新使用的
            xyu::size_t index = 0;              // 参数递增索引
            xyu::size_t i = 0, j = 0, k = 0;
            for (auto& b : reused) b = true;
            // 查找'{'
            while ((j = fmt.find('{', i)) != -1)
            {
                // 不完整的模式
                if (XY_UNLIKELY(j == fmt.size() - 1)) return {E_UNCOMPLETED_PLACEHOLDER};
                // 查找中间可能出现的 '}' 转义 或 错误格式
                total += j - i;
                while ((i = fmt.find('}', i, j)) != -1)
                {
                    // 不完整的模式 且 不是转义 '}'
                    if (XY_UNLIKELY(++i == j || fmt[i] != '}')) return {E_UNCOMPLETED_PLACEHOLDER};
                    ++i;
                    --total; // 转义 '}'
                }
                // 查找另一半'}'
                if (fmt[++j] != '{')
                {
                    i = fmt.find('}', j);
                    // 不完整的模式
                    if (XY_UNLIKELY(i == -1)) return {E_UNCOMPLETED_PLACEHOLDER};
                    // 查找动态参数
                    k = j + 1;
                    bool runtime = false;
                    while ((k = fmt.find('{', k, i)) != -1)
                    {
                        // 动态参数必须为 空的 "{}" 或 带索引的 "{n}"
                        while (++k != i && fmt[k] == ' ');  // 前空格
                        // 指定索引
                        xyu::size_t cur = index;
                        if (k != i) for (cur = 0;;)
                        {
                            if (!__::is_digit(fmt[k])) return {E_DYFMT_NOT_NUMBER};
                            cur = cur * 10 + (fmt[k] - '0');
                            if (cur > limit / 16) return {E_DYFMT_INDEX_OVER_LIMIT};
                            if (++k == i) break;
                            // 尾空格
                            if (fmt[k] == ' ') {
                                while (++k != i) if (fmt[k] != ' ') return {E_DYFMT_NOT_NUMBER};
                                break;
                            }
                        }
                        if (XY_UNLIKELY(cur >= sizeof...(Args))) return {E_TOO_BIG_INDEX};
                        if (XY_UNLIKELY(!dyn_allowed[cur])) return {E_DYFMT_UNALLOWED_TYPE};
                        // 更新参数
                        reused[cur] = false;
                        update_used(cur, index);
                        // 寻找下一个 '}'
                        i = fmt.find('}', ++k);
                        if (XY_UNLIKELY(i == -1)) return {E_UNCOMPLETED_PLACEHOLDER};
                        runtime = true;
                        result.dynamic = true;
                    }

                    /// 解析模式
                    StringView partfmt = fmt.subview(j, i - j);
                    Format_Spec spec = parse_spec(partfmt);
                    // 解析 env (禁止传递动态参数)
                    xyu::size_t cur = parse_env(spec.env, total);
                    if (XY_UNLIKELY(cur == -2)) return {E_FAILED_PARSE_ENV};
                    // 未指定参数
                    if (cur == -1)
                    {
                        if (XY_UNLIKELY(index >= sizeof...(Args))) return {E_TOO_FEW_ARGS};
                        cur = index;
                    }
                    // 指定参数
                    else if (XY_UNLIKELY(cur >= sizeof...(Args))) return {E_TOO_BIG_INDEX};
                    // 更新参数
                    update_used(cur, index);
                    // 运行期解析
                    if (runtime || need_runtime[cur])
                    {
                        buf[last++] = '{';
                        for (char c : partfmt.range()) buf[last++] = c;
                        buf[last++] = '}';
                        reused[cur] = false;
                    }
                    else {
                        // 编译期解析
                        // 解析 lay
                        Format_Layout fl = parse_lay(spec.lay);
                        if (XY_UNLIKELY(fl.width == -1)) return {E_FAILED_PARSE_LAY};
                        if (XY_UNLIKELY(fl.width == -2)) return {E_TOO_BIG_WIDTH};
                        // 解析 rep, ex
                        xyu::size_t add = parsers[cur](fl, spec.rep, spec.ex);
                        // 解析失败
                        if (XY_UNLIKELY(add > limit)) return {E_FAILED_PARSE_PTNEX};
                        total += xyu::max(fl.width, add);
                        if (XY_UNLIKELY(total > limit)) return {E_RESERVE_OVERFLOW};
                    }
                    ++i;
                } else {
                    i = j + 1;
                    ++total; // 转义 '{'
                }
            }
            // 不准浪费
            if (index != sizeof...(Args)) return {E_TOO_MANY_ARGS};
            // 处理末尾 '}' 转义 或 错误格式
            total += fmt.size() - i;
            while ((i = fmt.find('}', i)) != -1)
            {
                // 不完整的模式 且 不是转义 '}'
                if (XY_UNLIKELY(i == fmt.size() - 1 || fmt[++i] != '}')) return {E_UNCOMPLETED_PLACEHOLDER};
                ++i;
                --total; // 转义 '}'
            }
            // 返回总长度
            return result;
        }

        /**
         * @brief 格式化字符串 - 运行期解析编译期解析后的剩余部分
         * @param fmt parse_compile后剩余的格式化字符串 (runtime类型，或存在动态参数的占位符)
         */
        template <bool dynamic = true, typename... Args>
        static xyu::size_t parse_remain(StringView fmt, const bool(&reused)[sizeof...(Args)], const Args&... args) noexcept((... && noexcept(call_parse_remain<Args>({},{},{},{}))))
        {
            // 解析器
            using parser_t = xyu::size_t(*const)(const void*, Format_Layout, StringView, StringView);
            constexpr parser_t parsers[sizeof...(Args)] = { call_parse_remain<Args>... };

            // 动态格式解析器
            using dyfmt_t = void(*const)(String&, const void*);
            constexpr dyfmt_t dyfmts[] = { __::format_dyn<Args>... };

            // 参数
            const void* pargs[] = { xyu::mem_addr(args)... };
            // 是否使用过
            bool used[sizeof...(Args)];
            for (xyu::size_t i=0; i < sizeof...(Args); ++i) used[i] = reused[i];
            // 更新参数
            auto update_used = [&](xyu::size_t cur, xyu::size_t& index) {
                if (!used[cur]) {
                    if (cur == index) do ++index; while (index < sizeof...(Args) && used[index]);
                    used[cur] = true;
                }
            };

            /// 循环解析
            String dybuf;           // 动态参数缓存
            xyu::size_t total = 0;  // 总可能长度
            xyu::size_t index = 0;  // 参数递增索引
            xyu::size_t i = 0, j = 0, k, p;
            // 查找'{'
            do {
                // 查找另一半'}'
                i = fmt.find('}', ++j);
                // 查找动态参数
                Format_Spec spec;
                if constexpr (dynamic)
                {
                    k = j + 1;
                    if ((k = fmt.find('{', k, i)) != -1)
                    {
                        xyu::uint spec_pos[4] {-1u, -1u, -1u, -1u};
                        p = j;
                        dybuf.clear();
                        do {
                            dybuf << fmt.subview(p, k - p);
                            update_spec(spec_pos, dybuf, k - p);
                            // 动态参数必须为 空的 "{}" 或 带索引的 "{n}"
                            while (++k != i && fmt[k] == ' ');  // 跳过前空格
                            // 指定索引
                            xyu::size_t cur;
                            if (k != i) {
                                for (cur = 0;;) {
                                    cur = cur * 10 + (fmt[k] - '0');
                                    if (++k == i) break;
                                    if (fmt[k] == ' ') { k = i; break; }
                                }
                            }
                            else cur = index;
                            dyfmts[cur](dybuf, pargs[cur]);
                            // 更新参数
                            update_used(cur, index);
                            // 寻找下一个 '}'
                            p = ++k;
                            i = fmt.find('}', p);
                        }
                        while ((k = fmt.find('{', k, i)) != -1);
                        dybuf << fmt.subview(p, i - p);
                        update_spec(spec_pos, dybuf, i - p);
                        finish_spec(spec_pos, dybuf, spec);
                    }
                    else spec = parse_spec(fmt.subview(j, i - j));
                }
                else spec = parse_spec(fmt.subview(j, i - j));

                // 解析 env
                xyu::size_t cur = parse_idx(spec.env, index);
                // 解析 lay
                Format_Layout fl = parse_lay(spec.lay);
                // 解析 rep, ex
                xyu::size_t add = parsers[cur](pargs[cur], fl, spec.rep, spec.ex);
                // rep / ex 解析失败 (具体异常信息应该自行处理，否则使用这个退化版)
                if (XY_UNLIKELY(add > limit)) xyu::E_Format::abort_repex_failed(XY_CurPos("FormatCore::parse_remain"));
                total += xyu::max(fl.width, add);
                if (XY_UNLIKELY(total > limit))
                    xyu::E_Format::abort_reserve_over_limit(total, XY_CurPos("FormatCore::parse_remain"));
                update_used(cur, index);
                j = i + 1;
            } while (j < fmt.size());
            // 返回总长度
            return total;
        }

        /* 纯运行期解析 */

        /**
         * @brief 解析格式化字符串
         * @return 解析成功则返回可能的总长度，失败则抛出异常
         * @exception E_Format 格式化错误
         * @note fmt 组成: { [env] [|lay] [:rep] [?ex] } 每个部分可省略，但顺序固定
         */
        template <typename... Args>
        static xyu::size_t parse_runtime(StringView fmt, const Args&... args) noexcept((... && noexcept(call_parse_runtime<Args>({},{},{},{}))))
        {
            if (XY_UNLIKELY(fmt.empty()))
            {
                if constexpr (sizeof...(Args) == 0) return 0;
                if constexpr (!XY_RELEASE) xyu::E_Format::abort_args_too_many(XY_CurPos("FormatCore::parse_runtime"));
            }

            // 解析器
            using parser_t = xyu::size_t(*const)(const void*, Format_Layout, StringView, StringView);
            constexpr parser_t parsers[] = { call_parse_runtime<Args>... };

            // 检测动态参数类型是否允许
            constexpr auto& dyn_allowed = __::is_dyn_allowed<Args...>;
            // 动态格式解析器
            using dyfmt_t = void(*const)(String&, const void*);
            constexpr dyfmt_t dyfmts[] = { __::format_dyn<Args>... };

            // 参数
            const void* pargs[] = { xyu::mem_addr(args)... };
            // 解析器是否已经使用过
            bool used[sizeof...(Args)] = {};
            // 更新参数
            auto update_used = [&](xyu::size_t cur, xyu::size_t& index) {
                if (!used[cur]) {
                    if (cur == index) do ++index; while (index < sizeof...(Args) && used[index]);
                    used[cur] = true;
                }
            };

            /// 循环解析
            String dybuf;           // 动态参数缓存
            xyu::size_t total = 0;  // 总可能长度
            xyu::size_t index = 0;  // 参数递增索引
            xyu::size_t i = 0, j, k, p;
            // 查找 '{'.j
            while ((j = fmt.find('{', i)) != -1)
            {
                // 不完整的模式
                if constexpr (!XY_RELEASE) if (XY_UNLIKELY(j == fmt.size() - 1))
                    xyu::E_Format::abort_syntax_uncompleted_placeholder(XY_CurPos("FormatCore::parse_runtime"));
                // 查找中间可能出现的 '}' 转义 或 错误格式
                total += j - i;
                while ((i = fmt.find('}', i, j)) != -1)
                {
                    // 不完整的模式 且 不是转义 '}'
                    ++i;
                    if constexpr (!XY_RELEASE)
                    {
                        if (XY_UNLIKELY(i == j || fmt[i] != '}'))
                            xyu::E_Format::abort_syntax_uncompleted_placeholder(XY_CurPos("FormatCore::parse_runtime"));
                    }
                    ++i;
                    --total; // 转义 '}'
                }
                // 查找另一半 '}'.i
                if (fmt[++j] != '{')
                {
                    i = fmt.find('}', j);
                    // 不完整的模式
                    if constexpr (!XY_RELEASE) if (XY_UNLIKELY(i == -1))
                        xyu::E_Format::abort_syntax_uncompleted_placeholder(XY_CurPos("FormatCore::parse_runtime"));
                    // 查找动态参数
                    Format_Spec spec;
                    k = j + 1;
                    if ((k = fmt.find('{', k, i)) != -1)
                    {
                        xyu::uint spec_pos[4] {-1u, -1u, -1u, -1u};
                        p = j;
                        dybuf.clear();
                        do {
                            dybuf << fmt.subview(p, k - p);
                            update_spec(spec_pos, dybuf, k - p);
                            // 动态参数必须为 空的 "{}" 或 带索引的 "{n}"
                            while (++k != i && fmt[k] == ' ');  // 前空格
                            // 指定索引
                            xyu::size_t cur;
                            if (k != i)
                            {
                                auto x = k;
                                for (cur = 0;;)
                                {
                                    if constexpr (XY_DEBUG) if (XY_UNLIKELY(!__::is_digit(fmt[k])))
                                        xyu::E_Format::abort_dyn_not_number(XY_CurPos("FormatCore::parse_runtime"));
                                    cur = cur * 10 + (fmt[k] - '0');
                                    if constexpr (XY_DEBUG) if (XY_UNLIKELY(cur > limit / 16))
                                    {
                                        do ++k; while (__::is_digit(fmt[k]) && k < i);
                                        xyu::E_Format::abort_dyn_index_over_limit(fmt.subview(x, k - x), XY_CurPos("FormatCore::parse_runtime"));
                                    }
                                    if (++k == i) break;
                                    // 尾空格
                                    if (fmt[k] == ' ')
                                    {
                                        if constexpr (XY_DEBUG)
                                        {
                                            while (++k != i) if (fmt[k] != ' ')
                                                xyu::E_Format::abort_dyn_not_number(XY_CurPos("FormatCore::parse_runtime"));
                                        }
                                        else k = i;
                                        break;
                                    }
                                }
                            }
                            else cur = index;
                            if constexpr (!XY_RELEASE)
                            {
                                if (XY_UNLIKELY(cur >= sizeof...(Args)))
                                    xyu::E_Format::abort_args_too_few(XY_CurPos("FormatCore::parse_runtime"));
                                if (XY_UNLIKELY(!dyn_allowed[cur]))
                                    xyu::E_Format::abort_dyn_unallowed_type(XY_CurPos("FormatCore::parse_runtime"));
                            }
                            dyfmts[cur](dybuf, pargs[cur]);
                            // 更新参数
                            update_used(cur, index);
                            // 寻找下一个 '}'
                            p = ++k;
                            i = fmt.find('}', p);
                            if constexpr (!XY_RELEASE) if (XY_UNLIKELY(i == -1))
                                xyu::E_Format::abort_syntax_uncompleted_placeholder(XY_CurPos("FormatCore::parse_runtime"));
                        }
                        while ((k = fmt.find('{', k, i)) != -1);
                        dybuf << fmt.subview(p, i - p);
                        update_spec(spec_pos, dybuf, i - p);
                        finish_spec(spec_pos, dybuf, spec);
                    }
                    else spec = parse_spec(fmt.subview(j, i - j));

                    // 解析 env (禁止传递动态参数)
                    xyu::size_t cur = parse_env(spec.env, total);
                    // 未指定参数
                    if (cur == -1)
                    {
                        if constexpr (!XY_RELEASE) if (XY_UNLIKELY(index >= sizeof...(Args)))
                            xyu::E_Format::abort_args_too_few(XY_CurPos("FormatCore::parse_runtime"));
                        cur = index;
                    }
                    // 指定参数
                    else if (XY_UNLIKELY(cur >= sizeof...(Args)) && !XY_RELEASE)
                        xyu::E_Format::abort_args_too_few(XY_CurPos("FormatCore::parse_runtime"));
                    // 解析 lay
                    Format_Layout fl = parse_lay(spec.lay);
                    // 解析 rep, ex
                    xyu::size_t add = parsers[cur](pargs[cur], fl, spec.rep, spec.ex);
                    // rep / ex 解析失败 (具体异常信息应该自行处理，否则使用这个退化版)
                    if (XY_UNLIKELY(add > limit))
                        xyu::E_Format::abort_repex_failed(XY_CurPos("FormatCore::parse_runtime"));
                    // 更新参数
                    total += xyu::max(fl.width, add);
                    if (XY_UNLIKELY(total > limit))
                        xyu::E_Format::abort_reserve_over_limit(total, XY_CurPos("FormatCore::parse_runtime"));
                    update_used(cur, index);
                    ++i;
                }
                else {
                    i = j + 1;
                    ++total; // 转义 '{'
                }
            }
            // 不准浪费
            if constexpr (!XY_RELEASE) if (index != sizeof...(Args))
                xyu::E_Format::abort_args_too_many(XY_CurPos("FormatCore::parse_runtime"));
            // 处理末尾 '}' 转义 或 错误格式 .i
            total += fmt.size() - i;
            while ((i = fmt.find('}', i)) != -1)
            {
                // 不完整的模式 且 不是转义 '}'
                ++i;
                if constexpr (!XY_RELEASE)
                {
                    if (XY_UNLIKELY(i == fmt.size() || fmt[i] != '}'))
                        xyu::E_Format::abort_syntax_uncompleted_placeholder(XY_CurPos("FormatCore::parse_runtime"));
                }
                ++i;
                --total; // 转义 '}'
            }
            // 返回总长度
            return total;
        }

        /* 运行期格式化 */

        /**
         * @brief 格式化字符串
         * @return 解析成功则返回可能的总长度，失败则抛出异常
         * @exception E_Format 格式化错误
         * @note fmt 组成: { [env] [|lay] [:rep] [?ex] } 每个部分可省略，但顺序固定
         */
        template <bool dynamic = true, typename... Args, typename Stream>
        static void format(Stream&& out, StringView fmt, const Args&... args) noexcept((... && noexcept(call_format<Args, Stream>(out,{},{},{},{}))) && is_noexcept_write<Stream>)
        {
            if (XY_UNLIKELY(fmt.empty())) return;

            // 格式化器
            using format_t = void(*const)(Stream&, const void*, Format_Layout, StringView, StringView);
            constexpr format_t formats[] = { call_format<Args, Stream>... };

            using format_s = void(*const)(OStream<String>&, const void*, Format_Layout, StringView, StringView);
            constexpr format_s formatss[] = { call_format<Args, OStream<String>>... };

            // 格式化器是否使用 Format_Layout
            constexpr bool dyused[] = { __::formatimpl_format_layout<Args, Stream>... };

            // 动态格式解析器
            using dyfmt_t = void(*const)(String&, const void*);
            constexpr dyfmt_t dyfmts[] = { __::format_dyn<Args>... };

            // 解析器是否已经使用过
            bool used[sizeof...(Args)] = {};

            // 参数
            const void* pargs[] = { xyu::mem_addr(args)... };

            /// 循环解析
            String dybuf;           // 动态参数缓存
            xyu::size_t index = 0;  // 参数递增索引
            xyu::size_t i = 0, j, k, p;
            // 查找 '{'
            while ((j = fmt.find('{', i)) != -1)
            {
                // 查找中间可能出现的 '}' 转义
                while ((k = fmt.find('}', i, j)) != -1)
                {
                    out << fmt.subview(i, ++k - i);
                    i = ++k;
                }
                out << fmt.subview(i, j - i);
                // 查找另一半 '}'
                if (fmt[++j] != '{')
                {
                    i = fmt.find('}', j);

                    Format_Spec spec;
                    // 查找动态参数
                    if constexpr (dynamic)
                    {
                        k = j + 1;
                        if ((k = fmt.find('{', k, i)) != -1)
                        {
                            xyu::uint spec_pos[4] {-1u, -1u, -1u, -1u};
                            p = j;
                            dybuf.clear();
                            do {
                                dybuf << fmt.subview(p, k - p);
                                update_spec(spec_pos, dybuf, k - p);
                                // 动态参数必须为 空的 "{}" 或 带索引的 "{n}"
                                while (++k != i && fmt[k] == ' ');  // 跳过前空格
                                // 指定索引
                                xyu::size_t cur;
                                if (k != i) for (cur = 0;;) {
                                    cur = cur * 10 + (fmt[k] - '0');
                                    if (++k == i) break;
                                    if (fmt[k] == ' ') { k = i; break; }
                                }
                                else cur = index;
                                dyfmts[cur](dybuf, pargs[cur]);

                                // 更新参数
                                if (!used[cur]) {
                                    if (cur == index) do ++index; while (index < sizeof...(Args) && used[index]);
                                    used[cur] = true;
                                }
                                // 寻找下一个 '}'
                                p = ++k;
                                i = fmt.find('}', p);
                            }
                            while ((k = fmt.find('{', k, i)) != -1);
                            dybuf << fmt.subview(p, i - p);
                            update_spec(spec_pos, dybuf, i - p);
                            finish_spec(spec_pos, dybuf, spec);
                        }
                        else spec = parse_spec(fmt.subview(j, i - j));
                    }
                    else spec = parse_spec(fmt.subview(j, i - j));

                    // 解析 env
                    xyu::size_t cur = parse_idx(spec.env, index);
                    // 解析 lay
                    Format_Layout fl = parse_lay(spec.lay);
                    // 解析 ptn, ex
                    if (dyused[cur] || !fl.width) formats[cur](out, pargs[cur], fl, spec.rep, spec.ex);
                    else {
                        String str;
                        auto ss = OStream(str);
                        // 格式化
                        formatss[cur](ss, pargs[cur], fl, spec.rep, spec.ex);
                        // 对齐
                        StringView view = str.view();
                        if (fl.width > str.size())
                        {
                            xyu::uint flen = fl.width - view.size();
                            switch (fl.align)
                            {
                                case Format_Layout::RIGHT: out.write(fl.fill, flen).write(view); break;
                                case Format_Layout::CENTER: {
                                    xyu::uint llen = flen / 2; // 偏右填充，整体偏左
                                    xyu::uint rlen = flen - llen;
                                    out.write(fl.fill, llen).write(view).write(fl.fill, rlen);
                                    break;
                                }
                                default: out.write(view).write(fl.fill, flen); break; // 默认左对齐
                            }
                        }
                        else out << view;
                    }
                    // 更新参数
                    if (!used[cur]) {
                        if (cur == index) do ++index; while (index < sizeof...(Args) && used[index]);
                        used[cur] = true;
                    }
                    ++i;
                }
                else {
                    out << '{';
                    i = j + 1;
                }
            }
            // 处理末尾 '}' 转义 或 错误格式
            while ((k = fmt.find('}', i)) != -1)
            {
                out << fmt.subview(i, ++k - i);
                i = ++k;
            }
            out << fmt.subview(i, fmt.size() - i);
        }

    public:
        // 正常 count 上限
        static constexpr xyu::size_t limit = xyu::t_number<xyu::size_t>::max / 2;

        // 调用 FormatImpl<T>::parse_compile
        template <typename U>
        constexpr static xyu::size_t call_parse_compile(Format_Layout fl, StringView rep, StringView ex) noexcept
        {
            using T = xyu::t_rem_cr<U>;
            constexpr auto type = __::formatimpl_parse_compile_type<T>;
            if      constexpr (type == 6) return FormatImpl<T>::parse_compile(fl, rep, ex);
            else if constexpr (type == 5) return FormatImpl<T>::parse_compile(rep, ex);
            else if constexpr (type == 4) return FormatImpl<T>::parse_compile(fl, rep);
            else if constexpr (type == 3) return FormatImpl<T>::parse_compile(rep);
            else if constexpr (type == 2) return FormatImpl<T>::parse_compile(fl);
            else if constexpr (type == 1) return FormatImpl<T>::parse_compile();
            else return xyu::min(sizeof(T) * 2, 64);
        }

        // 调用 FormatImpl<T>::parse_runtime
        template <typename U>
        static xyu::size_t call_parse_runtime(const void* p, Format_Layout fl, StringView rep, StringView ex) noexcept(__::noexcept_parse_runtime<U>())
        {
            using T = xyu::t_rem_cr<U>;
            auto& arg = *static_cast<const T*>(p);
            constexpr auto type = __::formatimpl_parse_runtime_type<T>;
            if      constexpr (type == 6) return FormatImpl<T>::parse_runtime(arg, fl, rep, ex);
            else if constexpr (type == 5) return FormatImpl<T>::parse_runtime(arg, rep, ex);
            else if constexpr (type == 4) return FormatImpl<T>::parse_runtime(arg, fl, rep);
            else if constexpr (type == 3) return FormatImpl<T>::parse_runtime(arg, rep);
            else if constexpr (type == 2) return FormatImpl<T>::parse_runtime(arg, fl);
            else if constexpr (type == 1) return FormatImpl<T>::parse_runtime(arg);
            else return call_parse_compile<T>(fl, rep, ex);
        }

        // 调用 FormatImpl<T>::parse_remain
        template <typename U>
        static xyu::size_t call_parse_remain(const void* p, Format_Layout fl, StringView rep, StringView ex) noexcept(__::noexcept_parse_remain<U>())
        {
            using T = xyu::t_rem_cr<U>;
            auto& arg = *static_cast<const T*>(p);
            constexpr auto type = __::formatimpl_parse_remain_type<T>;
            if      constexpr (type == 6) return FormatImpl<T>::parse_remain(arg, fl, rep, ex);
            else if constexpr (type == 5) return FormatImpl<T>::parse_remain(arg, rep, ex);
            else if constexpr (type == 4) return FormatImpl<T>::parse_remain(arg, fl, rep);
            else if constexpr (type == 3) return FormatImpl<T>::parse_remain(arg, rep);
            else if constexpr (type == 2) return FormatImpl<T>::parse_remain(arg, fl);
            else if constexpr (type == 1) return FormatImpl<T>::parse_remain(arg);
            else return call_parse_runtime<T>(p, fl, rep, ex);
        }

        // 调用 FormatImpl<T>::format
        template <typename U, typename Stream>
        static void call_format(Stream& out, const void* p, Format_Layout ly, StringView rep, StringView ex) noexcept(__::noexcept_format<U, Stream>())
        {
            using T = xyu::t_rem_cr<U>;
            auto& arg = *static_cast<const T*>(p);
            constexpr auto type = __::formatimpl_format_type<T, Stream>;
            static_assert(type != 0, "Formatter<T>::format not found");
            if      constexpr (type == 6) FormatImpl<T>::format(out, arg, ly, rep, ex);
            else if constexpr (type == 5) FormatImpl<T>::format(out, arg, rep, ex);
            else if constexpr (type == 4) FormatImpl<T>::format(out, arg, ly, rep);
            else if constexpr (type == 3) FormatImpl<T>::format(out, arg, rep);
            else if constexpr (type == 2) FormatImpl<T>::format(out, arg, ly);
            else if constexpr (type == 1) FormatImpl<T>::format(out, arg);
        }

        // 测试是否为强制运行期解析
        template <typename T>
        constexpr static bool is_runtime = decltype(__::test_formatimpl_runtime<xyu::t_rem_cr<T>>(0))::value;

        // 测试是否为无异常写入
        template <typename Stream>
        constexpr static bool is_noexcept_write = noexcept(xyu::t_val<Stream&>() << char{}, xyu::t_val<Stream&>() << StringView{});

    protected:
        /// 解析模式
        struct Format_Spec { StringView env, lay, rep, ex; };
        constexpr static Format_Spec parse_spec(StringView spec) noexcept
        {
            /// 解析模式
            spec = spec.strip(' ');
            // 查找 '|' >> s1 , ':' >> s2 , '?' >> s3
            xyu::size_t s1 = spec.find('|');
            xyu::size_t s2 = spec.find(':');
            xyu::size_t s3 = spec.find('?');
            // {env|lay:rep?ex}
            // .j..1...2...3..i
            xyu::size_t m3 = xyu::min(s3, spec.size());
            xyu::size_t m2 = xyu::min(s2, m3);
            xyu::size_t m1 = xyu::min(s1, m2);
            StringView env = spec.subview(0, m1);
            StringView lay = m2 == m1 ? StringView{} : spec.subview(m1 + 1, m2 - m1 - 1);
            StringView rep = m3 == m2 ? StringView{} : spec.subview(m2 + 1, m3 - m2 - 1);
            StringView ex  = spec.size() == m3 ? StringView{} : spec.subview(m3 + 1, spec.size() - m3 - 1);
            return { env, lay, rep, ex };
        }

        // 动态格式符解析 - 更新
        static void update_spec(xyu::uint(&pos)[4], String& dybuf, xyu::size_t len) noexcept
        {
            xyu::size_t start = dybuf.size() - len;
            if (pos[3] != -1) return;
            if (auto idx = dybuf.rfind('?', start); idx != -1) pos[3] = idx;
            if (pos[2] != -1) return;
            if (auto idx = dybuf.rfind(':', start, pos[1]); idx != -1) pos[2] = idx;
            if (pos[1] != -1) return;
            if (auto idx = dybuf.rfind('|', start, pos[2]); idx != -1) pos[1] = idx;
            pos[0] = xyu::min(pos[3], pos[2], pos[1]);
        };
        // 动态格式符解析 - 完成
        static void finish_spec(xyu::uint(&pos)[4], String& dybuf, Format_Spec& spec) noexcept
        {
            if (pos[3] != -1) spec.ex = dybuf.subview(pos[3] + 1);
            if (pos[2] != -1) spec.rep = dybuf.subview(pos[2] + 1, pos[3]);
            if (pos[1] != -1) spec.lay = dybuf.subview(pos[1] + 1, pos[2]);
            spec.env = dybuf.subview(0, pos[0]).strip(' ');
        }

        /**
         * @brief 检测 环境格式符
         * @details 返回索引值，未指定则返回-1，解析失败返回-2
         * @par
         *   n 索引值 (如果有必须以索引值为开头)
         *   +n 额外预分配的容量
         * @code
         *   "0", "4+48"
         */
        constexpr static xyu::size_t parse_env(StringView env, xyu::size_t& total) noexcept
        {
            if (env.empty()) return -1;

            // 解析索引
            xyu::size_t id = 0;
            xyu::size_t j = 0;
            if (!__::is_digit(env[j])) id = -1;
            else {
                for (;;)
                {
                    id = id * 10 + (env[j] - '0');
                    if constexpr (XY_DEBUG) if (XY_UNLIKELY(id > limit / 16))
                    {
                        if (xyu::t_constant()) return -2;
                        do ++j; while (__::is_digit(env[j]));
                        xyu::E_Format::abort_env_index_over_limit(env.subview(0, j), XY_CurPos("FormatCore::parse_env"));
                    }
                    if (++j == env.size()) return id;
                    if (!__::is_digit(env[j])) break;
                }
            }

            // 解析其他
            bool change = false;
            xyu::size_t mode = 0, newmode = 0;
            xyu::size_t tmp = 0;
            for(;; ++j)
            {
                // 模式结算
                if (change || j == env.size())
                {
                    // 模式 - 额外预分配
                    if (mode == 1)
                    {
                        total += tmp;
                        if constexpr (!XY_RELEASE) if (XY_UNLIKELY(total > limit))
                        {
                            if (xyu::t_constant()) return -2;
                            xyu::E_Format::abort_reserve_over_limit(total, XY_CurPos("FormatCore::parse_env"));
                        }
                    }
                    // 更新参数
                    change = false;
                    mode = newmode;
                    tmp = 0;
                    // 解析完毕
                    if (j == env.size()) break;
                }

                // 读取参数
                char c = env[j];

                // --解析数字--
                if (__::is_digit(c))
                {
                    // 模式 - 额外预分配
                    if (mode == 1)
                    {
                        tmp = tmp * 10 + (c - '0');
                        if constexpr (XY_DEBUG) if (XY_UNLIKELY(tmp > limit / 16))
                        {
                            if (xyu::t_constant()) return -2;
                            xyu::E_Format::abort_reserve_over_limit(tmp, XY_CurPos("FormatCore::parse_env"));
                        }
                    }
                    continue;
                }
                change = true;
                // --模式切换--
                // 增加预分配容量
                if (c == '+') newmode = 1;
                // --模式控制--
                // --模式阻断--
                else if (c == ' ') newmode = 0;
                // --未知参数--
                else
                {
                    if (xyu::t_constant()) return -2;
                    xyu::E_Format::abort_env_invalid_param(env, XY_CurPos("FormatCore::parse_env"));
                }
            }

            // 返回索引
            return id;
        }

        /**
         * @brief 检测 环境格式符
         * @details 返回索引值
         */
        static xyu::size_t parse_idx(StringView env, xyu::size_t index) noexcept
        {
            if (env.empty() || !__::is_digit(env[0])) return index;

            // 解析索引
            xyu::size_t id = 0;
            xyu::size_t j = 0;
            for (;;)
            {
                id = id * 10 + (env[j] - '0');
                if constexpr (XY_DEBUG) if (XY_UNLIKELY(id > limit / 16))
                {
                    do ++j; while (__::is_digit(env[j]));
                    xyu::E_Format::abort_env_index_over_limit(env.subview(0, j), XY_CurPos("FormatCore::parse_idx"));
                }
                if (++j == env.size() || !__::is_digit(env[j])) return id;
            }
        }

        /**
         * @brief 检测 布局格式符
         * @throws E_Format_Layout 对齐格式化错误
         * @details 返回解析后的 Format_Layout，格式如下:
         * [ fill [ align ]][ width ]
         *   - fill 填充字符 (默认为 ' ')
         *   - align 对齐方式 (左对齐 '<', 右对齐 '>', 居中对齐 '^', 数字对齐 '=')
         *   - width 宽度 (默认为 0，即自动宽度)
         * @par
         *   "12", ">12", "0=12", " ^8"
         */
        constexpr static Format_Layout parse_lay(StringView layout) noexcept
        {
            if (layout.empty()) return 0;

            // 单字符
            if (layout.size() == 1)
            {
                if constexpr (!XY_RELEASE) if (XY_UNLIKELY(!__::is_digit(layout[0])))
                {
                    if (xyu::t_constant()) return -1;
                    xyu::E_Format::abort_lay_no_width(XY_CurPos("FormatCore::parse_lay"));
                }
                return layout[0] - '0';
            }

            Format_Layout fl;

            // [[ fill ] align ]
            xyu::size_t ia = !__::is_digit(layout[1]) ? 1 : (!__::is_digit(layout[0]) ? 0 : -1);
            if (ia != -1)
            {
                // [fill]
                if (ia > 0) fl.fill = layout[ia - 1];
                // [align]
                fl.align = Format_Layout::parse_align(layout[ia]);

                if constexpr (!XY_RELEASE)
                {
                    // 未知的对齐方式
                    if (XY_UNLIKELY(fl.align == Format_Layout::UNKNOWN))
                    {
                        if (xyu::t_constant()) return -1;
                        xyu::E_Format::abort_lay_unknown_align(XY_CurPos("FormatCore::parse_lay"));
                    }
                    // 指定了对齐方式却没有宽度
                    if (XY_UNLIKELY(ia + 1 == layout.size()))
                    {
                        if (xyu::t_constant()) return -1;
                        xyu::E_Format::abort_lay_no_width(XY_CurPos("FormatCore::parse_lay"));
                    }
                }
            }

            // [width]
            xyu::size_t width = 0;
            for (xyu::size_t i = ia + 1; i < layout.size(); ++i)
            {
                if (layout[i] == ' ')
                {
                    if (XY_DEBUG || xyu::t_constant())
                    {
                        while (++i < layout.size()) if (layout[i] != ' ')
                        {
                            if (xyu::t_constant()) return -1;
                            xyu::E_Format::abort_lay_width_not_number(XY_CurPos("FormatCore::parse_lay"));
                        }
                    }
                    break;
                }
                if constexpr (XY_DEBUG) if (XY_UNLIKELY(!__::is_digit(layout[i])))
                {
                    if (xyu::t_constant()) return -1;
                    xyu::E_Format::abort_lay_width_not_number(XY_CurPos("FormatCore::parse_lay"));
                }
                width = width * 10 + (layout[i] - '0');
                if constexpr (XY_DEBUG) if (XY_UNLIKELY(width > limit / 16))
                {
                    if (xyu::t_constant()) return -2;
                    do ++i; while (__::is_digit(layout[i]));
                    xyu::E_Format::abort_lay_width_over_limit(layout.subview(ia + 1, i - ia - 1), XY_CurPos("FormatCore::parse_lay"));
                }
            }
            fl.width = width;

            return fl;
        }
    };
}
