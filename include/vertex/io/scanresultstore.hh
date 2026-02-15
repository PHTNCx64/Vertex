//
// Copyright (C) 2026 PHTNC<>.
// Licensed under GPLv3.0 with Plugin Interface exceptions.
//
#pragma once

#include <sdk/statuscode.h>
#include <cstddef>
#include <memory>

namespace Vertex::IO
{
    class ScanResultStore final
    {
      public:
        ScanResultStore() = default;
        ~ScanResultStore() noexcept;

        ScanResultStore(const ScanResultStore&) = delete;
        ScanResultStore& operator=(const ScanResultStore&) = delete;

        ScanResultStore(ScanResultStore&& other) noexcept;
        ScanResultStore& operator=(ScanResultStore&& other) noexcept;

        [[nodiscard]] StatusCode open();
        [[nodiscard]] StatusCode append(const void* data, std::size_t size);
        [[nodiscard]] StatusCode finalize();

        [[nodiscard]] const void* base() const noexcept;
        [[nodiscard]] std::size_t data_size() const noexcept;
        [[nodiscard]] bool is_valid() const noexcept;

      private:
        void cleanup() noexcept;
        [[nodiscard]] StatusCode flush_buffer();

        void* m_fileHandle{};
        void* m_mappingHandle{};
        void* m_mappedBase{};
        std::size_t m_dataSize{};
        std::unique_ptr<char[]> m_writeBuffer{};
        std::size_t m_bufferPos{};
        bool m_finalized{};

        static constexpr std::size_t WRITE_BUFFER_SIZE = 4ULL * 1024 * 1024;
    };
} // namespace Vertex::IO
