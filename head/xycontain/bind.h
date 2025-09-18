#pragma clang diagnostic push
#pragma ide diagnostic ignored "OCUnusedGlobalDeclarationInspection"
#pragma once

#include "../../link/tuple"

/// 前后绑定
namespace xylu::xycontain
{
    /**
     * @brief 一个高性能的函数绑定对象，用于将参数绑定到可调用对象的头部或尾部。
     *
     * @tparam front  一个布尔值，`true` 表示前向绑定 (类似 std::bind_front)，
     *                `false` 表示后向绑定。
     * @tparam Functor 存储的可调用对象的类型 (通常是退化后的类型)。
     * @tparam BArgs... 存储的绑定参数的类型包 (通常是退化后的类型)。
     *
     * @details
     * `Bind` 是一个轻量级的、`constexpr`-友好的函数包装器。它将一个可调用对象和
     * 一系列参数存储在一起。当 `Bind` 对象被调用时，它会将存储的参数与调用时
     * 提供的新参数组合起来，然后调用原始的可调用对象。
     *
     * 相比 `std::bind`，它具有以下优势：
     * - **API 更简洁：** 通过 `bool front` 模板参数统一了前向和后向绑定。
     * - **类型安全：** 所有类型检查都在编译期完成。
     * - **高性能：** 内部使用 `xyu::Tuple` 存储参数，利用了空基类优化 (EBO)。
     * - **完美转发：** 对 `operator()` 进行了四种引用限定的重载，确保了值类别的正确传递。
     *
     * @note 通常不直接使用此类，而是通过便捷的工厂函数 `make_bind_front` 和 `make_bind_back`
     *       或统一的 `make_bind` 来创建。
     */
    template <bool front, typename Functor, typename... BArgs>
    struct Bind
    {
    private:
        Functor func;           // 类函数
        Tuple<BArgs...> bargs;  // 绑定的参数

    public:
        /// 构造函数
        template <typename Func, typename... Args>
        constexpr explicit Bind(Func&& func, Args&&... args) noexcept(xyu::t_can_nothrow_listinit<Functor, Func> && xyu::t_can_nothrow_listinit<Tuple<BArgs...>, Args...>)
            : func{xyu::forward<Func>(func)}, bargs{xyu::forward<Args>(args)...} {}

        /// 调用 (左值)
        template <typename... Args>
        constexpr decltype(auto) operator()(Args&&... args) & noexcept(noexcept(call_help(*this, bind_indexs, xyu::forward<Args>(args)...)))
        { return call_help(*this, bind_indexs, xyu::forward<Args>(args)...); }
        /// 调用 (const左值)
        template <typename... Args>
        constexpr decltype(auto) operator()(Args&&... args) const& noexcept(noexcept(call_help(*this, bind_indexs, xyu::forward<Args>(args)...)))
        { return call_help(*this, bind_indexs, xyu::forward<Args>(args)...); }
        /// 调用 (右值)
        template <typename... Args>
        constexpr decltype(auto) operator()(Args&&... args) && noexcept(noexcept(call_help(xyu::move(*this), bind_indexs, xyu::forward<Args>(args)...)))
        { return call_help(xyu::move(*this), bind_indexs, xyu::forward<Args>(args)...); }
        /// 调用 (const右值)
        template <typename... Args>
        constexpr decltype(auto) operator()(Args&&... args) const&& noexcept(noexcept(call_help(xyu::move(*this), bind_indexs, xyu::forward<Args>(args)...)))
        { return call_help(xyu::move(*this), bind_indexs, xyu::forward<Args>(args)...); }

    private:
        // 绑定索引序列
        static constexpr auto bind_indexs = xyu::make_isequence<sizeof...(BArgs)>();

