//
// Copyright (C) 2026 PHTNC<>.
// Licensed under GPLv3.0 with Plugin Interface exceptions.
//
#pragma once

#include <sdk/statuscode.h>
#include <vertex/io/file.hh>

#include <filesystem>
#include <optional>
#include <functional>

namespace Vertex::IO
{
    class IIO
    {
    public:
        virtual ~IIO() = default;
        virtual StatusCode create_temp_sparse_file(const std::filesystem::path& path, std::uint64_t sizeInBytes) = 0;
        virtual StatusCode delete_temp_sparse_file(const std::filesystem::path& path) = 0;
        virtual StatusCode delete_temp_sparse_files() = 0;
        virtual StatusCode trim_sparse_file(File& file) = 0;
        virtual StatusCode sync_mapped_region(const File& file, std::size_t offset, std::uint64_t size) = 0;
        virtual StatusCode map_file(File& file) = 0;
        virtual StatusCode unmap_file(File& file) = 0;
        virtual StatusCode resize_file_map(File& file, std::size_t newSize) = 0;
        virtual StatusCode resize_file_map_unlocked(File& file, std::size_t newSize) = 0;
        virtual StatusCode set_storage_path(const std::filesystem::path& path) = 0;
        virtual std::optional<std::reference_wrapper<File>> get_file(const std::filesystem::path& path) = 0;
        virtual StatusCode write_at_offset(const File& file, std::size_t offset, const void* data, std::size_t dataSize) = 0;
        virtual StatusCode read_at_offset(const File& file, std::size_t offset, void* buffer, std::size_t bufferSize) = 0;
        virtual StatusCode resize_file(File& file, std::size_t newSize) = 0;
    };
}
