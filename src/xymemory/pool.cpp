#pragma clang diagnostic push
#pragma ide diagnostic ignored "cppcoreguidelines-pro-type-member-init"
#pragma ide diagnostic ignored "cppcoreguidelines-narrowing-conversions"
#pragma ide diagnostic ignored "hicpp-exception-baseclass"
#pragma ide diagnostic ignored "modernize-use-nodiscard"

#include "../../head/xymemory/pool.h"
#include "../../link/log"
#include <immintrin.h>

using namespace xyu;

template <typename T>
using nt = number_traits<T>;

using namespace xylu::xymemory::__;

// 固定大小栈缓冲区
using S64 = xylu::xycore::__::SString<64>;
using S128 = xylu::xycore::__::SString<128>;
using S256 = xylu::xycore::__::SString<256>;

/** 分块内存池 */
namespace xylu::xymemory
{
    // 辅助函数 - 找到第一个不小于提供值的位置
    template <typename T, typename U>
    static T* bsearch_nolower_first(T* start, T* end, const U& value)
    {
        while (start < end)
        {
            T* middle = start + ((end - start) >> 1);
            if (*middle < value) start = middle + 1;
            else end = middle;
        }
        return start;
    }

    /// 辅助类 - 有序数组
    //除了最后一个元素外有序
    template <typename T, typename = t_enable<t_can_trivial_copy<T>>>
    class SortedArray
    {
    private:
        T* data = nullptr;  // 元素指针
        uint32 size = 0;    // 元素个数
        uint32 capa = 0;    // 容量
    public:
        // 插入一个元素 (调用者对返回指针进行 place-new)
        T* append() { fix_sort(); return data + size++; }

        // 释放内存 (手动调用,一次性)
        void release() noexcept
        {
            if (!data) return;
            under_dealloc_align(data);
        }

        // 元素数量
        uint32 count() const noexcept { return size; }
        explicit operator bool() const noexcept { return size; }

        // 获取元素
        T& operator[](uint32 index) noexcept { return data[index]; }
        T& back() noexcept { return data[size - 1]; }
        T* first() noexcept { return data; }
        T* last() noexcept { return data + size - 1; }

    private:
        // 排序并确保容量 (保证剩余容量>=1, 并将元素排序)
        void fix_sort()
        {
            //初次扩容
            if (XY_UNLIKELY(!data))
            {
                capa = 8;
                data = (T*)under_alloc_align(capa * sizeof(T), align);
            }
            //容量充足
            else if (XY_LIKELY(size < capa))
            {
                if (size <= 1) return;
                uint32 i = bsearch_nolower_first(data, data+size-1, data[size-1]) - data;
                if (i + 1 >= size) return;
                mem_move(data + i + 1, data + i, (size - i) * sizeof(T));
                data[i] = data[size];
            }
            //容量不足
            else
            {
                //大小修正
                constexpr size_t maxcapa = nt<uint32>::max / sizeof(T);
                uint32 newcapa = capa << 1;
                if (XY_UNLIKELY(newcapa > maxcapa ||  newcapa < capa))
                {
                    if (XY_UNLIKELY(capa >= maxcapa)) {
                        xyloge(0, "E_Memory_Capacity: new capacity {} over array limit {}", newcapa, maxcapa);
                        throw xyu::E_Memory_Capacity{};
                    }
                    newcapa = maxcapa;
                }
                //分配新内存 (失败了由所属类析构函数处理)
                T* newdata = (T*)under_alloc_align(newcapa * sizeof(T), align);
                size_t i = bsearch_nolower_first(data, data+size-1, data[size-1]) - data;
                mem_copy(newdata, data, i * sizeof(T));
                newdata[i] = data[size-1];
                if (i+1 < size) mem_copy(newdata + i + 1, data + i, (size - i - 1) * sizeof(T));
                under_dealloc_align(data);
                data = newdata;
                capa = newcapa;
            }
        }

    private:
        constexpr static uint32 align = xyu::max(alignof(T), alignof(void*));
    };

    /// 辅助类 - 无序数组
    //用于一次性获取多个元素空间
    template <typename T, typename = t_enable<t_can_trivial_copy<T>>>
    class UnsortedArray
    {
    private:
        T* data = nullptr;  // 元素指针
        uint32 size = 0;    // 元素个数
        uint32 capa = 0;    // 容量
    public:
        // 获取多个空间
        T* get(uint32 count)
        {
            fix(count);
            T* p = data + size;
            size += count;
            return p;
        }