        // 调用辅助函数
        template <typename Bind, typename... Args, xyu::size_t... indexs>
        static constexpr decltype(auto) call_help(Bind&& bind, xyu::isequence_c<indexs...>, Args&&... args)
        noexcept((front && xyu::t_can_nothrow_call<xyu::t_sync<Bind, Functor>, xyu::t_sync<Bind, BArgs>..., Args...>) || (!front && xyu::t_can_nothrow_call<xyu::t_sync<Bind, Functor>, Args..., xyu::t_sync<Bind, BArgs>...>))
        {
            if constexpr (front)
                return xyu::forward<Bind>(bind).func(
                        xyu::forward<Bind>(bind).bargs.template get<indexs>()...,
                        xyu::forward<Args>(args)...);
            else
                return xyu::forward<Bind>(bind).func(
                        xyu::forward<Args>(args)...,
                        xyu::forward<Bind>(bind).bargs.template get<indexs>()...);
        }
    };

    // 推导指引
    template <typename Functor, typename... BArgs>
    Bind(Functor&&, BArgs&&...) -> Bind<true, xyu::t_decay<Functor>, xyu::t_decay<BArgs>...>;

    /**
     * @brief 创建绑定对象 (绑定到前面)
     * @param func 待绑定的函数 (去除cvref的退化类型)
     * @param args 待绑定的参数 (去除cvref的退化类型)
     * @note 需要传递引用类型可以用 xyu::make_refer(arg)
     */
    template <typename Function, typename... BArgs>
    constexpr auto make_bind_front(Function&& func, BArgs&&... args)
    noexcept(noexcept(Bind<true, xyu::t_decay<Function>, xyu::t_decay<BArgs>...>{xyu::forward<Function>(func), xyu::forward<BArgs>(args)...}))
    { return Bind<true, xyu::t_decay<Function>, xyu::t_decay<BArgs>...>{xyu::forward<Function>(func), xyu::forward<BArgs>(args)...}; }

    /**
     * @brief 创建绑定对象 (绑定到后面)
     * @param func 待绑定的函数 (去除cvref的退化类型)
     * @param args 待绑定的参数 (去除cvref的退化类型)
     * @note 需要传递引用类型可以用 xyu::make_refer(arg)
     */
    template <typename Function, typename... BArgs>
    constexpr auto make_bind_back(Function&& func, BArgs&&... args)
    noexcept(noexcept(Bind<false, xyu::t_decay<Function>, xyu::t_decay<BArgs>...>{xyu::forward<Function>(func), xyu::forward<BArgs>(args)...}))
    { return Bind<false, xyu::t_decay<Function>, xyu::t_decay<BArgs>...>{xyu::forward<Function>(func), xyu::forward<BArgs>(args)...}; }
}

/// 随机绑定
namespace xylu::xycontain
{
    /**
     * @brief 一个功能极其强大的函数绑定对象，允许将参数绑定到调用签名的任意位置。
     *
     * @tparam BIndexs... 一个编译期整数序列，指定了每个绑定参数在最终调用中的位置索引。
     * @tparam Functor    存储的可调用对象的类型。
     * @tparam BArgs...   存储的绑定参数的类型包。
     *
     * @details
     * `Bind_Any` 是对传统 `std::bind` 的一次彻底超越。它不使用运行时占位符，而是通过
     * 编译期模板参数 `BIndexs...` 来精确地、静态地定义绑定参数的布局。
     *
     * ### 核心特性 (Key Features):
     *
     * 1.  **编译期索引绑定：** 通过模板非类型参数 `template <size_t... BIndexs>` 来指定
     *     绑定位置，例如 `make_bind_any<2, 0>(f, arg1, arg2)` 表示将 `arg1` 绑定到
     *     位置2，`arg2` 绑定到位置0。
     *
     * 2.  **编译期自动排序：** 即使用户提供的绑定索引是无序的（如 `<2, 0>`），`make_bind_any`
     *     也会在编译期通过元编程对索引进行排序，并相应地调整内部参数的存储顺序，
     *     从而保证高效的运行时调用。
     *
     * 3.  **零开销调用：** 所有的参数“编织”逻辑都在编译期完成。运行时的 `operator()`
     *     调用几乎等同于一次直接的函数调用，没有任何占位符解析或动态决策的开销。
     *
     * 4.  **支持末尾相对索引：** 可以使用非常大的 `size_t` 值，或**负数**（如 `-1`, `-2`），
     *     来表示相对于调用时参数列表末尾的位置。例如，`-1` 表示绑定到最后一个位置。
     *
     * @note 通常不直接使用此类，而是通过便捷的工厂函数 `make_bind_any` 或统一的
     *       `make_bind` 来创建。
     */
    template <typename BindIndexs, typename Functor, typename... BArgs>
    class Bind_Any;

