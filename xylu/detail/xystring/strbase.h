#pragma once

/**
 * @file    string/strbase.h
 * @brief   字符串可变基类
 * @date    2025-12-27
 */

#include "./strview.h"

/// 字符串基类
namespace xylu::xystring
{
    namespace __
    {
        template <typename Str> struct StringRef;

        [[noreturn]] void abort_iter_unowned_impl(const void* p, const void* start, const void* end, xyu::ErrorPos) noexcept XY_COLD;
        
        constexpr void abort_iter_unowned(const void* p, const void* start, const void* end, xyu::ErrorPos pos) noexcept
        {
            if (xyu::t_constant()) xyu::abort(xyu::native_v);
            else abort_iter_unowned_impl(p, start, end, pos);
        }
    }

    /**
     * @brief 字符串算法 CRTP 基类
     * @tparam Str 派生字符串类型
     *
     * @details
     *   方法简述：
     *   - 任何继承 `StringBase` 的派生类，将自动获得只读字符串操作接口及该类的增删查改操作接口
     *   - 支持 `append`、`insert`、`erase`、`set`、`update` 等，并且均支持任意自引用
     *
     *   继承要求（派生类必须提供以下方法）：
     *   - `xyu::size_t size() const noexcept`: 返回字符串的长度
     *   - `char* data() noexcept`: 返回指向字符串数据的指针
     *   - `const char* data() const noexcept`: 返回指向字符串数据的只读指针
     *
     *   - `xyu::size_t capacity() const noexcept`: 返回字符串的容量
     *   - `static xyu::size_t limit() noexcept`: (可选)返回字符串最大容量，do_expand 前会检查
     *
     *   - `void set_size(xyu::size_t new_len) noexcept`: 设置字符串的新长度
     *   - `template ˂typename Migration˃ void do_expand(xyu::size_t mincapa, Migration&& migrate) noexcept`:
     *      扩容函数，mincapa 为最小新容量，内部通过回调 migrate(old_data, new_data) 进行不同地址的数据迁移
     *
     *   注意事项：
     *   - 空指针禁止：`data()`严禁返回空指针
     *   - c风格检测：`data()[size()]` 必须是可访问的，但必须通过 const get 访问
     */
    template <typename Str>
    struct StringBase : StringViewBase<Str>
    {
        /* 数据容量 */

        /**
         * @brief 获取字符串的最大容量
         * @note 防止加法溢出
         */
        constexpr static xyu::size_t limit() noexcept { return xyu::t_number<xyu::size_t>::max / 2; }

        /// 获取字符串大小
        constexpr xyu::size_t size() const noexcept { return derived().size(); }
        /// 获取字符串容量
        constexpr xyu::size_t capacity() const noexcept { return derived().capacity(); }

        /* 数据获取 */

        /// 获取字符串 (不会返回 nullptr)
        constexpr char* data() noexcept XY_RETURNS_NONNULL { return derived().data(); }
        /// 获取字符串 (不会返回 nullptr)
        constexpr const char* data() const noexcept XY_RETURNS_NONNULL { return derived().data(); }

        /// 获取字符
        constexpr char& get(xyu::size_t index) noexcept
        {
            if constexpr (!XY_RELEASE) if (XY_UNLIKELY(index >= size()))
                xyu::E_Logic::abort_out_range(XY_CurPos("StringBase::get"), index, 0, size());
            return data()[index];
        }
        /// 获取字符
        constexpr char get(xyu::size_t index) const noexcept
        {
            if constexpr (!XY_RELEASE) if (XY_UNLIKELY(index > size()))
                xyu::E_Logic::abort_out_range(XY_CurPos("StringBase::get"), index, 0, size());
            return data()[index];
        }

        /// 获取字符串的子串
        Str substr(xyu::size_t start = 0, xyu::size_t len = -1) const noexcept
        {
            auto s = size();
            if (XY_UNLIKELY(start >= s)) return {};
            if (s - start <= len) len = s - start;
            return Str{StringView{data() + start, len}};
        }

        /**
         * @brief 获取字符串的切片引用
         * @param start 区间起始位置
         * @param len 区间当前有效长度
         * @param capa 区间总长度
         * @return
         */
        auto subref(xyu::size_t start = 0, xyu::size_t len = -1, xyu::size_t capa = -1) noexcept;

        /* 增加 */

