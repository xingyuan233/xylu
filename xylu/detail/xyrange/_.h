#pragma once

#include "../_.h"

/**
 * @file     xylu/xyrange
 * @brief    基于 C++17 的高性能、策略式范围与迭代器工具库
 *
 * @details  xyrange 模块旨在提供一套抽象层次极高且性能零损耗的遍历工具。
 *           不同于传统 STL 迭代器的硬编码实现，本模块采用“策略模式”将迭代器的行为解耦。
 *           整个模块划分为以下核心部分：
 *           - **迭代器 (Iter)**: 提供通用的 `Iterator` 模板，通过组合不同的存储、导航、解引用策略构建迭代器。
 *           - **生成器 (Gen)**: 支持动态计算序列（等差数列、重复序列等），支持哨兵（Sentinel）机制。
 *           - **范围 (Range)**: 对序列起点与终点的统一封装，支持容器适配、数组转换及指针序列探测。
 */

namespace xylu::xyrange {}

namespace xyu { using namespace xylu::xyrange; }
