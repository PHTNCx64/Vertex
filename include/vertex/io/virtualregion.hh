//
// Copyright (C) 2026 PHTNC<>.
// Licensed under GPLv3.0 with Plugin Interface exceptions.
//
#pragma once

#include <sdk/statuscode.h>
#include <cstddef>

namespace Vertex::IO
{
    class VirtualRegion final
    {
      public:
        VirtualRegion() = default;
        ~VirtualRegion() noexcept;

        VirtualRegion(const VirtualRegion&) = delete;
        VirtualRegion& operator=(const VirtualRegion&) = delete;

        VirtualRegion(VirtualRegion&& other) noexcept;
        VirtualRegion& operator=(VirtualRegion&& other) noexcept;

        [[nodiscard]] StatusCode reserve(std::size_t reserveBytes);
        [[nodiscard]] StatusCode ensure_committed(std::size_t neededBytes);
        void release() noexcept;

        [[nodiscard]] void* base() const noexcept;
        [[nodiscard]] std::size_t reserved_bytes() const noexcept;
        [[nodiscard]] std::size_t committed_bytes() const noexcept;
        [[nodiscard]] bool is_reserved() const noexcept;

      private:
        void* m_baseAddr{};
        std::size_t m_reservedBytes{};
        std::size_t m_committedBytes{};
        static constexpr std::size_t COMMIT_GRANULARITY = 64ULL * 1024 * 1024;
    };
} // namespace Vertex::IO
