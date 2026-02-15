//
// Copyright (C) 2026 PHTNC<>.
// Licensed under GPLv3.0 with Plugin Interface exceptions.
//
#pragma once

#include <atomic>
#include <cstdint>
#include <new>

namespace Vertex::Memory
{
    inline constexpr std::size_t CACHE_LINE_SIZE = std::hardware_destructive_interference_size;
    inline constexpr std::size_t SIMD_ALIGNMENT = 32;
    inline constexpr std::size_t PAGE_SIZE = 4096;

#if VERTEX_MEMORY_DEBUG
    struct MemoryStats
    {
        std::atomic<std::uint64_t> arenaAllocations{};
        std::atomic<std::uint64_t> arenaBytesAllocated{};
        std::atomic<std::uint64_t> arenaChunksCreated{};
        std::atomic<std::uint64_t> poolAllocations{};
        std::atomic<std::uint64_t> poolDeallocations{};
        std::atomic<std::uint64_t> poolBlocksCreated{};

        void reset() noexcept
        {
            arenaAllocations.store(0, std::memory_order_relaxed);
            arenaBytesAllocated.store(0, std::memory_order_relaxed);
            arenaChunksCreated.store(0, std::memory_order_relaxed);
            poolAllocations.store(0, std::memory_order_relaxed);
            poolDeallocations.store(0, std::memory_order_relaxed);
            poolBlocksCreated.store(0, std::memory_order_relaxed);
        }
    };

    [[nodiscard]] inline MemoryStats& get_memory_stats() noexcept
    {
        static MemoryStats stats;
        return stats;
    }

    #define VERTEX_MEMORY_STAT_INC(stat) get_memory_stats().stat.fetch_add(1, std::memory_order_relaxed)
    #define VERTEX_MEMORY_STAT_ADD(stat, val) get_memory_stats().stat.fetch_add(val, std::memory_order_relaxed)
#else
    #define VERTEX_MEMORY_STAT_INC(stat) ((void)0)
    #define VERTEX_MEMORY_STAT_ADD(stat, val) ((void)0)
#endif

} // namespace Vertex::Memory
