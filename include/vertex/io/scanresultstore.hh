//
// Copyright (C) 2026 PHTNC<>.
// Licensed under GPLv3.0 with Plugin Interface exceptions.
//
#pragma once

#include <sdk/statuscode.h>
#include <array>
#include <cstddef>
#include <cstdint>
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
        static constexpr std::size_t WRITE_BUFFER_COUNT = 1;
        static constexpr std::size_t WRITE_BUFFER_SIZE = 4ULL * 1024 * 1024;

        void cleanup() noexcept;
        [[nodiscard]] StatusCode flush_buffer(bool allowAsync);
        [[nodiscard]] StatusCode wait_for_pending_flush();
        [[nodiscard]] StatusCode write_buffer_sync(const char* buffer, std::size_t size, std::uint64_t offset) const;

#if defined(_WIN32) || defined(_WIN64)
        void* m_fileHandle{};
        void* m_mappingHandle{};
#else
        int m_fileDescriptor{-1};
#endif
        void* m_mappedBase{};
        std::size_t m_dataSize{};
        std::array<std::unique_ptr<char[]>, WRITE_BUFFER_COUNT> m_writeBuffers{};
        std::array<std::size_t, WRITE_BUFFER_COUNT> m_bufferSizes{};
        std::size_t m_activeBufferIndex{};
        std::uint64_t m_writeOffset{};
        bool m_finalized{};
    };
} // namespace Vertex::IO
