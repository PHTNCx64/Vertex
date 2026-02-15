//
// Copyright (C) 2026 PHTNC<>.
// Licensed under GPLv3.0 with Plugin Interface exceptions.
//
#include <vertex/io/scanresultstore.hh>

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <Windows.h>

#include <algorithm>
#include <cstring>

namespace Vertex::IO
{
    ScanResultStore::~ScanResultStore() noexcept
    {
        cleanup();
    }

    ScanResultStore::ScanResultStore(ScanResultStore&& other) noexcept
        : m_fileHandle(other.m_fileHandle)
        , m_mappingHandle(other.m_mappingHandle)
        , m_mappedBase(other.m_mappedBase)
        , m_dataSize(other.m_dataSize)
        , m_writeBuffer(std::move(other.m_writeBuffer))
        , m_bufferPos(other.m_bufferPos)
        , m_finalized(other.m_finalized)
    {
        other.m_fileHandle = nullptr;
        other.m_mappingHandle = nullptr;
        other.m_mappedBase = nullptr;
        other.m_dataSize = 0;
        other.m_bufferPos = 0;
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
            m_writeBuffer = std::move(other.m_writeBuffer);
            m_bufferPos = other.m_bufferPos;
            m_finalized = other.m_finalized;

            other.m_fileHandle = nullptr;
            other.m_mappingHandle = nullptr;
            other.m_mappedBase = nullptr;
            other.m_dataSize = 0;
            other.m_bufferPos = 0;
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

        HANDLE hFile = CreateFileW(
            tempFile,
            GENERIC_READ | GENERIC_WRITE,
            0,
            nullptr,
            CREATE_ALWAYS,
            FILE_ATTRIBUTE_TEMPORARY | FILE_FLAG_SEQUENTIAL_SCAN | FILE_FLAG_DELETE_ON_CLOSE,
            nullptr);

        if (hFile == INVALID_HANDLE_VALUE)
        {
            DeleteFileW(tempFile);
            return StatusCode::STATUS_ERROR_MEMORY_ALLOCATION_FAILED;
        }

        m_fileHandle = hFile;
        m_writeBuffer = std::make_unique<char[]>(WRITE_BUFFER_SIZE);
        m_bufferPos = 0;
        m_dataSize = 0;
        m_finalized = false;

        return StatusCode::STATUS_OK;
    }

    StatusCode ScanResultStore::append(const void* data, const std::size_t size)
    {
        if (m_fileHandle == nullptr || m_finalized)
        {
            return StatusCode::STATUS_ERROR_GENERAL;
        }

        auto src = static_cast<const char*>(data);
        std::size_t remaining = size;

        while (remaining > 0)
        {
            const std::size_t space = WRITE_BUFFER_SIZE - m_bufferPos;
            const std::size_t toCopy = std::min(remaining, space);

            std::memcpy(m_writeBuffer.get() + m_bufferPos, src, toCopy);
            m_bufferPos += toCopy;
            src += toCopy;
            remaining -= toCopy;

            if (m_bufferPos == WRITE_BUFFER_SIZE)
            {
                const StatusCode status = flush_buffer();
                if (status != StatusCode::STATUS_OK)
                {
                    return status;
                }
            }
        }

        m_dataSize += size;
        return StatusCode::STATUS_OK;
    }

    StatusCode ScanResultStore::flush_buffer()
    {
        if (m_bufferPos == 0)
        {
            return StatusCode::STATUS_OK;
        }

        DWORD bytesWritten{};
        if (!WriteFile(m_fileHandle, m_writeBuffer.get(), static_cast<DWORD>(m_bufferPos), &bytesWritten, nullptr))
        {
            return StatusCode::STATUS_ERROR_GENERAL;
        }

        m_bufferPos = 0;
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

        const StatusCode status = flush_buffer();
        if (status != StatusCode::STATUS_OK)
        {
            return status;
        }

        if (m_dataSize == 0)
        {
            m_finalized = true;
            return StatusCode::STATUS_OK;
        }

        m_mappingHandle = CreateFileMappingW(
            m_fileHandle,
            nullptr,
            PAGE_READONLY,
            static_cast<DWORD>(static_cast<std::uint64_t>(m_dataSize) >> 32),
            static_cast<DWORD>(m_dataSize & 0xFFFFFFFF),
            nullptr);

        if (m_mappingHandle == nullptr)
        {
            return StatusCode::STATUS_ERROR_MEMORY_ALLOCATION_FAILED;
        }

        m_mappedBase = MapViewOfFile(
            m_mappingHandle,
            FILE_MAP_READ,
            0,
            0,
            m_dataSize);

        if (m_mappedBase == nullptr)
        {
            CloseHandle(m_mappingHandle);
            m_mappingHandle = nullptr;
            return StatusCode::STATUS_ERROR_MEMORY_ALLOCATION_FAILED;
        }

        m_finalized = true;
        return StatusCode::STATUS_OK;
    }

    const void* ScanResultStore::base() const noexcept
    {
        return m_mappedBase;
    }

    std::size_t ScanResultStore::data_size() const noexcept
    {
        return m_dataSize;
    }

    bool ScanResultStore::is_valid() const noexcept
    {
        return m_finalized && (m_dataSize == 0 || m_mappedBase != nullptr);
    }

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

        m_dataSize = 0;
        m_bufferPos = 0;
        m_finalized = false;
    }
} // namespace Vertex::IO
