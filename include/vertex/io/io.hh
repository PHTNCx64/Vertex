//
// Copyright (C) 2026 PHTNC<>.
// Licensed under GPLv3.0 with Plugin Interface exceptions.
//
#pragma once

#include <sdk/statuscode.h>
#include <vertex/io/iio.hh>

#include <filesystem>
#include <shared_mutex>
#include <mutex>
#include <list>

namespace Vertex::IO
{
    class IO final : public IIO
    {
    public:
        StatusCode create_temp_sparse_file(const std::filesystem::path& path, std::uint64_t size) override;
        StatusCode delete_temp_sparse_file(const std::filesystem::path& path) override;
        StatusCode delete_temp_sparse_files() override;
        StatusCode trim_sparse_file(File& file) override;
        StatusCode sync_mapped_region(const File& file, std::size_t offset, std::uint64_t size) override;
        StatusCode map_file(File& file) override;
        StatusCode unmap_file(File& file) override;
        StatusCode resize_file_map(File& file, std::size_t newSize) override;
        StatusCode resize_file_map_unlocked(File& file, std::size_t newSize) override;
        StatusCode set_storage_path(const std::filesystem::path& path) override;
        StatusCode write_at_offset(const File& file, std::size_t offset, const void* data, std::size_t dataSize) override;
        StatusCode read_at_offset(const File& file, std::size_t offset, void* buffer, std::size_t bufferSize) override;
        StatusCode resize_file(File& file, std::size_t newSize) override;
        std::optional<std::reference_wrapper<File>> get_file(const std::filesystem::path& path) override;

    private:
        std::list<File> m_mappedHandles {};
        std::filesystem::path m_storePath {};

        mutable std::shared_mutex m_handlesMutex {};
        mutable std::shared_mutex m_pathMutex {};
    };
}
