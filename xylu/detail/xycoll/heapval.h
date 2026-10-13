#pragma once

/**
 * @file    xycoll/heapval.h
 * @brief   堆对象
 * @date    2026-05-05
 */

#include "./__/tool.h"
#include "../xystd/structbind.h"

/// 堆对象
namespace xylu::xycoll
{
    namespace __
    {
        // 数组封装
        template <typename T, xyu::size_t N>
        struct ArrayData
        {
            using ArrRef = T(&)[N];
            using ArrCRef = xyu::t_add_const<T>(&)[N];

            T arr[N];    // 数组

            // 获取元素 (左值)
            template <xyu::size_t index>
            T& get() & noexcept
            {
                static_assert(index < N);
                return arr[index];
            }
            // 获取元素 (右值)
            template <xyu::size_t index>
            T&& get() && noexcept { return xyu::move(((ArrayData&)*this).get<index>()); }
            // 获取元素 (const)
            template <xyu::size_t index>
            xyu::t_add_const<T>& get() const& noexcept { return ((ArrayData&)*this).get<index>(); }

            // 索引访问
            T& operator[](xyu::size_t index) & noexcept
            {
                Checker::check_index(index, N, XY_CurPos("HeapVal::[]"));
                return arr[index];
            }
            T&& operator[](xyu::size_t index) && noexcept { return xyu::move(((ArrayData&)*this)[index]); }
            xyu::t_add_const<T>& operator[](xyu::size_t index) const& noexcept { return ((ArrayData&)*this)[index]; }

            // 转换为数组引用
            operator ArrRef() noexcept { return arr; }
            // 转换为数组引用 (const)
            operator ArrCRef() const noexcept { return arr; }

            // 创建范围
            auto range() noexcept { return xyu::make_range(arr); }
            // 创建范围 (const)
            auto range() const noexcept { return xyu::make_range(arr).crange(); }
        };

        // 底层 data
        template <typename T, bool cross_thread>
        struct HeapValData
        {
        protected:
            T* data;      // 堆指针
        public:
            // 构造
            HeapValData(T* data = nullptr) noexcept : data(data) {}
            // 析构
            ~HeapValData() noexcept
            {
                if (!data) return;
                data->~T();
                Common::dealloc<cross_thread>(data);
                data = nullptr;
            }
            // 特殊函数清空
            HeapValData(const HeapValData&) noexcept {}
            HeapValData(HeapValData&&) noexcept {}
            HeapValData& operator=(const HeapValData&) noexcept { return *this; }
            HeapValData& operator=(HeapValData&&) noexcept { return *this; }
        };

        template <typename T, bool cross_thread>
        struct HeapValData<T[], cross_thread>
        {
        protected:
            xyu::size_t n;  // 元素数量
            T* data;        // 堆指针
        public:
            // 构造
            HeapValData() noexcept : n(0), data(nullptr) {}
            HeapValData(xyu::size_t n, T* data = nullptr) noexcept : n(n), data(data)
            {
                if (!data) data = Common::alloc<T, cross_thread>(n);
            }
            // 特殊函数清空
            ~HeapValData() noexcept {}
            HeapValData(const HeapValData&) noexcept {}
            HeapValData(HeapValData&&) noexcept {}
            HeapValData& operator=(const HeapValData&) noexcept { return *this; }
            HeapValData& operator=(HeapValData&&) noexcept { return *this; }
        };
    }

