#pragma once

/**
 * @file    xyrange/ops.h
 * @brief   范围操作
 * @date    2026-07-30
 */

#include "./cmp.h"

namespace xylu::xyrange
{
    namespace __
    {
        // 检测迭代器操作是否抛出异常
        template <typename Rg>
        constexpr bool noexcept_ops() noexcept
        {
            static_assert(xyu::t_can_range<Rg>);
            using Iter = decltype(make_range(xyu::t_val<Rg>()).begin());
            if constexpr (!xyu::t_can_nothrow_range<Rg>) return false;
            if constexpr (!xyu::t_can_nothrow_cpconstr<Iter>) return false;
            if constexpr (!noexcept(__::nextiter(xyu::t_val<Iter>(), {}))) return false;
            if constexpr (xyu::t_is_diter<Iter>)
                return noexcept(__::previter(xyu::t_val<Iter>(), {}));
            return true;
        }

        // 通过头尾迭代器获取下一个迭代器
        template <typename Iter, typename Iter2>
        constexpr decltype(auto) nextiter(Iter it, xyu::size_t step, Iter2 end, xyu::size_t count)
        {
            if constexpr (xyu::t_is_diter<Iter> && !xyu::t_is_riter<Iter> && xyu::t_is_same<Iter, Iter2>)
                if (count <= xyu::size_t(-1) / 2 && step > (count - 1) / 2)
                    return previter(end, count - step);
            return nextiter(it, step);
        }
    }
    /**
     * @brief 索引访问
     * @param rg 可以构造范围的对象
     * @param index 支持正负索引
     * @details
     *   对于随机迭代器为O(1)，双向为O(n/2)，单向为O(n)；
     *   仅已知大小的有限迭代器支持负索引
     * @note 对于未知大小类型且索引为负时会先遍历一遍获取大小
     */
    template <typename Rg, typename = xyu::t_enable<xyu::t_can_range<Rg>>>
    constexpr decltype(auto) operator|(Rg&& rg, xyu::diff_t index) noexcept(__::noexcept_ops<Rg>() && noexcept(*xyu::make_range(xyu::t_val<Rg>()).begin()))
    {
        auto range = make_range(xyu::forward<Rg>(rg));
        if (range.indefinite() && index < 0)
        {
            xyu::size_t count = 0;
            auto i = range.begin();
            auto j = range.end();
            while (i != j) ++i, ++count;
            range = decltype(range){count, range.begin(), j};
        }
        if constexpr (!XY_RELEASE)
        {
            if (range.definite())
            {
                if (index >= 0 && XY_UNLIKELY(index >= range.count()))
                    xyu::E_Logic::abort_out_range(XY_CurPos("Range|index"), index, 0, range.count());
                if (index < 0 && XY_UNLIKELY(-index > range.count()))
                    xyu::E_Logic::abort_out_range(XY_CurPos("Range|index"), range.count() + index, 0, range.count());
            }
            else if (index < 0)
                xyu::E_Logic::abort_range_not_definite(XY_CurPos("Range|index"));
        }
        xyu::size_t abs = index < 0 ? range.count() + index : index;
        return *__::nextiter(range.begin(), abs, range.end(), range.count());
    }


    /// 方法适配器
    namespace RangeOps
    {
        // 获取子范围
        struct _OpsSlice { xyu::size_t start, len; };
        static _OpsSlice slice(xyu::size_t start = 0, xyu::size_t len = -1) noexcept { return {start, len}; }

        // 转换
        template <typename Func>
        struct _OpsTrans { Func func; };
        template <typename Func, typename Store = xyu::t_decay<Func>>
        static _OpsTrans<Store> transform(Func&& func) noexcept(xyu::t_can_init<Store, Func>) { return {xyu::forward<Func>(func)}; }

        // 过滤
        template <typename Fv>
        struct _OpsFliter { Fv fv; };
        template <typename Fv, typename Store = xyu::t_decay<Fv>>
        static _OpsFliter<Store> filter(Fv&& fv) noexcept(xyu::t_can_init<Store, Fv>) { return {xyu::forward<Fv>(fv)}; }

        // 连接两个不同范围
        template <typename Range>
        struct _OpsConcat { Range range; };
        template <typename Rg, xyu::t_enable<xyu::t_can_range<Rg>, bool> = true, typename Range = xyu::t_rem_cr<xyu::t_range<Rg>>>
        static _OpsConcat<Range> concat(Rg&& rg) noexcept(xyu::t_can_nothrow_range<Rg> && xyu::t_can_nothrow_cpconstr<Range>)
        { return {make_range(xyu::forward<Rg>(rg))}; }