        // 释放内存 (手动调用,一次性)
        void release() noexcept
        {
            if (!data) return;
            under_dealloc_align(data);
        }

        // 元素数量
        uint32 count() const noexcept { return size; }
        uint32 capacity() const noexcept { return capa; }

        // 起始指针
        T* first() noexcept { return data; }

    private:
        // 保证空间足够
        void fix(uint32 count)
        {
            //初次扩容
            if (XY_UNLIKELY(!data))
            {
                capa = 16;
                data = (T*)under_alloc_align(capa * sizeof(T), align);
            }
            //容量充足
            else if (size + count <= capa) return;
            //容量不足
            else
            {
                //大小修正
                constexpr size_t maxcapa = nt<uint32>::max / sizeof(T);
                uint32 newcapa = bit_get_2ceil(size + count);
                if (XY_UNLIKELY(newcapa > maxcapa ||  newcapa < capa))
                {
                    if (XY_UNLIKELY(maxcapa < size + count)) {
                        xyloge(0, "E_Memory_Capacity: new capacity {} over array limit {}", newcapa, maxcapa);
                        throw xyu::E_Memory_Capacity{};
                    }
                    newcapa = maxcapa;
                }
                //分配新内存 (失败了由所属类析构函数处理)
                T* newdata = (T*)under_alloc_align(newcapa * sizeof(T), align);
                mem_copy(newdata, data, size * sizeof(T));
                under_dealloc_align(data);
                data = newdata;
                capa = newcapa;
            }
        }

    private:
        constexpr static uint32 align = xyu::max(alignof(T), alignof(void*));
    };

    /// 内存小块
    //1: [状态块数, 最小空闲状态块索引, 块大小]-[数据指针]  (数组存储多个块)
    //2: [状态块...]  (数组存储多个状态块)
    //3: [数据块(由多个内存单元构成)...]  (每个块独立,防止对齐空间浪费,及溢出影响状态信息)
    struct Chunk
    {
        uint32 state_pos;   // 状态块起始位置
        uint16 state_count; // 状态块数 (必须大于0)
        uint16 state_next;  // 最小空闲状态块索引
        uint32 data_bytes;  // 数据单元总大小
        uint8* data_ptr;    // 数据指针
    public:
        // 初始化
        Chunk(uint16 state_count, uint32 cell_count, uint64* state_ptr, uint32 state_pos,
              uint64 data_bytes, uint8* data_ptr) noexcept
            : state_count(state_count), state_next(0), data_bytes(data_bytes), data_ptr(data_ptr), state_pos(state_pos)
        {
            mem_set(state_ptr, state_count * nt<uint64>::size);
            uint32 last = cell_count & (nt<uint64>::digits - 1);
            if (last) state_ptr[state_count - 1] = uint64(-1) << last;
        }

        // 获取内存单元
        void* get(uint32 cell_size, uint64* sp) noexcept
        {
            if (state_next >= state_count) return nullptr;
            uint64* states = sp + state_pos;
            uint32 index_of_block = bit_count_1_back(states[state_next]);
            uint32 index = state_next * nt<uint64>::digits + index_of_block;
            states[state_next] |= uint64(1) << index_of_block;
            while (states[state_next] == uint64(-1))
                if (++state_next >= state_count) break;

            xylogl(xyu::N_LOG_TRACE, xyu::K_LOG_MEMPOOL,
                   xyfmtt(S128{}, "Get cell: (ptr={}, size={}, state_index={}, cell_index={})",
                          data_ptr + index * cell_size, cell_size, state_next, index_of_block));

            return data_ptr + index * cell_size;
        }

