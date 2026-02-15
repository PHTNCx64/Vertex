//
// Copyright (C) 2026 PHTNC<>.
// Licensed under GPLv3.0 with Plugin Interface exceptions.
//
#pragma once

#include <vertex/memory/alignedallocator.hh>
#include <vertex/memory/arenaallocator.hh>
#include <vertex/memory/mimallocresource.hh>
#include <vertex/memory/poolallocator.hh>

#include <cstdint>
#include <memory_resource>
#include <vector>

namespace Vertex::Memory
{
    class ScannerMemoryContext final
    {
    public:
        static constexpr std::size_t RESULT_RECORD_SIZE = CACHE_LINE_SIZE;
        static constexpr std::size_t RESULT_POOL_BLOCK_SIZE = 8192;

        explicit ScannerMemoryContext(std::size_t arenaSize = 64 * 1024 * 1024)
            : m_arena(arenaSize)
            , m_initialized(true)
        {
        }

        ~ScannerMemoryContext()
        {
            m_initialized = false;
        }

        ScannerMemoryContext(const ScannerMemoryContext&) = delete;
        ScannerMemoryContext& operator=(const ScannerMemoryContext&) = delete;
        ScannerMemoryContext(ScannerMemoryContext&&) = delete;
        ScannerMemoryContext& operator=(ScannerMemoryContext&&) = delete;

        [[nodiscard]] void* arena_allocate(const std::size_t size, const std::size_t alignment = CACHE_LINE_SIZE)
        {
            return m_arena.allocate(size, alignment);
        }

        [[nodiscard]] void* arena_allocate_nothrow(const std::size_t size, const std::size_t alignment = CACHE_LINE_SIZE) noexcept
        {
            return m_arena.allocate_nothrow(size, alignment);
        }

        template<class T, class... Args>
        [[nodiscard]] T* arena_create(Args&&... args)
        {
            return m_arena.create<T>(std::forward<Args>(args)...);
        }

        template<class T>
        [[nodiscard]] T* arena_allocate_array(std::size_t count)
        {
            return m_arena.allocate_array<T>(count);
        }

        [[nodiscard]] void* pool_allocate()
        {
            return m_resultPool.allocate();
        }

        [[nodiscard]] void* pool_allocate_nothrow() noexcept
        {
            return m_resultPool.allocate_nothrow();
        }

        void pool_deallocate(void* ptr) noexcept
        {
            m_resultPool.deallocate(ptr);
        }

        void reset() noexcept
        {
            m_arena.reset();
            m_resultPool.reset();
        }

        void clear() noexcept
        {
            m_arena.shrink_to_fit();
            m_resultPool.shrink_to_fit();
        }

        void destroy() noexcept
        {
            m_arena.clear_all();
            m_resultPool.clear();
            m_initialized = false;
        }

        [[nodiscard]] ArenaAllocator& arena() noexcept { return m_arena; }
        [[nodiscard]] const ArenaAllocator& arena() const noexcept { return m_arena; }

        [[nodiscard]] auto& result_pool() noexcept { return m_resultPool; }
        [[nodiscard]] const auto& result_pool() const noexcept { return m_resultPool; }

        [[nodiscard]] bool is_initialized() const noexcept { return m_initialized; }

        [[nodiscard]] std::size_t arena_bytes_allocated() const noexcept
        {
            return m_arena.total_allocated();
        }

        [[nodiscard]] std::size_t arena_capacity() const noexcept
        {
            return m_arena.total_capacity();
        }

        [[nodiscard]] std::size_t pool_objects_allocated() const noexcept
        {
            return m_resultPool.allocated_count();
        }

        [[nodiscard]] std::size_t pool_capacity() const noexcept
        {
            return m_resultPool.total_capacity();
        }

    private:
        ArenaAllocator m_arena;
        PoolAllocator<RESULT_RECORD_SIZE, RESULT_POOL_BLOCK_SIZE> m_resultPool;
        bool m_initialized{};
    };

    namespace detail
    {
        inline thread_local bool g_cleanupRegistered = false;
        inline thread_local ScannerMemoryContext* g_contextPtr = nullptr;
    }

    [[nodiscard]] inline ScannerMemoryContext& get_thread_memory_context()
    {
        thread_local ScannerMemoryContext context;
        detail::g_contextPtr = &context;
        return context;
    }

    inline void cleanup_thread_memory_context() noexcept
    {
        if (detail::g_contextPtr && detail::g_contextPtr->is_initialized())
        {
            detail::g_contextPtr->destroy();
        }
    }

    [[nodiscard]] inline bool has_thread_memory_context() noexcept
    {
        return detail::g_contextPtr != nullptr && detail::g_contextPtr->is_initialized();
    }

    using AlignedByteVector = std::vector<char, AlignedAllocator<char, CACHE_LINE_SIZE>>;
    using AlignedU64Vector = std::vector<std::uint64_t, AlignedAllocator<std::uint64_t, CACHE_LINE_SIZE>>;
    using AlignedU8Vector = std::vector<std::uint8_t, AlignedAllocator<std::uint8_t, CACHE_LINE_SIZE>>;

    using SimdAlignedByteVector = std::vector<char, AlignedAllocator<char, SIMD_ALIGNMENT>>;
    using SimdAlignedU8Vector = std::vector<std::uint8_t, AlignedAllocator<std::uint8_t, SIMD_ALIGNMENT>>;

    [[nodiscard]] inline std::pmr::vector<char> make_pmr_byte_vector(const std::size_t initialCapacity = 0)
    {
        std::pmr::vector<char> vec(MimallocMemoryResource::instance());
        if (initialCapacity > 0)
        {
            vec.reserve(initialCapacity);
        }
        return vec;
    }

    [[nodiscard]] inline std::pmr::vector<std::uint64_t> make_pmr_u64_vector(const std::size_t initialCapacity = 0)
    {
        std::pmr::vector<std::uint64_t> vec(MimallocMemoryResource::instance());
        if (initialCapacity > 0)
        {
            vec.reserve(initialCapacity);
        }
        return vec;
    }

    [[nodiscard]] inline std::pmr::vector<std::uint8_t> make_pmr_u8_vector(const std::size_t initialCapacity = 0)
    {
        std::pmr::vector<std::uint8_t> vec(MimallocMemoryResource::instance());
        if (initialCapacity > 0)
        {
            vec.reserve(initialCapacity);
        }
        return vec;
    }

    template<typename T>
    [[nodiscard]] inline std::pmr::vector<T> make_pmr_vector(const std::size_t initialCapacity = 0)
    {
        std::pmr::vector<T> vec(MimallocMemoryResource::instance());
        if (initialCapacity > 0)
        {
            vec.reserve(initialCapacity);
        }
        return vec;
    }

} // namespace Vertex::Memory