        // 连接多个相同范围
        template <typename Range, xyu::size_t N>
        struct _OpsMerge { Range ranges[N]; bool bound = false; };
        template <typename... Rgs, xyu::t_enable<sizeof...(Rgs) && xyu::t_can_range<Rgs...> && xyu::t_is_same_nocr<decltype(make_range(xyu::t_val<Rgs>()))...>, bool> = true, typename Range = xyu::t_rem_cr<decltype(make_range(xyu::t_val<xyu::t_tps_get<xyu::typelist_c<Rgs...,void>, 0>>()))>>
        static _OpsMerge<Range, sizeof...(Rgs)> merge(Rgs&&... rgs) noexcept(xyu::t_can_nothrow_range<Rgs...> && xyu::t_can_nothrow_cpconstr<Range>)
        { return _OpsMerge<Range, sizeof...(Rgs)>{make_range(xyu::forward<Rgs>(rgs))...}; }
        template <typename... Rgs, xyu::t_enable<sizeof...(Rgs) && xyu::t_can_range<Rgs...> && xyu::t_is_same_nocr<decltype(make_range(xyu::t_val<Rgs>()))...>, bool> = true, typename Range = xyu::t_rem_cr<decltype(make_range(xyu::t_val<xyu::t_tps_get<xyu::typelist_c<Rgs...,void>, 0>>()))>>
        static _OpsMerge<Range, sizeof...(Rgs)> merge(bool bound, Rgs&&... rgs) noexcept(xyu::t_can_nothrow_range<Rgs...> && xyu::t_can_nothrow_cpconstr<Range>)
        { return _OpsMerge<Range, sizeof...(Rgs)>{make_range(xyu::forward<Rgs>(rgs))..., bound}; }

        // 分割
        template <typename Fv>
        struct _OpsSplit { Fv fv; };
        template <typename Fv, typename Store = xyu::t_decay<Fv>>
        static _OpsSplit<Store> split(Fv&& fv) noexcept(xyu::t_can_init<Store, Fv>)
        { return {xyu::forward<Fv>(fv)}; }

        // 展开
        struct _OpsJoin { bool bound = false; };
        static _OpsJoin join(bool bound = false) noexcept { return {bound}; }

        // 拼接展开
        template <typename Fv>
        struct _OpsJoinWith { Fv fv; bool bound = false; };
        template <typename Fv, typename Store = xyu::t_decay<Fv>>
        static _OpsJoinWith<Store> join(Fv&& fv, bool bound = false) noexcept(xyu::t_can_init<Store, Fv>)
        { return {fv, bound}; }

    };


    namespace __
    {
        // 检测迭代器构建与拷贝是否抛出异常
        template <typename Rg>
        constexpr bool noexcept_ops_copy() noexcept
        {
            static_assert(xyu::t_can_range<Rg>);
            using Iter = decltype(make_range(xyu::t_val<Rg>()).begin());
            if constexpr (!xyu::t_can_nothrow_range<Rg>) return false;
            return xyu::t_can_nothrow_cpconstr<Iter>;
        }

        // 检测所有参数都在允许范围内
        template <typename... T>
        constexpr void check_param(xyu::ErrorPos pos, T... params) noexcept
        {
            constexpr auto limit = xyu::size_t(-1) / 2;
            if constexpr (XY_DEBUG)
            {
                (..., (XY_UNLIKELY(params > limit) ?
                xyu::E_Logic::abort_over_limit(pos, params, 0, limit) : (void)0));
            }
        }

        // 根据类型迭代器/生成器创建空迭范围
        template <typename Range, typename Iter, typename Iter2>
        Range make_empty_range(Iter begin, Iter2 end)
        {
            if constexpr (xyu::t_is_gen_range<Range>) return {0, begin};
            else return {0, end, end};
        }
    }
    /**
     * @brief 获取子范围
     * @param rg 可以构造范围的对象
     * @param slice {start,len} 子范围
     * @details 可以将未知大小范围转换为已知大小范围
     */
    template <typename Rg>
    constexpr auto operator|(Rg&& rg, RangeOps::_OpsSlice slice) noexcept(__::noexcept_ops<Rg>())
    {
        static_assert(xyu::t_can_range<Rg>);
        __::check_param(XY_CurPos(), slice.start);
        auto range = make_range(xyu::forward<Rg>(rg));
        using Range = xyu::t_rem_cr<decltype(range)>;
        // 空范围
        if (XY_UNLIKELY(slice.start >= range.count()))
            return __::make_empty_range<Range>(range.begin(), range.end());
        // 未知范围
        if (range.indefinite())
        {
            auto i = range.begin();
            auto j = range.end();
            while (slice.start && i != j) ++i, --slice.start;
            if (slice.start) return __::make_empty_range<Range>(i, j);
            auto begin = i;
            xyu::size_t tmp = slice.len;
            while (slice.len && i != j) ++i, --slice.len;
            return Range{tmp - slice.len, begin, i};
        }
        // 已知范围
        auto begin = __::nextiter(range.begin(), slice.start, range.end(), range.count());
        xyu::size_t last = range.limited() ? range.count() - slice.start : range.count();
        if constexpr (xyu::t_is_gen_range<Range>)
            return Range{xyu::min(last, slice.len), begin};
        else if (slice.len >= last || range.unlimited())
            return Range{last, begin, range.end()};
        else
        {
            auto end = __::nextiter(begin, slice.len, range.end(), last);
            return Range{slice.len, begin, end};
        }
    }


    namespace __
    {
        // 存储封装
        template <typename Func>
        struct pack_trans_storage
        {
            template <typename Storage>
            struct inner : Storage
            {
                Func func;

                template <typename... Args, typename = xyu::t_enable<xyu::t_can_listinit<Storage, Args...>>>
                constexpr inner(Func func, Args... args) noexcept(xyu::t_can_nothrow_cpconstr<Func> && xyu::t_can_nothrow_listinit<Storage, Args...>)
                : func(func), Storage{xyu::forward<Args>(args)...} {}
            };
        };

