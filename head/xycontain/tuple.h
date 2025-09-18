#pragma clang diagnostic push
#pragma ide diagnostic ignored "bugprone-forwarding-reference-overload"
#pragma ide diagnostic ignored "modernize-use-nodiscard"
#pragma ide diagnostic ignored "UnreachableCallsOfFunction"
#pragma ide diagnostic ignored "OCUnusedTypeAliasInspection"
#pragma ide diagnostic ignored "ConstantConditionsOC"
#pragma ide diagnostic ignored "OCUnusedStructInspection"
#pragma ide diagnostic ignored "NotImplementedFunctions"
#pragma ide diagnostic ignored "OCUnusedTemplateParameterInspection"
#pragma ide diagnostic ignored "google-explicit-constructor"
#pragma ide diagnostic ignored "OCUnusedGlobalDeclarationInspection"
#pragma ide diagnostic ignored "UnusedValue"
#pragma ide diagnostic ignored "UnusedLocalVariable"
#pragma ide diagnostic ignored "cert-dcl58-cpp"
#pragma once

#include "../../link/hashfun"
#include "../../link/format"
#include "../../link/compare"

/**
 * @file
 * @brief 定义了 `Tuple` 类及其视图 `TupleView`，一个功能丰富的异构容器。
 * @details
 *   本文件提供了一个强大的元组实现，除了基本的元素存储和访问外，还包括一整套
 *   用于在编译期修改元组结构（如添加、删除、切片元素）的接口，以及 `apply`,
 *   `foreach` 等函数式编程工具和对格式化库的深度支持。
 */

/// 元组
namespace xylu::xycontain
{
    // 前向声明
    template <typename... Args> struct Tuple;
    template <typename Tp, xyu::size_t start, xyu::size_t len> struct TupleView;

    /// Tuple 基类
    namespace __
    {
        // 获取元素类型 (防止 TupleBase 访问时子类未定义)
        template <typename Derived, xyu::size_t index>
        struct tuple_arg_t;
        // 获取元素索引 (防止 TupleBase 访问时子类未定义)
        template <typename Derived, typename Arg, xyu::size_t start = 0, xyu::size_t end = static_cast<xyu::size_t>(-1)>
        struct tuple_arg_i;

        /**
         * @internal
         * @brief `Tuple` 和 `TupleView` 的 CRTP 功能基类。
         * @details
         *   这个基类使用奇异递归模板模式（CRTP），将所有元组的通用操作（如 `get`,
         *   `apply`, `foreach`, `append`, `compare` 等）集中实现，供 `Tuple` 和
         *   `TupleView` 继承，从而避免了代码重复。
         * @tparam Istart 起始索引
         * @tparam Indexs 索引序列 (从 0 开始，实际索引需加上 Istart 偏移)
         * @tparam Args 完整参数包
         */
        template <typename Derived, xyu::size_t Istart, typename Indexs, typename... Args>
        struct TupleBase;
        template <typename Derived, xyu::size_t Istart, xyu::size_t... Indexs, typename... Args>
        struct TupleBase<Derived, Istart, xyu::isequence_c<Indexs...>, Args...>
        {
            // 获取元素类型
            template <xyu::size_t index>
            using arg_t = typename tuple_arg_t<Derived, Istart + index>::type;
            // 获取元素索引
            template <typename Arg, xyu::size_t start = 0, xyu::size_t end = sizeof...(Indexs)>
            static constexpr xyu::size_t arg_i = tuple_arg_i<Derived, Arg, Istart + start, Istart + xyu::min(end, sizeof...(Indexs))>::value - Istart;

            /// 获取元素数量
            constexpr static xyu::size_t count() noexcept { return sizeof...(Indexs); }

            /// 获取元素 (左值引用)
            template <xyu::size_t index, typename = xyu::t_enable<index < count()>>
            constexpr decltype(auto) get() & noexcept { return derived().template inget<index>(); }
            /// 获取元素 (const左值引用)
            template <xyu::size_t index, typename = xyu::t_enable<index < count()>>
            constexpr decltype(auto) get() const& noexcept { return derived().template inget<index>(); }
            /// 获取元素 (右值引用)
            template <xyu::size_t index, typename = xyu::t_enable<index < count()>>
            constexpr decltype(auto) get() && noexcept { return static_cast<arg_t<index>&&>(derived().template inget<index>()); }
            /// 获取元素 (const右值引用)
            template <xyu::size_t index, typename = xyu::t_enable<index < count()>>
            constexpr decltype(auto) get() const&& noexcept { return static_cast<const arg_t<index>&&>(derived().template inget<index>()); }

            /// 获取元素 (左值引用)
            template <typename Arg, xyu::size_t start = 0, xyu::size_t end = sizeof...(Indexs)>
            constexpr decltype(auto) get() & noexcept { return get<arg_i<Arg, start, end>>(); }
            /// 获取元素 (const左值引用)
            template <typename Arg, xyu::size_t start = 0, xyu::size_t end = sizeof...(Indexs)>
            constexpr decltype(auto) get() const& noexcept { return get<arg_i<Arg, start, end>>(); }
            /// 获取元素 (右值引用)
            template <typename Arg, xyu::size_t start = 0, xyu::size_t end = sizeof...(Indexs)>
            constexpr decltype(auto) get() && noexcept { return get<arg_i<Arg, start, end>>(); }
            /// 获取元素 (const右值引用)
            template <typename Arg, xyu::size_t start = 0, xyu::size_t end = sizeof...(Indexs)>
            constexpr decltype(auto) get() const&& noexcept { return get<arg_i<Arg, start, end>>(); }

            /// 相等
            template <typename Derived2, xyu::size_t Istart2, xyu::size_t... Indexs2, typename... Args2>
            constexpr bool equals(const TupleBase<Derived2, Istart2, xyu::isequence_c<Indexs2...>, Args2...>& other) const noexcept(equals_noexcept<TupleBase<Derived2, Istart2, xyu::isequence_c<Indexs2...>, Args2...>>)
            {
                if constexpr (sizeof...(Indexs) != sizeof...(Indexs2)) return false;
                else return (... && xyu::equals(get<Indexs>(), other.template get<Indexs2>()));
            }
            /// 比较
            template <typename Derived2, xyu::size_t Istart2, xyu::size_t... Indexs2, typename... Args2>
            constexpr int compare(const TupleBase<Derived2, Istart2, xyu::isequence_c<Indexs2...>, Args2...>& other) const noexcept(compare_noexcept<TupleBase<Derived2, Istart2, xyu::isequence_c<Indexs2...>, Args2...>>)
            {
                int r = 0;
                constexpr int r2 = xyu::compare(sizeof...(Indexs), sizeof...(Indexs2));
                if constexpr (sizeof...(Indexs) >= sizeof...(Indexs2))
                    (..., (r = r ? r : xyu::compare(get<Indexs2>(), other.template get<Indexs2>())));
                else (..., (r = r ? r : xyu::compare(get<Indexs>(), other.template get<Indexs>())));
                return r ? r : r2;
            }

            /**
             * @brief 给指定索引范围赋值
             * @tparam start 起始索引 (默认 0)
             * @note 当仅有一个参数且无法直接赋值时，尝试解包后赋值
             */
            template <xyu::size_t start = 0, typename... Args2>
            constexpr Derived& assign(Args2&&... args) noexcept(assign_noexcept<start, Args2...>)
            {
                // 单参数
                if constexpr (sizeof...(Args2) == 1)
                {
                    using Arg = xyu::t_args_get<xyu::typelist_c<Args2...>, 0>;
                    // 如果不能通过参数直接赋值，则尝试解包后赋值
                    if constexpr (!xyu::t_can_assign<arg_t<start>&, Arg>)
                        assign_impl_tp<start>(xyu::make_isequence<xyu::min(xyu::t_remove_cvref<Arg>::count(), count() - start)>(),
                                              xyu::forward<Args2>(args)...);
                    else assign_impl_args<start>(xyu::make_isequence<sizeof...(Args2)>(), xyu::forward<Args2>(args)...);
                }
                // 直接赋值
                else assign_impl_args<start>(xyu::make_isequence<sizeof...(Args2)>(), xyu::forward<Args2>(args)...);
                return derived();
            }