    template <xyu::size_t... BIndexs, typename Functor, typename... BArgs>
    class Bind_Any<xyu::isequence_c<BIndexs...>, Functor, BArgs...>
    {
        static_assert(sizeof...(BArgs) == sizeof...(BIndexs));

    private:
        Functor func;           // 类函数
        Tuple<BArgs...> bargs;  // 绑定的参数

    public:
        /// 构造函数
        template <typename Func, typename... Args>
        constexpr explicit Bind_Any(Func&& func, Args&&... args) noexcept(xyu::t_can_nothrow_listinit<Functor, Func> && xyu::t_can_nothrow_listinit<Tuple<BArgs...>, Args...>)
            : func{xyu::forward<Func>(func)}, bargs{xyu::forward<Args>(args)...} {}

        /// 调用 (左值)
        template <typename... Args>
        constexpr decltype(auto) operator()(Args&&... args) & noexcept(noexcept(call_help(*this, xyu::forward<Args>(args)...)))
        { return call_help(*this, xyu::forward<Args>(args)...); }
        /// 调用 (const左值)
        template <typename... Args>
        constexpr decltype(auto) operator()(Args&&... args) const& noexcept(noexcept(call_help(*this, xyu::forward<Args>(args)...)))
        { return call_help(*this, xyu::forward<Args>(args)...); }
        /// 调用 (右值)
        template <typename... Args>
        constexpr decltype(auto) operator()(Args&&... args) && noexcept(noexcept(call_help(xyu::move(*this), xyu::forward<Args>(args)...)))
        { return call_help(xyu::move(*this), xyu::forward<Args>(args)...); }
        /// 调用 (const右值)
        template <typename... Args>
        constexpr decltype(auto) operator()(Args&&... args) const&& noexcept(noexcept(call_help(xyu::move(*this), xyu::forward<Args>(args)...)))
        { return call_help(xyu::move(*this), xyu::forward<Args>(args)...); }

    private:
        // 调用辅助函数
        template <typename Bind, typename... Args>
        static constexpr decltype(auto) call_help(Bind&& bind, Args&&... args)
        noexcept(noexcept(call_impl<0, 0, 0>(xyu::forward<Bind>(bind), xyu::isequence_c<BIndexs...>(), xyu::t_val<Tuple<xyu::t_remove_cvref<Args>...>&>())))
        {
            auto tp = xyu::make_tuple(xyu::forward<Args>(args)...);
            return call_impl<0, 0, 0>(xyu::forward<Bind>(bind), xyu::isequence_c<BIndexs...>{}, tp);
        }

        // 双参数包 - 使用绑定参数
        template <xyu::size_t cIndex, xyu::size_t bIndex, xyu::size_t tIndex, xyu::size_t bIdx, xyu::size_t... bIdxs,
                  typename Bind, typename Tp, typename... Args,
                  xyu::t_enable<(tIndex < Tp::count() && cIndex == bIdx), bool> = true>
        static constexpr decltype(auto) call_impl(Bind&& bind, xyu::isequence_c<bIdx, bIdxs...>, Tp& tp, Args&&... args)
        noexcept(noexcept(call_impl<cIndex + 1, bIndex + 1, tIndex>(xyu::forward<Bind>(bind), xyu::isequence_c<bIdxs...>{}, tp, xyu::forward<Args>(args)..., xyu::forward<Bind>(bind).bargs.template get<bIndex>())))
        {
            return call_impl<cIndex + 1, bIndex + 1, tIndex>(
                    xyu::forward<Bind>(bind), xyu::isequence_c<bIdxs...>{}, tp,
                    xyu::forward<Args>(args)..., xyu::forward<Bind>(bind).bargs.template get<bIndex>());
        }