        // 释放内存单元 (由调用者判断所属)
        void put(void* p, uint32 cell_size, uint64* sp) XY_NOEXCEPT_NDEBUG
        {
#if XY_DEBUG
            if (p < data_ptr || p >= data_ptr + data_bytes) {
                xylogw(xyu::K_LOG_MEMPOOL, "Put cell not belong to this chunk");
                return;
            }
#endif
            uint32 index = ((uint8*)p - data_ptr) / cell_size;
            uint32 index_of_state = index / nt<uint64>::digits;
            uint64* states = sp + state_pos;
#if XY_DEBUG
            if (states[index_of_state] & (uint64(1) << (index % nt<uint64>::digits)));
            else {
                xylogw(xyu::K_LOG_MEMPOOL, "Put cell not be used");
                return;
            }
#endif
            xylogl(xyu::N_LOG_TRACE, xyu::K_LOG_MEMPOOL,
                   xyfmtt(S128{}, "Put cell: (ptr={}, size={}, state_index={}, cell_index={})",
                          p, cell_size, index_of_state, index % nt<uint64>::digits));

            states[index_of_state] &= ~(uint64(1) << (index % nt<uint64>::digits));
            if (state_next > index_of_state) state_next = index_of_state;
        }

#if XY_DEBUG
        // 计算使用单元数量
        uint32 count(uint32 cell_size, uint64* sp) const noexcept
        {
            uint32 count = 0;
            for (uint32 i = state_count - 2; i < state_count; --i)
                count += bit_count_1_total(sp[state_pos + i]);
            uint32 last = (data_bytes / cell_size) & (nt<uint64>::digits - 1);
            if (last) last = nt<uint64>::digits - last;
            count += bit_count_1_total(sp[state_pos + state_count - 1]) - last;
            return count;
        }
#endif

        // 比较
        XY_CONST bool operator<(const Chunk& other) const noexcept { return data_ptr < other.data_ptr; }
    };

    /// 内存小块组
    //管理可拓展的多个内存单元大小相同的Chunk
    struct ChunkGroup
    {
    private:
        SortedArray<Chunk> chunks;      // 内存块数组
        UnsortedArray<uint64> states;   // 状态块数组
        uint32 cell_size;               // 内存单元大小
        uint32 cell_count;              // 内存单元数量
    public:
        // 初始化
        ChunkGroup(uint32 size, uint32 count) noexcept : cell_size(size), cell_count(count) {}

        //创新新内存块
        void create(const MemPool_Block::Option& op)
        {
            size_t cell_bytes = cell_size * cell_count;
            size_t align = max(bit_get_2ceil(cell_size), alignof(uint64));
            void* cp = under_alloc_align(cell_bytes, align);

            size_t state_count = (cell_count + nt<uint64>::digits - 1) / nt<uint64>::digits;
            size_t state_pos = states.count();
            uint64* sp = states.get(state_count);

            if constexpr (xyu::K_LOG_LEVEL >= xyu::N_LOG_ALL && xyu::K_LOG_MEMPOOL >= xyu::N_LOG_ALL) {
                xylogl(xyu::N_LOG_ALL, xyu::K_LOG_MEMPOOL,
                       xyfmtt(S256{}, "Create chunk: (ptr={}, bytes={}, size={}, align={}, count={}, "
                                      "state_ptr={}, state_count={}, state_used={}, state_capa={})",
                              cp, cell_bytes, cell_size, align, cell_count, sp, state_count, state_pos + state_count, states.capacity()));
            }
            else if constexpr (xyu::K_LOG_LEVEL >= xyu::N_LOG_TRACE && xyu::K_LOG_MEMPOOL >= xyu::N_LOG_TRACE) {
                xylogl(xyu::N_LOG_TRACE, xyu::K_LOG_MEMPOOL,
                       xyfmtt(S256{}, "Create chunk: (ptr={}, bytes={}, size={}, align={}, count={}, "
                                      "state_ptr={}, state_count={})",
                              cp, cell_bytes, cell_size, align, cell_count, sp, state_count));
            }
            else {
                xylogl(xyu::N_LOG_DEBUG, xyu::K_LOG_MEMPOOL,
                       xyfmtt(S256{}, "Create chunk: (ptr={}, bytes={}, size={}, align={}, count={})",
                              cp, cell_bytes, cell_size, align, cell_count));
            }

            //排序旧内存块并插入新内存块
            ::new (chunks.append()) Chunk(state_count, cell_count, sp, state_pos, cell_bytes, (uint8*)cp);

            //动态调整数据
            if (XY_LIKELY(cell_count < op.chunk_max_cells))
            {
                cell_count = cell_count * op.grow_factor;
                if (XY_UNLIKELY(cell_count > op.chunk_max_cells)) cell_count = op.chunk_max_cells;
                auto max_cells = nt<uint32>::max / cell_size;
                if (XY_UNLIKELY(cell_count > max_cells)) cell_count = max_cells;
            }
        }

