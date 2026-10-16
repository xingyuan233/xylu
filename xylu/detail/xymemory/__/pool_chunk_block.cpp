#include "../pool.h"

#include "../../xymath/func.h"
#include "../../xystring/fmt.h"

#if !XY_OUTPUT_ERROR_LOG
#  undef xylog
#  define xylog(...) 0
#endif

using namespace xyu;

/// 辅助工具
namespace
{
    // 未初始化
    alignas(K_Cache_Line_Size) constexpr uint8 uninit_block[64] =
    {
        XY_PP_REPEAT( 0xCC, 16 ), XY_PP_REPEAT( 0xCC, 16 ),
        XY_PP_REPEAT( 0xCC, 16 ), XY_PP_REPEAT( 0xCC, 16 ),
    };

    // 禁止访问
    alignas(K_Cache_Line_Size) constexpr uint8 unallow_block[64] =
    {
        XY_PP_REPEAT( 0xDD, 16 ), XY_PP_REPEAT( 0xDD, 16 ),
        XY_PP_REPEAT( 0xDD, 16 ), XY_PP_REPEAT( 0xDD, 16 ),
    };

    // 越界访问检测
    XY_NOINLINE XY_COLD
    void log_unbound(const void* p, size_t bytes) noexcept
    {
        xylogf("xyu::MemPool_BC: out-of-bounds write {} bytes from {}", bytes, p);
    }

    XY_NOINLINE
    bool unallow_check(const void* p, size_t size)
    {
        bool safa = true;
        while (size > 0)
        {
            size_t chunk = size > sizeof(unallow_block) ? sizeof(unallow_block) : size;
            if (mem_cmp(p, unallow_block, chunk) != 0)
            {
                size_t si = 0;
                const auto* sp = (uint8*)p;
                while (sp[si] == unallow_block[0]) ++si;
                sp = sp + si;

                size_t last = size - si;
                si = 1;
                while (si < last && sp[si] != unallow_block[0]) ++si;

                if (N_Log_Level::Fatal <= K_Log_Level_New) log_unbound(sp, si);

                chunk = si + sp - (uint8*)p;
                safa = false;
            }
            p = (char*)p + chunk;
            size -= chunk;
        }
        return safa;
    }
}

/// 内存小块
namespace
{
    /// 辅助类 - 数组
    //用于一次性获取多个元素空间
    template <typename T>
    struct mArray
    {
        T* data = nullptr;  // 元素指针
        uint32 count = 0;   // 元素个数
        uint32 capa = 0;    // 容量

        // 获取多个空间
        T* get(uint32 n)
        {
            if (XY_UNLIKELY(!fix(n))) return nullptr;
            T* p = data + count;
            count += n;
            return p;
        }

        // 释放内存 (手动调用，一次性)
        void release() noexcept { if (data) dealloc<void>(native_v, data); }

    private:
        // 保证空间足够
        bool fix(uint32 n) noexcept
        {
            //容量充足
            if (n <= capa - count) return true;
            //容量不足
            constexpr uint32 maxcapa = bit_floor(t_number<uint32>::max / sizeof(T));
            uint32 mincapa = count + n;
            if (XY_UNLIKELY(mincapa > maxcapa)) abort_over_capacity(mincapa);
            uint32 newcapa = bit_ceil(count + n);
            if (XY_UNLIKELY(newcapa < 16)) newcapa = 16;
            //分配新内存
            T* newdata = realloc(native_v, nullptr, data, newcapa);
            if (XY_UNLIKELY(!newdata)) return false;
            data = newdata;
            capa = newcapa;
            return true;
        }

        [[noreturn]] XY_NOINLINE XY_COLD
        static void abort_over_capacity(uint32 mincapa) noexcept
        {
            constexpr uint32 maxcapa = bit_floor(t_number<uint32>::max / sizeof(T));
            xyloge_if(K_Log_Level_New,
                "xyu::MemPool_BC: chunk state bitmap capa {} cannot larger than maxcapa {}",
                mincapa, maxcapa);
            xyu::abort();
        }
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
        uint32 used_count;  // 使用中的单元数
        uint32 data_bytes;  // 数据单元总大小
        char* data_ptr;     // 数据指针

