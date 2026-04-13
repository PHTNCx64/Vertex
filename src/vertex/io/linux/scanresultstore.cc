//
// Copyright (C) 2026 PHTNC<>.
// Licensed under GPLv3.0 with Plugin Interface exceptions.
//
#include <vertex/io/scanresultstore.hh>

#include <sys/mman.h>
#include <unistd.h>

#include <algorithm>
#include <cstdlib>
#include <limits>
#include <utility>

namespace Vertex::IO
{
    ScanResultStore::~ScanResultStore() noexcept { cleanup(); }

    ScanResultStore::ScanResultStore(ScanResultStore&& other) noexcept
        : m_fileDescriptor(std::exchange(other.m_fileDescriptor, -1)),
          m_mappedBase(std::exchange(other.m_mappedBase, nullptr)),
          m_dataSize(std::exchange(other.m_dataSize, 0)),
          m_writeBuffers(std::move(other.m_writeBuffers)),
          m_bufferSizes(other.m_bufferSizes),
          m_activeBufferIndex(std::exchange(other.m_activeBufferIndex, 0)),
          m_writeOffset(std::exchange(other.m_writeOffset, 0)),
          m_finalized(std::exchange(other.m_finalized, false))
    {
        other.m_bufferSizes.fill(0);
    }

    ScanResultStore& ScanResultStore::operator=(ScanResultStore&& other) noexcept
    {
        if (this != &other)
        {
            cleanup();

            m_fileDescriptor = std::exchange(other.m_fileDescriptor, -1);
            m_mappedBase = std::exchange(other.m_mappedBase, nullptr);
            m_dataSize = std::exchange(other.m_dataSize, 0);
            m_writeBuffers = std::move(other.m_writeBuffers);
            m_bufferSizes = other.m_bufferSizes;
            m_activeBufferIndex = std::exchange(other.m_activeBufferIndex, 0);
            m_writeOffset = std::exchange(other.m_writeOffset, 0);
            m_finalized = std::exchange(other.m_finalized, false);

            other.m_bufferSizes.fill(0);
        }
        return *this;
    }

    StatusCode ScanResultStore::open()
    {
        if (m_fileDescriptor != -1)
        {
            return StatusCode::STATUS_ERROR_GENERAL;
        }

        char tempPath[] = "/tmp/vxs-XXXXXX";
        const int fd = mkstemp(tempPath);
        if (fd == -1)
        {
            return StatusCode::STATUS_ERROR_GENERAL;
        }

        unlink(tempPath);

        m_fileDescriptor = fd;
        for (auto& buffer : m_writeBuffers)
        {
            buffer.reset();
        }
        m_bufferSizes.fill(0);
        m_activeBufferIndex = 0;
        m_dataSize = 0;
        m_writeOffset = 0;
        m_finalized = false;

        return StatusCode::STATUS_OK;
    }

    StatusCode ScanResultStore::append(const void* data, const std::size_t size)
    {
        if (m_fileDescriptor == -1 || m_finalized)
        {
            return StatusCode::STATUS_ERROR_GENERAL;
        }

        if (size == 0)
        {
            return StatusCode::STATUS_OK;
        }

        auto src = static_cast<const char*>(data);
        std::size_t remaining = size;

        while (remaining > 0)
        {
            auto& activeSize = m_bufferSizes[m_activeBufferIndex];
            auto& activeBuffer = m_writeBuffers[m_activeBufferIndex];
            if (!activeBuffer)
            {
                activeBuffer = std::make_unique<char[]>(WRITE_BUFFER_SIZE);
            }
            const std::size_t space = WRITE_BUFFER_SIZE - activeSize;
            const std::size_t toCopy = std::min(remaining, space);

            std::copy_n(src, toCopy, activeBuffer.get() + activeSize);
            activeSize += toCopy;
            src += toCopy;
            remaining -= toCopy;

            if (activeSize == WRITE_BUFFER_SIZE)
            {
                const StatusCode status = flush_buffer(true);
                if (status != StatusCode::STATUS_OK)
                {
                    return status;
                }
            }
        }

        m_dataSize += size;
        return StatusCode::STATUS_OK;
    }