        // 解引用封装
        template <typename Deref>
        struct pack_trans_deref : Deref
        {
            template <typename Iter>
            constexpr decltype(auto) operator()(Iter& it) noexcept(noexcept(it.func(base()(it))))
            { return it.func(base()(it)); }
        protected:
            constexpr Deref& base() noexcept { return static_cast<Deref&>(*this); }
        };
    }
    /**
     * @brief 转换范围
     * @param rg 可以构造范围的对象
     * @param trans 转换函数对象，作用于解引用后的值
     */
    template <typename Rg, typename Func>
    constexpr auto operator|(Rg&& rg, RangeOps::_OpsTrans<Func> trans) noexcept(__::noexcept_ops_copy<Rg>() && xyu::t_can_nothrow_cpconstr<Func>)
    {
        static_assert(xyu::t_can_range<Rg>);
        auto range = make_range(xyu::forward<Rg>(rg));

        using Iter = xyu::t_chg_iter<decltype(range.begin()),
            IterTag::Storage, xyu::t_pack_tmpl<__::pack_trans_storage<Func>::template inner>,
            IterTag::Dereference, xyu::t_pack_tmpl<__::pack_trans_deref>>;

        auto make_iter = [&](auto old_iter) { return Iter{trans.func, old_iter}; };
        if constexpr (xyu::t_is_gen_range<decltype(range)>)
            return Range<Iter>(range.count(), make_iter(range.begin()));
        else
            return Range<Iter>(range.count(), make_iter(range.begin()), make_iter(range.end()));
    }


    namespace __
    {
        template <typename Iter, typename Fv, bool is_gen>
        struct pack_fliter_iter
        {
            template <typename Gen, typename Test = Fv, xyu::t_enable<xyu::t_can_call<Test, decltype(*xyu::t_val<Iter>())>, bool> = true>
            constexpr static bool match(Gen& gen) noexcept(xyu::t_can_nothrow_call<Fv, decltype(*xyu::t_val<Iter>())>)
            { return gen.fv(*gen.p); }

            template <typename Gen, typename Test = Fv, xyu::t_enable<!xyu::t_can_call<Test, decltype(*xyu::t_val<Iter>())>, bool> = false>
            constexpr static bool match(Gen& gen) noexcept(noexcept(xyu::equals(gen.fv, *gen.p)))
            { return xyu::equals(gen.fv, *gen.p); }

            struct Storage
            {
                Fv fv;
                Iter p;
                xyu::t_cond<is_gen, int[0], Iter> end;

                template <bool Test = is_gen, typename = xyu::t_enable<!Test>>
                Storage(Fv& fv, Iter begin, Iter end) noexcept(xyu::t_can_nothrow_cpconstr<Fv, Iter>)
                : fv(fv), p(begin), end(end) {}

                template <typename T, bool Test = is_gen, typename = xyu::t_enable<Test>>
                Storage(Fv& fv, Iter begin, T&&) noexcept(xyu::t_can_nothrow_cpconstr<Fv, Iter>)
                : fv(fv), p(begin) {}

                Storage(const Storage&) = default;
                Storage(Storage&&) = default;
                Storage& operator=(const Storage& other) noexcept(noexcept(xyu::object_assign(fv,fv), p=p))
                {
                    xyu::object_assign(fv, other.fv);
                    p = other.p;
                    if constexpr (!is_gen) end = other.end;
                    return *this;
                }
                Storage& operator=(Storage&& other) noexcept(noexcept(xyu::object_assign(fv,xyu::move(fv)), p=xyu::move(p)))
                {
                    xyu::object_assign(fv, xyu::move(other.fv));
                    p = xyu::move(other.p);
                    if constexpr (!is_gen) end = xyu::move(other.end);
                    return *this;
                }
            };

            struct Valid
            {
                template <typename Gen>
                constexpr bool operator()(const Gen& gen) noexcept(noexcept(gen.p != gen.end))
                { return gen.p != gen.end; }
            };

            struct Increment
            {
                template <typename Gen>
                constexpr void operator()(Gen& gen) noexcept(noexcept(gen.p != gen.end && match((++gen.p, gen))))
                { while (gen.p != gen.end && match((++gen.p, gen))); }
            };

            struct Decrement
            {
                template <typename Gen>
                constexpr void operator()(Gen& gen) noexcept(noexcept(match((--gen.p, gen))))
                { while (match((--gen.p, gen))); }
            };

            using Gen = Iterator<Storage, Valid,
                IterPolicies::Default::Dereference,
                xyu::t_cond<xyu::t_is_void<typename Iter::Address_t>, void, IterPolicies::Iter::Address>,
                Increment,
                xyu::t_cond<xyu::t_is_void<typename Iter::Decrement_t>, void, Decrement>,
                void, void, void, void, void
            >;
        };
    }
    /**
     * @brief 过滤范围
     * @param rg 可以构造范围的对象
     * @param fliter 过滤函数（符合条件的会被过滤）或值
     * @note 返回未知大小范围
     */
    template <typename Rg, typename Fv, typename Range = xyu::t_rem_cr<xyu::t_range<Rg>>, typename Iter = decltype(xyu::t_val<Range>().begin()), typename HT = __::pack_fliter_iter<Iter, Fv, xyu::t_is_gen_range<Range>>>
    constexpr auto operator|(Rg&& rg, RangeOps::_OpsFliter<Fv> fliter) noexcept(__::noexcept_ops<Rg>() && xyu::t_can_nothrow_cpconstr<Fv> && noexcept(++xyu::t_val<typename HT::Gen&>()))
    {
        static_assert(xyu::t_can_range<Rg>);
        auto range = make_range(xyu::forward<Rg>(rg));
        auto begin = range.begin();
        auto end = range.end();

        using Gen = typename HT::Gen;
        Gen gen{fliter.fv, begin, end};
        return xyu::Range<Gen>(-2, gen && HT::match(gen) ? ++gen : gen);
    }


