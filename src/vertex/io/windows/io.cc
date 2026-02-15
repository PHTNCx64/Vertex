//
// Copyright (C) 2026 PHTNC<>.
// Licensed under GPLv3.0 with Plugin Interface exceptions.
//
#include <vertex/safehandle.hh>
#include <vertex/io/io.hh>

#include <Windows.h>
#include <winioctl.h>
#include <algorithm>

namespace Vertex::IO
{
    // NOTE: This was in past versions of the memory scanner
    // It was quite fast but it was heavily limited by the fact that a fixed size had to be set prior to a scan start and resizing tanked performance heavily since
    // we had to lock like crazy.

    // and sparse files can heavily fragment which is okayish for SSDs and especially NVMEs, but HDDs... not a good combo.
    // new scanner uses virtual region in virtualregion.cc

    // this might be useful in the future for other cases, so i'll keep it for now.

    StatusCode IO::create_temp_sparse_file(const std::filesystem::path& path, const std::size_t size)
    {
        if (!size)
        {
            return StatusCode::STATUS_ERROR_INVALID_PARAMETER;
        }

        std::filesystem::path tmpPath(path);

        if (tmpPath.is_relative())
        {
            std::shared_lock pathLock(m_pathMutex);
            tmpPath = m_storePath / tmpPath;
        }

        SafeHandle fileHandle = CreateFile(tmpPath.c_str(), GENERIC_READ | GENERIC_WRITE, 0, nullptr, CREATE_ALWAYS,
                                           FILE_ATTRIBUTE_TEMPORARY | FILE_FLAG_SEQUENTIAL_SCAN | FILE_FLAG_DELETE_ON_CLOSE | FILE_ATTRIBUTE_SPARSE_FILE, nullptr);

        if (!fileHandle.is_valid())
        {
            return StatusCode::STATUS_ERROR_FILE_CREATION_FAILED;
        }

        DWORD bytesReturned{};
        if (!DeviceIoControl(fileHandle, FSCTL_SET_SPARSE, nullptr, 0, nullptr, 0, &bytesReturned, nullptr))
        {
            return StatusCode::STATUS_ERROR_FILE_CONFIGURATION_INVALID;
        }

        LARGE_INTEGER fileSize{};
        fileSize.QuadPart = static_cast<LONGLONG>(size);

        if (!SetFilePointerEx(fileHandle, fileSize, nullptr, FILE_BEGIN))
        {
            return StatusCode::STATUS_ERROR_FILE_CONFIGURATION_INVALID;
        }

        if (!SetEndOfFile(fileHandle))
        {
            return StatusCode::STATUS_ERROR_FILE_CONFIGURATION_INVALID;
        }

        File newFile{};
        newFile.set_file_map(fileHandle);
        newFile.set_mapped_addr(0);
        newFile.set_path(tmpPath);
        newFile.set_size(size);

        std::scoped_lock handleLock(m_handlesMutex);
        m_mappedHandles.emplace_back(std::move(newFile));

        File& finalFile = m_mappedHandles.back();
        finalFile.m_cleanUpFunc = [&finalFile]() noexcept
        {
            const std::uintptr_t addr = finalFile.get_mapped_addr();
            if (addr)
            {
                UnmapViewOfFile(reinterpret_cast<void*>(addr));
                finalFile.set_mapped_addr(0);
            }

            finalFile.set_mapping_handle(SafeHandle{});
        };

        return StatusCode::STATUS_OK;
    }