        //获取一个未使用的内存小块
        void* try_get() noexcept
        {
            if (XY_LIKELY(!!chunks))
                for (Chunk* i = chunks.last(), *j = chunks.first(); i>=j; --i)
                {
                    xylogl(xyu::N_LOG_ALL, xyu::K_LOG_MEMPOOL,
                           xyfmtt(S128{}, "Try get cell: (size={}, chunk_index={})", cell_size, i-chunks.first()));

                    void* p = i->get(cell_size, states.first());
                    if (p) return p;
                }
            return nullptr;
        }
        void* get(const MemPool_Block::Option& op)
        {
            //尝试从已有的内存块中获取内存小块
            if (void* p = try_get()) return p;
            //失败，则创建新内存块
            create(op);
            return chunks.back().get(cell_size, states.first());
        }
        //释放一个内存小块
        void put(void* p) XY_NOEXCEPT_NDEBUG
        {
            if (XY_UNLIKELY(!chunks)) {
                xylogw(xyu::K_LOG_MEMPOOL, "Put cell not belong to this chunkgroup");
                return;
            }
            //判断p是否属于最后一个内存块 (最后一块未排序)
            Chunk& back = chunks.back();

            xylogl(xyu::N_LOG_ALL, xyu::K_LOG_MEMPOOL,
                   xyfmtt(S128{}, "Try put cell: (ptr={}, size={}, chunk_index={})",
                          p, cell_size, chunks.count() - 1));

            if (p >= back.data_ptr && p < back.data_ptr + back.data_bytes) {
                back.put(p, cell_size, states.first());
                return;
            }
            if (chunks.count() == 1) {
                xylogw(xyu::K_LOG_MEMPOOL, "Put cell not belong to this chunkgroup");
                return;
            }
            //查找p所属的内存块 (不包括最后一个内存块)
            Chunk *start = chunks.first(), *over = chunks.last();
            while (start < over) {
                Chunk* mid = start + ((over - start) >> 1);
                if (mid->data_ptr > p) over = mid;
                else start = mid + 1;
            }
            //p小于所有内存块
            if (XY_UNLIKELY(start == chunks.first())) {
                xylogw(xyu::K_LOG_MEMPOOL, "Put cell not belong to this chunkgroup");
                return;
            }
            //释放内存
            --start;

            xylogl(xyu::N_LOG_ALL, xyu::K_LOG_MEMPOOL,
                   xyfmtt(S128{}, "Try put cell: (ptr={}, size={}, chunk_index={})",
                          p, cell_size, start - chunks.first()));

            if (XY_UNLIKELY(p >= start->data_ptr + start->data_bytes)) {
                xylogw(xyu::K_LOG_MEMPOOL, "Put cell not belong to this chunkgroup");
                return;
            }
            start->put(p, cell_size, states.first());
        }

        //释放内存
        void release() noexcept
        {
            if (XY_LIKELY(!!chunks))
            {
                for (Chunk *i=chunks.last(), *j=chunks.first(); i>=j; --i)
                {
#if XY_DEBUG
                    uint32 used = i->count(cell_size, states.first());
                    if (used)
                        xylogl(xyu::N_LOG_WARN, xyu::K_LOG_MEMPOOL,
                               xyfmtt(S128{}, "Release chunk: (ptr={}, size={}, bytes={}, used={})",
                                      i->data_ptr, cell_size, i->data_bytes, used));
                    else
#endif
                    xylogl(xyu::N_LOG_DEBUG, xyu::K_LOG_MEMPOOL,
                           xyfmtt(S128{}, "Release chunk: (ptr={}, size={}, bytes={})",
                                  i->data_ptr, cell_size, i->data_bytes));

                    under_dealloc_align(i->data_ptr);
                }
            }
            chunks.release();
            states.release();
        }
    };

    /// 内存大块
    struct Block
    {
        uint8* p;       // 内存指针
#if XY_DEBUG
        size_t size;    // 内存大小
        size_t align;   // 内存对齐
#endif
    };

    /// 内存大块组
    //单独分配的大型内存块，使用底层alloc
    //内存构成: [next] [p指向实际数据] [size实际数据大小(仅DEBUG)] [align(仅DEBUG)] [...(对齐填充)] [实际数据(后置,防止破坏前面的状态)]
    struct BlockSet
    {
        //链表节点
        struct Node { Node* next; Block val; };
        //桶
        struct Bucket { Node* next; };
    private:
        Bucket* buckets = nullptr;          // 哈希桶
        size_t bucket_count = 0;            // 桶数量 (2^n)
        size_t elem_count = 0;              // 元素数量
        size_t elem_capa = 0;               // 元素容量 (准确来说是扩容界限,不是实际的容量)
    public:
        static constexpr float load_factor = 0.75f; // 负载因子