    StatusCode ScanResultStore::wait_for_pending_flush() { return StatusCode::STATUS_OK; }

    StatusCode ScanResultStore::write_buffer_sync(const char* buffer, const std::size_t size, std::uint64_t offset) const
    {
        std::size_t remaining = size;
        const char* current = buffer;
        std::uint64_t currentOffset = offset;

        while (remaining > 0)
        {
            const std::size_t writeSize = std::min(remaining, static_cast<std::size_t>(std::numeric_limits<ssize_t>::max()));
            const ssize_t written = ::pwrite(m_fileDescriptor, current, writeSize, static_cast<off_t>(currentOffset));

            if (written <= 0)
            {
                return StatusCode::STATUS_ERROR_GENERAL;
            }

            current += written;
            remaining -= static_cast<std::size_t>(written);
            currentOffset += static_cast<std::uint64_t>(written);
        }

        return StatusCode::STATUS_OK;
    }

    StatusCode ScanResultStore::flush_buffer(const bool allowAsync)
    {
        auto& activeSize = m_bufferSizes[m_activeBufferIndex];
        if (activeSize == 0)
        {
            return StatusCode::STATUS_OK;
        }

        const char* activeBuffer = m_writeBuffers[m_activeBufferIndex].get();
        if (activeBuffer == nullptr)
        {
            return StatusCode::STATUS_ERROR_GENERAL;
        }

        StatusCode status = wait_for_pending_flush();
        if (status != StatusCode::STATUS_OK)
        {
            return status;
        }

        status = write_buffer_sync(activeBuffer, activeSize, m_writeOffset);
        if (status != StatusCode::STATUS_OK)
        {
            return status;
        }

        m_writeOffset += static_cast<std::uint64_t>(activeSize);
        activeSize = 0;

        if (allowAsync)
        {
            m_activeBufferIndex = (m_activeBufferIndex + 1) % m_writeBuffers.size();
        }

        return StatusCode::STATUS_OK;
    }

    StatusCode ScanResultStore::finalize()
    {
        if (m_finalized)
        {
            return StatusCode::STATUS_OK;
        }

        if (m_fileDescriptor == -1)
        {
            return StatusCode::STATUS_ERROR_GENERAL;
        }

        StatusCode status = wait_for_pending_flush();
        if (status != StatusCode::STATUS_OK)
        {
            return status;
        }

        status = flush_buffer(false);
        if (status != StatusCode::STATUS_OK)
        {
            return status;
        }

        if (m_dataSize == 0)
        {
            for (auto& buffer : m_writeBuffers)
            {
                buffer.reset();
            }
            m_finalized = true;
            return StatusCode::STATUS_OK;
        }

        void* mapped = mmap(nullptr, m_dataSize, PROT_READ, MAP_PRIVATE, m_fileDescriptor, 0);
        if (mapped == MAP_FAILED)
        {
            return StatusCode::STATUS_ERROR_MEMORY_ALLOCATION_FAILED;
        }

        m_mappedBase = mapped;
        for (auto& buffer : m_writeBuffers)
        {
            buffer.reset();
        }
        m_finalized = true;
        return StatusCode::STATUS_OK;
    }

    const void* ScanResultStore::base() const noexcept { return m_mappedBase; }

    std::size_t ScanResultStore::data_size() const noexcept { return m_dataSize; }

    bool ScanResultStore::is_valid() const noexcept { return m_finalized && (m_dataSize == 0 || m_mappedBase != nullptr); }

    void ScanResultStore::cleanup() noexcept
    {
        if (m_mappedBase != nullptr)
        {
            munmap(m_mappedBase, m_dataSize);
            m_mappedBase = nullptr;
        }

        if (m_fileDescriptor != -1)
        {
            ::close(m_fileDescriptor);
            m_fileDescriptor = -1;
        }

        for (auto& buffer : m_writeBuffers)
        {
            buffer.reset();
        }
        m_bufferSizes.fill(0);
        m_activeBufferIndex = 0;
        m_dataSize = 0;
        m_writeOffset = 0;
        m_finalized = false;
    }
} // namespace Vertex::IO
