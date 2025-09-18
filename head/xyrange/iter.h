#pragma clang diagnostic push
#pragma ide diagnostic ignored "UnreachableCallsOfFunction"
#pragma ide diagnostic ignored "OCUnusedStructInspection"
#pragma ide diagnostic ignored "OCUnusedGlobalDeclarationInspection"
#pragma ide diagnostic ignored "OCUnusedTypeAliasInspection"
#pragma ide diagnostic ignored "google-explicit-constructor"
#pragma once

#include "../../link/traits"

/**
 * @file
 * @brief 提供了一个高度可定制的、基于策略设计模式的迭代器框架。
 * @details
 *   本文件定义了 `RangeIter` 类，这是一个通用的迭代器模板。
 *   通过组合不同的 “策略”模板参数，可以轻松构建出满足各种需求的迭代器，
 *   如指针迭代器、链表节点迭代器、常量迭代器、反向迭代器等。
 */

/// 迭代器存储类
namespace xylu::xyrange
{
    // 迭代器 - 存储类 - 指针
    template <typename T>
    struct RangeIter_Storage_Ptr
    {
        T* i;
        constexpr RangeIter_Storage_Ptr(T* p = nullptr) noexcept : i{p} {}
    };

    // 迭代器 - 存储类 - 迭代器
    template <typename Iter>
    struct RangeIter_Storage_Iter
    {
        Iter i;
        constexpr RangeIter_Storage_Iter(Iter it = {}) noexcept(noexcept(Iter{it})) : i{it} {}
    };
}

/// 迭代器操作类
namespace xylu::xyrange
{
    // 迭代器 - 判断有效 - 直接转为bool
    struct RangeIter_Valid_Native
    {
        template <typename Iter>
        constexpr bool operator()(Iter& it) noexcept(noexcept(static_cast<bool>(it.i)))
        { return static_cast<bool>(it.i); }
    };
    
    // 迭代器 - 判断有效 - 总是有效
    struct RangeIter_Valid_Always
    {
        template <typename Iter>
        constexpr bool operator()(Iter&) noexcept { return true; }
    };

    // 迭代器 - 获取对象地址 - 直接获取
    struct RangeIter_Address_Native
    {
        template <typename Iter>
        constexpr decltype(auto) operator()(Iter& it) noexcept { return it.i; }
    };

    // 迭代器 - 获取对象地址 - 解引用的地址
    struct RangeIter_Address_Deref
    {
        template <typename Iter>
        constexpr decltype(auto) operator()(Iter& it) noexcept(noexcept(&*it.i)) { return &*it.i; }
    };

    // 迭代器 - 解引用 - 直接解引用
    struct RangeIter_Dereference_Native
    {
        template <typename Iter>
        constexpr decltype(auto) operator()(Iter& it) noexcept(noexcept(*it.i)) { return *it.i; }
    };

    // 迭代器 - 解引用 - 原值
    struct RangeIter_Dereference_Self
    {
        template <typename Iter>
        constexpr decltype(auto) operator()(Iter& it) noexcept { return (it.i); }
    };

    // 迭代器 - 解引用 - 节点指针
    struct RangeIter_Dereference_Node
    {
        template <typename Iter>
        constexpr decltype(auto) operator()(Iter& it) noexcept { return (it.i->val); }
    };

    // 迭代器 - 获取对象地址 - 迭代器
    struct RangeIter_Address_Iter
    {
        template <typename Iter>
        constexpr decltype(auto) operator()(Iter& it) noexcept(noexcept(it.i.operator->()))
        { return it.i.operator->(); }
    };

    // 迭代器 - 获取对象地址 - 常量
    template <typename Address>
    struct RangeIter_Address_Const : private Address
    {
        template <typename Iter>
        constexpr decltype(auto) operator()(Iter& it) noexcept(noexcept(base()(it)))
        { return xyu::t_add_const_inptr<decltype(base()(it))>(base()(it)); }
    private:
        constexpr Address& base() noexcept { return static_cast<Address&>(*this); }
    };

    // 迭代器 - 解引用 - 常量
    template <typename Deref>
    struct RangeIter_Dereference_Const : private Deref
    {
        template <typename Iter>
        constexpr decltype(auto) operator()(Iter& it) noexcept(noexcept(base()(it)))
        { return xyu::t_add_const_inref<decltype(base()(it))>(base()(it)); }
    private:
        constexpr Deref& base() noexcept { return static_cast<Deref&>(*this); }
    };

    // 迭代器 - 解引用 - 移动
    template <typename Deref>
    struct RangeIter_Dereference_Move : private Deref
    {
        template <typename Iter>
        constexpr decltype(auto) operator()(Iter& it) noexcept(noexcept(base()(it)))
        { return xyu::move(base()(it)); }
    private:
        constexpr Deref& base() noexcept { return static_cast<Deref&>(*this); }
    };