    public:
        //创建一个大内存块
        void* make(size_t bytes, size_t align)
        {
            fix();

            size_t size_node = (sizeof(Node) + align - 1) & -align;
            void* p = under_alloc_align(size_node + bytes, align);
            uint8* data = (uint8*)p + size_node;

            size_t index = rangehash(data, bucket_count);
#if XY_DEBUG
            ::new (p) Node{buckets[index].next, {data, bytes, align}};
#else
            ::new (p) Node{buckets[index].next, {data}};
#endif
            buckets[index].next = (Node*)p;
            ++elem_count;

            if constexpr (xyu::K_LOG_LEVEL >= xyu::N_LOG_ALL && xyu::K_LOG_MEMPOOL >= xyu::N_LOG_ALL) {
                xylogl(xyu::N_LOG_ALL, xyu::K_LOG_MEMPOOL,
                       xyfmtt(S256{}, "Create block: (ptr={}, bytes={}, align={}, elem_used={}, elem_capa={}, bucket_count={})",
                              data, bytes, align, elem_count, elem_capa, bucket_count));
            } else {
                xylogl(xyu::N_LOG_TRACE, xyu::K_LOG_MEMPOOL,
                       xyfmtt(S128{}, "Create block: (ptr={}, bytes={}, align={})", data, bytes, align));
            }

            return data;
        }

        //释放一个大内存块
        void free(void* p, size_t bytes, size_t align) XY_NOEXCEPT_NDEBUG
        {
            size_t index = rangehash(p, bucket_count);

            //删除元素 (直接将 Bucket 作为 Node 处理，因为仅使用 next 指针)
            Node* np = reinterpret_cast<Node*>(&buckets[index]);
            for(Node* nn = np->next; ; np=nn, nn=nn->next) {
                if (!nn) {
                    xylogw(xyu::K_LOG_MEMPOOL, "Free block not belong to this blockset");
                    return;
                } else if (nn->val.p == p) {
                    np->next = nn->next;
                    np = nn;
                    break;
                }
            }
#if XY_DEBUG
            if (np->val.size != bytes) {
                xylogw(xyu::K_LOG_MEMPOOL, "Free block size {} not match with create size {}", np->val.size, bytes);
            }
            if (np->val.align != align) {
                xylogw(xyu::K_LOG_MEMPOOL, "Free block align {} not match with create align {}", np->val.align, align);
            }
#endif
            xylogl(xyu::N_LOG_TRACE, xyu::K_LOG_MEMPOOL, xyfmtt(S128{},
                   "Release block: (ptr={}, bytes={}, align={})", np->val.p, bytes, align));

            under_dealloc_align(np);
            --elem_count;
        }

        //释放内存
        void release() noexcept
        {
            for (size_t i = 0; i < bucket_count; ++i)
            {
                Node* p = buckets[i].next;
                while (p)
                {
                    Node* q = p->next;

#if XY_DEBUG
                    xylogl(xyu::N_LOG_WARN, xyu::K_LOG_MEMPOOL,
                           xyfmtt(S128{}, "Release block: (ptr={}, bytes={}, align={})",
                                  p->val.p, p->val.size, p->val.align));
#else
                    xylogl(xyu::N_LOG_WARN, xyu::K_LOG_MEMPOOL,
                           xyfmtt(S64{}, "Release block: (ptr={})", p->val.p));
#endif
                    under_dealloc_align(p);
                    p = q;
                }
            }
            under_dealloc_align(buckets);
        }