    namespace __
    {
        template <typename It1, typename It2, bool is_gen1, bool is_gen2>
        struct pack_concat_iter
        {
            struct Storage
            {
                It1 p1;
                xyu::t_cond<is_gen1, int[0], It1> e1;
                It2 p2;
                xyu::t_cond<is_gen2, int[0], It2> e2;

                template <bool Test = is_gen1 || is_gen2, typename = xyu::t_enable<!Test>>
                Storage(It1 b1, It1 e1, It2 b2, It2 e2) noexcept(xyu::t_can_nothrow_cpconstr<It1, It2>)
                : p1(b1), e1(e1), p2(b2), e2(e2) {}

                template <typename T, bool Test = !is_gen1 && is_gen2, typename = xyu::t_enable<Test>>
                Storage(It1 b1, It1 e1, It2 b2, T&&) noexcept(xyu::t_can_nothrow_cpconstr<It1, It2>)
                : p1(b1), e1(e1), p2(b2) {}

                template <typename T, bool Test = is_gen1 && !is_gen2, typename = xyu::t_enable<Test>>
                Storage(It1 b1, T&&, It2 b2, It2 e2) noexcept(xyu::t_can_nothrow_cpconstr<It1, It2>)
                : p1(b1), p2(b2), e2(e2) {}

                template <typename T, typename U, bool Test = is_gen1 && is_gen2, typename = xyu::t_enable<Test>>
                Storage(It1 b1, T&&, It2 b2, U&&) noexcept(xyu::t_can_nothrow_cpconstr<It1, It2>)
                : p1(b1), p2(b2) {}
            };

            struct Valid
            {
                template <typename Iter>
                constexpr bool operator()(const Iter& it) noexcept(noexcept(it.p2 != it.e2 || it.p1 != it.e1))
                { return it.p2 != it.e2 || it.p1 != it.e1; }
            };

            struct Dereference
            {
                template <typename Iter>
                constexpr decltype(auto) operator()(Iter& it) noexcept(noexcept(*it.p1, *it.p2, it.p1 == it.e1))
                {
                    using Ret = xyu::t_wide<decltype(*it.p1), decltype(*it.p2)>;
                    static_assert(!xyu::t_is_void<Ret>);
                    return it.p1 == it.e1 ? static_cast<Ret>(*it.p2) : static_cast<Ret>(*it.p1);
                }
            };

            struct Address
            {
                template <typename Iter>
                constexpr decltype(auto) operator()(Iter& it) noexcept(noexcept(it.p1.operator->(), it.p2.operator->(), it.p1 == it.e1))
                { return it.p1 == it.e1 ? it.p2.operator->() : it.p1.operator->(); }
            };

            struct Increment
            {
                template <typename Iter>
                constexpr void operator()(Iter& it) noexcept(noexcept(it.p1 != it.e1, ++it.p1, ++it.p2))
                {
                    if (it.p1 != it.e1) ++it.p1;
                    else ++it.p2;
                }
            };

            using Gen = Iterator<Storage, Valid, Dereference,
                xyu::t_cond<xyu::t_is_void<typename It1::Address_t> || xyu::t_is_void<typename It2::Address_t>, void, IterPolicies::Iter::Address>,
                Increment, void, void, void, void, void, void
            >;
        };
    }
    /**
     * @brief 拼接范围
     * @param rg 可以构造范围的对象
     * @param concat 待拼接的不同类型范围对象
     */
    template <typename Rg, typename Range2>
    constexpr auto operator|(Rg&& rg, RangeOps::_OpsConcat<Range2> concat) noexcept(__::noexcept_ops_copy<Rg>() && __::noexcept_ops_copy<Range2>())
    {
        static_assert(xyu::t_can_range<Rg>);
        auto range = make_range(xyu::forward<Rg>(rg));
        using Gen = typename __::pack_concat_iter<decltype(range.begin()), decltype(concat.range.begin()),
                            xyu::t_is_gen_range<decltype(range)>, xyu::t_is_gen_range<decltype(concat.range)>>::Gen;
        xyu::size_t count{};
        if (range.definite() && concat.range.definite())
        {
            count = range.count() + concat.range.count();
            if constexpr (XY_DEBUG) if (XY_UNLIKELY(count > xyu::size_t(-1) / 2))
                xyu::E_Logic::abort_out_range(XY_CurPos("Range|concat"), count, 0, xyu::size_t(-1) / 2);
        }
        else count = xyu::max(range.count(), concat.range.count());
        return xyu::Range<Gen>{count, Gen{range.begin(), range.end(), concat.range.begin(), concat.range.end()}};
    }


    namespace __
    {
        template <typename Iter, xyu::size_t N, bool is_gen>
        struct pack_merge_iter
        {
            struct Storage
            {
                Iter p[N];
                xyu::t_cond<is_gen, int[0], Iter> e[N];
                xyu::size_t index = 0;
            };

            struct Valid
            {
                template <typename Gen>
                constexpr bool operator()(const Gen& gen) noexcept
                { return gen.index != N; }
            };

            struct Dereference
            {
                template <typename Gen>
                constexpr decltype(auto) operator()(Gen& gen) noexcept(noexcept(*gen.p[0]))
                { return *gen.p[gen.index]; }
            };