    template <typename Deref>
    struct RangeIter_Dereference_Reverse;
    namespace __
    {
        // 判断是否为 RangeIter_Dereference_Reverse<Deref>
        template <typename T>
        struct is_rangeiter_deref_reverse_test : xyu::false_c {};
        template <typename Deref>
        struct is_rangeiter_deref_reverse_test<RangeIter_Dereference_Reverse<Deref>> : xyu::true_c {};
        template <typename Deref>
        constexpr bool is_rangeiter_deref_reverse = is_rangeiter_deref_reverse_test<Deref>::value;

        // 递归计算 实例化链上的 RangeIter_Dereference_Reverse<Deref> 数量
        template <typename T>
        struct count_deref_reverse_test { static constexpr xyu::size_t value = 0; };
        template <template <typename> class Tmpl, typename U>
        struct count_deref_reverse_test<Tmpl<U>>
        {
            static constexpr xyu::size_t value = count_deref_reverse_test<U>::value + is_rangeiter_deref_reverse<Tmpl<U>>;
        };
        // (目前不存在多个参数的实例化组件，暂时仅作实现参考)
        template <template <typename> class Tmpl, typename... Args>
        struct count_deref_reverse_test<Tmpl<Args...>>
        {
            static constexpr xyu::size_t value = (... + count_deref_reverse_test<Args>::value) +
                    is_rangeiter_deref_reverse<Tmpl<Args...>>;
        };
        template <typename T>
        constexpr xyu::size_t count_deref_reverse = count_deref_reverse_test<T>::value;
    }
    // 迭代器 - 解引用 - 反向
    // 通过判断 陆续(可隔代) 实例化的 Reverse 个数，来判断是 反向 还是 修正反向
    template <typename Deref>
    struct RangeIter_Dereference_Reverse : private Deref
    {
        template <typename Iter>
        constexpr decltype(auto) operator()(Iter& it) noexcept(noexcept(base()(--(++Iter(it)))))
        {
            constexpr xyu::size_t reverse_value = __::count_deref_reverse<Deref> + 1;
            if constexpr (reverse_value & 1) return base()(++Iter(it));
            else return base()(--Iter(it));
        }
    private:
        constexpr Deref& base() noexcept { return static_cast<Deref&>(*this); }
    };
}

/// 迭代器移动类
namespace xylu::xyrange
{
    // 迭代器 - 自增 - 直接自增
    struct RangeIter_Increment_Native
    {
        template <typename Iter>
        constexpr void operator()(Iter& it) noexcept(noexcept(++it.i)) { ++it.i; }
    };

    // 迭代器 - 自减 - 直接自减
    struct RangeIter_Decrement_Native
    {
        template <typename Iter>
        constexpr void operator()(Iter& it) noexcept(noexcept(--it.i)) { --it.i; }
    };

    // 迭代器 - 前进 - 直接前进
    struct RangeIter_Forward_Native
    {
        template <typename Iter, typename Distance>
        constexpr void operator()(Iter& it, Distance n) noexcept(noexcept(it.i += n)) { it.i += n; }
    };

    // 迭代器 - 后退 - 直接后退
    struct RangeIter_Backward_Native
    {
        template <typename Iter, typename Distance>
        constexpr void operator()(Iter& it, Distance n) noexcept(noexcept(it.i -= n)) { it.i -= n; }
    };

    // 迭代器 - 自增 - 节点指针
    struct RangeIter_Increment_Node
    {
        template <typename Iter>
        constexpr void operator()(Iter& it) noexcept
        { it.i = static_cast<decltype(it.i)>(it.i->next); }
    };

    // 迭代器 - 自减 - 节点指针
    struct RangeIter_Decrement_Node
    {
        template <typename Iter>
        constexpr void operator()(Iter& it) noexcept
        { it.i = static_cast<decltype(it.i)>(it.i->prev); }
    };

    // 迭代器 - 自增 - 树指针
    struct RangeIter_Increment_TreeNode
    {
        template <typename Iter>
        constexpr void operator()(Iter& it) noexcept
        {
            decltype(it.i->up) p = it.i;
            if (p->right) { p = p->right; while (p->left) p = p->left; }
            else {
                decltype(it.i->up) u = p->up;
                while (u->right == p) { p = u; u = u->up; }
                if (XY_LIKELY(p->right != u)) p = u;
            }
            it.i = static_cast<decltype(it.i)>(p);
        }
    };

    // 迭代器 - 自减 - 树指针
    struct RangeIter_Decrement_TreeNode
    {
        template <typename Iter>
        constexpr void operator()(Iter& it) noexcept
        {
            decltype(it.i->up) p = it.i;
            if (p->left) { p = p->left; while (p->right) p = p->right; }
            else {
                decltype(it.i->up) u = p->up;
                while (u->left == p) { p = u; u = u->up; }
                if (XY_LIKELY(p->left != u)) p = u;
            }
            it.i = static_cast<decltype(it.i)>(p);
        }
    };
}

/// 迭代器比较类
namespace xylu::xyrange
{
    // 迭代器 - 差值 - 直接相减
    struct RangeIter_Distance_Native
    {
        template <typename Iter>
        constexpr auto operator()(Iter& it1, Iter& it2) noexcept(noexcept(it1.i - it2.i)) { return it1.i - it2.i; }
    };

