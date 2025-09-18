#pragma clang diagnostic push
#pragma ide diagnostic ignored "UnreachableCallsOfFunction"
#pragma ide diagnostic ignored "hicpp-exception-baseclass"
#pragma clang diagnostic ignored "-Wundefined-internal"
#pragma ide diagnostic ignored "UnusedValue"
#pragma ide diagnostic ignored "OCUnusedGlobalDeclarationInspection"
#pragma ide diagnostic ignored "google-explicit-constructor"
#pragma ide diagnostic ignored "ConstantParameter"
#pragma ide diagnostic ignored "NotImplementedFunctions"
#pragma once

#include "../../link/stream"
#include "../../link/error"

/**
 * @file
 * @brief 提供了格式化库的底层基础结构和核心解析逻辑。
 * @details
 *   本文件定义了 `Formatter<T>` 的概念和接口约定，提供了用于探测 `Formatter`
 *   特化版本的 SFINAE 工具，并包含了核心的格式字符串解析器 `FmtManager`。
 *   这部分主要供库的内部实现使用，普通用户通常不需要直接与之交互。
 */

namespace xylu::xystring
{
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
        constexpr Format_Layout(xyu::uint w, char f, Align a) noexcept
                : width(w), align(a), fill(f) {}
        // 构造函数
        constexpr Format_Layout(xyu::uint w, Align a) noexcept
                : width(w), align(a), fill(' ') {}