        // 初始化
        Chunk(uint16 state_count, uint32 cell_count, uint64* state_ptr, uint32 state_pos, uint64 data_bytes, char* data_ptr) noexcept
                : state_count(state_count), state_next(0), used_count(0), data_bytes(data_bytes), data_ptr(data_ptr), state_pos(state_pos)
        {
            mem_set(state_ptr, (char)0x00, state_count);
            uint32 last = cell_count % 64;
            if (last) state_ptr[state_count - 1] = uint64(-1) << last;
            if constexpr (XY_DEBUG) mem_set(data_ptr, unallow_block[0], data_bytes);
        }

        // 获取内存单元
        void* get(uint32 cell_size, uint64* sp) noexcept
        {
            if (state_next >= state_count) return nullptr;
            uint64* states = sp + state_pos;
            uint32 index_of_block = bit_count_1_low(states[state_next]);
            uint32 index = state_next * 64 + index_of_block;
            void* p = data_ptr + index * cell_size;
            if constexpr (XY_DEBUG) unallow_check(p, cell_size);
            ++used_count;
            states[state_next] |= uint64(1) << index_of_block;
            while (states[state_next] == uint64(-1))
                if (++state_next >= state_count) break;
            return p;
        }

        // 释放内存单元 (由调用者判断所属)
        void put(void* p, uint32 cell_size, uint64* sp) noexcept
        {
            auto err = [=] { if (N_Log_Level::Error <= K_Log_Level_New) log_invalid_free(p, cell_size); };

            uint32 diff = (char*)p - data_ptr;
            if constexpr (!XY_RELEASE) if (diff % cell_size != 0) return err();
            uint32 index = diff / cell_size;
            uint32 index_of_state = index / 64;
            uint64* states = sp + state_pos;
            if constexpr (!XY_RELEASE) if (!(states[index_of_state] & (uint64(1) << (index % 64)))) return err();
            if constexpr (XY_DEBUG) mem_set(p, unallow_block[0], cell_size);
            --used_count;
            states[index_of_state] &= ~(uint64(1) << (index % 64));
            if (state_next > index_of_state) state_next = index_of_state;
        }

        // 所属判断
        bool owns(void* p) const noexcept { return data_ptr <= p && p < data_ptr + data_bytes; }

        // 检查所有单元是否存在越界访问
        void check_unallow(uint32 cell_size, uint64* sp) const noexcept
        {
            uint32 cell_count = data_bytes / cell_size;
            uint64* states = sp + state_pos;
            for (uint32 i = 0; i < cell_count; ++i)
            {
                uint32 idx_state = i / 64;
                uint32 idx_bit   = i % 64;
                if (!(states[idx_state] & (uint64(1) << idx_bit)))
                    unallow_check(data_ptr + i * cell_size, cell_size);
            }
        }

        // 比较
        bool operator<(const Chunk& other) const noexcept { return data_ptr < other.data_ptr; }


        XY_NOINLINE XY_COLD
        static void log_invalid_free(void* p, uint32 cell_size) noexcept
        {
            xyloge("xyu::MemPool_BC: invalid cell free with ptr={}, size={}", p, cell_size);
        }
    };

    /// 内存小块组
    //管理可拓展的多个内存单元大小相同的Chunk
    struct ChunkGroup
    {
    private:
        uint32 cell_size;               // 内存单元大小
        uint32 cell_count;              // 下一个内存单元数量
        Chunk* chunks = nullptr;        // 内存块指针
        uint32 chunk_count = 0;         // 内存块数量
        uint32 chunk_capa = 0;          // 内存块容量
        mArray<uint64> states;          // 状态块数组
        uint32 last_free = -1;          // 上一个释放的块
        uint32 next_get = -1;           // 下一个获取的块

