#pragma once

/**
 * @file    mutex.h
 * @brief   互斥锁
 * @date    2026-04-14
 */

#include "../xytraits/class.h"

/// 互斥锁
namespace xylu::xyconc
{
    namespace __
    {
        /**
         * @brief 锁卫 CRTP 基类
         * @details 自动上锁解锁
         */
        template <typename G, typename MTX>
        struct GuardBase : xyu::class_no_copy_t
        {
        protected:
            MTX* m;             // 锁
            bool own;           // 是否已上锁

        public:
            /// 构造函数
            explicit GuardBase(MTX& mutex, bool need_lock = true) : m{&mutex}, own{false} { if (need_lock) derived().lock(); }
            /// 析构函数
            ~GuardBase() noexcept { if (own) derived().unlock(); }

            /// 移动构造
            GuardBase(GuardBase&& other) noexcept : m{other.m}, own{other.own} { other.own = false; }
            /// 移动赋值
            G& operator=(GuardBase&& other) noexcept { xyu::swap(m, other.m); xyu::swap(own, other.own); return derived(); }

            /// 是否已上锁
            bool is_locked() const noexcept { return own; }

        private:
            // 获取子类
            G& derived() noexcept { return static_cast<G&>(*this); }
        };
    }

    /**
     * @brief 互斥锁
     * @details
     *   底层基于 SRWLOCK (Windows) 或 pthread_mutex_t。
     *   不存储线程 ID 或递归计数，递归语义通过 @ref RecursiveGuard 在栈上显式实现。
     *
     * @par 普通锁用法
     * @code
     * Mutex mtx;
     * void foo() {
     *     // ... 预处理 ...
     *     {
     *         auto g = mtx.guard();   // 上锁
     *         // ... 临界区 ...
     *     }   // 自动解锁
     *     // ... 后续处理 ...
     * }
     * @endcode
     *
     * @par 递归锁用法（软递归）
     * @code
     * Mutex mtx;
     * void recursive_work(int n, Mutex::RecursiveGuard& rg) {
     *     auto g = rg.guard();    // 增加递归深度
     *     if (n > 0) recursive_work(n - 1, rg);   // 传递同一个 rg
     * }   // 每层 g 析构时减少深度，最外层释放底层锁
     *
     * void entry() {
     *     auto rg = mtx.rguard();          // 创建递归状态对象
     *     recursive_work(5, rg);
     * }
     * @endcode
     *
     * @attention 确保在对于同一个 Mutex,
     *   一个线程中同时只存在一个有效的 Guard 或 RecursiveGuard，以保证状态的一致性。
     */
    class Mutex : xyu::class_no_copy_move_t
    {
        friend struct Condvar;
    private:
        void *h;    // 锁句柄

    public:
        /* 构造析构 */

        /// 创建互斥锁
        Mutex() noexcept;

        /// 销毁互斥锁
        ~Mutex() noexcept;

    public:
        /* 互斥锁 */

        /**
         * @brief 互斥锁锁卫
         * @details 自动上锁和解锁，并检测重复操作
         * @attention 对于同一个 Mutex，一个线程同时只能存在一个有效的 Guard 对象
         */
        struct Guard : __::GuardBase<Guard, Mutex>
        {
            friend struct Condvar;
            using GuardBase::GuardBase;

            /// 上锁
            void lock() noexcept;
            /// 尝试上锁
            bool trylock() noexcept;
            /// 解锁
            void unlock() noexcept;
        };

        /**
         * @brief 生成互斥锁锁卫
         * @param need_lock 是否需要上锁 (默认为 true)
         * @attention 一个线程中同时只能存在一个有效的 Guard，否则状态异常
         */
        [[nodiscard]] Guard guard(bool need_lock = true) noexcept { return Guard{*this, need_lock}; }
        
    public:
        /* 递归互斥锁 */

        /**
         * @brief 递归锁
         * @details
         *   允许生成多个 gurad，即重复上锁。
         *   递归最大深度为 size_t 的最大值。
         */
        struct RecursiveGuard : class_no_copy_move_t
        {
        private:
            Mutex* m;       // 互斥锁
            xyu::size_t d;  // 递归深度

