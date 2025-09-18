#pragma clang diagnostic push
#pragma ide diagnostic ignored "hicpp-exception-baseclass"
#pragma ide diagnostic ignored "UnreachableCode"
#pragma ide diagnostic ignored "ConstantConditionsOC"
#pragma ide diagnostic ignored "ConstantFunctionResult"
#pragma ide diagnostic ignored "OCUnusedGlobalDeclarationInspection"
#pragma ide diagnostic ignored "google-explicit-constructor"
#pragma ide diagnostic ignored "cppcoreguidelines-pro-type-member-init"
#pragma ide diagnostic ignored "modernize-use-nodiscard"
#pragma once

#include "./strfun.h"
#include "../../link/new"

/* 字符串 */

/// 字符串
namespace xylu::xystring
{
    /**
     * @brief 一个拥有所有权的、可修改的、支持小字符串优化（SSO）的字符串类。
     * @details
     *   `String` 类负责管理其内部字符数据的生命周期。为了优化性能，它采用了三种
     *   内部存储模型，根据字符串的来源和长度自动选择：
     *
     *   ### 存储模型:
     *   1. **`Small` (小字符串)**:
     *      对于长度较短的字符串，其内容直接存储在 `String` 对象内部的栈上缓冲区中，
     *      避免了任何堆内存分配，从而极大地提高了性能。
     *
     *   2. **`Large` (大字符串)**:
     *      对于超过内部缓冲区容量的字符串，其内容会被分配在堆上。`String` 对象
     *      此时存储指向堆内存的指针、长度和容量信息。
     *
     *   3. **`Fixed` (固定字符串)**:
     *      当 `String` 从编译期字符串字面量构造时，它会进入此模式。此时，它仅存储
     *      一个指向该常量数据的指针和长度，不进行任何内存分配。这是一种零开销的模式。
     *
     *   ### 存储方式:
     *   - Small 的 data 访问与 Large/Fixed 的必然不同 (因为 Small 存储"实体"，而 Large/Fixed 存储"指针") <br>
     *   - size 与 kind 放在前面，无需判断即可访问 <br>
     *   - kind 在 size 后面，Small 的 data 无需被对齐吞掉空间 <br>
     *   - 访问 data 只需要判断是否为 Small (即一个分支)，因为 Large/Fixed 的 data 统一 <br>
     *
     * @note
     *   - **写时复制 (Copy-on-Write)**: 对一个 `Fixed` 状态的 `String` 进行任何
     *     非 `const` 的修改操作（如 `data()`, `operator[]` 的非 `const` 版本），
     *     会触发一次“复制”操作，将其内容拷贝到一个新的 `Small` 或 `Large` 存储中，
     *     从而保证原始的常量数据不被修改。
     *   - **常量访问**: 为了避免不必要的复制，当您只需要读取 `Fixed` 字符串的
     *     内容时，请使用 `cdata()`, `cget()`, `crange()` 等 `const` 版本的访问函数。
     */
    class String : public StringBase<String>
    {
    private:
        /// 字符串类型
        enum KIND : xyu::uint8 { Small, Large, Fixed };
        /// 堆上长字符串
        struct SLarge
        {
            xyu::size_t size;   // 字符串大小
            KIND kind;          // 字符串类型
            char* data;         // 堆内存地址
            xyu::size_t capa;   // 堆内存容量
        };
        /// 栈上短字符串
        struct SSmall
        {
            xyu::size_t size;   // 字符串大小
            KIND kind;          // 字符串类型
            char data[sizeof(SLarge) - sizeof(xyu::size_t) - sizeof(xyu::uint8)];
            // 字符串存储 (包括'\0')
        };
        /// 固定常量字符串
        struct SFixed
        {
            xyu::size_t size;   // 字符串大小
            KIND kind;          // 字符串类型
            const char* data;   // 常量字符串地址
        };

    public:
        /* 静态常量  */
        /// 释放空间条件比例
        static constexpr auto K_shrink_factor = 0.5;
        /// 扩容比例
        static constexpr auto K_grow_factor = 1.5;

    private:
        /* 联合体存储 */
        union { SSmall s; SLarge l; SFixed f; };

    public:
        /* 构造析构 */

        /**
         * @brief 默认构造函数
         * @note 在栈上构造一个空字符串
         */
        String() noexcept : s{0, Small, '\0'} {}

        /// 复制构造函数
        String(const String& other)
        {
            s.kind = other.s.kind;
            s.size = other.s.size;
            // Fixed字符串
            if (s.kind == Fixed) l.data = other.l.data;
            // Small字符串
            else if (s.kind == Small)
                xyu::mem_copy(s.data, other.s.data, s.size),
                s.data[s.size] = '\0';
            // Large字符串
            else
                s.kind = Small, // 防止 Large 内存分配失败后 析构函数错误释放内存
                large_alloc(other.l.data, s.size, s.size);
        }

        /// 移动构造函数
        String(String&& other) noexcept
        {
            s.kind = other.s.kind;
            s.size = other.s.size;
            // Fixed字符串
            if (s.kind == Fixed) l.data = other.l.data;
            // Small字符串
            else if (s.kind == Small)
                xyu::mem_copy(s.data, other.s.data, s.size),
                s.data[s.size] = '\0';
            // Large字符串
            else
                l.capa = other.l.capa,
                l.data = other.l.data,
                //重置 other
                other.s.kind = Small,
                other.s.size = 0,
                other.s.data[0] = '\0';
        }

