# xylu - A Modern C++17 Foundational Library

<p align="center">
  <img src="https://img.shields.io/badge/C%2B%2B-17-blue.svg" alt="C++17">
  <img src="https://img.shields.io/badge/Compiler-GCC%20%7C%20Clang-lightgrey.svg" alt="Compiler">
  <img src="https://img.shields.io/badge/License-MIT-green.svg" alt="License">
  <img src="https://img.shields.io/badge/Status-In%20Development%20(Paused)-orange.svg" alt="Status">
</p>

`xylu` 是一个从零开始构建的、现代化的、以性能和易用性为导向的C++17基础库。它旨在重新思考和实现C++标准库的核心组件，并提供更强大、更灵活、更内聚的API。

*(当前项目因作者参与其他事情而暂停开发)*

---

## 核心特性 (Features)

`xylu` 库由一系列高度解耦又可无缝协作的模块构成：

🚀 **高性能内存管理 (`xymemory`)**:
*   内置**线程局部内存池**，为线程内的频繁小对象分配提供无锁的高性能支持。
*   线程退出时自动回收内存，增强程序健壮性。
*   提供 `xyu::alloc`/`xyu::dealloc` 作为主要的、与内存池绑定的分配接口。
*   重载的全局 `new`/`delete` 基于底层 `malloc`/`free`，保证了与标准库及第三方库的兼容性和跨线程安全性。

✍️ **编译期格式化引擎 (`xystring`)**:
*   受 `std::format` 启发但功能更强大的格式化库。
*   使用 `xyfmt()` 宏实现**编译期**格式字符串解析和类型检查。
*   通过**编译期/运行期混合模式**，为动态内容实现单次、精确的内存分配。
*   支持丰富的正交格式说明符，甚至**支持格式本身由参数动态决定**。
*   通过 `StreamWrapper` 适配器，可向任何兼容的流（`String`, `File`, `vector`...）输出。

🧩 **革命性的迭代器与范围框架 (`xyrange`)**:
*   **“范围是一等公民”**: 容器只需提供 `range()` 方法，即可融入整个生态。
*   **策略模式迭代器**: 将迭代器的每个行为（解引用、自增、比较等）拆分为独立的策略模板，允许在编译期任意“组装”和“改造”迭代器。
*   **生成器 (Generators)**: 支持创建无限或惰性求值的序列（如数列、重复值），并无缝适配C++的范围for循环。

🧠 **高级并发原语 (`xyconc`)**:
*   一个完整的并发工具集，包括：
    *   **`Thread`**: 一体化的异步任务句柄，集成了线程启动、结果获取和异常传递。
    *   **`Mutex`/`Mutex_RW`**: 零动态分配开销（Windows）、强制RAII的互斥锁和读写锁。
    *   **`CondVar`**: 功能强大的条件变量，支持等待条件函数或普通值。
    *   **`Atomic`**: 对编译器内建原子操作的零开销封装，单线程模式下自动退化为普通变量。

🧱 **现代化的核心容器 (`xycontain`)**:
*   已实现 `Array`, `List`, `Tuple`, `HashTable`, `RbTree` 等核心容器。
*   深度集成 `xylu` 内存池和范围库。
*   提供更友好的API，如 `operator[]` 支持负索引。
*   通过泛型接口和元编程，实现了高度一致和强大的 `append`/`insert` 功能。

🛠️ **以及更多...**
*   **通用比较框架 (`xyrange/compare.h`):** 自动发现并使用最高效的比较策略。
*   **详尽的异常体系 (`xycore/error.h`):** 轻量级、语义化的异常类型，与日志系统深度集成。
*   **强大的类型萃取库 (`xytraits`):** 包含一套完整的、用于操作类型和模板的元编程工具。

---

## 设计哲学小剧场