    /**
     * @brief 堆对象
     * @tparam T  值类型。可为 `U`（单对象）、`U[N]`（固定数组）或 `U[]`（动态数组）
     * @tparam cross_thread  是否允许跨线程内存管理。
     *         - false（默认）: 使用线程局部内存池，性能更好；
     *         - true : 使用全局内存分配，对象可跨线程重分配和释放。
     *
     * @details
     *   存储方式：堆上分配 T，通过 `class_auto_copy/move` 转发 复制/移动 构造/赋值 能力
     *
     *   `U`-单对象：
     *     方法简述：
     *       - 支持任意构造、赋值、交换
     *       - 转发被包装类型的各类运算符（前置/后置 ++ --、算术、比较、下标、调用等）
     *       - 支持到原类型的隐式转换，并转发原类型自身的隐式/显式类型转换
     *       - 支持 `get<index>()`结构化绑定、`range()`范围
     *       - 上述均通过 sfinae 检测，只会生成原类型所拥有的，其他成员函数依旧需要通过 `get` 获取原对象后访问
     *       - 支持 `take` 转移所有权，`xyu::special_v` 通过 `take`转移的 或 同种分配模式的堆指针 构造
     *
     *     注意事项：
     *       - 空指针：延迟构造或 `take` 后访问空指针，或 `xyu::special_v` 传入空指针
     *                会在 !XY_RELEASE 下 abort
     *       - 取地址：`op &` 会返回真实对象的地址，而不是包装类的地址，与栈对象完全相同
     *
     *     异常安全：
     *       - 赋值完全转发底层类型，与底层类型相同
     *
     *   `U[N]`-固定数组：
     *     方法简述：
     *       - 支持范围构造/赋值（元素不足时默认填充，超出则截断）、`xyu::native_t`延迟构造
     *       - 支持 `count`、`op []`、`range` 范围、`get<index>()` 结构化绑定
     *       - 支持隐式转换为数组引用
     *       - 支持 `take` 转移所有权，`xyu::special_v` 通过 `take`转移的堆指针 构造
     *
     *     注意事项：
     *       - 空指针：`take` 后访问空指针会在 !XY_RELEASE 下 abort
     *       - 索引越界：`op []` 索引越界 !XY_RELEASE 下 abort
     *       - 取地址：`op &` 会返回真实数组的地址，而不是包装类的地址，与栈对象完全相同
     *
     *     异常安全：
     *       - 范围赋值：若可无抛逐元素赋值则直接操作；否则构造临时对象并 swap，抛出异常时恢复到原状态
     *
     *   `U[]`-动态数组：
     *     方法简述：
     *       - 支持定大小默认构造、任意构造（逐值提供 + 默认填充）、范围构造/赋值、`xyu::native_t`延迟构造
     *       - 提供 `count`、`op[]`、`range` 范围，支持隐式转换为 `U*`
     *       - 支持 `take` 转移所有权，`xyu::special_v` 通过 `take`转移的堆指针 构造
     *
     *     注意事项：
     *       - 范围限制：传入非有限范围在 !XY_RELEASE 下检查并触发 abort
     *       - 空指针：`take` 后访问空指针会在 !XY_RELEASE 下 abort
     *       - 索引越界：`op []` 索引越界 !XY_RELEASE 下 abort
     *
     *     异常安全：
     *       - 范围赋值：若长度匹配且可无异常赋值则直接操作；否则通过临时对象 swap 抛出时恢复原状态
     *
     *
     *   @bar 单对象
     *   @code
     *   // 示例1
     *   HeapVal˂SString˂65536˃˃ str("Hello");    // 在堆上构造 SString
     *   str.get().append(" world");              // 通过 get() 访问
     *
     *   // 示例2
     *   HeapVal˂SString˂65536˃˃ build_greeting() {
     *       HeapVal˂SString˂65536˃˃ raw("Hello");
     *       raw ˂˂ " World";
     *       ......
     *       return raw;
     *   }
     *   void process() {
     *       auto str = HeapVal(xyu::special_v, build_greeting().take());
     *       str += "!";
     *   }
     *   @endcode
     *
     *   @bar 固定数组
     *   @code
     *   HeapVal˂Vec3[100]˃ models;              // 默认构造100个Vec3
     *   models[0] = Vec3{1,0,0};                // 索引访问
     *   for (auto& v : models.range()) { ... }  // 范围遍历
     *   @endcode
     *
     *   @bar 动态数组
     *   @code
     *   HeapVal˂int[]˃ nums(20);                 // 20个默认初始化的int
     *   nums[5] = 42;
     *   auto copied = nums;                      // 深拷贝
     *   auto r = nums.range();                   // 获取范围
     *   @endcode
     */
    template <typename T, bool cross_thread = false>
    struct HeapVal : __::HeapValData<T, cross_thread>,
            xyu::class_auto_copy_construct_t<HeapVal<T>, xyu::t_can_cpconstr<T>>,
            xyu::class_auto_copy_assign_t<HeapVal<T>, xyu::t_can_cpassign<T>>,
            xyu::class_auto_move_construct_t<HeapVal<T>, xyu::t_can_mvconstr<T>>,
            xyu::class_auto_move_assign_t<HeapVal<T>, xyu::t_can_mvassign<T>>
    {
        static_assert(xyu::t_is_complete<T>, "can't allocate memory for incomplete type");
        static_assert(xyu::t_can_nothrow_destruct<T>, "destruct function can't throw");

        /* 构造赋值 */

        /// 任意构造
        template <typename... Args, typename = xyu::t_enable<xyu::t_can_init<T, Args...> && (sizeof...(Args) != 1 || !xyu::t_is_same_nocr<HeapVal, Args...>)>>
        HeapVal(Args&&... args) noexcept(xyu::t_can_nothrow_init<T, Args...>)
        : __::HeapValData<T, cross_thread>{__::Common::alloc<T, cross_thread>()}
        {
            if constexpr (xyu::t_can_nothrow_init<T, Args...>)
                xyu::place_init(this->data, xyu::forward<Args>(args)...);
            else try { xyu::place_init(this->data, xyu::forward<Args>(args)...); }
                catch (...) { __::Common::dealloc<cross_thread>(this->data); throw; }
        }

        /// 任意赋值
        template <typename Arg, typename = xyu::t_enable<xyu::t_can_assign<T, Arg> && !xyu::t_is_same_nocr<Arg, HeapVal>>>
        HeapVal& operator=(Arg&& arg) noexcept(xyu::t_can_nothrow_assign<T, Arg>)
        {
            get() = xyu::forward<Arg>(arg);
            return *this;
        }

        /* 交换转移 */

        /// 交换
        HeapVal& swap(HeapVal& other) noexcept
        {
            xyu::swap(this->data, other.data);
            return *this;
        }

        /**
         * @brief 转移对象构造
         * @param p 必须为 xyu::alloc 或 take() 获取的对象
         */
        XY_NONNULL_RELEASE(3)
        explicit HeapVal(xyu::special_t, T* p) noexcept : __::HeapValData<T, cross_thread>(p) { get(); }

        /**
         * @brief 转移对象
         * @attention 转移后原对象失效，无法继续使用
         * 转移后的对象需要手动管理，或通过 xyu::special_v 转移给 另一个 HeapVal
         */
        [[nodiscard]]
        T* take() noexcept
        {
            T* tmp = &get();
            this->data = nullptr;
            return tmp;
        }

        /* 对象操作 */

        /// 获取对象
        T& get() & noexcept { return *__::Checker::check_ptr(this->data, XY_CurPos("HeapVal::get")); }
        /// 获取对象 (移动)
        T&& get() && noexcept { return xyu::move(((HeapVal&)(*this)).get()); }
        /// 获取对象 (const)
        xyu::t_add_const<T>& get() const& noexcept { return ((HeapVal&)(*this)).get(); }

        /// 隐式转换
        operator T&() & noexcept { return get(); }
        /// 隐式转换 (移动)
        operator T&&() && noexcept { return xyu::move(get()); }
        /// 隐式转换 (const)
        operator xyu::t_add_const<T>&() const& noexcept { return get(); }

        /// 获取指针
        T* operator&() noexcept { return &get(); }
        /// 获取指针 (const)
        xyu::t_add_const<T>* operator&() const noexcept { return ((HeapVal&)(*this)).operator&(); }

        /* 转换支持 */

        /// 隐式转换
        template <typename U, typename Test = T, xyu::t_enable<xyu::t_can_icast<Test&,U&>, bool> = true>
        operator U&() & noexcept(xyu::t_can_nothrow_icast<T&,U&>) { return get(); }
        /// 隐式转换 (移动)
        template <typename U, typename Test = T, xyu::t_enable<xyu::t_can_icast<Test&&,U&>, bool> = true>
        operator U&() && noexcept(xyu::t_can_nothrow_icast<T&&,U&>) { return xyu::move(get()); }
        /// 隐式转换 (const)
        template <typename U, typename Test = T, xyu::t_enable<xyu::t_can_icast<const Test&,U&>, bool> = true>
        operator U&() const& noexcept(xyu::t_can_nothrow_icast<const T&,U&>) { return get(); }

        /// 隐式转换
        template <typename U, typename Test = T, xyu::t_enable<xyu::t_can_icast<Test&,U> && !xyu::t_can_icast<Test&,U&>, bool> = true>
        operator U() & noexcept(xyu::t_can_nothrow_icast<T&,U>) { return get(); }
        /// 隐式转换 (移动)
        template <typename U, typename Test = T, xyu::t_enable<xyu::t_can_icast<Test&&,U> && !xyu::t_can_icast<Test&&,U&>, bool> = true>
        operator U() && noexcept(xyu::t_can_nothrow_icast<T&&,U>) { return xyu::move(get()); }
        /// 隐式转换 (const)
        template <typename U, typename Test = T, xyu::t_enable<xyu::t_can_icast<const Test&,U> && !xyu::t_can_icast<const Test&,U&>, bool> = true>
        operator U() const& noexcept(xyu::t_can_nothrow_icast<const T&,U>) { return get(); }

        /// 显式转换
        template <typename U, typename Test = T, xyu::t_enable<xyu::t_can_cast<Test&,U&> && !xyu::t_can_icast<Test&,U&>, bool> = false>
        explicit operator U&() & noexcept(xyu::t_can_nothrow_cast<T&,U&>) { return static_cast<U&>(get()); }
        /// 显式转换 (移动)
        template <typename U, typename Test = T, xyu::t_enable<xyu::t_can_cast<Test&&,U&> && !xyu::t_can_icast<Test&&,U&>, bool> = true>
        explicit operator U&() && noexcept(xyu::t_can_nothrow_cast<T&&,U&>) { return static_cast<U&>(xyu::move(get())); }
        /// 显式转换 (const)
        template <typename U, typename Test = T, xyu::t_enable<xyu::t_can_cast<const Test&,U&> && !xyu::t_can_icast<const Test&,U&>, bool> = true>
        explicit operator U&() const& noexcept(xyu::t_can_nothrow_cast<const T&,U&>) { return static_cast<U&>(get()); }

        /// 显式转换
        template <typename U, typename Test = T, xyu::t_enable<xyu::t_can_cast<Test&,U> && !xyu::t_can_icast<Test&,U> && !xyu::t_can_cast<Test&,U&>, bool> = false>
        explicit operator U() & noexcept(xyu::t_can_nothrow_cast<T&,U>) { return static_cast<U>(get()); }
        /// 显式转换 (移动)
        template <typename U, typename Test = T, xyu::t_enable<xyu::t_can_cast<Test&&,U> && !xyu::t_can_icast<Test&&,U> && !xyu::t_can_cast<Test&&,U&>, bool> = true>
        explicit operator U() && noexcept(xyu::t_can_nothrow_cast<T&&,U>) { return static_cast<U>(xyu::move(get())); }
        /// 显式转换 (const)
        template <typename U, typename Test = T, xyu::t_enable<xyu::t_can_cast<const Test&,U> && !xyu::t_can_icast<const Test&,U> && !xyu::t_can_cast<const Test&,U&>, bool> = true>
        explicit operator U() const& noexcept(xyu::t_can_nothrow_cast<const T&,U>) { return static_cast<U>(get()); }

        /* 运算符支持 */

        // 无参数
    #define XY_memop0_test_gen( op ) \
        template <typename Test = T, typename = decltype(op xyu::t_val<Test&>())> \
        decltype(auto) operator op() & noexcept(noexcept(op xyu::t_val<T&>())) { return op get();} \
        template <typename Test = T, typename = decltype(op xyu::t_val<Test&&>())> \
        decltype(auto) operator op() && noexcept(noexcept(op xyu::t_val<T&&>())) { return op get();} \
        template <typename Test = T, typename = decltype(op xyu::t_val<const Test&>())> \
        decltype(auto) operator op() const& noexcept(noexcept(op xyu::t_val<const T&>())) { return op get();}
    XY_PP_FORWARD(XY_memop0_test_gen, 1, ++, --, *, +, -, !, ~)
    #undef XY_memop0_test_gen
    
        // 后置 ++ --
    #define XY_memop0_test_gen( op ) \
        template <typename Test = T, typename = decltype(xyu::t_val<Test&>() op)> \
        decltype(auto) operator op(int) & noexcept(noexcept(xyu::t_val<T&>() op)) { return get() op;} \
        template <typename Test = T, typename = decltype(xyu::t_val<Test&&>() op)> \
        decltype(auto) operator op(int) && noexcept(noexcept(xyu::t_val<T&&>() op)) { return get() op;} \
        template <typename Test = T, typename = decltype(xyu::t_val<const Test&>() op)> \
        decltype(auto) operator op(int) const& noexcept(noexcept(xyu::t_val<const T&>() op)) { return get() op;}
    XY_PP_FORWARD(XY_memop0_test_gen, 1, ++, --)
    #undef XY_memop0_test_gen

        // 单参数
    #define XY_memop1_test_gen( op ) \
        template <typename Arg, typename Test = T, typename = decltype(xyu::t_val<Test&>() op xyu::t_val<Arg>())> \
        decltype(auto) operator op(Arg&& arg) & noexcept(noexcept(xyu::t_val<T&>() op xyu::t_val<Arg>())) { return get() op xyu::forward<Arg>(arg); } \
        template <typename Arg, typename Test = T, typename = decltype(xyu::t_val<Test&&>() op xyu::t_val<Arg>())> \
        decltype(auto) operator op(Arg&& arg) && noexcept(noexcept(xyu::t_val<T&&>() op xyu::t_val<Arg>())) { return xyu::move(get()) op xyu::forward<Arg>(arg); } \
        template <typename Arg, typename Test = T, typename = decltype(xyu::t_val<const Test&>() op xyu::t_val<Arg>())> \
        decltype(auto) operator op(Arg&& arg) const& noexcept(noexcept(xyu::t_val<const T&>() op xyu::t_val<Arg>())) { return get() op xyu::forward<Arg>(arg); }
    XY_PP_FORWARD(XY_memop1_test_gen, 1, +,+=, -,-=, *,*=, /,/=, %,%=)
    XY_PP_FORWARD(XY_memop1_test_gen, 1, &,&=, |,|=, ^,^=, <<,<<=, >>,>>=)
    XY_PP_FORWARD(XY_memop1_test_gen, 1, ==, !=, >, >=, <, <=)
    #undef XY_memop1_test_gen

        // []
        template <typename Arg, typename Test = T, typename = decltype(xyu::t_val<Test&>()[xyu::t_val<Arg>()])> \
        decltype(auto) operator[](Arg&& arg) & noexcept(noexcept(xyu::t_val<T&>()[xyu::t_val<Arg>()])) { return get()[xyu::forward<Arg>(arg)]; } \
        template <typename Arg, typename Test = T, typename = decltype(xyu::t_val<Test&&>()[xyu::t_val<Arg>()])> \
        decltype(auto) operator[](Arg&& arg) && noexcept(noexcept(xyu::t_val<T&&>()[xyu::t_val<Arg>()])) { return xyu::move(get()) [xyu::forward<Arg>(arg)]; } \
        template <typename Arg, typename Test = T, typename = decltype(xyu::t_val<const Test&>()[xyu::t_val<Arg>()])> \
        decltype(auto) operator[](Arg&& arg) const& noexcept(noexcept(xyu::t_val<const T&>()[xyu::t_val<Arg>()])) { return get()[xyu::forward<Arg>(arg)]; }

        // ()
        template <typename... Args, typename Test = T, typename = decltype(xyu::t_val<Test&>().operator()(xyu::t_val<Args>()...))> \
        decltype(auto) operator()(Args&&... args) & noexcept(noexcept(xyu::t_val<T&>().operator()(xyu::t_val<Args>()...))) { return get().operator()(xyu::forward<Args>(args)...); } \
        template <typename... Args, typename Test = T, typename = decltype(xyu::t_val<Test&&>().operator()(xyu::t_val<Args>()...))> \
        decltype(auto) operator()(Args&&... args) && noexcept(noexcept(xyu::t_val<T&&>().operator()(xyu::t_val<Args>()...))) { return xyu::move(get()).operator()(xyu::forward<Args>(args)...); } \
        template <typename... Args, typename Test = T, typename = decltype(xyu::t_val<const Test&>().operator()(xyu::t_val<Args>()...))> \
        decltype(auto) operator()(Args&&... args) const& noexcept(noexcept(xyu::t_val<const T&>().operator()(xyu::t_val<Args>()...))) { return get().operator()(xyu::forward<Args>(args)...); }

        /* 通用比较支持 */

    #define XY_memfun1_test_gen( func ) \
        template <typename Arg, typename Test = T, typename = decltype(xyu::t_val<Test&>().func(xyu::t_val<Arg>()))> \
        decltype(auto) func(Arg&& arg) & noexcept(noexcept(xyu::t_val<T&>().func(xyu::t_val<Arg>()))) { return get().func(xyu::forward<Arg>(arg)); } \
        template <typename Arg, typename Test = T, typename = decltype(xyu::t_val<Test&&>().func(xyu::t_val<Arg>()))> \
        decltype(auto) func(Arg&& arg) && noexcept(noexcept(xyu::t_val<T&&>().func(xyu::t_val<Arg>()))) { return xyu::move(get()).func(xyu::forward<Arg>(arg)); } \
        template <typename Arg, typename Test = T, typename = decltype(xyu::t_val<const Test&>().func(xyu::t_val<Arg>()))> \
        decltype(auto) func(Arg&& arg) const& noexcept(noexcept(xyu::t_val<const T&>().func(xyu::t_val<Arg>()))) { return get().func(xyu::forward<Arg>(arg)); }
    XY_PP_FORWARD(XY_memfun1_test_gen, 1, equals, compare)
    #undef XY_memfun1_test_gen

        /* 范围支持 */

    #define XY_memfun0_test_gen( func ) \
        template <typename Test = T, typename = decltype(xyu::t_val<Test&>().func())> \
        decltype(auto) func() & noexcept(noexcept(xyu::t_val<T&>().func())) { return get().func(); } \
        template <typename Test = T, typename = decltype(xyu::t_val<Test&&>().func())> \
        decltype(auto) func() && noexcept(noexcept(xyu::t_val<T&&>().func())) { return xyu::move(get()).func(); } \
        template <typename Test = T, typename = decltype(xyu::t_val<const Test&>().func())> \
        decltype(auto) func() const& noexcept(noexcept(xyu::t_val<const T&>().func())) { return get().func(); }
        // 获取元素个数
        XY_memfun0_test_gen( count )
        // 获取范围
        XY_memfun0_test_gen( range )
    #undef XY_memfun0_test_gen

        /* 结构化绑定支持 */

        template <xyu::size_t index, typename Test = T, typename = decltype(xyu::t_val<Test&>().template get<index>())>
        decltype(auto) get() & noexcept(noexcept(xyu::t_val<T&>().template get<index>())) { return get().template get<index>(); }
        template <xyu::size_t index, typename Test = T, typename = decltype(xyu::t_val<Test&&>().template get<index>())>
        decltype(auto) get() && noexcept(noexcept(xyu::t_val<T&&>().template get<index>())) { return xyu::move(get()).template get<index>(); }
        template <xyu::size_t index, typename Test = T, typename = decltype(xyu::t_val<const Test&>().template get<index>())>
        decltype(auto) get() const& noexcept(noexcept(xyu::t_val<const T&>().template get<index>())) { return get().template get<index>(); }

    protected:
        // 延迟构造
        HeapVal(xyu::native_t) noexcept : __::HeapValData<T, cross_thread>{__::Common::alloc<T, cross_thread>()} {}

    private:
        /* 复制移动 */

        friend struct xyu::class_auto_copy_construct_t<HeapVal, xyu::t_can_cpconstr<T>>;
        friend struct xyu::class_auto_copy_assign_t<HeapVal, xyu::t_can_cpassign<T>>;
        friend struct xyu::class_auto_move_construct_t<HeapVal, xyu::t_can_mvconstr<T>>;
        friend struct xyu::class_auto_move_assign_t<HeapVal, xyu::t_can_mvassign<T>>;

        // 复制构造
        void do_cpconstr(const HeapVal& other) noexcept(xyu::t_can_nothrow_cpconstr<T>)
        {
            this->data = __::Common::alloc<T, cross_thread>();
            if constexpr (xyu::t_can_nothrow_cpconstr<T>) xyu::place_init(this->data, other.get());
            else try { xyu::place_init(this->data, other.get()); }
                catch (...) { __::Common::dealloc<cross_thread>(this->data); throw; }
        }
        // 复制赋值
        void do_cpassign(const HeapVal& other) noexcept(xyu::t_can_nothrow_cpassign<T>) { get() = other.get(); }
        // 移动构造
        void do_mvconstr(HeapVal&& other) noexcept(xyu::t_can_nothrow_mvconstr<T>)
        {
            this->data = __::Common::alloc<T, cross_thread>();
            if constexpr (xyu::t_can_nothrow_mvconstr<T>) xyu::place_init(this->data, xyu::move(other.get()));
            else try { xyu::place_init(this->data, xyu::move(other.get())); }
                catch (...) { __::Common::dealloc<cross_thread>(this->data); throw; }
        }
        // 移动赋值
        void do_mvassign(HeapVal&& other) noexcept(xyu::t_can_nothrow_mvassign<T>) { get() = xyu::move(other.get()); }
    };