        /// 尾部增加 count 个字符c
        constexpr Str& append(xyu::size_t count, char c) noexcept { return append(c, count); }
        /// 尾部增加 count 个字符c
        constexpr Str& append(char c, xyu::size_t count = 1) noexcept
        {
            //--参数检测--
            if constexpr (!XY_RELEASE) check_param(count, XY_CurPos("StringBase::append"));
            //--容量检测--
            auto s = size();
            // 不需要扩容
            if (s + count <= capacity()) {}
            // 需要扩容
            else
            {
                check_add(s, count, XY_CurPos("StringBase::append"));
                do_expand(s + count, [s](const char* op, char* np){ xyu::mem_copy(np, op, s); });
            }
            xyu::mem_set(data() + s, c, count);
            set_size(s + count);
            return derived();
        }
        /// 尾部追加 字符串
        constexpr Str& append(StringView str) noexcept
        {
            auto count = str.size();
            //--参数检测--
            if constexpr (XY_DEBUG) check_param(count, XY_CurPos("StringBase::append"));
            //--容量检测--
            auto sp = str.data();
            auto s = size();
            // 不需要扩容
            if (s + count <= capacity()) xyu::mem_copy(data() + s, sp, count);
            // 需要扩容
            else
            {
                check_add(s, count, XY_CurPos("StringBase::append"));
                do_expand(s + count, [s, sp, count](const char* op, char* np){
                    xyu::mem_copy(np, op, s);
                    xyu::mem_copy(np + s, sp, count);
                });
            }
            set_size(s + count);
            return derived();
        }

        /// 在 index 位置前 插入 count 个字符c
        constexpr Str& insert(xyu::size_t index, xyu::size_t count, char c) noexcept { return insert(index, c, count); }
        /// 在 index 位置前 插入 count 个字符c
        constexpr Str& insert(xyu::size_t index, char c, xyu::size_t count = 1) noexcept
        {
            auto s = size();
            if (index >= s) return append(c, count);
            //--参数检测--
            if constexpr (!XY_RELEASE)check_param(count, XY_CurPos("StringBase::insert"));
            //--容量检测--
            // 不需要扩容
            if (s + count <= capacity())
            {
                auto p = data();
                xyu::mem_move(p + index + count, p + index, s - index);
                xyu::mem_set(p + index, c, count);
            }
            // 需要扩容
            else
            {
                check_add(s, count, XY_CurPos("StringBase::insert"));
                do_expand(s + count, [=](const char* op, char* np){
                    xyu::mem_copy(np, op, index);
                    xyu::mem_set(np + index, c, count);
                    xyu::mem_copy(np + index + count, op + index, s - index);
                });
            }
            set_size(s + count);
            return derived();
        }
        /// 在 index 位置前 插入 字符串
        constexpr Str& insert(xyu::size_t index, StringView str) noexcept
        {
            auto s = size();
            if (index >= s) return append(str);
            auto count = str.size();
            //--参数检测--
            if constexpr (XY_DEBUG) check_param(count, XY_CurPos("StringBase::insert"));
            //--容量检测--
            auto sp = str.data();
            // 不需要扩容
            if (s + count <= capacity())
            {
                auto p = data();
                if (xyu::t_constant())
                {
                    Str tmp{str};
                    sp = tmp.data();
                    xyu::mem_move(p + index + count, p + index, s - index);
                    xyu::mem_copy(p + index, sp, count);
                }
                else
                {
                    //--自引用检测--
                    bool alias = XY_UNLIKELY(sp >= p && sp < p + s);
                    // 非自引用
                    if (XY_LIKELY(!alias))
                    {
                        xyu::mem_move(p + index + count, p + index, s - index);
                        xyu::mem_copy(p + index, sp, count);
                    }
                    // 自引用
                    else
                    {
                        xyu::size_t off_s = sp - p;
                        xyu::size_t off_e = off_s + count;
                        // 移动
                        xyu::mem_move(p + index + count, p + index, s - index);
                        // 引用部分在左边
                        if (off_e <= index) xyu::mem_copy(p + index, sp, count);
                        // 引用部分在右边
                        else if (off_s >= index) xyu::mem_copy(p + index, sp + count, count);
                        // 引用部分被切开
                        else
                        {
                            xyu::size_t off_m = index;
                            xyu::size_t count_l = off_m - off_s;
                            xyu::size_t count_r = off_e - off_m;
                            xyu::mem_copy(p + index, sp, count_l);
                            xyu::mem_copy(p + index + count_l, sp + count + count_l, count_r);
                        }
                    }
                }
            }
            // 需要扩容
            else
            {
                check_add(s, count, XY_CurPos("StringBase::insert"));
                do_expand(s + count, [=](const char* op, char* np){
                    xyu::mem_copy(np, op, index);
                    xyu::mem_copy(np + index, sp, count);
                    xyu::mem_copy(np + index + count, op + index, s - index);
                });
            }
            set_size(s + count);
            return derived();
        }

