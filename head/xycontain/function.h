#pragma clang diagnostic push
#pragma ide diagnostic ignored "hicpp-exception-baseclass"
#pragma ide diagnostic ignored "modernize-use-nodiscard"
#pragma ide diagnostic ignored "google-explicit-constructor"
#pragma ide diagnostic ignored "bugprone-forwarding-reference-overload"
#pragma ide diagnostic ignored "cppcoreguidelines-pro-type-member-init"
#pragma ide diagnostic ignored "OCUnusedGlobalDeclarationInspection"
#pragma ide diagnostic ignored "Simplify"
#pragma once

#include "../../link/log"

namespace xylu::xycontain
{
    namespace __
    {
        /// 类函数数据存储
        struct XY_MAY_ALIAS FuncData
        {
            class AnyClass;
            union Data
            {
                void* obj;                  //对象指针
                const void* cobj;           //const对象指针
                void(*fp)();                //函数指针
                void(AnyClass::*mp)();      //成员指针
            };
            alignas(alignof(Data)) xyu::uint8 buf[sizeof(Data)];   //数据缓冲区

            // 获取数据
            template <typename T>
            T& get() const noexcept
            {
                auto* p = const_cast<xyu::uint8*>(buf);
                return *reinterpret_cast<T*>(p);
            }
        };

        /// 类函数管理器基类
        template <typename Ret, typename Func>
        struct FuncManagerBase
        {
            // Func 是否直接存储在缓冲区中
            static constexpr bool local = sizeof(Func) <= sizeof(FuncData) && alignof(Func) <= alignof(FuncData);

            // 函数调用
            template <typename... Args>
            static Ret call(const FuncData& data, Args... args) noexcept(xyu::t_can_nothrow_call_to<Ret, Func, Args...>)
            {
                if constexpr (local) return data.get<Func>()(xyu::forward<Args>(args)...);
                else return (*data.get<Func*>())(xyu::forward<Args>(args)...);
            }
        };
        // 成员指针特化
        template <typename Ret, typename Ord, typename Class>
        struct FuncManagerBase<Ret, Ord Class::*>
        {
            static constexpr bool local = true;

            // 函数调用
            template <typename CP, typename... Args>
            static Ret call(const FuncData& data, CP arg, Args... args) noexcept(!XY_DEBUG && xyu::t_can_nothrow_call_to<Ret, Ord Class::*, CP, Args...>)
            {
                using Func = Ord Class::*;
                constexpr bool obj = xyu::t_is_base_of<Class, xyu::t_remove_refer<CP>>;
#if XY_DEBUG
                if constexpr (!obj) {
                    if (XY_UNLIKELY(arg == nullptr)) {
                        xyloge(false, "E_Logic_Null_Pointer: null pointer passed to member function pointer");
                        throw xyu::E_Logic_Null_Pointer{};
                    }
                }
#endif
                // 成员变量指针
                if constexpr (xyu::t_is_memvptr<Func>)
                {
                    static_assert(sizeof...(Args) == 0);
                    if constexpr (obj) return xyu::forward<CP>(arg).*data.get<Func>();
                    else return (*xyu::forward<CP>(arg)).*data.get<Func>();
                }
                // 成员函数指针
                else if constexpr (obj) return (xyu::forward<CP>(arg).*data.get<Func>())(xyu::forward<Args>(args)...);
                else return ((*xyu::forward<CP>(arg)).*data.get<Func>())(xyu::forward<Args>(args)...);
            }
        };

        // 类函数操作指令
        enum class FuncOrder
        {
            Destroy,        // 析构
            Copy,           // 复制
        };

        /// 类函数管理器
        template <typename Ret, typename Func>
        struct FuncManager : FuncManagerBase<Ret, Func>
        {
            using FuncManagerBase<Ret, Func>::local;

            // 创建 (复制或移动构造，使用列表初始化不会产生歧义)
            template <typename FuncT>
            static void create(FuncData& data, FuncT&& func) noexcept(local && xyu::t_can_nothrow_listinit<Func, FuncT>)
            {
                if constexpr (local) ::new (data.buf) Func{xyu::forward<FuncT>(func)};
                else {
                    Func* p = xyu::alloc<Func>(1);
                    ::new (p) Func{xyu::forward<FuncT>(func)};
                    data.get<Func*>() = p;
                }
            }

            // 析构
            static void destroy(FuncData& data) noexcept
            {
                if constexpr (local) data.get<Func>().~Func();
                else {
                    data.get<Func*>()->~Func();
                    xyu::dealloc<Func>(data.get<Func*>(), 1);
                }
            }

            // 操作
            static void manage(FuncData& target, const FuncData& source, FuncOrder order)
            {
                switch (order)
                {
                    //析构
                    case FuncOrder::Destroy:{
                        destroy(target);
                        break;
                    }
                    //复制
                    case FuncOrder::Copy:{
                        if constexpr (local) create(target, source.get<Func>());
                        else create(target, *source.get<Func*>());
                        break;
                    }
                }
            }
        };
    }