    private:
        //确保容量
        void fix()
        {
            //初次扩容
            if (XY_UNLIKELY(!buckets))
            {
                bucket_count = 16;
                buckets = (Bucket*)under_alloc_align(bucket_count * sizeof(Bucket), alignof(Bucket));
                mem_set(buckets, bucket_count * sizeof(Bucket));
                elem_capa = bucket_count * load_factor;
            }
            //容量充足
            else if (XY_LIKELY(elem_count < elem_capa)) return;
            //容量不足
            else
            {
                //大小修正
                size_t new_count = bucket_count << 1;
                if (XY_UNLIKELY(new_count < bucket_count)) {
                    xyloge(0, "block's bucket count overflow");
                    throw xyu::E_Memory_Capacity{};
                }
                //分配新内存 (失败了由所属类析构函数处理)
                auto* new_buckets = (Bucket*)under_alloc_align(new_count * sizeof(Bucket), alignof(Bucket));
                mem_set(new_buckets, new_count * sizeof(Bucket));
                //转移元素
                for (size_t i = 0; i < bucket_count; ++i)
                {
                    Node* p = buckets[i].next;
                    while (p)
                    {
                        Node* q = p->next;
                        size_t index = rangehash(p->val.p, new_count);
                        p->next = new_buckets[index].next;
                        new_buckets[index].next = p;
                        p = q;
                    }
                }
                under_dealloc_align(buckets);
                buckets = new_buckets;
                bucket_count = new_count;
                elem_capa = bucket_count * load_factor;
            }
        }

        //获取索引
        XY_CONST static size_t rangehash(void* p, size_t count) noexcept
        {
            auto v = (size_t)p;
            // 丢弃对齐低位
            constexpr auto diff = bit_count_0_back(K_DEFAULT_ALIGN);
            v >>= diff;
            // 混合哈希
            if constexpr (sizeof(size_t) >= 8) v ^= v >> 32;
            v ^= v >> 16;
            v ^= v >> 8;
            return v & (count - 1);
        }
    };

    namespace
    {
        //每个池的内存单元大小
        alignas(32) constexpr uint32 cell_sizes[] = {
                8, 16, 24, 32, 48, 64, 80, 96, 112,
                128, 192, 256, 320, 384, 448, 512, 768,
                1024, 1536, 2048, 3072,
                1<<12, 1<<13, 1<<14,
                1<<15, 1<<16, 1<<17,
                1<<20, 1<<21, 1<<22
        };
        //找到需要的池的个数
        XY_PURE size_t find_counts_of_chunks(size_t max_block_size) noexcept
        {
            // assert max_block_size >= 1
            max_block_size--;
            // - 1 为了使找到的 index 为 第一个大于等于 的索引 (即需要的池的个数)
            //预估值较大，且仅在init时调用，不使用SIMD优化
            if (max_block_size <= 3072) {
                for (size_t i = 19; i < 20; --i) if (cell_sizes[i] < max_block_size) return i + 2;
                return 1;
            } else {
                using vt = xyu::number_traits<size_t>;
                uint bits = bit_count_0_front(max_block_size - 1);
                if (bits >= vt::digits - 17) return vt::digits - bits + 10;
                else return vt::digits - bits + 8;
            }
        }