> 😇 **天使:** 为什么要使用C++17？
>
> 😈 **恶魔:** 因此C++17是最早的现代C++版本。
>
> 😇 **天使:** 好吧，可是你为什么要删掉分配器 ，这样不就导致逻辑固化了嘛？
>
> 😈 **恶魔:** 并非如此，你只要把线程全局内存池给替换掉就行了。
>
> 😇 **天使:** 那和修改默认分配器有什么区别？
>
> 😈 **恶魔:** 区别可大了，因为可以少一个模板参数🤓
>
> 😇 **天使:** 😑认真一点！
>
> 😈 **恶魔:** 咳...好。因为不需要繁琐的rebind来rebind去了。而且你需要额外考虑分配器的构造和状态，导致移动语义的实现很繁琐，远不如全局内存池来得简便。
>
> 😇 **天使:** 原来如此。那迭代器呢？你不会把迭代器也给删了吧？
>
> 😈 **恶魔:** 没删，只是给分尸了😈
>
> 😇 **天使:** ？？？😨什么玩意？？？
>
> 😈 **恶魔:** 就是将迭代器的每个函数（运算符）都给拆成独立的模板策略了。只需要几套基础模具，还有提供修改的工具，就能组装出任何想要的迭代器。
>
> 😇 **天使:** 哦，😇那很方便了。
>
> 😈 **恶魔:** 不过话说回来，我的确把容器里的迭代器给删了。
>
> 😇 **天使:** 不是说没删嘛，那你那套迭代器给谁用的？
>
> 😈 **恶魔:** 🤓范围是一等公民，世界是范围的。容器只需要提供一个`range()`，剩下的交给范围自己搞定。
>
> 😇 **天使:** 那比较运算符呢？C++17可没办法使用`<=>`呢。
>
> 😈 **恶魔:** 異議あり！那我提供一套 `equals` 和 `compare`，然后把用户的 `==`, `<` “偷梁换柱”，在编译期把它们换成我的实现不就好了。
>
> 😇 **天使:** ...忘不了你的范围是吗？不过确实挺方便的。
>
> 😈 **恶魔:** 😇范围是一等公民
>
> 😇 **天使:** 不许偷我表情包！
>
> 😈 **恶魔:** 😇好😇~😇的😇~😇
>
> 😇 **天使:** 你！！...算了，所以你到底为什么不升级C++的版本？
>
> 😈 **恶魔:** 因为我高中的学号是17😇
>
> 😇 **天使:** 😑...

---

## 编码规范与约定 (Guiding Principles)

`xylu` 遵循一套严格且实用的编码规范，以保证代码的清晰、一致和高效。

### 核心 (Core)
- **前置要求**:
    1.  需要使用C++17及以上版本编译。
    2.  仅支持 **GCC / Clang**，不保证与MSVC等其他编译器的兼容性。
    3.  编译或使用前请保证 `xylu/xycore/config.h` 中的配置正确。
- **命名空间**:
    1.  不在任何头文件引入标准库文件，避免命名空间污染。
    2.  每个模块有独立的命名空间 `xylu::module_name`。
    3.  通过 `link/` 目录下的头文件统一将常用类型和函数引入到便捷的 `xyu` 命名空间。
    4.  内部实现细节位于 `xylu::module_name::__` 命名空间下。
- **内存分配**:
    1.  **线程内高性能分配**: 使用 `xyu::alloc` / `xyu::dealloc`，它背后是 `thread_local` 的高性能内存池。
    2.  **通用/跨线程分配**: 使用全局 `new` / `delete`，或带 `xyu::native_t` 标签的 `alloc`/`dealloc`。它们基于底层的 `malloc`/`free`，保证线程安全和兼容性。
- **迭代器与范围**:
    1.  **范围优先**: 容器和算法优先处理 `Range` 对象，而非原始迭代器对。
    2.  **容器接口**: 容器只需提供 `range()` 方法即可融入生态。
- **比较运算符**:
    1.  为自定义类型提供 `equals()` / `compare()` (成员或非成员函数)，或重载标准比较运算符，即可被 `xylu` 的通用比较框架自动识别和使用。
    2.  若类型可转换为 `Range`，则默认进行逐元素字典序比较。

### 命名约定 (Naming Conventions)
- **类型**: `PartName_Detail` (PascalCase with underscore)，体现归属关系 (e.g., `RangeIter_Ptr`)。若非归属关系，则直接使用 `PascalCase` (e.g., `StringView`)。
- **基础/特殊类型**: `xxx_t` (e.g., `size_t`, `native_t`)。
- **函数**: `module_xxx` (e.g., `calc_sqrt`, `make_range`)。
- **常量/枚举**: `K_XXX` (常量), `N_XXX` (枚举), `E_XXX` (异常)。
- **宏**: `XY_XXX` (定义宏), `xyxxx` (函数宏)。
- **编译期常量**: `xxx_c` (e.g., `true_c`, `int_c`)。
- **标识常量**: `xxx_v` (e.g., `nothrow_v`, `native_v`)。