    /// 固定数组
    template <typename T, xyu::size_t N, bool cross_thread>
    struct HeapVal<T[N], cross_thread> : HeapVal<__::ArrayData<T,N>, cross_thread>
    {
        static_assert(N < xyu::t_number<xyu::size_t>::max / xyu::max(2, sizeof(T)));
    private:
        using Base = HeapVal<__::ArrayData<T,N>>;
        using ArrPtr = T(*)[N];
        using ArrCPtr = xyu::t_add_const<T>(*)[N];
        using ArrRef = T(&)[N];
        using ArrCRef = xyu::t_add_const<T>(&)[N];

    public:
        /* 构造赋值 */

        /// 延迟构造
        explicit HeapVal(xyu::native_t) noexcept : Base(xyu::native_v) {}

        /// 任意构造
        using HeapVal<__::ArrayData<T,N>>::HeapVal;

        /// 范围构造
        template <typename Rg, typename = xyu::t_enable<xyu::t_can_range<Rg> && xyu::t_can_init<T,xyu::t_access<Rg>> && !xyu::t_can_init<T,Rg>>>
        explicit HeapVal(Rg&& rg) noexcept(xyu::t_can_nothrow_init<T, xyu::t_noexcept_access<Rg>> && xyu::t_can_nothrow_dfconstr<T>) : Base(xyu::native_v)
        {
            auto range = xyu::make_range(xyu::forward<Rg>(rg));
            T *data = this->data->arr, *p = data, *e = data + N;
            auto ib = range.begin(), ie = range.end();
            if (range.count() <= N)
            {
                if constexpr (xyu::t_can_nothrow_cpconstr<T>)
                    for (;ib != ie; ++ib, ++p) xyu::place_init(p, *ib);
                else try { for (;ib != ie; ++ib, ++p) xyu::place_init(p, *ib); }
                    catch (...) { do_destroy(p - data); throw; }
                if constexpr (xyu::t_can_nothrow_dfconstr<T>)
                    for (; p < e; ++p) new (p) T{};
                else try { for (; p < e; ++p) new (p) T{}; }
                    catch (...) { do_destroy(p - data); throw; }
            }
            else
            {
                if constexpr (xyu::t_can_nothrow_cpconstr<T>)
                    for (;ib != ie && p < e; ++ib, ++p) xyu::place_init(p, *ib);
                else try { for (;ib != ie && p < e; ++ib, ++p) xyu::place_init(p, *ib); }
                    catch (...) { do_destroy(p - data); throw; }
                if (XY_UNLIKELY(range.indet()))
                {
                    if constexpr (xyu::t_can_nothrow_dfconstr<T>)
                        for (; p < e; ++p) new (p) T{};
                    else try { for (; p < e; ++p) new (p) T{}; }
                        catch (...) { do_destroy(p - data); throw; }
                }
            }
        }