        //找到最合适的池的索引
        XY_HOT XY_PURE size_t find_index_of_chunks(uint32 bytes) noexcept
        {
            // assert bytes >= 1
            if (bytes <= 3072) {
#if defined(__AVX2__)
                // --- AVX2 实现 (256位, 一次处理8个元素) ---
                // 1.将 'bytes-1' 广播到一个256位向量中的8个通道
                const __m256i v_byte = _mm256_set1_epi32(static_cast<int>(bytes-1));

                // 2.分批处理8个元素
                // 2.1 加载 cell_sizes[0..7] / cell_sizes[8..15] / cell_sizes[16..23] (有效部分仅 [16..20])
                // 2.2 执行无符号32位整数的 "大于" 比较 (原来为 >=, 但第二个参数已经-1， 只需要 >)
                // 2.3 从比较结果中创建位掩码，提取每个字节的最高位
                // 2.4 从右边找到第一个为1的索引，每个uint32_t产生4个比特，右移4位得到元素索引

                __m256i v_cell_1 = _mm256_load_si256(reinterpret_cast<const __m256i*>(&cell_sizes[0]));
                __m256i gt1 = _mm256_cmpgt_epi32(v_cell_1, v_byte);
                uint32 mask1 = _mm256_movemask_epi8(gt1);
                if (mask1) return bit_count_0_back(mask1) >> 2;

                __m256i v_cell_2 = _mm256_load_si256(reinterpret_cast<const __m256i*>(&cell_sizes[8]));
                __m256i gt2 = _mm256_cmpgt_epi32(v_cell_2, v_byte);
                uint32 mask2 = _mm256_movemask_epi8(gt2);
                if (mask2) return 8 + (bit_count_0_back(mask2) >> 2);

                __m256i v_cell_3 = _mm256_load_si256(reinterpret_cast<const __m256i*>(&cell_sizes[16]));
                __m256i gt3 = _mm256_cmpgt_epi32(v_cell_3, v_byte);
                uint32 mask3 = _mm256_movemask_epi8(gt3);
                //if (mask3 != 0) // 必然成立
                return 16 + (bit_count_0_back(mask3) >> 2);
#elif defined(__SSE2__)
                // --- SSE2 实现 (128位, 一次处理4个元素) ---
                // 1.将 'bytes-1' 广播到一个128位向量中的4个通道
                const __m128i v_byte = _mm_set1_epi32(static_cast<int>(bytes-1));

                // 2.分批处理4*2个元素
                // 2.1 加载 cell_sizes[0..3][4..7] / [8..11][12..15] / [16..19][20..23] (有效部分仅 [16..20])
                // 2.2 执行无符号32位整数的 "大于" 比较 (原来为 >=, 但第二个参数已经-1， 只需要 >)
                // 2.3 从比较结果中创建位掩码，提取每个字节的最高位
                // 2.4 从右边找到第一个为1的索引，每个uint32_t产生4个比特，右移4位得到元素索引

                __m128i v_cell_1 = _mm_load_si128(reinterpret_cast<const __m128i*>(&cell_sizes[0]));
                __m128i v_cell_2 = _mm_load_si128(reinterpret_cast<const __m128i*>(&cell_sizes[4]));
                __m128i gt1 = _mm_cmpgt_epi32(v_cell_1, v_byte);
                __m128i gt2 = _mm_cmpgt_epi32(v_cell_2, v_byte);
                uint32 mask1 = _mm_movemask_epi8(gt1);
                uint32 mask2 = _mm_movemask_epi8(gt2);
                if (mask1) return bit_count_0_back(mask1) >> 2;
                if (mask2) return 4 + (bit_count_0_back(mask2) >> 2);

                __m128i v_cell_3 = _mm_load_si128(reinterpret_cast<const __m128i*>(&cell_sizes[8]));
                __m128i v_cell_4 = _mm_load_si128(reinterpret_cast<const __m128i*>(&cell_sizes[12]));
                __m128i gt3 = _mm_cmpgt_epi32(v_cell_3, v_byte);
                __m128i gt4 = _mm_cmpgt_epi32(v_cell_4, v_byte);
                uint32 mask3 = _mm_movemask_epi8(gt3);
                uint32 mask4 = _mm_movemask_epi8(gt4);
                if (mask3) return 8 + (bit_count_0_back(mask3) >> 2);
                if (mask4) return 12 + (bit_count_0_back(mask4) >> 2);

                __m128i v_cell_5 = _mm_load_si128(reinterpret_cast<const __m128i*>(&cell_sizes[16]));
                __m128i v_cell_6 = _mm_load_si128(reinterpret_cast<const __m128i*>(&cell_sizes[20]));
                __m128i gt5 = _mm_cmpgt_epi32(v_cell_5, v_byte);
                __m128i gt6 = _mm_cmpgt_epi32(v_cell_6, v_byte);
                uint32 mask5 = _mm_movemask_epi8(gt5);
                uint32 mask6 = _mm_movemask_epi8(gt6);
                if (mask5) return 16 + (bit_count_0_back(mask5) >> 2);
                return 20 + (bit_count_0_back(mask6) >> 2);
#else
                return bsearch_nolower_first(cell_sizes, cell_sizes+21, bytes) - cell_sizes;
#endif
            } else {
                using vt = xyu::number_traits<uint32>;
                uint bits = bit_count_0_front<uint32>(bytes - 1);
                if (bits >= vt::digits - 17) return vt::digits - bits + 9;
                else return vt::digits - bits + 7;
            }
        }
    }

