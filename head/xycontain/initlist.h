#pragma clang diagnostic push
#pragma ide diagnostic ignored "OCUnusedGlobalDeclarationInspection"
#pragma ide diagnostic ignored "modernize-use-nodiscard"
#pragma ide diagnostic ignored "cert-dcl58-cpp"
#pragma once

#include "../../link/type"

#ifndef _INITIALIZER_LIST
#define _INITIALIZER_LIST

namespace std
{
    /**
     * @brief 初始化列表
     * @tparam T 元素类型
     * @note 固定使用 size() begin() end()
     */
    template <typename T>
    struct initializer_list
    {
    private:
        T* arr{};
        xyu::size_t s{};
    public:
        // 默认构造函数
        constexpr initializer_list() noexcept = default;
        // 获取元素数量
        constexpr xyu::size_t size() const noexcept { return s; }
        // 迭代器 - 头节点
        constexpr const T* begin() const noexcept { return arr; }
        // 迭代器 - 尾节点
        constexpr const T* end() const noexcept { return arr + s; }
    private:
        // 内置构造函数
        constexpr initializer_list(const T* p, xyu::size_t len) : arr{p}, s{len} {}
    };
}

#endif

#pragma clang diagnostic pop