    StatusCode IO::write_at_offset(const File& file, const std::size_t offset, const void* data, const std::size_t dataSize)
    {
        if (!file.get_file_handle().is_valid())
        {
            return StatusCode::STATUS_ERROR_INVALID_PARAMETER;
        }

        const std::size_t fileSize = file.get_size();
        if (offset > fileSize || dataSize > fileSize - offset)
        {
            return StatusCode::STATUS_ERROR_INVALID_PARAMETER;
        }

        if (dataSize > std::numeric_limits<DWORD>::max())
        {
            return StatusCode::STATUS_ERROR_INVALID_PARAMETER;
        }

        const HANDLE rawHandle = file.get_file_handle();

        OVERLAPPED overlapped{};
        overlapped.Offset = static_cast<DWORD>(offset & 0xFFFFFFFF);
        overlapped.OffsetHigh = static_cast<DWORD>(offset >> 32);

        DWORD bytesWritten{};
        if (!WriteFile(rawHandle, data, static_cast<DWORD>(dataSize), &bytesWritten, &overlapped))
        {
            return StatusCode::STATUS_ERROR_FILE_WRITE_FAILED;
        }

        if (bytesWritten != dataSize)
        {
            return StatusCode::STATUS_ERROR_FILE_WRITE_FAILED;
        }

        return StatusCode::STATUS_OK;
    }

    StatusCode IO::read_at_offset(const File& file, const std::size_t offset, void* buffer, const std::size_t bufferSize)
    {
        if (!file.get_file_handle().is_valid())
        {
            return StatusCode::STATUS_ERROR_INVALID_PARAMETER;
        }

        const HANDLE rawHandle = file.get_file_handle();

        OVERLAPPED overlapped{};
        overlapped.Offset = static_cast<DWORD>(offset & 0xFFFFFFFF);
        overlapped.OffsetHigh = static_cast<DWORD>(offset >> 32);

        DWORD bytesRead{};
        if (!ReadFile(rawHandle, buffer, static_cast<DWORD>(bufferSize), &bytesRead, &overlapped))
        {
            return StatusCode::STATUS_ERROR_FILE_READ_FAILED;
        }

        if (bytesRead != bufferSize)
        {
            return StatusCode::STATUS_ERROR_FILE_READ_FAILED;
        }

        return StatusCode::STATUS_OK;
    }

    StatusCode IO::resize_file(File& file, const std::size_t newSize)
    {
        if (!file.get_file_handle().is_valid() || !newSize)
        {
            return StatusCode::STATUS_ERROR_INVALID_PARAMETER;
        }

        LARGE_INTEGER fileSize{};
        fileSize.QuadPart = static_cast<LONGLONG>(newSize);

        const HANDLE rawHandle = file.get_file_handle();

        if (!SetFilePointerEx(rawHandle, fileSize, nullptr, FILE_BEGIN))
        {
            return StatusCode::STATUS_ERROR_FILE_RESIZE_FAILED;
        }

        if (!SetEndOfFile(rawHandle))
        {
            return StatusCode::STATUS_ERROR_FILE_RESIZE_FAILED;
        }

        file.set_size(newSize);
        return StatusCode::STATUS_OK;
    }

    StatusCode IO::map_file(File& file)
    {
        std::scoped_lock<std::shared_mutex> fileLock(file.get_shared_mutex());

        if (!file.get_file_handle().is_valid())
        {
            return StatusCode::STATUS_ERROR_INVALID_PARAMETER;
        }

        const std::size_t fileSize = file.get_size();
        if (!fileSize)
        {
            return StatusCode::STATUS_ERROR_INVALID_PARAMETER;
        }

        SafeHandle mappingHandle = CreateFileMapping(file.get_file_handle(), nullptr, PAGE_READWRITE | SEC_RESERVE, static_cast<DWORD>(fileSize >> 32), static_cast<DWORD>(fileSize & 0xFFFFFFFF), nullptr);

        if (!mappingHandle.is_valid())
        {
            return StatusCode::STATUS_ERROR_FILE_MAPPING_FAILED;
        }

        void* mappedAddr = MapViewOfFile(mappingHandle, FILE_MAP_ALL_ACCESS, 0, 0, fileSize);

        if (!mappedAddr)
        {
            return StatusCode::STATUS_ERROR_FILE_MAPPING_FAILED;
        }

        file.set_mapping_handle(mappingHandle);
        file.set_mapped_addr(reinterpret_cast<std::uintptr_t>(mappedAddr));

        return StatusCode::STATUS_OK;
    }