    // 迭代器 - 相等 - 直接==比较
    struct RangeIter_Equal_Native
    {
        template <typename Iter>
        constexpr bool operator()(Iter& it1, Iter& it2) noexcept(noexcept(it1.i == it2.i)) { return it1.i == it2.i; }
    };

    // 迭代器 - 比较 - 相减
    struct RangeIter_Compare_Subtract
    {
        template <typename Iter>
        constexpr auto operator()(Iter& it1, Iter& it2) noexcept(noexcept(it1.i - it2.i)) { return it1.i - it2.i; }
    };

    // 迭代器 - 差值 - 反向
    template <typename Distance>
    struct RangeIter_Distance_Reverse : private Distance
    {
        template <typename Iter>
        constexpr auto operator()(Iter& it1, Iter& it2) noexcept(noexcept(base()(it2, it1))) { return base()(it2, it1); }
    private:
        constexpr Distance& base() noexcept { return static_cast<Distance&>(*this); }
    };

    // 迭代器 - 比较 - 反向
    template <typename Compare>
    struct RangeIter_Compare_Reverse : private Compare
    {
        template <typename Iter>
        constexpr auto operator()(Iter& it1, Iter& it2) noexcept(noexcept(base()(it2, it1))) { return base()(it2, it1); }
    private:
        constexpr Compare& base() noexcept { return static_cast<Compare&>(*this); }
    };
}

/** 核心迭代器类 */
namespace xylu::xyrange
{
    /// 一个空类型，用作模板参数的占位符，表示某个迭代器功能被禁用。
    struct RangeIter_None {};

    /**
     * @brief 一个基于策略设计模式的通用迭代器模板。
     * @details
     *   `RangeIter` 通过模板参数接受一系列“策略类”，每个策略类负责实现迭代器的一种
     *   特定行为（如存储、解引用、自增、比较等）。通过组合不同的策略，可以灵活地
     *   构建出功能各异的迭代器，而无需编写大量重复的 boilerplate 代码。
     *
     *   策略类的实现要求:
     *   - 必须通过 `operator(iter&)` 调用 (不需要为 const)
     *   - 可以为非空类 (即可以有成员变量或函数)
     *   - 不需要 Iter& 或 Iter 的返回值 (即对于 Increment/Decrement/Forward/Backward 等操作，直接声明为 void)
     *
     * @tparam Storage     [必须] 负责存储迭代器的核心状态（如指针、或其他迭代器）。
     * @tparam Valid       [必须] 负责判断迭代器当前是否有效。
     * @tparam Dereference [必须] 负责实现解引用操作 (`operator*`)。
     * @tparam Address     [可选] 负责实现地址获取操作 (`operator->`)。
     * @tparam Increment   [可选] 负责实现自增操作 (`operator++`)。
     * @tparam Decrement   [可选] 负责实现自减操作 (`operator--`)。
     * @tparam Forward     [可选] 负责实现前进N步操作 (`operator+=`)。
     * @tparam Backward    [可选] 负责实现后退N步操作 (`operator-=`)。
     * @tparam Distance    [可选] 负责计算两个迭代器之间的距离 (`operator-`)。
     * @tparam Equal       [可选] 负责实现相等比较 (`operator==`)。
     * @tparam Compare     [可选] 负责实现其他比较 (`operator<`, `>`, etc.)。若提供，可自动推导 `==`。
     *
     * @note 对于可选的策略，可以使用 `RangeIter_None` 标签来禁用相应的功能。
     */
    template <typename Storage,
              typename Valid, typename Address, typename Dereference,
              typename Increment, typename Decrement, typename Forward, typename Backward,
              typename Distance, typename Equal, typename Compare
    >
    class RangeIter : public Storage, xyu::t_ebo<Valid, 1>, xyu::t_ebo<Address, 2>, xyu::t_ebo<Dereference, 3>, xyu::t_ebo<Increment, 4>, xyu::t_ebo<Decrement, 5>, xyu::t_ebo<Forward, 6>, xyu::t_ebo<Backward, 7>, xyu::t_ebo<Distance, 8>, xyu::t_ebo<Equal, 9>, xyu::t_ebo<Compare, 10>
    {
        static_assert(!xyu::t_is_same<Storage, RangeIter_None>, "Storage Policy cannot be None");
        static_assert(!xyu::t_is_same<Valid, RangeIter_None>, "Valid Policy cannot be None");
        static_assert(!xyu::t_is_same<Dereference, RangeIter_None>, "Dereference Policy cannot be None");

    public:
        /* 构造赋值 */
        using Storage::Storage;

        /// 同 Storage 迭代器 构造
        template <typename... Args>
        constexpr RangeIter(RangeIter<Storage, Args...> other) noexcept(noexcept(Storage{other.self()}))
            : Storage{other.self()} {}