        // 双参数包 - 使用传递参数
        template <xyu::size_t cIndex, xyu::size_t bIndex, xyu::size_t tIndex, xyu::size_t bIdx, xyu::size_t... bIdxs,
                  typename Bind, typename Tp, typename... Args,
                  xyu::t_enable<(tIndex < Tp::count() && cIndex != bIdx), bool> = true>
        static constexpr decltype(auto) call_impl(Bind&& bind, xyu::isequence_c<bIdx, bIdxs...>, Tp& tp, Args&&... args)
        noexcept(noexcept(call_impl<cIndex + 1, bIndex, tIndex + 1>(xyu::forward<Bind>(bind), xyu::isequence_c<bIdx, bIdxs...>{}, tp, xyu::forward<Args>(args)..., tp.template get<tIndex>())))
        {
            return call_impl<cIndex + 1, bIndex, tIndex + 1>(
                    xyu::forward<Bind>(bind), xyu::isequence_c<bIdx, bIdxs...>{}, tp,
                    xyu::forward<Args>(args)..., tp.template get<tIndex>());
        }

        // 单参数包 - 使用绑定参数
        template <xyu::size_t cIndex, xyu::size_t bIndex, xyu::size_t tIndex, xyu::size_t... bIdxs,
                  typename Bind, typename Tp, typename... Args,
                  xyu::t_enable<(tIndex >= Tp::count()), bool> = false>
        static constexpr decltype(auto) call_impl(Bind&& bind, xyu::isequence_c<bIdxs...>, Tp&, Args&&... args)
        noexcept(noexcept(call_tmpl_bargs(xyu::forward<Bind>(bind), xyu::make_isequence<bIndex, sizeof...(BArgs)>(), xyu::forward<Args>(args)...)))
        {
            return call_tmpl_bargs(xyu::forward<Bind>(bind), xyu::make_isequence<bIndex, sizeof...(BArgs)>(),
                                   xyu::forward<Args>(args)...);
        };

        // 单参数包 - 使用传递参数
        template <xyu::size_t cIndex, xyu::size_t bIndex, xyu::size_t tIndex,
                  typename Bind, typename Tp, typename... Args>
        static constexpr decltype(auto) call_impl(Bind&& bind, xyu::isequence_c<>, Tp& tp, Args&&... args)
        noexcept(noexcept(call_tmpl_args(xyu::forward<Bind>(bind), xyu::make_isequence<tIndex, Tp::count()>(), tp, xyu::forward<Args>(args)...)))
        {
            return call_tmpl_args(xyu::forward<Bind>(bind), xyu::make_isequence<tIndex, Tp::count()>(),
                                  tp, xyu::forward<Args>(args)...);
        }

        // 调用 - 剩余绑定参数
        template <typename Bind, xyu::size_t... bIndexs, typename... Args>
        static constexpr decltype(auto) call_tmpl_bargs(Bind&& bind, xyu::isequence_c<bIndexs...>, Args&&... args)
        noexcept(noexcept(xyu::forward<Bind>(bind).func(xyu::forward<Args>(args)..., xyu::forward<Bind>(bind).bargs.template get<bIndexs>()...)))
        {
            return xyu::forward<Bind>(bind).func(
                    xyu::forward<Args>(args)...,
                    xyu::forward<Bind>(bind).bargs.template get<bIndexs>()...);
        }

