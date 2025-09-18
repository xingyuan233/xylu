#pragma clang diagnostic push
#pragma ide diagnostic ignored "OCUnusedStructInspection"
#pragma ide diagnostic ignored "OCUnusedGlobalDeclarationInspection"
#pragma ide diagnostic ignored "NotImplementedFunctions"
#pragma ide diagnostic ignored "google-explicit-constructor"
#pragma once

#include "../../link/string"

/* 流包装器 */

namespace xylu::xystring
{
    namespace __
    {
        // 测试扩容函数
        template <typename S, typename = decltype(xyu::t_val<S>().reserve(xyu::size_t{}))>
        xyu::true_c has_reverse_test(int);
        template <typename>
        xyu::false_c has_reverse_test(...);
        template <typename S>
        constexpr bool has_reverse = decltype(has_reverse_test<S>(0))::value;
        
        // 测试获取当前大小函数
        template <typename S, typename = decltype(xyu::t_val<S>().size())>
        xyu::int_c<1> has_count_test(char);
        template <typename S, typename = decltype(xyu::t_val<S>().length())>
        xyu::int_c<2> has_count_test(int);
        template <typename S, typename = decltype(xyu::t_val<S>().count())>
        xyu::int_c<3> has_count_test(long);
        template <typename>
        xyu::int_c<0> has_count_test(...);
        template <typename S>
        constexpr int has_count = decltype(has_count_test<S>('0'))::value;

        // 测试写入函数
        template <typename S, typename T, typename V = decltype(xyu::t_val<S&>() << xyu::t_val<T>(), xyu::success_t{}), typename = xyu::t_enable<xyu::t_is_same<V, xyu::success_t>>>
        xyu::int_c<1> test_stream_write(char&&);    // (防止匹配到传递两个模板参数的类型)
        template <typename S, typename... T, typename = decltype(xyu::t_val<S&>().write(xyu::t_val<T>()...))>
        xyu::int_c<2> test_stream_write(const char&&);
        template <typename S, typename... T, typename = decltype(xyu::t_val<S&>().append(xyu::t_val<T>()...))>
        xyu::int_c<3> test_stream_write(const char&);
        template <typename S, typename... T, typename = decltype(xyu::t_val<S&>().push(xyu::t_val<T>()...))>
        xyu::int_c<4> test_stream_write(int&&) noexcept;
        template <typename S, typename... T, typename = decltype(xyu::t_val<S&>().emplace_back(xyu::t_val<T>()...))>
        xyu::int_c<5> test_stream_write(const int&&);
        template <typename S, typename... T, typename = decltype(xyu::t_val<S&>().push_back(xyu::t_val<T>()...))>
        xyu::int_c<6> test_stream_write(double);
        template <typename, typename... T>
        xyu::int_c<0> test_stream_write(...);
        template <typename S, typename... T>
        constexpr int get_stream_write_type = decltype(test_stream_write<S, T...>('0'))::value;
    }

    /**
     * @brief 一个通用的输出流适配器，为不同类型的输出目标提供统一的流式写入接口。
     * @details
     *   `Stream_Out` 包装了一个输出目标对象 `S`（如 `xyu::String`, `xyu::File` 等），
     *   并为其提供了统一的 `write()` 和 `operator<<` 方法。
     *
     *   它通过 SFINAE 在编译期自动探测被包装对象 `S` 支持的写入方法，并按以下
     *   优先级顺序进行适配：
     *   1. `s << value`
     *   2. `s.write(value)`
     *   3. `s.append(value)`
     *   4. `s.push(value)`
     *   5. `s.emplace_back(value)`
     *   6. `s.push_back(value)`
     *
     *   这使得 `xyfmt` 等格式化工具可以将结果无缝地输出到任何实现了上述接口之一的对象中。
     *
     * @tparam S 被包装的输出流对象的类型。
     */
    template <typename S>
    class Stream_Out
    {
    private:
        S& s;   // 被包装的流对象

    public:
        /**
         * @brief 构造函数
         * @param s 被包装的流对象
         */
        constexpr Stream_Out(S& s) noexcept : s{s} {}

        /**
         * @brief 确保空间还能容纳 bytes 个字节
         * @param bytes 字节数
         * @note 基于 reserve() 的实现，通过 size()/length()/count() 获取当前容量
         * @note 如果流不支持或不需要扩容则不做处理
         */
        constexpr Stream_Out& expand(xyu::size_t bytes) noexcept(expand_noexcept)
        {
            if constexpr (__::has_reverse<S>)
            {
                if constexpr (__::has_count<S> == 1) s.reserve(s.size() + bytes);
                else if constexpr (__::has_count<S> == 2) s.reserve(s.length() + bytes);
                else if constexpr (__::has_count<S> == 3) s.reserve(s.count() + bytes);
            }
            return *this;
        }
        