        /// 同 Storage 迭代器 赋值
        template <typename... Args>
        constexpr RangeIter& operator=(RangeIter<Storage, Args...> other) noexcept(noexcept(Storage::operator=(other.self())))
             { Storage::operator=(other.self()); return *this; }

        /* 迭代器操作 */
        /// 判断有效
        constexpr explicit operator bool() const noexcept(noexcept(valid()(self()))) { return valid()(self()); }
        /// 获取对象地址
        template <typename Test = Address, typename = xyu::t_enable<!xyu::t_is_same<Test, RangeIter_None>>>
        constexpr decltype(auto) operator->() const noexcept(noexcept(ptr()(self()))) { return ptr()(self()); }
        /// 解引用
        constexpr decltype(auto) operator*() const noexcept(noexcept(deref()(self()))) { return deref()(self()); }

        /* 迭代器移动 */
        /// 自增
        template <typename Test = Increment, typename = xyu::t_enable<!xyu::t_is_same<Test, RangeIter_None>>>
        constexpr RangeIter& operator++() noexcept(noexcept(inc()(self()))) { inc()(self()); return *this; }
        /// 自增后置
        template <typename Test = Increment, typename = xyu::t_enable<!xyu::t_is_same<Test, RangeIter_None>>>
        constexpr RangeIter operator++(int) noexcept(noexcept(inc()(self()))) { RangeIter r(*this); inc()(self()); return r; }
        /// 自减
        template <typename Test = Decrement, typename = xyu::t_enable<!xyu::t_is_same<Test, RangeIter_None>>>
        constexpr RangeIter& operator--() noexcept(noexcept(dec()(self()))) { dec()(self()); return *this; }
        /// 自减后置
        template <typename Test = Decrement, typename = xyu::t_enable<!xyu::t_is_same<Test, RangeIter_None>>>
        constexpr RangeIter operator--(int) noexcept(noexcept(dec()(self()))) { RangeIter r(*this); dec()(self()); return r; }
        /// 前进
        template <typename Difference, typename Test = Forward, typename = xyu::t_enable<!xyu::t_is_same<Test, RangeIter_None>>>
        constexpr RangeIter& operator+=(Difference n) noexcept(noexcept(next()(self(), n))) { next()(self(), n); return *this; }
        /// 前进新值
        template <typename Difference, typename Test = Forward, typename = xyu::t_enable<!xyu::t_is_same<Test, RangeIter_None>>>
        constexpr RangeIter operator+(Difference n) const noexcept(noexcept(RangeIter(*this) += n)) { return RangeIter(*this) += n; }
        /// 后退
        template <typename Difference, typename Test = Backward, typename = xyu::t_enable<!xyu::t_is_same<Test, RangeIter_None>>>
        constexpr RangeIter& operator-=(Difference n) noexcept(noexcept(back()(self(), n))) { back()(self(), n); return *this; }
        /// 后退新值
        template <typename Difference, typename Test = Backward, typename = xyu::t_enable<!xyu::t_is_same<Test, RangeIter_None>>>
        constexpr RangeIter operator-(Difference n) const noexcept(noexcept(RangeIter(*this) -= n)) { return RangeIter(*this) -= n; }

        /* 迭代器比较 */
        /// 差值
        template <typename Test = Distance, typename = xyu::t_enable<!xyu::t_is_same<Test, RangeIter_None>>>
        constexpr auto operator-(const RangeIter& other) const noexcept(noexcept(dis()(self(), other.self()))) { return dis()(self(), other.self()); }
        /// 相等
        template <typename Test = Equal, xyu::t_enable<!xyu::t_is_same<Test, RangeIter_None>, bool> = true>
        constexpr bool operator==(const RangeIter& other) const noexcept(noexcept(equal()(self(), other.self()))) { return equal()(self(), other.self()); }
        template <typename Test = Equal, xyu::t_enable<xyu::t_is_same<Test, RangeIter_None> && !xyu::t_is_same<Compare, RangeIter_None>, bool> = false>
        constexpr bool operator==(const RangeIter& other) const noexcept(noexcept(cmp()(self(), other.self()))) { return cmp()(self(), other.self()) == 0; }
        /// 不等
        template <typename Test = Equal, xyu::t_enable<!xyu::t_is_same<Test, RangeIter_None>, bool> = true>
        constexpr bool operator!=(const RangeIter& other) const noexcept(noexcept(equal()(self(), other.self()))) { return !equal()(self(), other.self()); }
        template <typename Test = Equal, xyu::t_enable<xyu::t_is_same<Test, RangeIter_None> && !xyu::t_is_same<Compare, RangeIter_None>, bool> = false>
        constexpr bool operator!=(const RangeIter& other) const noexcept(noexcept(cmp()(self(), other.self()))) { return cmp()(self(), other.self()) != 0; }
        /// 小于
        template <typename Test = Compare, typename = xyu::t_enable<!xyu::t_is_same<Test, RangeIter_None>>>
        constexpr bool operator<(const RangeIter& other) const noexcept(noexcept(cmp()(self(), other.self()))) { return cmp()(self(), other.self()) < 0; }
        /// 小于等于
        template <typename Test = Compare, typename = xyu::t_enable<!xyu::t_is_same<Test, RangeIter_None>>>
        constexpr bool operator<=(const RangeIter& other) const noexcept(noexcept(cmp()(self(), other.self()))) { return cmp()(self(), other.self()) <= 0; }
        /// 大于
        template <typename Test = Compare, typename = xyu::t_enable<!xyu::t_is_same<Test, RangeIter_None>>>
        constexpr bool operator>(const RangeIter& other) const noexcept(noexcept(cmp()(self(), other.self()))) { return cmp()(self(), other.self()) > 0; }
        /// 大于等于
        template <typename Test = Compare, typename = xyu::t_enable<!xyu::t_is_same<Test, RangeIter_None>>>
        constexpr bool operator>=(const RangeIter& other) const noexcept(noexcept(cmp()(self(), other.self()))) { return cmp()(self(), other.self()) >= 0; }