        /* 删除 */

        /// 删除 index 位置开始的 count 个元素 (默认index后的全部)
        constexpr Str& erase(xyu::size_t index, xyu::size_t count = -1) noexcept
        {
            auto s = size();
            // 移动字符
            if (index >= s) return derived();
            if (s - index <= count) s = index;
            else
            {
                auto p = data();
                xyu::mem_move(p + index, p + index + count, s - index - count);
                s -= count;
            }
            // 修改状态
            set_size(s);
            return derived();
        }

        /// 删除迭代器位置，并返回下一个位置的迭代器
        template <typename Iter, typename = xyu::t_enable<xyu::t_is_iter_to<Iter, char&, const char&>>>
        constexpr Iter erase(Iter it, xyu::size_t count = 1) noexcept
        {
            const char* p{};
            if constexpr (xyu::t_is_vptr<Iter>) p = it;
            else p = &*it;
            if constexpr (XY_DEBUG) if (XY_UNLIKELY(p < data() || p >= data() + size()))
                __::abort_iter_unowned(p, data(), data() + size(), XY_CurPos("StringBase::erase"));
            erase(p - data(), count);
            return it;
        }

        /* 修改 */

        /// 设置 索引位置开始为 count 个字符c，越界则扩容追加
        constexpr Str& set(xyu::size_t start, xyu::size_t count, char c) noexcept { return set(start, c, count); }
        /// 设置 索引位置开始为 count 个字符c，越界则扩容追加
        constexpr Str& set(xyu::size_t start, char c, xyu::size_t count = 1) noexcept
        {
            auto s = size();
            if (start >= s) return append(c, count);
            //--参数检测--
            if constexpr (!XY_RELEASE) check_param(count, XY_CurPos("StringBase::set"));
            //--容量检测--
            bool app = start + count > s;
            // 不需要扩容
            if (start + count <= capacity())
            {
                xyu::mem_set(data() + start, c, count);
                if (app) set_size(start + count);
            }
            // 需要扩容
            else
            {
                if (XY_LIKELY(app)) check_add(s, start + count - s, XY_CurPos("StringBase::set"));
                do_expand(start + count, [=](const char* op, char* np){
                    xyu::mem_copy(np, op, start);
                    xyu::mem_set(np + start, c, count);
                    if (XY_UNLIKELY(!app)) xyu::mem_copy(np + start + count, op + start + count, s - start - count);
                });
                set_size(XY_LIKELY(app) ? start + count : s);
            }
            return derived();
        }
        /// 设置 索引位置开始为 新字符串，越界则扩容追加
        constexpr Str& set(xyu::size_t start, StringView str) noexcept
        {
            auto s = size();
            if (start >= s) return append(str);
            auto count = str.size();
            //--参数检测--
            if constexpr (XY_DEBUG) check_param(count, XY_CurPos("StringBase::set"));
            //--容量检测--
            auto sp = str.data();
            bool app = start + count > s;
            // 不需要扩容
            if (start + count <= capacity())
            {
                if (xyu::t_constant())
                {
                    Str tmp{str};
                    xyu::mem_copy(data() + start, tmp.data(), count);
                }
                else xyu::mem_move(data() + start, sp, count);
                if (app) set_size(start + count);
            }
            // 需要扩容
            else
            {
                // 若 size 可能大于 capacity
                if (XY_LIKELY(app)) check_add(s, start + count - s, XY_CurPos("StringBase::set"));
                do_expand(start + count, [=](const char* op, char* np){
                    xyu::mem_copy(np, op, start);
                    xyu::mem_copy(np + start, sp, count);
                    if (XY_UNLIKELY(!app)) xyu::mem_copy(np + start + count, op + start + count, s - start - count);
                });
                set_size(XY_LIKELY(app) ? start + count : s);
            }
            return derived();
        }

