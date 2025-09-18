#pragma clang diagnostic push
#pragma ide diagnostic ignored "OCUnusedMacroInspection"
#pragma once

// 标记函数不会返回 (已支持)
// #define XY_NORETURN [[noreturn]]

// 标记已弃用的函数、类、变量、枚举或命名空间等 (已支持)
// #define XY_DEPRECATED [[deprecated]]

// 强制检查返回值，避免忽略 (已支持)
// #define XY_NODISCARD [[nodiscard]]

// 抑制未使用变量/函数的警告 (已支持)
// #define XY_MAYBE_UNUSED [[maybe_unused]]

// 标记标记 switch-case 故意不写 break 的情况 (已支持)
// #define XY_FALLTHROUGH [[fallthrough]]

// 分支预测，指示发生概率较高
#define XY_LIKELY( condition ) __builtin_expect( (condition), 1 )
// 分支预测，指示发生概率较低
#define XY_UNLIKELY( condition ) __builtin_expect( (condition), 0 )

// 指示某条语句是不可到达的
#define XY_UNREACHABLE __builtin_unreachable();

// 强制内联函数，忽略编译器的优化决策 (除非无法内联，如递归函数)
#define XY_ALWAYS_INLINE __attribute__((always_inline))
// 强制不内联函数，即使编译器认为合适
#define XY_NOINLINE __attribute__((noinline))

// 禁用别名分析
#define XY_MAY_ALIAS __attribute__((may_alias))

// 标记为冷函数，减少对指令缓存的污染
#define XY_COLD __attribute__((cold))
// 标记为热函数，提高指令缓存命中率
#define XY_HOT __attribute__((hot))

// 如果函数被调用，让编译器产生一个警告
#define XY_WARNING( message ) __attribute__((warning( message )))
// 如果函数被调用，让编译器产生一个错误
#define XY_ERROR( message ) __attribute__((error( message )))

// 如果变量、函数或类等未使用，不发出警告
#define XY_UNUSED __attribute__((unused))
// 即使变量或函数看起来未使用，也避免连接器删除
#define XY_USED __attribute__((used))

// 指定函数的某些指针参数不能为 NULL (从第1个参数开始计数)
#define XY_NONNULL(...) __attribute__((nonnull(__VA_ARGS__)))
// 标记函数返回的指针永远不会为 NULL
#define XY_RETURNS_NONNULL __attribute__((returns_nonnull))

// 标记函数分配内存
#define XY_MALLOC __attribute__((malloc))
// 标记函数的返回指针，且参数中第 position1, ... 个参数表示分配的内存大小 (从第1个参数开始计数)
#define XY_ALLOC_SIZE(...) __attribute__((alloc_size(__VA_ARGS__)))
// 标记函数的返回指针，且至少对齐到第 position 个参数的要求 (从第1个参数开始计数)
#define XY_ALLOC_ALIGN( position ) __attribute__((alloc_align( position )))

// 将符号定义为弱符号
#define XY_WEAK __attribute__((weak))

// 表明函数只会修改自身栈帧，且不调用其他非 leaf 函数
#define XY_LEAF __attribute__((leaf))

// 表明函数的行为只依赖传递参数或全局变量，相同的参数将返回相同结果，除返回值外无副作用
#define XY_PURE __attribute__((pure))
// 表明函数的行为只依赖传递参数，且不修改全局变量，相同的参数将返回相同结果，无副作用
#define XY_CONST __attribute__((const))

// 用于 struct 或 union，不进行对齐填充
#define XY_PACKED __attribute__((packed))

#pragma clang diagnostic pop