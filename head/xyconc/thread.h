#pragma clang diagnostic push
#pragma ide diagnostic ignored "cppcoreguidelines-pro-type-member-init"
#pragma ide diagnostic ignored "OCUnusedGlobalDeclarationInspection"
#pragma ide diagnostic ignored "modernize-use-nodiscard"
#pragma once

#include "../../link/tuple"
#include "../../link/atomic"
#include "../../link/time"

/// 原生线程
namespace xylu::xyconc
{
    namespace __
    {
        struct ThreadStatus
        {
            enum Status { Uninit, Failed, Finished, Running, Joined, Detached };
        };
    }

    /**
     * @brief 一个封装了原生线程句柄的、符合RAII原则的线程类。
     * @details
     *   此类旨在提供一个最小化的、面向过程的线程接口，它使用 `void(*)(void*)` 形式的
     *   函数指针作为线程入口。它负责管理线程从创建到销毁的完整生命周期。
     *
     *   一个关键特性是，它的析构函数会自动等待（join）尚未结束的线程，
     *   以防止资源泄露。该类是移动专属的，禁止拷贝。
     */
    class Thread_Native : xyu::class_no_copy_t, __::ThreadStatus
    {
    public:
        /// 线程函数类型
        using Fun_t = void(*)(void*);

    private:
        void* h = nullptr;      // 线程句柄
        Status s = Uninit;      // 线程状态

    public:
        /* 构造析构 */
        /// 默认构造函数
        explicit Thread_Native() noexcept = default;

        /**
         * @brief 构造并立即创建一个新线程来执行指定的函数。
         * @param fun 要在新线程中执行的函数。
         * @param arg 传递给线程函数的参数。
         * @throws E_Logic_Null_Pointer fun 为 nullptr。
         * @exception E_Logic_Invalid_Argument 线程状态是 Running 或 Joined
         * @throws E_Thread_*
         */
        explicit Thread_Native(Fun_t fun, void* arg = nullptr) { create(fun, arg); }

        /**
          * @brief 析构函数。
          * @details 如果线程对象在销毁时仍处于 Running 状态，此析构函数将
          *          自动调用 join() 来阻塞并等待线程执行完毕，以确保资源被正确回收。
          */
        ~Thread_Native() noexcept;

        /* 移动 */
        /// 移动构造函数
        Thread_Native(Thread_Native&& other) noexcept : s{other.s}, h{other.h} { other.h = nullptr; other.s = Uninit; }
        /// 移动赋值运算符
        Thread_Native& operator=(Thread_Native&& other) noexcept { xyu::swap(s, other.s); xyu::swap(h, other.h); return *this; }

        /* 线程属性 */
        /// 获取线程句柄
        void* handle() const noexcept { return h; }
        /// 获取线程状态
        Status status() const noexcept { return s; }

        /* 线程管理 */

        /**
         * @brief 在当前线程对象上创建并启动一个新线程。
         * @param fun 要在新线程中执行的函数。
         * @param arg 传递给线程函数的参数。
         * @throws E_Logic_Null_Pointer fun 为 nullptr。
         * @exception E_Logic_Invalid_Argument 线程状态是 Running 或 Joined
         * @throws E_Thread_*
         */
        void create(Fun_t fun, void* arg = nullptr);

        /**
         * @brief 等待线程结束
         * @exception E_Logic_Invalid_Argument 线程状态不是 Running
         * @exception E_Thread_*
         */
        void join();

        /**
         * @brief 分离线程
         * @exception E_Logic_Invalid_Argument 线程状态不是 Running
         * @exception E_Thread_*
         */
        void detach();

    public:
        /* 线程执行控制 */
        ///返回当前线程句柄
        static void* id() noexcept;
        ///线程让步
        static void yield() noexcept;
    };

}

/// 线程
namespace xylu::xyconc
{
    namespace __
    {
        // 线程状态基类
        struct ThreadStatus_Base : ThreadStatus
        {
            xyu::Atomic<Status> s = Uninit;  // 线程状态
        };
        // 线程状态析构
        struct ThreadStatus_Del : ThreadStatus_Base
        {
            void(*dtor)(void*) = nullptr;  // 返回值析构函数
        };
        // 线程状态
        template <typename T>
        struct ThreadStatus_Total : ThreadStatus_Del
        {
            union {
                alignas(T) xyu::uint8 data[sizeof(T)];  // 线程状态数据
                xyu::ErrorPtr ep;                       // 异常指针
            };
        };
        template <>
        struct ThreadStatus_Total<void> : ThreadStatus_Del
        {
            xyu::ErrorPtr ep;              // 异常指针
        };
    }