        /**
         * @brief 通过字符串字面量构造
         * @note 高效存储为 Fixed 类型
         * @note 此处的 N 包括'\0'
         */
        template <xyu::size_t N, typename T, typename = xyu::t_enable<xyu::t_is_same<T, const char>>>
        String(T(&str)[N]) noexcept : f{N-1, Fixed, str} { static_assert(N > 0); }

        /**
         * @brief 通过char*构造
         * @param bytes 字符串长度，不包括'\0'
         * @note 根据长度存储为 Small 或 Large 类型
         */
        String(const char* str, xyu::size_t bytes) : String{StringView{str, bytes}} {}
        /// 通过 StringView 构造
        String(const StringView& view)
        {
            const char* str = view.data();
            xyu::size_t bytes = view.size();
            // 先设置为 Small 类型 (防止 Large 内存分配失败后 析构函数错误释放内存)
            s.kind = Small;
            // 长度较小
            if (bytes < sizeof(s.data))
                xyu::mem_copy(s.data, str, bytes),
                s.size = bytes,
                s.data[bytes] = '\0';
            // 长度较大
            else large_alloc(str, bytes, bytes);
        }

        /**
         * @brief 预分配空间构造
         * @param capa 为最小字符串容量，不包括'\0'
         * @note 短字符串将直接存储在栈上，长字符串将容量对齐后分配堆内存
         */
        template <typename Int, typename = xyu::t_enable<!xyu::t_is_same_nocvref<Int, char> && xyu::t_is_integral<Int>>>
        explicit String(Int capa)
        {
            // 先设置为 Small 类型 (防止 Large 内存分配失败后 析构函数错误释放内存)
            s.kind = Small;
            // 栈空间充足
            if (capa < sizeof(s.data))
                s.size = 0,
                s.data[0] = '\0';
            // 栈空间不足
            else large_alloc("", 0, capa);
        }

        /// 分配 count 个 c字符
        String(xyu::size_t count, char c)
        {
            // 先设置为 Small 类型 (防止 Large 内存分配失败后 析构函数错误释放内存)
            s.kind = Small;
            char* p;
            // 栈空间充足
            if (count < sizeof(s.data))
                p = s.data;
            // 栈空间不足
            else
                large_alloc("", 0, count),
                p = l.data;
            s.size = count;
            xyu::mem_set(p, count, c);
            p[count] = '\0';
        }
        /// 分配 count 个 c字符
        String(char c, xyu::size_t count) : String{count, c} {}

        /**
         * @brief 通过 native_t 标记 构造固定常量字符串
         * @param bytes 字符串长度，不包括'\0'
         */
        String(xyu::native_t, const char* str, xyu::size_t bytes) XY_NOEXCEPT_NDEBUG : String{xyu::native_v, StringView{str, bytes}} {}
        /// 通过 native_t 标记 构造固定常量字符串
        String(xyu::native_t, const StringView& view) noexcept : f{view.size(), Fixed, view.data()} {}

        /**
         * @brief 通过 special_t 标记 转移动态分配的字符串
         * @note capa 不包括'\0'
         * @note str 必须通过 alloc() 分配，转移后不应该再使用
         */
        String(xyu::special_t, xyu::size_t capa, char* buf) XY_NOEXCEPT_NDEBUG : String{xyu::special_v, capa, buf, calc_size(buf)} {}
        /**
         * @brief 通过 special_t 标记 转移动态分配的字符串
         * @note bytes 与 capa 均不包括'\0'
         * @note str 必须通过 alloc() 分配，转移后不应该再使用
         */
        String(xyu::special_t, xyu::size_t capa, char* buf, xyu::size_t bytes) XY_NOEXCEPT_NDEBUG : l{bytes, Large, buf, capa}
        {
#if XY_DEBUG
            if (XY_UNLIKELY(buf == nullptr)) {
                xylogei(false, "E_Logic_Null_Pointer: null pointer");
                throw xyu::E_Logic_Null_Pointer{};
            }
#endif
        }

        /// 析构函数
        ~String() noexcept
        {
            if (s.kind != Large) return;
            xyu::dealloc<char>(l.data, l.capa + 1, xyu::K_DEFAULT_ALIGN);
        }

        /* 赋值交换 */

        /**
         * @brief 复制赋值运算符
         * @note Fixed得到Fixed，其他则根据 other 的长度决定 Large/Small 类型
         */
        String& operator=(const String& other)
        {
            if (XY_UNLIKELY(this == &other)) return *this;
            s.size = other.s.size;
            const char* str = other.data();
            // 释放内存
            if (s.kind == Large && (other.s.kind == Fixed || s.size < sizeof(s.data)))
                xyu::dealloc<char>(l.data, l.capa + 1, xyu::K_DEFAULT_ALIGN),
                s.kind = Small; // 防止 Large 内存分配失败后 析构函数错误释放内存
            // Fixed字符串
            if (other.s.kind == Fixed)
                s.kind = Fixed,
                f.data = str;
            else {
                // Large字符串且容量不足
                if (other.s.kind == Large && s.size > l.capa)
                    large_alloc(str, s.size, s.size);
                // Small字符串 或 长度较小的Large字符串
                else
                    xyu::mem_copy(s.data, str, s.size),
                    s.data[s.size] = '\0';
            }
            return *this;
        }

        /**
         * @brief 移动赋值运算符
         * @note 直接交换两个对象
         */
        String& operator=(String&& other) noexcept { swap(other); return *this; }