        // 调用 - 剩余传递参数
        template <typename Bind, typename Tp, xyu::size_t... tIndexs, typename... Args>
        static constexpr decltype(auto) call_tmpl_args(Bind&& bind, xyu::isequence_c<tIndexs...>, Tp& tp, Args&&... args)
        noexcept(noexcept(xyu::forward<Bind>(bind).func(xyu::forward<Args>(args)..., tp.template get<tIndexs>()...)))
        {
            return xyu::forward<Bind>(bind).func(
                    xyu::forward<Args>(args)...,
                    tp.template get<tIndexs>()...);
        }
    };

    namespace __
    {
        // 递增检测 (-1: 有重复索引, 0: 非递增, 1: 递增)
        template <xyu::size_t... Idxs>
        struct bind_index_check_impl;
        template <xyu::size_t Idx>
        struct bind_index_check_impl<Idx> : xyu::int_c<1> {};
        template <xyu::size_t Idx, xyu::size_t Idx2, xyu::size_t... Idxs>
        struct bind_index_check_impl<Idx, Idx2, Idxs...>
        {
            static constexpr int value = (Idx == Idx2 ? -1 :
                                         (Idx < Idx2 ? bind_index_check_impl<Idx2, Idxs...>::value : 0));
        };
        template <xyu::size_t... Idxs>
        constexpr int bind_index_check = bind_index_check_impl<Idxs...>::value;

        // 找出最小值的索引与值
        template <xyu::size_t mIdx, xyu::size_t mVal, xyu::size_t cIdx, xyu::size_t... Vals>
        struct bind_min_index_impl;
        template <xyu::size_t mIdx, xyu::size_t mVal, xyu::size_t cIdx>
        struct bind_min_index_impl<mIdx, mVal, cIdx>
        {
            static constexpr xyu::size_t index = mIdx;
            static constexpr xyu::size_t value = mVal;
        };
        template <xyu::size_t mIdx, xyu::size_t mVal, xyu::size_t cIdx, xyu::size_t cVal, xyu::size_t... Vals>
        struct bind_min_index_impl<mIdx, mVal, cIdx, cVal, Vals...>
        {
            static constexpr xyu::size_t index = mVal <= cVal ?
                    bind_min_index_impl<mIdx, mVal, cIdx + 1, Vals...>::index :
                    bind_min_index_impl<cIdx, cVal, cIdx + 1, Vals...>::index ;
            static constexpr xyu::size_t value = mVal <= cVal ?
                    bind_min_index_impl<mIdx, mVal, cIdx + 1, Vals...>::value :
                    bind_min_index_impl<cIdx, cVal, cIdx + 1, Vals...>::value ;
        };
        template <xyu::size_t Val, xyu::size_t... Vals>
        constexpr xyu::size_t bind_min_index = bind_min_index_impl<0, Val, 1, Vals...>::index;
        template <xyu::size_t Val, xyu::size_t... Vals>
        constexpr xyu::size_t bind_min_value = bind_min_index_impl<0, Val, 1, Vals...>::value;

