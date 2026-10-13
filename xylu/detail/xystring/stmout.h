#pragma once

/**
 * @file    xystring/stmout.h
 * @brief   输出流适配器
 * @date    2025-12-29
 */
 
#include "./strview.h"
#include "../xymemory/new.h"

namespace xylu::xystring
{
    namespace __
    {
        // 测试扩容函数
        template <typename T, typename = decltype(xyu::t_val<T>().reserve(xyu::size_t{}))>
        xyu::true_c has_method_reserve_test(int) noexcept(noexcept(xyu::t_val<T>().reserve(xyu::size_t{})));
        template <typename>
        xyu::false_c has_method_reserve_test(...) noexcept;
        template <typename T>
        constexpr bool has_method_reserve = decltype(has_method_reserve_test<xyu::t_rem_cr<T>>(0))::value;
        template <typename T>
        constexpr bool noexcept_reserve = noexcept(has_method_reserve_test<xyu::t_rem_cr<T>>(0));

        // 测试获取元素个数函数 count()/length()/size()
        template <typename T, typename = decltype(xyu::t_val<T>().length())>
        xyu::size_c<1> has_method_count_test(char) noexcept(noexcept(xyu::t_val<T>().length()));
        template <typename T, typename = decltype(xyu::t_val<T>().size())>
        xyu::size_c<2> has_method_count_test(int) noexcept(noexcept(xyu::t_val<T>().size()));
        template <typename T, typename = decltype(xyu::t_val<T>().count())>
        xyu::size_c<3> has_method_count_test(long) noexcept(noexcept(xyu::t_val<T>().count()));
        template <typename T>
        xyu::size_c<0> has_method_count_test(...) noexcept;
        template <typename T>
        constexpr auto has_method_count = decltype(has_method_count_test<xyu::t_rem_cr<T>>('0'))::value;
        template <typename T>
        constexpr bool noexcept_count = noexcept(has_method_count_test<xyu::t_rem_cr<T>>('0'));

        // 测试写入函数
        template <typename S, typename T, typename V = decltype(xyu::t_val<S&>() << xyu::t_val<T>(), xyu::special_t{}), typename..., typename = xyu::t_enable<xyu::t_is_same<V, xyu::special_t>>>  // (防止匹配到传递两个模板参数的类型)
        xyu::size_c<1> has_method_write_test(char&&) noexcept(noexcept(xyu::t_val<S&>() << xyu::t_val<T>()));
        template <typename S, typename... T, typename = decltype(xyu::t_val<S&>().write(xyu::t_val<T>()...))>
        xyu::size_c<2> has_method_write_test(const char&&) noexcept(noexcept(xyu::t_val<S&>().write(xyu::t_val<T>()...)));
        template <typename S, typename... T, typename = decltype(xyu::t_val<S&>().append(xyu::t_val<T>()...))>
        xyu::size_c<3> has_method_write_test(const char&) noexcept(noexcept(xyu::t_val<S&>().append(xyu::t_val<T>()...)));
        template <typename S, typename... T, typename = decltype(xyu::t_val<S&>().push(xyu::t_val<T>()...))>
        xyu::size_c<4> has_method_write_test(int&&) noexcept(noexcept(xyu::t_val<S&>().push(xyu::t_val<T>()...)));
        template <typename S, typename... T, typename = decltype(xyu::t_val<S&>().emplace_back(xyu::t_val<T>()...))>
        xyu::size_c<5> has_method_write_test(const int&&) noexcept(noexcept(xyu::t_val<S&>().emplace_back(xyu::t_val<T>()...)));
        template <typename S, typename... T, typename = decltype(xyu::t_val<S&>().push_back(xyu::t_val<T>()...))>
        xyu::size_c<6> has_method_write_test(double) noexcept(noexcept(xyu::t_val<S&>().push_back(xyu::t_val<T>()...)));
        template <typename, typename... T>
        xyu::size_c<0> has_method_write_test(...) noexcept(false);
        template <typename S, typename... T>
        constexpr auto has_method_write = decltype(has_method_write_test<S, T...>('0'))::value;
        template <typename S, typename... T>
        constexpr bool noexcept_write = noexcept(has_method_write_test<S, T...>('0'));
    }

