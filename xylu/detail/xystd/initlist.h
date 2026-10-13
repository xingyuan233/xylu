#pragma once

/**
 * @file    xystd/initlist.h
 * @brief   初始化列表，为获得语法支持
 * @date    2025-12-24
 */

#include "../xycore/type.h"

#ifndef _INITIALIZER_LIST
#define _INITIALIZER_LIST

namespace std
{
    /**
     * @brief 初始化列表
     * @tparam T 元素类型
     * @details 固定使用 size、begin、end
     * @details 生命周期极短，不拥有数据无法延长其生命周期
     */
    template <typename T>
    struct initializer_list
    {
    private:
        const T* arr{};
        xyu::size_t s{};
    public:
        // 默认构造函数
        constexpr initializer_list() noexcept = default;
        // 元素数量
        constexpr xyu::size_t size() const noexcept { return s; }
        // begin 迭代器
        constexpr const T* begin() const noexcept { return arr; }
        // end 迭代器
        constexpr const T* end() const noexcept { return arr + s; }
    private:
        // 内置构造函数
        constexpr initializer_list(const T* p, xyu::size_t len) : arr{p}, s{len} {}
    };
}

#endif // _INITIALIZER_LIST