            /// 将元组中的所有元素作为参数调用 Func
            template <typename Func>
            constexpr decltype(auto) apply(Func&& func) & noexcept(xyu::t_can_nothrow_call<Func, arg_t<Indexs>&...>)
            { return xyu::forward<Func>(func)(get<Indexs>()...); }
            /// 将元组中的所有元素作为参数调用 Func
            template <typename Func>
            constexpr decltype(auto) apply(Func&& func) const& noexcept(xyu::t_can_nothrow_call<Func, const arg_t<Indexs>&...>)
            { return xyu::forward<Func>(func)(get<Indexs>()...); }
            /// 将元组中的所有元素作为参数调用 Func (移动参数)
            template <typename Func>
            constexpr decltype(auto) apply(Func&& func) && noexcept(xyu::t_can_nothrow_call<Func, arg_t<Indexs>&&...>)
            { return xyu::forward<Func>(func)(get<Indexs>()...); }

            /**
             * @brief 遍历元组中的每个元素并调用 Func
             * @tparam use_ret 是否使用返回值构造一个新元组 (默认使用, 若存在返回值为 void 的函数则不使用)
             */
            template <bool use_ret = true, typename Func>
            constexpr decltype(auto) foreach(Func&& func) & noexcept(foreach_noexcept<use_ret, TupleBase&, Func>)
            {
                if constexpr (!use_ret || (... || xyu::t_is_void<decltype(xyu::forward<Func>(func)(get<Indexs>()))>))
                    (..., xyu::forward<Func>(func)(get<Indexs>()));
                else return Tuple<xyu::t_remove_cvref<decltype(xyu::forward<Func>(func)(get<Indexs>()))>...>
                            { xyu::forward<Func>(func)(get<Indexs>())... };
            }
            /**
             * @brief 遍历元组中的每个元素并调用 Func
             * @tparam use_ret 是否使用返回值构造一个新元组 (默认使用, 若存在返回值为 void 的函数则不使用)
             */
            template <bool use_ret = true, typename Func>
            constexpr decltype(auto) foreach(Func&& func) const& noexcept(foreach_noexcept<use_ret, const TupleBase&, Func>)
            {
                if constexpr (!use_ret || (... || xyu::t_is_void<decltype(xyu::forward<Func>(func)(get<Indexs>()))>))
                    (..., xyu::forward<Func>(func)(get<Indexs>()));
                else return Tuple<xyu::t_remove_cvref<decltype(xyu::forward<Func>(func)(get<Indexs>()))>...>
                        { xyu::forward<Func>(func)(get<Indexs>())... };
            }
            /**
             * @brief 遍历元组中的每个元素并调用 Func (移动参数)
             * @tparam use_ret 是否使用返回值构造一个新元组 (默认使用, 若存在返回值为 void 的函数则不使用)
             */
            template <bool use_ret = true, typename Func>
            constexpr decltype(auto) foreach(Func&& func) && noexcept(foreach_noexcept<use_ret, TupleBase&&, Func>)
            {
                if constexpr (!use_ret || (... || xyu::t_is_void<decltype(xyu::forward<Func>(func)(get<Indexs>()))>))
                    (..., xyu::forward<Func>(func)(get<Indexs>()));
                else return Tuple<xyu::t_remove_cvref<decltype(xyu::forward<Func>(func)(get<Indexs>()))>...>
                            { xyu::forward<Func>(func)(get<Indexs>())... };
            }

            /**
             * @brief 获取元组的引用视图
             * @note 从索引为 start 开始，长度为 len(默认剩下所有) 的视图
             * @note 元素不可修改
             */
            template <xyu::size_t start, xyu::size_t len = sizeof...(Indexs)>
            constexpr auto subview() const noexcept
            {
                constexpr xyu::size_t s = xyu::min(start, sizeof...(Indexs)) + Istart;
                constexpr xyu::size_t l = xyu::min(len, sizeof...(Indexs) + Istart - s);
                return TupleView<const typename Derived::Org_t, s, l>{ derived() };
            }
            /**
             * @brief 获取元组的const引用视图
             * @note 从索引为 start 开始，长度为 len(默认剩下所有) 的视图
             * @note 元素可以修改 (但如果本身为const引用视图，则不能修改)
             */
            template <xyu::size_t start, xyu::size_t len = sizeof...(Indexs)>
            constexpr auto subview() noexcept
            {
                constexpr xyu::size_t s = xyu::min(start, sizeof...(Indexs)) + Istart;
                constexpr xyu::size_t l = xyu::min(len, sizeof...(Indexs) + Istart - s);
                return TupleView<typename Derived::Org_t, s, l>{ derived() };
            }

            /**
             * @brief 末尾添加新类型
             * @example Tuple˂int˃ tp{10}; auto tp2 = tp.append(true); // Tuple˂int, bool˃{10, true};
             */
            template <typename... Args2>
            constexpr auto append(Args2&&... args) const& noexcept(append_noexcept<false, Args2...>)
            {
                return xyu::t_args_add<Effective_t, xyu::t_remove_cvref<Args2>...>
                        {get<Indexs>()..., xyu::forward<Args2>(args)...};
            }
            /**
             * @brief 末尾添加新类型 (移动参数)
             * @example Tuple˂int˃{10}.append(true); // Tuple˂int, bool˃{10, true};
             */
            template <typename... Args2>
            constexpr auto append(Args2&&... args) && noexcept(append_noexcept<true, Args2...>)
            {
                return xyu::t_args_add<Effective_t, xyu::t_remove_cvref<Args2>...>
                        {get<Indexs>()..., xyu::forward<Args2>(args)...};
            }

            /**
             * @brief 中间插入新类型
             * @tparam index 插入位置索引 (超过有效范围则调用 append)
             * @example Tuple˂int, char˃ tp{10, 'a'}; auto tp2 = tp.insert<1>(true); // Tuple˂int, bool, char˃{10, true, 'a'};
             */
            template <xyu::size_t index, typename... Args2>
            constexpr auto insert(Args2&&... args) const& noexcept(insert_noexcept<false, index, Args2...>)
            {
                if constexpr (index >= count()) return append(xyu::forward<Args2>(args)...);
                else return insert_impl<index>(xyu::make_isequence<0, index>(),
                                               xyu::make_isequence<index, sizeof...(Indexs)>(),
                                               xyu::forward<Args2>(args)...);
            }
            /**
             * @brief 中间插入新类型 (移动参数)
             * @tparam index 插入位置索引 (超过有效范围则调用 append)
             * @example Tuple˂int, char˃{10, 'a'}.insert<1>(true); // Tuple˂int, bool, char˃{10, true, 'a'};
             */
            template <xyu::size_t index, typename... Args2>
            constexpr auto insert(Args2&&... args) && noexcept(insert_noexcept<true, index, Args2...>)
            {
                if constexpr (index >= count()) return append(xyu::forward<Args2>(args)...);
                else return insert_impl<index>(xyu::make_isequence<0, index>(),
                                               xyu::make_isequence<index, sizeof...(Indexs)>(),
                                               xyu::forward<Args2>(args)...);
            }

            /**
             * @brief 删除指定索引范围的类型
             * @tparam start 起始索引
             * @tparam len 长度 (默认删除到末尾)
             * @example Tuple˂int, char, bool˃ tp{10, 'a', true}; auto tp2 = tp.erase˂1˃(); // Tuple˂int˃{10};
             */
            template <xyu::size_t start, xyu::size_t len = sizeof...(Indexs)>
            constexpr auto erase() const& noexcept(erase_noexcept<false, start, len>)
            {
                constexpr xyu::size_t s = xyu::min(start, sizeof...(Indexs));
                constexpr xyu::size_t l = xyu::min(len, sizeof...(Indexs) - s);
                return erase_impl<s,l>(xyu::make_isequence<0, s>(), xyu::make_isequence<s + l, sizeof...(Indexs)>());
            }
            /**
             * @brief 删除指定索引范围的类型 (移动参数)
             * @tparam start 起始索引
             * @tparam len 长度 (默认删除到末尾)
             * @example Tuple˂int, char, bool˃{10, 'a', true}.erase˂1˃(); // Tuple˂int˃{10};
             */
            template <xyu::size_t start, xyu::size_t len = sizeof...(Indexs)>
            constexpr auto erase() && noexcept(erase_noexcept<true, start, len>)
            {
                constexpr xyu::size_t s = xyu::min(start, sizeof...(Indexs));
                constexpr xyu::size_t l = xyu::min(len, sizeof...(Indexs) - s);
                return erase_impl<s,l>(xyu::make_isequence<0, s>(), xyu::make_isequence<s + l, sizeof...(Indexs)>());
            }