        // 交换元组的两个索引元素
        template <xyu::size_t Idx1, xyu::size_t Idx2, xyu::size_t... lIdxs, xyu::size_t... mIdxs, xyu::size_t... rIdxs, typename... Args>
        constexpr auto bind_swap_tuple_impl(Tuple<Args...>& tp, xyu::isequence_c<lIdxs...>, xyu::isequence_c<mIdxs...>, xyu::isequence_c<rIdxs...>)
        noexcept(noexcept(xyu::make_tuple(tp.template get<lIdxs>()..., tp.template get<Idx2>(), tp.template get<mIdxs>()..., tp.template get<Idx1>(), tp.template get<rIdxs>()...)))
        {
            return xyu::make_tuple(tp.template get<lIdxs>()..., tp.template get<Idx2>(),
                                   tp.template get<mIdxs>()..., tp.template get<Idx1>(),
                                   tp.template get<rIdxs>()...);
        }
        template <xyu::size_t Idx1, xyu::size_t Idx2, typename... Args>
        constexpr auto bind_swap_tuple(Tuple<Args...>& tp)
        noexcept(noexcept(bind_swap_tuple_impl<Idx1, Idx2>(tp, xyu::make_isequence<0, Idx1>(), xyu::make_isequence<Idx1 + 1, Idx2>(), xyu::make_isequence<xyu::min(Idx2 + 1, sizeof...(Args)), sizeof...(Args)>())))
        {
            return bind_swap_tuple_impl<Idx1, Idx2>(tp, xyu::make_isequence<0, Idx1>(),
                    xyu::make_isequence<Idx1 + 1, Idx2>(), xyu::make_isequence<xyu::min(Idx2 + 1, sizeof...(Args)), sizeof...(Args)>());
        }

        // make_bind_any_impl 异常检测
        template <typename Functor, typename... BArgs, xyu::size_t... sIdxs, xyu::size_t cIdx, xyu::size_t... cIdxs>
        constexpr auto make_bind_any_impl_noexcept(xyu::isequence_c<sIdxs...>, xyu::isequence_c<cIdx, cIdxs...>) noexcept
        {
            constexpr xyu::size_t mi = bind_min_index<cIdx, cIdxs...>;
            using Tp = Tuple<xyu::t_decay<BArgs>...>;
            // 已经有序
            if constexpr (mi == 0)
                return noexcept(make_bind_any_impl(xyu::isequence_c<sIdxs..., cIdx>{}, xyu::isequence_c<cIdxs...>{},
                                                   xyu::t_val<Functor>(), xyu::t_val<Tp>()));
            // 将未排序中的最小值交换到前面
            else {
                constexpr xyu::size_t mv = bind_min_value<cIdx, cIdxs...>;
                constexpr xyu::size_t diff = sizeof...(sIdxs);
                return noexcept(make_bind_any_impl(xyu::isequence_c<sIdxs..., mv>{}, xyu::t_args_del<xyu::isequence_c<cIdx, cIdxs...>, mi>{},
                                                   xyu::t_val<Functor>(), bind_swap_tuple<diff, mi + diff>(xyu::t_val<Tp&>())));
            }
        }

        // 选择排序
        template <xyu::size_t... sIdxs, xyu::size_t cIdx, xyu::size_t... cIdxs,
                  typename Functor, typename... BArgs>
        constexpr auto make_bind_any_impl(xyu::isequence_c<sIdxs...>, xyu::isequence_c<cIdx, cIdxs...>,
                                          Functor&& func, Tuple<BArgs...> bargs)
        noexcept(make_bind_any_impl_noexcept<Functor, BArgs...>(xyu::isequence_c<sIdxs...>{}, xyu::isequence_c<cIdx, cIdxs...>{}))
        {
            constexpr xyu::size_t mi = bind_min_index<cIdx, cIdxs...>;
            // 已经有序
            if constexpr (mi == 0)
                return make_bind_any_impl(xyu::isequence_c<sIdxs..., cIdx>{}, xyu::isequence_c<cIdxs...>{},
                                          xyu::forward<Functor>(func), bargs);
            // 将未排序中的最小值交换到前面
            else {
                constexpr xyu::size_t mv = bind_min_value<cIdx, cIdxs...>;
                constexpr xyu::size_t diff = sizeof...(sIdxs);
                return make_bind_any_impl(xyu::isequence_c<sIdxs..., mv>{}, xyu::t_args_del<xyu::isequence_c<cIdx, cIdxs...>, mi>{},
                                          xyu::forward<Functor>(func), bind_swap_tuple<diff, mi + diff>(bargs));
            }
        }

