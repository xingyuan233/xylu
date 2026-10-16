#pragma once

/**
 * @file    xystring/strsso.h
 * @brief   SSO 优化 字符串
 * @date    2026-06-28
 */

#include "./strbase.h"
#include "../xymemory/new.h"

namespace xylu::xystring
{
    /**
     * @brief 支持 SSO 的通用字符串类
     * @tparam cross_thread  是否允许跨线程内存管理。
     *         - false: 使用线程局部内存池，性能更好；
     *         - true : 使用全局内存分配，对象可跨线程重分配和释放
     * @details
     *   内部通过 `union` 维护两种状态：
     *   - SStack `char[]` + `size_and_sign`
     *   - SHeap: `ptr` + `size` + `capa_and_sign`
     */
    template <bool cross_thread>
    struct CompactString : StringBase<CompactString<cross_thread>>
    {
    public:
        /* 静态常量  */
        /// 释放空间条件比例
        static constexpr auto K_shrink_factor = 0.5;
        /// 扩容比例
        static constexpr auto K_grow_factor = 2.0;

    private:
        /// 堆上字符串
        struct SHeap
        {
            char* data;         // 堆内存地址
            xyu::size_t size;   // 字符串长度
            xyu::size_t capa;   // 堆内存容量 + 标志位
        };
        /// 栈上字符串
        struct SStack
        {
            // 字符串存储 (包括'\0')
            char data[sizeof(SHeap) - sizeof(xyu::uint8)];
            xyu::uint8 size;    // 字符串长度 + 标志位
        };
        // 小端序下，size_t 物理地址的最后一个字节为实际的最高字节，将其最高位作为标志位
        // 大端序下，size_t 物理地址的最后一个字节为实际的最低字节，将其最低位作为标志位
        union
        {
            SStack s;
            SHeap h;
        };

    public:

        /* 构造赋值*/

        /// 析构函数
        ~CompactString() noexcept { release(); }

        /// 默认构造
        CompactString() noexcept { set_size_s(0); }

        /**
         * @brief 预分配空间构造
         * @details 短字符串将直接存储在栈上，长字符串将容量对齐后分配堆内存
         */
        explicit CompactString(xyu::size_t capa) noexcept
        {
            if (capa < sizeof(s.data)) set_size_s(0);
            else large_alloc<false>("", 0, capa);
        }
        /**
         * @brief 填充构造
         * @details 短字符串将直接存储在栈上，长字符串将容量对齐后分配堆内存
         */
        explicit CompactString(xyu::size_t capa, char c) noexcept : CompactString{capa} { resize(capa, c); }

        /// 字符串构造
        CompactString(const char* str) noexcept : CompactString(StringView{str}) {}
        /// 字符串构造
        CompactString(const char* str, xyu::size_t bytes) noexcept : CompactString{StringView{str, bytes}} {}
        /// 字符串构造
        CompactString(StringView view) noexcept
        {
            xyu::size_t n = view.size();
            // 长度较小
            if (n < sizeof(s.data))
            {
                xyu::mem_copy(s.data, view.data(), n);
                set_size_s(n);
            }
            // 长度较大
            else large_alloc<false>(view.data(), n, n);
        }

        /// 字符串赋值
        CompactString& operator=(const char* str) noexcept { return operator=(StringView{str}); }
        /// 字符串赋值
        CompactString& operator=(StringView view) noexcept
        {
            const char* src = view.data();
            xyu::size_t newsize = view.size();

            // to Stack
            if (newsize < sizeof(s.data))
            {
                if (!is_sso())
                {
                    char* p = h.data;
                    xyu::size_t oldcapa = get_capa_h();
                    xyu::mem_copy(s.data, src, newsize);
                    do_dealloc(p, oldcapa);
                }
                else xyu::mem_move(s.data, src, newsize);
                set_size_s(newsize);
            }
            // to Heap
            else
            {
                // 扩容
                if (newsize > capacity()) large_alloc(src, newsize, newsize);
                else
                {
                    // 一定为 Heap
                    xyu::mem_move(h.data, src, newsize);
                    h.data[newsize] = '\0';
                    h.size = newsize;
                }
            }
            return *this;
        }

        /// 复制构造
        CompactString(const CompactString& other) noexcept
        {
            auto n = other.size();
            // Stack
            if (n < sizeof(s.data))
            {
                xyu::mem_copy(s.data, other.data(), n);
                set_size_s(n);
            }
            // Heap
            else large_alloc<false>(other.h.data, n, n);
        }
        /// 复制赋值
        CompactString& operator=(const CompactString& other) noexcept
        {
            if (XY_UNLIKELY(this == &other)) return *this;
            const char* str = other.data();
            auto newsize = other.size();

            bool sso = is_sso();
            // 释放内存
            if (!sso && (newsize < sizeof(s.data) || newsize > get_capa_h()))
            {
                do_dealloc(h.data, get_capa_h());
                sso = true;
            }
            // to Stack
            if (newsize < sizeof(s.data))
            {
                xyu::mem_copy(s.data, str, newsize);
                set_size_s(newsize);
            }
            // to Heap
            else
            {
                if (!sso)
                {
                    xyu::mem_copy(h.data, str, newsize);
                    h.size = newsize;
                    h.data[newsize] = '\0';
                }
                else large_alloc(str, newsize, newsize);
            }
            return *this;
        }