    public:
        // 初始化
        ChunkGroup(uint32 size, uint32 count) noexcept : cell_size(size), cell_count(count) {}

        // 获取一个内存小块
        void* get(const MemPool_BC::Option& op) noexcept
        {
            // 尝试获取内存小块
            for (uint32 i = next_get; i < chunk_count; ++i)
            {
                void* p = chunks[i].get(cell_size, states.data);
                if (p) { next_get = i; return p; }
            }
            // 失败，则创建新内存块
            if (XY_UNLIKELY(!create(op))) return nullptr;
            return chunks[next_get].get(cell_size, states.data);
        }

        // 预留 count 个内存单元
        bool expand(size_t count, const MemPool_BC::Option& op) noexcept
        {
            // 计算当前空闲单元数
            size_t free_cells = 0;
            if (chunk_count) for (uint32 i = next_get; i < chunk_count; ++i)
            {
                uint32 total_cells = chunks[i].data_bytes / cell_size;
                free_cells += total_cells - chunks[i].used_count;
                if (count <= free_cells) return true;
            }

            // 分配额外内存块
            auto tmp = next_get;
            do {
                free_cells += cell_count;
                if (XY_UNLIKELY(!create(op))) return false;
            } while (count > free_cells);
            if (tmp < next_get) next_get = tmp;
            return true;
        }

        // 释放一个内存小块
        void put(void* p) noexcept
        {
            auto err = [=] { if (N_Log_Level::Error <= K_Log_Level_New) Chunk::log_invalid_free(p, cell_size); };

            if (XY_UNLIKELY(!chunk_count)) return err();

            // 判断p是否属于最近释放的块
            if (chunks[last_free].owns(p))
                return chunks[last_free].put(p, cell_size, states.data);

            // 查找p所属的内存块
            uint32 start = 0, end = chunk_count;
            while (start < end)
            {
                uint32 mid = (start + end) >> 1;
                if (chunks[mid].data_ptr > p) end = mid;
                else start = mid + 1;
            }
            // p小于所有内存块
            if (XY_UNLIKELY(start == 0)) return err();
            --start;
            if (XY_UNLIKELY(p >= chunks[start].data_ptr + chunks[start].data_bytes)) return err();

            // 释放内存
            chunks[start].put(p, cell_size, states.data);
            last_free = start;
            if (start < next_get) next_get = start;
        }

        // 释放内存 (一次性)
        void release() noexcept
        {
            if (!chunks) return;
            for (uint32 i = 0; i < chunk_count; ++i)
            {
#if !XY_RELEASE
#if XY_DEBUG
                chunks[i].check_unallow(cell_size, states.data);
#endif
                if (chunks[i].used_count)
                    xylogw_if(K_Log_Level_New, "xyu::MemPool_BC: release chunk with ptr={}, size={}, bytes={}, unfree={}",
                            (void*)(chunks[i].data_ptr), cell_size, chunks[i].data_bytes, chunks[i].used_count);
                else
#endif
                    xylogd_if(K_Log_Level_New, "xyu::MemPool_BC: release chunk with ptr={}, size={}, bytes={}",
                            (void*)(chunks[i].data_ptr), cell_size, chunks[i].data_bytes);
                dealloc(native_v, chunks[i].data_ptr, cell_size & -cell_size);
            }
            dealloc<void>(native_v, chunks);
            states.release();
        }