        /// 修改 index 位置开始的 len个字符 为 count 个字符c
        constexpr Str& update(xyu::size_t index, xyu::size_t len, xyu::size_t count, char c) noexcept { return update(index, len, c, count); }
        /// 修改 index 位置开始的 len个字符 为 count 个字符c
        constexpr Str& update(xyu::size_t index, xyu::size_t len, char c, xyu::size_t count = 1) noexcept
        {
            auto s = size();
            if (XY_UNLIKELY(index >= s)) return append(c, count);
            //--参数检测--
            if constexpr (!XY_RELEASE) check_param(count, XY_CurPos("StringBase::update"));
            if (s - index < len) len = s - index;
            //--长度检测--
            char* p = {};
            auto ns = s + count - len;
            // 需要扩容
            if (ns > capacity())
            {
                check_add(s, count - len, XY_CurPos("StringBase::update"));
                do_expand(ns, [=](const char* op, char* np){
                    xyu::mem_copy(np, op, index);
                    xyu::mem_set(np + index, c, count);
                    xyu::mem_copy(np + index + count, op + index + len, s - index - len);
                });
            }
            // 不需要扩容
            else
            {
                p = data();
                if (count != len) xyu::mem_move(p + index + count, p + index + len, s - index - len);
                xyu::mem_set(p + index, c, count);
            }
            set_size(ns);
            return derived();
        }
        /// 修改 index 位置开始的 len个字符 为 字符串 str
        constexpr Str& update(xyu::size_t index, xyu::size_t len, StringView str) noexcept
        {
            auto s = size();
            if (XY_UNLIKELY(index >= s)) return append(str);
            auto count = str.size();
            //--参数检测--
            if constexpr (XY_DEBUG) check_param(count, XY_CurPos("StringBase::update"));
            if (s - index < len) len = s - index;
            //--长度检测--
            auto sp = str.data();
            auto ns = s + count - len;
            // 需要扩容
            if (ns > capacity())
            {
                check_add(s, count - len, XY_CurPos("StringBase::update"));
                do_expand(ns, [=](const char* op, char* np){
                    xyu::mem_copy(np, op, index);
                    xyu::mem_copy(np + index, sp, count);
                    xyu::mem_copy(np + index + count, op + index + len, s - index - len);
                });
            }
            // 不需要扩容
            else if (count != len)
            {
                auto p = data();
                if (xyu::t_constant())
                {
                    Str tmp{str};
                    sp = tmp.data();
                    xyu::mem_move(p + index + count, p + index + len, s - index - len);
                    xyu::mem_copy(p + index, sp, count);
                }
                else
                {
                    //--自引用检测--
                    bool alias = XY_UNLIKELY(sp >= p && sp < p + s);
                    // 非自引用
                    if (XY_LIKELY(!alias))
                    {
                        xyu::mem_move(p + index + count, p + index + len, s - index - len);
                        xyu::mem_copy(p + index, sp, count);
                    }
                    // 自引用
                    else
                    {
                        xyu::size_t off_s = sp - p;
                        xyu::size_t off_e = off_s + count;
                        // 引用部分在左边
                        if (off_e <= index)
                        {
                            xyu::mem_move(p + index + count, p + index + len, s - index - len);
                            xyu::mem_copy(p + index, sp, count);
                        }
                        // 引用部分在右边
                        else if (off_s >= index + len)
                        {
                            xyu::mem_move(p + index + count, p + index + len, s - index - len);
                            xyu::mem_copy(p + index, sp + count - len, count);
                        }
                        // 引用部分在中间
                        else
                        {
                            if (count <= StringView::K_Stack_Buffer_Size)
                            {
                                char buf[StringView::K_Stack_Buffer_Size]{};
                                xyu::mem_copy(buf, sp, count);
                                xyu::mem_move(p + index + count, p + index + len, s - index - len);
                                xyu::mem_copy(p + index, buf, count);
                            }
                            else
                            {
                                Str tmp{str};
                                xyu::mem_move(p + index + count, p + index + len, s - index - len);
                                xyu::mem_copy(p + index, tmp.data(), count);
                            }
                        }
                    }
                }

            }
            else
            {
                auto p = data();
                if (xyu::t_constant())
                {
                    Str tmp{str};
                    xyu::mem_copy(p + index, tmp.data(), count);
                }
                else xyu::mem_move(p + index, sp, count);
            }
            set_size(ns);
            return derived();
        }

        /* 范围 */

