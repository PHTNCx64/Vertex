//
// Copyright (C) 2026 PHTNC<>.
// Licensed under GPLv3.0 with Plugin Interface exceptions.
//
#pragma once

#include <vertex/memory/memoryconstants.hh>

#include <limits>
#include <mimalloc.h>
#include <new>

namespace Vertex::Memory
{
    template<class T, std::size_t Alignment = CACHE_LINE_SIZE>
    class AlignedAllocator
    {
    public:
        using value_type = T;
        using size_type = std::size_t;
        using difference_type = std::ptrdiff_t;
        using propagate_on_container_move_assignment = std::true_type;
        using is_always_equal = std::true_type;

        AlignedAllocator() noexcept = default;

        template<class U>
        explicit AlignedAllocator(const AlignedAllocator<U, Alignment>&) noexcept {}

        [[nodiscard]] T* allocate(const std::size_t n)
        {
            if (n == 0)
            {
                return nullptr;
            }

            if (n > std::numeric_limits<std::size_t>::max() / sizeof(T))
            {
                throw std::bad_array_new_length();
            }

            void* ptr = mi_malloc_aligned(n * sizeof(T), Alignment);
            if (!ptr)
            {
                throw std::bad_alloc();
            }

            return static_cast<T*>(ptr);
        }

        void deallocate(T* ptr, [[maybe_unused]] std::size_t n) noexcept
        {
            mi_free_aligned(ptr, Alignment);
        }

        template<class U>
        struct rebind
        {
            using other = AlignedAllocator<U, Alignment>;
        };
    };

    template<class T, class U, std::size_t A>
    [[nodiscard]] constexpr bool operator==(const AlignedAllocator<T, A>&, const AlignedAllocator<U, A>&) noexcept
    {
        return true;
    }

} // namespace Vertex::Memory