            /**
             * @brief 移除指定类型
             * @tparam Target 要移除的类型 (精确匹配)
             * @tparam limit 限制移除的次数 (默认移除所有)
             * @example Tuple˂int, char˃ tp{10, 'a'}; auto tp2 = tp.remove˂char˃(); // Tuple˂int˃{10};
             */
            template <typename Target, xyu::size_t limit = sizeof...(Indexs)>
            constexpr auto remove() const& noexcept(remove_noexcept<false, Target, limit>)
            { return remove_impl<Target, limit>(remove_index<Target, limit>(xyu::isequence_c<Indexs...>{})); }
            /**
             * @brief 移除指定类型 (移动参数)
             * @tparam Target 要移除的类型 (精确匹配)
             * @tparam limit 限制移除的次数 (默认移除所有)
             * @example Tuple˂int, char˃{10, 'a'}.remove˂char˃(); // Tuple˂int˃{10};
             */
            template <typename Target, xyu::size_t limit = sizeof...(Indexs)>
            constexpr auto remove() && noexcept(remove_noexcept<true, Target, limit>)
            { return remove_impl<Target, limit>(remove_index<Target, limit>(xyu::isequence_c<Indexs...>{})); }

            /**
             * @brief 反转元组
             * @example Tuple˂int, bool˃ tp{10, true}; auto tp2 = tp.reverse(); // Tuple˂bool, int˃{true, 10};
             */
            constexpr auto reverse() const& noexcept(noexcept(xyu::t_args_rev<Tuple<Args...>>{get<count() - Indexs - 1>()...}))
            { return xyu::t_args_rev<Effective_t>{get<count() - Indexs - 1>()...}; }
            /**
             * @brief 反转元组 (移动参数)
             * @example Tuple˂int, bool˃{10, true}.reverse(); // Tuple˂bool, int˃{true, 10};
             */
            constexpr auto reverse() && noexcept(noexcept(xyu::t_args_rev<Tuple<Args...>>{get<count() - Indexs - 1>()...}))
            { return xyu::t_args_rev<Effective_t>{get<count() - Indexs - 1>()...}; }

            /**
             * @brief 连接两个元组
             * @example Tuple˂int˃ tp{10}, tp2{20}; auto tp3 = tp.concat(tp2); // Tuple˂int, int˃{10, 20};
             */
            template <typename Derived2, xyu::size_t Istart2, xyu::size_t... Indexs2, typename... Args2>
            constexpr auto concat(const TupleBase<Derived2, Istart2, xyu::isequence_c<Indexs2...>, Args2...>& other) const& noexcept(concat_noexcept<false, false, TupleBase<Derived2, Istart2, xyu::isequence_c<Indexs2...>, Args2...>>)
            {
                return xyu::t_args_concat<Effective_t, xyu::t_args_slice<Tuple<Args2...>, Istart2, sizeof...(Indexs2)>>
                {get<Indexs>()..., other.template get<Indexs2>()...};
            }
            /**
             * @brief 连接两个元组
             * @example Tuple˂int˃ tp{10}; auto tp2 = tp.concat(Tuple˂int˃{20}); // Tuple˂int, int˃{10, 20};
             */
            template <typename Derived2, xyu::size_t Istart2, xyu::size_t... Indexs2, typename... Args2>
            constexpr auto concat(TupleBase<Derived2, Istart2, xyu::isequence_c<Indexs2...>, Args2...>&& other) const& noexcept(concat_noexcept<false, true, TupleBase<Derived2, Istart2, xyu::isequence_c<Indexs2...>, Args2...>>)
            {
                return xyu::t_args_concat<Effective_t, xyu::t_args_slice<Tuple<Args2...>, Istart2, sizeof...(Indexs2)>>
                {get<Indexs>()..., xyu::forward<TupleBase<Derived2, Istart2, xyu::isequence_c<Indexs2...>, Args2...>&&>(other).template get<Indexs2>()...};
            }
            /**
             * @brief 连接两个元组 (移动参数)
             * @example Tuple˂int˃ tp{10}; auto tp2 = Tuple˂int˃{20}.concat(tp); // Tuple˂int, int˃{20, 10};
             */
            template <typename Derived2, xyu::size_t Istart2, xyu::size_t... Indexs2, typename... Args2>
            constexpr auto concat(const TupleBase<Derived2, Istart2, xyu::isequence_c<Indexs2...>, Args2...>& other) && noexcept(concat_noexcept<true, false, TupleBase<Derived2, Istart2, xyu::isequence_c<Indexs2...>, Args2...>>)
            {
                return xyu::t_args_concat<Effective_t, xyu::t_args_slice<Tuple<Args2...>, Istart2, sizeof...(Indexs2)>>
                {get<Indexs>()..., other.template get<Indexs2>()...};
            }
            /**
             * @brief 连接两个元组 (移动参数)
             * @example auto tp = Tuple˂int˃{10}.concat(Tuple˂int˃{20}); // Tuple˂int, int˃{10, 20};
             */
            template <typename Derived2, xyu::size_t Istart2, xyu::size_t... Indexs2, typename... Args2>
            constexpr auto concat(TupleBase<Derived2, Istart2, xyu::isequence_c<Indexs2...>, Args2...>&& other) && noexcept(concat_noexcept<true, true, TupleBase<Derived2, Istart2, xyu::isequence_c<Indexs2...>, Args2...>>)
            {
                return xyu::t_args_concat<Effective_t, xyu::t_args_slice<Tuple<Args2...>, Istart2, sizeof...(Indexs2)>>
                {get<Indexs>()..., xyu::forward<TupleBase<Derived2, Istart2, xyu::isequence_c<Indexs2...>, Args2...>&&>(other).template get<Indexs2>()...};
            }

            /**
             * @brief 获取元组切片
             * @tparam start 起始索引
             * @tparam len 长度 (默认到末尾)
             * @example Tuple˂int, char, bool˃ tp{10, 'a', true}; auto tp2 = tp.slice˂1˃(); // Tuple˂char, bool˃{'a', true};
             */
            template <xyu::size_t start, xyu::size_t len = sizeof...(Indexs)>
            constexpr auto slice() const& noexcept(slice_noexcept<false, start, len>)
            {
                constexpr xyu::size_t s = xyu::min(start, sizeof...(Indexs));
                constexpr xyu::size_t l = xyu::min(len, sizeof...(Indexs) - s);
                return slice_impl<s,l>(xyu::make_isequence<s, s + l>());
            }
            /**
             * @brief 获取元组切片 (移动参数)
             * @tparam start 起始索引
             * @tparam len 长度 (默认到末尾)
             * @example Tuple˂int, char, bool˃{10, 'a', true}.slice˂1˃(); // Tuple˂char, bool˃{'a', true};
             */
            template <xyu::size_t start, xyu::size_t len = sizeof...(Indexs)>
            constexpr auto slice() && noexcept(slice_noexcept<true, start, len>)
            {
                constexpr xyu::size_t s = xyu::min(start, sizeof...(Indexs));
                constexpr xyu::size_t l = xyu::min(len, sizeof...(Indexs) - s);
                return slice_impl<s,l>(xyu::make_isequence<s, s + l>());
            }

        private:
            // 获取派生类
            constexpr Derived& derived() noexcept { return static_cast<Derived&>(*this); }
            constexpr const Derived& derived() const noexcept { return static_cast<const Derived&>(*this); }
            // 获取生效部分类型
            using Effective_t = xyu::t_args_slice<Tuple<Args...>, Istart, sizeof...(Indexs)>;