            struct Address
            {
                template <typename Gen>
                constexpr decltype(auto) operator()(Gen& gen) noexcept(noexcept(gen.p[0].operator->()))
                { return gen.p[gen.index].operator->(); }
            };

            struct Increment
            {
                template <typename Gen>
                constexpr void operator()(Gen& gen) noexcept(noexcept(++gen.p[0]))
                { if (++gen.p[gen.index] == gen.e[gen.index]) ++gen.index; }
            };

            using Gen = Iterator<Storage, Valid, Dereference,
                xyu::t_cond<xyu::t_is_void<typename Iter::Address_t>, void, Address>,
                Increment, void, void, void, void, void, void
            >;
        };

        template <typename Range, xyu::size_t N, xyu::size_t... Ids>
        auto make_merge_range_impl(xyu::size_t count, Range& range, Range(&ranges)[N], xyu::isequence_c<Ids...>)
        {
            using Gen = typename pack_merge_iter<decltype(range.begin()), N + 1, xyu::t_is_gen_range<Range>>::Gen;
            auto start_fix = [](auto&& gen) -> decltype((gen))
            {
                while (gen.index <= N && gen.p[gen.index] == gen.e[gen.index]) ++gen.index;
                return gen;
            };
            if constexpr (xyu::t_is_gen_range<Range>)
                return xyu::Range<Gen>{count, start_fix(Gen{range.begin(), range.begin(), ranges[Ids].begin()...})};
            else return xyu::Range<Gen>{count, start_fix(Gen{range.begin(), range.begin(), ranges[Ids].begin()..., range.end(), ranges[Ids].end()...})};
        }
    }
    /**
     * @brief 连接范围
     * @param rg 可以构造范围的对象
     * @param merge 待连接的同类型范围对象数组
     * @details 若 bound 为 true 且 每个范围均已知大小时 连接范围也是已知大小
     */
    template <typename Rg, typename Range2, xyu::size_t N>
    constexpr auto operator|(Rg&& rg, RangeOps::_OpsMerge<Range2, N> merge) noexcept(__::noexcept_ops_copy<Rg>() && __::noexcept_ops<Range2>())
    {
        static_assert(xyu::t_can_range<Rg> && xyu::t_is_same_nocr<xyu::t_range<Rg>, Range2>);
        auto range = make_range(xyu::forward<Rg>(rg));

        xyu::size_t count = range.count();
        if (range.definite())
        {
            if (merge.bound) for (xyu::size_t i = 0; i <= N; ++i)
            {
                auto& r = merge.ranges[i];
                if (!r.definite()) { count = r.count(); break; }
                count += r.count();
                if constexpr (XY_DEBUG) if (XY_UNLIKELY(count > xyu::size_t(-1) / 2))
                    xyu::E_Logic::abort_out_range(XY_CurPos("Range|merge"), count, 0, xyu::size_t(-1) / 2);
            }
        }
        return __::make_merge_range_impl(count, range, merge.ranges, xyu::make_isequence<N>());
    }


    namespace __
    {
        template <typename Iter, typename Fv, bool is_gen>
        struct pack_split_iter
        {
            struct Increment
            {
                template <typename Gen, typename Test = Fv, xyu::t_enable<xyu::t_can_call<Test, decltype(*xyu::t_val<Iter>())>, bool> = true>
                constexpr static bool match(Gen& gen) noexcept(xyu::t_can_nothrow_call<Fv, decltype(*xyu::t_val<Iter>())>)
                { return gen.fv(*gen.over); }

                template <typename Gen, typename Test = Fv, xyu::t_enable<!xyu::t_can_call<Test, decltype(*xyu::t_val<Iter>())>, bool> = false>
                constexpr static bool match(Gen& gen) noexcept(noexcept(xyu::equals(gen.fv, *gen.over)))
                { return xyu::equals(gen.fv, *gen.over); }

                template <typename Gen>
                constexpr static void fix(Gen& gen) noexcept(noexcept(++(gen.p = gen.over) != gen.end, match(gen)))
                {
                    if (gen.over != gen.end)
                    {
                        if (match(gen)) gen.area = 0;
                        else
                        {
                            gen.area = 1;
                            while (++gen.over != gen.end && !match(gen)) ++gen.area;
                        }
                    }
                }

                template <typename Gen>
                constexpr void operator()(Gen& gen) noexcept(noexcept(fix(gen)))
                {
                    gen.area = 0;
                    gen.p = gen.over;
                    if (gen.over != gen.end)
                        while (++gen.over != gen.end && !match(gen)) ++gen.area;
                    gen.first = false;
                }
            };

            struct Storage
            {
                Fv fv;
                Iter p;
                xyu::t_cond<is_gen, int[0], Iter> end;
                Iter over;
                bool first = true;
                xyu::size_t area = 0;

                template <bool Test = is_gen, typename = xyu::t_enable<!Test>>
                Storage(Fv fv, Iter begin, Iter end) noexcept(xyu::t_can_nothrow_cpconstr<Fv, Iter> && noexcept(Increment::fix(*this)))
                : fv(fv), p(begin), end(end), over(begin) { Increment::fix(*this); first = true; }

                template <typename T, bool Test = is_gen, typename = xyu::t_enable<Test>>
                Storage(Fv fv, Iter begin, T&&) noexcept(xyu::t_can_nothrow_cpconstr<Fv, Iter> && noexcept(Increment::fix(*this)))
                : fv(fv), p(begin), over(begin) { Increment::fix(*this); first = true; }