        // 解析 align
        static constexpr Align parse_align(char c) noexcept
        {
            return [c]() {
                switch (c) {
                    case '<': return LEFT;
                    case '>': return RIGHT;
                    case '^': return CENTER;
                    case '=': return NUMERIC;
                    case '-': return DEFAULT;
                    case '\0': return DYNAMIC;
                    default: return UNKNOWN;
                }
            }();
        }
    };

    // 前置声明
    template <typename T, typename = void>
    struct Formatter;

    // Formatter<T> 测试函数
    namespace __
    {
        // 测试 Formatter<T>::prepare 接受的参数类型  Format_Layout, [patten], [expand]
        template <typename T, typename = decltype(Formatter<T>::prepare(Format_Layout{}, StringView{}, StringView{}))>
        xyu::int_c<5> test_formatter_prepare(char&&);
        template <typename T, typename = decltype(Formatter<T>::prepare(StringView{}, StringView{}))>
        xyu::int_c<4> test_formatter_prepare(const char&&);
        template <typename T, typename = decltype(Formatter<T>::prepare(Format_Layout{}, StringView{}))>
        xyu::int_c<3> test_formatter_prepare(const char&);
        template <typename T, typename = decltype(Formatter<T>::prepare(StringView{}))>
        xyu::int_c<2> test_formatter_prepare(int&&);
        template <typename T, typename = decltype(Formatter<T>::prepare(Format_Layout{}))>
        xyu::int_c<1> test_formatter_prepare(const int&&);
        template <typename T, typename = decltype(Formatter<T>::prepare())>
        xyu::int_c<0> test_formatter_prepare(const int&);
        template <typename T>
        xyu::int_c<-1> test_formatter_prepare(...);
        template <typename T>
        constexpr int get_formatter_prepare_type = decltype(test_formatter_prepare<T>('0'))::value;

        // 封装调用 Formatter<T>::prepare
        template <typename U>
        constexpr xyu::size_t call_formatter_prepare(const Format_Layout& fl, const StringView& pattern, const StringView& expand) noexcept
        {
            using T = xyu::t_remove_cvref<U>;
            constexpr int type = get_formatter_prepare_type<T>;
            if      constexpr (type == 5) return Formatter<T>::prepare(fl, pattern, expand);
            else if constexpr (type == 4) return Formatter<T>::prepare(pattern, expand);
            else if constexpr (type == 3) return Formatter<T>::prepare(fl, pattern);
            else if constexpr (type == 2) return Formatter<T>::prepare(pattern);
            else if constexpr (type == 1) return Formatter<T>::prepare(fl);
            else if constexpr (type == 0) return Formatter<T>::prepare();
            else return xyu::min(sizeof(T) * 2, 64);
        }


        // 测试 Formatter<T>::parse 接受的参数类型 const T&, Format_Layout, [patten], [expand]
        template <typename T, typename = decltype(Formatter<T>::parse(xyu::t_val<const T&>(), Format_Layout{}, StringView{}, StringView{}))>
        xyu::int_c<5> test_formatter_parse(char&&);
        template <typename T, typename = decltype(Formatter<T>::parse(xyu::t_val<const T&>(), StringView{}, StringView{}))>
        xyu::int_c<4> test_formatter_parse(const char&&);
        template <typename T, typename = decltype(Formatter<T>::parse(xyu::t_val<const T&>(), Format_Layout{}, StringView{}))>
        xyu::int_c<3> test_formatter_parse(const char&);
        template <typename T, typename = decltype(Formatter<T>::parse(xyu::t_val<const T&>(), StringView{}))>
        xyu::int_c<2> test_formatter_parse(int&&);
        template <typename T, typename = decltype(Formatter<T>::parse(xyu::t_val<const T&>(), Format_Layout{}))>
        xyu::int_c<1> test_formatter_parse(const int&&);
        template <typename T, typename = decltype(Formatter<T>::parse(xyu::t_val<const T&>()))>
        xyu::int_c<0> test_formatter_parse(const int&);
        template <typename T>
        xyu::int_c<-1> test_formatter_parse(...);
        template <typename T>
        constexpr int get_formatter_parse_type = decltype(test_formatter_parse<T>('0'))::value;

        // 封装调用 Formatter<T>::parse
        template <typename U>
        constexpr xyu::size_t call_formatter_parse(const void* p, const Format_Layout& fl, const StringView& pattern, const StringView& expand) noexcept
        {
            using T = xyu::t_remove_cvref<U>;
            auto& arg = *static_cast<const T*>(p);
            constexpr int type = get_formatter_parse_type<T>;
            if      constexpr (type == 5) return Formatter<T>::parse(arg, fl, pattern, expand);
            else if constexpr (type == 4) return Formatter<T>::parse(arg, pattern, expand);
            else if constexpr (type == 3) return Formatter<T>::parse(arg, fl, pattern);
            else if constexpr (type == 2) return Formatter<T>::parse(arg, pattern);
            else if constexpr (type == 1) return Formatter<T>::parse(arg, fl);
            else if constexpr (type == 0) return Formatter<T>::parse(arg);
            else return call_formatter_prepare<T>(fl, pattern, expand);
        }


        // 测试 Formatter<T>::preparse 接受的参数类型 const T&, Format_Layout, [patten], [expand]
        template <typename T, typename = decltype(Formatter<T>::preparse(xyu::t_val<const T&>(), Format_Layout{}, StringView{}, StringView{}))>
        xyu::int_c<5> test_formatter_preparse(char&&);
        template <typename T, typename = decltype(Formatter<T>::preparse(xyu::t_val<const T&>(), StringView{}, StringView{}))>
        xyu::int_c<4> test_formatter_preparse(const char&&);
        template <typename T, typename = decltype(Formatter<T>::preparse(xyu::t_val<const T&>(), Format_Layout{}, StringView{}))>
        xyu::int_c<3> test_formatter_preparse(const char&);
        template <typename T, typename = decltype(Formatter<T>::preparse(xyu::t_val<const T&>(), StringView{}))>
        xyu::int_c<2> test_formatter_preparse(int&&);
        template <typename T, typename = decltype(Formatter<T>::preparse(xyu::t_val<const T&>(), Format_Layout{}))>
        xyu::int_c<1> test_formatter_preparse(const int&&);
        template <typename T, typename = decltype(Formatter<T>::preparse(xyu::t_val<const T&>()))>
        xyu::int_c<0> test_formatter_preparse(const int&);
        template <typename T>
        xyu::int_c<-1> test_formatter_preparse(...);
        template <typename T>
        constexpr int get_formatter_preparse_type = decltype(test_formatter_preparse<T>('0'))::value;

        // 封装调用 Formatter<T>::preparse
        template <typename U>
        constexpr xyu::size_t call_formatter_preparse(const void* p, const Format_Layout& fl, const StringView& pattern, const StringView& expand) noexcept
        {
            using T = xyu::t_remove_cvref<U>;
            auto& arg = *static_cast<const T*>(p);
            constexpr int type = get_formatter_preparse_type<T>;
            if      constexpr (type == 5) return Formatter<T>::preparse(arg, fl, pattern, expand);
            else if constexpr (type == 4) return Formatter<T>::preparse(arg, pattern, expand);
            else if constexpr (type == 3) return Formatter<T>::preparse(arg, fl, pattern);
            else if constexpr (type == 2) return Formatter<T>::preparse(arg, pattern);
            else if constexpr (type == 1) return Formatter<T>::preparse(arg, fl);
            else if constexpr (type == 0) return Formatter<T>::preparse(arg);
            else return call_formatter_parse<T>(p, fl, pattern, expand);
        }


        // 测试 Formatter<T>::format 接受的参数类型 Stream&, const T&, Format_Layout, [patten], [expand]
        template <typename T, typename Stream, typename = decltype(Formatter<T>::format(xyu::t_val<Stream&>(), xyu::t_val<const T&>(), Format_Layout{}, StringView{}, StringView{}))>
        xyu::int_c<5> test_formatter_format(char&&);
        template <typename T, typename Stream, typename = decltype(Formatter<T>::format(xyu::t_val<Stream&>(), xyu::t_val<const T&>(), StringView{}, StringView{}))>
        xyu::int_c<4> test_formatter_format(const char&&);
        template <typename T, typename Stream, typename = decltype(Formatter<T>::format(xyu::t_val<Stream&>(), xyu::t_val<const T&>(), Format_Layout{}, StringView{}))>
        xyu::int_c<3> test_formatter_format(const char&);
        template <typename T, typename Stream, typename = decltype(Formatter<T>::format(xyu::t_val<Stream&>(), xyu::t_val<const T&>(), StringView{}))>
        xyu::int_c<2> test_formatter_format(int&&);
        template <typename T, typename Stream, typename = decltype(Formatter<T>::format(xyu::t_val<Stream&>(), xyu::t_val<const T&>(), Format_Layout{}))>
        xyu::int_c<1> test_formatter_format(const int&&);
        template <typename T, typename Stream, typename = decltype(Formatter<T>::format(xyu::t_val<Stream&>(), xyu::t_val<const T&>()))>
        xyu::int_c<0> test_formatter_format(const int&);
        template <typename T, typename>
        xyu::int_c<-1> test_formatter_format(...);
        template <typename T, typename Stream>
        constexpr int get_formatter_format_type = decltype(test_formatter_format<T, Stream>('0'))::value;
        // 测试 Formatter<T>::format 是否接受 Format_Layout
        template <typename T, typename Stream>
        constexpr bool is_formatter_format_use_layout = get_formatter_format_type<T, Stream> & 1;

        // 封装调用 Formatter<T>::format
        template <typename U, typename Stream>
        void call_formatter_format(Stream& out, const void* p, const Format_Layout& ly, const StringView& pattern, const StringView& expand)
        {
            using T = xyu::t_remove_cvref<U>;
            auto& arg = *static_cast<const T*>(p);
            constexpr int type = get_formatter_format_type<T, Stream>;
            static_assert(type != -1, "Formatter<T>::format not found");
            if      constexpr (type == 5) Formatter<T>::format(out, arg, ly, pattern, expand);
            else if constexpr (type == 4) Formatter<T>::format(out, arg, pattern, expand);
            else if constexpr (type == 3) Formatter<T>::format(out, arg, ly, pattern);
            else if constexpr (type == 2) Formatter<T>::format(out, arg, pattern);
            else if constexpr (type == 1) Formatter<T>::format(out, arg, ly);
            else if constexpr (type == 0) Formatter<T>::format(out, arg);
        }


        // 测试 Formatter<T>::runtime
        template <typename T>
        xyu::bool_c<Formatter<T>::runtime> test_formatter_runtime(int);
        template <typename T>
        xyu::false_c test_formatter_runtime(...);
        template <typename T>
        constexpr bool is_formatter_runtime = decltype(test_formatter_runtime<xyu::t_remove_cvref<T>>(0))::value;
    }

    // 错误处理
    namespace __
    {
        /// 编译期错误
        enum FmtError : xyu::size_t
        {
            E_FMT_LIMIT = String::limit(),
            E_FMT_TOO_MANY_ARGS,
            E_FMT_TOO_FEW_ARGS,
            E_FMT_TOO_BIG_INDEX,
            E_FMT_TOO_BIG_WIDTH,
            E_FMT_UNCOMPLETED_PLACEHOLDER,
            E_FMT_UNCOMPLETED_BRACKET,
            E_FMT_FAILED_PARSE_ENV,
            E_FMT_FAILED_PARSE_LAY,
            E_FMT_FAILED_PARSE_PTNEX,
            E_FMT_DYFMT_NOT_NUMBER,
            E_EMT_DYFMT_UNALLOWED_TYPE,
        };

        // 编译期错误解析
        template <xyu::size_t VAL>
        constexpr void parser_constant_error()
        {
            constexpr auto V = static_cast<FmtError>(VAL);
            static_assert(V != E_FMT_TOO_MANY_ARGS, "too many arguments");
            static_assert(V != E_FMT_TOO_FEW_ARGS, "too few arguments");
            static_assert(V != E_FMT_TOO_BIG_INDEX, "argument's index out of range");
            static_assert(V != E_FMT_TOO_BIG_WIDTH, "width out of range");
            static_assert(V != E_FMT_UNCOMPLETED_PLACEHOLDER, "uncompleted {{}} placeholder");
            static_assert(V != E_FMT_UNCOMPLETED_BRACKET, "uncompleted () bracket");
            static_assert(V != E_FMT_FAILED_PARSE_ENV, "failed to parse environment-fmt");
            static_assert(V != E_FMT_FAILED_PARSE_LAY, "failed to parse layout-fmt");
            static_assert(V != E_FMT_FAILED_PARSE_PTNEX, "failed to parse pattern-fmt or expand-fmt");
            static_assert(V != E_FMT_DYFMT_NOT_NUMBER, "dynamic-fmt's index is not a number");
            static_assert(V != E_EMT_DYFMT_UNALLOWED_TYPE, "dynamic-fmt's type is not allowed"); // 字符/字符串/整数
            static_assert(V <= E_FMT_LIMIT, "error");
        }
    }

    // 辅助函数
    namespace __
    {
        // 检查字符是否为数字
        constexpr bool is_digit(char c) noexcept { return c >= '0' && c <= '9'; }

        // 检测动态参数是否允许
        template <typename T>
        constexpr bool is_allowed_dyn_arg =
                xyu::t_is_same<T, char> || xyu::t_can_icast<T, StringView> || xyu::t_is_mathint<T>;
    }

    /// 格式化管理器
    namespace __
    {
        /**
         * @brief 格式化管理器
        * @internal
        * @brief 核心的格式化字符串解析与分派管理器。
        * @details
        *   这个内部类包含了四个核心的静态方法：
        *   - `prepare`: 在编译期解析格式字符串，预计算长度，检查基本语法错误，
        *                并分离出需要运行期处理的部分。
        *   - `preparse`: 在运行期处理 `prepare` 分离出的动态参数部分，计算其长度。
        *   - `parse`: 在运行期完整地解析格式字符串，计算总长度并检查错误。
        *   - `format`: 在运行期解析格式字符串，并将格式化结果写入输出流。
        */
        struct FmtManager
        {
        private:
            template <xyu::size_t N>
            struct Preret
            {
                xyu::size_t len;    // 编译期预留长度
                xyu::size_t last;   // 运行期或动态参数长度
                bool dynamic;       // 是否有动态参数
                char buf[N];        // 动态参数缓存
            };
        public:
            /**
             * @brief 预解析格式化字符串
             * @return 预解析成功则返回 Preret，失败则返回 FmtError (len参数)
             * @note fmt 组成: { [env] [|lay] [:ptn] [?ex] } 每个部分可省略，但顺序固定
             * @note 仅编译期调用，解析失败会编译失败 (ptn及ex部分解析错误返回-1)
             * @note 复用索引不会使原始索引增加，否则索引会是最小的未使用过的索引 ，即 {}{0}{} => {0}{0}{1}， {}{2}{}{} => {0}{2}{1}{3}
             */
            template <xyu::size_t N, typename... Args>
            constexpr static Preret<N> prepare(const StringView& fmt) noexcept
            {
                if (XY_UNLIKELY(fmt.empty())) return {sizeof...(Args) == 0 ? 0 : E_FMT_TOO_MANY_ARGS};

                // 预解析器
                using prepare_t = xyu::size_t(*const)(const Format_Layout&, const StringView&, const StringView&);
                constexpr prepare_t prepares[] = { call_formatter_prepare<Args>... };
                // 预解析器是否已经使用过
                bool used[sizeof...(Args)] = {};

                Preret<N> result {};
                // 是否存在动态参数
                bool& dynamic = result.dynamic;
                // 检测动态参数类型是否允许
                constexpr bool dyn_allowed[]= { is_allowed_dyn_arg<Args>... };
                // 是否需要运行期解析
                constexpr bool need_runtime[] = { is_formatter_runtime<Args>... };

                /// 循环解析
                char (&buf)[N] = result.buf;        // 动态解析参数
                xyu::size_t& total = result.len;    // 总可能长度
                xyu::size_t& last = result.last;    // 运行期或动态参数长度
                xyu::size_t index = 0;              // 参数递增索引
                xyu::size_t i = 0, j = 0, k = 0;
                // 查找'{' >> j
                while ((j = fmt.find('{', i)) != -1)
                {
                    // 不完整的模式
                    if (XY_UNLIKELY(j == fmt.size() - 1)) return {E_FMT_UNCOMPLETED_PLACEHOLDER};
                    // 查找中间可能出现的 '}' 转义 或 错误格式 >> i
                    total += j - i;
                    while ((i = fmt.find('}', i, j)) != -1)
                    {
                        // 不完整的模式 且 不是转义 '}'
                        if (XY_UNLIKELY(++i == j || fmt.get(i) != '}')) return {E_FMT_UNCOMPLETED_PLACEHOLDER};
                        ++i;
                        --total; // 转义 '}'
                    }
                    // 查找另一半'}' >> i
                    if (fmt.get(++j) != '{')
                    {
                        i = fmt.find('}', j);
                        // 不完整的模式
                        if (XY_UNLIKELY(i == -1)) return {E_FMT_UNCOMPLETED_PLACEHOLDER};
                        // 查找动态参数
                        k = j + 1;
                        bool runtime = false;
                        while ((k = fmt.find('{', k, i)) != -1)
                        {
                            // 动态参数必须为 空的 "{}" 或 带索引的 "{n}"
                            while (++k != i && fmt.get(k) == ' ');  // 前空格
                            // 指定索引
                            xyu::size_t cur = index;
                            if (k != i) {
                                cur = 0;
                                for (;;) {
                                    if (!is_digit(fmt.get(k))) return {E_FMT_DYFMT_NOT_NUMBER};
                                    cur = cur * 10 + (fmt.get(k) - '0');
                                    if (++k == i) break;
                                    // 尾空格
                                    if (fmt.get(k) == ' ') {
                                        while (++k != i) if (fmt.get(k) != ' ') return {E_FMT_DYFMT_NOT_NUMBER};
                                        break;
                                    }
                                }
                            }
                            if (XY_UNLIKELY(cur >= sizeof...(Args))) return {E_FMT_TOO_BIG_INDEX};
                            if (XY_UNLIKELY(!dyn_allowed[cur])) return {E_EMT_DYFMT_UNALLOWED_TYPE};
                            // 更新参数
                            if (!used[cur]) {
                                if (cur == index) do ++index; while (index < sizeof...(Args) && used[index]);
                                used[cur] = true;
                            }
                            // 寻找下一个 '}'
                            i = fmt.find('}', ++k);
                            if (XY_UNLIKELY(i == -1)) return {E_FMT_UNCOMPLETED_PLACEHOLDER};
                            runtime = true;
                            dynamic = true;
                        }

                        /// 解析模式
                        StringView partfmt = fmt.subview(j, i - j);
                        Format_Spec spec = parse_spec(partfmt);
                        if (!spec.success) return {E_FMT_UNCOMPLETED_BRACKET};
                        // 解析 env (禁止传递动态参数)
                        xyu::size_t cur = parse_env(spec.env, index, total);
                        if (XY_UNLIKELY(cur == -1)) return {E_FMT_FAILED_PARSE_ENV};
                        // 索引过大
                        if (XY_UNLIKELY(cur >= sizeof...(Args))) return {E_FMT_TOO_BIG_INDEX};
                        // 更新参数
                        if (!used[cur]) {
                            if (cur == index) do ++index; while (index < sizeof...(Args) && used[index]);
                            used[cur] = true;
                        }
                        // 运行期解析
                        if (runtime || need_runtime[cur])
                        {
                            buf[last++] = '{';
                            for (char c : partfmt.range()) buf[last++] = c;
                            buf[last++] = '}';
                        }
                        else {
                        // 编译期解析
                            // 解析 lay
                            Format_Layout fl = parse_lay(spec.lay);
                            if (XY_UNLIKELY(fl.width == -1)) return {E_FMT_FAILED_PARSE_LAY};
                            else if (XY_UNLIKELY(fl.width == -2)) return {E_FMT_TOO_BIG_WIDTH};
                            // 解析 ptn, ex
                            xyu::size_t add = prepares[cur](fl, spec.ptn, spec.ex);
                            // 解析失败
                            if (XY_UNLIKELY(add == -1)) return {E_FMT_FAILED_PARSE_PTNEX};
                            else total += xyu::max(add, fl.width);
                        }
                        ++i;
                    } else {
                        i = j + 1;
                        ++total; // 转义 '{'
                    }
                }
                // 不准浪费
                if (index != sizeof...(Args)) return {E_FMT_TOO_MANY_ARGS};
                // 处理末尾 '}' 转义 或 错误格式 .i
                total += fmt.size() - i;
                while ((i = fmt.find('}', i)) != -1)
                {
                    // 不完整的模式 且 不是转义 '}'
                    if (XY_UNLIKELY(++i == j || fmt.get(i) != '}')) return {E_FMT_UNCOMPLETED_PLACEHOLDER};
                    ++i;
                    --total; // 转义 '}'
                }
                // 返回总长度
                return result;
            }

            /**
             * @brief 解析动态参数部分
             * @param fmt 预解析(prepare)后剩余的格式化字符串 (runtime的类型，或存在动态参数的占位符)
             */
            template <bool dynamic = true, typename... Args>
            static xyu::size_t preparse(const StringView& fmt, const Args&... args)
            {
                // 解析器
                using parser_t = xyu::size_t(*const)(const void*, const Format_Layout&, const StringView&, const StringView&);
                constexpr parser_t parsers[] = { call_formatter_preparse<Args>... };
                // 解析器是否已经使用过
                bool used[sizeof...(Args)] = {};
                // 参数
                const void* pargs[] = { &args... };

                // 动态格式解析器
                using dynps_t = void(*const)(String&, const void*);
                constexpr dynps_t dynps[] = { parser_dyn<Args>... };
                // 是否需要运行期解析
                constexpr bool need_runtime[] = { is_formatter_runtime<Args>... };

                /// 循环解析
                String dybuf;           // 动态参数缓存
                xyu::size_t total = 0;  // 总可能长度
                xyu::size_t index = 0;  // 参数递增索引
                xyu::size_t i = 0, j = 0, k, p;
                // 查找'{' >> j
                do {
                    // 查找另一半'}' >> i
                    i = fmt.find('}', j);
                    StringView partfmt = fmt.subview(j, i - j);
                    bool runtime = false;
                    // 查找动态参数
                    if constexpr (dynamic)
                    {
                        k = j + 1;
                        if ((k = fmt.find('{', k, i)) != -1)
                        {
                            runtime = true;
                            p = j;
                            dybuf.clear();
                            do {
                                dybuf << fmt.subview(p, k - p);
                                // 动态参数必须为 空的 "{}" 或 带索引的 "{n}"
                                while (++k != i && fmt.get(k) == ' ');  // 跳过前空格
                                // 指定索引
                                xyu::size_t cur;
                                if (k != i)
                                    for (cur = 0;;) {
                                        cur = cur * 10 + (fmt.get(k) - '0');
                                        if (++k == i) break;
                                        if (fmt.get(k) == ' ') { k = i; break; }
                                    }
                                else cur = index;
                                dynps[cur](dybuf, pargs[cur]);
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
                            partfmt = dybuf.view();
                        }
                    }

                    /// 解析模式
                    Format_Spec spec = parse_spec(partfmt);
                    // 解析 env
                    xyu::size_t cur = parse_env_idx(spec.env, index);
                    if (!runtime || cur == index) while (!need_runtime[cur]) used[cur++] = true;
                    // 解析 lay
                    Format_Layout fl = parse_lay(spec.lay);
                    // 解析 ptn, ex
                    xyu::size_t add = parsers[cur](pargs[cur], fl, spec.ptn, spec.ex);
                    // ptn 解析失败 (具体异常信息应该自行处理，否则使用这个退化版)
                    if (XY_UNLIKELY(add > E_FMT_LIMIT)) {
                        xylogei(false, "E_Format_PtnEx: failed to parse pattern-fmt or expand-fmt");
                        throw xyu::E_Format_PtnEx{};
                    }
                    // 更新参数
                    else total += xyu::max(add, fl.width);
                    if (!used[cur]) {
                        if (cur == index) do ++index; while (index < sizeof...(Args) && used[index]);
                        used[cur] = true;
                    }
                    j = i + 1;
                } while (j < fmt.size());
                // 返回总长度
                return total;
            }

            /**
             * @brief 解析格式化字符串
             * @return 解析成功则返回可能的总长度，失败则返回-1 (无符号类型)
             * @exception E_Format 格式化错误
             * @note fmt 组成: { [env] [|lay] [:ptn] [?ex] } 每个部分可省略，但顺序固定
             * @note 运行期调用，解析失败会抛出异常 (ptn及ex部分解析错误自行输出异常信息，否则返回-1使用退化版)
             * @note 复用索引不会使原始索引增加，否则索引会是最小的未使用过的索引 ，即 {}{0}{} => {0}{0}{1}， {}{2}{}{} => {0}{2}{1}{3}
             */
            template <typename... Args>
            static xyu::size_t parse(const StringView& fmt, const Args&... args)
            {
                if (XY_UNLIKELY(fmt.empty())) {
                    if constexpr (sizeof...(Args) == 0) return 0;
                    xylogei(false, "E_Format_Argument: too many arguments for empty format string");
                    throw xyu::E_Format_Argument{};
                }

                // 解析器
                using parser_t = xyu::size_t(*const)(const void*, const Format_Layout&, const StringView&, const StringView&);
                constexpr parser_t parsers[] = { call_formatter_parse<Args>... };

                // 动态格式解析器
                using dynps_t = void(*const)(String&, const void*);
                constexpr dynps_t dynps[] = { parser_dyn<Args>... };

                // 解析器是否已经使用过
                bool used[sizeof...(Args)] = {};

                // 参数
                const void* pargs[] = { &args... };

                /// 循环解析
                String dybuf;           // 动态参数缓存
                xyu::size_t total = 0;  // 总可能长度
                xyu::size_t index = 0;  // 参数递增索引
                xyu::size_t i = 0, j, k, p;
                // 查找 '{'>j
                while ((j = fmt.find('{', i)) != -1)
                {
                    // 不完整的模式
                    if (XY_UNLIKELY(j == fmt.size() - 1)) {
                        xylogei(false, "E_Format_Syntax: uncompleted {{}} placeholder");
                        throw xyu::E_Format_Syntax{};
                    }
                    // 查找中间可能出现的 '}' 转义 或 错误格式 .i
                    total += j - i;
                    while ((i = fmt.find('}', i, j)) != -1)
                    {
                        // 不完整的模式 且 不是转义 '}'
                        if (XY_UNLIKELY(++i == j || fmt.get(i) != '}')) {
                            xylogei(false, "E_Format_Syntax: uncompleted {{}} placeholder");
                            throw xyu::E_Format_Syntax{};
                        }
                        ++i;
                        --total; // 转义 '}'
                    }
                    // 查找另一半 '}'>i
                    if (fmt.get(++j) != '{')
                    {
                        i = fmt.find('}', j);
                        // 不完整的模式
                        if (XY_UNLIKELY(i == -1)) {
                            xylogei(false, "E_Format_Syntax: uncompleted {{}} placeholder");
                            throw xyu::E_Format_Syntax{};
                        }
                        // 查找动态参数
                        StringView partfmt;
                        k = j + 1;
                        if ((k = fmt.find('{', k, i)) != -1)
                        {
                            p = j;
                            dybuf.clear();
                            do {
                                dybuf << fmt.subview(p, k - p);
                                // 动态参数必须为 空的 "{}" 或 带索引的 "{n}"
                                while (++k != i && fmt.get(k) == ' ');  // 前空格
                                // 指定索引
                                xyu::size_t cur;
                                if (k != i) {
                                    for (cur = 0;;) {
                                        if (!is_digit(fmt.get(k))) {
                                            xylogei(false, "E_Format_Dynamic: index is not a number");
                                            throw xyu::E_Format_Dynamic{};
                                        }
                                        cur = cur * 10 + (fmt.get(k) - '0');
                                        if (++k == i) break;
                                        // 尾空格
                                        if (fmt.get(k) == ' ') {
                                            while (++k != i) if (fmt.get(k) != ' ') {
                                                    xylogei(false, "E_Format_Dynamic: index is not a number");
                                                    throw xyu::E_Format_Dynamic{};
                                                }
                                            break;
                                        }
                                    }
                                }
                                else cur = index;
                                if (XY_UNLIKELY(cur >= sizeof...(Args))) {
                                    if constexpr (sizeof...(Args) == 0) xylogei(false, "E_Format_Argument: no arguments for dynamic placeholder");
                                    else xylogei(false, "E_Format_Argument: too few arguments for dynamic placeholder");
                                    throw xyu::E_Format_Argument{};
                                }
                                dynps[cur](dybuf, pargs[cur]);
                                // 更新参数
                                if (!used[cur]) {
                                    if (cur == index) do ++index; while (index < sizeof...(Args) && used[index]);
                                    used[cur] = true;
                                }
                                // 寻找下一个 '}'
                                p = ++k;
                                i = fmt.find('}', p);
                                if (XY_UNLIKELY(i == -1)) {
                                    xylogei(false, "E_Format_Syntax: uncompleted {{}} placeholder");
                                    throw xyu::E_Format_Syntax{};
                                }
                            }
                            while ((k = fmt.find('{', k, i)) != -1);
                            dybuf << fmt.subview(p, i - p);
                            partfmt = dybuf.view();
                        }
                        else partfmt = fmt.subview(j, i - j);

                        /// 解析模式
                        Format_Spec spec = parse_spec(partfmt);
                        // 解析 env
                        xyu::size_t cur = parse_env(spec.env, index, total);
                        if (XY_UNLIKELY(cur == -1)) {
                            xylogei(false, "E_Format_Environment: failed to parse environment-fmt");
                            throw xyu::E_Format_Environment{};
                        }
                        // 索引过大
                        if (XY_UNLIKELY(cur >= sizeof...(Args))) {
                            if constexpr (sizeof...(Args) == 0) xylogei(false, "E_Format_Argument: no arguments for placeholder");
                            else xylogei(false, "E_Format_Argument: too few arguments for placeholder");
                            throw xyu::E_Format_Argument{};
                        }
                        // 解析 lay
                        Format_Layout fl = parse_lay(spec.lay);
                        // 解析 ptn, ex
                        xyu::size_t add = parsers[cur](pargs[cur], fl, spec.ptn, spec.ex);
                        // ptn 解析失败 (具体异常信息应该自行处理，否则使用这个退化版)
                        if (XY_UNLIKELY(add > E_FMT_LIMIT)) {
                            xylogei(false, "E_Format_PtnEx: failed to parse pattern-fmt or expand-fmt");
                            throw xyu::E_Format_PtnEx{};
                        }
                        // 更新参数
                        else total += xyu::max(add, fl.width);
                        if (!used[cur]) {
                            if (cur == index) do ++index; while (index < sizeof...(Args) && used[index]);
                            used[cur] = true;
                        }
                        ++i;
                    }
                    else {
                        i = j + 1;
                        ++total; // 转义 '{'
                    }
                }
                // 不准浪费
                if (index != sizeof...(Args)) {
                    xylogei(false, "E_Format_Argument: too many arguments for format string");
                    throw xyu::E_Format_Argument{};
                }
                // 处理末尾 '}' 转义 或 错误格式 .i
                total += fmt.size() - i;
                while ((i = fmt.find('}', i)) != -1)
                {
                    // 不完整的模式 且 不是转义 '}'
                    if (XY_UNLIKELY(++i == j || fmt.get(i) != '}')) {
                        xylogei(false, "E_Format_Syntax: uncompleted {{}} placeholder");
                        throw xyu::E_Format_Syntax{};
                    }
                    ++i;
                    --total; // 转义 '}'
                }
                // 返回总长度
                return total;
            }

            /**
             * @brief 格式化输出
             * @tparam dynamic 是否存在动态参数 (prepare 编译期预解析检测)
             * @exception E_Format 格式化错误
             * @note out 为被 StreamWarpper 封装的任意对象 (String, Vector, File, ...)
             * @note fmt 组成: { [env] [|lay] [:ptn] [?ex] } 每个部分可省略，但顺序固定
             * @note 运行期调用，省去一些检查 (由 prepare 或 parse 检查通过的部分)
             * @note 复用索引不会使原始索引增加，否则索引会是最小的未使用过的索引 ，即 {}{0}{} => {0}{0}{1}， {}{2}{}{} => {0}{2}{1}{3}
             */
            template <bool dynamic = true, typename... Args, typename Stream>
            static void format(Stream& out, const StringView& fmt, const Args&... args)
            {
                if (XY_UNLIKELY(fmt.empty())) return;

                // 格式化器
                using format_t = void(*const)(Stream&, const void*, const Format_Layout&, const StringView&, const StringView&);
                constexpr format_t formats[] = { call_formatter_format<Args, Stream>... };

                using sstream_t = decltype(Stream_Out(xyu::t_val<String&>()));
                using format_s = void(*const)(sstream_t&, const void*, const Format_Layout&, const StringView&, const StringView&);
                constexpr format_s formatss[] = { call_formatter_format<Args, sstream_t>... };

                // 格式化器是否使用 Format_Layout
                constexpr bool dynused[sizeof...(Args)] = { is_formatter_format_use_layout<Args, Stream>... };

                // 动态格式解析器
                using dynps_t = void(*const)(String&, const void*);
                constexpr dynps_t dynps[] = { parser_dyn<Args>... };

                // 解析器是否已经使用过
                bool used[sizeof...(Args)] = {};

                // 参数
                const void* pargs[] = { &args... };

                /// 循环解析
                String dybuf;           // 动态参数缓存
                xyu::size_t index = 0;  // 参数递增索引
                xyu::size_t i = 0, j, k, p;
                // 查找 '{'>j
                while ((j = fmt.find('{', i)) != -1)
                {
                    // 查找中间可能出现的 '}' 转义
                    while ((k = fmt.find('}', i, j)) != -1)
                    {
                        out << fmt.subview(i, ++k - i);
                        i = ++k;
                    }
                    out << fmt.subview(i, j - i);
                    // 查找另一半 '}'>i
                    if (fmt.get(++j) != '{')
                    {
                        i = fmt.find('}', j);
                        // 查找动态参数
                        StringView partfmt = fmt.subview(j, i - j);
                        if constexpr (dynamic)
                        {
                            k = j + 1;
                            if ((k = fmt.find('{', k, i)) != -1)
                            {
                                p = j;
                                dybuf.clear();
                                do {
                                    dybuf << fmt.subview(p, k - p);
                                    // 动态参数必须为 空的 "{}" 或 带索引的 "{n}"
                                    while (++k != i && fmt.get(k) == ' ');  // 跳过前空格
                                    // 指定索引
                                    xyu::size_t cur;
                                    if (k != i)
                                        for (cur = 0;;) {
                                            cur = cur * 10 + (fmt.get(k) - '0');
                                            if (++k == i) break;
                                            if (fmt.get(k) == ' ') { k = i; break; }
                                        }
                                    else cur = index;
                                    dynps[cur](dybuf, pargs[cur]);
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
                                partfmt = dybuf.view();
                            }
                        }

                        /// 解析模式
                        Format_Spec spec = parse_spec(partfmt);
                        // 解析 env
                        xyu::size_t cur = parse_env_idx(spec.env, index);
                        // 解析 lay
                        Format_Layout fl = parse_lay(spec.lay);
                        // 解析 ptn, ex
                        if (dynused[cur] || !fl.width) formats[cur](out, pargs[cur], fl, spec.ptn, spec.ex);
                        else {
                            String str;
                            auto ss = Stream_Out(str);
                            // 格式化
                            formatss[cur](ss, pargs[cur], fl, spec.ptn, spec.ex);
                            // 对齐
                            StringView view = str.view();
                            if (fl.width > str.size())
                            {
                                xyu::uint flen = fl.width - view.size();
                                switch (fl.align)
                                {
                                    case Format_Layout::LEFT: out.write(fl.fill, flen).write(view); break;
                                    case Format_Layout::CENTER: {
                                        xyu::uint llen = flen / 2; // 偏右填充，整体偏左
                                        xyu::uint rlen = flen - llen;
                                        out.write(fl.fill, llen).write(view).write(fl.fill, rlen);
                                        break;
                                    }
                                    default: out.write(view).write(fl.fill, flen); break; // 默认右对齐
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
                // 处理末尾 '}' 转义 或 错误格式 .i
                while ((k = fmt.find('}', i)) != -1)
                {
                    out << fmt.subview(i, ++k - i);
                    i = ++k;
                }
                out << fmt.subview(i, fmt.size() - i);
            }

        private:
            /// 解析模式
            struct Format_Spec { StringView env, lay, ptn, ex; bool success = true; };
            constexpr static Format_Spec parse_spec(const StringView& spec)
            {
                /// 解析模式
                // 查找 '|' >> s1 , ':' >> s2 , '?' >> s3
                xyu::size_t s1 = spec.find('|');
                xyu::size_t s2 = spec.find(':');
                xyu::size_t s3 = spec.find('?');
                if (s3 == -2) return {{},{},{},{}, false};
                // {env|lay:ptn?ex}
                // .j..1...2...3..i
                xyu::size_t m3 = xyu::min(s3, spec.size());
                xyu::size_t m2 = xyu::min(s2, m3);
                xyu::size_t m1 = xyu::min(s1, m2);
                StringView env = spec.subview(0, m1).strip(' ');
                StringView lay = m2 == m1 ? StringView{} : spec.subview(m1 + 1, m2 - m1 - 1).rstrip(' ');
                StringView ptn = m3 == m2 ? StringView{} : spec.subview(m2 + 1, m3 - m2 - 1);
                StringView ex  = spec.size() == m3 ? StringView{} : spec.subview(m3 + 1, spec.size() - m3 - 1);
                return { env, lay, ptn, ex };
            }

            /**
             * @brief 检测 环境格式符
             * @return 返回索引值，无效则返回-1
             * @sign n 索引值 (如果有必须以索引值为开头)
             * @sign +n 额外预分配的容量
             * @example "0", "4+48"
             */
            constexpr static xyu::size_t parse_env(const StringView& env, xyu::size_t index, xyu::size_t& total) noexcept
            {
                if (env.empty()) return index;

                // 解析索引
                xyu::size_t ix = 0;
                xyu::size_t j = 0;
                if (!is_digit(env.get(j))) ix = index;
                else {
                    for (;;)
                    {
                        ix = ix * 10 + (env.get(j) - '0');
                        if (++j == env.size()) return ix;
                        if (!is_digit(env.get(j))) break;
                    }
                }

                // 解析其他
                xyu::size_t mode = 0, newmode = 0;
                xyu::size_t tmp = 0;
                for(;; ++j)
                {
                    // 模式结算
                    if (newmode != mode || j == env.size())
                    {
                        // 模式 - 额外预分配
                        if (mode == 1) total += tmp;
                        // 更新参数
                        mode = newmode;
                        tmp = 0;
                        // 解析完毕
                        if (j == env.size()) break;
                    }

                    // 读取参数
                    char c = env.get(j);

                    // --模式切换--
                    // 增加预分配容量
                    if (c == '+') newmode = 1;
                    // --模式控制--
                    // --解析数字--
                    else if (is_digit(c))
                    {
                        // 不存在的模式
                        if (XY_UNLIKELY(!is_digit(c))) return -1;
                        // 模式 - 额外预分配
                        if (mode == 1) tmp = tmp * 10 + (c - '0');
                    }
                    // --模式阻断--
                    else if (c == ' ') newmode = 0;
                    // --未知参数--
                    else return -1;
                }

                // 返回索引
                return ix;
            }

            /**
             * @brief 检测 环境格式符
             * @return 返回索引值，无效则返回-1
             */
            static xyu::size_t parse_env_idx(const StringView& env, xyu::size_t index) noexcept
            {
                if (env.empty() || !is_digit(env.get(0))) return index;

                // 解析索引
                xyu::size_t ix = 0;
                xyu::size_t j = 0;
                for (;;)
                {
                    ix = ix * 10 + (env.get(j) - '0');
                    if (++j == env.size() || !is_digit(env.get(j))) return ix;
                }
            }

            /**
             * @brief 检测 布局格式符
             * @return 返回解析后的 Format_Layout，无效则抛出异常
             * @exception E_Format_Layout 对齐格式化错误
             * @support [ fill [ align ]][ width ]
             * @sign fill 填充字符 (默认为 ' ')
             * @sign align 对齐方式 (左对齐 '<', 右对齐 '>', 居中对齐 '^', 数字对齐 '=')
             * @sign width 宽度 (默认为 0，即自动宽度)
             * @example "12", ">12", "0=12", " ^8"
             */
            constexpr static Format_Layout parse_lay(const StringView& layout)
            {
                if (layout.empty()) return 0;

                // 单字符
                if (layout.size() == 1) {
                    if (!is_digit(layout.get(0))) {
                        if (xyu::t_constant()) return -1;
                        else {
                            xylogei(false, "E_Format_Layout: specified layout without width");
                            throw xyu::E_Format_Layout();
                        }
                    }
                    else return layout.get(0) - '0';
                }

                Format_Layout fl;

                // [[ fill ] align ]
                xyu::size_t ia = !is_digit(layout.get(1)) ? 1 : (!is_digit(layout.get(0)) ? 0 : -1);
                if (ia != -1)
                {
                    // [fill]
                    if (ia > 0) fl.fill = layout.get(ia - 1);
                    // [align]
                    fl.align = Format_Layout::parse_align(layout.get(ia));
                    // 未知的对齐方式
                    if (XY_UNLIKELY(fl.align == Format_Layout::UNKNOWN)) {
                        if (xyu::t_constant()) return -1;
                        else {
                            xylogei(false, "E_Format_Layout: unknown align kind");
                            throw xyu::E_Format_Layout();
                        }
                    }
                    // 指定了对齐方式却没有宽度
                    if (XY_UNLIKELY(ia + 1 == layout.size())) {
                        if (xyu::t_constant()) return -1;
                        else {
                            xylogei(false, "E_Format_Layout: specified align without width");
                            throw xyu::E_Format_Layout();
                        }
                    }
                }

                // [width]
                xyu::size_t width = 0;
                for (xyu::size_t i = ia + 1; i < layout.size(); ++i)
                {
                    if (XY_UNLIKELY(!is_digit(layout.get(i)))) {
                        if (xyu::t_constant()) return -1;
                        else {
                            xylogei(false, "E_Format_Layout: width is not a number");
                            throw xyu::E_Format_Layout();
                        }
                    }
                    width = width * 10 + (layout.get(i) - '0');
                    if (XY_UNLIKELY(width > xyu::number_traits<int>::max)) {
                        if (xyu::t_constant()) return -2;
                        else {
                            xylogei(false, "E_Format_Layout: width {} is over limit {}", width, xyu::number_traits<int>::max);
                            throw xyu::E_Format_Layout();
                        }
                    }
                }
                fl.width = width;

                return fl;
            }

        private:
            // 动态解析格式器
            template <typename T>
            static void parser_dyn(String& buf, const void* p)
            {
                auto& arg = *static_cast<const T*>(p);
                // 字符 or 字符串
                if constexpr (xyu::t_is_same<T, char> || xyu::t_can_icast<T, StringView>) buf << arg;
                // 整数
                else if constexpr (xyu::t_is_mathint<T>) {
                    constexpr char digits[201] =
                            "0001020304050607080910111213141516171819"
                            "2021222324252627282930313233343536373839"
                            "4041424344454647484950515253545556575859"
                            "6061626364656667686970717273747576777879"
                            "8081828384858687888990919293949596979899";
                    T v = arg;
                    while (v >= 100) buf << StringView(digits + (v % 100) * 2, 2), v /= 100;
                    if    (v >= 10)  buf << StringView(digits + v * 2, 2);
                    else             buf << ('0' + v);
                }
                // 不支持的类型
                else {
                    xylogei(false, "E_Format_Dynamic: unsupported dynamic argument type (should be char, integer or string-like)");
                    throw xyu::E_Format_Dynamic();
                }
            }

            // 宏辅助函数 (防止编译器因 Args... 为非编译期参数而报错)
        public:
            template <xyu::size_t N, typename Tp>
            constexpr static Preret<N> prepare_tp(const StringView& fmt) noexcept
            { return prepare_tp_help<N>(fmt, Tp{}); }
        private:
            template <xyu::size_t N, typename... Args>
            constexpr static Preret<N> prepare_tp_help(const StringView& fmt, xyu::typelist_c<Args...>) noexcept
            { return prepare<N, Args...>(fmt); }
        };
    }
}

#pragma clang diagnostic pop