    /**
     * @brief 一个通用的、多态的函数包装器，其行为类似于 std::function。
     *
     * @tparam Ret 函数的返回类型。
     * @tparam Args 函数的参数类型列表。
     *
     * @details
     * `xyu::Function` 能够存储、复制和调用任何可调用目标——包括普通函数指针、
     * 成员函数指针、成员变量指针、lambda表达式以及自定义的仿函数对象。
     *
     * ### 核心特性 (Key Features):
     *
     * 1.  **类型擦除 (Type Erasure):**
     *     - 通过类型擦除技术，`Function˂Ret(Args...)˃` 可以持有任何签名兼容的
     *       可调用对象，而调用者无需关心其具体类型。
     *
     * 2.  **小对象优化 (Small Object Optimization - SSO):**
     *     - 对于体积较小的可调用对象（如无捕获的lambda、函数指针），`Function` 会将其
     *       直接存储在对象内部的缓冲区中，避免了任何动态内存分配，提供了极致的性能。
     *     - 只有当可调用对象体积较大时，才会通过 `xylu` 的内存池在堆上分配内存。
     *
     * 3.  **全面的可调用目标支持 (Broad Callable Target Support):**
     *     - 完美支持成员函数指针和成员变量指针的调用，能够自动处理第一个参数是
     *       对象、对象引用还是对象指针的情况。
     *
     * 4.  **健壮的生命周期管理与异常安全:**
     *     - `Function` 对象的生命周期管理是完全自动且异常安全的。在赋值等操作中，
     *       如果新目标的构造失败，对象会保持有效的空状态，杜绝了内存泄露和状态损坏。
     *
     * 5.  **现代C++特性集成 (Modern C++ Integration):**
     *     - 提供了类模板参数推导 (CTAD)，使得可以方便地通过 `xyu::Function func = ...;`
     *       来创建对象，而无需手动指定模板参数。
     *     - 完美支持移动语义，可以高效地转移可调用对象的所有权。
     *
     * @example
     *   // 存储 lambda
     *   xyu::Function˂int(int, int)˃ add = [](int a, int b) { return a + b; };
     *   add(1, 2); // returns 3
     *
     *   // 存储成员函数
     *   struct MyClass { int value(int i) { return i; } };
     *   xyu::Function˂int(MyClass&, int)˃ mem_fn = &MyClass::value;
     *   MyClass obj;
     *   mem_fn(obj, 10); // returns 10
     */
    template <typename Func>
    class Function;

    template <typename Ret, typename... Args>
    class Function<Ret(Args...)>
    {
    private:
        // 函数数据
        __::FuncData data;
        // 类函数调用指针
        Ret(*caller)(const __::FuncData&, Args...);
        // 类函数管理指针
        void(*manager)(__::FuncData&, const __::FuncData&, __::FuncOrder);

    public:
        /* 构造析构 */

        /// 默认构造
        Function() noexcept : caller{nullptr}, manager{nullptr} {}
        /// 复制构造
        Function(const Function& other) : Function{} { *this = other; }
        /// 移动构造
        Function(Function&& other) noexcept : caller{other.caller}, manager{other.manager}
        {
            if (XY_UNLIKELY(manager == nullptr)) return;
            data = xyu::move(other.data);
            other.manager = nullptr;
        }

        /// nullptr 构造
        Function(xyu::nullptr_t) noexcept : Function{} {}
        /// 类函数构造
        template <typename Func, typename = xyu::t_enable<!xyu::t_is_same_nocvref<Func, Function>>, typename F = xyu::t_decay<Func>>
        Function(Func&& func) noexcept(noexcept(__::FuncManager<Ret, F>::create(data, xyu::forward<Func>(func)))) : Function{}
        {
            static_assert(xyu::t_can_call_to<Ret, Func, Args...>);
            static_assert(xyu::t_can_nothrow_destruct<F>);
            if constexpr (xyu::t_is_pointer<F>) if (XY_UNLIKELY(func == nullptr)) return;
            __::FuncManager<Ret, F>::create(data, xyu::forward<F>(func));
            caller = &__::FuncManager<Ret, F>::template call<Args...>;
            manager = &__::FuncManager<Ret, F>::manage;
        }

        /// 析构
        ~Function() noexcept
        {
            if (XY_UNLIKELY(manager == nullptr)) return;
            manager(data, data, __::FuncOrder::Destroy);
        }

        /* 赋值交换 */