    /**
     * @brief 一个一体化的异步任务执行器，融合了线程启动、结果获取与异常传递的功能。
     *
     * @details
     * `xyu::Thread` 提供了一个与 `std::thread` 和 `std::async` 截然不同的设计范式。
     * 它不直接管理底层线程的所有权（所有创建的线程默认都是分离的），而是作为一个
     * “任务句柄”，用于启动一个异步任务，并随后查询其状态、获取其返回值或捕获其抛出的异常。
     *
     * ### 核心设计哲学 (Design Philosophy):
     *
     * 1.  **任务导向，而非线程导向 (Task-Oriented, not Thread-Oriented):**
     *     - `Thread` 对象代表一个被提交到后台执行的“任务”，而非一个需要手动管理的线程资源。
     *     - 所有通过 `create()` 启动的线程都是天生分离 (detached) 的，其生命周期由其任务
     *       本身决定，而不是由 `Thread` 对象的生命周期决定。
     *
     * 2.  **一体化异步模型 (Integrated Asynchronous Model):**
     *     - `Thread` 类集 `std::thread` 的启动功能、`std::future` 的结果获取功能和
     *       `std::promise` 的结果传递功能于一身，提供了高度内聚的API。
     *     - 你只需创建一个 `Thread` 对象，就可以完成任务的提交、等待、结果获取和异常处理
     *       所有流程。
     *
     * 3.  **通过共享状态块实现通信 (Communication via Shared State):**
     *     - `Thread` 对象和其启动的后台任务通过一个内部的、线程安全的共享状态块进行通信。
     *     - 这个状态块负责原子地传递线程状态 (`Status`)、存储返回值（支持左值引用）、
     *       以及通过 `xyu::ErrorPtr` 跨线程传递异常。
     *
     * ### 主要功能 (Key Features):
     *
     * - **构造即运行:** `Thread t(my_func, arg1, ...);`
     * - **泛型任务支持:** 可以接受任何可调用对象（函数指针、lambda、成员指针等）和任意参数。
     * - **自动返回值处理:** 内部自动处理 `void`、值类型和引用类型的返回值。
     * - **无缝异常传递:** 后台任务抛出的任何继承自 `xyu::Error` 的异常都会被捕获，并在
     *   调用 `get()` 的线程中被重新抛出。
     * - **非阻塞状态查询:** `status()` 方法可以随时无锁地查询任务的当前状态。
     * - **阻塞式结果获取:** `get<T>()` 方法会阻塞等待任务完成，并返回其结果。
     *
     * @example
     *   xyu::Thread t([](int x) {
     *       if (x < 0) throw xyu::E_Logic_Invalid_Argument{};
     *       return x * 2;
     *   }, 21);
     *   // ... do other work ...
     *   try {
     *       int result = t.get<int>(); // result will be 42
     *   } catch (const xyu::E_Logic_Invalid_Argument& e) {
     *       // ... handle exception ...
     *   }
     */
    class Thread : xyu::class_no_copy_t, __::ThreadStatus
    {
    private:
        void* sp = nullptr;     // 线程状态

    public:
        /* 构造析构 */

        /// 默认构造
        Thread() noexcept = default;

        /**
         * @brief 启动一个新的异步任务。
         * @tparam Fun  一个可调用对象的类型。
         * @tparam Args 传递给可调用对象的参数类型。
         * @param fun   要异步执行的可调用对象。
         * @param args  要传递给可调用对象的参数。
         * @details
         * 此方法会创建一个新的后台线程来执行 `fun(args...)`。
         * 新创建的线程是分离的 (detached)，`Thread` 对象仅作为该任务的句柄。
         * 如果当前的 `Thread` 对象已经持有一个未完成的任务，旧任务的状态将被安全地销毁。
         */
        template <typename Fun, typename... Args>
        explicit Thread(Fun&& fun, Args&&... args)
        { create(xyu::forward<Fun>(fun), xyu::forward<Args>(args)...); }

