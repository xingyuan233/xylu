#pragma clang diagnostic push
#pragma ide diagnostic ignored "OCUnusedStructInspection"
#pragma ide diagnostic ignored "google-explicit-constructor"
#pragma ide diagnostic ignored "modernize-use-nodiscard"
#pragma ide diagnostic ignored "OCUnusedGlobalDeclarationInspection"
#pragma once

#include "../../link/time"

/// 锁
namespace xylu::xyconc
{
    /**
     * @brief 一个基础的、移动专属的互斥锁。
     * @details
     *   `Mutex` 用于保护共享资源，确保在任何时刻只有一个有效的 guard 以保证状态的一致性。
     *   它必须通过其内部的“锁卫”（Guard）对象来使用，以保证锁的自动释放。
     *
     * @note 使用方式为：
     *   @code
     *   Mutex my_mutex;
     *   ...
     *   {
     *       auto guard = my_mutex.guard(); // 上锁
     *       // ... 访问受保护的资源 ...
     *   } // guard 在此销毁，自动解锁
     *   @endcode
     */
    class Mutex : xyu::class_no_copy_t
    {
        friend class CondVar;
    private:
        void *h;    // 锁句柄

    public:
        /**
         * @brief 创建互斥锁
         * @exception E_Mutex_*
         */
        Mutex();
        /**
         * @brief 销毁互斥锁
         * @exception E_Mutex_*
         */
        ~Mutex() noexcept;

        /// 移动构造
        Mutex(Mutex&& other) noexcept : h{other.h} { other.h = nullptr; }
        /// 移动赋值
        Mutex& operator=(Mutex&& other) noexcept { xyu::swap(h, other.h); return *this; }

    public:
        /**
         * @brief 互斥锁锁卫
         * @note 检测重复上锁和解锁，及自动解锁
         */
        struct Guard : xyu::class_no_copy_t
        {
            friend class CondVar;
        private:
            Mutex& m;   // 互斥锁引用
            bool own;   // 是否已上锁

        public:
            /// 构造函数 (默认上锁)
            Guard(Mutex& m, bool need_lock = true) : m{m}, own{false} { if (need_lock) lock(); }
            /// 析构函数 (自动解锁)
            ~Guard() noexcept { try { if (own) unlock(); } catch(...) {} }

            /// 判断是否已上锁
            bool is_locked() const noexcept { return own; }

            /**
             * @brief 上锁
             * @exception E_Mutex_Already_Locked 互斥锁已上锁 (DEBUG下抛出，否则忽略)
             * @exception E_Mutex_*
             */
            void lock();

            /**
             * @brief 解锁
             * @exception E_Mutex_Not_Locked 互斥锁未上锁 (DEBUG下抛出，否则忽略)
             * @exception E_Mutex_*
             */
            void unlock();

            /**
             * @brief 尝试上锁
             * @exception E_Mutex_Already_Locked 互斥锁已上锁 (DEBUG下抛出，否则忽略)
             * @exception E_Mutex_*
             */
            bool trylock();

        public:
            /// 移动构造
            Guard(Guard&& other) noexcept : m{other.m}, own{other.own} { other.own = false; }
            /// 移动赋值
            Guard& operator=(Guard&& other) noexcept { xyu::swap(m, other.m); xyu::swap(own, other.own); return *this; }
        };

        /**
         * @brief 生成互斥锁锁卫
         * @param need_lock 是否需要上锁
         * @note 一个线程内只能同时存在一个有效的 guard，否则状态异常
         */
        [[nodiscard]] Guard guard(bool need_lock = true) { return {*this, need_lock}; }

    public:
        /**
         * @brief 递归锁锁卫
         * @note 允许重复上锁，但需要手动解锁 (否则会发出警告)
         * @note 递归最大深度为 xyu::number_traits<size_t>::max()
         */
        struct Guard_Recursive : xyu::class_no_copy_t
        {
            friend class CondVar;
        private:
            Mutex& m;       // 互斥锁引用
            xyu::size_t dp; // 递归深度

        public:
            /// 构造函数 (默认上锁)
            Guard_Recursive(Mutex& m, bool need_lock = true) : m{m}, dp{0} { if (need_lock) lock(); }
            /// 析构函数 (自动解锁)
            ~Guard_Recursive() noexcept;

            /// 判断是否已上锁
            bool is_locked() const noexcept { return dp > 0; }
            /// 递归深度
            xyu::size_t depth() const noexcept { return dp; }

            /**
             * @brief 上锁
             * @exception E_Mutex_Recursive_Limit 超过递归锁最大深度
             * @exception E_Mutex_*
             */
            void lock();

            /**
             * @brief 解锁
             * @exception E_Mutex_Not_Locked 互斥锁未上锁
             * @exception E_Mutex_*
             */
            void unlock();

            /**
             * @brief 尝试上锁
             * @exception E_Mutex_Recursive_Limit 超过递归锁最大深度
             * @exception E_Mutex_*
             */
            bool trylock();

        public:
            /// 移动构造
            Guard_Recursive(Guard_Recursive&& other) noexcept : m{other.m}, dp{other.dp} { other.dp = 0; }
            /// 移动赋值
            Guard_Recursive& operator=(Guard_Recursive&& other) noexcept { xyu::swap(m, other.m); xyu::swap(dp, other.dp); return *this; }
        };
        /**
         * @brief 生成递归锁锁卫
         * @param need_lock 是否需要上锁
         * @note 一个线程内只能同时存在一个有效的 guard，否则状态异常
         */
        [[nodiscard]] Guard_Recursive rguard(bool need_lock = true) { return {*this, need_lock}; }
    };

