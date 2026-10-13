# xylu (C++17 High-Performance Base Library)

[![C++ Standard](https://img.shields.io/badge/C%2B%2B-17%20%7C%2020-blue.svg)](https://en.cppreference.com/w/cpp/compiler_support)
[![Compiler](https://img.shields.io/badge/Compiler-GCC%20%7C%20Clang-orange.svg)]()
[![License](https://img.shields.io/badge/License-MIT-green.svg)](LICENSE)

**xylu** 是一款专为极致性能与现代 C++ 哲学打造的底层基础组件库。它完全脱离传统的 C++ 标准库（STL）实现体系，从内存分配、类型推导、SIMD 硬件加速，到高并发容器与网络 IO，提供了一整套零开销、无命名空间污染的硬核解决方案。

## ✨ 核心设计哲学

- 🚫 **无标准库污染**：头文件中**绝对不引入**任何 C++ 标准库文件，所有基础设施（包括 `initializer_list`、`type_info` 等）均自行极简实现，彻底告别庞大头文件带来的编译负担与命名空间冲突。
- ⚡ **极致性能驱动**：全面引入基于硬件特性的 SIMD 优化 (AVX2/SSE/NEON)，涵盖字符串搜索、哈希探测、内存比对等场景；内置高性能的 **线程局部内存池 (Thread-Local Memory Pool)**，接管所有的容器动态分配。
- 🛡 **编译期安全与计算**：深度利用 C++17 特性和 SFINAE，提供包含严格编译期类型/长度检查的格式化引擎 (`xyfmt`)，在编译期拦截格式错误；提供丰富的元编程算法库 (`xytraits`)。
- 🧩 **高度抽象的策略模式**：抛弃繁冗的传统迭代器设计，采用基于策略（Policy-based）的迭代器与生成器架构 (`xyrange`)，通过任意部件（Storage, Dereference, Increment...）组合构建具有单向/双向/反转能力的迭代器。

## 📦 模块概览

所有组件均在 `xylu::` 命名空间下按模块划分，并可通过便捷的别名空间 `xyu::` 直接访问。

| 模块名称 | 简介与包含内容 |
| :--- | :--- |
| **`xycore`** | **核心基础**：编译期与运行期配置宏 (`XY_DEBUG` 等)、全手写极速 SIMD 封装、自定义异常体系 (`ErrorPtr`, `E_Logic`)、高性能多线程异步/彩色日志系统 (`xylog`)。 |
| **`xytraits`** | **元编程工具**：涵盖类型推导、复合类型鉴定、深层 `const` 与引用剥离、同构/异构编译期常量序列 (`isequence_c` / `typelist_c`) 以及模板的打包解包。 |
| **`xymemory`** | **内存管理**：基于双策略（大小块分离）的高性能内存池 `MemPool_BC`。全局统一接管 `alloc`/`dealloc`。 |
| **`xymath`** | **数学与随机**：内置指令位运算 (`bit_xxx`)、数学函数 (`calc_xxx`)、大数运算 (`BigNum`)、硬件加密级与高速伪随机生成器 (`RandPCG64`, 采用 Ziggurat 的 `RandNorm`)，以及混合哈希算法。 |
| **`xystring`** | **字符串与格式化**：自定义 SSO 优化字符串 (`String`)、定长栈字符串 (`FixedString`)、视图 (`StringView`)，以及支持动态布局与自定义类型的编译期格式化引擎 `xyfmt`。 |
| **`xycoll`** | **容器集合**：基于 SIMD 的开放寻址哈希表 (`HashMap`/`HashSet`)，基于 B-Tree 的有序字典 (`BTreeMap`/`BTreeSet`)，数组、链表、循环队列，以及类型安全的函数包装器 (`Function`, `Bind`)。 |
| **`xyrange`** | **范围与生成器**：摒弃单独的迭代器传递，统一使用 `Range`。提供等差/重复等 `Generator`，支持使用 `\|` 管道符进行范围映射、过滤、拼接 (`slice`, `filter`, `concat`, `split`)。 |
| **`xyconc`** | **并发原语**：脱离 `<thread>` 的跨平台原生并发组件。包含原子操作 (`Atomic`)、轻量级线程 (`Thread`)、软递归与读写锁 (`Mutex`, `RwMutex`)，以及异步任务执行 (`Async`)。 |
| **`xysystem`** | **系统操作**：文件流操作 (`File`)、跨平台文件路径管理 (`Path`)，以及零开销的绝对时间戳与日历换算 (`Duration`, `Calendar`, `Clock`)。 |
| **`xyconn`** | **网络通信**：高度封装的 `Socket`（支持 IPv4/IPv6, TCP/UDP），并为高并发服务器提供基于 Windows IOCP 及 Linux `io_uring` 的多路复用组件 `Smulio`。 |

## 🚀 快速展示 (Showcase)

### 1. 编译期类型安全的格式化与打印

```cpp
#include "xylu/xystring/fmt.h"
#include "xylu/xycore/log.h"

int main() {
    // 编译期校验格式字符串，预分配精准内存，零动态开销
    xyu::String s = xyfmt("Hello, {}! Hex: {:#X}, Float: {:.2f}", "xylu", 255, 3.14159);
    
    // 高性能异步日志记录 (支持 File:Line, Thread ID, 彩色等级输出)
    xylogi("Logger Initialized. String content: {}", s);
    
    // 文件格式化流输出
    xyu::File file("output.txt", xyu::File::RW);
    xyfmtto(file, "User ID: {:04d}", 42); 
    
    return 0;
}
```

### 2. 现代化的范围操作 (Range Operations)

```cpp
#include "xylu/xyrange/ops.h"
#include "xylu/xycoll/nearlist.h"
#include "xylu/xystring/strview.h"

void process_data() {
    xyu::NearList<int> list;
    list.append(1, 2, 3, 4, 5, 6, 7);

    // 支持管道符流式操作
    auto sub = list | xyu::RangeOps::slice(1, -1) 
                    | xyu::RangeOps::filter([](int x){ return x % 2 == 0; });
                    
    // 字符串拆分与拼接
    xyu::StringView text = "c++,rust,go";
    auto splitted = text | xyu::RangeOps::split(',');
}
```

### 3. 基于 io_uring / IOCP 的极限异步网络 IO

```cpp
#include "xylu/xyconn/smulio.h"

void start_server() {
    xyu::Socket server(xyu::Socket::AF_ipv4, xyu::Socket::Sk_stream);
    server.bind(xyu::Socket::AddressIPv4("0.0.0.0:8080"));
    server.listen();

    // 创建多路 IO 抽象 (Linux 自动走 io_uring，Win 走 IOCP)
    xyu::Smulio<> smulio(1024);
    smulio.bind(server);
    
    // 投递 Accept 异步任务
    smulio.accept(server, [](xyu::SmulioCtx& ctx) {
        ctx.buf = xyu::alloc<xyu::Socket::AddressIPv4>(1); // 从内存池高速分配
        ctx.len = sizeof(xyu::Socket::AddressIPv4);
    });

    while (true) {
        auto& ctx = smulio.get(-1); // 阻塞等待 IO 完成
        if (ctx.op == xyu::SmulioCtx::Op_accept && ctx.state.success()) {
            xylogi("New Connection from {}", *(xyu::Socket::AddressIPv4*)ctx.buf);
            // 接管新创建的连接 socket
            xyu::Socket client = xyu::move(ctx.newsock());
        }
        smulio.put(ctx); // 归还 Context
    }
}
```

## 🛠 编译与环境要求

本项目极其注重底层的运行性能，因此抛弃了无法提供稳定跨平台性能特性的标准库及编译器。

- **编译器**：必须使用 **GCC** 或 **Clang**。完全**不支持 MSVC** 及 Cygwin 等模拟环境。
- **语言标准**：严格要求 **C++17** 及以上。
- **系统支持**：原生支持 Linux 与 Windows 平台（网络库在 Win 下需链接 `ws2_32`，Linux 下需链接 `uring`）。

### 引入你的项目

目前推荐通过源文件直接集成，或使用 CMake 将其作为静态库/子目录引入。

```cmake
# CMakeLists.txt 示例
add_subdirectory(xylu)

add_executable(my_app main.cpp)
target_link_libraries(my_app PRIVATE xylu)
```

## ⚠️ 开发者须知

1. **宏定义配置**：在包含任何头文件之前，请确保 `xycore/config.h` 中的环境配置（如 `XY_DEBUG`, `XY_THREAD`, 指令集宏等）准确无误。
2. **数据类型**：库中绝大部分情况下使用 `xyu::uint32`, `xyu::int64`, `xyu::size_t` 等固定宽度类型替代传统的 `int`/`long`，以保证跨平台二进制安全。
3. **全局变量**：库内极少使用全局变量（全部通过静态单例或 TLS 解决）。`alloc`/`dealloc` 关联的是线程局部分配器，**切忌跨线程回收内存**（除非显式指定使用 Native 分配或大内存块）。

## 📄 许可协议

本项目采用 [MIT License](LICENSE) 开源协议。版权所有 (c) xingyuan。