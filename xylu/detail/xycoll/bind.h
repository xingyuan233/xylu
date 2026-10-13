#pragma once

/**
 * @file    xycoll/bind.h
 * @brief   绑定对象
 * @date    2026-06-16
 */

#include "./__/tool.h"
#include "./tuple.h"

/// 绑定对象
namespace xylu::xycoll
{
    /**
     * @brief 绑定对象
     * @tparam BindInfo  绑定策略，指定绑定参数在调用时的位置：
     *         - xyu::bool_c<true>            : 前绑定（绑定参数位于调用参数之前）
     *         - xyu::bool_c<false>           : 后绑定（绑定参数位于调用参数之后）
     *         - xyu::isequence_c<BIndexs...> : 随机绑定（绑定参数放置在调用参数的指定索引处）
     * @tparam Functor  可调用对象类型（函数指针、函数对象、lambda 等）
     * @tparam BArgs    绑定的参数类型包
     *
     * @details
     *   存储方式：
     *   - 内部存储 Functor 对象 func 和 Tuple<BArgs...> bargs
     *   - 前/后绑定：绑定参数以原始顺序存储，调用时直接拼接
     *   - 随机绑定：绑定索引必须升序且不重复
     *
     *   方法简述：
     *   - operator()：支持const及引用限定（&, const&, &&），转发绑定参数与调用参数执行调用
     *
     *   注意事项：
     *   - 空指针：构造函数接收的 Functor 若为指针则在 !XY_RELEASE 下检查空指针并 abort
     *   - 类型退化：存储时对 Functor 和 BArgs 应用 decay，数组退化为指针，函数退化为函数指针，
     *             若需要存储引用，请使用 `xyu::RefVal{v}` 进行封装
     *   - 临时存储：随机绑定中为随机分发会临时构造 Tuple 作为参数转发，参数通过引用存储 (当值存储安全且高效时使用值存储)
     *
     *   异常安全：抛出异常时状态完全取决于原 Functor 的异常保证
     */
    template <typename BindInfo, typename Functor, typename... BArgs>
    struct Bind;

    /// 绑定对象
    template <bool front, typename Functor, typename... BArgs>
    struct Bind<xyu::bool_c<front>, Functor, BArgs...>
    {
    private:
        Functor func;           // 类函数
        Tuple<BArgs...> bargs;  // 绑定的参数

    public:
        /// 构造函数
        template <typename Func, typename... Args>
        constexpr explicit Bind(Func&& func, Args&&... args) noexcept(xyu::t_can_nothrow_listinit<Functor, Func> && xyu::t_can_nothrow_listinit<Tuple<BArgs...>, Args...>)
            : func{xyu::forward<Func>(func)}, bargs{xyu::forward<Args>(args)...}
        { __::Checker::check_ptr(func, XY_CurPos("Bind::Bind")); }

        /// 调用 (左值)
        template <typename... Args>
        constexpr decltype(auto) operator()(Args&&... args) & noexcept(noexcept(call_help(*this, bind_indexs, xyu::forward<Args>(args)...)))
        { return call_help(*this, bind_indexs, xyu::forward<Args>(args)...); }
        /// 调用 (右值)
        template <typename... Args>
        constexpr decltype(auto) operator()(Args&&... args) && noexcept(noexcept(call_help(xyu::move(*this), bind_indexs, xyu::forward<Args>(args)...)))
        { return call_help(xyu::move(*this), bind_indexs, xyu::forward<Args>(args)...); }
        /// 调用 (const)
        template <typename... Args>
        constexpr decltype(auto) operator()(Args&&... args) const& noexcept(noexcept(call_help(*this, bind_indexs, xyu::forward<Args>(args)...)))
        { return call_help(*this, bind_indexs, xyu::forward<Args>(args)...); }

    private:
        // 绑定索引序列
        static constexpr auto bind_indexs = xyu::make_isequence<sizeof...(BArgs)>();

        // 调用辅助函数
        template <typename Bind, typename... Args, xyu::size_t... indexs>
        static constexpr decltype(auto) call_help(Bind&& bind, xyu::isequence_c<indexs...>, Args&&... args)
        noexcept((front && xyu::t_can_nothrow_call<xyu::t_sync_cr<Bind, Functor>, xyu::t_sync_cr<Bind, BArgs>..., Args...>) ||
                (!front && xyu::t_can_nothrow_call<xyu::t_sync_cr<Bind, Functor>, Args..., xyu::t_sync_cr<Bind, BArgs>...>))
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