        /**
         * @brief 析构函数。
         * @details 如果线程对象在销毁时仍处于 Running 状态，此析构函数将
         *          阻塞并等待线程执行完毕，以确保资源被正确回收。
         */
        ~Thread() noexcept
        {
            if (XY_UNLIKELY(!sp)) return;
            auto& status = *reinterpret_cast<__::ThreadStatus_Del*>(sp);
            for (;;)
            {
                Status s = status.s.load(xyu::N_ATOMIC_ACQUIRE);
                // 等待
                if (s > Finished) { xyu::Duration_s{1}.sleep(); continue; }
                // 析构返回值
                if (s == Finished && status.dtor) status.dtor(sp);
                // 释放内存
                xyu::dealloc(xyu::native_v, sp);
                sp = nullptr;
                break;
            }
        }

        /* 移动 */
        /// 移动构造
        Thread(Thread&& other) noexcept : sp{other.sp} { other.sp = nullptr; }
        /// 移动赋值
        Thread& operator=(Thread&& other) noexcept { xyu::swap(sp, other.sp); return *this; }

        /* 线程属性 */
        /// 获取线程状态
        Status status() const noexcept
        {
            if (XY_UNLIKELY(!sp)) return Uninit;
            return reinterpret_cast<__::ThreadStatus_Base*>(sp)->s.load(xyu::N_ATOMIC_ACQUIRE);
        }

        /* 线程管理 */

        /**
         * @brief 启动一个新的异步任务。
         * @tparam Fun  一个可调用对象的类型。
         * @tparam Args 传递给可调用对象的参数类型。
         * @param fun   要异步执行的可调用对象。
         * @param args  要传递给可调用对象的参数。
         * @exception E_Thread_Invalid_State 旧线程正在运行。
         * @details
         * 此方法会创建一个新的后台线程来执行 `fun(args...)`。
         * 新创建的线程是分离的 (detached)，`Thread` 对象仅作为该任务的句柄。
         */
        template <typename Fun, typename... Args>
        void create(Fun&& fun, Args&&... args)
        {
            static_assert(xyu::t_can_call<Fun, Args...>);
            using ret = xyu::t_get_ret<Fun, Args...>;
            // 状态检查
            void* tmp = sp; // (状态缓存，防止丢失)
            check_status();
            // 分配新状态
            using Sta = __::ThreadStatus_Total<ret>;
            sp = xyu::alloc(xyu::native_v, sizeof(Sta), alignof(Sta));
            ::new (sp) __::ThreadStatus_Base{{}, Uninit};
            constexpr auto dtor = xyu::t_is_void<ret> || xyu::t_can_trivial_destruct<ret> ? nullptr : ret_dtor<ret>;
            reinterpret_cast<__::ThreadStatus_Del*>(sp)->dtor = dtor;
            // 释放旧状态
            if (tmp) xyu::dealloc(xyu::native_v, tmp);
            // 线程创建
            create_help(call_fun<Fun, Args...>, make_arg(xyu::forward<Fun>(fun), xyu::forward<Args>(args)...));
            reinterpret_cast<__::ThreadStatus_Base*>(sp)->s.store(Running, xyu::N_ATOMIC_RELEASE);
        }

        /**
        * @brief 阻塞等待，直到异步任务完成（成功或失败）。
        * @param du 轮询等待的间隔时间。
        * @note 这是一个简单的轮询等待。在任务完成前，它会阻塞当前线程。
        */
        void wait(xyu::Duration_ms du = {1000}) noexcept
        {
            if (XY_UNLIKELY(!sp)) return;
            auto& status = *reinterpret_cast<__::ThreadStatus_Del*>(sp);
            for (;;)
            {
                Status s = status.s.load(xyu::N_ATOMIC_ACQUIRE);
                if (s <= Finished) break;
                du.sleep();
            }
        }