            // 异常检测 - equals
            template <typename Derived2, xyu::size_t Istart2, xyu::size_t... Indexs2, typename... Args2>
            static constexpr bool equals_noexcept_impl(xyu::typelist_c<TupleBase<Derived2, Istart2, xyu::isequence_c<Indexs2...>, Args2...>>) noexcept
            {
                if constexpr (sizeof...(Indexs) != sizeof...(Indexs2)) return true;
                else return noexcept((... && xyu::equals(xyu::t_val<const TupleBase&>().template get<Indexs>(),
                                                         xyu::t_val<const TupleBase<Derived2, Istart2, xyu::isequence_c<Indexs2...>, Args2...>&>().template get<Indexs2>())));
            }
            template <typename TB>
            static constexpr bool equals_noexcept = equals_noexcept_impl(xyu::typelist_c<TB>{});
            // 异常检测 - compare
            template <typename Derived2, xyu::size_t Istart2, xyu::size_t... Indexs2, typename... Args2>
            static constexpr bool compare_noexcept_impl(xyu::typelist_c<TupleBase<Derived2, Istart2, xyu::isequence_c<Indexs2...>, Args2...>>) noexcept
            {
                using TB = TupleBase<Derived2, Istart2, xyu::isequence_c<Indexs2...>, Args2...>;
                constexpr int r2 = xyu::compare(sizeof...(Indexs), sizeof...(Indexs2));
                if constexpr (sizeof...(Indexs) >= sizeof...(Indexs2))
                    return noexcept((..., xyu::compare(xyu::t_val<const TupleBase&>().template get<Indexs2>(),
                                                       xyu::t_val<const TB&>().template get<Indexs2>())));
                else return noexcept((..., xyu::compare(xyu::t_val<const TupleBase&>().template get<Indexs>(),
                                                        xyu::t_val<const TB&>().template get<Indexs>())));
            }
            template <typename TB>
            static constexpr bool compare_noexcept = compare_noexcept_impl(xyu::typelist_c<TB>{});

            // 辅助函数 - assign
            template <xyu::size_t start, typename... Args2, xyu::size_t... indexs>
            constexpr void assign_impl_args(xyu::isequence_c<indexs...>, Args2&&... args)
            { (..., (get<start + indexs>() = xyu::forward<Args2>(args))); }
            template <xyu::size_t start, typename Tp, typename... Empty, xyu::size_t... indexs>
            constexpr void assign_impl_tp(xyu::isequence_c<indexs...>, Tp&& tp, Empty...)
            { (..., (get<start + indexs>() = xyu::forward<Tp>(tp).template get<indexs>())); }
            // 异常检测 - assign
            template <xyu::size_t start, typename... Args2, xyu::size_t... indexs>
            static constexpr bool assign_noexcept_args(xyu::isequence_c<indexs...>) noexcept
            { return noexcept((..., (xyu::t_val<TupleBase&>().template get<start + indexs>() = xyu::t_val<Args2>()))); }
            template <xyu::size_t start, typename Tp, xyu::size_t... indexs>
            static constexpr bool assign_noexcept_tp(xyu::isequence_c<indexs...>) noexcept
            { return noexcept((..., (xyu::t_val<TupleBase&>().template get<start + indexs>() = xyu::t_val<Tp>().template get<indexs>()))); }
            template <xyu::size_t start, typename... Args2>
            static constexpr bool assign_noexcept = []
            {
                static_assert(start + sizeof...(Args2) <= count(), "Too many arguments");
                if constexpr (sizeof...(Args2) == 1)
                {
                    using Arg = xyu::t_args_get<xyu::typelist_c<Args2...>, 0>;
                    if constexpr (!xyu::t_can_assign<arg_t<start>&, Arg>)
                        return assign_noexcept_tp<start, Arg>(xyu::make_isequence<xyu::min(xyu::t_remove_cvref<Arg>::count(), count() - start)>());
                    else return assign_noexcept_args<start, Args2...>(xyu::make_isequence<sizeof...(Args2)>());
                }
                else return assign_noexcept_args<start, Args2...>(xyu::make_isequence<sizeof...(Args2)>());
            }();

            // 异常检测 - foreach
            template <bool use_ret, typename Tp, typename Func>
            static constexpr bool foreach_noexcept = []
            {
                if constexpr (!use_ret || (... || xyu::t_is_void<xyu::t_get_ret<Func, decltype(xyu::t_val<Tp>().template get<Indexs>())>>))
                    return noexcept((... && noexcept(xyu::t_val<Func>()(xyu::t_val<Tp>().template get<Indexs>()))));
                else return noexcept(Tuple<xyu::t_remove_cvref<xyu::t_get_ret<Func, decltype(xyu::t_val<Tp>().template get<Indexs>())>>...>
                                      { xyu::t_val<Func>()(xyu::t_val<Tp>().template get<Indexs>())... });
            }();

            // 异常检测 - append
            template <bool is_move, typename... Args2, xyu::size_t... indexs>
            static constexpr bool append_noexcept_impl(xyu::isequence_c<indexs...>) noexcept
            {
                return noexcept(xyu::t_args_add<Effective_t, xyu::t_remove_cvref<Args2>...>
                        {xyu::t_val<xyu::t_cond<is_move, TupleBase&&, const TupleBase&>>().template get<indexs>()...,
                         xyu::t_val<Args2&&>()...});
            }
            template <bool is_move, typename... Args2>
            static constexpr bool append_noexcept = append_noexcept_impl<is_move, Args2...>(xyu::make_isequence<count(), count() + sizeof...(Args2)>());

            // 辅助函数 - insert
            template <xyu::size_t index, xyu::size_t... ils, xyu::size_t... irs, typename... Args2>
            constexpr auto insert_impl(xyu::isequence_c<ils...>, xyu::isequence_c<irs...>, Args2&&... args) const&
            {
                return xyu::t_args_ins<Effective_t, index, xyu::t_remove_cvref<Args2>...>
                        {get<ils>()..., xyu::forward<Args2>(args)..., get<irs>()...};
            }
            template <xyu::size_t index, xyu::size_t... ils, xyu::size_t... irs, typename... Args2>
            constexpr auto insert_impl(xyu::isequence_c<ils...>, xyu::isequence_c<irs...>, Args2&&... args) &&
            {
                return xyu::t_args_ins<Effective_t, index, xyu::t_remove_cvref<Args2>...>
                        {get<ils>()..., xyu::forward<Args2>(args)..., get<irs>()...};
            }
            // 异常检测 - insert
            template <bool is_move, xyu::size_t index, typename... Args2, xyu::size_t... ils, xyu::size_t... irs>
            static constexpr bool insert_noexcept_impl(xyu::isequence_c<ils...>, xyu::isequence_c<irs...>) noexcept
            {
                return noexcept(xyu::t_args_ins<Effective_t, index, xyu::t_remove_cvref<Args2>...>
                        {xyu::t_val<xyu::t_cond<is_move, TupleBase&&, const TupleBase&>>().template get<ils>()...,
                         xyu::t_val<Args2&&>()...,
                         xyu::t_val<xyu::t_cond<is_move, TupleBase&&, const TupleBase&>>().template get<irs>()...});
            }
            template <bool is_move, xyu::size_t index, typename... Args2>
            static constexpr bool insert_noexcept = []
            {
                if constexpr (index >= count()) return append_noexcept<is_move, Args2...>;
                else return insert_noexcept_impl<is_move, index, Args2...>(xyu::make_isequence<0, index>(), xyu::make_isequence<index, sizeof...(Indexs)>());
            }();

            // 获取移除指定元素后的索引序列
            template <typename Target, xyu::size_t limit, xyu::size_t... ils, xyu::size_t... ias>
            static constexpr auto remove_index(xyu::isequence_c<ias...>, xyu::isequence_c<ils...> = xyu::isequence_c<>{}) noexcept
            { return xyu::isequence_c<ils..., ias...>{}; }
            template <typename Target, xyu::size_t limit, xyu::size_t ia, xyu::size_t... ias, xyu::size_t... ils, typename = xyu::t_enable<limit>>
            static constexpr auto remove_index(xyu::isequence_c<ia, ias...>, xyu::isequence_c<ils...> = xyu::isequence_c<>{}) noexcept
            {
                if constexpr (xyu::t_is_same<Target, arg_t<ia>>)
                     return remove_index<Target, limit - 1>(xyu::isequence_c<ias...>{}, xyu::isequence_c<ils...>{});
                else return remove_index<Target, limit>(xyu::isequence_c<ias...>{}, xyu::isequence_c<ils..., ia>());
            }
            // 辅助函数 - remove
            template <typename Target, xyu::size_t limit, xyu::size_t... indexs>
            constexpr auto remove_impl(xyu::isequence_c<indexs...>) const&
            { return xyu::t_args_rem<Effective_t, Target, limit> {get<indexs>()...}; }
            template <typename Target, xyu::size_t limit, xyu::size_t... indexs>
            constexpr auto remove_impl(xyu::isequence_c<indexs...>) &&
            { return xyu::t_args_rem<Effective_t, Target, limit> {get<indexs>()...}; }
            // 异常检测 - remove
            template <bool is_move, typename Target, xyu::size_t limit, xyu::size_t... indexs>
            static constexpr bool remove_noexcept_impl(xyu::isequence_c<indexs...>) noexcept
            {
                return noexcept(xyu::t_args_rem<Effective_t, Target, limit>
                        {xyu::t_val<xyu::t_cond<is_move, TupleBase&&, const TupleBase&>>().template get<indexs>()...});
            }
            template <bool is_move, typename Target, xyu::size_t limit>
            static constexpr bool remove_noexcept = remove_noexcept_impl<is_move, Target, limit>(
                    remove_index<Target, limit>(xyu::isequence_c<Indexs...>{}));