    StatusCode IO::unmap_file(File& file)
    {
        if (!file.is_valid())
        {
            return StatusCode::STATUS_ERROR_INVALID_PARAMETER;
        }

        if (file.get_mapped_addr())
        {
            if (!UnmapViewOfFile(reinterpret_cast<void*>(file.get_mapped_addr())))
            {
                return StatusCode::STATUS_ERROR_FILE_UNMAPPING_FAILED;
            }
            file.set_mapped_addr(0);
        }

        file.close();

        return StatusCode::STATUS_OK;
    }

    StatusCode IO::resize_file_map(File& file, const std::uint64_t newSize)
    {
        std::scoped_lock<std::shared_mutex> fileLock(file.get_shared_mutex());
        return resize_file_map_unlocked(file, newSize);
    }

    StatusCode IO::resize_file_map_unlocked(File& file, const std::uint64_t newSize)
    {
        if (!file.get_file_handle().is_valid())
        {
            return StatusCode::STATUS_ERROR_INVALID_PARAMETER;
        }

        if (!newSize)
        {
            return StatusCode::STATUS_ERROR_INVALID_PARAMETER;
        }

        const std::size_t currentSize = file.get_size();
        if (newSize <= currentSize)
        {
            const double usageRatio = file.get_usage_ratio();
            constexpr double RESIZE_THRESHOLD = 0.80;
            if (usageRatio < RESIZE_THRESHOLD)
            {
                return StatusCode::STATUS_ERROR_FILE_MAP_RESIZE_NOT_REQUIRED;
            }
        }

        const bool wasMapped = file.is_valid();

        if (wasMapped)
        {
            const std::uintptr_t mappedAddr = file.get_mapped_addr();

            if (mappedAddr != 0)
            {
                if (!UnmapViewOfFile(reinterpret_cast<void*>(mappedAddr)))
                {
                    return StatusCode::STATUS_ERROR_FILE_UNMAPPING_FAILED;
                }
            }

            file.set_mapped_addr(0);
            file.set_mapping_handle(SafeHandle{});

            file.m_cleanUpFunc = nullptr;
        }

        if (newSize > static_cast<std::uint64_t>(std::numeric_limits<LONGLONG>::max()))
        {
            return StatusCode::STATUS_ERROR_INVALID_PARAMETER;
        }

        LARGE_INTEGER liNewSize{};
        liNewSize.QuadPart = static_cast<LONGLONG>(newSize);
        LARGE_INTEGER resultingPointer{};
        const HANDLE rawHandle = file.get_file_handle().get();

        if (!SetFilePointerEx(rawHandle, liNewSize, &resultingPointer, FILE_BEGIN))
        {
            return StatusCode::STATUS_ERROR_FILE_RESIZE_FAILED;
        }

        if (resultingPointer.QuadPart != liNewSize.QuadPart)
        {
            return StatusCode::STATUS_ERROR_FILE_RESIZE_FAILED;
        }

        if (!SetEndOfFile(rawHandle))
        {
            return StatusCode::STATUS_ERROR_FILE_RESIZE_FAILED;
        }

        file.set_size(static_cast<std::size_t>(newSize));

        if (wasMapped)
        {
            const std::size_t mapFileSize = file.get_size();
            if (!mapFileSize)
            {
                return StatusCode::STATUS_ERROR_INVALID_PARAMETER;
            }

            const DWORD maxSizeHigh = static_cast<DWORD>((static_cast<unsigned long long>(mapFileSize) >> 32) & 0xFFFFFFFFu);
            const DWORD maxSizeLow = static_cast<DWORD>(static_cast<unsigned long long>(mapFileSize) & 0xFFFFFFFFu);

            SafeHandle mappingHandle = CreateFileMapping(file.get_file_handle(), nullptr, PAGE_READWRITE | SEC_RESERVE, maxSizeHigh, maxSizeLow, nullptr);

            if (!mappingHandle.is_valid())
            {
                return StatusCode::STATUS_ERROR_FILE_MAPPING_FAILED;
            }

            void* mappedAddr = MapViewOfFile(mappingHandle, FILE_MAP_ALL_ACCESS, 0, 0, static_cast<SIZE_T>(mapFileSize));

            if (!mappedAddr)
            {
                return StatusCode::STATUS_ERROR_FILE_MAPPING_FAILED;
            }

            file.set_mapping_handle(std::move(mappingHandle));
            file.set_mapped_addr(reinterpret_cast<std::uintptr_t>(mappedAddr));

            file.m_cleanUpFunc = [&file]() noexcept
            {
                const std::uintptr_t addr = file.get_mapped_addr();
                if (addr)
                {
                    UnmapViewOfFile(reinterpret_cast<void*>(addr));
                    file.set_mapped_addr(0);
                }

                file.set_mapping_handle(SafeHandle{});
            };
        }

        return StatusCode::STATUS_OK;
    }