        /**
         * @brief 字符串字面量赋值
         * @note 直接改为 Fixed 存储，如果为 Large 类型，则先释放旧内存，
         */
        template <xyu::size_t N, typename T, typename = xyu::t_enable<xyu::t_is_same<T, const T>>>
        String& operator=(T(&str)[N]) noexcept
        {
            static_assert(N > 0);
            if (s.kind == Large) xyu::dealloc<char>(l.data, l.capa + 1, xyu::K_DEFAULT_ALIGN);
            s.kind = Fixed;
            s.size = N-1;
            f.data = str;
            return *this;
        }

        /**
         * @brief StringView 赋值
         * @note 当新字符串的长度 ˂ 栈容量 * K_free_factor，或 ˃ 堆容量时，将释放旧内存，否则直接拷贝内存
         */
        String& operator=(const StringView& view)
        {
            const char* str = view.data();
            s.size = view.size();
            // 长字符串
            if (s.size >= sizeof(s.data)) {
                if (s.size > capacity()) large_alloc(str, s.size, s.size);
                else {
                    xyu::mem_copy(l.data, str, s.size);
                    l.data[s.size] = '\0';
                }
            }
            // 短字符串
            else {
                if (s.kind == Large) xyu::dealloc<char>(l.data, l.capa + 1, xyu::K_DEFAULT_ALIGN);
                s.kind = Small;
                xyu::mem_copy(s.data, str, s.size);
                s.data[s.size] = '\0';
            }
            return *this;
        }

        /// 交换两个字符串
        String& swap(String&& other) noexcept { return swap(other); }
        /// 交换两个字符串
        String& swap(String& other) noexcept
        {
            xyu::uint8 buf[sizeof(String)];
            xyu::mem_copy(buf, this, sizeof(String));
            xyu::mem_copy(this, &other, sizeof(String));
            xyu::mem_copy(&other, buf, sizeof(String));
            return *this;
        }

        /* 数据容量 */

        /**
         * @brief 字符串最大长度 (不包括 '\0')
         * @note 必然为 2^n - 1
         */
        static constexpr xyu::size_t limit() noexcept { return xyu::number_traits<xyu::size_t>::max / 2; }
        /**
         * @brief 获取字符串容量 (不包括 '\0')
         * @note Fixed 字符串容量为 0
         * @note 非频繁访问，多分支判断综合影响较小
         */
        xyu::size_t capacity() const noexcept
        { return s.kind == Large ? l.capa : (s.kind == Small ? sizeof(s.data)-1 : 0); }

        /// 获取字符串长度 (不包括 '\0')
        xyu::size_t size() const noexcept { return s.size; }

        /* 元素获取 */

        /**
         * @brief 获取字符串指针 (可修改)
         * @note 对 Fixed 字符串进行非 const 操作 将先进行复制
         */
        XY_RETURNS_NONNULL char* data()
        {
            // 对 Fixed 字符串进行非 const 操作 将先进行复制
            // (并且推测如果调用一次，则非const操作很可能调用多次，后面同理)
            if (XY_UNLIKELY(s.kind == Fixed)) {
                const char* str = l.data;
                xyu::size_t size = s.size;
                s.kind = Small;  // 防止 Large 内存分配失败后 析构函数错误释放内存
                if (size < sizeof(s.data))
                    xyu::mem_copy(s.data, str, size),
                    s.data[size] = '\0';
                else
                    large_alloc(str, size, size);
            }
            // Fixed 与 Large 的 data 访问相同
            return s.kind == Small ? s.data : l.data;
        }
        /// 获取字符串指针 (不可修改)
        XY_RETURNS_NONNULL const char* data() const noexcept { return cdata(); }
        /// 获取字符串指针 (不可修改)
        XY_RETURNS_NONNULL const char* cdata() const noexcept { return s.kind == Small ? s.data : l.data; }

        /**
         * @brief 获取字符 (无范围检测) (可修改)
         * @note 对 Fixed 字符串进行非 const 操作 将先进行复制
         */
        char& get(xyu::size_t index) XY_NOEXCEPT_NDEBUG
        {
#if XY_DEBUG
            return at(index);
#else
            return data()[index];
#endif
        }
        /// 获取字符 (无范围检测)
        char get(xyu::size_t index) const XY_NOEXCEPT_NDEBUG { return cget(index); }
        /// 获取字符 (无范围检测)
        char cget(xyu::size_t index) const XY_NOEXCEPT_NDEBUG
        {
#if XY_DEBUG
            return cat(index);
#else
            return cdata()[index];
#endif
        }

        /**
         * @brief 获取字符 (有范围检测) (可修改)
         * @note 对 Fixed 字符串进行非 const 操作 将先进行复制
         * @exception xxx index 超出范围
         */
        char& at(xyu::size_t index)
        {
            if (XY_UNLIKELY(index >= s.size)) {
                xylogei(false, "E_Logic_Out_Of_Range: index {} out of range [0, {})", index, s.size);
                throw xyu::E_Logic_Out_Of_Range{};
            }
            return data()[index];
        }
        /// 获取字符 (有范围检测)
        char at(xyu::size_t index) const { return cat(index); }
        /// 获取字符 (有范围检测)
        char cat(xyu::size_t index) const
        {
            if (XY_UNLIKELY(index >= s.size)) {
                xylogei(false, "E_Logic_Out_Of_Range: index {} out of range [0, {})", index, s.size);
                throw xyu::E_Logic_Out_Of_Range{};
            }
            return cdata()[index];
        }