            // 辅助函数 - erase
            template <xyu::size_t start, xyu::size_t len, xyu::size_t... ils, xyu::size_t... irs>
            constexpr auto erase_impl(xyu::isequence_c<ils...>, xyu::isequence_c<irs...>) const&
            { return xyu::t_args_del<Effective_t, start, len>{get<ils>()..., get<irs>()...}; }
            template <xyu::size_t start, xyu::size_t len, xyu::size_t... ils, xyu::size_t... irs>
            constexpr auto erase_impl(xyu::isequence_c<ils...>, xyu::isequence_c<irs...>) &&
            { return xyu::t_args_del<Effective_t, start, len>{get<ils>()..., get<irs>()...}; }
            // 异常检测 - erase
            template <bool is_move, xyu::size_t start, xyu::size_t len, xyu::size_t... ils, xyu::size_t... irs>
            static constexpr bool erase_noexcept_impl(xyu::isequence_c<ils...>, xyu::isequence_c<irs...>) noexcept
            {
                return noexcept(xyu::t_args_del<Effective_t, start, len>
                        {xyu::t_val<xyu::t_cond<is_move, TupleBase&&, const TupleBase&>>().template get<ils>()...,
                         xyu::t_val<xyu::t_cond<is_move, TupleBase&&, const TupleBase&>>().template get<irs>()...});
            }
            template <bool is_move, xyu::size_t start, xyu::size_t len>
            static constexpr bool erase_noexcept = []
            {
                constexpr xyu::size_t s = xyu::min(start, sizeof...(Indexs));
                constexpr xyu::size_t l = xyu::min(len, sizeof...(Indexs) - s);
                return erase_noexcept_impl<is_move, s, l>(xyu::make_isequence<0, s>(),
                                                          xyu::make_isequence<s + l, sizeof...(Indexs)>());
            }();

            // 异常检测 - concat
            template <bool is_move1, bool is_move2, typename Derived2, xyu::size_t Istart2, xyu::size_t... Indexs2, typename... Args2>
            static constexpr bool concat_noexcept_impl(xyu::typelist_c<TupleBase<Derived2, Istart2, xyu::isequence_c<Indexs2...>, Args2...>>) noexcept
            {
                using TB = TupleBase<Derived2, Istart2, xyu::isequence_c<Indexs2...>, Args2...>;
                return noexcept(xyu::t_args_concat<Effective_t, xyu::t_args_slice<Tuple<Args2...>, Istart2, sizeof...(Indexs2)>>
                        {xyu::t_val<xyu::t_cond<is_move1, TupleBase&&, const TupleBase&>>().template get<Indexs>()...,
                         xyu::t_val<xyu::t_cond<is_move2, TB&&, const TB&>>().template get<Indexs2>()...});
            }
            template <bool is_move1, bool is_move2, typename Tp>
            static constexpr bool concat_noexcept = compare_noexcept_impl(xyu::typelist_c<Tp>{});

            // 辅助函数 - slice
            template <xyu::size_t start, xyu::size_t len, xyu::size_t... indexs>
            constexpr auto slice_impl(xyu::isequence_c<indexs...>) const&
            { return xyu::t_args_slice<Effective_t, start, len>{get<indexs>()...}; }
            template <xyu::size_t start, xyu::size_t len, xyu::size_t... indexs>
            constexpr auto slice_impl(xyu::isequence_c<indexs...>) &&
            { return xyu::t_args_slice<Effective_t, start, len>{get<indexs>()...}; }
            // 异常检测 - slice
            template <bool is_move, xyu::size_t start, xyu::size_t len, xyu::size_t... indexs>
            static constexpr bool slice_noexcept_impl(xyu::isequence_c<indexs...>) noexcept
            {
                return noexcept(xyu::t_args_slice<Effective_t, start, len>
                        {xyu::t_val<xyu::t_cond<is_move, TupleBase&&, const TupleBase&>>().template get<indexs>()...});
            }
            template <bool is_move, xyu::size_t start, xyu::size_t len>
            static constexpr bool slice_noexcept = []
            {
                constexpr xyu::size_t s = xyu::min(start, sizeof...(Indexs));
                constexpr xyu::size_t l = xyu::min(len, sizeof...(Indexs) - s);
                return slice_noexcept_impl<is_move, s, l>(xyu::make_isequence<s, s + l>());
            }();
        };
    }

    namespace __
    {
        // 获取元素类型
        template <xyu::size_t start, xyu::size_t len, typename... Args, xyu::size_t index>
        struct tuple_arg_t<TupleView<Tuple<Args...>, start, len>, index>
        { using type = xyu::t_args_get<Tuple<Args...>, index>; };
        template <xyu::size_t start, xyu::size_t len, typename... Args, xyu::size_t index>
        struct tuple_arg_t<TupleView<const Tuple<Args...>, start, len>, index>
        { using type = xyu::t_args_get<Tuple<xyu::t_add_const_inref<Args>...>, index>; };

        // 获取元素索引
        template <typename Tp, xyu::size_t start, xyu::size_t len, typename Arg, xyu::size_t istart, xyu::size_t iend>
        struct tuple_arg_i<TupleView<Tp, start, len>, Arg, istart, iend>
        : xyu::size_c<xyu::t_args_find<Tp, Arg, istart, iend>> {};

        // Tuple 视图
        template <typename Tp, xyu::size_t start, typename Index>
        struct TupleViewImpl;
        template <xyu::size_t start, xyu::size_t... Indexs, typename... Args>
        struct TupleViewImpl<Tuple<Args...>, start, xyu::isequence_c<Indexs...>>
        : TupleBase<TupleView<Tuple<Args...>, start, sizeof...(Indexs)>, start, xyu::isequence_c<Indexs...>, Args...> {};
        template <xyu::size_t start, xyu::size_t... Indexs, typename... Args>
        struct TupleViewImpl<const Tuple<Args...>, start, xyu::isequence_c<Indexs...>>
        : TupleBase<TupleView<const Tuple<Args...>, start, sizeof...(Indexs)>, start, xyu::isequence_c<Indexs...>, Args...> {};
    }
    /**
     * @brief 一个表示对 `Tuple` 部分连续元素的非拥有（non-owning）视图。
     * @details
     *   `TupleView` 内部只持有一个对原始 `Tuple` 的引用，允许以零开销的方式
     *   操作原始元组的一个子序列。对视图的修改会直接反映在原始元组上。
     *
     * @tparam Tp    原始 `Tuple` 的类型（可以是 `const` 限定的）。
     * @tparam start 视图在原始元组中的起始索引。
     * @tparam len   视图的长度（元素个数）。
     */

    template <typename Tp, xyu::size_t start, xyu::size_t len>
    struct TupleView : __::TupleViewImpl<Tp, start, decltype(xyu::make_isequence<len>())>
    {
    private:
        // 原元组引用
        Tp& tp;

    public:
        // 构造
        constexpr TupleView(Tp& tp) noexcept : tp(tp) {}
        // 同类型元组视图
        template <typename Tp2, xyu::size_t start2, xyu::size_t len2, typename = xyu::t_enable<xyu::t_is_same_nocv<Tp, Tp2>>>
        constexpr TupleView(TupleView<Tp2, start2, len2>& tpl) noexcept : tp(tpl.tp) {}
        // 同类型const元组视图
        template <typename Tp2, xyu::size_t start2, xyu::size_t len2, typename = xyu::t_enable<xyu::t_is_same_nocv<Tp, Tp2>>>
        constexpr TupleView(const TupleView<Tp2, start2, len2>& tpl) noexcept : tp(tpl.tp) {}

        // 获取元素类型
        template <xyu::size_t index>
        using arg_t = typename __::tuple_arg_t<TupleView, index + start>::type;
        // 获取元素索引
        template <typename Arg, xyu::size_t istart = 0, xyu::size_t iend = len>
        static constexpr xyu::size_t arg_i = __::tuple_arg_i<TupleView, Arg, istart + start, istart + xyu::min(iend, len)>::value - istart;