    /**
     * @brief 一个允许多个读者或单个写者的读写锁。
     * @details
     *   `Mutex_RW` 提高了并发性能，因为它允许多个线程同时进行读操作。
     *   只有在需要进行写操作时，才会对所有其他读者和写者进行独占锁定。
     * @note 必须使用 `rguard()` 获取读锁卫，`guard()` 获取写锁卫。
     */
    class Mutex_RW : xyu::class_no_copy_t
    {
#if (defined(_WIN32) || defined(__CYGWIN__))
        friend class CondVar;
#endif
    private:
        void *h;    // 锁句柄

    public:
        /**
         * @brief 创建互斥锁
         * @exception E_Mutex_*
         */
        Mutex_RW();
        /**
         * @brief 销毁互斥锁
         * @exception E_Mutex_*
         */
        ~Mutex_RW() noexcept;

        /// 移动构造
        Mutex_RW(Mutex_RW&& other) noexcept : h{other.h} { other.h = nullptr; }
        /// 移动赋值
        Mutex_RW& operator=(Mutex_RW&& other) noexcept { xyu::swap(h, other.h); return *this; }

    public:
        /**
         * @brief 写锁锁卫
         * @note 检测重复上锁和解锁，及自动解锁
         */
        struct Guard_Write : xyu::class_no_copy_t
        {
#if (defined(_WIN32) || defined(__CYGWIN__))
            friend class CondVar;
#endif
        private:
            Mutex_RW& m;    // 互斥锁引用
            bool own;       // 是否已上锁

        public:
            /// 构造函数 (默认上锁)
            Guard_Write(Mutex_RW& m, bool need_lock = true) : m{m}, own{false} { if (need_lock) lock(); }
            /// 析构函数 (自动解锁)
            ~Guard_Write() noexcept { try { if (own) unlock(); } catch(...) {} }

            /// 判断是否已上锁
            bool is_locked() const noexcept { return own; }

            /**
             * @brief 上锁
             * @exception E_Mutex_Already_Locked 互斥锁已上锁 (DEBUG下抛出，否则忽略)
             * @exception E_Mutex_*
             */
            void lock();

            /**
             * @brief 解锁
             * @exception E_Mutex_Not_Locked 互斥锁未上锁 (DEBUG下抛出，否则忽略)
             * @exception E_Mutex_*
             */
            void unlock();

            /**
             * @brief 尝试上锁
             * @exception E_Mutex_Already_Locked 互斥锁已上锁 (DEBUG下抛出，否则忽略)
             * @exception E_Mutex_*
             */
            bool trylock();

        public:
            /// 移动构造
            Guard_Write(Guard_Write&& other) noexcept : m{other.m}, own{other.own} { other.own = false; }
            /// 移动赋值
            Guard_Write& operator=(Guard_Write&& other) noexcept { xyu::swap(m, other.m); xyu::swap(own, other.own); return *this; }
        };
        /**
         * @brief 生成写锁锁卫
         * @param need_lock 是否需要上锁
         * @note 一个线程内只能同时存在一个有效的 guard，否则状态异常
         */
        [[nodiscard]] Guard_Write guard(bool need_lock = true) { return {*this, need_lock}; }

        /**
         * @brief 读锁锁卫
         * @note 检测重复上锁和解锁，及自动解锁
         */
        struct Guard_Read : xyu::class_no_copy_t
        {
#if (defined(_WIN32) || defined(__CYGWIN__))
            friend class CondVar;
#endif
        private:
            Mutex_RW& m;    // 互斥锁引用
            bool have;      // 是否已上锁

        public:
            /// 构造函数 (默认上锁)
            Guard_Read(Mutex_RW& m, bool need_lock = true) : m{m}, have{false} { if (need_lock) lock(); }
            /// 析构函数 (自动解锁)
            ~Guard_Read() noexcept { try { if (have) unlock(); } catch(...) {} }

            /// 判断是否已上锁
            bool is_locked() const noexcept { return have; }

            /**
             * @brief 上锁
             * @exception E_Mutex_Already_Locked 互斥锁已上锁 (DEBUG下抛出，否则忽略)
             * @exception E_Mutex_*
             */
            void lock();

            /**
             * @brief 解锁
             * @exception E_Mutex_Not_Locked 互斥锁未上锁 (DEBUG下抛出，否则忽略)
             * @exception E_Mutex_*
             */
            void unlock();

            /**
             * @brief 尝试上锁
             * @exception E_Mutex_Already_Locked 互斥锁已上锁 (DEBUG下抛出，否则忽略)
             * @exception E_Mutex_*
             */
            bool trylock();

        public:
            /// 移动构造
            Guard_Read(Guard_Read&& other) noexcept : m{other.m}, have{other.have} { other.have = false; }
            /// 移动赋值
            Guard_Read& operator=(Guard_Read&& other) noexcept { xyu::swap(m, other.m); xyu::swap(have, other.have); return *this; }
        };
        /**
         * @brief 生成读锁锁卫
         * @param need_lock 是否需要上锁
         * @note 一个线程内只能同时存在一个有效的 guard，否则状态异常
         */
        [[nodiscard]] Guard_Read rguard(bool need_lock = true) { return {*this, need_lock}; }
    };
}

#pragma clang diagnostic pop