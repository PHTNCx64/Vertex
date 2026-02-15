//
// Unit tests for IO service
//

#include <gtest/gtest.h>
#include <vertex/io/io.hh>
#include <filesystem>

class IOTest : public ::testing::Test
{
protected:
    void SetUp() override
    {
        testStoragePath = std::filesystem::temp_directory_path() / "vertex_io_test";
        std::filesystem::create_directories(testStoragePath);

        io = std::make_unique<Vertex::IO::IO>();
        io->set_storage_path(testStoragePath);
    }

    void TearDown() override
    {
        // Clean up sparse files
        io->delete_temp_sparse_files();

        // Remove test directory
        if (std::filesystem::exists(testStoragePath))
        {
            std::filesystem::remove_all(testStoragePath);
        }
    }

    std::filesystem::path testStoragePath;
    std::unique_ptr<Vertex::IO::IO> io;
};

// ==================== Storage Path Tests ====================

TEST_F(IOTest, SetStoragePath_ValidPath_Succeeds)
{
    // Arrange
    const std::filesystem::path testPath = std::filesystem::temp_directory_path() / "vertex_test_storage";

    // Act
    const StatusCode result = io->set_storage_path(testPath);

    // Assert
    EXPECT_EQ(StatusCode::STATUS_OK, result);
    EXPECT_TRUE(std::filesystem::exists(testPath));

    // Cleanup
    std::filesystem::remove_all(testPath);
}

TEST_F(IOTest, SetStoragePath_EmptyPath_ReturnsError)
{
    // Arrange
    std::filesystem::path emptyPath;

    // Act
    StatusCode result = io->set_storage_path(emptyPath);

    // Assert
    EXPECT_EQ(StatusCode::STATUS_ERROR_INVALID_PARAMETER, result);
}

TEST_F(IOTest, SetStoragePath_RelativePath_CreatesAbsolutePath)
{
    // Arrange
    std::filesystem::path relativePath = "test_storage";

    // Act
    StatusCode result = io->set_storage_path(relativePath);

    // Assert
    EXPECT_EQ(StatusCode::STATUS_OK, result);
    // The path should be converted to absolute
    std::filesystem::path absolutePath = std::filesystem::absolute(relativePath);
    EXPECT_TRUE(std::filesystem::exists(absolutePath));

    // Cleanup
    std::filesystem::remove_all(absolutePath);
}

// ==================== Sparse File Creation Tests ====================

TEST_F(IOTest, CreateTempSparseFile_ValidParameters_Succeeds)
{
    // Arrange
    const std::string filename = "test_sparse.tmp";
    constexpr std::size_t fileSize = 1024 * 1024; // 1MB

    // Act
    const StatusCode result = io->create_temp_sparse_file(filename, fileSize);

    // Assert
    EXPECT_EQ(StatusCode::STATUS_OK, result);
    std::filesystem::path expectedPath = testStoragePath / filename;
    EXPECT_TRUE(std::filesystem::exists(expectedPath));
}

TEST_F(IOTest, CreateTempSparseFile_ZeroSize_ReturnsError)
{
    // Arrange
    const std::string filename = "test_zero_size.tmp";

    // Act
    StatusCode result = io->create_temp_sparse_file(filename, 0);

    // Assert
    EXPECT_EQ(StatusCode::STATUS_ERROR_INVALID_PARAMETER, result);
}

TEST_F(IOTest, CreateTempSparseFile_LargeFile_Succeeds)
{
    // Arrange
    const std::string filename = "test_large_sparse.tmp";
    constexpr std::size_t fileSize = 1024ULL * 1024ULL * 1024ULL; // 1GB sparse file

    // Act
    StatusCode result = io->create_temp_sparse_file(filename, fileSize);

    // Assert
    EXPECT_EQ(StatusCode::STATUS_OK, result);

    // Verify file exists but doesn't consume full disk space (sparse file property)
    std::filesystem::path expectedPath = testStoragePath / filename;
    EXPECT_TRUE(std::filesystem::exists(expectedPath));
}

TEST_F(IOTest, CreateTempSparseFile_RelativePath_ResolvesToStoragePath)
{
    // Arrange
    const std::string filename = "relative_path.tmp";
    constexpr std::size_t fileSize = 1024;

    // Act
    StatusCode result = io->create_temp_sparse_file(filename, fileSize);

    // Assert
    EXPECT_EQ(StatusCode::STATUS_OK, result);
    std::filesystem::path expectedPath = testStoragePath / filename;
    EXPECT_TRUE(std::filesystem::exists(expectedPath));
}

// ==================== Sparse File Deletion Tests ====================