        /**
         * @brief 获取字符串的子串
         * @param pos 起始位置
         * @param len 子串长度 (可以大于字符串长度，自动截断)
         * @note Fixed 字符串调用时则仍得到 Fixed 字串
         */
        String substr(xyu::size_t pos = 0, xyu::size_t len = -1) const
        {
            if (XY_UNLIKELY(pos >= s.size)) return {};
            if (s.size - pos <= len) {
                if (s.kind == Fixed) return {xyu::native_v, f.data + pos, s.size - pos};
                else len = s.size - pos;
            }
            if (s.kind == Small) return {s.data + pos, len};
            else return {f.data + pos, len};
        }

        /* 数据管理 */

        /**
         * @brief 预分配空间
         * @param minsize 最小容量
         * @return 新容量
         * @note Fixed 字符串调用时则先进行复制
         */
        xyu::size_t reserve(xyu::size_t minsize)
        {
            // Fixed 字符串 复制数据
            if (XY_UNLIKELY(s.kind == Fixed)) {
                xyu::size_t ms = xyu::max(minsize, s.size);
                // 变为 Small 字符串
                if (ms < sizeof(s.data)) {
                    s.kind = Small;
                    xyu::mem_copy(s.data, f.data, s.size);
                    s.data[s.size] = '\0';
                    return sizeof(s.data)-1;
                }
                // 变为 Large 字符串
                else large_alloc(f.data, s.size, ms);
            }
            else if (minsize > capacity()) large_alloc(data(), s.size, minsize);
            return l.capa;
        }
        /**
         * @brief 缩减容量
         * @note 仅当元素数量 ˂ 容量 * K_shrink_factor 时才进行缩减
         * @note 仅当字符串为 Large 类型时才进行操作
         */
        void reduce()
        {
            if (s.kind != Large) return;
            if (s.size < static_cast<xyu::size_t>(static_cast<double>(capacity()) * K_shrink_factor))
            {
                // 容量足够小使用 Small 类型
                if (s.size < sizeof(s.data)) {
                    char* p = l.data;
                    xyu::size_t capa = l.capa;
                    xyu::mem_copy(s.data, p, s.size);
                    s.data[s.size] = '\0';
                    s.kind = Small;
                    xyu::dealloc<char>(p, capa + 1, xyu::K_DEFAULT_ALIGN);
                }
                // 否则仍然为 Large 类型
                else large_alloc(data(), s.size, s.size);
            }
        }

        /**
         * @brief 调整元素数量
         * @note 可以增加(仅增大空间，不设置字符填充)，也可以减小
         * @note Fixed 字符串调用时会先进行复制
         */
        void resize(xyu::size_t newsize)
        {
            if (XY_UNLIKELY(s.kind == Fixed) && newsize < s.size) s.size = newsize;  // 防止 Fixed 复制不需要的部分
            reserve(newsize);
            s.size = newsize;
            data()[newsize] = '\0';
        }
        /**
         * @brief 调整元素数量
         * @note 可以增加(用c填充增加部分)，也可以减小
         * @note Fixed 字符串调用时会先进行复制
         */
        void resize(xyu::size_t newsize, char c)
        {
            if (XY_UNLIKELY(s.kind == Fixed) && newsize < s.size) s.size = newsize;  // 防止 Fixed 复制不需要的部分
            reserve(newsize);
            if (newsize > s.size) xyu::mem_set(data() + s.size, newsize - s.size, c);
            s.size = newsize;
            data()[newsize] = '\0';
        }

        /**
         * @brief 清空数据
         * @note Fixed 字符串将变为 空的Small，Large仍保留容量
         */
        void clear() noexcept
        {
            s.size = 0;
            if (s.kind == Fixed)
                s.kind = Small,
                s.data[0] = '\0';
            else (s.kind == Small ? s.data : l.data)[0] = '\0';
        }
        /**
         * @brief 释放内存
         * @note 释放Large内存，并统一重置为 空的Small
         */
        void release() noexcept
        {
            if (s.kind == Large) xyu::dealloc<char>(l.data, l.capa + 1, xyu::K_DEFAULT_ALIGN);
            s.kind = Small,
            s.size = 0,
            s.data[0] = '\0';
        }

        /* 增 */

        /// 尾部追加 count(默认为1) 个字符c
        String& append(char c, xyu::size_t count = 1)
        {
            check_add(count);
            reserve(s.size + count);
            xyu::mem_set(data() + s.size, count, c);
            s.size += count;
            data()[s.size] = '\0';
            return *this;
        }
        /// 尾部追加 count(默认为1) 个字符c
        String& append(xyu::size_t count, char c) { return append(c, count); }
        /// 尾部追加字符串
        String& append(const StringView& view)
        {
            check_add(view.size());
            reserve(size() + view.size());
            xyu::mem_copy(data() + s.size, view.data(), view.size());
            data()[s.size + view.size()] = '\0';
            s.size += view.size();
            return *this;
        }

        /// 中间插入 count(默认为1) 个字符c 到 index 开始的位置
        String& insert(xyu::size_t index, char c, xyu::size_t count = 1)
        {
            char *sp = after_move(index, count);
            xyu::mem_set(sp + index, count, c);
            return *this;
        }
        /// 中间插入 count(默认为1) 个字符c 到 index 开始的位置
        String& insert(xyu::size_t index, xyu::size_t count, char c) { return insert(index, c, count); }
        /// 中间插入字符串 到 index 开始的位置
        String& insert(xyu::size_t index, const StringView& view)
        {
            char *sp = after_move(index, view.size());
            xyu::mem_copy(sp + index, view.data(), view.size());
            return *this;
        }

        /* 删 */