    /**
     * @brief 创建绑定对象 (绑定到前面)
     * @param func 绑定的函数
     * @param args 绑定的参数
     */
    template <typename Function, typename... BArgs>
    constexpr auto make_bind_front(Function&& func, BArgs&&... args)
    noexcept(noexcept(Bind<xyu::true_c, xyu::t_decay<Function>, xyu::t_decay<BArgs>...>{xyu::forward<Function>(func), xyu::forward<BArgs>(args)...}))
    { return Bind<xyu::true_c, xyu::t_decay<Function>, xyu::t_decay<BArgs>...>{xyu::forward<Function>(func), xyu::forward<BArgs>(args)...}; }

    /**
     * @brief 创建绑定对象 (绑定到后面)
     * @param func 绑定的函数
     * @param args 绑定的参数
     */
    template <typename Function, typename... BArgs>
    constexpr auto make_bind_back(Function&& func, BArgs&&... args)
    noexcept(noexcept(Bind<xyu::false_c, xyu::t_decay<Function>, xyu::t_decay<BArgs>...>{xyu::forward<Function>(func), xyu::forward<BArgs>(args)...}))
    { return Bind<xyu::false_c, xyu::t_decay<Function>, xyu::t_decay<BArgs>...>{xyu::forward<Function>(func), xyu::forward<BArgs>(args)...}; }
}

/// 随机绑定对象
namespace xylu::xycoll
{
    /// 随即绑定对象
    template <xyu::size_t... BIndexs, typename Functor, typename... BArgs>
    struct Bind<xyu::isequence_c<BIndexs...>, Functor, BArgs...>
    {
        static_assert(sizeof...(BArgs) == sizeof...(BIndexs));
    private:
        Functor func;           // 类函数
        Tuple<BArgs...> bargs;  // 绑定的参数

    public:
        /// 构造函数
        template <typename Func, typename... Args>
        constexpr explicit Bind(Func&& func, Args&&... args) noexcept(xyu::t_can_nothrow_listinit<Functor, Func> && xyu::t_can_nothrow_listinit<Tuple<BArgs...>, Args...>)
            : func{xyu::forward<Func>(func)}, bargs{xyu::forward<Args>(args)...}
        { __::Checker::check_ptr(func, XY_CurPos("Bind::Bind")); }

        /// 调用 (左值)
        template <typename... Args>
        constexpr decltype(auto) operator()(Args&&... args) & noexcept(noexcept(this->template call_impl<0,0,Args...>(xyu::t_val<Args>()..., *this)))
        { return call_impl<0,0,Args...>(xyu::forward<Args>(args)..., *this); }
        /// 调用 (右值)
        template <typename... Args>
        constexpr decltype(auto) operator()(Args&&... args) && noexcept(noexcept(this->template call_impl<0,0,Args...>(xyu::forward<Args>(args)..., xyu::move(*this))))
        { return call_impl<0,0,Args...>(xyu::forward<Args>(args)..., xyu::move(*this)); }
        /// 调用 (const)
        template <typename... Args>
        constexpr decltype(auto) operator()(Args&&... args) const& noexcept(noexcept(this->template call_impl<0,0,Args...>(xyu::forward<Args>(args)..., *this)))
        { return call_impl<0,0,Args...>(xyu::forward<Args>(args)..., *this); }

    private:
        // 索引数组
        static constexpr xyu::size_t bIndexs[sizeof...(BIndexs)] = {BIndexs...};

        // 双参数包 - 使用绑定参数
        template <xyu::size_t cIndex, xyu::size_t bIdx, typename... Args, typename B, typename... Vargs,
                  xyu::t_enable<bIdx < sizeof...(BArgs) && sizeof...(Args) && cIndex == bIndexs[bIdx], bool> = true>
        static constexpr decltype(auto) call_impl(Args&&... args, B&& bind, Vargs&&... vargs)
        noexcept(noexcept(Bind::template call_impl<cIndex+1, bIdx+1, Args...>(xyu::t_val<Args>()..., xyu::t_val<B>(), xyu::t_val<Vargs>()..., xyu::t_val<B>().bargs.template get<bIdx>())))
        {
            return call_impl<cIndex+1, bIdx+1, Args...>(xyu::forward<Args>(args)..., xyu::forward<B>(bind),
                    xyu::forward<Vargs>(vargs)..., xyu::forward<B>(bind).bargs.template get<bIdx>());
        }