    StatusCode IO::delete_temp_sparse_file(const std::filesystem::path& path)
    {
        std::filesystem::path tmpPath(path);

        if (tmpPath.is_relative())
        {
            std::shared_lock pathLock(m_pathMutex);
            tmpPath = m_storePath / tmpPath;
        }

        std::scoped_lock handleLock(m_handlesMutex);

        const auto it = std::ranges::find_if(m_mappedHandles,
                                             [&tmpPath](const File& file)
                                             {
                                                 return file.get_path() == tmpPath;
                                             });

        if (it != m_mappedHandles.end())
        {
            {
                std::scoped_lock<std::shared_mutex> fileLock(it->get_shared_mutex());

                if (it->is_valid())
                {
                    File& fileRef = *it;
                    if (fileRef.get_mapped_addr())
                    {
                        if (!UnmapViewOfFile(reinterpret_cast<void*>(fileRef.get_mapped_addr())))
                        {
                            return StatusCode::STATUS_ERROR_FILE_UNMAPPING_FAILED;
                        }
                        fileRef.set_mapped_addr(0);
                    }
                }
            }

            m_mappedHandles.erase(it);
        }

        if (std::filesystem::exists(tmpPath))
        {
            std::error_code ec{};
            if (!std::filesystem::remove(tmpPath, ec))
            {
                return StatusCode::STATUS_ERROR_GENERAL;
            }
        }

        return StatusCode::STATUS_OK;
    }

    StatusCode IO::delete_temp_sparse_files()
    {
        std::list<File> filesToDelete{};
        {
            std::scoped_lock handleLock(m_handlesMutex);
            filesToDelete = std::move(m_mappedHandles);
        }

        for (auto& file : filesToDelete)
        {
            std::scoped_lock<std::shared_mutex> fileLock(file.get_shared_mutex());
            if (file.is_valid() && file.get_mapped_addr())
            {
                UnmapViewOfFile(reinterpret_cast<void*>(file.get_mapped_addr()));
                file.set_mapped_addr(0);
            }
        }

        filesToDelete.clear();

        return StatusCode::STATUS_OK;
    }