        // 释放未使用的块
        void reduce(const MemPool_BC::Option& op) noexcept
        {
            // 找到第一个未使用的块
            uint32 rp;
            for (rp = 0; rp < chunk_count; ++rp)
                if (chunks[rp].used_count == 0) goto deal_start;
            return;

        deal_start:
            uint32 wp = rp;
            for (; rp < chunk_count; ++rp)
            {
                // 释放块
                if (chunks[rp].used_count == 0)
                {
                    if constexpr (XY_DEBUG) chunks[rp].check_unallow(cell_size, states.data);
                    if (next_get == rp) next_get = rp + 1;
                    dealloc(native_v, chunks[rp].data_ptr, cell_size & -cell_size);
                }
                // 移动块
                else
                {
                    if (next_get == rp) next_get = wp;
                    chunks[wp] = chunks[rp];
                    ++wp;
                }
            }

            // 状态块
            if (wp < 64 || states.count > 4096)
            {
                uint32 total_states = 0;
                for (uint32 i = 0; i < wp; ++i)
                    total_states += chunks[i].state_count;

                mArray<uint64> new_states;
                uint64* sp = new_states.get(total_states);

                if (XY_LIKELY(sp))
                {
                    for (uint32 i = 0, npos = 0; i < wp; ++i)
                    {
                        uint32 pos = chunks[i].state_pos;
                        chunks[i].state_pos = npos;
                        mem_copy(sp + npos, states.data + pos, chunks[i].state_count);
                        npos += chunks[i].state_count;
                    }
                    states.release();
                    states = new_states;
                }
            }

            // chunk块
            if (wp < chunk_capa / 4)
            {
                uint32 newcapa = bit_ceil(wp);
                auto* new_chunks = alloc<Chunk>(native_v, nullptr, newcapa);
                if (XY_UNLIKELY(!new_chunks)) goto skip_chunk;
                mem_copy(new_chunks, chunks, wp);
                dealloc<void>(native_v, chunks);
                chunk_capa = newcapa;
                chunks = new_chunks;

            skip_chunk:{}
            }

            // 状态调整
            if (wp)
            {
                size_t cell_min_count = max(op.chunk_min_cells, op.chunk_min_size / cell_size);
                cell_count = cell_count / calc_pow(op.grow_factor, chunk_count - wp);
                if (cell_count < cell_min_count) cell_count = cell_min_count;
            }
            chunk_count = wp;
            last_free = chunk_count - 1;
        }

    private:
        // 创新新内存块
        bool create(const MemPool_BC::Option& op) noexcept
        {
            //---分配内存块数据---
            if (XY_UNLIKELY(!reserve())) return false;

            size_t cell_bytes = (size_t)cell_size * cell_count;
            size_t align = cell_size & -cell_size;
            char* cp = alloc<char>(native_v, nullptr, cell_bytes, align);
            if (XY_UNLIKELY(!cp)) return false;

            //---分配状态块---
            size_t state_count = ((size_t)cell_count + 64 - 1) / 64;
            size_t state_pos = states.count;
            uint64* sp = states.get(state_count);
            if (XY_UNLIKELY(!sp)) { dealloc(native_v, cp, align); return false; }

            //---插入新内存快---
            Chunk newchunk(state_count, cell_count, sp, state_pos, cell_bytes, cp);
            uint32 i = bsearch_nolower_first(chunks, chunks + chunk_count, newchunk) - chunks;
            if (i < chunk_count) mem_move(chunks + i + 1, chunks + i, chunk_count - i);
            chunks[i] = move(newchunk);
            ++chunk_count;

            //---动态调整数据---
            if (i <= last_free) last_free++;
            next_get = i;

            if (XY_LIKELY(cell_count < op.chunk_max_cells))
            {
                cell_count = cell_count * op.grow_factor;
                if (XY_UNLIKELY(cell_count > op.chunk_max_cells)) cell_count = op.chunk_max_cells;
                auto max_cells = t_number<uint32>::max / cell_size;
                if (XY_UNLIKELY(cell_count > max_cells)) cell_count = max_cells;
            }
            return true;
        }

        // 确保有容量剩余
        bool reserve() noexcept
        {
            if (chunk_count < chunk_capa) return true;
            //容量不足
            uint32 newcapa = chunk_capa << 1;
            if (XY_UNLIKELY(newcapa < 8)) newcapa = 8;
            else if (XY_UNLIKELY(newcapa < chunk_capa))
            {
                xyloge_if(K_Log_Level_New, "xyu::MemPool_BC: chunk of cell_size={} can't over {} groups",
                        cell_size, chunk_count);
                xyu::abort();
            }
            //分配新内存
            Chunk* newc = realloc(native_v, nullptr, chunks, newcapa);
            if (XY_UNLIKELY(!newc)) return false;
            chunks = newc;
            chunk_capa = newcapa;
            return true;
        }