        /**
         * @brief 阻塞等待任务完成，并获取其返回值。
         * @tparam T 期望的返回类型。必须与任务函数的实际返回类型兼容 或为 void。
         * @param du 轮询等待的间隔时间。
         * @return 任务函数的返回值。返回值通过复制获取，可以重复得到。
         * @exception ... 如果后台任务抛出了异常，该异常将被在此处重新抛出。
         * @details
         * 如果任务尚未完成，此函数会阻塞等待。
         * 如果任务成功完成，它会返回结果（支持值和左值引用）。
         * 如果任务因抛出异常而失败，此函数会重新抛出该异常。
         */
        template <typename T>
        T get(xyu::Duration_ms du = {1000})
        {
            // 等待线程结束
            get_help(du);
            auto& status = *reinterpret_cast<__::ThreadStatus_Total<T>*>(sp);
            // 获取返回值 (不进行类型检测，调用者确保与原返回类型相同)
            if constexpr (xyu::t_is_void<T>) return;
            else if constexpr (xyu::t_is_lrefer<T>) return **reinterpret_cast<xyu::t_change_ref2ptr<T>*>(status.data);
            else return *reinterpret_cast<T*>(status.data);
        }

    private:
        // 返回值析构函数
        template <typename T>
        static void ret_dtor(void* sp) noexcept
        {
            auto& status = *reinterpret_cast<__::ThreadStatus_Total<T>*>(sp);
            T& data = *reinterpret_cast<T*>(status.data);
            data.~T();
            status.s.store(Uninit, xyu::N_ATOMIC_RELEASE);
        }

        // 构造传递参数
        template <typename Fun, typename... Args>
        void* make_arg(Fun&& fun, Args&&... args)
        {
            using ret = xyu::t_get_ret<Fun, Args...>;
            using Sta = __::ThreadStatus_Total<ret>;
            auto* p = xyu::alloc<xyu::Tuple<Sta&, xyu::t_decay<Fun>, xyu::Tuple<xyu::t_decay<Args>...>>>(xyu::native_v, 1);
            new (p) xyu::Tuple<Sta&, xyu::t_decay<Fun>, xyu::Tuple<xyu::t_decay<Args>...>>
                    { *reinterpret_cast<Sta*>(sp), xyu::forward<Fun>(fun), { xyu::forward<Args>(args)... } };
            return p;
        }

        // 获取调用函数
        template <typename Fun, typename... Args>
#if (defined(__CYGWIN__) || defined(_WIN32))
        static xyu::uint __stdcall call_fun(void* arg) noexcept
#else
        static void* call_fun(void* arg) noexcept
#endif
        {
            using ret = xyu::t_get_ret<Fun, Args...>;
            using Sta = __::ThreadStatus_Total<ret>;
            // 获取参数
            auto& tp = *reinterpret_cast<xyu::Tuple<Sta&, xyu::t_decay<Fun>, xyu::Tuple<xyu::t_decay<Args>...>>*>(arg);
            Sta& status = tp.template get<0>();
            try {
                // 无返回值
                if constexpr (xyu::t_is_void<ret>) tp.template get<2>().apply(tp.template get<1>());
                // 返回值动态分配 (引用)
                else if constexpr (xyu::t_is_lrefer<ret>)
                    ::new (status.data) xyu::t_change_ref2ptr<ret>(&(tp.template get<2>().apply(tp.template get<1>())));
                // 返回值动态分配 (非引用)
                else ::new (status.data) ret(tp.template get<2>().apply(tp.template get<1>()));
                // 释放参数内存
                xyu::dealloc(xyu::native_v, arg);
                // 状态设置为 Finished
                status.s.store(Finished, xyu::N_ATOMIC_RELEASE);
            }
                // 捕获异常并设置状态为 Failed
            catch (...) {
                xyu::dealloc(xyu::native_v, arg);
                status.ep = xyu::ErrorPtr::current();
                status.s.store(Failed, xyu::N_ATOMIC_RELEASE);
            }
            return 0;
        }

        // 检查旧线程状态
        void check_status();

        // 创建线程
#if (defined(__CYGWIN__) || defined(_WIN32))
        static void create_help(xyu::uint __stdcall (*fun)(void*), void* arg);
#else
        static void create_help(void* (*fun)(void*), void* arg);
#endif

        // 等待返回值
        void get_help(xyu::Duration_ms du);
    };

}

#pragma clang diagnostic pop