    StatusCode IO::trim_sparse_file(File& file)
    {
        std::scoped_lock<std::shared_mutex> fileLock(file.get_shared_mutex());

        if (!file.get_file_handle().is_valid())
        {
            return StatusCode::STATUS_ERROR_INVALID_PARAMETER;
        }

        const std::size_t usedBytes = file.get_used_bytes();
        const std::size_t totalSize = file.get_size();

        if (usedBytes >= totalSize)
        {
            return StatusCode::STATUS_OK;
        }

        const bool wasMapped = file.is_mapped();
        if (wasMapped)
        {
            if (!UnmapViewOfFile(reinterpret_cast<void*>(file.get_mapped_addr())))
            {
                return StatusCode::STATUS_ERROR_FILE_UNMAPPING_FAILED;
            }
            file.set_mapped_addr(0);

            file.set_mapping_handle(SafeHandle{});
        }

        LARGE_INTEGER newSize{};
        newSize.QuadPart = static_cast<LONGLONG>(usedBytes);

        if (!SetFilePointerEx(file.get_file_handle(), newSize, nullptr, FILE_BEGIN))
        {
            return StatusCode::STATUS_ERROR_FILE_TRIM_FAILED;
        }

        if (!SetEndOfFile(file.get_file_handle()))
        {
            return StatusCode::STATUS_ERROR_FILE_TRIM_FAILED;
        }

        file.set_size(usedBytes);

        if (wasMapped)
        {
            const HANDLE mappingHandle = CreateFileMappingW(
                file.get_file_handle(),
                nullptr,
                PAGE_READWRITE,
                0,
                0,
                nullptr
            );

            if (!mappingHandle || mappingHandle == INVALID_HANDLE_VALUE)
            {
                return StatusCode::STATUS_ERROR_FILE_MAPPING_FAILED;
            }

            file.set_mapping_handle(SafeHandle{mappingHandle});

            void* mappedAddr = MapViewOfFile(mappingHandle, FILE_MAP_ALL_ACCESS, 0, 0, 0);
            if (!mappedAddr)
            {
                return StatusCode::STATUS_ERROR_FILE_MAPPING_FAILED;
            }

            file.set_mapped_addr(reinterpret_cast<std::uintptr_t>(mappedAddr));
        }

        return StatusCode::STATUS_OK;
    }

    StatusCode IO::sync_mapped_region(const File& file, const std::size_t offset, const std::uint64_t size)
    {
        std::scoped_lock<std::shared_mutex> fileLock(file.get_shared_mutex());

        if (!file.is_valid())
        {
            return StatusCode::STATUS_ERROR_INVALID_PARAMETER;
        }

        if (offset + size > file.get_size())
        {
            return StatusCode::STATUS_ERROR_INVALID_PARAMETER;
        }

        const auto regionAddr = reinterpret_cast<void*>(file.get_mapped_addr() + offset);

        if (!FlushViewOfFile(regionAddr, size))
        {
            return StatusCode::STATUS_ERROR_FILE_SYNC_FAILED;
        }

        if (!FlushFileBuffers(file.get_file_handle()))
        {
            return StatusCode::STATUS_ERROR_FILE_SYNC_FAILED;
        }

        return StatusCode::STATUS_OK;
    }

    StatusCode IO::set_storage_path(const std::filesystem::path& path)
    {
        if (path.empty())
        {
            return StatusCode::STATUS_ERROR_INVALID_PARAMETER;
        }

        std::filesystem::path absolutePath = path;
        if (absolutePath.is_relative())
        {
            absolutePath = std::filesystem::absolute(absolutePath);
        }

        if (!std::filesystem::exists(absolutePath))
        {
            std::error_code ec{};
            if (!std::filesystem::create_directories(absolutePath, ec))
            {
                return StatusCode::STATUS_ERROR_DIRECTORY_CREATION_FAILED;
            }
        }

        std::scoped_lock pathLock(m_pathMutex);
        m_storePath = absolutePath;

        return StatusCode::STATUS_OK;
    }

    std::optional<std::reference_wrapper<File>> IO::get_file(const std::filesystem::path& path)
    {
        std::filesystem::path tmpPath(path);

        if (tmpPath.is_relative())
        {
            std::shared_lock pathLock(m_pathMutex);
            tmpPath = m_storePath / tmpPath;
        }

        std::shared_lock handleLock(m_handlesMutex);

        const auto it = std::ranges::find_if(m_mappedHandles,
                                             [&tmpPath](const File& file)
                                             {
                                                 return file.get_path() == tmpPath;
                                             });

        if (it != m_mappedHandles.end())
        {
            return std::ref(*it);
        }

        return std::nullopt;
    }
}