        /// 删除索引开始的所有元素
        String& erase(xyu::size_t index, xyu::size_t count = -1)
        {
            if (index >= s.size) return *this;
            if (s.size - index <= count) resize(index);
            else before_move(index + count, count);
            return *this;
        }
        /// 删除迭代器位置，并返回下一个位置的迭代器
        template <typename Iter, typename = xyu::t_enable<xyu::t_is_same_nocvref<decltype(*xyu::t_val<Iter>()), char>>>
        Iter erase(Iter it, xyu::size_t count = 1)
        {
            auto p = it.operator->();
#if XY_DEBUG
            if (XY_UNLIKELY(p < data() || p >= data() + size())) {
                xylogei(false, "E_Logic_Invalid_Iterator: Iterator {} not belong to string range [{}, {})", p, data(), data() + size());
                throw xyu::E_Logic_Invalid_Iterator{};
            }
#endif
            erase(p - data(), count);
            return it;
        }

        /**
         * @brief 删除字符串中的字串
         * @param pattern 匹配的字符串
         * @param start 开始位置
         * @param end 结束位置 (不包括)
         * @param count 重复删除的次数 (默认为-1，表示全部删除)
         * @return 返回删除的次数
         * @note 如果为 Fixed 字符串，则会先进行复制
         */
        xyu::size_t remove(const StringView& pattern, xyu::size_t start = 0, xyu::size_t end = -1, xyu::size_t count = -1)
        {
            // 范围检测
            if (XY_UNLIKELY(start >= s.size) || XY_UNLIKELY(count == 0)) return 0;
            if (end > s.size) end = s.size;
            else if (XY_UNLIKELY(start >= end)) return 0;

            // 查找第一个
            xyu::size_t sfind =  find(pattern, start, end);  // 当前匹配位置
            if (sfind == -1) return 0;

            // 记录位置
            xyu::size_t scover = sfind;                     // 开始覆盖的位置
            xyu::size_t smove = sfind + pattern.size();     // 开始移动的位置

            // 循环查找
            char* str = data();
            xyu::size_t rm_count = 1;
            for (; rm_count < count; ++rm_count)
            {
                //再次查找
                sfind = find(pattern, sfind + pattern.size(), end);
                if (sfind == -1) break;
                //覆盖上一块
                xyu::size_t len = sfind - smove;
                xyu::mem_move(str + scover, str + smove, len);
                //更新位置
                scover += len;
                smove = sfind + pattern.size();
            }

            // 处理最后一块
            xyu::size_t len = s.size - smove;
            xyu::mem_move(str + scover, str + smove, len);

            // 更新大小并返回
            s.size = scover + len;
            data()[s.size] = '\0';
            return rm_count;
        }

        /* 改 */

        /// 更新/覆盖索引位置的字符串为 count(默认为1) 个字符c (允许扩容)
        String& update(xyu::size_t index, char c, xyu::size_t count = 1)
        {
            if (index >= s.size) return append(c, count);
            auto [overcapa, sp] = after_update(index, count);
            xyu::mem_set(sp + index, count, c);
            if (overcapa) sp[s.size] = '\0';
            return *this;
        }
        /// 更新/覆盖索引位置的字符串为 count(默认为1) 个字符c (允许扩容)
        String& update(xyu::size_t index, xyu::size_t count, char c) { return update(index, c, count); }
        /// 更新/覆盖索引位置的字符串 (允许扩容)
        String& update(xyu::size_t index, const StringView& view)
        {
            if (index >= s.size) return append(view);
            auto [overcapa, sp] = after_update(index, view.size());
            xyu::mem_copy(sp + index, view.data(), view.size());
            if (overcapa) sp[s.size] = '\0';
            return *this;
        }

        /// 从 index 开始，将后面部分往左移动 step 个字符 (长度缩减)
        String& lmove(xyu::size_t index, xyu::size_t step) { before_move(index, step); return *this; }
        /**
         * @brief 从 index 开始，将后面部分往右移动 step 个字符 (长度增加, 允许扩容)
         * @note 为了效率，进行 Fixed的复制 或 Large的扩容 时移动的部分[index,index+step)会跳过，所以得到的结果中移动的部分是不确定的。
         */
        String& rmove(xyu::size_t index, xyu::size_t step) { after_move(index, step); return *this; }

        /**
         * @brief 替换字符串中的指定位置
         * @param index 开始位置
         * @param rpbytes 被替换的字节数
         * @param c 被替换的字符
         * @param count 重复替换的字符数
         */
        String& replace(xyu::size_t index, xyu::size_t rpbytes, char c, xyu::size_t count = 1)
        {
            if (index > s.size) index = s.size;
            if (index + rpbytes > s.size) rpbytes = s.size - index;
            //移动数据
            if (rpbytes >= count) before_move(index, rpbytes - count);
            else after_move(index, count - rpbytes);
            //更新数据
            char* p = data() + index;
            xyu::mem_set(p, count, c);
            return *this;
        }
        /**
         * @brief 替换字符串中的指定位置
         * @param index 开始位置
         * @param rpbytes 被替换的字节数
         * @param view 被替换的字符串
         */
        String& replace(xyu::size_t index, xyu::size_t rpbytes, const StringView& view)
        {
            xyu::size_t bytes = view.size();
            //范围检查
            if (index > s.size) index = s.size;
            if (index + rpbytes > s.size) rpbytes = s.size - index;
            //移动数据
            if (rpbytes >= bytes) before_move(index + rpbytes, rpbytes - bytes);
            else after_move(index, bytes - rpbytes);
            //更新数据
            char* sp = data() + index;
            xyu::mem_copy(sp, view.data(), bytes);
            return *this;
        }

