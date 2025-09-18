#pragma clang diagnostic push
#pragma ide diagnostic ignored "hicpp-exception-baseclass"
#pragma ide diagnostic ignored "google-explicit-constructor"
#pragma ide diagnostic ignored "OCUnusedGlobalDeclarationInspection"
#pragma ide diagnostic ignored "modernize-use-nodiscard"
#pragma ide diagnostic ignored "NotImplementedFunctions"
#pragma once

#include "../../link/attr"
#include "../../link/error"
#include "../../link/config"
#include "../../link/range"

/* 字符串函数 */
namespace xylu::xystring
{
    // 前置声明
    struct StringView;

    namespace __
    {
        template <typename U, typename = decltype(xyu::t_val<U>().size())>
        constexpr static xyu::constant_c<int, 1> test_size(char);
        template <typename U, typename = decltype(xyu::t_val<U>().length())>
        constexpr static xyu::constant_c<int, 2> test_size(int);
        template <typename U, typename = decltype(xyu::t_val<U>().count())>
        constexpr static xyu::constant_c<int, 3> test_size(double);
        template <typename U>
        constexpr static xyu::constant_c<int,-1> test_size(...);
        template <typename U>
        constexpr static int get_size_type = decltype(test_size<U>('0'))::value;
    }

    /**
     * @brief 为字符串类型提供一套通用的只读成员函数（CRTP 基类）。
     * @details
     *   `StringBase` 采用了奇异的递归模板模式 (CRTP)，它本身不存储任何数据，
     *   而是将其所有操作委托给继承它的派生类 `ST`。任何继承自 `StringBase˂ST˃`
     *   的类，都将自动获得此处定义的所有只读字符串操作接口。
     *
     * @tparam ST 派生字符串类型。
     * @note
     *   为了使 `StringBase` 能正常工作，派生类 `ST` 必须提供以下两个成员函数：
     *   - `const char* data() const noexcept`: 返回指向字符串数据的指针。
     *   - `xyu::size_t size() const noexcept`: 返回字符串的长度。
     *     (或者提供 `length()` / `count()` 作为替代)。
     */
    template <typename ST>
    struct StringBase
    {
        /* 数据 */

        // 获取字符串 (不会返回 nullptr)
        XY_RETURNS_NONNULL
        constexpr const char* data() const noexcept { return drived().data(); }
        
        /// 判断是否非空
        constexpr explicit operator bool() const noexcept { return size() != 0; }
        /// 判断是否为空
        constexpr bool empty() const noexcept { return size() == 0; }

        /// 获取字符串长度
        constexpr xyu::size_t size() const noexcept 
        {
            constexpr int type = __::get_size_type<ST>;
            static_assert(type != -1, "ST must support size() or length() or count() member function");
            if constexpr (type == 1) return drived().size();
            else if constexpr (type == 2) return drived().length();
            else return drived().count();
        }
        /// 获取字符串长度
        constexpr xyu::size_t length() const noexcept { return size(); }
        /// 获取字符数量
        constexpr xyu::size_t count() const noexcept { return size(); }

        /// 获取字符
        constexpr char get(xyu::size_t index) const XY_NOEXCEPT_NDEBUG
        {
#if XY_DEBUG
            return at(index);
#else
            return data()[index];
#endif
        }
        /// 获取字符 [范围检测]
        constexpr char at(xyu::size_t index) const
        {
            if (XY_UNLIKELY(index >= size())) {
                if (xyu::t_constant()) return '\0';
                else {
                    xylogei(false, "E_Logic_Out_Of_Range: index {} out of range [0, {})", index, size());
                    throw xyu::E_Logic_Out_Of_Range{};
                }
            }
            return data()[index];
        }

        /**
         * @brief 索引访问 (不可修改)
         * @note 支持负索引，即 [-size,-1] 映射到 [0,size-1]
         * @note (-∞,-size) 与 [size,+∞) 则 映射到 两端 即 0 和 size-1
         */
        constexpr char operator[](xyu::diff_t index) const XY_NOEXCEPT_NDEBUG
        {
            auto s = size();
            auto i = static_cast<xyu::size_t>(index);
            if (index >= 0) return get(i >= s ? s - 1 : i);
            else return get(XY_UNLIKELY((i += s) >= s) ? 0 : i);
        }

