//
// Copyright (C) 2026 PHTNC<>.
// Licensed under LGPLv3.0+
//
#include <vertexusrrt/native_handle.hh>
#include <sdk/api.h>

#include <algorithm>
#include <cerrno>
#include <cstdint>
#include <limits>
#include <vector>

#include <sys/uio.h>
#include <unistd.h>

extern native_handle& get_native_handle();

namespace
{
    const std::uint64_t PAGE_SIZE = static_cast<std::uint64_t>(sysconf(_SC_PAGESIZE));

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

    [[nodiscard]] std::uint64_t align_to_next_page(const std::uint64_t addr)
    {
        return (addr + PAGE_SIZE) & ~(PAGE_SIZE - 1);
    }
}

extern "C" VERTEX_EXPORT StatusCode VERTEX_API vertex_memory_read_process(const std::uint64_t address, const std::uint64_t size, char* buffer)
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

    const iovec localIov{.iov_base = buffer, .iov_len = size};
    const iovec remoteIov{.iov_base = reinterpret_cast<void*>(address), .iov_len = size};

    const auto bytesRead = process_vm_readv(nativeHandle, &localIov, 1, &remoteIov, 1, 0);

    if (bytesRead == static_cast<ssize_t>(size)) [[likely]]
    {
        return StatusCode::STATUS_OK;
    }

    if (size <= PAGE_SIZE)
    {
        return StatusCode::STATUS_ERROR_MEMORY_READ;
    }

    std::uint64_t offset = (bytesRead > 0) ? static_cast<std::uint64_t>(bytesRead) : 0;

    while (offset < size)
    {
        const std::uint64_t currentAddr = address + offset;
        const std::uint64_t nextPage = align_to_next_page(currentAddr);
        const std::uint64_t skipBytes = std::min(nextPage - currentAddr, size - offset);

        std::fill_n(buffer + offset, skipBytes, 0);
        offset += skipBytes;

        if (offset >= size)
        {
            break;
        }

        const std::uint64_t remaining = size - offset;
        const iovec retryLocal{.iov_base = buffer + offset, .iov_len = remaining};
        const iovec retryRemote{.iov_base = reinterpret_cast<void*>(address + offset), .iov_len = remaining};

        const auto retryRead = process_vm_readv(nativeHandle, &retryLocal, 1, &retryRemote, 1, 0);

        if (retryRead == static_cast<ssize_t>(remaining))
        {
            return StatusCode::STATUS_OK;
        }

        if (retryRead > 0)
        {
            offset += static_cast<std::uint64_t>(retryRead);
        }
    }

    return StatusCode::STATUS_OK;
}

extern "C" VERTEX_EXPORT StatusCode VERTEX_API vertex_memory_read_process_bulk(const BulkReadRequest* requests,
                                                                                 BulkReadResult* results,
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
            const auto& request = requests[offset + i];
            localIovs[i] = {
                .iov_base = request.buffer,
                .iov_len = static_cast<std::size_t>(request.size)
            };
            remoteIovs[i] = {
                .iov_base = reinterpret_cast<void*>(request.address),
                .iov_len = static_cast<std::size_t>(request.size)
            };
            expectedBytes += request.size;
        }

        const auto bytesRead = process_vm_readv(
            nativeHandle,
            localIovs.data(),
            static_cast<unsigned long>(chunkCount),
            remoteIovs.data(),
            static_cast<unsigned long>(chunkCount),
            0);

        if (bytesRead < 0)
        {
            switch (errno)
            {
                case ESRCH:
                    return StatusCode::STATUS_ERROR_PROCESS_INVALID;
                case EFAULT:
                case EIO:
                    results[offset].status = StatusCode::STATUS_ERROR_MEMORY_READ;
                    ++offset;
                    continue;
                case EINVAL:
                case ENOMEM:
                default:
                    return StatusCode::STATUS_ERROR_MEMORY_READ;
            }
        }

        const std::uint64_t transferredBytes = static_cast<std::uint64_t>(bytesRead);
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

            results[offset + i].status = StatusCode::STATUS_ERROR_MEMORY_READ;
            ++processed;
            break;
        }

        if (processed == 0)
        {
            results[offset].status = StatusCode::STATUS_ERROR_MEMORY_READ;
            processed = 1;
        }

        offset += processed;
    }

    return StatusCode::STATUS_OK;
}

extern "C" VERTEX_EXPORT StatusCode VERTEX_API vertex_memory_get_bulk_request_limit(std::uint32_t* maxRequestCount)
{
    if (maxRequestCount == nullptr)
    {
        return StatusCode::STATUS_ERROR_INVALID_PARAMETER;
    }

    *maxRequestCount = static_cast<std::uint32_t>(get_iovec_limit());
    return StatusCode::STATUS_OK;
}