        /// 复制赋值
        Function& operator=(const Function& other)
        {
            if (XY_UNLIKELY(this == &other)) return *this;
            if (XY_LIKELY(manager != nullptr))
            {
                manager(data, data, __::FuncOrder::Destroy);
                manager = nullptr; // 防止 copy 抛出异常时重复析构
            }
            if (XY_LIKELY(other.manager != nullptr)) other.manager(data, other.data, __::FuncOrder::Copy);
            caller = other.caller;
            manager = other.manager;
            return *this;
        }
        /// 移动赋值
        Function& operator=(Function&& other) noexcept { swap(other); }

        /// nullptr 赋值
        Function& operator=(xyu::nullptr_t) noexcept
        {
            if (XY_LIKELY(manager != nullptr)) manager(data, data, __::FuncOrder::Destroy);
            manager = nullptr;
            return *this;
        }
        /// 类函数赋值
        template <typename Func, typename = xyu::t_enable<!xyu::t_is_same_nocvref<Func, Function>>, typename F = xyu::t_decay<Func>>
        Function& operator=(Func&& func) noexcept(noexcept(__::FuncManager<Ret, F>::create(data, xyu::forward<Func>(func))))
        {
            static_assert(xyu::t_can_call_to<Ret, Func, Args...>);
            static_assert(xyu::t_can_nothrow_destruct<F>);
            if (XY_LIKELY(manager != nullptr)) manager(data, data, __::FuncOrder::Destroy);
            if constexpr (!noexcept(__::FuncManager<Ret, F>::create(data, xyu::forward<Func>(func))))
                manager = nullptr; // 防止 create 抛出异常时重复析构
            if constexpr (xyu::t_is_pointer<F>) if (XY_UNLIKELY(func == nullptr)) return *this;
            __::FuncManager<Ret, F>::create(data, xyu::forward<Func>(func));
            caller = &__::FuncManager<Ret, F>::template call<Args...>;
            manager = &__::FuncManager<Ret, F>::manage;
            return *this;
        }

        /// 交换
        Function& swap(Function&& other) noexcept { return swap(other); }
        /// 交换
        Function& swap(Function& other) noexcept
        {
            xyu::swap(data, other.data);
            xyu::swap(caller, other.caller);
            xyu::swap(manager, other.manager);
            return *this;
        }

        /* 对象 */

        /// 是否为空
        bool empty() const noexcept { return manager == nullptr; }
        /// 是否有效
        explicit operator bool() const noexcept { return manager != nullptr; }

        /// 释放
        void release() noexcept
        {
            if (XY_UNLIKELY(manager == nullptr)) return;
            manager(data, data, __::FuncOrder::Destroy);
            manager = nullptr;
        }

        /* 调用 */

        /// 调用
        template <typename... VArgs>
        Ret operator()(VArgs&&... args) const
        {
            static_assert(sizeof...(Args) == sizeof...(VArgs));
            static_assert((... && xyu::t_can_icast<VArgs, Args>));
#if XY_DEBUG
            if (XY_UNLIKELY(manager == nullptr)) {
                xyloge(false, "E_Logic_Invalid_Argument: function is empty");
                throw xyu::E_Logic_Invalid_Argument{};
            }
#endif
            return caller(data, xyu::forward<VArgs>(args)...);
        }
    };

    namespace __
    {
        template <typename T> struct get_function_type;
        // 普通函数指针
        template <typename Res, typename... Args, bool Nt>
        struct get_function_type<Res(*)(Args...) noexcept(Nt)> { using type = Res(Args...); };
        // 成员变量指针
        template <typename Res, typename Class>
        struct get_function_type<Res(Class::*)> { using type = Res(Class&); };
        // 成员函数指针
        template <typename Res, typename Class, bool Nt, typename... Args>
        struct get_function_type<Res (Class::*) (Args...) noexcept(Nt)> { using type = Res(Class&, Args...); };
        template <typename Res, typename Class, bool Nt, typename... Args>
        struct get_function_type<Res (Class::*) (Args...) & noexcept(Nt)> { using type = Res(Class&, Args...); };
        template <typename Res, typename Class, bool Nt, typename... Args>
        struct get_function_type<Res (Class::*) (Args...) && noexcept(Nt)> { using type = Res(Class&&, Args...); };
        template <typename Res, typename Class, bool Nt, typename... Args>
        struct get_function_type<Res (Class::*) (Args...) const noexcept(Nt)> { using type = Res(const Class&, Args...); };
        template <typename Res, typename Class, bool Nt, typename... Args>
        struct get_function_type<Res (Class::*) (Args...) const & noexcept(Nt)> { using type = Res(const Class&, Args...); };
        template <typename Res, typename Class, bool Nt, typename... Args>
        struct get_function_type<Res (Class::*) (Args...) const && noexcept(Nt)> { using type = Res(const Class&&, Args...); };
    }
    //推导指引
    template <typename Func, typename F = typename __::get_function_type<Func>::type>
    Function(Func) -> Function<F>;
}

#pragma clang diagnostic pop