                Storage(const Storage&) = default;
                Storage(Storage&&) = default;
                Storage& operator=(const Storage& other) noexcept(noexcept(xyu::object_assign(fv,fv), p=p))
                {
                    xyu::object_assign(fv, other.fv);
                    p = other.p;
                    if constexpr (!is_gen) end = other.end;
                    over = other.over;
                    first = other.first;
                    area = other.area;
                    return *this;
                }
                Storage& operator=(Storage&& other) noexcept(noexcept(xyu::object_assign(fv,xyu::move(fv)), p=xyu::move(p)))
                {
                    xyu::object_assign(fv, xyu::move(other.fv));
                    p = xyu::move(other.p);
                    if constexpr (!is_gen) end = xyu::move(other.end);
                    over = xyu::move(other.over);
                    first = other.first;
                    area = other.area;
                    return *this;
                }
            };

            struct Valid
            {
                template <typename Gen>
                constexpr bool operator()(const Gen& gen) noexcept(noexcept(gen.p != gen.end))
                { return gen.p != gen.end; }
            };

            struct Inner
            {
                struct Storage
                {
                    Iter p;
                    xyu::t_cond<is_gen, int[0], Iter> end;

                    template <bool Test = is_gen, typename = xyu::t_enable<!Test>>
                    Storage(Iter begin, Iter end) noexcept(xyu::t_can_nothrow_cpconstr<Fv, Iter>)
                    : p(begin), end(end) {}

                    template <typename T, bool Test = is_gen, typename = xyu::t_enable<Test>>
                    Storage(Iter begin, T&&) noexcept(xyu::t_can_nothrow_cpconstr<Fv, Iter>)
                    : p(begin) {}
                };

                struct Valid
                {
                    template <typename G>
                    constexpr bool operator()(const G& gen) noexcept(noexcept(gen.p != gen.end))
                    { return gen.p != gen.end; }
                };

                using Gen = Iterator<Storage, Valid,
                    IterPolicies::Default::Dereference,
                    xyu::t_cond<xyu::t_is_void<typename Iter::Address_t>, void, IterPolicies::Iter::Address>,
                    IterPolicies::Default::Increment, void, void, void, void, void, void
                >;
            };

            struct Dereference
            {
                using R = typename Inner::Gen;

                template <typename Gen>
                constexpr auto operator()(Gen& gen) noexcept(noexcept(xyu::Range<R>{gen.area, R{++Iter{gen.p}, gen.over}}))
                { return xyu::Range<R>{gen.area, R{gen.first ? gen.p : ++Iter{gen.p}, gen.over}}; }
            };

            using Gen = Generator<Storage, Valid, Dereference, Increment>;
        };
    }
    /**
     * @brief 分割范围
     * @param rg 可以构造范围的对象
     * @param split 分割函数对象，符合条件或值相等则进行一次分割
     * @note 返回未知大小范围
     */
    template <typename Rg, typename Fv, typename Range = xyu::t_rem_cr<xyu::t_range<Rg>>, typename Gen = typename __::pack_split_iter<decltype(xyu::t_val<Range>().begin()), Fv, xyu::t_is_gen_range<Range>>::Gen>
    constexpr auto operator|(Rg&& rg, RangeOps::_OpsSplit<Fv> split) noexcept(xyu::t_can_nothrow_range<Rg> && noexcept(xyu::Range<Gen>(-2, Gen{xyu::t_val<Fv>(), xyu::t_val<Range>().begin(), xyu::t_val<Range>().end()})))
    {
        static_assert(xyu::t_can_range<Rg> && (xyu::t_can_call<Fv&, xyu::t_access<Rg>> || xyu::t_can_equals<Fv&, xyu::t_access<Rg>>));
        auto range = make_range(xyu::forward<Rg>(rg));
        return xyu::Range<Gen>(-2, Gen{split.fv, range.begin(), range.end()});
    }

    namespace __
    {
        template <typename Iter, bool is_gen, typename = void>
        struct pack_join_iter;

        template <typename Iter, bool is_gen>
        struct pack_join_iter<Iter, is_gen, xyu::t_void<xyu::t_enable<xyu::t_can_range<xyu::t_access<Iter>>>>>
        {
            using Inner = decltype(make_range(*xyu::t_val<Iter>()).begin());
            constexpr static bool is_gen_inner = xyu::t_is_gen_range<xyu::t_range<xyu::t_access<Iter>>>;

            struct Increment
            {
                template <bool next = false, typename Gen>
                constexpr static void fix(Gen& gen) noexcept(noexcept(++gen.fp != gen.fend, gen.p = make_range(*gen.fp).begin()))
                {
                    if constexpr (next) ++gen.fp;
                    for (;gen.fp != gen.fend; ++gen.fp)
                    {
                        auto rg = make_range(*gen.fp);
                        gen.p = rg.begin();
                        if constexpr (!is_gen_inner) gen.end = rg.end();
                        if (gen.p != gen.end) break;
                    }
                }

                template <typename Gen>
                constexpr void operator()(Gen& gen) noexcept(noexcept(++gen.p, fix<true>(gen)))
                { if (++gen.p == gen.end) fix<true>(gen); }
            };

            struct Storage
            {
                Iter fp;
                xyu::t_cond<is_gen, int[0], Iter> fend;
                Inner p;
                xyu::t_cond<is_gen_inner, int[0], Inner> end;

