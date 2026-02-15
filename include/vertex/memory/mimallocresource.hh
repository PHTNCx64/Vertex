//
// Copyright (C) 2026 PHTNC<>.
// Licensed under GPLv3.0 with Plugin Interface exceptions.
//
#pragma once

#include <memory_resource>
#include <mimalloc.h>
#include <new>

namespace Vertex::Memory
{
    class MimallocMemoryResource final : public std::pmr::memory_resource
    {
    public:
        [[nodiscard]] static MimallocMemoryResource* instance() noexcept
        {
            static MimallocMemoryResource instance;
            return &instance;
        }

    private:
        MimallocMemoryResource() = default;

        void* do_allocate(const std::size_t bytes, const std::size_t alignment) override
        {
            void* ptr = mi_malloc_aligned(bytes, alignment);
            if (!ptr)
            {
                throw std::bad_alloc();
            }
            return ptr;
        }

        void do_deallocate(void* ptr, [[maybe_unused]] std::size_t bytes, std::size_t alignment) noexcept override
        {
            mi_free_aligned(ptr, alignment);
        }

        [[nodiscard]] bool do_is_equal(const memory_resource& other) const noexcept override
        {
            return dynamic_cast<const MimallocMemoryResource*>(&other) != nullptr;
        }
    };

} // namespace Vertex::Memory
