#pragma once

/**
 * @file    xystd/structbind.h
 * @brief   结构化绑定，为获得语法支持
 * @date    2026-04-22
 */

#include "../xycore/type.h"

#ifndef _GLIBCXX_UTILITY_H

namespace std
{
    // 元素容量
    template <typename T> struct tuple_size;
    // 元素类型
    template <xyu::size_t index, typename T> struct tuple_element;

    // 元素容量 const 转发
    template <typename T>
    struct tuple_size<const T> : tuple_size<T> {};
    // 元素类型 const 转发
    template <xyu::size_t index, typename T>
    struct tuple_element<index, const T> { using type = const typename tuple_element<index, T>::type; };
}

#endif
