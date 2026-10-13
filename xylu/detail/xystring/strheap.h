#pragma once

/**
 * @file    xystring/strheap.h
 * @brief   堆字符串
 * @date    2025-12-28
 */

#include "./strbase.h"
#include "../xymemory/new.h"

namespace xylu::xystring
{
    /**
    * @brief 堆字符串
     * @tparam cross_thread  是否允许跨线程内存管理。
     *         - false: 使用线程局部内存池，性能更好；
     *         - true : 使用全局内存分配，对象可跨线程重分配和释放
     */
    template <bool cross_thread = false>
    struct HeapString : StringBase<HeapString<cross_thread>>
    {
        /* 静态常量  */
        /// 释放空间条件比例
        static constexpr auto K_shrink_factor = 0.5;
        /// 扩容比例
        static constexpr auto K_grow_factor = 2.0;

    private:
        char* p;            // 数据
        xyu::size_t len;    // 长度
        xyu::size_t capa;   // 容量

    public:
        /* 构造赋值 */

        /// 析构
        ~HeapString() noexcept { release(); }

        /// 默认构造
        HeapString() noexcept : p(EmptyStr), len(0), capa(0) {}

        /// 预分配空间构造
        explicit HeapString(xyu::size_t mincapa) noexcept : HeapString() { reserve(mincapa); }

        /// 字符串构造
        HeapString(const char* str) noexcept : HeapString(StringView{str}) {}
        /// 字符串构造
        HeapString(const char* str, xyu::size_t len) noexcept : HeapString(StringView{str, len}) {}
        /// 字符串构造
        HeapString(StringView str) noexcept : HeapString() { operator=(str); }

        /// 字符串赋值
        HeapString& operator=(const char* str) noexcept { return operator=(StringView{str}); }
        /// 字符串赋值
        HeapString& operator=(StringView str) noexcept
        {
            if (str.size() > capa)
            {
                do_expand(str.size(), [&str](char* op, char* np){
                    xyu::mem_copy(np, str.data(), str.size());
                });
            }
            else xyu::mem_move(p, str.data(), str.size());
            set_size(str.size());
            return *this;
        }

        /// 拷贝构造
        HeapString(const HeapString& other) noexcept : HeapString() { operator=(other); }
        /// 拷贝赋值
        HeapString& operator=(const HeapString& other) noexcept { return operator=(other.view()); }

        /* 移动交换 */

        /// 移动构造
        HeapString(HeapString&& other) noexcept : p(other.p), len(other.len), capa(other.capa)
        {
            other.p = EmptyStr;
            other.len = other.capa = 0;
        }

        /// 移动赋值
        HeapString& operator=(HeapString&& other) noexcept
        {
            if (XY_LIKELY(this != &other))
            {
                release();
                p = other.p;
                len = other.len;
                capa = other.capa;
                other.p = EmptyStr;
                other.len = other.capa = 0;
            }
            return *this;
        }