        /// 生成只读范围
        constexpr auto range() const noexcept { return xyu::make_range(size(), data(), data() + size()); }

        /* 视图 */

        /// 隐式转换为字符串视图
        constexpr operator StringView() const noexcept;

        /// 生成字符串视图
        constexpr StringView view() const noexcept;

        /**
         * @brief 获取字符串子视图
         * @param index 子视图起始位置
         * @param len 子视图长度
         * @note 不保证以 '\0' 结尾
         */
        constexpr StringView subview(xyu::size_t index, xyu::size_t len = -1) const noexcept;

        /* 格式化 */

        /// 生成格式化后的字符串 (字符串常量更推荐用 xyfmt)
        template <typename... Args>
        auto format(const Args&... args) const;

        /* 比较 */

        /// 判断两个字符串是否相等
        constexpr bool equals(const StringView& other) const noexcept;
        /// 比较两个字符串
        constexpr int compare(const StringView& other) const noexcept;

        /* 查找 */

        /**
         * @brief 从前到后，在 [start,end) 范围内 查找字符位置
         * @note 存在时返回起始位置索引，不存在时返回 -1 (无符号数)
         */
        constexpr xyu::size_t find(char c, xyu::size_t start = 0, xyu::size_t end = -1) const noexcept;
        /**
         * @brief 从前到后，在 [start,end) 范围内 查找字符串位置
         * @note 存在时返回起始位置索引，不存在时返回 -1 (无符号数)
         */
        constexpr xyu::size_t find(const StringView& pattern, xyu::size_t start = 0, xyu::size_t end = -1) const noexcept;

        /**
         * @brief 从后到前，在 [start,end) 范围内 查找字符串位置
         * @note 存在时返回起始位置索引，不存在时返回 -1 (无符号数)
         */
        constexpr xyu::size_t rfind(char c, xyu::size_t start = 0, xyu::size_t end = -1) const noexcept;
        /**
         * @brief 从后到前，在 [start,end) 范围内 查找字符串位置
         * @note 存在时返回起始位置索引，不存在时返回 -1 (无符号数)
         */
        constexpr xyu::size_t rfind(const StringView& pattern, xyu::size_t start = 0, xyu::size_t end = -1) const noexcept;

        /* 开头结尾匹配 */

        /// 判断是否以 字符c 开头
        constexpr bool starts_with(char c) const noexcept;
        /// 判断是否以 字符串 开头
        constexpr bool starts_with(const StringView& prefix) const noexcept;

        /// 判断是否以 字符c 结尾
        constexpr bool ends_with(char c) const noexcept;
        /// 判断是否以 字符串 结尾
        constexpr bool ends_with(const StringView& suffix) const noexcept;

        /* 开头结尾去除 */

        /// 去除开头的空字符
        constexpr StringView lstrip() const noexcept;
        /// 去除结尾的空字符
        constexpr StringView rstrip() const noexcept;
        /// 去除开头结尾的空字符
        constexpr StringView strip() const noexcept;

        /// 去除开头的指定字符
        constexpr StringView lstrip(char c) const noexcept;
        /// 去除结尾的指定字符
        constexpr StringView rstrip(char c) const noexcept;
        /// 去除开头结尾的指定字符
        constexpr StringView strip(char c) const noexcept;

    protected:
        // 计算字符串长度 (禁止传递 nullptr)
        static constexpr xyu::size_t calc_size(const char* str) XY_NOEXCEPT_NDEBUG
        {
#if XY_DEBUG
            if (XY_UNLIKELY(str == nullptr)) {
                if (xyu::t_constant()) return 0;
                else {
                    xylogei(false, "E_Logic_Null_Pointer: null pointer");
                    throw xyu::E_Logic_Null_Pointer{};
                }
            }
#endif
            return __builtin_strlen(str);
        }

    private:
        // 获取派生类
        constexpr const ST& drived() const noexcept { return static_cast<const ST&>(*this); }
    };

}

#pragma clang diagnostic pop