    private:
        using Org_t = Tp;
        template <typename, xyu::size_t, xyu::size_t> friend struct TupleView;
        template <typename Derived, xyu::size_t Istart, typename Indexs, typename... Args> friend struct __::TupleBase;

        // 获取元素
        template <xyu::size_t index, typename Test = Tp, typename = xyu::t_enable<!xyu::t_is_const<Test>>>
        constexpr arg_t<index>& inget() noexcept { return tp.template get<index + start>(); }
        // 获取元素
        template <xyu::size_t index>
        constexpr const arg_t<index>& inget() const noexcept { return static_cast<const Tp&>(tp).template get<index + start>(); }
    };

    namespace __
    {
        // 获取元素类型
        template <typename... Args, xyu::size_t index>
        struct tuple_arg_t<Tuple<Args...>, index>
        { using type = xyu::t_args_get<Tuple<Args...>, index>; };
        // 获取元素索引
        template <typename... Args, typename Arg, xyu::size_t start, xyu::size_t end>
        struct tuple_arg_i<Tuple<Args...>, Arg, start, end>
        : xyu::size_c<xyu::t_args_find<Tuple<Args...>, Arg, start, end>> {};

        // Tuple 转发
        template <typename Index, typename... Args>
        struct TupleImpl;
        template <xyu::size_t... Indexs, typename... Args>
        struct TupleImpl<xyu::isequence_c<Indexs...>, Args...> : protected xyu::t_ebo<Args, Indexs>..., 
                                        TupleBase<Tuple<Args...>, 0, xyu::isequence_c<Indexs...>, Args...> 
        {
            // 默认构造函数
            constexpr TupleImpl() = default;

            // 构造函数 (辅助列表初始化推导)
            constexpr TupleImpl(const Args&... args) noexcept(xyu::t_can_nothrow_cpconstr<Args...>)
            : xyu::t_ebo<Args, Indexs>(args)... {}

            // 任意构造函数
            template <typename Arg2, typename... Args2, xyu::t_enable<((sizeof...(Args) > 1 || ((... && xyu::t_can_construct<Args, Args2>)))), bool> = true>
            constexpr TupleImpl(Arg2&& arg, Args2&&... args) noexcept(noexcept(TupleImpl{xyu::native_v, xyu::forward<Arg2>(arg), xyu::forward<Args2>(args)...}))
            : TupleImpl{xyu::native_v, xyu::forward<Arg2>(arg), xyu::forward<Args2>(args)...} {}

            // 元组解包构造
            template <typename Arg2, xyu::t_enable<((sizeof...(Args) != 1 || (... && !xyu::t_can_construct<Args, Arg2>)) && xyu::t_remove_cvref<Arg2>::count() == sizeof...(Args)) && !xyu::t_is_base_of<TupleImpl, xyu::t_remove_cvref<Arg2>>, bool> = false>
            constexpr TupleImpl(Arg2&& arg) noexcept(noexcept(TupleImpl{xyu::make_isequence<xyu::t_remove_cvref<Arg2>::count()>(), xyu::forward<Arg2>(arg)}))
            : TupleImpl{xyu::make_isequence<xyu::t_remove_cvref<Arg2>::count()>(), xyu::forward<Arg2>(arg)} {}

            // 分段构造函数
            template <xyu::size_t unpark_depth, typename... Args2>
            constexpr TupleImpl(xyu::size_c<unpark_depth>, Args2&&... args) noexcept(construct_noexcept<unpark_depth, Args2...>());

        private:
            // 辅助函数 - 任意构造
            template <typename... Args2>
            constexpr TupleImpl(xyu::native_t, Args2&&... args) noexcept((... && xyu::t_can_nothrow_construct<Args, Args2>))
            : xyu::t_ebo<Args, Indexs>(xyu::forward<Args2>(args))... {}

            // 辅助函数 - 元组解包
            template <xyu::size_t... indexs, typename Arg2>
            constexpr TupleImpl(xyu::isequence_c<indexs...>, Arg2&& arg) noexcept((... && xyu::t_can_nothrow_construct<Args, xyu::t_sync<Arg2, __::tuple_arg_t<xyu::t_remove_cvref<Arg2>, indexs>>>))
            : TupleImpl{xyu::forward<Arg2>(arg).template get<indexs>()...} {}

            // 异常检测 - 分段构造
            template <xyu::size_t unpark_depth, typename... Args2>
            static constexpr bool construct_noexcept() noexcept;
        };

        template <>
        struct TupleImpl<xyu::isequence_c<>> : TupleBase<Tuple<>, 0, xyu::isequence_c<>>
        {
            // 默认构造函数
            constexpr TupleImpl() noexcept = default;
        };
    }
    /**
     * @brief 一个拥有其元素的、功能丰富的异构类型安全容器。
     * @details
     *   `Tuple` 可以存储任意数量、任意类型的元素。它通过 `xyu::t_ebo` 实现，对于空类型的元素，可以实现空基类优化以节省空间。
     *   除了标准的元素访问外，它还提供了一套强大的编译期接口，用于生成新的、修改过的元组类型，实现了高度的元编程灵活性。
     *
     * @note
     *   - **构造**: 支持从元素列表直接构造，也支持从另一个元组或视图“解包”构造。
     *   - **分段构造**: 通过传递 `xyu::size_c<N>` 作为第一个参数，可以实现“分段解包构造”，控制构造函数解包嵌套元组的深度。
     */
    template <typename... Args>
    struct Tuple : __::TupleImpl<decltype(xyu::make_isequence<sizeof...(Args)>()), Args...>
    {
        // 继承构造函数
        using __::TupleImpl<decltype(xyu::make_isequence<sizeof...(Args)>()), Args...>::TupleImpl;
        // 获取元素类型
        template <xyu::size_t index>
        using arg_t = xyu::t_args_get<Tuple, index>;
        // 获取元素索引
        template <typename Arg, xyu::size_t start = 0, xyu::size_t end = sizeof...(Args)>
        static constexpr xyu::size_t arg_i = xyu::t_args_find<Tuple, Arg, start, end>;

    private:
        using Org_t = Tuple<Args...>;
        template <typename, xyu::size_t, typename, typename...> friend struct __::TupleBase;

        // 获取元素
        template <xyu::size_t index>
        constexpr arg_t<index>& inget() noexcept { return this->xyu::t_ebo<arg_t<index>, index>::base(); }
        // 获取元素
        template <xyu::size_t index>
        constexpr const arg_t<index>& inget() const noexcept { return this->xyu::t_ebo<arg_t<index>, index>::base(); }
    };

    // 推导指引
    template <typename... Args>
    Tuple(Args&&...) -> Tuple<xyu::t_remove_cvref<Args>...>;
}

/// 类型属性
namespace xylu::xytraits
{
    namespace __
    {
        template <typename T>
        struct is_tuple : false_c {};
        template <typename... Args>
        struct is_tuple<xylu::xycontain::Tuple<Args...>> : true_c {};
        template <typename Tp, xyu::size_t start, xyu::size_t len>
        struct is_tuple<xylu::xycontain::TupleView<Tp, start, len>> : true_c {};
        template <typename Derived, xyu::size_t Istart, typename Indexs, typename... Args>
        struct is_tuple<xylu::xycontain::__::TupleBase<Derived, Istart, Indexs, Args...>> : true_c {};
    }
    /// 判断是否为元组类型 (包括 TupleBase, TupleView, Tuple)
    template <typename... T>
    constexpr bool t_is_tuple = (... && (__::is_tuple<t_remove_cvref<T>>::value));
}

/// 元组构造
namespace xylu::xycontain
{
    /**
     * @brief 创建一个 `Tuple` 对象，自动推导其元素类型。
     * @details
     *   这是创建 `Tuple` 的首选方式。它会自动移除参数的 cv限定符和引用。
     *   若要存储引用，请使用 `xyu::make_refer` 包装参数。
     * @example
     *   auto t1 = make_tuple(10, 'c', true); // -> Tuple˂int, char, bool˃
     *   int x = 5;
     *   auto t2 = make_tuple(xyu::make_refer(x)); // -> Tuple˂xyu::t_refer˂int˃˃
     */
    template <typename... Args>
    constexpr auto make_tuple(Args&&... args) noexcept(noexcept(Tuple{ xyu::forward<Args>(args)... }))
    { return Tuple{ xyu::forward<Args>(args)... }; }