        /**
         * @brief 替换字符串中的指定位置
         * @param pattern 匹配的字符串
         * @param rp 替换后的字符串
         * @param start 开始位置
         * @param end 结束位置 (不包括)
         * @param count 重复替换的次数 (默认为-1，表示全部替换)
         * @return 返回替换(匹配成功)的次数
         */
        xyu::size_t replace(const StringView& pattern, const StringView& rp, xyu::size_t start = 0, xyu::size_t end = -1, xyu::size_t count = -1)
        {
            // 范围检测
            if (XY_UNLIKELY(start >= s.size) || XY_UNLIKELY(count == 0)) return 0;
            if (end > s.size) end = s.size;
            else if (XY_UNLIKELY(start >= end)) return 0;

            // 查找第一个
            xyu::size_t sfind = find(pattern, start, end);  // 当前匹配位置
            if (sfind == -1) return 0;

            //---- 不会扩容 ----
            if (rp.size() <= pattern.size() && XY_LIKELY(s.kind != Fixed))
            {
                // 记录位置
                xyu::size_t scover = sfind;                     // 开始覆盖的位置
                xyu::size_t smove = sfind + pattern.size();     // 开始移动的位置

                // 循环查找
                char* str = data();
                xyu::size_t rm_count = 1;
                for (; rm_count < count; ++rm_count)
                {
                    //再次查找
                    sfind = find(pattern, sfind + pattern.size(), end);
                    if (sfind == -1) break;
                    //写入替换内容
                    xyu::mem_copy(str + scover, rp.data(), rp.size());
                    scover += rp.size();
                    //移动未匹配部分
                    xyu::size_t len = sfind - smove;
                    xyu::mem_move(str + scover, str + smove, len);
                    //更新位置
                    scover += len;
                    smove = sfind + pattern.size();
                }

                // 处理最后一块
                xyu::mem_copy(str + scover, rp.data(), rp.size());
                scover += rp.size();
                xyu::size_t len = s.size - smove;
                xyu::mem_move(str + scover, str + smove, len);

                // 更新大小并返回
                s.size = scover + len;
                data()[s.size] = '\0';
                return rm_count;
            }

            //---- 可能会扩容 ----
            // 查找所有出现的位置
            struct ExVec
            {
            private:
                xyu::size_t tmp[8];
                xyu::size_t* p = tmp;
                xyu::size_t capa = 8;
                xyu::size_t size = 0;
            public:
                // 析构，自动释放内存
                ~ExVec() noexcept { if (p != tmp) xyu::dealloc<xyu::size_t>(p, capa); }
                // 添加元素
                void push(xyu::size_t v)
                {
                    // 扩容
                    if (XY_UNLIKELY(size == capa)) {
                        // 其元素数量不可能超过 limit()，即 最大字符容量
                        xyu::size_t newcapa = capa << 1;
                        if (XY_UNLIKELY(newcapa < capa)) newcapa = String::limit();
                        auto newp = xyu::alloc<xyu::size_t>(newcapa);
                        xyu::mem_copy(newp, p, size);
                        if (p != tmp) xyu::dealloc<xyu::size_t>(p, capa);
                        p = newp;
                        capa = newcapa;
                    }
                    p[size++] = v;
                }
                // 获取元素
                xyu::size_t operator[](xyu::size_t i) const noexcept { return p[i]; }
            } matchs;  // 记录所有匹配位置的索引数组
            xyu::size_t rm_count = 1;
            matchs.push(sfind);
            for (;rm_count < count; ++rm_count)
            {
                sfind = find(pattern, sfind + pattern.size(), end);
                if (sfind == -1) break;
                matchs.push(sfind);
            }

            // 新容量计算
            xyu::size_t addcapa = rm_count * (rp.size() - pattern.size());
            xyu::size_t mincapa = s.size + addcapa;
            xyu::size_t oldcapa = capacity();   // Fixed 容量为 0

            //-- 不需要扩容 --
            if (mincapa <= oldcapa)
            {
                // 处理最后部分
                char* first = data();
                char* scover = first + matchs[rm_count - 1] + addcapa;
                xyu::size_t len = s.size - matchs[rm_count - 1] - pattern.size() + 1; // 包括 '\0'
                xyu::mem_move(scover, first + matchs[rm_count - 1] + pattern.size(), len);
                scover -= rp.size();
                xyu::mem_copy(scover, rp.data(), rp.size());

                // 处理中间部分
                for (xyu::size_t i = rm_count - 2; i < rm_count; --i)
                {
                    len = matchs[i + 1] - matchs[i] - pattern.size();
                    scover -= len;
                    xyu::mem_move(scover, first + matchs[i] + pattern.size(), len);
                    scover -= rp.size();
                    xyu::mem_copy(scover, rp.data(), rp.size());
                }

                // 开头部分不变，无需处理
            }

            //-- 需要扩容 --
            else
            {
                // s.kind == Fixed || s.kind == Large
                xyu::size_t newcapa = calc_new_capa(mincapa);
                // 是否需要使用动态分配内存 (Fixed较小 或 Large缩窄)
                bool overstack = mincapa >= sizeof(s.data);

                // 扩容
                const char* olddata = cdata();
                char* newdata;
                if (overstack) newdata = xyu::alloc<char>(newcapa + 1, xyu::K_DEFAULT_ALIGN);
                else newdata = s.data;
                char* cur = newdata;

                // 处理开头未匹配部分
                xyu::mem_copy(cur, data(), matchs[0]);
                cur += matchs[0];

                // 复制匹配部分
                for (xyu::size_t i = 1; i < rm_count; ++i)
                {
                    // 写入替换内容
                    xyu::mem_copy(cur, rp.data(), rp.size());
                    cur += rp.size();
                    // 复制未匹配部分
                    xyu::size_t len = matchs[i] - matchs[i - 1] - pattern.size();
                    xyu::mem_copy(cur, olddata + matchs[i - 1] + pattern.size(), len);
                    cur += len;
                }

                // 处理最后部分
                xyu::mem_copy(cur, rp.data(), rp.size());
                cur += rp.size();
                xyu::size_t len = s.size - matchs[rm_count - 1] - pattern.size();
                xyu::mem_copy(cur, olddata + matchs[rm_count - 1] + pattern.size(), len);
                cur += len;
                *cur = '\0';
                s.size = cur - newdata;

                // 替换空间
                if (s.kind == Large) xyu::dealloc<char>((char*)olddata, l.capa + 1, xyu::K_DEFAULT_ALIGN);
                if (!overstack) s.kind = Small;
                else
                    s.kind = Large,
                    l.capa = newcapa,
                    l.data = newdata;
            }

            return rm_count;
        }