        /* 移动交换 */

        /// 移动构造
        CompactString(CompactString&& other) noexcept
        {
            xyu::mem_copy(this, &other, 1);
            other.set_size_s(0);
        }

        /// 移动赋值
        CompactString& operator=(CompactString&& other) noexcept { swap(other); return *this; }

        /// 交换两个字符串
        CompactString& swap(CompactString& other) noexcept
        {
            char buf[sizeof(CompactString)];
            xyu::mem_copy((CompactString*)buf, this, 1);
            xyu::mem_copy(this, &other, 1);
            xyu::mem_copy(&other, (CompactString*)buf, 1);
            return *this;
        }

        /* 转移 */

        /**
         * @brief 转移构造
         * @param capa 分配的字符串容量
         * @param buf 必须为 alloc 分配的对象
         * @details 自动检查有效的字符串长度
         */
        XY_NONNULL_RELEASE(4)
        CompactString(xyu::special_t, xyu::size_t capa, char* buf) noexcept : CompactString(xyu::special_v, capa, buf, this->strlen(buf)) {}
        /**
         * @brief 转移构造
         * @param capa 分配的字符串容量
         * @param buf 必须为 alloc 分配的对象
         * @param bytes 实际有效的字符串长度
         */
        XY_NONNULL_RELEASE(4)
        CompactString(xyu::special_t, xyu::size_t capa, char* buf, xyu::size_t bytes) noexcept
        {
            if constexpr (!XY_RELEASE)
            {
                if (XY_UNLIKELY(buf == nullptr)) xyu::E_Logic::abort_null_pointer(XY_CurPos("String::String"), "buf");
                if (XY_UNLIKELY(capa > this->limit()))
                    xyu::E_Logic::abort_over_limit(XY_CurPos("String::String"), capa, 0, this->limit());
            }
            h.data = buf;
            h.size = bytes;
            set_capa_h(capa);
        }

        /**
         * @brief 转移字符串
         * @details 若原本较短且存储在栈上会重新分配内存并复制。
         * @attention
         *   需要提前获取 size / capa 等信息存储，转移后对象已清空。
         *   转移后的对象需要手动管理，或通过 xyu::special_v 转移给 另一个 CompactString。
         */
        char* take() noexcept
        {
            char* tmp;
            if (XY_UNLIKELY(is_sso()))
            {
                tmp = do_alloc(sizeof(s.data));
                xyu::mem_copy(tmp, s.data, get_size_s() + 1);
            }
            else tmp = h.data;
            set_size_s(0);
            return tmp;
        }

        /* 数据获取 */

        /// 获取字符串容量
        xyu::size_t capacity() const noexcept { return is_sso() ? sizeof(s.data) - 1 : get_capa_h(); }

        /// 获取字符串长度
        xyu::size_t size() const noexcept { return is_sso() ? get_size_s(): h.size; }

        /// 获取字符串指针
        char* data() noexcept XY_RETURNS_NONNULL { return is_sso() ? s.data : h.data; }
        /// 获取字符串指针 (const)
        const char* data() const noexcept XY_RETURNS_NONNULL { return is_sso() ? s.data : h.data; }

        /* 数据管理 */

        /**
         * @brief 清空数据
         * @details Heap 仍保留容量
         */
        void clear() noexcept { set_size(0); }

        /**
         * @brief 释放内存
         * @note 释放 Heap 内存，并统一重置为 空Stack
         */
        void release() noexcept
        {
            if (!is_sso()) do_dealloc(h.data, get_capa_h());
            set_size_s(0);
        }

        /**
         * @brief 预分配空间
         * @param minsize 最小容量
         * @note Fixed 字符串调用时则先进行复制
         */
        void reserve(xyu::size_t minsize) noexcept
        {
            xyu::size_t oldcapa;
            if (is_sso())
            {
                oldcapa = sizeof(s.data) - 1;
                if (minsize > oldcapa) large_alloc<false>(s.data, get_size_s(), minsize);
            }
            else
            {
                oldcapa = get_capa_h();
                if (minsize > oldcapa) large_alloc(h.data, h.size, minsize);
            }
        }

        /// 尝试缩减容量
        void reduce() noexcept
        {
            if (is_sso()) return;
            char* p = h.data;
            xyu::size_t size = h.size;
            xyu::size_t capa = get_capa_h();
            if (size < static_cast<xyu::size_t>(capa * K_shrink_factor))
            {
                // to Stack
                if (size < sizeof(s.data))
                {
                    xyu::mem_copy(s.data, p, size + 1);
                    set_size_s(size);
                }
                // to Heap
                else large_alloc<false>(p, size, size);
                do_dealloc(p, capa);
            }
        }