    private:
        constexpr RangeIter&    self()  const noexcept { return const_cast<RangeIter&>(*this); }
        constexpr Valid&        valid() const noexcept { return const_cast<RangeIter&>(*this).xyu::t_ebo<Valid,       1>::base(); }
        constexpr Address&      ptr()   const noexcept { return const_cast<RangeIter&>(*this).xyu::t_ebo<Address,     2>::base(); }
        constexpr Dereference&  deref() const noexcept { return const_cast<RangeIter&>(*this).xyu::t_ebo<Dereference, 3>::base(); }
        constexpr Increment&    inc()   const noexcept { return const_cast<RangeIter&>(*this).xyu::t_ebo<Increment,   4>::base(); }
        constexpr Decrement&    dec()   const noexcept { return const_cast<RangeIter&>(*this).xyu::t_ebo<Decrement,   5>::base(); }
        constexpr Forward&      next()  const noexcept { return const_cast<RangeIter&>(*this).xyu::t_ebo<Forward,     6>::base(); }
        constexpr Backward&     back()  const noexcept { return const_cast<RangeIter&>(*this).xyu::t_ebo<Backward,    7>::base(); }
        constexpr Distance&     dis()   const noexcept { return const_cast<RangeIter&>(*this).xyu::t_ebo<Distance,    8>::base(); }
        constexpr Equal&        equal() const noexcept { return const_cast<RangeIter&>(*this).xyu::t_ebo<Equal,       9>::base(); }
        constexpr Compare&      cmp()   const noexcept { return const_cast<RangeIter&>(*this).xyu::t_ebo<Compare,    10>::base(); }

    public:
        using Storage_t     = Storage;
        using Valid_t       = Valid;
        using Address_t     = Address;
        using Dereference_t = Dereference;
        using Increment_t   = Increment;
        using Decrement_t   = Decrement;
        using Forward_t     = Forward;
        using Backward_t    = Backward;
        using Distance_t    = Distance;
        using Equal_t       = Equal;
        using Compare_t     = Compare;

        // 友元，为支持同类型 Storage 迭代器构造赋值
        template <typename S, typename V, typename A, typename D, typename I, typename Dc, typename F, typename B, typename Ds, typename E, typename C>
        friend class RangeIter;
    };
}

/// 迭代器部件标志
namespace xylu::xyrange
{
    namespace __ { struct it_tag {}; }

    // 迭代器 - 部件标志 - 存储
    constexpr struct RangeIter_Storage_t    : xyu::size_c<0>, __::it_tag {} RangeIter_Storage_v;
    // 迭代器 - 部件标志 - 有效
    constexpr struct RangeIter_Valid_t      : xyu::size_c<1>, __::it_tag {} RangeIter_Valid_v;
    // 迭代器 - 部件标志 - 获取对象地址
    constexpr struct RangeIter_Address_t    : xyu::size_c<2>, __::it_tag {} RangeIter_Address_v;
    // 迭代器 - 部件标志 - 解引用
    constexpr struct RangeIter_Dereference_t: xyu::size_c<3>, __::it_tag {} RangeIter_Dereference_v;
    // 迭代器 - 部件标志 - 自增
    constexpr struct RangeIter_Increment_t  : xyu::size_c<4>, __::it_tag {} RangeIter_Increment_v;
    // 迭代器 - 部件标志 - 自减
    constexpr struct RangeIter_Decrement_t  : xyu::size_c<5>, __::it_tag {} RangeIter_Decrement_v;
    // 迭代器 - 部件标志 - 前进
    constexpr struct RangeIter_Forward_t    : xyu::size_c<6>, __::it_tag {} RangeIter_Forward_v;
    // 迭代器 - 部件标志 - 后退
    constexpr struct RangeIter_Backward_t   : xyu::size_c<7>, __::it_tag {} RangeIter_Backward_v;
    // 迭代器 - 部件标志 - 差值
    constexpr struct RangeIter_Distance_t   : xyu::size_c<8>, __::it_tag {} RangeIter_Distance_v;
    // 迭代器 - 部件标志 - 相等
    constexpr struct RangeIter_Equal_t      : xyu::size_c<9>, __::it_tag {} RangeIter_Equal_v;
    // 迭代器 - 部件标志 - 比较
    constexpr struct RangeIter_Compare_t    : xyu::size_c<10>,__::it_tag {} RangeIter_Compare_v;
}