    private:
        // 二分查找
        template <typename T, typename U>
        T* bsearch_nolower_first(T* start, T* end, const U& value) noexcept
        {
            while (start < end)
            {
                T* middle = start + ((end - start) >> 1);
                if (*middle < value) start = middle + 1;
                else end = middle;
            }
            return start;
        }
    };
}

/// 内存大块
namespace
{
    /// 内存大块组
    //单独分配的大型内存块，使用底层alloc
#if !XY_RELEASE
    struct BlockLink
    {
    private:
        struct Node
        {
            Node* prev;     // 上一个节点
            Node* next;     // 下一个节点
            uint64 magic;   // 魔法数字 (校验指针合法性)
            size_t bytes;   // 内存大小
            size_t align;   // 内存对齐
        };

        Node root;      // 哨兵节点
        size_t count;   // 节点数量

        static constexpr uint64 K_MAGIC = 0xDEADC0DE12345678ULL;  // 魔法数字

    public:
        // 初始化
        BlockLink() noexcept : root{&root, &root, K_MAGIC}, count{0} {}

        // 移动
        BlockLink(BlockLink&& other) noexcept : count(other.count)
        {
            if (other.count == 0) root.prev = root.next = &root;
            else {
                root.prev = other.root.prev;
                root.next = other.root.next;
                root.prev->next = root.next->prev = &root;
            }
            root.magic = K_MAGIC;
        }

        // 创建一个大内存块
        void* make(size_t bytes, size_t align) noexcept
        {
            //---分配内存---
            size_t newalign = max(alignof(Node), align);
            size_t offset = (sizeof(Node) + newalign - 1) & -newalign;
            void* p = alloc<char>(native_v, nullptr, offset + bytes, newalign);
            if (XY_UNLIKELY(!p)) return nullptr;

            //---构建节点---
            Node* node = static_cast<Node*>(p);
            node->magic = K_MAGIC;
            node->bytes = bytes;
            node->align = align;

            //---连接节点---
            Node* prev = root.prev;
            node->prev = prev;
            node->next = &root;
            prev->next = node;
            root.prev = node;
            count++;

            return (char*)p + offset;
        }

        // 释放一个大内存块
        void free(void* p, size_t bytes, size_t align) noexcept
        {
            // 计算 Node 位置
            size_t newalign = max(alignof(Node), align);
            size_t offset = (sizeof(Node) + newalign - 1) & -newalign;
            Node* node = (Node*)((char*)p - offset);

            if (XY_UNLIKELY(node->magic != K_MAGIC))
            {
                if (N_Log_Level::Error <= K_Log_Level_New)
                    log_invalid_free_magic(p, node->magic);
                return;
            }

            if (XY_UNLIKELY(node->bytes != bytes || node->align != align))
            {
                if (N_Log_Level::Warn <= K_Log_Level_New)
                    log_invalid_free_param(p, bytes, align, node->bytes, node->align);
            }

            // 断开链表
            node->prev->next = node->next;
            node->next->prev = node->prev;
            node->magic = 0;
            count--;

            // 归还系统内存
            dealloc(native_v, node, max(alignof(Node), node->align));
        }

        // 释放内存
        void release() noexcept
        {
            Node* n = root.next;
            while (n != &root)
            {
                Node* next = n->next;
                void* p = (char*)n + ((sizeof(Node) + n->align - 1) & -n->align);
                xylogw_if(K_Log_Level_New, "xyu::MemPool_BC: release block unfree with ptr={}, bytes={}, align={}",
                                            p, n->bytes, n->align);
                dealloc(native_v, n, max(alignof(Node), n->align));
                n = next;
            }
        }

    private:
        XY_NOINLINE XY_COLD
        static void log_invalid_free_magic(void* p, size_t magic) noexcept
        {
            xyloge("xyu::MemPool_BC: invalid block free with ptr={} magic={} (expected={})", p, magic, K_MAGIC);
        }