    namespace __
    {
        // 逐步构造
        template <xyu::size_t unpark_depth, typename Tp, xyu::size_t... Indexs>
        constexpr auto make_typw_constr(Tp tp, xyu::isequence_c<Indexs...>) noexcept { return tp; }
        template <xyu::size_t unpark_depth, typename Tp, xyu::size_t... Indexs, typename... Args, typename = xyu::t_enable<unpark_depth == 0>>
        constexpr auto make_typw_constr(Tp tp, xyu::isequence_c<Indexs...>, Args&&... args) noexcept(noexcept(xyu::t_args_add<Tp, Args...>(xyu::move(tp).template get<Indexs>()..., xyu::forward<Args>(args)...)))
        { return xyu::t_args_add<Tp, Args...>(xyu::move(tp).template get<Indexs>()..., xyu::forward<Args>(args)...); }

        template <xyu::size_t unpark_depth, typename Tp, xyu::size_t... tIndexs, typename Arg, xyu::size_t... Indexs>
        constexpr auto make_typw_constr_tp(Tp& tp, xyu::isequence_c<tIndexs...>, Arg&& arg, xyu::isequence_c<Indexs...>) noexcept(noexcept(make_typw_constr<unpark_depth-1>(tp, xyu::isequence_c<tIndexs...>{}, xyu::forward<Arg>(arg).template get<Indexs>()...)))
        { return make_typw_constr<unpark_depth-1>(tp, xyu::isequence_c<tIndexs...>{}, xyu::forward<Arg>(arg).template get<Indexs>()...); }

        template <xyu::size_t unpark_depth, typename Tp, xyu::size_t... Indexs, typename Arg, typename... Args, xyu::t_enable<unpark_depth && !xyu::t_is_tuple<Arg>, bool> = false>
        constexpr auto make_typw_constr(Tp tp, xyu::isequence_c<Indexs...>, Arg&& arg, Args&&... args) noexcept(noexcept(make_typw_constr<unpark_depth>(xyu::t_args_add<Tp, Arg>(xyu::move(tp).template get<Indexs>()..., xyu::forward<Arg>(arg)),
                                                                                                                                                    xyu::isequence_c<Indexs..., sizeof...(Indexs)>{}, xyu::forward<Args>(args)...)))
        {
            auto tp2 = xyu::t_args_add<Tp, Arg>(xyu::move(tp).template get<Indexs>()..., xyu::forward<Arg>(arg));
            return make_typw_constr<unpark_depth>(tp2, xyu::isequence_c<Indexs..., sizeof...(Indexs)>{}, xyu::forward<Args>(args)...);
        }
        template <xyu::size_t unpark_depth, typename Tp, xyu::size_t... Indexs, typename Arg, typename... Args, xyu::t_enable<unpark_depth && xyu::t_is_tuple<Arg>, bool> = true>
        constexpr auto make_typw_constr(Tp tp, xyu::isequence_c<Indexs...>, Arg&& arg, Args&&... args) noexcept(noexcept(make_typw_constr<unpark_depth>(make_typw_constr_tp<unpark_depth>(tp, xyu::isequence_c<Indexs...>{}, xyu::forward<Arg>(arg), xyu::make_isequence<xyu::t_remove_cvref<Arg>::count()>()),
                                                                          xyu::make_isequence<decltype(make_typw_constr_tp<unpark_depth>(tp, xyu::isequence_c<Indexs...>{}, xyu::forward<Arg>(arg), xyu::make_isequence<xyu::t_remove_cvref<Arg>::count()>()))::count()>(), xyu::forward<Args>(args)...)))
        {
            auto tp2 = make_typw_constr_tp<unpark_depth>(tp, xyu::isequence_c<Indexs...>{}, xyu::forward<Arg>(arg), xyu::make_isequence<xyu::t_remove_cvref<Arg>::count()>());
            return make_typw_constr<unpark_depth>(tp2, xyu::make_isequence<decltype(tp2)::count()>(), xyu::forward<Args>(args)...);
        }
    }
    /**
     * @brief 通过指定解包深度，从一系列参数（可能包含其他元组）构造一个新的 `Tuple`。
     * @tparam unpark_depth 解包的递归深度。
     *        - `0`: 不解包，将参数（包括元组）直接作为新元组的元素。
     *        - `1`: 将遇到的第一层元组参数展开，将其元素作为新元组的元素。
     *        - `N`: 递归地解包 `N` 层。
     * @example
     *   auto t = make_tuple(1, 2); // Tuple˂int, int˃
     *   make_tuple(xyu::size_c<0>(), t, 3) // -> Tuple˂Tuple˂int, int˃, int˃
     *   make_tuple(xyu::size_c<1>(), t, 3) // -> Tuple˂int, int, int˃
     */
    template <xyu::size_t unpark_depth, typename... Args>
    constexpr decltype(auto) make_tuple(xyu::size_c<unpark_depth>, Args&&... args) noexcept(noexcept(__::make_typw_constr<unpark_depth>(Tuple<>{}, xyu::isequence_c<>{}, xyu::t_val<Args>()...)))
    {
        return __::make_typw_constr<unpark_depth>(Tuple<>{}, xyu::isequence_c<>{}, xyu::forward<Args>(args)...);
    }

    namespace __
    {
        // 异常检测
        template <xyu::size_t... Indexs, typename... Args>
        template <xyu::size_t unpark_depth, typename... Args2>
        constexpr bool TupleImpl<xyu::isequence_c<Indexs...>, Args...>::construct_noexcept() noexcept
        {
            return noexcept(TupleImpl(make_tuple(xyu::size_c<unpark_depth>{}, xyu::t_val<Args2>()...)));
        }

        // 分段构造
        template <xyu::size_t... Indexs, typename... Args>
        template <xyu::size_t unpark_depth, typename... Args2>
        constexpr TupleImpl<xyu::isequence_c<Indexs...>, Args...>::TupleImpl(xyu::size_c<unpark_depth>, Args2&&... args) noexcept(construct_noexcept<unpark_depth, Args2...>())
        : TupleImpl{make_tuple(xyu::size_c<unpark_depth>{}, xyu::forward<Args2>(args)...)} {}
    }
}

/// 格式化
namespace xylu::xystring
{
    template <typename Derived, xyu::size_t start, xyu::size_t... Indexs, typename... Args>
    struct Formatter<xylu::xycontain::__::TupleBase<Derived, start, xyu::isequence_c<Indexs...>, Args...>>
    {
        using T = xylu::xycontain::__::TupleBase<Derived, start, xyu::isequence_c<Indexs...>, Args...>;

        /// 预解析元组类型格式串
        template <xyu::size_t index>
        constexpr static void prepare_impl(StringView& ptn, StringView& ex, xyu::size_t& count, bool& peach, bool& eeach)
        {
            if (XY_UNLIKELY(count == -1)) return;
            auto ptnex = update_ptnex(ptn, ex, peach, eeach);
            using Arg = typename T::template arg_t<index>;
            xyu::size_t add = __::call_formatter_prepare<Arg>(Format_Layout{}, ptnex.ptn, ptnex.ex);
            if (XY_UNLIKELY(add == -1)) count = -1;
            else count += add;
        }
        constexpr static xyu::size_t prepare(StringView pattern, const StringView& expand) noexcept
        {
            auto es = parse_split(expand);
            xyu::size_t count = 2 + sizeof...(Indexs) > 1 ? (sizeof...(Indexs) - 1) * es.split.size() : 0;
            bool ptnforeach = pattern.find(',') == -1;
            bool exforeach = expand.find(',') == -1;
            (..., prepare_impl<Indexs>(pattern, es.last, count, ptnforeach, exforeach));
            return count;
        }

        /// 解析元组类型格式串
        template <xyu::size_t index>
        constexpr static void parse_impl(const T& tp, StringView& ptn, StringView& ex, xyu::size_t& count, bool& peach, bool& eeach)
        {
            auto ptnex = update_ptnex(ptn, ex, peach, eeach);
            using Arg = typename T::template arg_t<index>;
            count += __::call_formatter_parse<Arg>(&tp.template get<index>(), Format_Layout{}, ptnex.ptn, ptnex.ex);
        }
        static xyu::size_t parse(const T& tp, StringView pattern, const StringView& expand)
        {
            auto es = parse_split(expand);
            xyu::size_t count = 2 + sizeof...(Indexs) > 1 ? (sizeof...(Indexs) - 1) * es.split.size() : 0;
            bool ptnforeach = pattern.find(',') == -1;
            bool exforeach = expand.find(',') == -1;
            (..., parse_impl<Indexs>(tp, pattern, es.last, count, ptnforeach, exforeach));
            return count;
        }