    /**
     * @brief 一个通用的输出流适配器，为不同类型的输出目标提供统一的流式写入接口
     * @details
     *   `OStream` 包装了一个输出目标对象 `S`（如 `xyu::String`, `xyu::File` 等），
     *   并为其提供了统一的 `write()` 和 `operator˂˂` 方法。
     *
     *   它通过 SFINAE 在编译期自动探测被包装对象 `S` 支持的写入方法，并按以下
     *   优先级顺序进行适配：
     *   1. `s ˂˂ value`
     *   2. `s.write(value)`
     *   3. `s.append(value)`
     *   4. `s.push(value)`
     *   5. `s.emplace_back(value)`
     *   6. `s.push_back(value)`
     */
    template <typename S>
    struct OStream
    {
    private:
        S& s;
        
    public:
        /// 构造函数
        constexpr explicit OStream(S& stream) noexcept : s(stream) {}
        
        /**
         * @brief 确保空间还能容纳 bytes 个字节
         * @details 基于 reserve() 与 size()/count()/length() 实现，若没有，则不做处理
         */
        constexpr OStream& expand(xyu::size_t bytes) noexcept(__::noexcept_reserve<S> && __::noexcept_count<S>)
        {
            if constexpr (__::has_method_reserve<S>)
            {
                constexpr auto type = __::has_method_count<S>;
                if constexpr (type == 1) s.reserve(s.length() + bytes);
                else if constexpr (type == 2) s.reserve(s.size() + bytes);
                else if constexpr (type == 3) s.reserve(s.count() + bytes);
            }
            return *this;
        }
        
        /// 写入一个字符
        constexpr OStream& write(char c) noexcept(__::noexcept_write<S, char>)
        {
            constexpr auto type = __::has_method_write<S, char>;
            if constexpr (type == 1) s << c;
            else if constexpr (type == 2) s.write(c);
            else if constexpr (type == 3) s.append(c);
            else if constexpr (type == 4) s.push(c);
            else if constexpr (type == 5) s.emplace_back(c);
            else if constexpr (type == 6) s.push_back(c);
            else static_assert(type != 0, "stream not support to write char");
            return *this;
        }

        /// 写入多个字符
        constexpr OStream& write(xyu::size_t n, char c) noexcept(noexcept(write(c, n))) { return write(c, n); }
        /// 写入多个字符
        constexpr OStream& write(char c, xyu::size_t n) noexcept(__::noexcept_write<S, char, xyu::size_t> || __::noexcept_write<S, xyu::size_t, char> || noexcept(write(c)))
        {
            constexpr auto type = __::has_method_write<S, char, char>;
            if constexpr (type == 2)
            {
                if constexpr (fill_test_write<decltype(s.write(c, n))>::value) s.write(c, n);
                else s.write(n, c);
            }
            else if constexpr (type == 3)
            {
                if constexpr (fill_test_append<decltype(s.append(c, n))>::value) s.append(c, n);
                else s.append(n, c);
            }
            else if constexpr (type == 4)
            {
                if constexpr (fill_test_push<decltype(s.push(c, n))>::value) s.push(c, n);
                else s.push(n, c);
            }
            else if constexpr (type == 5)
            {
                if constexpr (fill_test_emplace_back<decltype(s.emplace_back(c, n))>::value) s.emplace_back(c, n);
                else s.emplace_back(n, c);
            }
            else if constexpr (type == 6)
            {
                if constexpr (fill_test_push_back<decltype(s.push_back(c, n))>::value) s.push_back(c, n);
                else s.push_back(n, c);
            }
            else
            {
                static_assert(__::has_method_write<S, char> != 0, "stream not support to write chars");
                expand(n);
                for (xyu::size_t i = 0; i < n; ++i) write(c);
            }
            return *this;
        }