---

## 快速上手 (Quick Start)

下面的示例将展示 `xylu` 库的几个核心特性：
- 使用 `xyu::Thread` 进行简单的异步任务处理。
- 捕获 `xylu` 的语义化异常。
- 通过 `xylog` 进行高性能、类型安全的日志记录。
- 使用 `xyfmt` 进行强大的编译期格式化。
- `xyu::Vector` 和范围for循环的无缝集成。

```cpp
#include <xylu/xycore> 
#include <xylu/xycontain>
#include <xylu/xythread> 

// 一个可能失败的异步任务
double risky_division(double numerator, double denominator)
{
    if (denominator == 0) {
        // 抛出 xylu 的语义化异常
		xyloge(false, "denominator can't be 0");
        throw xyu::E_Logic_Invalid_Argument{};
    }
    // 模拟耗时操作
    xyu::Duration_ms{50}.sleep();
    return numerator / denominator;
}

int main()
{
    // 配置全局日志，添加一个输出到 "output.log" 的文件目标
    xyu::flog.add_file("output.log");
    
    // ---- 异步任务与异常处理 ----
    
    xylog(XY_LOG_LEVEL_INFO, "Starting two asynchronous division tasks.");

    // 任务1: 正常执行
    xyu::Thread task1(risky_division, 10.0, 2.0);

    // 任务2: 将会因除零而抛出异常
    xyu::Thread task2(risky_division, 5.0, 0.0);

    try {
        // 等待并获取 task1 的结果
        double result1 = task1.get<double>();
        xylog(XY_LOG_LEVEL_INFO, "Task 1 succeeded with result: {}", result1);
    }
    catch (const xyu::Error& e) {
        // 这里的 catch 块不会被执行
        xylog(XY_LOG_LEVEL_ERROR, "Caught an unexpected exception from Task 1!");
    }

    try {
        // 等待并获取 task2 的结果
        // 这将会重新抛出在后台线程中捕获的异常
        task2.get<double>();
    }
    catch (const xyu::E_Logic_Invalid_Argument& e) {
        // 异常被成功捕获！
        xylog(XY_LOG_LEVEL_WARN, "Caught expected exception from Task 2: Invalid argument.");
    }
    
    // ---- 格式化与容器 ----

    xyu::Vector<xyu::String> items;
    items.append("Apple", "Banana", "Cherry");
    items.insert(0, xyu::make_range_repeat("Orange", 2)); // 在头部插入2个"Orange"

    // 使用 xyfmt 进行强大的编译期格式化
    // - {0:^10}   : 第0个参数，居中对齐，宽度10
    // - {1:?}     : 第1个参数(items)，使用默认的容器格式化
    // - {1:? | }  : 第1个参数(items)，但元素间的分隔符改为空格和竖线
    auto formatted_string = xyfmt(
        "--- Shopping List ---\nFruit of the day: {0:^10}\nItems: {1:? | }\n",
        "Banana", items
    );

    // 使用 xylog 直接输出格式化后的字符串
    xylog(XY_LOG_LEVEL_INFO, "\n{}", formatted_string);

    return 0;
}
```

### 预期输出

**控制台 (`stdout`) 可能的输出:**
```
[2025-08-01 10:30:00.123456] [INFO ] [0x...]: Starting two asynchronous division tasks.
[2025-08-01 10:30:00.174567] [INFO ] [0x...]: Task 1 succeeded with result: 5
[2025-08-01 10:30:00.174890] [WARN ] [0x...]: Caught expected exception from Task 2: Invalid argument.
[2025-08-01 10:30:00.175123] [INFO ] [0x...]: 
--- Shopping List ---
Fruit of the day:   Banana  
Items: [Orange | Orange | Apple | Banana | Cherry]

```

**日志文件 (`output.log`) 中可能包含:**```
... (前面的日志)
[2025-08-01 10:30:00.123888] [ERROR] [0x...]: path/to/your/source.cpp:11:risky_division E_Logic_Invalid_Argument: invalid argument to function
... (后面的日志)
```

## 许可证 (License)


本项目采用 **MIT License**。详情请见 `LICENSE` 文件。