        /* 范围 */

        /// 生成可变范围
        auto range() noexcept { return xyu::make_range(s.size, data(), data() + s.size); }
        /// 生成不可变范围
        auto range() const noexcept { return xyu::make_range(s.size, cdata(), cdata() + s.size); }
        /// 生成不可变范围 (方便 Fixed 类型使用)
        auto crange() const noexcept { return xyu::make_range(s.size, cdata(), cdata() + s.size); }

        /* 运算符 */

        /**
         * @brief 索引访问 (可修改)
         * @note 支持负索引，即 [-size,-1] 映射到 [0,size-1]
         * @note (-∞,-size) 与 [size,+∞) 则 映射到 两端 即 0 和 size-1
         */
        char& operator[](xyu::diff_t index) XY_NOEXCEPT_NDEBUG
        {
            auto i = static_cast<xyu::size_t>(index);
            if (index >= 0) return get(i >= s.size ? s.size - 1 : i);
            else return get(XY_UNLIKELY((i += s.size) >= s.size) ? 0 : i);
        }
        /**
         * @brief 索引访问 (不可修改)
         * @note 支持负索引，即 [-size,-1] 映射到 [0,size-1]
         * @note (-∞,-size) 与 [size,+∞) 则 映射到 两端 即 0 和 size-1
         */
        char operator[](xyu::diff_t index) const XY_NOEXCEPT_NDEBUG
        {
            auto i = static_cast<xyu::size_t>(index);
            if (index >= 0) return cget(i >= s.size ? s.size - 1 : i);
            else return cget(XY_UNLIKELY((i += s.size) >= s.size) ? 0 : i);
        }

        template <typename T>
        String& operator<<(T&& t) { return append(xyu::forward<T>(t)); }
        template <typename T>
        String& operator+=(T&& t) { return append(xyu::forward<T>(t)); }

        String operator+(char c) const { String tmp(s.size + 1); return tmp.append(*this).append(c); }
        String operator+(const StringView& view) const { String tmp(s.size + view.size()); return tmp.append(*this).append(view); }
        friend String operator+(char c, const String& str) { String tmp(str.size() + 1); return tmp.append(c).append(str); }
        friend String operator+(const StringView& view, const String& str) { String tmp(str.size() + view.size()); return tmp.append(view).append(str); }

        template <typename T>
        String operator+(T&& v) && { return xyu::move(append(xyu::forward<T>(v))); }
        String operator+(String&& str) const { return xyu::move(str.insert(0, *this)); }
        friend String operator+(char c, String&& str) { return xyu::move(str.insert(0, c)); }
        friend String operator+(const StringView& view, String&& str) { return xyu::move(str.insert(0, view)); }

    private:
        // 检查增加的元素数量
        static void check_add(xyu::size_t count)
        {
            if (XY_UNLIKELY(count > limit())) {
                xylogei(false, "E_Memory_Capacity: add {} over limit {}", count, limit());
                throw xyu::E_Memory_Capacity{};
            }
        }
        // 计算新容量
        xyu::size_t calc_new_capa(xyu::size_t mincapa) const
        {
            // 超出容量上限
            if (XY_UNLIKELY(mincapa > limit())) {
                xylogei(false, "E_Memory_Capacity: mincapa {} over limit {}", mincapa, limit());
                throw xyu::E_Memory_Capacity{};
            }
            // 新容量 = mincapa
            xyu::size_t newcapa = mincapa;
            // 如果 新容量 较大，则 新容量 = max(最小容量, 旧容量 * K_grow_factor) --> align
            if (XY_LIKELY(newcapa > capacity())) {
                newcapa = xyu::max(mincapa, static_cast<xyu::size_t>(static_cast<double>(capacity()) * K_grow_factor));
                if (XY_UNLIKELY(newcapa > limit())) return limit();
            }
            // 将 newcapa 调整到 K_DEFAULT_ALIGN 对齐的倍数 (必定 <= limit())
            newcapa = ((newcapa + xyu::K_DEFAULT_ALIGN) & -xyu::K_DEFAULT_ALIGN) - 1;
            return newcapa;
        }
        // 重新分配堆内存
        void large_alloc(const char* str, xyu::size_t bytes, xyu::size_t mincapa)
        {
            xyu::size_t newcapa = calc_new_capa(mincapa);
            char* newdata = xyu::alloc<char>(newcapa + 1, xyu::K_DEFAULT_ALIGN);
            xyu::mem_copy(newdata, str, bytes);
            newdata[bytes] = '\0';
            if (s.kind == Large) xyu::dealloc<char>(l.data, l.capa + 1, xyu::K_DEFAULT_ALIGN);
            l.data = newdata;
            l.capa = newcapa;
            s.size = bytes;
            s.kind = Large;
        }