        /**
         * @brief 设置字符串长度
         * @details 仅修改长度，扩容部分内容不确定
         */
        void resize(xyu::size_t newsize) noexcept
        {
            reserve(newsize);
            set_size(newsize);
        }
        /**
         * @brief 设置字符串长度
         * @details 若扩充，扩充部分填充为指定字符
         */
        void resize(xyu::size_t newsize, char c) noexcept
        {
            reserve(newsize);
            if (is_sso())
            {
                xyu::size_t n = get_size_s();
                if (newsize > n) xyu::mem_set(s.data + n, c, newsize - n);
                set_size_s(newsize);
            }
            else
            {
                if (newsize > h.size) xyu::mem_set(h.data + h.size, c, newsize - h.size);
                h.data[newsize] = '\0';
                h.size = newsize;
            }
        }

    private:
        friend class StringBase<CompactString>;

        // 计算新容量
        template <bool inited = true>
        xyu::size_t calc_capa(xyu::size_t mincapa) const noexcept
        {
            // 新容量 = max(最小容量, 旧容量 * K_grow_factor) --> align
            xyu::size_t newcapa = mincapa;
            if constexpr (inited) newcapa = xyu::max(newcapa, static_cast<xyu::size_t>(capacity() * K_grow_factor));
            if (XY_UNLIKELY(newcapa > this->limit())) return this->limit();
            // 将 newcapa 调整到 K_DEFAULT_ALIGN 对齐的倍数
            newcapa = ((newcapa + xyu::K_Default_Align) & -xyu::K_Default_Align) - 1;
            return newcapa;
        }

        // 重新分配堆内存
        template <bool inited = true>
        void large_alloc(const char* str, xyu::size_t bytes, xyu::size_t mincapa) noexcept
        {
            if (XY_UNLIKELY(mincapa > this->limit()))
                xyu::E_Logic::abort_over_limit(XY_CurPos("String::alloc"), mincapa, 0, this->limit());
            xyu::size_t newcapa = calc_capa<inited>(mincapa);
            char* newdata = do_alloc(newcapa);
            xyu::mem_copy(newdata, str, bytes);
            newdata[bytes] = '\0';
            if constexpr (inited) if (!is_sso()) do_dealloc(h.data, get_capa_h());
            h.data = newdata;
            h.size = bytes;
            set_capa_h(newcapa);
        }

        // 设置新长度
        void set_size(xyu::size_t new_len) noexcept
        {
            if (is_sso()) set_size_s(new_len);
            else h.data[new_len] = '\0', h.size = new_len;
        }

        // 扩容
        template <typename Migration>
        void do_expand(xyu::size_t mincapa, Migration&& migrate) noexcept
        {
            auto newcapa = calc_capa(mincapa);
            auto newdata = do_alloc(newcapa);
            bool sso = is_sso();
            migrate(sso ? s.data : h.data, newdata);
            if (!sso) do_dealloc(h.data, get_capa_h());
            h.data = newdata;
            set_capa_h(newcapa);
        }

    private:
        static char* do_alloc(xyu::size_t capa) noexcept
        {
            if constexpr (cross_thread) return xyu::alloc<char>(xyu::native_v, capa + 1);
            else return xyu::alloc<char>(capa + 1);
        }
        static void do_dealloc(char* p, xyu::size_t capa) noexcept
        {
            if constexpr (cross_thread) return xyu::dealloc(xyu::native_v, p);
            else xyu::dealloc(p, capa + 1);
        }

        bool is_sso() const noexcept
        {
            if constexpr (XY_LITTLE_ENDIAN) return s.size & 0x80;
            else return s.size & 1;
        }
        xyu::size_t get_size_s() const noexcept
        {
            if constexpr (XY_LITTLE_ENDIAN) return s.size & 0x7f;
            else return s.size >> 1;
        }
        void set_size_s(xyu::size_t newsize) noexcept
        {
            s.data[newsize] = '\0';
            if constexpr (XY_LITTLE_ENDIAN) s.size = newsize | 0x80;
            else s.size = (newsize << 1) | 1;
        }
        xyu::size_t get_capa_h() const noexcept
        {
            if constexpr (XY_LITTLE_ENDIAN) return h.capa;
            else return h.capa >> 1;
        }
        void set_capa_h(xyu::size_t newcapa) noexcept
        {
            if constexpr (XY_LITTLE_ENDIAN) h.capa = newcapa;
            else h.capa = newcapa << 1;
        }
    };

    /**
     * @brief 支持 SSO 的通用字符串类
     * @details 使用线程局部内存池，禁止跨线程内存操作
     */
    using String = CompactString<false>;

}