        // 双参数包 - 使用传递参数
        template <xyu::size_t cIndex, xyu::size_t bIdx, typename Arg, typename... Args, typename B, typename... Vargs,
                  xyu::t_enable<bIdx < sizeof...(BArgs) && cIndex != bIndexs[bIdx], bool> = false>
        static constexpr decltype(auto) call_impl(Arg&& arg, Args&&... args, B&& bind, Vargs&&... vargs)
        noexcept(noexcept(call_impl<cIndex+1, bIdx, Args...>(xyu::t_val<Args>()..., xyu::t_val<B>(), xyu::t_val<Arg>(), xyu::t_val<Vargs>()...)))
        {
            return call_impl<cIndex+1, bIdx, Args...>(xyu::forward<Args>(args)..., xyu::forward<B>(bind),
                xyu::forward<Arg>(arg), xyu::forward<Vargs>(vargs)...);
        }

        // 单参数包 - 使用绑定参数
        template <xyu::size_t cIndex, xyu::size_t bIdx, typename B, typename... Vargs,
                  typename I = xyu::make_isequence_t<bIdx, sizeof...(BArgs)>,
                  typename = xyu::t_enable<bIdx < sizeof...(BArgs) && xyu::t_is_same_nocr<B,Bind>>>
        static constexpr decltype(auto) call_impl(B&& bind, Vargs&&... vargs)
        noexcept(noexcept(call_impl_tp(I{}, xyu::t_val<B>(), xyu::t_val<Vargs>()...)))
        { return call_impl_tp(I{}, xyu::forward<B>(bind), xyu::forward<Vargs>(vargs)...); }

        // 单参数包 - 展开绑定参数
        template <xyu::size_t... ids, typename B, typename... Vargs>
        static constexpr decltype(auto) call_impl_tp(xyu::isequence_c<ids...>, B&& bind, Vargs&&... vargs)
        noexcept(noexcept(xyu::t_val<B>().func(xyu::t_val<Vargs>()..., xyu::t_val<B>().bargs.template get<ids>()...)))
        {
            return xyu::forward<B>(bind).func(xyu::forward<Vargs>(vargs)...,
                xyu::forward<B>(bind).bargs.template get<ids>()...);
        }

        // 单参数包 - 使用传递参数
        template <xyu::size_t cIndex, xyu::size_t bIdx, typename... Args, typename B, typename... Vargs,
                  typename = xyu::t_enable<bIdx >= sizeof...(BArgs)>>
        static constexpr decltype(auto) call_impl(Args&&... args, B&& bind, Vargs&&... vargs)
        noexcept(noexcept(xyu::forward<B>(bind).func(xyu::forward<Vargs>(vargs)..., xyu::forward<Args>(args)...)))
        { return xyu::forward<B>(bind).func(xyu::forward<Vargs>(vargs)..., xyu::forward<Args>(args)...); }
    };

    namespace __
    {
        // 临时存储
        template <typename Arg, typename T = xyu::t_rem_refer<Arg>>
        using bind_tmp_store = xyu::t_cond<xyu::t_is_function<Arg>, T*,
                               xyu::t_cond<xyu::t_is_array<Arg>, xyu::t_chg_arr2ptr<T>,
                               xyu::t_cond<xyu::t_is_refer<Arg>, Arg,
                               xyu::t_cond<xyu::t_is_trivial_copyable<T> && sizeof(T) <= sizeof(T&), T, T&>>>>;

        // 处理索引
        template <xyu::size_t... BIndexs>
        constexpr auto process_indices() noexcept
        {
            constexpr xyu::size_t N = sizeof...(BIndexs);
            constexpr xyu::size_t input[N] = {BIndexs...};

            struct Ret { int type; xyu::size_t abs_idx[N], rel_idx[N]; } ret{};

            // 检查是否升序
            bool sorted_flag = true;
            for (xyu::size_t i = 0; i + 1 < N; ++i)
            {
                if (input[i] >= input[i + 1])
                {
                    sorted_flag = false;
                    break;
                }
            }
            if (sorted_flag) return ret;

            // 选择排序
            for (xyu::size_t i = 0; i < N; ++i)
            {
                ret.abs_idx[i] = input[i];
                ret.rel_idx[i] = i;
            }
            for (xyu::size_t i = 0; i + 1 < N; ++i)
            {
                xyu::size_t min_idx = i;
                for (xyu::size_t j = i + 1; j < N; ++j)
                {
                    if (ret.abs_idx[j] < ret.abs_idx[min_idx]) min_idx = j;
                }
                if (min_idx != i)
                {
                    xyu::swap(ret.abs_idx[i], ret.abs_idx[min_idx]);
                    xyu::swap(ret.rel_idx[i], ret.rel_idx[min_idx]);
                }
            }

            // 检查重复
            for (xyu::size_t i = 0; i + 1 < N; ++i)
            {
                if (ret.abs_idx[i] == ret.abs_idx[i + 1])
                {
                    ret.type = 2;
                    return ret;
                }
            }

            // 返回排序结果
            ret.type = 1;
            return ret;
        }