        XY_NOINLINE XY_COLD
        static void log_invalid_free_param(void* p, size_t bytes, size_t align, size_t expected_bytes, size_t expected_align) noexcept
        {
            if (bytes == expected_bytes)
                xylogw("xyu::MemPool_BC: mismatch block free with ptr={} bytes={} align={} (expected={})", p, bytes, align, expected_align);
            else if (align == expected_align)
                xylogw("xyu::MemPool_BC: mismatch block free with ptr={} bytes={} (expected={}) align={}", p, bytes, expected_bytes, align);
            else
                xylogw("xyu::MemPool_BC: mismatch block free with ptr={} bytes={} (expected={}) align={} (expected={})",
                                          p, bytes, expected_bytes, align, expected_align);
        }
    };
#else
    struct BlockLink
    {
        //创建一个大内存块
        static void* make(size_t bytes, size_t align) noexcept { return alloc<void>(native_v, nullptr, bytes, align); }

        //释放一个大内存块
        static void free(void* p, size_t, size_t align) noexcept { dealloc<void>(native_v, p, align); }

        //释放内存
        void release() noexcept {}
    };
#endif
}

/// 池分块信息
namespace
{
    constexpr uint32 chunks_count = 24;
    constexpr uint32 cell_max_size = 4096;

    // 每个池的内存单元大小
    constexpr uint32 cell_sizes[] =
    {
        8, 16, 24, 32, 48, 64, 80, 96, 112, 128,
        192, 256, 320, 384, 448, 512, 768, 1024,
        1536, 2048, 2560, 3072, 3584, 4096,
    };

    // 找到最合适的池的索引
    XY_HOT XY_PURE size_t find_index_of_chunks(uint32 bytes) noexcept
    {
        // bytes >= 1
        --bytes;
        if (bytes < 128)
        {
            if (bytes < 32) return bytes >> 3;
            return (bytes >> 4) + 2;
        }
        if (bytes < 512) return (bytes >> 6) + 8;
        if (bytes < 1024) return (bytes >> 8) + 14;
        return (bytes >> 9) + 16;
    }
}

/// 创建与释放
namespace xylu::xymemory
{
    XY_NOINLINE
    static void log_create(const MemPool_BC::Option& op) noexcept
    {
        xylogi("xyu::MemPool_BC: created with chunk_min_size={}, chunk_min_cells={}, chunk_max_cells={}, grow_factor={}, chunks_count={}",
               op.chunk_min_size, op.chunk_min_cells, op.chunk_max_cells, op.grow_factor, chunks_count);
    }

    // 创建
    void MemPool_BC::create(Option option) noexcept
    {
        if constexpr (!XY_RELEASE) if (XY_UNLIKELY(is_inited)) xyloge("xyu::MemPool_BC::create: pool already inited"), xyu::abort();
        //---修正配置---
        // 扩容因子
        option.grow_factor = max(1.f, option.grow_factor);
        // 最小单元数量
        option.chunk_min_cells = max(1, option.chunk_min_cells);
        // 最大单元数量
        option.chunk_max_cells = max(option.chunk_min_cells, option.chunk_max_cells);
        // 最多单元数量 (状态单元限制 ≈ 400万)
        option.chunk_max_cells = min(option.chunk_max_cells, t_number<uint16>::max * t_number<uint64>::digits);
        // 保存修正后的配置
        op = option;

        //---管理器初始化---
        // 初始化大块
        static_assert(sizeof(BlockLink) <= sizeof(block));
        new (block) BlockLink{};
        // 初始化小块
        static_assert(chunks_count * sizeof(ChunkGroup) <= sizeof(chunks));
        for (size_t i = 0; i < chunks_count; ++i)
        {
            size_t cell_size = cell_sizes[i];
            size_t cell_count = max(op.chunk_min_cells, op.chunk_min_size / cell_size);
            new (chunks + i * sizeof(ChunkGroup)) ChunkGroup(cell_size, cell_count);
        }

        //---完成初始化---
        is_inited = true;

        if (N_Log_Level::Info <= K_Log_Level_New) log_create(op);
    }