    /// 内存池
    // 初始化
    void MemPool_Block::init(Option option) noexcept
    {
        //已经初始化过
        if (XY_UNLIKELY(chunk_count)) return;
        //初始化选项修正
        if (XY_UNLIKELY(option.cell_max_size < sizeof(BlockSet::Node)))
            option.cell_max_size = sizeof(BlockSet::Node);  // (确保内存大块分配节点时使用内存小块)
        if (XY_UNLIKELY(option.chunk_min_cells < 1))
            option.chunk_min_cells = 1;  // (最小单元数量)
        if (XY_UNLIKELY(option.chunk_max_cells < option.chunk_min_cells))
            option.chunk_max_cells = option.chunk_min_cells;  // (最大单元数量)
        if (XY_UNLIKELY(option.grow_factor < 1.f))
            option.grow_factor = 1.f;  // (扩容因子)
        if (XY_UNLIKELY(option.chunk_max_cells > nt<uint16>::max * nt<uint64>::digits))
            option.chunk_max_cells = nt<uint16>::max * nt<uint64>::digits;  // (状态单元限制)
        op = option;
        //初始化大块
        ::new (block) BlockSet{};
        //初始化小块
        chunk_count = find_counts_of_chunks(option.cell_max_size);
        option.cell_max_size = cell_sizes[chunk_count-1];
        for (size_t i = 0; i < chunk_count; ++i)
        {
            size_t cell_size = cell_sizes[i];
            size_t cell_count = max(op.chunk_min_cells, op.chunk_min_size / cell_size);
            ::new (chunks + i * sizeof(ChunkGroup)) ChunkGroup(cell_size, cell_count);
        }

        xylogl(xyu::N_LOG_INFO, xyu::K_LOG_MEMPOOL,
               xyfmtt(S256{}, "Init pool: (chunk_min_size={}, chunk_min_cells={}, chunk_max_cells={}, cell_max_size={}, grow_factor={}, chunk_count={})",
                      option.chunk_min_size, option.chunk_min_cells, option.chunk_max_cells, option.cell_max_size, option.grow_factor, chunk_count));
    }

    // 释放
    void MemPool_Block::release() noexcept
    {
        if (!chunk_count) return;

        xylogl(xyu::N_LOG_INFO, xyu::K_LOG_MEMPOOL, "Release pool start");

        ((BlockSet*)block)->release();

        for (size_t i = 0; i < chunk_count; ++i)
            ((ChunkGroup*)(chunks + i * sizeof(ChunkGroup)))->release();
        chunk_count = 0;

        xylogl(xyu::N_LOG_INFO, xyu::K_LOG_MEMPOOL, "Release pool finished");
    }

    // 分配内存
    void *MemPool_Block::alloc_helper(size_t bytes, size_t align)
    {
#if XY_DEBUG
        if (XY_UNLIKELY(!chunk_count)) {
            xyloge(0, "E_Resource_Invalid_State: Alloc memory after pool release or before init");
            throw xyu::E_Resource_Invalid_State{};
        }
#endif
        size_t ms = max(bytes, align);
        //大块内存
        if (ms > op.cell_max_size) {
            xylogl(xyu::N_LOG_TRACE, xyu::K_LOG_MEMPOOL,
                   xyfmtt(S128{}, "Alloc block: (bytes={}, align={})", bytes, align));
            return ((BlockSet*)block)->make(bytes, align);
        }
        //小块内存
        size_t index = find_index_of_chunks(ms);
        xylogl(xyu::N_LOG_TRACE, xyu::K_LOG_MEMPOOL,
               xyfmtt(S128{}, "Alloc cell: (bytes={}, align={}, index={})", bytes, align, index));
        return ((ChunkGroup*)(chunks + index * sizeof(ChunkGroup)))->get(op);
    }

    // 释放内存
    void MemPool_Block::dealloc_helper(void *p, size_t bytes, size_t align) XY_NOEXCEPT_NDEBUG
    {
#if XY_DEBUG
        // 正常情况下，不会发生此情况，下面的语句不会被触发
        if (XY_UNLIKELY(!chunk_count)) {
            xylogw(xyu::K_LOG_MEMPOOL, "Free memory after pool release");
            return;
        }
#endif
        size_t ms = max(bytes, align);
        //大块内存
        if (ms > op.cell_max_size) {
            xylogl(xyu::N_LOG_TRACE, xyu::K_LOG_MEMPOOL,
                   xyfmtt(S128{}, "Free block: (ptr={}, bytes={}, align={})", p, bytes, align));
            ((BlockSet*)block)->free(p, bytes, align);
        }
        //小块内存
        else
        {
            size_t index = find_index_of_chunks(ms);
            xylogl(xyu::N_LOG_TRACE, xyu::K_LOG_MEMPOOL,
                   xyfmtt(S128{}, "Free cell: (ptr={}, bytes={}, align={}, index={})", p, bytes, align, index));
            ((ChunkGroup*)(chunks + index * sizeof(ChunkGroup)))->put(p);
        }
    }
}

#pragma clang diagnostic pop