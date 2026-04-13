//
// Copyright (C) 2026 PHTNC<>.
// Licensed under GPLv3.0 with Plugin Interface exceptions.
//
#include <vertex/io/scanresultstore.hh>

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>

#include <algorithm>
#include <limits>
#include <utility>

namespace Vertex::IO
{
    ScanResultStore::~ScanResultStore() noexcept { cleanup(); }

    ScanResultStore::ScanResultStore(ScanResultStore&& other) noexcept
        : m_fileHandle(other.m_fileHandle),
          m_mappingHandle(other.m_mappingHandle),
          m_mappedBase(other.m_mappedBase),
          m_dataSize(other.m_dataSize),
          m_writeBuffers(std::move(other.m_writeBuffers)),
          m_bufferSizes(other.m_bufferSizes),
          m_activeBufferIndex(other.m_activeBufferIndex),
          m_writeOffset(other.m_writeOffset),
          m_finalized(other.m_finalized)
    {
        other.m_fileHandle = nullptr;
        other.m_mappingHandle = nullptr;
        other.m_mappedBase = nullptr;
        other.m_dataSize = 0;
        other.m_bufferSizes.fill(0);
        other.m_activeBufferIndex = 0;
        other.m_writeOffset = 0;
        other.m_finalized = false;
    }

    ScanResultStore& ScanResultStore::operator=(ScanResultStore&& other) noexcept
    {
        if (this != &other)
        {
            cleanup();

            m_fileHandle = other.m_fileHandle;
            m_mappingHandle = other.m_mappingHandle;
            m_mappedBase = other.m_mappedBase;
            m_dataSize = other.m_dataSize;
            m_writeBuffers = std::move(other.m_writeBuffers);
            m_bufferSizes = other.m_bufferSizes;
            m_activeBufferIndex = other.m_activeBufferIndex;
            m_writeOffset = other.m_writeOffset;
            m_finalized = other.m_finalized;

            other.m_fileHandle = nullptr;
            other.m_mappingHandle = nullptr;
            other.m_mappedBase = nullptr;
            other.m_dataSize = 0;
            other.m_bufferSizes.fill(0);
            other.m_activeBufferIndex = 0;
            other.m_writeOffset = 0;
            other.m_finalized = false;
        }
        return *this;
    }

    StatusCode ScanResultStore::open()
    {
        if (m_fileHandle != nullptr)
        {
            return StatusCode::STATUS_ERROR_GENERAL;
        }

        wchar_t tempPath[MAX_PATH]{};
        if (GetTempPathW(MAX_PATH, tempPath) == 0)
        {
            return StatusCode::STATUS_ERROR_GENERAL;
        }

        wchar_t tempFile[MAX_PATH]{};
        if (GetTempFileNameW(tempPath, L"vxs", 0, tempFile) == 0)
        {
            return StatusCode::STATUS_ERROR_GENERAL;
        }

        HANDLE hFile = CreateFileW(tempFile, GENERIC_READ | GENERIC_WRITE, 0, nullptr, CREATE_ALWAYS, FILE_ATTRIBUTE_TEMPORARY | FILE_FLAG_SEQUENTIAL_SCAN | FILE_FLAG_DELETE_ON_CLOSE, nullptr);

        if (hFile == INVALID_HANDLE_VALUE)
        {
            DeleteFileW(tempFile);
            return StatusCode::STATUS_ERROR_MEMORY_ALLOCATION_FAILED;
        }

        m_fileHandle = hFile;
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
        if (m_fileHandle == nullptr || m_finalized)
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

    StatusCode ScanResultStore::write_buffer_sync(const char* buffer, const std::size_t size, const std::uint64_t offset) const
    {
        LARGE_INTEGER fileOffset{};
        fileOffset.QuadPart = static_cast<LONGLONG>(offset);
        if (!SetFilePointerEx(m_fileHandle, fileOffset, nullptr, FILE_BEGIN))
        {
            return StatusCode::STATUS_ERROR_GENERAL;
        }

        std::size_t remaining = size;
        const char* current = buffer;

        while (remaining > 0)
        {
            const std::size_t writeSize = std::min(remaining, static_cast<std::size_t>(std::numeric_limits<DWORD>::max()));
            DWORD bytesWritten{};
            if (!WriteFile(m_fileHandle, current, static_cast<DWORD>(writeSize), &bytesWritten, nullptr))
            {
                return StatusCode::STATUS_ERROR_GENERAL;
            }

            if (bytesWritten == 0)
            {
                return StatusCode::STATUS_ERROR_GENERAL;
            }

            current += bytesWritten;
            remaining -= static_cast<std::size_t>(bytesWritten);
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

        StatusCode status = wait_for_pending_flush();
        if (status != StatusCode::STATUS_OK)
        {
            return status;
        }

        status = write_buffer_sync(m_writeBuffers[m_activeBufferIndex].get(), activeSize, m_writeOffset);
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

        if (m_fileHandle == nullptr)
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

        m_mappingHandle = CreateFileMappingW(m_fileHandle, nullptr, PAGE_READONLY, static_cast<DWORD>(static_cast<std::uint64_t>(m_dataSize) >> 32), static_cast<DWORD>(m_dataSize & 0xFFFFFFFF), nullptr);

        if (m_mappingHandle == nullptr)
        {
            return StatusCode::STATUS_ERROR_MEMORY_ALLOCATION_FAILED;
        }

        m_mappedBase = MapViewOfFile(m_mappingHandle, FILE_MAP_READ, 0, 0, m_dataSize);

        if (m_mappedBase == nullptr)
        {
            CloseHandle(m_mappingHandle);
            m_mappingHandle = nullptr;
            return StatusCode::STATUS_ERROR_MEMORY_ALLOCATION_FAILED;
        }

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
            UnmapViewOfFile(m_mappedBase);
            m_mappedBase = nullptr;
        }

        if (m_mappingHandle != nullptr)
        {
            CloseHandle(m_mappingHandle);
            m_mappingHandle = nullptr;
        }

        if (m_fileHandle != nullptr)
        {
            CloseHandle(m_fileHandle);
            m_fileHandle = nullptr;
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