        // make_bind_any 重排
        template <typename... BArgs, typename Functor, typename TT, typename Tp, xyu::size_t... BIndexs, xyu::size_t... AIndexs>
        constexpr auto make_bind_any_resort(Functor&& func, TT&&, Tp&& tp, xyu::isequence_c<BIndexs...>, xyu::isequence_c<AIndexs...>)
        noexcept(noexcept(Bind<xyu::isequence_c<BIndexs...>, xyu::t_decay<Functor>, xyu::t_decay<BArgs>...>{xyu::t_val<Functor>(), xyu::forward<xyu::t_tps_get<TT, AIndexs>>(tp.template get<AIndexs>())...}))
        {
            return Bind<xyu::isequence_c<BIndexs...>, xyu::t_decay<Functor>, xyu::t_decay<BArgs>...>
                {xyu::forward<Functor>(func), xyu::forward<xyu::t_tps_get<TT, AIndexs>>(tp.template get<AIndexs>())...};
        }

        // make_bind_any 辅助
        template <xyu::size_t... BIndexs, xyu::size_t... indexs, typename Functor, typename... BArgs>
        constexpr auto make_bind_any_impl(xyu::isequence_c<indexs...>, Functor&& func, BArgs&&... args)
        {
            constexpr auto ret = process_indices<BIndexs...>();
            static_assert(ret.type != 2, "Duplicate bind index");
            if constexpr (ret.type == 0) return Bind<xyu::isequence_c<BIndexs...>, xyu::t_decay<Functor>, xyu::t_decay<BArgs>...>
                    {xyu::forward<Functor>(func), xyu::forward<BArgs>(args)...};
            else
            {
                return make_bind_any_resort<BArgs...>(xyu::forward<Functor>(func),
                    xyu::typelist_c<BArgs...>{},
                    Tuple<bind_tmp_store<BArgs>...>{static_cast<bind_tmp_store<BArgs>>(args)...},
                    xyu::isequence_c<ret.abs_idx[indexs]...>{}, xyu::isequence_c<ret.rel_idx[indexs]...>{});
            }
        }

        // make_bind_any 异常检测
        template <typename Functor, typename... BArgs, xyu::size_t... BIndexs, xyu::size_t... indexs>
        constexpr auto make_bind_any_noexcept(xyu::isequence_c<BIndexs...>, xyu::isequence_c<indexs...>) noexcept
        {
            static_assert(sizeof...(BArgs) == sizeof...(BIndexs));
            constexpr auto ret = process_indices<BIndexs...>();
            static_assert(ret.type != 2, "Duplicate bind index");
            if constexpr (ret.type == 0) return noexcept(Bind<xyu::isequence_c<BIndexs...>, xyu::t_decay<Functor>, xyu::t_decay<BArgs>...>
                    {xyu::t_val<Functor>(), xyu::t_val<BArgs>()...});
            else return noexcept(make_bind_any_resort<BArgs...>(xyu::t_val<Functor>(),
                    xyu::typelist_c<BArgs...>{}, xyu::t_val<Tuple<bind_tmp_store<BArgs>...>>(),
                    xyu::isequence_c<ret.abs_idx[indexs]...>{}, xyu::isequence_c<ret.rel_idx[indexs]...>{}));
        }
    }

