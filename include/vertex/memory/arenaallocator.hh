//
// Copyright (C) 2026 PHTNC<>.
// Licensed under GPLv3.0 with Plugin Interface exceptions.
//
#pragma once

#include <vertex/memory/memoryconstants.hh>

#include <algorithm>
#include <cassert>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <mimalloc.h>
#include <new>

namespace Vertex::Memory
{
    class ArenaAllocator
    {
        struct Chunk
        {
            Chunk* next;
            std::size_t size;
            std::size_t used;
            alignas(CACHE_LINE_SIZE) std::byte data[];

            [[nodiscard]] static Chunk* create(const std::size_t dataSize)
            {
                const std::size_t totalSize = sizeof(Chunk) + dataSize;
                auto* chunk = static_cast<Chunk*>(mi_malloc_aligned(totalSize, CACHE_LINE_SIZE));
                if (!chunk)
                {
                    throw std::bad_alloc();
                }

                chunk->next = nullptr;
                chunk->size = dataSize;
                chunk->used = 0;
                return chunk;
            }

            [[nodiscard]] static Chunk* create_nothrow(const std::size_t dataSize) noexcept
            {
                const std::size_t totalSize = sizeof(Chunk) + dataSize;
                auto* chunk = static_cast<Chunk*>(mi_malloc_aligned(totalSize, CACHE_LINE_SIZE));
                if (!chunk)
                {
                    return nullptr;
                }

                chunk->next = nullptr;
                chunk->size = dataSize;
                chunk->used = 0;
                return chunk;
            }

            static void destroy(Chunk* chunk) noexcept
            {
                if (chunk)
                {
                    mi_free_aligned(chunk, CACHE_LINE_SIZE);
                }
            }
        };

    public:
        explicit ArenaAllocator(const std::size_t initialSize = 64 * 1024 * 1024)
            : m_defaultChunkSize(initialSize)
        {
            m_currentChunk = Chunk::create(initialSize);
            m_firstChunk = m_currentChunk;
            m_chunkCount = 1;

            VERTEX_MEMORY_STAT_INC(arenaChunksCreated);
        }

        ~ArenaAllocator()
        {
            clear_all();
        }

        ArenaAllocator(const ArenaAllocator&) = delete;
        ArenaAllocator& operator=(const ArenaAllocator&) = delete;

        ArenaAllocator(ArenaAllocator&& other) noexcept
            : m_firstChunk(other.m_firstChunk)
            , m_currentChunk(other.m_currentChunk)
            , m_defaultChunkSize(other.m_defaultChunkSize)
            , m_chunkCount(other.m_chunkCount)
            , m_totalAllocated(other.m_totalAllocated)
            , m_totalCapacity(other.m_totalCapacity)
        {
            other.m_firstChunk = nullptr;
            other.m_currentChunk = nullptr;
            other.m_chunkCount = 0;
            other.m_totalAllocated = 0;
            other.m_totalCapacity = 0;
        }

        ArenaAllocator& operator=(ArenaAllocator&&) = delete;

        [[nodiscard]] void* allocate(std::size_t size, std::size_t alignment = CACHE_LINE_SIZE)
        {
            assert((alignment & (alignment - 1)) == 0 && "Alignment must be power of 2");
            assert(alignment > 0 && "Alignment must be non-zero");

            const auto currentAddr = reinterpret_cast<std::uintptr_t>(m_currentChunk->data + m_currentChunk->used);
            const std::size_t alignmentMask = alignment - 1;
            const std::size_t alignedAddr = (currentAddr + alignmentMask) & ~alignmentMask;
            const std::size_t padding = alignedAddr - currentAddr;
            const std::size_t totalSize = padding + size;

            if (m_currentChunk->used + totalSize > m_currentChunk->size)
            {
                const std::size_t newChunkSize = std::max(m_defaultChunkSize, size + alignment);

                Chunk* newChunk = Chunk::create(newChunkSize);

                m_currentChunk->next = newChunk;
                m_currentChunk = newChunk;
                m_totalCapacity += newChunkSize;
                ++m_chunkCount;

                VERTEX_MEMORY_STAT_INC(arenaChunksCreated);

                const auto newAddr = reinterpret_cast<std::uintptr_t>(m_currentChunk->data);
                const std::size_t newAlignedAddr = (newAddr + alignmentMask) & ~alignmentMask;
                const std::size_t newPadding = newAlignedAddr - newAddr;
                const std::size_t newTotalSize = newPadding + size;

                void* result = m_currentChunk->data + newPadding;
                m_currentChunk->used = newTotalSize;
                m_totalAllocated += size;

                VERTEX_MEMORY_STAT_INC(arenaAllocations);
                VERTEX_MEMORY_STAT_ADD(arenaBytesAllocated, size);

                return result;
            }

            void* result = m_currentChunk->data + m_currentChunk->used + padding;
            m_currentChunk->used += totalSize;
            m_totalAllocated += size;

            VERTEX_MEMORY_STAT_INC(arenaAllocations);
            VERTEX_MEMORY_STAT_ADD(arenaBytesAllocated, size);

            return result;
        }

