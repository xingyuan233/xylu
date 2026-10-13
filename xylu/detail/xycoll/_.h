#pragma once

#include "../_.h"

/**
 * @file     xylu/xycoll
 * @brief    容器
 *
 * @details
 *   为了安全以及特殊类型的划分，任何容器的存储上限都不会超过 t_number<size_t>::max / 2。
 *   两个 小于等于 t_number<size_t>::max / 2 的值可以安全相加。
 */

namespace xylu::xycoll {}

namespace xyu { using namespace xylu::xycoll; }