        // 创建对象辅助函数
        template <xyu::size_t... sIdxs, typename Functor, typename... BArgs, xyu::size_t... Indexs>
        constexpr auto make_bind_any_impl_help(xyu::isequence_c<sIdxs...>, Functor&& func,
                                               Tuple<BArgs...> bargs, xyu::isequence_c<Indexs...>)
        noexcept(noexcept(Bind_Any<xyu::isequence_c<sIdxs...>, xyu::t_decay<Functor>, xyu::t_decay<BArgs>...>{xyu::forward<Functor>(func), bargs.template get<Indexs>()...}))
        {
            return Bind_Any<xyu::isequence_c<sIdxs...>, xyu::t_decay<Functor>, xyu::t_decay<BArgs>...>
                {xyu::forward<Functor>(func), bargs.template get<Indexs>()...};
        }
        // 排序结束
        template <xyu::size_t... sIdxs, typename Functor, typename... BArgs>
        constexpr auto make_bind_any_impl(xyu::isequence_c<sIdxs...>, xyu::isequence_c<>,
                                          Functor&& func, Tuple<BArgs...> bargs)
        noexcept(noexcept(make_bind_any_impl_help(xyu::isequence_c<sIdxs...>{}, xyu::forward<Functor>(func), bargs, xyu::make_isequence<sizeof...(BArgs)>())))
        {
            return make_bind_any_impl_help(xyu::isequence_c<sIdxs...>{}, xyu::forward<Functor>(func),
                                          bargs, xyu::make_isequence<sizeof...(BArgs)>());
        }

        // make_bind_any 异常检测
        template <typename Functor, typename... BArgs, xyu::size_t... BIndexs, typename = xyu::t_enable<(sizeof...(BIndexs) > 0)>>
        constexpr auto make_bind_any_noexcept(xyu::isequence_c<BIndexs...>) noexcept
        {
            constexpr int index_check = bind_index_check<BIndexs...>;
            static_assert(index_check != -1, "Duplicate bind index.");
            if constexpr (index_check == 1)
                return noexcept(Bind_Any<xyu::isequence_c<BIndexs...>, xyu::t_decay<Functor>, xyu::t_decay<BArgs>...>
                    {xyu::t_val<Functor>(), xyu::t_val<BArgs>()...});
            else return noexcept(make_bind_any_impl(xyu::isequence_c<BIndexs...>{}, xyu::isequence_c<>{},
                                                    xyu::t_val<Functor>(), xyu::t_val<Tuple<xyu::t_decay<BArgs>...>>()));
        }
    }

    /**
     * @brief 创建绑定对象
     * @tparam BIndexs 每个参数的绑定索引
     * @param func 待绑定的函数 (去除cvref的退化类型)
     * @param args 待绑定的参数 (去除cvref的退化类型)
     * @note 需要传递引用类型可以用 xyu::make_refer(arg)
     * @note 如果想将参数绑定到调用时参数列表的末尾，可以提供较大的索引值（超出调用时参数的数量），或者使用负数索引（如 -1, -2）来指定相对于末尾的位置。
     * @attention 绑定索引不能重复
     */
    template <xyu::size_t... BIndexs, typename Functor, typename... BArgs, typename = xyu::t_enable<(sizeof...(BIndexs) > 0)>>
    constexpr auto make_bind_any(Functor&& func, BArgs&&... args)
    noexcept(__::make_bind_any_noexcept<Functor, BArgs...>(xyu::isequence_c<BIndexs...>{}))
    {
        constexpr int index_check = __::bind_index_check<BIndexs...>;
        static_assert(index_check != -1, "Duplicate bind index.");
        if constexpr (index_check == 1)
            return Bind_Any<xyu::isequence_c<BIndexs...>, xyu::t_decay<Functor>, xyu::t_decay<BArgs>...>
                {xyu::forward<Functor>(func), xyu::forward<BArgs>(args)...};
        else
            return __::make_bind_any_impl(xyu::isequence_c<>{}, xyu::isequence_c<BIndexs...>{},
                                          xyu::forward<Functor>(func), xyu::make_tuple(xyu::forward<BArgs>(args)...));
    }
}