        // 将index及之后的位置向后移动step，返回新数据指针
        char* after_move(xyu::size_t& index, xyu::size_t step)
        {
            if (XY_UNLIKELY(s.size + step < s.size || s.size + step > limit())) {
                xylogei(false, "E_Memory_Capacity: now {} add {} over limit {}", capacity(), step, limit());
                throw xyu::E_Memory_Capacity{};
            }

            if (XY_UNLIKELY(index > s.size)) index = s.size;
            char* p;
            xyu::size_t oldcapa;
            // Fixed 字符串 复制数据
            if (XY_LIKELY(s.kind == Fixed)) {
                p = l.data;
                // 变为 Small 字符串
                if (s.size + step < sizeof(s.data)) {
                    s.kind = Small;
                    xyu::mem_copy(s.data, p, index);
                    xyu::mem_copy(s.data + index + step, p + index, s.size - index + 1);  // 包括 '\0'
                    s.size += step;
                    return s.data;
                }
                // 变为 Large 字符串 (后续通过 扩容 部分实现)
                else oldcapa = s.size;
            }
            else p = data(), oldcapa = capacity();
            // 容量充足
            if (oldcapa >= step + s.size)
                xyu::mem_move(p + index + step, p + index, s.size - index + 1);  // 包括 '\0'
            // 扩容
            else {
                xyu::size_t newcapa = calc_new_capa(s.size + step);
                char* newdata = xyu::alloc<char>(newcapa + 1, xyu::K_DEFAULT_ALIGN);
                xyu::mem_copy(newdata, p, index);
                xyu::mem_copy(newdata + index + step, p + index, s.size - index + 1);  // 包括 '\0'
                l.data = newdata;
                l.capa = newcapa;
                if (s.kind == Large) xyu::dealloc<char>(p, oldcapa + 1, xyu::K_DEFAULT_ALIGN);
                else s.kind = Large;
                p = newdata;
            }
            s.size += step;
            return p;
        }
        // 将index及之后的位置向前移动step
        void before_move(xyu::size_t index, xyu::size_t step)
        {
            if (XY_UNLIKELY(index > s.size)) return;
            //清空
            if (step >= s.size) return clear();
            //修正参数 (减少无效移动量)
            if (step > index) index = step;
            // Fixed 字符串 复制数据
            if (XY_LIKELY(s.kind == Fixed)) {
                // 变为 Small 字符串
                if (s.size - step < sizeof(s.data)) {
                    const char* p = f.data;
                    s.kind = Small;
                    xyu::mem_copy(s.data, p, index - step);
                    xyu::mem_copy(s.data + index - step, p + index, s.size - index + 1);  // 包括 '\0'
                }
                // 变为 Large 字符串
                else {
                    xyu::size_t newcapa = calc_new_capa(s.size - step);
                    char* newdata = xyu::alloc<char>(newcapa + 1, xyu::K_DEFAULT_ALIGN);
                    xyu::mem_copy(newdata, f.data, index - step);
                    xyu::mem_copy(newdata + index - step, f.data + index, s.size - index + 1);  // 包括 '\0'
                    l.data = newdata;
                    l.capa = newcapa;
                    s.kind = Large;
                }
            }
            // 直接移动
            else xyu::mem_move(data() + index - step, data() + index, s.size - index + 1);  // 包括 '\0'
            s.size -= step;
        }

        struct AU_Ret { bool overcapa; char* p; };
        // 更新自身数据，返回是否超出原大小 及 新数据指针
        AU_Ret after_update(xyu::size_t index, xyu::size_t step)
        {
            xyu::size_t si = index + step;
            if (XY_UNLIKELY(si < index || si > limit())) {
                xylogei(false, "E_Memory_Capacity: now {} add {} over limit {}", capacity(), step, limit());
                throw xyu::E_Memory_Capacity{};
            }
            bool overcapa = si > s.size;
            xyu::size_t sm = xyu::max(si, s.size);
            char* p;
            xyu::size_t oldcapa;
            // Fixed 字符串 复制数据
            if (XY_LIKELY(s.kind == Fixed)) {
                p = l.data;
                // 变为 Small 字符串
                if (sm < sizeof(s.data)) {
                    s.kind = Small;
                    xyu::mem_copy(s.data, p, index);
                    if (!overcapa)
                        xyu::mem_copy(s.data + index + step, p + index + step, s.size - index - step + 1);  // 包括 '\0'
                    s.size = sm;
                    return {overcapa, s.data};
                }
                // 变为 Large 字符串 (后续通过 扩容 部分实现)
                else oldcapa = s.size;
            }
            else p = data(), oldcapa = capacity();
            // 扩容
            if (oldcapa < step + s.size) {
                xyu::size_t newcapa = calc_new_capa(s.size + step);
                char* newdata = xyu::alloc<char>(newcapa + 1, xyu::K_DEFAULT_ALIGN);
                xyu::mem_copy(newdata, p, index);
                if (!overcapa)
                    xyu::mem_copy(newdata + index + step, p + index + step, s.size - index - step + 1);  // 包括 '\0'
                l.data = newdata;
                l.capa = newcapa;
                if (s.kind == Large) xyu::dealloc<char>(p, oldcapa + 1, xyu::K_DEFAULT_ALIGN);
                else s.kind = Large;
                p = newdata;
            }
            s.size = sm;
            return {overcapa, p};
        }
    };

}

#pragma clang diagnostic pop