        [[nodiscard]] void* allocate_nothrow(std::size_t size, std::size_t alignment = CACHE_LINE_SIZE) noexcept
        {
            assert((alignment & (alignment - 1)) == 0 && "Alignment must be power of 2");

            const auto currentAddr = reinterpret_cast<std::uintptr_t>(m_currentChunk->data + m_currentChunk->used);
            const std::size_t alignmentMask = alignment - 1;
            const std::size_t alignedAddr = (currentAddr + alignmentMask) & ~alignmentMask;
            const std::size_t padding = alignedAddr - currentAddr;
            const std::size_t totalSize = padding + size;

            if (m_currentChunk->used + totalSize > m_currentChunk->size)
            {
                const std::size_t newChunkSize = std::max(m_defaultChunkSize, size + alignment);

                Chunk* newChunk = Chunk::create_nothrow(newChunkSize);
                if (!newChunk)
                {
                    return nullptr;
                }

                m_currentChunk->next = newChunk;
                m_currentChunk = newChunk;
                m_totalCapacity += newChunkSize;
                ++m_chunkCount;

                VERTEX_MEMORY_STAT_INC(arenaChunksCreated);

                const auto newAddr = reinterpret_cast<std::uintptr_t>(m_currentChunk->data);
                const std::size_t newAlignedAddr = (newAddr + alignmentMask) & ~alignmentMask;
                const std::size_t newPadding = newAlignedAddr - newAddr;
                const std::size_t newTotalSize = newPadding + size;

                void* result = m_currentChunk->data + newPadding;
                m_currentChunk->used = newTotalSize;
                m_totalAllocated += size;

                return result;
            }

            void* result = m_currentChunk->data + m_currentChunk->used + padding;
            m_currentChunk->used += totalSize;
            m_totalAllocated += size;

            return result;
        }

        template<class T, class... Args>
        [[nodiscard]] T* create(Args&&... args)
        {
            void* ptr = allocate(sizeof(T), alignof(T));
            return std::construct_at(static_cast<T*>(ptr), std::forward<Args>(args)...);
        }

        template<class T>
        [[nodiscard]] T* allocate_array(const std::size_t count)
        {
            if (count == 0)
            {
                return nullptr;
            }

            void* ptr = allocate(sizeof(T) * count, alignof(T));
            T* array = static_cast<T*>(ptr);

            if constexpr (!std::is_trivially_default_constructible_v<T>)
            {
                for (std::size_t i = 0; i < count; ++i)
                {
                    std::construct_at(&array[i]);
                }
            }

            return array;
        }

        void reset() noexcept
        {
            Chunk* chunk = m_firstChunk;
            while (chunk)
            {
                chunk->used = 0;
                chunk = chunk->next;
            }
            m_currentChunk = m_firstChunk;
            m_totalAllocated = 0;
        }

        void shrink_to_fit() noexcept
        {
            if (!m_firstChunk)
            {
                return;
            }

            Chunk* chunk = m_firstChunk->next;
            m_firstChunk->next = nullptr;
            m_firstChunk->used = 0;
            m_currentChunk = m_firstChunk;

            while (chunk)
            {
                Chunk* next = chunk->next;
                m_totalCapacity -= chunk->size;
                Chunk::destroy(chunk);
                chunk = next;
                --m_chunkCount;
            }

            m_totalAllocated = 0;
        }

        void clear_all() noexcept
        {
            Chunk* chunk = m_firstChunk;
            while (chunk)
            {
                Chunk* next = chunk->next;
                Chunk::destroy(chunk);
                chunk = next;
            }
            m_firstChunk = nullptr;
            m_currentChunk = nullptr;
            m_chunkCount = 0;
            m_totalAllocated = 0;
            m_totalCapacity = 0;
        }

        [[nodiscard]] std::size_t total_allocated() const noexcept { return m_totalAllocated; }
        [[nodiscard]] std::size_t total_capacity() const noexcept { return m_totalCapacity; }
        [[nodiscard]] std::size_t chunk_count() const noexcept { return m_chunkCount; }
        [[nodiscard]] std::size_t default_chunk_size() const noexcept { return m_defaultChunkSize; }

        [[nodiscard]] bool is_valid() const noexcept { return m_firstChunk != nullptr; }

    private:
        Chunk* m_firstChunk{};
        Chunk* m_currentChunk{};
        std::size_t m_defaultChunkSize;
        std::size_t m_chunkCount{};
        std::size_t m_totalAllocated{};
        std::size_t m_totalCapacity{};
    };

} // namespace Vertex::Memory