        /// 写入字符串
        constexpr OStream& write(StringView str) noexcept(__::noexcept_write<S, StringView> || __::noexcept_write<S, const char*> || noexcept(write(char{})))
        {
            constexpr auto type = __::has_method_write<S, StringView>;
            if constexpr (type == 1) s << str;
            else if constexpr (type == 2) s.write(str);
            else if constexpr (type == 3) s.append(str);
            else if constexpr (type == 4) s.push(str);
            else if constexpr (type == 5) s.emplace_back(str);
            else if constexpr (type == 6) s.push_back(str);
            else
            {
                constexpr auto type2 = __::has_method_write<S, const char*>;
                if constexpr (type2 != 0)
                {
                    bool standard = str[str.size()] == '\0';
                    if (str.size() <= StringView::K_Stack_Buffer_Size && !standard)
                    {
                        char buf[StringView::K_Stack_Buffer_Size + 1] {};
                        xyu::mem_copy(buf, str.data(), str.size());
                        buf[str.size()] = '\0';

                        if constexpr (type2 == 1) s << buf;
                        else if constexpr (type2 == 2) s.write(buf);
                        else if constexpr (type2 == 3) s.append(buf);
                        else if constexpr (type2 == 4) s.push(buf);
                        else if constexpr (type2 == 5) s.emplace_back(buf);
                        else if constexpr (type2 == 6) s.push_back(buf);
                    }
                    else
                    {
                        auto* p = const_cast<char*>(str.data());
                        if (!standard)
                        {
                            p = xyu::alloc<char>(str.size() + 1);
                            xyu::mem_copy(p, str.data(), str.size());
                            p[str.size()] = '\0';
                        }

                        struct Deleter
                        {
                            bool standard;
                            char* p;
                            xyu::size_t size;
                            ~Deleter() noexcept { if (standard) xyu::dealloc(p, size + 1); }
                        };
                        Deleter deleter { standard, p, str.size() };

                        if constexpr (type2 == 1) s << p;
                        else if constexpr (type2 == 2) s.write(p);
                        else if constexpr (type2 == 3) s.append(p);
                        else if constexpr (type2 == 4) s.push(p);
                        else if constexpr (type2 == 5) s.emplace_back(p);
                        else if constexpr (type2 == 6) s.push_back(p);
                    }
                }
                else
                {
                    static_assert(__::has_method_write<S, char> != 0, "stream not support to write string");
                    expand(str.size());
                    for (char c : str.range()) write(c);
                }
            }
            return *this;
        }

        /// 格式化后写入其他类型
        template <typename T, xyu::t_enable<!xyu::t_can_init<StringView, const T&>, bool> = false>
        OStream& write(const T& val) noexcept(is_noexcept_write<T>());

        /// 写入转发
        template <typename T>
        constexpr OStream& operator<<(T&& v) noexcept(noexcept(write(xyu::forward<T>(v)))) { return write(xyu::forward<T>(v)); }

    private:
        // 格式化是否异常
        template <typename T>
        static constexpr bool is_noexcept_write() noexcept;

        // 测试多个字符传参顺序
        #define XY_fill_test_part_gen( fun, param, sfinae ) \
            template <typename T, typename = decltype(static_cast<Ret(T::*)(char, param)>(&T::fun))> \
            static xyu::true_c test(sfinae);
        #define XY_fill_test_gen( fun ) \
            template <typename Ret, typename Stream = S> \
            struct fill_test_##fun \
            {  \
                XY_fill_test_part_gen( fun, short, char&& ) \
                XY_fill_test_part_gen( fun, unsigned short, const char&&) \
                XY_fill_test_part_gen( fun, int, const char&)             \
                XY_fill_test_part_gen( fun, unsigned int, int&&)    \
                XY_fill_test_part_gen( fun, long, const int&&)            \
                XY_fill_test_part_gen( fun, unsigned long, const int&)    \
                XY_fill_test_part_gen( fun, long long, long long&&)       \
                XY_fill_test_part_gen( fun, unsigned long long, const long long&&) \
                template <typename T>  \
                static xyu::false_c test(...); \
                constexpr static bool value = decltype(test<Stream>('0'))::value; \
            };
        XY_PP_FORWARD( XY_fill_test_gen, 1, append, write, push, emplace_back, push_back)
        #undef XY_fill_test_gen
        #undef XY_fill_test_part_gen
    };
}
