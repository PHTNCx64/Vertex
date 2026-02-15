//
// Copyright (C) 2026 PHTNC<>.
// Licensed under GPLv3.0 with Plugin Interface exceptions.
//
#pragma once

#include <vertex/memory/memoryconstants.hh>

#include <algorithm>
#include <array>
#include <cstddef>
#include <mimalloc.h>

namespace Vertex::Memory
{
    template<std::size_t ObjectSize, std::size_t BlockObjects = 4096>
    class PoolAllocator
    {
        static_assert(ObjectSize >= sizeof(void*), "Object size must be at least pointer size for free list");
        static_assert(BlockObjects > 0, "BlockObjects must be greater than 0");

        struct FreeNode
        {
            FreeNode* next;
        };

        struct Block
        {
            alignas(CACHE_LINE_SIZE) std::array<std::byte, ObjectSize * BlockObjects> data;
            Block* nextBlock;
        };

    public:
        PoolAllocator() = default;

        ~PoolAllocator()
        {
            clear();
        }

        PoolAllocator(const PoolAllocator&) = delete;
        PoolAllocator& operator=(const PoolAllocator&) = delete;

        PoolAllocator(PoolAllocator&& other) noexcept
            : m_freeList(other.m_freeList)
            , m_blocks(other.m_blocks)
            , m_blockCount(other.m_blockCount)
            , m_allocatedCount(other.m_allocatedCount)
        {
            other.m_freeList = nullptr;
            other.m_blocks = nullptr;
            other.m_blockCount = 0;
            other.m_allocatedCount = 0;
        }

        PoolAllocator& operator=(PoolAllocator&& other) noexcept
        {
            if (this != &other)
            {
                clear();
                m_freeList = other.m_freeList;
                m_blocks = other.m_blocks;
                m_blockCount = other.m_blockCount;
                m_allocatedCount = other.m_allocatedCount;
                other.m_freeList = nullptr;
                other.m_blocks = nullptr;
                other.m_blockCount = 0;
                other.m_allocatedCount = 0;
            }
            return *this;
        }

        [[nodiscard]] void* allocate()
        {
            if (!m_freeList)
            {
                allocate_block();
            }

            FreeNode* node = m_freeList;
            m_freeList = node->next;
            ++m_allocatedCount;

            VERTEX_MEMORY_STAT_INC(poolAllocations);

            return node;
        }

        [[nodiscard]] void* allocate_nothrow() noexcept
        {
            if (!m_freeList)
            {
                if (!allocate_block_nothrow())
                {
                    return nullptr;
                }
            }

            FreeNode* node = m_freeList;
            m_freeList = node->next;
            ++m_allocatedCount;

            VERTEX_MEMORY_STAT_INC(poolAllocations);

            return node;
        }

        void deallocate(void* ptr) noexcept
        {
            if (!ptr)
            {
                return;
            }

            auto* node = static_cast<FreeNode*>(ptr);
            node->next = m_freeList;
            m_freeList = node;
            --m_allocatedCount;

            VERTEX_MEMORY_STAT_INC(poolDeallocations);
        }

        void clear() noexcept
        {
            while (m_blocks)
            {
                Block* next = m_blocks->nextBlock;
                mi_free_aligned(m_blocks, CACHE_LINE_SIZE);
                m_blocks = next;
            }
            m_freeList = nullptr;
            m_blockCount = 0;
            m_allocatedCount = 0;
        }

        void reset() noexcept
        {
            m_freeList = nullptr;
            m_allocatedCount = 0;

            Block* block = m_blocks;
            while (block)
            {
                for (std::size_t i = BlockObjects; i > 0; --i)
                {
                    auto* node = reinterpret_cast<FreeNode*>(&block->data[(i - 1) * ObjectSize]);
                    node->next = m_freeList;
                    m_freeList = node;
                }
                block = block->nextBlock;
            }
        }

        void shrink_to_fit(const std::size_t minBlocksToKeep = 1) noexcept
        {
            if (m_blockCount <= minBlocksToKeep)
            {
                return;
            }

            const std::size_t blocksNeeded = (m_allocatedCount + BlockObjects - 1) / BlockObjects;
            const std::size_t blocksToKeep = std::max(blocksNeeded, minBlocksToKeep);

            if (m_blockCount <= blocksToKeep)
            {
                return;
            }

            Block* current = m_blocks;
            Block* prev = nullptr;
            std::size_t kept = 0;

            while (current && kept < blocksToKeep)
            {
                prev = current;
                current = current->nextBlock;
                ++kept;
            }

            if (prev)
            {
                prev->nextBlock = nullptr;
            }

            while (current)
            {
                Block* next = current->nextBlock;
                mi_free_aligned(current, CACHE_LINE_SIZE);
                current = next;
                --m_blockCount;
            }

            reset();
        }

        [[nodiscard]] std::size_t allocated_count() const noexcept { return m_allocatedCount; }
        [[nodiscard]] std::size_t block_count() const noexcept { return m_blockCount; }
        [[nodiscard]] std::size_t total_capacity() const noexcept { return m_blockCount * BlockObjects; }
        [[nodiscard]] std::size_t free_count() const noexcept { return total_capacity() - m_allocatedCount; }
        [[nodiscard]] static constexpr std::size_t object_size() noexcept { return ObjectSize; }
        [[nodiscard]] static constexpr std::size_t objects_per_block() noexcept { return BlockObjects; }

    private:
        void allocate_block()
        {
            auto* block = static_cast<Block*>(mi_malloc_aligned(sizeof(Block), CACHE_LINE_SIZE));
            if (!block)
            {
                throw std::bad_alloc();
            }

            initialize_block(block);
        }

        [[nodiscard]] bool allocate_block_nothrow() noexcept
        {
            auto* block = static_cast<Block*>(mi_malloc_aligned(sizeof(Block), CACHE_LINE_SIZE));
            if (!block)
            {
                return false;
            }

            initialize_block(block);
            return true;
        }

        void initialize_block(Block* block) noexcept
        {
            block->nextBlock = m_blocks;
            m_blocks = block;
            ++m_blockCount;

            VERTEX_MEMORY_STAT_INC(poolBlocksCreated);

            for (std::size_t i = BlockObjects; i > 0; --i)
            {
                auto* node = reinterpret_cast<FreeNode*>(&block->data[(i - 1) * ObjectSize]);
                node->next = m_freeList;
                m_freeList = node;
            }
        }

        FreeNode* m_freeList{};
        Block* m_blocks{};
        std::size_t m_blockCount{};
        std::size_t m_allocatedCount{};
    };

} // namespace Vertex::Memory