        /// 范围赋值
        template <typename Rg, typename = xyu::t_enable<xyu::t_can_range<Rg> && xyu::t_can_assign<T,xyu::t_access<Rg>> && !xyu::t_is_same_nocr<Rg, HeapVal>>>
        HeapVal& operator=(Rg&& rg) noexcept(xyu::t_can_nothrow_assign<T, xyu::t_noexcept_access<Rg>>)
        {
            auto range = xyu::make_range(xyu::forward<Rg>(rg));
            if constexpr (xyu::t_can_nothrow_assign<T, xyu::t_noexcept_access<Rg>>)
            {
                T *p = this->data->arr, *e = p + N;
                auto ib = range.begin(), ie = range.end();
                if (range.count() <= N) for (;ib != ie; ++ib, ++p) *p = *ib;
                else for (;ib != ie && p < e; ++ib, ++p) *p = *ib;
            }
            else
            {
                HeapVal tmp(range);
                swap(tmp);
            }
            return *this;
        }

        /* 交换转移 */

        /// 交换
        HeapVal& swap(HeapVal& other) noexcept { Base::swap(other); return *this; }

        /**
         * @brief 转移对象构造
         * @param p 必须为 alloc<T[N]>(1) 或 take() 获取的对象
         */
        XY_NONNULL_RELEASE(3)
        explicit HeapVal(xyu::special_t, T(*p)[N]) noexcept : Base(xyu::special_v, (__::ArrayData<T,N>*)p) {}