        /**
         * @brief 写入一个字符
         * @note 依次尝试调用 << / write() / append() / push() /emplace_back() / push_back()
         */
        constexpr Stream_Out& write(char c) noexcept(write_noexcept<char>)
        {
            constexpr int t = __::get_stream_write_type<S, char>;
            if constexpr (t == 1) s << c;
            else if constexpr (t == 2) s.write(c);
            else if constexpr (t == 3) s.append(c);
            else if constexpr (t == 4) s.push(c);
            else if constexpr (t == 5) s.emplace_back(c);
            else if constexpr (t == 6) s.push_back(c);
            else static_assert(t == 404, "stream not support to write char");
            return *this;
        }

        /**
         * @brief 写入多个字符
         * @note 依次尝试调用 << / write() / append() / push() /emplace_back() / push_back()
         * @note 自动尝试 (char, size_t) 及 (size_t, char) 版本
         */
        constexpr Stream_Out& write(char c, xyu::size_t n) noexcept(write_noexcept<char, xyu::size_t> || write_noexcept<xyu::size_t, char> || write_noexcept<char>)
        {
            constexpr int t1 = __::get_stream_write_type<S, char, xyu::uint>;
            if constexpr (t1 == 0) for (xyu::size_t i = 0; i < n; ++i) write(c);
            else if constexpr (t1 == 2) {
                if constexpr (fill_test_write<decltype(s.write(c, n))>::value) s.append(c, n);
                else s.append(n, c);
            } else if constexpr (t1 == 3) {
                if constexpr (fill_test_append<decltype(s.append(c, n))>::value) s.append(c, n);
                else s.append(n, c);
            } else if constexpr (t1 == 4) {
                if constexpr (fill_test_push<decltype(s.push(c, n))>::value) s.push(c, n);
                else s.push(n, c);
            } else if constexpr (t1 == 5) {
                if constexpr (fill_test_emplace_back<decltype(s.emplace_back(c, n))>::value) s.emplace_back(c, n);
                else s.emplace_back(n, c);
            } else if constexpr (t1 == 6) {
                if constexpr (fill_test_push_back<decltype(s.push_back(c, n))>::value) s.push_back(c, n);
                else s.push_back(n, c);
            }
            return *this;
        }

        /**
        * @brief 写入多个字符
        * @note 依次尝试调用 << / write() / append() / push() /emplace_back() / push_back()
        * @note 自动尝试 (char, size_t) 及 (size_t, char) 版本
        */
        constexpr Stream_Out& write(xyu::size_t n, char c) noexcept(noexcept(write(' ', 0))) { return write(c, n); }

        /**
         * @brief 写入字符串
         * @note 依次尝试调用 << / write() / append() / push() /emplace_back() / push_back()
         * @note 依次尝试作为 const StringView& / const char* /char 写入
         */
        template <typename T, xyu::t_enable<xyu::t_can_icast<T, StringView>, bool> = true>
        Stream_Out& write(T&& str) noexcept(write_noexcept<const StringView&> || write_noexcept<const char*> || write_noexcept<char>)
        {
            const StringView sv = str; 
            constexpr int t1 = __::get_stream_write_type<S, const StringView&>;
            if constexpr (t1 == 1) s << sv;
            else if constexpr (t1 == 2) s.write(sv);
            else if constexpr (t1 == 3) s.append(sv);
            else if constexpr (t1 == 4) s.push(sv);
            else if constexpr (t1 == 5) s.emplace_back(sv);
            else if constexpr (t1 == 6) s.push_back(sv);
            else
            {
                constexpr int t2 = __::get_stream_write_type<S, const char*>;
                if constexpr (t2)
                {
                    const char* p = sv.data();
                    bool cp = sv.get(sv.size()) != '\0';
                    if (cp && sv.size() <= 63)
                    {
                        // 短字符串开辟临时缓冲区
                        char buf[64];
                        xyu::mem_copy(buf, p, sv.size());
                        buf[sv.size()] = '\0';
                        p = buf;
                        if constexpr (t2 == 1) s << p;
                        else if constexpr (t2 == 2) s.write(p);
                        else if constexpr (t2 == 3) s.append(p);
                        else if constexpr (t2 == 4) s.push(p);
                        else if constexpr (t2 == 5) s.emplace_back(p);
                        else if constexpr (t2 == 6) s.push_back(p);
                        cp = false;
                    }
                    else if (cp)
                    {
                        // 长字符串分配动态内存
                        char* buf = xyu::alloc<char>(sv.size() + 1);
                        xyu::mem_copy(buf, sv.data(), sv.size());
                        buf[sv.size()] = '\0';
                        p = buf;
                    }
                    if constexpr (t2 == 1) s << p;
                    else if constexpr (t2 == 2) s.write(p);
                    else if constexpr (t2 == 3) s.append(p);
                    else if constexpr (t2 == 4) s.push(p);
                    else if constexpr (t2 == 5) s.emplace_back(p);
                    else if constexpr (t2 == 6) s.push_back(p);
                    if (cp) xyu::dealloc(const_cast<char*>(p), sv.size() + 1);
                }
                else
                {
                    constexpr int t3 = __::get_stream_write_type<S, char>;
                    static_assert(t3, "stream not support to write string");
                    for (char c : sv.range()) write(c);
                }
            }
            return *this;
        }