        /// 运行期预解析元组类型格式串
        template <xyu::size_t index>
        constexpr static void preparse_impl(const T& tp, StringView& ptn, StringView& ex, xyu::size_t& count, bool& peach, bool& eeach)
        {
            auto ptnex = update_ptnex(ptn, ex, peach, eeach);
            using Arg = typename T::template arg_t<index>;
            count += __::call_formatter_preparse<Arg>(&tp.template get<index>(), Format_Layout{}, ptnex.ptn, ptnex.ex);
        }
        static xyu::size_t preparse(const T& tp, StringView pattern, const StringView& expand)
        {
            auto es = parse_split(expand);
            xyu::size_t count = 2 + sizeof...(Indexs) > 1 ? (sizeof...(Indexs) - 1) * es.split.size() : 0;
            bool ptnforeach = pattern.find(',') == -1;
            bool exforeach = expand.find(',') == -1;
            (..., preparse_impl<Indexs>(tp, pattern, es.last, count, ptnforeach, exforeach));
            return count;
        }

        /// 格式化元组类型
        template <typename Stream, xyu::size_t index>
        constexpr static void format_impl(Stream& out, const T& tp, const StringView& split, StringView& ptn, StringView& ex, bool& peach, bool& eeach)
        {
            if (index) out << split;
            auto ptnex = update_ptnex(ptn, ex, peach, eeach);
            __::call_formatter_format<typename T::template arg_t<index>>
                    (out, &tp.template get<index>(), Format_Layout{}, ptnex.ptn, ptnex.ex);
        }
        template <typename Stream>
        static void format(Stream& out, const T& tp, StringView pattern, const StringView& expand)
        {
            auto es = parse_split(expand);
            out << '(';
            bool ptnforeach = pattern.find(',') == -1;
            bool exforeach = expand.find(',') == -1;
            (..., format_impl<Stream, Indexs>(out, tp, es.split, pattern, es.last, ptnforeach, exforeach));
            out << ')';
        }

    private:
        // 获取分隔字符串与后续格式串
        struct expand_split { StringView split, last; };
        constexpr static expand_split parse_split(const StringView& expand) noexcept
        {
            // 空格式串
            if (expand.empty()) return { ",", {} };
            // 括号包裹
            if (expand.get(0) == '(')
            {
                auto i = expand.find(')');
                if (i == -1) return { expand, {} };
                auto j = expand.find(',', i + 1);
                if (j == -1) return { expand.subview(1, i - 1), {} };
                else return { expand.subview(1, i - 1), expand.subview(j + 1) };
            }
                // 直接
            else
            {
                auto i = expand.find(',');
                if (i == -1) return { expand, {} };
                else if (i == 0) return { ",", expand.subview(1) };
                else return { expand.subview(0, i), expand.subview(i + 1) };
            }
        }

        // 构建 ptn 和 ex 格式串
        struct ptnex_split { StringView ptn; StringView ex; };
        constexpr static ptnex_split update_ptnex(StringView& pattern, StringView& expand, bool& ptnforeach, bool& exforeach) noexcept
        {
            ptnex_split ptnex;
            if (ptnforeach) ptnex.ptn = pattern;
            else {
                auto i = pattern.find(',');
                if (i == -1) { ptnex.ptn = pattern; pattern = {}; ptnforeach = true; }
                else { ptnex.ptn = pattern.subview(0, i); pattern = pattern.subview(i + 1); }
            }
            if (exforeach) ptnex.ex = expand;
            else {
                auto i = expand.find(',');
                if (i == -1) { ptnex.ex = expand; expand = {}; exforeach = true; }
                else { ptnex.ex = expand.subview(0, i); expand = expand.subview(i + 1); }
            }
            return ptnex;
        }
    };

    /**
     * @brief `xylu::xycontain::Tuple` 的格式化器特化
     * @note 提供了对元组进行格式化输出的强大支持，可以分别控制每个元素的格式以及它们之间的分隔符。
     *
     * --- [ptn] (Pattern Specifier) ---
     * 用于为元组中的每个元素指定独立的 `pattern` 格式，由 ':' 引导。
     *
     * - **语法:** `":spec1,spec2,..."`
     * - **规则:**
     *   - 格式规范以逗号 `,` 分隔，依次对应元组中的每个元素。
     *   - 如果只提供一个规范（不含逗号），它将被应用于所有元素。
     *   - 如果某个元素的规范为空，则该元素使用默认格式。
     *   - 如果整个 `ptn` 部分为空，所有元素都使用默认格式。
     *
     * - **示例:**
     *   - `xyfmt("{:x,d}", Tuple(10, 20))`       -> `"(a,20)"` (第一个用16进制，第二个用10进制)
     *   - `xyfmt("{:+.2f}", Tuple(3.14, -1.5))` -> `"(+3.14,-1.50)"` (所有元素都保留两位小数并显示符号)
     *
     * --- [?ex] (Expand Specifier) - 扩展格式 ---
     * 用于控制元素间的分隔符以及为每个元素指定独立的 `expand` 格式，由 '?' 引导。
     *
     * - **语法:** `"?<separator>,spec1,spec2,..."`
     *
     * - **`<separator>` (分隔符) 部分:**
     *   - **默认:** 如果 `ex` 部分为空，或以逗号开头，分隔符默认为 `","`。
     *   - **自定义:** 逗号前的部分(没有则整个 `ex` 部分)被视为分隔符。
     *     - `"{? | }"` -> 分隔符为 `" | "`
     *   - **括号包裹:**
     *     - 使用括号包裹空字符串 `"{?()}"` 来指定空分隔符。
     *     - 括号内的内容即为分隔符，例如 `"{?(, )}"` 的分隔符是 `", "`。
     *
     * - **`spec` (元素扩展格式) 部分:**
     *   - 规则与 `ptn` 部分完全相同，用于为每个元素传递独立的 `expand` 格式。
     */
    template <typename... Args>
    struct Formatter<xylu::xycontain::Tuple<Args...>> : Formatter<xylu::xycontain::__::TupleBase<
            xylu::xycontain::Tuple<Args...>, 0, decltype(xyu::make_isequence<sizeof...(Args)>()), Args...>> {};

    template <xyu::size_t start, xyu::size_t len, typename... Args>
    struct Formatter<xylu::xycontain::TupleView<xylu::xycontain::Tuple<Args...>, start, len>> : Formatter<xylu::xycontain::__::TupleBase<
            xylu::xycontain::TupleView<xylu::xycontain::Tuple<Args...>, start, len>, start, decltype(xyu::make_isequence<len>()), Args...>> {};

    template <xyu::size_t start, xyu::size_t len, typename... Args>
    struct Formatter<xylu::xycontain::TupleView<const xylu::xycontain::Tuple<Args...>, start, len>> : Formatter<xylu::xycontain::__::TupleBase<
            xylu::xycontain::TupleView<const xylu::xycontain::Tuple<Args...>, start, len>, start, decltype(xyu::make_isequence<len>()), Args...>> {};
}

/// 哈希
namespace xylu::xymath
{
    template <typename Derived, xyu::size_t Istart, typename Indexs, typename... Args>
    constexpr xyu::size_t make_hash(const xylu::xycontain::__::TupleBase<Derived, Istart, Indexs, Args...>& tp) noexcept
    {
        xyu::size_t hash = 0;
        tp.foreach([&hash](const auto& v){ hash ^= make_hash(v); });
        return hash;
    }
}

/// 结构化绑定
namespace std
{
    template <typename T> struct tuple_size;
    template <xyu::size_t index, typename T> struct tuple_element;

    //获取元组容量
    template <typename... Args>
    struct tuple_size<xylu::xycontain::Tuple<Args...>> : xyu::size_c<sizeof...(Args)> {};
    //获取数据类型
    template <xyu::size_t index, typename... Args>
    struct tuple_element<index, xylu::xycontain::Tuple<Args...>>
    { using type = typename xylu::xycontain::Tuple<Args...>::template arg_t<index>; };
}

#pragma clang diagnostic pop