        /**
         * @brief 转移对象
         * @attention 转移后原对象失效，无法继续使用。
         * 转移后的对象需要手动管理，或通过 xyu::special_v 转移给 另一个 HeapVal
         */
        [[nodiscard]]
        ArrPtr take() noexcept { return &Base::take()->arr; }

        /* 数组操作 */

        /// 获取数组大小
        static xyu::size_t count() noexcept { return N; }

        /// 获取指针
        ArrPtr operator&() noexcept { return &Base::get().arr; }
        /// 获取指针 (const)
        ArrCPtr operator&() const noexcept { return &Base::get().arr; }

    private:
        // 析构辅助
        void do_destroy(xyu::size_t constr_count) noexcept
        {
            xyu::object_destroy(this->data->arr, constr_count);
            __::Common::dealloc<cross_thread>(this->data, N);
        }
    };

    /// 动态数组
    template <typename T, bool cross_thread>
    struct HeapVal<T[], cross_thread> : __::HeapValData<T[], cross_thread>,
            xyu::class_auto_copy_construct_t<HeapVal<T[]>, xyu::t_can_cpconstr<T>>,
            xyu::class_auto_copy_assign_t<HeapVal<T[]>, xyu::t_can_cpconstr<T>>,
            xyu::class_auto_move_construct_t<HeapVal<T[]>, xyu::t_can_mvconstr<T>>,
            xyu::class_auto_move_assign_t<HeapVal<T[]>, xyu::t_can_mvconstr<T>>
    {
        static_assert(xyu::t_can_nothrow_destruct<T>, "destruct function can't throw");
    private:
        using ArrPtr = T(*)[];
        using ArrCPtr = xyu::t_add_const<T>(*)[];
        struct ArrSPtr { xyu::size_t n; T* p; };

    public:
        /* 构造赋值 */

        /// 析构
        ~HeapVal() noexcept { if (this->n > 0) do_destroy(this->n); }

        /// 默认构造
        template <typename Test = T, typename = xyu::t_enable<xyu::t_can_dfconstr<Test>>>
        explicit HeapVal(xyu::size_t n) noexcept(xyu::t_can_nothrow_dfconstr<T>) : HeapVal{n, xyu::native_v}
        {
            xyu::size_t i;
            if constexpr (xyu::t_can_nothrow_dfconstr<T>)
                for (i = 0; i < n; ++i) new (this->data + i) T{};
            else try { for (i = 0; i < n; ++i) new (this->data + i) T{}; }
                catch (...) { do_destroy(i); throw; }
        }

        /// 延迟构造
        HeapVal(xyu::size_t n, xyu::native_t) noexcept : __::HeapValData<T[], cross_thread>(n) {}

        /// 任意构造
        template <typename... Args, typename = xyu::t_enable<(... && xyu::t_can_init<T, Args>) && xyu::t_can_dfconstr<T>>>
        explicit HeapVal(xyu::size_t n, Args&&... args) noexcept((... && xyu::t_can_nothrow_init<T, Args>) && xyu::t_can_nothrow_dfconstr<T>) : HeapVal{n, xyu::native_v}
        {
            if constexpr (!XY_RELEASE) if (XY_UNLIKELY(sizeof...(Args) > n))
                xyloge3("provide too many args (count={}) for array[{}]", sizeof...(Args), n);

            xyu::size_t i = 0;
            if constexpr ((... && xyu::t_can_nothrow_init<T, Args>))
                (..., (xyu::place_init(this->data + i, xyu::forward<Args>(args)), ++i));
            else try { (..., (xyu::place_init(this->data + i, xyu::forward<Args>(args)), ++i)); }
                catch (...) { do_destroy(i); throw; }
            if constexpr (xyu::t_can_nothrow_dfconstr<T>)
                for (;i < n; ++i) new (this->data + i) T{};
            else try { for (;i < n; ++i) new (this->data + i) T{}; }
                catch (...) { do_destroy(i); throw; }
        }

        /// 范围构造
        template <typename Rg, typename = xyu::t_enable<xyu::t_can_range<Rg> && xyu::t_can_init<T,xyu::t_access<Rg>> && !xyu::t_can_init<T,Rg>>>
        explicit HeapVal(Rg&& rg) noexcept(xyu::t_can_nothrow_init<T, xyu::t_noexcept_access<Rg>>) : __::HeapValData<T[], cross_thread>()
        {
            auto range = xyu::make_range(xyu::forward<Rg>(rg));
            __::Checker::check_range(range, XY_CurPos("HeapVal::HeapVal"));
            this->n = range.count();
            this->data = __::Common::alloc<T, cross_thread>(this->n);
            xyu::size_t i = 0;
            if constexpr (xyu::t_can_nothrow_init<T, xyu::t_noexcept_access<Rg>>)
                for (auto&& v : range) xyu::place_init(this->data + i, xyu::forward<decltype(v)>(v)), ++i;
            else try { for (auto&& v : range) xyu::place_init(this->data + i, xyu::forward<decltype(v)>(v)), ++i; }
                catch (...) { do_destroy(i); }
        }
        /// 范围部分构造
        template <typename Rg, typename = xyu::t_enable<xyu::t_can_range<Rg> && xyu::t_can_init<T,xyu::t_access<Rg>> && !xyu::t_can_init<T,Rg> && xyu::t_can_dfconstr<T>>>
        explicit HeapVal(xyu::size_t n, Rg&& rg) noexcept(xyu::t_can_nothrow_init<T, xyu::t_noexcept_access<Rg>> && xyu::t_can_nothrow_dfconstr<T>) : HeapVal{n, xyu::native_v}
        {
            auto range = xyu::make_range(xyu::forward<Rg>(rg));
            T *data = this->data, *p = data, *e = data + n;
            auto ib = range.begin(), ie = range.end();
            if (range.count() <= n)
            {
                if constexpr (xyu::t_can_nothrow_cpconstr<T>)
                    for (;ib != ie; ++ib, ++p) xyu::place_init(p, *ib);
                else try { for (;ib != ie; ++ib, ++p) xyu::place_init(p, *ib); }
                    catch (...) { do_destroy(p - data); throw; }
                if constexpr (xyu::t_can_nothrow_dfconstr<T>)
                    for (; p < e; ++p) new (p) T{};
                else try { for (; p < e; ++p) new (p) T{}; }
                    catch (...) { do_destroy(p - data); throw; }
            }
            else
            {
                if constexpr (xyu::t_can_nothrow_cpconstr<T>)
                    for (;ib != ie && p < e; ++ib, ++p) xyu::place_init(p, *ib);
                else try { for (;ib != ie && p < e; ++ib, ++p) xyu::place_init(p, *ib); }
                    catch (...) { do_destroy(p - data); throw; }
                if (XY_UNLIKELY(range.indet()))
                {
                    if constexpr (xyu::t_can_nothrow_dfconstr<T>)
                        for (; p < e; ++p) new (p) T{};
                    else try { for (; p < e; ++p) new (p) T{}; }
                        catch (...) { do_destroy(p - data); throw; }
                }
            }
        }

        /// 范围赋值
        template <typename Rg, typename = xyu::t_enable<xyu::t_can_range<Rg> && xyu::t_can_init<T,xyu::t_access<Rg>> && !xyu::t_is_same_nocr<Rg, HeapVal>>>
        HeapVal& operator=(Rg&& rg) noexcept(xyu::t_can_nothrow_init<T, xyu::t_noexcept_access<Rg>>)
        {
            auto range = xyu::make_range(xyu::forward<Rg>(rg));
            if constexpr (xyu::t_can_nothrow_assign<T, xyu::t_noexcept_access<Rg>>)
            {
                if (XY_LIKELY(range.definite()) && range.count() == this->n)
                {
                    xyu::size_t i = 0;
                    for (auto&& v : range) this->data[i] = xyu::forward<decltype(v)>(v), ++i;
                    return *this;
                }
            }
            HeapVal tmp(range);
            swap(tmp);
            return *this;
        }

        /* 交换转移 */

        /// 交换
        HeapVal& swap(HeapVal& other) noexcept
        {
            xyu::swap(this->n, other.n);
            xyu::swap(this->data, other.data);
            return *this;
        }

        /**
         * @brief 转移对象构造
         * @param p 必须为 take() 获取的对象
         */
        explicit HeapVal(xyu::special_t, ArrSPtr p) noexcept : __::HeapValData<T[], cross_thread>(p.n, p.p) { get(); }

        /**
         * @brief 转移对象
         * @attention 转移后原对象失效，无法继续使用。
         * 转移后的对象需要通过 xyu::special_v 转移给 另一个 HeapVal
         */
        [[nodiscard]]
        ArrSPtr take() noexcept
        {
            xyu::size_t n = this->n;
            T* tmp = get();
            this->data = nullptr;
            this->n = 0;
            return {n, tmp};
        }

        /* 数组操作 */

        /// 获取数组大小
        xyu::size_t count() const noexcept { return this->n; }

        /// 获取元素 &
        T& operator[](xyu::size_t index) & noexcept { return get()[index]; }
        /// 获取元素 &&
        T&& operator[](xyu::size_t index) && noexcept { return xyu::move(get()[index]); }
        /// 获取元素 const
        xyu::t_add_const<T>& operator[](xyu::size_t index) const& noexcept { return get()[index]; }

        /// 隐式转换
        operator T*() noexcept { return get(); }
        /// 隐式转换 (const)
        operator xyu::t_add_const<T>*() const noexcept { return get(); }

        /// 获取指针
        ArrPtr operator&() noexcept { return (ArrPtr)this->check_get(XY_CurPos("HeapVal::operator&")); }
        /// 获取指针 (const)
        ArrCPtr operator&() const noexcept { return (ArrCPtr)this->check_get(XY_CurPos("HeapVal::operator&")); }

        /* 范围支持 */

        /// 获取范围
        auto range() noexcept { return xyu::Range<xyu::Iterator_Ptr<T,HeapVal>>(this->n, get(), this->data + this->n); }
        /// 获取范围 (const)
        auto range() const noexcept { return const_cast<HeapVal*>(this)->range().crange(); }

    private:
        // 析构辅助
        void do_destroy(xyu::size_t constr_count) noexcept
        {
            xyu::object_destroy(this->data, constr_count);
            __::Common::dealloc<cross_thread>(this->data, this->n);
            this->data = nullptr;
            this->n = 0;
        }

        // 获取指针
        T* get() noexcept { return __::Checker::check_ptr(this->data, XY_CurPos("HeapVal::get")); }
        xyu::t_add_const<T>* get() const noexcept { return ((HeapVal&)*this).get(); }

    private:
        /* 复制移动 */

        friend struct xyu::class_auto_copy_construct_t<HeapVal, xyu::t_can_cpconstr<T>>;
        friend struct xyu::class_auto_copy_assign_t<HeapVal, xyu::t_can_cpconstr<T>>;
        friend struct xyu::class_auto_move_construct_t<HeapVal, xyu::t_can_mvconstr<T>>;
        friend struct xyu::class_auto_move_assign_t<HeapVal, xyu::t_can_mvconstr<T>>;

        // 复制构造
        void do_cpconstr(const HeapVal& other) noexcept(xyu::t_can_nothrow_cpconstr<T>)
        {
            this->n = 0;
            this->data = nullptr;
            HeapVal tmp(other.count(), other.range());
            swap(tmp);
        }
        // 复制赋值
        void do_cpassign(const HeapVal& other) noexcept(xyu::t_can_nothrow_cpconstr<T>) { operator=(other.range()); }
        // 移动构造
        void do_mvconstr(HeapVal&& other) noexcept(xyu::t_can_nothrow_mvconstr<T>)
        {
            this->n = 0;
            this->data = nullptr;
            HeapVal tmp(other.count(), other.range().mrange());
            swap(tmp);
        }
        // 移动赋值
        void do_mvassign(HeapVal&& other) noexcept(xyu::t_can_nothrow_mvconstr<T>) { operator=(other.range()); }
    };

}

/// 格式化
namespace xylu::xystring
{
    // 隐式转换为原类型
    template <typename T>
    struct FormatImpl<xyu::HeapVal<T>, xyu::t_enable<!xyu::t_is_uarray<T>>> : FormatImpl<T> {};
    template <typename T, xyu::size_t N>
    struct FormatImpl<xylu::xycoll::__::ArrayData<T,N>> : FormatImpl<T[N]> {};
}

/// 结构化绑定
namespace std
{
    //元素容量
    template <typename T>
    struct tuple_size<xyu::HeapVal<T>> : xyu::size_c<tuple_size<T>::value> {};
    template <typename T, xyu::size_t N>
    struct tuple_size<xyu::HeapVal<T[N]>> : xyu::size_c<N> {};

    //元素类型
    template <xyu::size_t index, typename T>
    struct tuple_element<index, xyu::HeapVal<T>> { using type = typename tuple_element<index, T>::type; };
    template <xyu::size_t index, typename T, xyu::size_t N>
    struct tuple_element<index, xyu::HeapVal<T[N]>> { using type = T; };
}