                template <bool Test = is_gen || is_gen_inner, typename = xyu::t_enable<!Test>>
                Storage(Iter begin, Iter end) noexcept(xyu::t_can_nothrow_cpconstr<Iter> && noexcept(Increment::fix(*this)))
                : fp(begin), fend(end), p(make_range(*begin).begin()), end(make_range(*begin).end()) { Increment::fix(*this); }

                template <bool Test = !is_gen && is_gen_inner, xyu::t_enable<Test, bool> = true>
                Storage(Iter begin, Iter end) noexcept(xyu::t_can_nothrow_cpconstr<Iter> && noexcept(Increment::fix(*this)))
                : fp(begin), fend(end), p(make_range(*begin).begin()) { Increment::fix(*this); }

                template <typename T, bool Test = is_gen && !is_gen_inner, typename = xyu::t_enable<Test>>
                Storage(Iter begin, T&&) noexcept(xyu::t_can_nothrow_cpconstr<Iter> && noexcept(Increment::fix(*this)))
                : fp(begin), p(make_range(*begin).begin()), end(make_range(*begin).end()) { Increment::fix(*this); }

                template <typename T, bool Test = is_gen && is_gen_inner, xyu::t_enable<Test, bool> = false>
                Storage(Iter begin, T&&) noexcept(xyu::t_can_nothrow_cpconstr<Iter> && noexcept(Increment::fix(*this)))
                : fp(begin), p(make_range(*begin).begin()) { Increment::fix(*this); }
            };

            struct Valid
            {
                template <typename Gen>
                constexpr bool operator()(const Gen& gen) noexcept
                { return gen.fp != gen.fend; }
            };

            using Gen = Iterator<Storage, Valid,
                IterPolicies::Default::Dereference,
                xyu::t_cond<xyu::t_is_void<typename Iter::Address_t>, void, IterPolicies::Iter::Address>,
                Increment, void, void, void, void, void, void
            >;
        };
    }
    /**
     * @brief 展开范围
     * @param rg 可以构造范围的对象
     * @details 将嵌套范围展开为平铺范围
     * @details 若 bound 为 true 且 每个子范围均已知大小时 连接范围也是已知大小
     * @attention 范围不能为空
     */
    template <typename Rg, typename Range = xyu::t_rem_cr<xyu::t_range<Rg>>, typename Gen = typename __::pack_join_iter<decltype(xyu::t_val<Range>().begin()), xyu::t_is_gen_range<Range>>::Gen>
    constexpr auto operator|(Rg&& rg, RangeOps::_OpsJoin join) noexcept(xyu::t_can_nothrow_range<Rg> && noexcept(xyu::Range<Gen>(0, Gen{xyu::t_val<Range>().begin(), xyu::t_val<Range>().end()})))
    {
        static_assert(xyu::t_can_range<Rg> && xyu::t_can_range<xyu::t_access<Rg>>);
        auto range = make_range(xyu::forward<Rg>(rg));
        if constexpr (!XY_RELEASE) if (XY_UNLIKELY(range.begin() == range.end()))
            xyu::E_Logic::abort_out_range(XY_CurPos("Range|join"), 0, 0, 0);

        xyu::size_t count = range.count();
        if (range.definite() && join.bound)
        {
            count = 0;
            for (auto&& i : range)
            {
                auto inner = make_range(xyu::forward<decltype(i)>(i));
                if (!inner.definite()) { count = inner.count(); break; }
                count += inner.count();
                if constexpr (XY_DEBUG) if (XY_UNLIKELY(count > xyu::size_t(-1) / 2))
                    xyu::E_Logic::abort_out_range(XY_CurPos("Range|join"), count, 0, xyu::size_t(-1) / 2);
            }
        }
        return xyu::Range<Gen>(count, Gen{range.begin(), range.end()});
    }

    namespace __
    {
        template <typename Iter, typename Fv, bool is_gen, typename = void>
        struct pack_joinwith_iter;

        template <typename Iter, typename Fv, bool is_gen>
        struct pack_joinwith_iter<Iter, Fv, is_gen, xyu::t_void<decltype(make_range(*xyu::t_val<Iter>()).begin())>>
        {
            using Inner = decltype(make_range(*xyu::t_val<Iter>()).begin());
            constexpr static bool is_gen_inner = xyu::t_is_gen_range<decltype(make_range(*xyu::t_val<Iter>()))>;

            struct Increment
            {
                template <typename Gen>
                constexpr void operator()(Gen& gen) noexcept(noexcept(++gen.p == gen.end, ++gen.fp != gen.fend, gen.p = make_range(*gen.fp).begin()))
                {
                    if (gen.ins) gen.ins = false;
                    else ++gen.p;
                    if (gen.p == gen.end)
                    {
                        if (++gen.fp != gen.fend)
                        {
                            gen.ins = true;
                            auto r = make_range(*gen.fp);
                            gen.p = r.begin();
                            if constexpr (!is_gen_inner) gen.end = r.end();
                        }
                    }
                }
            };

            struct Storage
            {
                Fv fv;
                bool ins = true;
                Iter fp;
                xyu::t_cond<is_gen, int[0], Iter> fend;
                Inner p;
                xyu::t_cond<is_gen_inner, int[0], Inner> end;

                template <bool Test = is_gen || is_gen_inner, typename = xyu::t_enable<!Test>>
                Storage(Fv fv, Iter begin, Iter end) noexcept(xyu::t_can_nothrow_cpconstr<Iter, Inner, Fv> && noexcept(Increment{}(*this)))
                : fv(fv), fp(begin), fend(end), p(make_range(*begin).begin()), end(make_range(*begin).end()) { Increment{}(*this); }