        /// 获取范围
        constexpr auto range() noexcept { return xyu::make_range(size(), data(), data() + size()); }
        /// 获取只读范围
        constexpr auto range() const noexcept { return xyu::make_range(size(), data(), data() + size()); }

        /* 运算符 */

        /// 获取字符
        constexpr char& operator[](xyu::size_t index) noexcept { return get(index); }
        /// 获取字符
        constexpr char operator[](xyu::size_t index) const noexcept { return get(index); }

        /// 追加字符串
        template <typename T>
        constexpr Str& operator<<(T&& t) noexcept { return append(xyu::forward<T>(t)); }
        /// 追加字符串
        template <typename T>
        constexpr Str& operator+=(T&& t) noexcept { return append(xyu::forward<T>(t)); }

        /// 拼接字符串
        template <typename T>
        constexpr Str operator+(T&& v) const& noexcept { return Str(derived()) += xyu::forward<T>(v); }
        /// 拼接字符串 (友元)
        template <typename T, typename = xyu::t_enable<!xyu::t_is_same_nocr<Str, T>>>
        constexpr friend Str operator+(T&& v, const Str& str) noexcept { return Str{} << xyu::forward<T>(v) << str; }
        /// 移动拼接字符串
        template <typename T>
        constexpr Str operator+(T&& v) && noexcept { return xyu::move(append(xyu::forward<T>(v))); }
    
    private:
        // 检查参数的值是否超过限制
        constexpr static void check_param(xyu::size_t count, xyu::ErrorPos pos) noexcept
        {
            constexpr auto lim = xyu::min(Str::limit(), limit());
            if (XY_UNLIKELY(count > lim))
                xyu::E_Logic::abort_over_limit(pos, count, 0, lim);
        }

        // 检查大小增加后是否超过限制
        constexpr static void check_add(xyu::size_t size, xyu::size_t count, xyu::ErrorPos pos) noexcept
        {
            constexpr auto lim = xyu::min(Str::limit(), limit());
            if (XY_UNLIKELY(size + count > lim))
                xyu::E_Logic::abort_over_limit(pos, size, count, lim);
        }

    private:
        // 获取派生类
        template <typename> friend struct __::StringRef;
        constexpr Str& derived() noexcept { return static_cast<Str&>(*this); }
        constexpr const Str& derived() const noexcept { return static_cast<const Str&>(*this); }

        constexpr void set_size(xyu::size_t s) noexcept { derived().set_size(s);  }
        template <typename Migration>
        constexpr void do_expand(xyu::size_t mincapa, Migration&& migrate) noexcept { derived().do_expand(mincapa, migrate); }
    };

}

/// 字符串引用
namespace xylu::xystring
{
    namespace __
    {
        template <typename Str>
        struct StringRef : StringBase<StringRef<Str>>
        {
        private:
            Str& str;               // 原字符串引用
            xyu::size_t start;      // 起始位置
            xyu::size_t len;        // 引用有效长度
            xyu::size_t capa;       // 区间长度上限

        public:
            /// 构造函数
            constexpr StringRef(Str& str, xyu::size_t start, xyu::size_t len, xyu::size_t capa) noexcept : str{str}, start{start}, len{len}, capa{capa} {}

            /// 获取允许使用的区间长度
            constexpr xyu::size_t capacity() const noexcept { return capa; }

            /// 获取有效长度
            constexpr xyu::size_t size() const noexcept { return len; }

            /// 获取字符串
            constexpr char* data() noexcept { return str.data() + start; }
            /// 获取字符串
            constexpr const char* data() const noexcept { return str.data() + start; }

        private:
            friend class StringBase<StringRef>;

            // 设置新长度 (不改变 '\0' 位置)
            void set_size(xyu::size_t new_len) noexcept { len = new_len; }

            // 扩容 (禁止)
            template <typename Migration>
            XY_COLD void do_expand(xyu::size_t mincapa, Migration&& migrate) noexcept
            {
                XY_UNREACHABLE;
                xyu::E_Logic::abort_over_limit(XY_CurPos("StringRef::do_expand"), mincapa, 0, capacity());
            }
        };
    }
    template <typename Str>
    auto StringBase<Str>::subref(xyu::size_t start, xyu::size_t len, xyu::size_t capa) noexcept
    {
        auto s = size();
        if (XY_UNLIKELY(start >= s)) start = s;
        if (s - start <= capa) capa = s - start;
        if (len > capa) len = capa;
        return __::StringRef<Str>{derived(), start, len, capa};
    }

}
