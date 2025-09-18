#pragma clang diagnostic push
#pragma ide diagnostic ignored "OCUnusedGlobalDeclarationInspection"
#pragma ide diagnostic ignored "OCUnusedTypeAliasInspection"
#pragma ide diagnostic ignored "OCUnusedStructInspection"
#pragma once

/// 固定长度类型
// (代替 int,long等 不确定长度的整型使用)
namespace xylu::xycore
{
    // 8位整型
    using int8 = char;
    // 无符号8位整型
    using uint8 = __UINT8_TYPE__;
    // 16位整型
    using int16 = __INT16_TYPE__;
    // 无符号16位整型
    using uint16 = __UINT16_TYPE__;
    // 32位整型
    using int32 = __INT32_TYPE__;
    // 无符号32位整型
    using uint32 = __UINT32_TYPE__;
    // 64位整型
    using int64 = __INT64_TYPE__;
    // 无符号64位整型
    using uint64 = __UINT64_TYPE__;
    // 128位整型
    using int128 = __int128_t;
    // 无符号128位整型
    using uint128 = __uint128_t;
    // 默认unsigned int类型
    using uint = unsigned int;
}

/// 特殊类型
namespace xylu::xycore
{
    // 大小类型 (无符号整型)
    using size_t = __SIZE_TYPE__;
    // 偏移类型 (有符号整型)
    using diff_t = __PTRDIFF_TYPE__;

    // 空指针类型
    using nullptr_t = decltype(nullptr);
    // 无异常类型
    constexpr struct nothrow_t {} nothrow_v;
    // 原类型
    constexpr struct native_t {} native_v;
    // 特殊类型
    constexpr struct special_t {} special_v;

    // 成功类型
    struct success_t {};
    // 失败类型
    struct fail_t {};
}

/// 类型限制类型
namespace xylu::xycore
{
    // 继承禁止构造
    struct class_no_construct_t
    {
        class_no_construct_t() = delete;
        ~class_no_construct_t() = delete;
        class_no_construct_t(const class_no_construct_t&) = delete;
        class_no_construct_t& operator=(const class_no_construct_t&) = delete;
        class_no_construct_t(class_no_construct_t&&) = delete;
        class_no_construct_t& operator=(class_no_construct_t&&) = delete;
    };
    // 继承禁止复制
    struct class_no_copy_t
    {
        constexpr class_no_copy_t() noexcept = default;
        class_no_copy_t(const class_no_copy_t&) = delete;
        class_no_copy_t& operator=(const class_no_copy_t&) = delete;
        constexpr class_no_copy_t(class_no_copy_t&&) = default;
        constexpr class_no_copy_t& operator=(class_no_copy_t&&) = default;
    };
    // 继承禁止移动
    struct class_no_move_t
    {
        constexpr class_no_move_t() noexcept = default;
        constexpr class_no_move_t(const class_no_move_t&) = default;
        constexpr class_no_move_t& operator=(const class_no_move_t&) = default;
        class_no_move_t(class_no_move_t&&) = delete;
        class_no_move_t& operator=(class_no_move_t&&) = delete;
    };
    // 继承禁止复制及移动
    struct class_no_copy_move_t
    {
        constexpr class_no_copy_move_t() noexcept = default;
        class_no_copy_move_t(const class_no_copy_move_t&) = delete;
        class_no_copy_move_t& operator=(const class_no_copy_move_t&) = delete;
        class_no_copy_move_t(class_no_copy_move_t&&) = delete;
        class_no_copy_move_t& operator=(class_no_copy_move_t&&) = delete;
    };
}

#pragma clang diagnostic pop