                template <bool Test = !is_gen && is_gen_inner, xyu::t_enable<Test, bool> = true>
                Storage(Fv fv, Iter begin, Iter end) noexcept(xyu::t_can_nothrow_cpconstr<Iter, Inner, Fv> && noexcept(Increment{}(*this)))
                : fv(fv), fp(begin), fend(end), p(make_range(*begin).begin()) { Increment{}(*this); }

                template <typename U, bool Test = is_gen && !is_gen_inner, typename = xyu::t_enable<Test>>
                Storage(Fv fv, Iter begin, U&&) noexcept(xyu::t_can_nothrow_cpconstr<Iter, Inner, Fv> && noexcept(Increment{}(*this)))
                : fv(fv), fp(begin), p(make_range(*begin).begin()), end(make_range(*begin).end()) { Increment{}(*this); }

                template <typename U, bool Test = is_gen && is_gen_inner, xyu::t_enable<Test, bool> = false>
                Storage(Fv fv, Iter begin, U&&) noexcept(xyu::t_can_nothrow_cpconstr<Iter, Inner, Fv> && noexcept(Increment{}(*this)))
                : fv(fv), fp(begin), p(make_range(*begin).begin()) { Increment{}(*this); }

                Storage(const Storage&) = default;
                Storage(Storage&&) = default;
                Storage& operator=(const Storage& other) noexcept(noexcept(xyu::object_assign(fv,fv), fp=fp, p=p))
                {
                    xyu::object_assign(fv, other.fv);
                    ins = other.ins;
                    fp = other.fp;
                    if constexpr (!is_gen) fend = other.fend;
                    p = other.p;
                    if constexpr (!is_gen_inner) end = other.end;
                    return *this;
                }
                Storage& operator=(Storage&& other) noexcept(noexcept(xyu::object_assign(fv,xyu::move(fv)), fp=xyu::move(fp), p=xyu::move(p)))
                {
                    xyu::object_assign(fv, xyu::move(other.fv));
                    ins = other.ins;
                    fp = xyu::move(other.fp);
                    if constexpr (!is_gen) fend = xyu::move(other.fend);
                    p = xyu::move(other.p);
                    if constexpr (!is_gen_inner) end = xyu::move(other.end);
                    return *this;
                }
            };

            struct Valid
            {
                template <typename Gen>
                constexpr bool operator()(const Gen& gen) noexcept
                { return gen.fp != gen.fend; }
            };

            struct Dereference
            {
                template <typename Gen>
                constexpr decltype(auto) operator()(Gen& gen) noexcept((xyu::t_can_call<Fv> ? xyu::t_can_nothrow_call<Fv> : true) && noexcept(*gen.p))
                {
                    if constexpr (xyu::t_can_call<Fv>) return gen.ins ? static_cast<decltype(*gen.p)>(gen.fv()) : *gen.p;
                    else return gen.ins ? static_cast<decltype(*gen.p)>(gen.fv) : *gen.p;
                }
            };

            using Gen = Iterator<Storage, Valid, Dereference, void, Increment, void, void, void, void, void, void>;
        };
    }
    /**
     * @brief 拼接展开范围
     * @param rg 可以构造范围的对象
     * @param join 将嵌套范围展开为平铺范围，嵌套范围间插入指定值或值生成函数
     * @details 若 bound 为 true 且 每个子范围均已知大小时 连接范围也是已知大小
     * @attention 范围不能为空
     */
    template <typename Rg, typename Fv, typename Range = xyu::t_rem_cr<xyu::t_range<Rg>>, typename Gen = typename __::pack_joinwith_iter<decltype(xyu::t_val<Range>().begin()), Fv, xyu::t_is_gen_range<Range>>::Gen>
    constexpr auto operator|(Rg&& rg, RangeOps::_OpsJoinWith<Fv> join) noexcept(xyu::t_can_nothrow_range<Rg> && noexcept(xyu::Range<Gen>(0, Gen{xyu::t_val<Fv>(), xyu::t_val<Range>().begin(), xyu::t_val<Range>().end()})))
    {
        static_assert(xyu::t_can_range<Rg> && xyu::t_can_range<xyu::t_access<Rg>>);
        static_assert(xyu::t_can_call<Fv> ? xyu::t_can_icast<xyu::t_call<Fv>, xyu::t_access<xyu::t_access<Rg>>> : xyu::t_can_icast<Fv, xyu::t_access<xyu::t_access<Rg>>>);
        auto range = make_range(xyu::forward<Rg>(rg));
        if constexpr (!XY_RELEASE) if (XY_UNLIKELY(range.begin() == range.end()))
            xyu::E_Logic::abort_out_range(XY_CurPos("Range|join"), 0, 0, 0);

        xyu::size_t count = range.count();
        if (range.definite() && join.bound)
        {
            --count;    // 拼接元素
            for (auto&& i : range)
            {
                auto inner = make_range(xyu::forward<decltype(i)>(i));
                if (!inner.definite()) { count = inner.count(); break; }
                count += inner.count();
                if constexpr (XY_DEBUG) if (XY_UNLIKELY(count > xyu::size_t(-1) / 2))
                    xyu::E_Logic::abort_out_range(XY_CurPos("Range|join"), count, 0, xyu::size_t(-1) / 2);
            }
        }
        return xyu::Range<Gen>(count, Gen{join.fv, range.begin(), range.end()});
    }

}