        /**
         * @brief 其他类型写入
         * @note 通过 xyfmt 格式化为 字符串 后写入
         */
        template <typename T, xyu::t_enable<!xyu::t_can_icast<T, StringView>, bool> = false>
        Stream_Out& write(const T& v) noexcept(noexcept(write(StringView{})));

        /// 写入转发
        template <typename T>
        Stream_Out& operator<<(T&& v) noexcept(noexcept(write(xyu::forward<T>(v)))) { return write(xyu::forward<T>(v)); }

    private:
        // 异常检测
        static constexpr bool expand_noexcept = []
        {
            if constexpr (__::has_reverse<S>)
            {
                if constexpr (__::has_count<S> == 1) return noexcept(xyu::t_val<S&>().reserve(xyu::t_val<S&>().size()));
                else if constexpr (__::has_count<S> == 2) return noexcept(xyu::t_val<S&>().reserve(xyu::t_val<S&>().length()));
                else if constexpr (__::has_count<S> == 3) return noexcept(xyu::t_val<S&>().reserve(xyu::t_val<S&>().count()));
            }
            return true;
        }();

        // 异常检测
        template <typename... T>
        static constexpr bool write_noexcept = []
        {
            constexpr int t = __::get_stream_write_type<S, T...>;
            if constexpr (t == 1) return noexcept((..., (xyu::t_val<S&>() << xyu::t_val<T>())));
            else if constexpr (t == 2) return noexcept(xyu::t_val<S&>().write(xyu::t_val<T>()...));
            else if constexpr (t == 3) return noexcept(xyu::t_val<S&>().append(xyu::t_val<T>()...));
            else if constexpr (t == 4) return noexcept(xyu::t_val<S&>().push(xyu::t_val<T>()...));
            else if constexpr (t == 5) return noexcept(xyu::t_val<S&>().emplace_back(xyu::t_val<T>()...));
            else if constexpr (t == 6) return noexcept(xyu::t_val<S&>().push_back(xyu::t_val<T>()...));
            else return false;
        }();

        // 测试多个字符传参顺序
#define XY_STREAM_FILL_TEST_HELP( fun, param, sfinae ) \
            template <typename T, typename = decltype(static_cast<Ret(T::*)(char, param)>(&T::fun))> \
            static xyu::true_c test(sfinae);
#define XY_STREAM_FILL_TEST( fun ) \
        template <typename Ret, typename Stream = S> \
        struct fill_test_##fun \
        { \
            XY_STREAM_FILL_TEST_HELP( fun, short, char&& ) \
            XY_STREAM_FILL_TEST_HELP( fun, unsigned short, const char&&) \
            XY_STREAM_FILL_TEST_HELP( fun, int, const char&)             \
            XY_STREAM_FILL_TEST_HELP( fun, unsigned int, int&&)    \
            XY_STREAM_FILL_TEST_HELP( fun, long, const int&&)            \
            XY_STREAM_FILL_TEST_HELP( fun, unsigned long, const int&)    \
            XY_STREAM_FILL_TEST_HELP( fun, long long, long long&&)       \
            XY_STREAM_FILL_TEST_HELP( fun, unsigned long long, const long long&&) \
            template <typename T>  \
            static xyu::false_c test(...);         \
            constexpr static bool value = decltype(test<Stream>('0'))::value; \
        };
        XY_STREAM_FILL_TEST( append )
        XY_STREAM_FILL_TEST( write )
        XY_STREAM_FILL_TEST( push )
        XY_STREAM_FILL_TEST( emplace_back )
        XY_STREAM_FILL_TEST( push_back )
#undef XY_STREAM_FILL_TEST_HELP
#undef XY_STREAM_FILL_TEST       
    };
}

#pragma clang diagnostic pop