        /// 交换两个字符串
        HeapString& swap(HeapString& other) noexcept
        {
            xyu::swap(p, other.p);
            xyu::swap(len, other.len);
            xyu::swap(capa, other.capa);
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
        HeapString(xyu::special_t, xyu::size_t capa, char* buf) noexcept : HeapString(xyu::special_v, capa, buf, this->strlen(buf)) {}
        /**
         * @brief 转移构造
         * @param capa 分配的字符串容量
         * @param buf 必须为 alloc 分配的对象
         * @param bytes 实际有效的字符串长度
         */
        XY_NONNULL_RELEASE(4)
        HeapString(xyu::special_t, xyu::size_t capa, char* buf, xyu::size_t bytes) noexcept : p(buf), len(bytes), capa(capa)
        {
            if constexpr (!XY_RELEASE) if (XY_UNLIKELY(buf == nullptr))
                xyu::E_Logic::abort_null_pointer(XY_CurPos("HeapString::HeapString"), "buf");
        }

        /**
         * @brief 转移字符串
         * @attention 转移后的对象需要手动管理，或通过 xyu::special_v 转移给 另一个 HeapString
         */
        char* take() noexcept 
        { 
            char* tmp = p;
            p = EmptyStr;
            len = capa = 0;
            return tmp;
        } 

        /* 数据获取 */

        /// 获取字符串大小
        xyu::size_t size() const noexcept { return len; }
        /// 获取字符串容量
        xyu::size_t capacity() const noexcept { return capa; }

        /// 获取字符串
        char* data() noexcept XY_RETURNS_NONNULL { return p; }
        /// 获取字符串
        const char* data() const noexcept XY_RETURNS_NONNULL { return p; }

        /* 数据管理 */

        /// 清空字符串
        void clear() noexcept { set_size(0); }
        /// 释放内存
        void release() noexcept
        {
            if (XY_UNLIKELY(capa == 0)) return;
            do_dealloc(p, capa);
            p = EmptyStr;
            capa = len = 0;
        }

        /// 预分配空间
        void reserve(xyu::size_t mincapa) noexcept
        {
            if (capa >= mincapa) return;
            if (XY_UNLIKELY(mincapa > this->limit()))
                xyu::E_Logic::abort_over_limit(XY_CurPos("HeapString::reserve"), mincapa, 0, this->limit());
            do_expand(mincapa, [this](char* op, char* np){
                xyu::mem_copy(np, op, len);
            });
        }

        /// 尝试缩减容量
        void reduce() noexcept
        {
            if (len >= capa * K_shrink_factor) return;
            char* newdata;
            if (len)
            {
                newdata = do_alloc(len);
                xyu::mem_copy(newdata, p, len);
            }
            else newdata = EmptyStr;
            if (XY_LIKELY(capa > 0)) do_dealloc(p, capa);
            p = newdata;
            capa = len;
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
        void resize(xyu::size_t newsize, char pad) noexcept
        {
            reserve(newsize);
            if (newsize > len) xyu::mem_set(p + len, pad, newsize - len);
            set_size(newsize);
        }

    private:
        // 计算新容量
        xyu::size_t calc_capa(xyu::size_t mincapa) const noexcept
        {
            // 新容量 = mincapa
            xyu::size_t newcapa = mincapa;
            // 如果 新容量 较大，则 新容量 = max(最小容量, 旧容量 * K_grow_factor) --> align
            if (XY_LIKELY(newcapa > capa))
            {
                newcapa = xyu::max(mincapa, static_cast<xyu::size_t>(capa * K_grow_factor));
                if (XY_UNLIKELY(newcapa > this->limit())) return this->limit();
            }
            // 将 newcapa 调整到 K_DEFAULT_ALIGN 对齐的倍数 (<= limit())
            newcapa = ((newcapa + xyu::K_Default_Align) & -xyu::K_Default_Align) - 1;
            return newcapa;
        }

    private:
        friend class StringBase<HeapString>;

        // 设置新长度
        void set_size(xyu::size_t new_len) noexcept
        {
            len = new_len;
            p[len] = '\0';
        }

        // 扩容
        template <typename Migration>
        void do_expand(xyu::size_t mincapa, Migration&& migrate) noexcept
        {
            auto newcapa = calc_capa(mincapa);
            auto newdata = do_alloc(newcapa);
            migrate(p, newdata);
            if (XY_LIKELY(capa > 0)) do_dealloc(p, capa);
            p = newdata;
            capa = newcapa;
        }

    private:
        static char* do_alloc(xyu::size_t capa) noexcept
        {
            if constexpr (cross_thread) return xyu::alloc<char>(xyu::native_v, capa + 1);
            else return xyu::alloc<char>(capa + 1);
        }
        static void do_dealloc(char* p, xyu::size_t capa) noexcept
        {
            if constexpr (cross_thread) xyu::dealloc(xyu::native_v, p);
            else xyu::dealloc(p, capa + 1);
        }

    private:
        inline static char EmptyStr[1] = {'\0'};
    };

}