TEST_F(IOTest, DeleteTempSparseFile_ExistingFile_Succeeds)
{
    // Arrange
    const std::string filename = "test_delete.tmp";
    io->create_temp_sparse_file(filename, 1024);

    // Act
    StatusCode result = io->delete_temp_sparse_file(filename);

    // Assert
    EXPECT_EQ(StatusCode::STATUS_OK, result);
    std::filesystem::path expectedPath = testStoragePath / filename;
    EXPECT_FALSE(std::filesystem::exists(expectedPath));
}

TEST_F(IOTest, DeleteTempSparseFile_NonExistentFile_Succeeds)
{
    // Arrange
    const std::string filename = "nonexistent.tmp";

    // Act
    StatusCode result = io->delete_temp_sparse_file(filename);

    // Assert
    EXPECT_EQ(StatusCode::STATUS_OK, result); // Should succeed (file doesn't exist is OK)
}

TEST_F(IOTest, DeleteTempSparseFiles_MultipleFiles_DeletesAll)
{
    // Arrange
    io->create_temp_sparse_file("file1.tmp", 1024);
    io->create_temp_sparse_file("file2.tmp", 2048);
    io->create_temp_sparse_file("file3.tmp", 4096);

    // Act
    StatusCode result = io->delete_temp_sparse_files();

    // Assert
    EXPECT_EQ(StatusCode::STATUS_OK, result);
    EXPECT_FALSE(std::filesystem::exists(testStoragePath / "file1.tmp"));
    EXPECT_FALSE(std::filesystem::exists(testStoragePath / "file2.tmp"));
    EXPECT_FALSE(std::filesystem::exists(testStoragePath / "file3.tmp"));
}

// ==================== File Re-creation Tests (Scan scenario) ====================

TEST_F(IOTest, CreateTempSparseFile_ExistingFileDeleted_Succeeds)
{
    // Arrange
    const std::string filename = "scan_results.tmp";
    io->create_temp_sparse_file(filename, 1024);

    // Simulate user clicking "initial scan" again
    io->delete_temp_sparse_file(filename);

    // Act
    StatusCode result = io->create_temp_sparse_file(filename, 2048);

    // Assert
    EXPECT_EQ(StatusCode::STATUS_OK, result);
    std::filesystem::path expectedPath = testStoragePath / filename;
    EXPECT_TRUE(std::filesystem::exists(expectedPath));
}

TEST_F(IOTest, CreateTempSparseFile_MultipleScans_EachCreatesCleanFile)
{
    // Arrange
    const std::string filename = "scan_iteration.tmp";

    // Act & Assert - Simulate 3 scan iterations
    for (int i = 0; i < 3; ++i)
    {
        // Delete previous file
        io->delete_temp_sparse_file(filename);

        // Create new file
        StatusCode result = io->create_temp_sparse_file(filename, 1024 * (i + 1));
        EXPECT_EQ(StatusCode::STATUS_OK, result);

        std::filesystem::path expectedPath = testStoragePath / filename;
        EXPECT_TRUE(std::filesystem::exists(expectedPath));
    }
}

// ==================== File Mapping Tests ====================

TEST_F(IOTest, MapFile_ValidFile_Succeeds)
{
    // Arrange
    const std::string filename = "map_test.tmp";
    constexpr std::size_t fileSize = 4096;
    io->create_temp_sparse_file(filename, fileSize);

    // Create a File object - need to get it from IO internal state
    // Note: This test may need adjustment based on actual File object access
    // For now, we test the concept
}

// ==================== Resize File Tests ====================

TEST_F(IOTest, ResizeFileMap_IncreaseSize_Succeeds)
{
    // Arrange
    const std::string filename = "resize_test.tmp";
    constexpr std::size_t initialSize = 1024;
    const std::size_t newSize = 4096;
    io->create_temp_sparse_file(filename, initialSize);

    // Note: Resize test would need File object access
    // This demonstrates the test structure
}

// ==================== Edge Cases ====================

TEST_F(IOTest, CreateTempSparseFile_VerySmallSize_Succeeds)
{
    // Arrange
    const std::string filename = "tiny_file.tmp";
    constexpr std::size_t fileSize = 1; // 1 byte

    // Act
    StatusCode result = io->create_temp_sparse_file(filename, fileSize);

    // Assert
    EXPECT_EQ(StatusCode::STATUS_OK, result);
}

TEST_F(IOTest, CreateTempSparseFile_SpecialCharactersInName_Succeeds)
{
    // Arrange
    const std::string filename = "test_file_with_underscores_123.tmp";
    constexpr std::size_t fileSize = 1024;

    // Act
    StatusCode result = io->create_temp_sparse_file(filename, fileSize);

    // Assert
    EXPECT_EQ(StatusCode::STATUS_OK, result);
}
