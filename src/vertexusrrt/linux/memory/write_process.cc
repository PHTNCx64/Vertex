//
// Copyright (C) 2026 PHTNC<>.
// Licensed under LGPLv3.0+
//
#include <vertexusrrt/native_handle.hh>
#include <sdk/api.h>

#include <algorithm>
#include <cerrno>
#include <cstdint>
#include <fstream>
#include <limits>
#include <string>
#include <vector>

#include <sys/uio.h>
#include <unistd.h>

extern native_handle& get_native_handle();

namespace
{
    [[nodiscard]] std::size_t get_iovec_limit()
    {
        long iovMax = sysconf(_SC_IOV_MAX);
        if (iovMax <= 0)
        {
#if defined(IOV_MAX)
            iovMax = IOV_MAX;
#else
            iovMax = 1024;
#endif
        }

        const auto clamped = std::clamp<long>(
            iovMax,
            1,
            static_cast<long>(std::numeric_limits<std::uint32_t>::max()));
        return static_cast<std::size_t>(clamped);
    }
}

extern "C" VERTEX_EXPORT StatusCode VERTEX_API vertex_memory_write_process(const std::uint64_t address, const std::uint64_t size, const char* buffer)
{
    if (!buffer || size == 0)
    {
        return StatusCode::STATUS_ERROR_INVALID_PARAMETER;
    }

    const auto& nativeHandle = get_native_handle();
    if (nativeHandle == INVALID_HANDLE_VALUE)
    {
        return StatusCode::STATUS_ERROR_PROCESS_INVALID;
    }

    iovec localIov{.iov_base = const_cast<char*>(buffer), .iov_len = size};
    iovec remoteIov{.iov_base = reinterpret_cast<void*>(address), .iov_len = size};

    auto bytesWritten = process_vm_writev(nativeHandle, &localIov, 1, &remoteIov, 1, 0);

    if (bytesWritten == static_cast<ssize_t>(size))
    {
        return StatusCode::STATUS_OK;
    }

    const auto memPath = std::string{"/proc/"} + std::to_string(nativeHandle) + "/mem";
    std::fstream memFile{memPath, std::ios::in | std::ios::out | std::ios::binary};

    if (!memFile.is_open())
    {
        return StatusCode::STATUS_ERROR_MEMORY_WRITE;
    }

    memFile.seekp(static_cast<std::streamoff>(address));
    memFile.write(buffer, static_cast<std::streamsize>(size));

    return memFile.good() ? StatusCode::STATUS_OK : StatusCode::STATUS_ERROR_MEMORY_WRITE;
}

extern "C" VERTEX_EXPORT StatusCode VERTEX_API vertex_memory_write_process_bulk(const BulkWriteRequest* requests,
                                                                                  BulkWriteResult* results,
                                                                                  const std::uint32_t count)
{
    if (requests == nullptr || results == nullptr)
    {
        return StatusCode::STATUS_ERROR_INVALID_PARAMETER;
    }

    if (count == 0)
    {
        return StatusCode::STATUS_OK;
    }

    const auto& nativeHandle = get_native_handle();
    if (nativeHandle == INVALID_HANDLE_VALUE)
    {
        return StatusCode::STATUS_ERROR_PROCESS_INVALID;
    }

    for (std::uint32_t i{}; i < count; ++i)
    {
        if (requests[i].buffer == nullptr && requests[i].size > 0)
        {
            return StatusCode::STATUS_ERROR_INVALID_PARAMETER;
        }

        if (requests[i].size > static_cast<std::uint64_t>(std::numeric_limits<std::size_t>::max()))
        {
            return StatusCode::STATUS_ERROR_INVALID_PARAMETER;
        }
    }

    const std::size_t maxIovecs = get_iovec_limit();
    std::vector<iovec> localIovs(maxIovecs);
    std::vector<iovec> remoteIovs(maxIovecs);
    std::size_t offset{};
    while (offset < count)
    {
        while (offset < count && requests[offset].size == 0)
        {
            results[offset].status = StatusCode::STATUS_OK;
            ++offset;
        }

        if (offset >= count)
        {
            break;
        }

        const std::size_t chunkCount = std::min(maxIovecs, static_cast<std::size_t>(count) - offset);

        std::uint64_t expectedBytes{};
        for (std::size_t i{}; i < chunkCount; ++i)
        {
            const auto& [address, size, buffer] = requests[offset + i];
            localIovs[i] = {
                .iov_base = const_cast<void*>(buffer),
                .iov_len = static_cast<std::size_t>(size)
            };
            remoteIovs[i] = {
                .iov_base = reinterpret_cast<void*>(address),
                .iov_len = static_cast<std::size_t>(size)
            };
            expectedBytes += size;
        }

        const auto bytesWritten = process_vm_writev(
            nativeHandle,
            localIovs.data(),
            static_cast<unsigned long>(chunkCount),
            remoteIovs.data(),
            static_cast<unsigned long>(chunkCount),
            0);

        if (bytesWritten < 0)
        {
            switch (errno)
            {
                case ESRCH:
                    return StatusCode::STATUS_ERROR_PROCESS_INVALID;
                case EFAULT:
                case EIO:
                    results[offset].status = StatusCode::STATUS_ERROR_MEMORY_WRITE;
                    ++offset;
                    continue;
                case EINVAL:
                case ENOMEM:
                default:
                    return StatusCode::STATUS_ERROR_MEMORY_WRITE;
            }
        }

        const auto transferredBytes = static_cast<std::uint64_t>(bytesWritten);
        if (transferredBytes >= expectedBytes)
        {
            for (std::size_t i{}; i < chunkCount; ++i)
            {
                results[offset + i].status = StatusCode::STATUS_OK;
            }
            offset += chunkCount;
            continue;
        }

        std::uint64_t accumulated{};
        std::size_t processed{};
        for (std::size_t i{}; i < chunkCount; ++i)
        {
            const auto entrySize = requests[offset + i].size;
            if (accumulated + entrySize <= transferredBytes)
            {
                results[offset + i].status = StatusCode::STATUS_OK;
                accumulated += entrySize;
                ++processed;
                continue;
            }

            results[offset + i].status = StatusCode::STATUS_ERROR_MEMORY_WRITE;
            ++processed;
            break;
        }

        if (processed == 0)
        {
            results[offset].status = StatusCode::STATUS_ERROR_MEMORY_WRITE;
            processed = 1;
        }

        offset += processed;
    }

    return StatusCode::STATUS_OK;
}