/// 绑定转发
namespace xylu::xycontain
{
    namespace __
    {
        // 获取第一个类型
        template <auto... Vs>
        struct tmpl_get_first_type
        {
            using type = bool;
            static constexpr auto value = false;
        };
        template <auto V, auto... Vs>
        struct tmpl_get_first_type<V, Vs...>
        {
            using type = decltype(V);
            static constexpr type value = V;
        };

        // make_bind 异常检测
        template <auto... Idxs, typename Functor, typename... Args>
        constexpr bool make_bind_noexcept(xyu::typelist_c<Functor, Args...>) noexcept
        {
            if constexpr (sizeof...(Idxs) == 0 || xyu::t_is_bool<__::tmpl_get_first_type<Idxs...>>)
            {
                if constexpr (sizeof...(Idxs) == 0 || __::tmpl_get_first_type<Idxs...>::value)
                    return noexcept(make_bind_front(xyu::t_val<Functor>(), xyu::t_val<Args>()...));
                else return noexcept(make_bind_back(xyu::t_val<Functor>(), xyu::t_val<Args>()...));
            }
            else return noexcept(make_bind_any<static_cast<xyu::size_t>(Idxs)...>(xyu::t_val<Functor>(), xyu::t_val<Args>()...));
        }
    }

    /**
     * @brief 创建一个函数绑定对象的统一工厂函数。
     *
     * @tparam Idxs... (可选) 绑定索引或绑定模式。
     * @param func     要绑定的可调用对象。
     * @param args...  要绑定的参数。
     * @return         一个 `Bind` 或 `Bind_Any` 对象。
     *
     * @details
     * `make_bind` 提供了一个单一的、高度灵活的接口来创建所有类型的绑定对象。
     *
     * --- 使用模式 ---
     *
     * 1.  **前向绑定 (默认):**
     *     - `make_bind(f, a, b)` or `make_bind<true>(f, a, b)`
     *     - 效果: 调用时 `f(a, b, new_args...)`
     *
     * 2.  **后向绑定:**
     *     - `make_bind<false>(f, a, b)`
     *     - 效果: 调用时 `f(new_args..., a, b)`
     *
     * 3.  **任意位置绑定:**
     *     - `make_bind<2, 0>(f, a, b)`
     *     - 效果: 调用时 `f(b, new_arg1, a, new_args...)`
     *
     * 4.  **末尾相对绑定 (使用负数索引):**
     *     - `make_bind<-1, 0>(f, a, b)`
     *     - 效果: `f(b, new_args..., a)`
     *
     * @note 所有绑定的参数默认都是按值存储（通过 `xyu::t_decay` 去除cvref）。
     *       如果需要按引用绑定，请使用 `xyu::t_refer` 进行包装，
     *       例如 `make_bind(f, make_refer(my_var))`。
     */
    template <auto... Idxs, typename Functor, typename... Args>
    constexpr auto make_bind(Functor&& func, Args&&... args)
    noexcept(__::make_bind_noexcept<Idxs...>(xyu::typelist_c<Functor, Args...>{}))
    {
        // 前后绑定
        if constexpr (sizeof...(Idxs) == 0 || xyu::t_is_bool<__::tmpl_get_first_type<Idxs...>>)
        {
            if constexpr (sizeof...(Idxs) == 0 || __::tmpl_get_first_type<Idxs...>::value)
                return make_bind_front(xyu::forward<Functor>(func), xyu::forward<Args>(args)...);
            else return make_bind_back(xyu::forward<Functor>(func), xyu::forward<Args>(args)...);
        }
        // 随机绑定
        else return make_bind_any<static_cast<xyu::size_t>(Idxs)...>(xyu::forward<Functor>(func), xyu::forward<Args>(args)...);
    }
}

#pragma clang diagnostic pop