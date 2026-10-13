#pragma once

#include "../_.h"

/**
 * @file     xylu/xytraits
 * @brief    基于 C++17 的底层类型属性及元编程工具库
 *
 * @details  xytraits 模块旨在提供高性能、零开销、功能丰富且易用的编译期类型内省与变换工具
 *           整个模块划分为以下几个部分：
 *           - 基础层 (base & crts): 提供常量封装、SFINAE 工具、const/引用变换等
 *           - 分类层 (kind): 提供对内置类型的类别分类
 *           - 特化层 (arrptr & class & func & numeric): 针对指针数组、类、可调用对象、数值属性的特殊处理
 *           - 模板层 (seq & tplis & tmpl): 编译期数值序列与类型列表、及针对模板本身的打包、解包与实例化检测等
 */

namespace xylu::xytraits {}

namespace xyu { using namespace xylu::xytraits; }