/// 迭代器属性
namespace xylu::xytraits
{
    namespace __
    {
        template <typename T>
        struct is_iterator : false_c {};
        template <typename... Args>
        struct is_iterator<xylu::xyrange::RangeIter<Args...>> : true_c {};
    }
    /// 判断是否为迭代器
    template <typename... T>
    constexpr bool t_is_iterator = (... && __::is_iterator<t_remove_cvref<T>>::value);

    /// 判断是否为单向迭代器
    template <typename... T>
    constexpr bool t_is_iter_single = [](){
        if constexpr (!t_is_iterator<T...>) return false;
        else return (... && !t_is_same<typename t_remove_cvref<T>::Increment_t, xylu::xyrange::RangeIter_None>);
    }();

    /// 判断是否为双向迭代器
    template <typename... T>
    constexpr bool t_is_iter_double = [](){
        if constexpr (!t_is_iter_single<T...>) return false;
        else return (... && !t_is_same<typename t_remove_cvref<T>::Decrement_t, xylu::xyrange::RangeIter_None>);
    }();

    /// 判断是否为随机迭代器
    template <typename... T>
    constexpr bool t_is_iter_random = [](){
        if constexpr (!t_is_iter_double<T...>) return false;
        else return (... && !t_is_same<typename t_remove_cvref<T>::Forward_t, xylu::xyrange::RangeIter_None>) &&
                    (... && !t_is_same<typename t_remove_cvref<T>::Backward_t, xylu::xyrange::RangeIter_None>);
    }();

    /// 判断是否为迭代器部件标签
    template <typename... T>
    constexpr bool t_is_iter_tag = (... && t_is_base_of<xylu::xyrange::__::it_tag, t_remove_cvref<T>>);
}

/// 迭代器修改
namespace xylu::xyrange::__
{//(防止污染 xylu::xytraits::__ 内部命名空间)

    // 解包模板参数 (如果 Origin 为 RangeIter_None，则仍返回 RangeIter_None)
    template <typename Change, typename Origin>
    struct unpack_iter_base { using type = xyu::t_unpack_tmpl<Change, Origin>; };
    template <typename Change>
    struct unpack_iter_base<Change, xylu::xyrange::RangeIter_None> { using type = RangeIter_None; };
    template <typename Change, typename Origin>
    using unpack_iter = typename unpack_iter_base<Change, Origin>::type;

    // 获取替换后的 Iter 类型
    template <typename Iter, typename Tag, typename Change>
    struct get_new_iter_impl
    {
        static constexpr auto index = Tag::value;
        static_assert(index <= xylu::xyrange::RangeIter_Compare_t::value, "unknown tag index");

        using oTps = xyu::typelist_c<typename Iter::Storage_t,
                typename Iter::Valid_t, typename Iter::Address_t, typename Iter::Dereference_t,
                typename Iter::Increment_t, typename Iter::Decrement_t, typename Iter::Forward_t, typename Iter::Backward_t,
                typename Iter::Distance_t, typename Iter::Equal_t, typename Iter::Compare_t>;

        using part = unpack_iter<Change, xyu::t_args_get<oTps, index>>;
        using nTps = xyu::t_args_set<oTps, index, part>;
        using type = xyu::t_unpack_tmpl<Iter, nTps>;
    };
    template <typename Iter, typename Tag, typename Change>
    using get_new_iter = typename get_new_iter_impl<Iter, Tag, Change>::type;

    // 依次替换每一个组件
    template <typename, typename Iter, typename... Arg>
    struct change_iter_base { static_assert(sizeof...(Arg) == 0, "invalid arguments"); };
    template <typename Iter>
    struct change_iter_base<void, Iter> { using type = Iter; };
    template <typename Iter, typename Tps, typename... Args>
    struct change_iter_base<xyu::t_enable<xyu::t_is_typelist<Tps>>, Iter, Tps, Args...>
    {
        static_assert(xyu::t_args_count<Tps> >= 2, "too few arguments for typelist_c");
        using Tag = xyu::t_args_get<Tps, 0>;
        using Change = xyu::t_args_get<Tps, 1>;
        using new_iter = get_new_iter<Iter, Tag, Change>;
        using type = typename change_iter_base<void, new_iter, Args...>::type;
    };
    template <typename Iter, typename Tag, typename Change, typename... Args>
    struct change_iter_base<xyu::t_enable<!xyu::t_is_typelist<Tag>>, Iter, Tag, Change, Args...>
    {
        using new_iter = get_new_iter<Iter, Tag, Change>;
        using type = typename change_iter_base<void, new_iter, Args...>::type;
    };
    