        public:
            /// 构造函数
            explicit RecursiveGuard(Mutex& mutex) noexcept : m{&mutex}, d{0} {}

            /// 是否已上锁
            bool is_locked() const noexcept { return d > 0; }
            /// 获取递归深度
            xyu::size_t depth() const noexcept { return d; }

        public:
            /**
             * @brief 递归锁锁卫
             * @details 自动上锁和解锁，并检测重复操作
             */
            struct Guard : __::GuardBase<Guard, RecursiveGuard>
            {
                using GuardBase::GuardBase;

                /// 上锁
                void lock() noexcept;
                /// 尝试上锁
                bool trylock() noexcept;
                /// 解锁
                void unlock() noexcept;
            };

            /**
             * @brief 生成递归锁锁卫
             * @param need_lock 是否需要上锁 (默认为 true)
             * @throws E_Mutex
             */
            [[nodiscard]] Guard guard(bool need_lock = true) noexcept { return Guard{*this, need_lock}; }
        };

        /**
         * @brief 生成递归锁锁卫
         * @attention
         *   每个需要使用递归锁的线程必须在递归外创建 RecursiveGuard 对象，并通过引用传递给所有递归调用的函数。
         *   禁止递归内部创建 RecursiveGuard，禁止跨线程传递 RecursiveGuard。
         *   一个线程中同时只能存在一个有效的 RecursiveGuard，否则状态异常。
         */
        [[nodiscard]] RecursiveGuard rguard() noexcept { return RecursiveGuard{*this}; }
    };
}

/// 读写锁
namespace xylu::xyconc
{
    /**
     * @brief 读写锁（多读单写）
     * @details
     *   底层基于 SRWLOCK (Windows) 或 pthread_rwlock_t。
     *   写锁为独占，读锁为共享，使用示例见 @ref Mutex 的普通锁用法。
     *
     * @attention 确保在对于同一个 RwMutex,
     *   一个线程中同时只存在一个有效的写锁守卫（Guard）或读锁守卫（SharedGuard）。
     */
    struct RwMutex : xyu::class_no_copy_move_t
    {
    private:
        void *h;    // 锁句柄

    public:
        /* 构造析构 */

        /// 创建读写锁
        RwMutex() noexcept;

        /// 销毁读写锁
        ~RwMutex() noexcept;

    public:
        /* 读锁 */

        /**
         * @brief 写锁锁卫
         * @details 自动上锁和解锁，并检测重复操作
         * @attention 对于同一个 RwMutex，一个线程同时只能存在一个有效的 Guard 对象
         */
        struct Guard : __::GuardBase<Guard, RwMutex>
        {
            using GuardBase::GuardBase;
            
            /// 上锁写锁
            void lock() noexcept;
            /// 尝试上锁写锁
            bool trylock() noexcept;
            /// 解锁写锁
            void unlock() noexcept;
        };

        /**
         * @brief 生成写锁锁卫
         * @param need_lock 是否需要上锁 (默认为 true)
         * @attention 一个线程中同时只能存在一个有效的 Guard，否则状态异常
         * @throws E_Mutex_Already_Locked 写锁已上锁
         * @throws E_Mutex
         */
        [[nodiscard]] Guard guard(bool need_lock = true) noexcept { return Guard{*this, need_lock}; }

    public:
        /* 写锁 */

        /**
         * @brief 读锁锁卫
         * @details 自动上锁和解锁，并检测重复操作
         * @attention 对于同一个 RwMutex，一个线程同时只能存在一个有效的 SharedGuard 对象
         */
        struct SharedGuard : __::GuardBase<SharedGuard, RwMutex>
        {
            using GuardBase::GuardBase;

            /// 上锁读锁
            void lock() noexcept;
            /// 尝试上锁读锁
            bool trylock() noexcept;
            /// 解锁读锁
            void unlock() noexcept;
        };

        /**
         * @brief 生成读锁锁卫
         * @param need_lock 是否需要上锁 (默认为 true)
         * @attention 一个线程中同时只能存在一个有效的 SharedGuard，否则状态异常
         * @throws E_Mutex_Already_Locked 读锁已上锁
         * @throws E_Mutex
         */
        [[nodiscard]] SharedGuard sguard(bool need_lock = true) { return SharedGuard{*this, need_lock}; }
    };
}