    XY_NOINLINE
    static void log_destroy() noexcept
    {
        xylogi("xyu::MemPool_BC: release pool finished");
    }

    // 释放所有
    void MemPool_BC::release() noexcept
    {
        // 未初始化
        if (XY_UNLIKELY(!is_inited)) return;
        // 释放大块
        ((BlockLink*)block)->release();
        // 释放小块
        for (size_t i = 0; i < chunks_count; ++i)
            ((ChunkGroup*)(chunks + i * sizeof(ChunkGroup)))->release();
        // 释放完毕
        is_inited = false;

        if (N_Log_Level::Info <= K_Log_Level_New) log_destroy();
    }

    // 移动
    MemPool_BC::MemPool_BC(MemPool_BC&& other) noexcept : is_inited(other.is_inited)
    {
        if (!is_inited) return;
        op = other.op;
        new ((BlockLink*)block)BlockLink(move(*(BlockLink*)other.block));
        for (uint32 i = 0; i < chunks_count; ++i)
            new ((ChunkGroup*)chunks + i) ChunkGroup(move(((ChunkGroup*)(other.chunks))[i]));
        other.is_inited = false;
    }
}

/// 分配
namespace xylu::xymemory
{
    [[noreturn]] XY_NOINLINE XY_COLD
    static void abort_uninited(bool alloc) noexcept
    {
        if (alloc) xyloge("xyu::MemPool_BC::alloc: alloc memory before pool init or after pool released");
        else xyloge("xyu::MemPool_BC::dealloc: dealloc memory before pool init or after pool released");
        xyu::abort();
    }

    XY_NOINLINE
    static void log_alloc(size_t bytes, size_t align, size_t index) noexcept
    {
        if (index == -1) xylogt("xyu::MemPool_BC: alloc block with bytes={}, align={}", bytes, align);
        else xylogt("xyu::MemPool_BC: alloc cell with bytes={}, align={}, index={}", bytes, align, index);
    }

    // 分配内存
    void* MemPool_BC::alloc_helper(size_t bytes, size_t align) noexcept
    {
        if constexpr (XY_DEBUG) if (XY_UNLIKELY(!is_inited)) abort_uninited(true);
        size_t ms = (bytes + align - 1) & -align;
        //小块内存
        if (ms <= cell_max_size)
        {
            size_t index = find_index_of_chunks(ms);
            if (N_Log_Level::Trace <= K_Log_Level_New) log_alloc(bytes, align, index);
            void* p = ((ChunkGroup*)(chunks))[index].get(op);
            if constexpr (XY_DEBUG) if (XY_LIKELY(p))
            {
                // 标记为未初始化
                mem_set(p, uninit_block[0], align == bytes && align <= K_Default_Align ? bytes : 1);
                // 标记为禁止访问
                mem_set((char*)p + bytes, unallow_block[0], cell_sizes[index] - bytes);
            }
            return p;
        }
        //大块内存
        else
        {
            if (N_Log_Level::Trace <= K_Log_Level_New) log_alloc(bytes, align, -1);
            return ((BlockLink*)block)->make(bytes, align);
        }
    }

    XY_NOINLINE
    static void log_prealloc(size_t index, size_t count) noexcept
    {
        xylogi("xyu::MemPool_BC: reserve {} cells with size={}", count, cell_sizes[index]);
    }

    // 预分配内存块
    bool MemPool_BC::prealloc_helper(size_t cell_size, size_t count) noexcept
    {
        if constexpr (XY_DEBUG) if (XY_UNLIKELY(!is_inited)) abort_uninited(true);
        if (XY_UNLIKELY(cell_size > cell_max_size)) return false;
        size_t index = find_index_of_chunks(cell_size);
        if (N_Log_Level::Debug <= K_Log_Level_New) log_prealloc(index, count);
        return ((ChunkGroup*)(chunks + index * sizeof(ChunkGroup)))->expand(count, op);
    }
}