    template <typename Iter, typename... Args>
    struct change_iter
    {
        static_assert(xyu::t_is_iterator<Iter>);
        using type = typename change_iter_base<void, Iter, Args...>::type;
    };
}
namespace xylu::xytraits
{
    /**
     * @brief 在编译期修改一个 `RangeIter` 类型的策略组合，生成一个新的迭代器类型。
     * @details
     *   此工具允许你取一个现有的 `RangeIter` 类型，并替换掉它的一个或多个策略组件。
     *   Args 参数是一系列“标签-策略”对。
     *
     * @tparam Iter 原始的 `RangeIter` 类型。
     * @tparam Args 一系列的修改指令，每个指令是一个 {Tag, Change}对 或 typelist_c˂Tag, Change˃。
     *
     * ### 修改规则:
     * - `Tag`: 一个迭代器部件标志，如 `RangeIter_Compare_t`。
     * - `Change`: 新的策略类。如果 `Change` 是一个“包装后的模板”(通过 `t_park_tmpl` 包装)，
     *             则它会被应用于原始策略上，形成一个新的复合策略。
     *
     * @example
     *   // 将 Iter 的比较策略替换为反向比较
     *   using NewIter = t_change_iter˂Iter,
     *                                 RangeIter_Compare_t,
     *                                 t_park_tmpl˂RangeIter_Compare_Reverse˃˃;
     *   // NewIter 的比较策略将是 RangeIter_Compare_Reverse˂typename Iter::Compare_t˃
     */
    template <typename Iter, typename... Args>
    using t_change_iter = typename xylu::xyrange::__::change_iter<t_remove_cvref<Iter>, t_remove_cvref<Args>...>::type;

    namespace __
    {
        // 移除组件
        template <typename Iter, typename... Tags>
        struct remove_iter_base { using type = Iter; };
        template <typename Iter, typename Tag, typename... Tags>
        struct remove_iter_base<Iter, Tag, Tags...>
        {
            static_assert(t_is_iter_tag<Tag>);
            static_assert(!t_is_same<Tag, xylu::xyrange::RangeIter_Storage_t>, "Cannot remove Storage_t");
            static_assert(!t_is_same<Tag, xylu::xyrange::RangeIter_Valid_t>, "Cannot remove Valid_t");
            static_assert(!t_is_same<Tag, xylu::xyrange::RangeIter_Dereference_t>, "Cannot remove Dereference_t");
            using new_iter = t_change_iter<Iter, Tag, xylu::xyrange::RangeIter_None>;
            using type = typename remove_iter_base<new_iter, Tags...>::type;
        };
    }
    /**
     * @brief 从一个 `RangeIter` 类型中移除一个或多个策略组件。
     * @details 通过将指定的策略替换为 `RangeIter_None` 来禁用某些功能。
     * @tparam Iter 原始的 `RangeIter` 类型。
     * @tparam Tags 要移除的策略组件的标签，如 `RangeIter_Decrement_t`。
     * @note 不能移除 `Storage_t`, `Valid_t`, `Dereference_t` 这三个必需的策略。
     */
    template <typename Iter, typename... Tags>
    using t_remove_iter = typename __::remove_iter_base<t_remove_cvref<Iter>, t_remove_cvref<Tags>...>::type;
}

/** 常用迭代器 */
namespace xylu::xyrange
{
    /**
     * @brief 一个模拟原生指针行为的随机访问迭代器。
     * @tparam T 迭代器(指针)指向的元素类型。
     */
    template <typename T>
    using RangeIter_Ptr = RangeIter<RangeIter_Storage_Ptr<T>,
            RangeIter_Valid_Native, RangeIter_Address_Native, RangeIter_Dereference_Native,
            RangeIter_Increment_Native, RangeIter_Decrement_Native, RangeIter_Forward_Native, RangeIter_Backward_Native,
            RangeIter_Distance_Native, RangeIter_Equal_Native, RangeIter_Compare_Subtract>;

    /**
     * @brief 用于遍历链表等节点式数据结构的迭代器。
     * @tparam Node 节点类型，应包含 val, next, (若双向) prev 成员。
     * @tparam double_ended 若为 true，则提供双向遍历能力。
     */
    template <typename Node, bool double_ended>
    using RangeIter_NodePtr = RangeIter<RangeIter_Storage_Ptr<Node>,
            RangeIter_Valid_Native, RangeIter_Address_Deref, RangeIter_Dereference_Node,
            RangeIter_Increment_Node, xyu::t_cond<double_ended, RangeIter_Decrement_Node, RangeIter_None>,
            RangeIter_None, RangeIter_None, RangeIter_None, RangeIter_Equal_Native, RangeIter_None>;

    /**
     * @brief 用于遍历二叉树等树形数据结构的双向迭代器。
     * @tparam TreeNode 节点类型，应包含 val, left, right, up 成员。
     */
    template <typename TreeNode>
    using RangeIter_TreePtr = RangeIter<RangeIter_Storage_Ptr<TreeNode>,
            RangeIter_Valid_Native, RangeIter_Address_Deref, RangeIter_Dereference_Native,
            RangeIter_Increment_TreeNode, RangeIter_Decrement_TreeNode, RangeIter_None, RangeIter_None,
            RangeIter_None, RangeIter_Equal_Native, RangeIter_None>;