    /**
     * @brief 创建绑定对象
     * @tparam BIndexs 每个参数的绑定索引
     * @param func 绑定的函数
     * @param args 绑定的参数
     * @details
     *   如果想将参数绑定到调用时参数列表的末尾，可以提供较大的索引值（超出调用时参数的数量），
     *   或者使用负数索引（如 -1, -2）来指定相对于末尾的位置。
     * @note 绑定索引不能重复
     */
    template <xyu::size_t... BIndexs, typename Functor, typename... BArgs, typename = xyu::t_enable<(sizeof...(BIndexs) > 0)>,
              typename indexs = xyu::make_isequence_t<sizeof...(BIndexs)>>
    constexpr auto make_bind_any(Functor&& func, BArgs&&... args)
    noexcept(__::make_bind_any_noexcept<Functor, BArgs...>(xyu::isequence_c<BIndexs...>{}, indexs{}))
    { return __::make_bind_any_impl<BIndexs...>(indexs{}, xyu::forward<Functor>(func), xyu::forward<BArgs>(args)...); }
}

/// 绑定转发
namespace xylu::xycoll
{
    namespace __
    {
        template <auto... Idxs> struct any_seq {};

        // make_bind 异常检测
        template <auto... Idxs, typename Functor, typename... Args>
        constexpr bool make_bind_noexcept(xyu::typelist_c<Functor, Args...>) noexcept
        {
            constexpr auto v = xyu::t_seq_get<any_seq<Idxs..., true>, 0>;
            if constexpr (sizeof...(Idxs) == 0 || xyu::t_is_bool<decltype(v)>)
            {
                if constexpr (v) return noexcept(make_bind_front(xyu::t_val<Functor>(), xyu::t_val<Args>()...));
                else return noexcept(make_bind_back(xyu::t_val<Functor>(), xyu::t_val<Args>()...));
            }
            else return noexcept(make_bind_any<xyu::size_t(Idxs)...>(xyu::t_val<Functor>(), xyu::t_val<Args>()...));
        }
    }

    // 推导指引
    template <typename Func, typename... Args, typename = xyu::t_enable<xyu::t_can_call<Func, Args...>>>
    Bind(Func&&, Args&&...) -> Bind<xyu::true_c, Func, Args...>;
    template <bool front, typename Func, typename... Args, typename = xyu::t_enable<xyu::t_can_call<Func, Args...>>>
    Bind(xyu::bool_c<front>, Func&&, Args&&...) -> Bind<xyu::bool_c<front>, Func, Args...>;

    /**
     * @brief 创建绑定对象的统一入口
     *
     * @tparam Idxs  编译期索引序列，可为空、bool 值或整数索引
     * @param  func  要绑定的可调用对象
     * @param  args  要绑定的参数
     * @return       绑定对象，类型由分派结果决定
     *
     * @details
     * 根据编译期索引序列 Idxs 自动选择绑定模式：
     * - 不提供索引（Idxs 为空）或第一个索引为 bool 值时，视为前/后绑定；
     *   true 对应绑定在前，false 对应绑定在后。
     * - 否则，视为随机位置绑定，Idxs 被转换为 size_t 后传给 make_bind_any，
     *   每个索引表示对应绑定参数在最终调用参数列表中的位置。
     *
     * @par 示例
     * @code
     * // 前绑定：func(bound1, bound2, arg1, arg2...)
     * auto b1 = make_bind(func, bound1, bound2);
     *
     * // 后绑定：func(arg1, arg2..., bound1, bound2)
     * auto b2 = make_bind˂false˃(func, bound1, bound2);
     *
     * // 随机绑定：func(0:b, 1:xxx, 2:a, 3:xxx, 4:xxx, 5:c, ...)
     * auto b3 = make_bind˂2, 0, 5˃(func, a, b, c);
     * @endcode
     */
    template <auto... Idxs, typename Functor, typename... Args>
    constexpr auto make_bind(Functor&& func, Args&&... args)
    noexcept(__::make_bind_noexcept<Idxs...>(xyu::typelist_c<Functor, Args...>{}))
    {
        // 前后绑定
        constexpr auto v = xyu::t_seq_get<__::any_seq<Idxs..., true>, 0>;
        if constexpr (sizeof...(Idxs) == 0 || xyu::t_is_bool<decltype(v)>)
        {
            if constexpr (v) return make_bind_front(xyu::forward<Functor>(func), xyu::forward<Args>(args)...);
            else return make_bind_back(xyu::forward<Functor>(func), xyu::forward<Args>(args)...);
        }
        // 随机绑定
        else return make_bind_any<xyu::size_t(Idxs)...>(xyu::forward<Functor>(func), xyu::forward<Args>(args)...);
    }
}
