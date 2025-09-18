#pragma clang diagnostic push
#pragma ide diagnostic ignored "hicpp-exception-baseclass"
#pragma ide diagnostic ignored "modernize-use-nodiscard"
#pragma ide diagnostic ignored "google-explicit-constructor"
#pragma

#include "./strbase.h"

/// 字符串视图
namespace xylu::xystring
{
    /**
     * @brief 一个表示对连续字符序列的非拥有（non-owning）、只读（read-only）视图的类。
     * @details
     *   `StringView` 封装了一个指向字符数据的指针和其长度，但它不管理该数据的生命周期。
     *   它提供了一种轻量级的方式来传递和操作字符串数据，而无需进行昂贵的内存分配和拷贝。
     *
     * @warning
     *   使用者必须确保 `StringView` 所指向的原始字符串数据在其生命周期内保持有效。
     *   使用指向已释放内存的 `StringView` 将导致未定义行为。
     *
     * @note
     *   - `StringView` 自身保证其内部指针永远不会是 `nullptr`。
     *   - 它所表示的字符串序列不保证以 `\0` 结尾。
     */
    struct StringView : StringBase<StringView>
    {
    private:
        const char* p;  // 字符串指针
        xyu::size_t n;  // 字符串长度

    public:
        /**
         * @brief 构造函数
         * @param str 字符串指针 (不能为 nullptr)
         * @param bytes 字符串长度，不包括'\0'
         */
        constexpr StringView(const char* str, xyu::size_t bytes) XY_NOEXCEPT_NDEBUG : p{str}, n{bytes}
        {
#if XY_DEBUG
            if (XY_UNLIKELY(str == nullptr)) {
                if (xyu::t_constant()) p = "";
                else {
                    xylogei(false, "E_Logic_Null_Pointer: null pointer");
                    throw xyu::E_Logic_Null_Pointer{};
                }
            }
#endif
        }
        /**
         * @brief 构造函数
         * @param str 字符串指针 (不能为 nullptr)
         */
        constexpr StringView(const char* str = "") XY_NOEXCEPT_NDEBUG : StringView{str, calc_size(str)} {}

        /**
         * @brief 赋值函数
         * @param str 字符串指针 (不能为 nullptr)
         */
        constexpr StringView& operator=(const char* str) XY_NOEXCEPT_NDEBUG
        {
            n = calc_size(str);
            p = str;
            return *this;
        }

        /// 获取字符串指针 (一定不为 nullptr)
        XY_RETURNS_NONNULL constexpr const char* data() const noexcept { return p; }
        /// 获取字符串长度
        constexpr xyu::size_t size() const noexcept { return n; }
    };

    /// 生成字符串视图
    template <typename ST>
    constexpr StringView StringBase<ST>::view() const noexcept { return {data(), size()}; }

    /// 转换为字符串视图
    template <typename ST>
    constexpr StringBase<ST>::operator StringView() const noexcept { return view(); }

    /// 获取字符串子视图
    template <typename ST>
    constexpr StringView StringBase<ST>::subview(xyu::size_t index, xyu::size_t len) const noexcept
    {
        if (XY_UNLIKELY(index >= size())) return {};
        if (len > size() - index) len = size() - index;
        return {data() + index, len};
    }
}

#pragma clang diagnostic pop