/// 释放
namespace xylu::xymemory
{
    XY_NOINLINE
    static void log_dealloc(void* p, size_t bytes, size_t align, size_t index) noexcept
    {
        if (index == -1) xylogt("xyu::MemPool_BC: free block with ptr={}, bytes={}, align={}", p, bytes, align);
        else xylogt("xyu::MemPool_BC: free cell with ptr={}, bytes={}, align={}, index={}", p, bytes, align, index);
    }

    XY_NOINLINE XY_COLD
    static void log_uninit(void* p, size_t bytes, size_t align, size_t index, const char* state) noexcept
    {
        xylogw("xyu::MemPool_BC: free cell with ptr={}, bytes={}, align={}, index={}, uninit={}",
                            p, bytes, align, index, state);
    }

    // 释放内存
    void MemPool_BC::dealloc_helper(void* p, size_t bytes, size_t align) noexcept
    {
        if constexpr (XY_DEBUG) if (XY_UNLIKELY(!is_inited)) abort_uninited(false);
        size_t ms = (bytes + align - 1) & -align;
        //小块内存
        if (ms <= cell_max_size)
        {
            size_t index = find_index_of_chunks(ms);
#if XY_DEBUG
            bool safe = true;

            // 字符串
            if (align == 1 && bytes > 1 && *(uint8*)p == uninit_block[0])
            {
                safe = false;
                if (N_Log_Level::Warn <= K_Log_Level_New) log_uninit(p, bytes, align, index, "perhaps");
            }
            // 基本类型
            else if (align == bytes && align <= K_Default_Align && mem_cmp(p, uninit_block, bytes) == 0)
            {
                static_assert(K_Default_Align <= sizeof(uninit_block));
                safe = false;
                if (N_Log_Level::Warn <= K_Log_Level_New) log_uninit(p, bytes, align, index, "probably");
            }
            // 其他无法明确类型不做检测

            // 越界访问
            if (XY_UNLIKELY(unallow_check((char*)p + bytes, cell_sizes[index] - bytes))) safe = false;

            // 已初始化且未越界访问
            if (safe)
#endif
            if (N_Log_Level::Trace <= K_Log_Level_New) log_dealloc(p, bytes, align, index);
            ((ChunkGroup*)(chunks))[index].put(p);
        }
        //大块内存
        else
        {
            if (N_Log_Level::Trace <= K_Log_Level_New) log_dealloc(p, bytes, align, -1);
            ((BlockLink*)block)->free(p, bytes, align);
        }
    }

    XY_NOINLINE
    static void log_unalloc(size_t index) noexcept
    {
        xylogi("xyu::MemPool_BC: release unused chunk with size={}", cell_sizes[index]);
    }

    // 释放空闲内存块
    void MemPool_BC::unalloc_helper(size_t cell_size) noexcept
    {
        if constexpr (XY_DEBUG) if (XY_UNLIKELY(!is_inited)) abort_uninited(false);
        if (XY_UNLIKELY(cell_size >  cell_max_size)) return;
        size_t index = find_index_of_chunks(cell_size);
        if (N_Log_Level::Debug <= K_Log_Level_New) log_unalloc(index);
        ((ChunkGroup*)(chunks + index * sizeof(ChunkGroup)))->reduce(op);
    }

    XY_NOINLINE
    static void log_unalloc() noexcept
    {
        xylogi("xyu::MemPool_BC: release unused chunks");
    }

    // 释放空闲内存
    void MemPool_BC::unalloc() noexcept
    {
        if constexpr (XY_DEBUG) if (XY_UNLIKELY(!is_inited)) abort_uninited(false);
        if (N_Log_Level::Info <= K_Log_Level_New) log_unalloc();
        for (size_t i = 0; i < chunks_count; ++i)
            ((ChunkGroup*)(chunks + i * sizeof(ChunkGroup)))->reduce(op);
    }
}