    /**
     * @brief 将一个迭代器转换为其对应的常量迭代器。
     * @details 修改了解引用和地址获取策略，使其返回 const 限定的引用和指针。
     * @tparam Iter 原始迭代器类型。
     */
    template <typename Iter>
    using RangeIter_Const = xyu::t_change_iter<Iter,
            RangeIter_Address_t, xyu::t_park_tmpl<RangeIter_Address_Const>,
            RangeIter_Dereference_t, xyu::t_park_tmpl<RangeIter_Dereference_Const>>;

    /**
     * @brief 将一个迭代器转换为移动迭代器。
     * @details 修改了解引用策略，使其在解引用时返回右值引用，用于移动语义。
     * @tparam Iter 原始迭代器类型。
     */
    template <typename Iter>
    using RangeIter_Move = xyu::t_change_iter<Iter,
            RangeIter_Dereference_t, xyu::t_park_tmpl<RangeIter_Dereference_Move>>;

    /**
     * @brief 将一个双向或随机访问迭代器转换为其对应的反向迭代器。
     * @details 通过交换自增/自减、前进/后退等策略，并反转比较和距离计算逻辑来实现。
     * @tparam Iter 原始迭代器类型。
     */
    template <typename Iter>
    using RangeIter_Reverse = RangeIter<typename Iter::Storage_t,
            typename Iter::Valid_t, typename Iter::Address_t,
            RangeIter_Dereference_Reverse<typename Iter::Dereference_t>,
            typename Iter::Decrement_t, typename Iter::Increment_t,
            typename Iter::Backward_t, typename Iter::Forward_t,
            RangeIter_Distance_Reverse<typename Iter::Distance_t>,
            RangeIter_Equal_Native, RangeIter_Compare_Reverse<typename Iter::Compare_t>>;
}

/** 迭代器迭代器 */
namespace xylu::xyrange
{
    namespace __
    {
        //测试迭代器操作
        template <typename Iter>
        decltype(xyu::t_val<Iter>().operator->(), RangeIter_Address_Iter{}) iter_has_address_test(int);
        template <typename>
        RangeIter_None iter_has_address_test(...);

        template <typename Iter>
        decltype(xyu::t_val<Iter>()++, RangeIter_Increment_Native{}) iter_has_increment_test(int);
        template <typename>
        RangeIter_None iter_has_increment_test(...);

        template <typename Iter>
        decltype(xyu::t_val<Iter>()--, RangeIter_Decrement_Native{}) iter_has_decrement_test(int);
        template <typename>
        RangeIter_None iter_has_decrement_test(...);

        template <typename Iter>
        decltype(xyu::t_val<Iter>()+=1, RangeIter_Increment_Native{}) iter_has_forward_test(int);
        template <typename>
        RangeIter_None iter_has_forward_test(...);

        template <typename Iter>
        decltype(xyu::t_val<Iter>()-=1, RangeIter_Decrement_Native{}) iter_has_backward_test(int);
        template <typename >
        RangeIter_None iter_has_backward_test(...);

        template <typename Iter>
        decltype(xyu::t_val<Iter>()-xyu::t_val<Iter>(), RangeIter_Distance_Native{}) iter_has_distance_test(int);
        template <typename>
        RangeIter_None iter_has_distance_test(...);

        template <typename Iter>
        decltype(xyu::t_val<Iter>()==xyu::t_val<Iter>(), RangeIter_Equal_Native{}) iter_has_equal_test(int);
        template <typename>
        RangeIter_None iter_has_equal_test(...);

        template <typename Iter>
        decltype(xyu::t_val<Iter>()-xyu::t_val<Iter>(), RangeIter_Compare_Subtract{}) iter_has_compare_test(int);
        template <typename>
        RangeIter_None iter_has_compare_test(...);
    }
    /**
     * @brief 将任意符合 C++ 标准的迭代器封装成 `RangeIter`。
     * @details
     *   此适配器通过 SFINAE 检测传入的 `Iter` 类型支持哪些操作（`++`, `--`, `==`, 等），
     *   并自动为其匹配合适的策略组件，从而将其无缝接入 `RangeIter` 框架。
     * @tparam Iter 任意有效的迭代器类型，不要求是 `RangeIter` 的特化。
     */
    template <typename Iter>
    using RangeIter_Iter = RangeIter<RangeIter_Storage_Iter<Iter>,
            RangeIter_Valid_Native, RangeIter_Address_Iter, RangeIter_Dereference_Native,
            decltype(__::iter_has_increment_test<Iter>(0)), decltype(__::iter_has_decrement_test<Iter>(0)),
            decltype(__::iter_has_forward_test<Iter>(0)), decltype(__::iter_has_backward_test<Iter>(0)),
            decltype(__::iter_has_distance_test<Iter>(0)), decltype(__::iter_has_equal_test<Iter>(0)),
            decltype(__::iter_has_compare_test<Iter>(0))>;
}

#pragma clang diagnostic pop