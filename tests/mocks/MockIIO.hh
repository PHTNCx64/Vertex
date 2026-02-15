//
// Mock for IIO interface
//

#pragma once

  #include <gmock/gmock.h>
  #include <vertex/io/iio.hh>

  namespace Vertex::Testing::Mocks
  {
      class MockIIO : public IO::IIO
      {
      public:
          ~MockIIO() override = default;

          MOCK_METHOD(StatusCode, create_temp_sparse_file, (const std::filesystem::path& path, std::uint64_t sizeInBytes), (override));
          MOCK_METHOD(StatusCode, delete_temp_sparse_file, (const std::filesystem::path& path), (override));
          MOCK_METHOD(StatusCode, delete_temp_sparse_files, (), (override));
          MOCK_METHOD(StatusCode, trim_sparse_file, (Vertex::IO::File& file), (override));
          MOCK_METHOD(StatusCode, sync_mapped_region, (const Vertex::IO::File& file, std::size_t offset, std::uint64_t size), (override));
          MOCK_METHOD(StatusCode, map_file, (Vertex::IO::File& file), (override));
          MOCK_METHOD(StatusCode, unmap_file, (Vertex::IO::File& file), (override));
          MOCK_METHOD(StatusCode, resize_file_map, (Vertex::IO::File& file, std::size_t newSize), (override));
          MOCK_METHOD(StatusCode, resize_file_map_unlocked, (Vertex::IO::File& file, std::size_t newSize), (override));
          MOCK_METHOD(StatusCode, set_storage_path, (const std::filesystem::path& path), (override));
          MOCK_METHOD(std::optional<std::reference_wrapper<Vertex::IO::File>>, get_file, (const std::filesystem::path& path), (override));
          MOCK_METHOD(StatusCode, write_at_offset, (const Vertex::IO::File& file, std::size_t offset, const void* data, std::size_t dataSize), (override));
          MOCK_METHOD(StatusCode, read_at_offset, (const Vertex::IO::File& file, std::size_t offset, void* buffer, std::size_t bufferSize), (override));
          MOCK_METHOD(StatusCode, resize_file, (Vertex::IO::File& file, std::size_t newSize), (override));
      };
  } // namespace Vertex::Testing::Mocks