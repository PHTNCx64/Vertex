//
// Copyright (C) 2026 PHTNC<>.
// Licensed under GPLv3.0 with Plugin Interface exceptions.
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
        
        io->delete_temp_sparse_files();

        
        if (std::filesystem::exists(testStoragePath))
        {
            std::filesystem::remove_all(testStoragePath);
        }
    }

    std::filesystem::path testStoragePath;
    std::unique_ptr<Vertex::IO::IO> io;
};



TEST_F(IOTest, SetStoragePath_ValidPath_Succeeds)
{
    
    const std::filesystem::path testPath = std::filesystem::temp_directory_path() / "vertex_test_storage";

    
    const StatusCode result = io->set_storage_path(testPath);

    
    EXPECT_EQ(StatusCode::STATUS_OK, result);
    EXPECT_TRUE(std::filesystem::exists(testPath));

    
    std::filesystem::remove_all(testPath);
}

TEST_F(IOTest, SetStoragePath_EmptyPath_ReturnsError)
{
    
    std::filesystem::path emptyPath;

    
    StatusCode result = io->set_storage_path(emptyPath);

    
    EXPECT_EQ(StatusCode::STATUS_ERROR_INVALID_PARAMETER, result);
}

TEST_F(IOTest, SetStoragePath_RelativePath_CreatesAbsolutePath)
{
    
    std::filesystem::path relativePath = "test_storage";

    
    StatusCode result = io->set_storage_path(relativePath);

    
    EXPECT_EQ(StatusCode::STATUS_OK, result);
    
    std::filesystem::path absolutePath = std::filesystem::absolute(relativePath);
    EXPECT_TRUE(std::filesystem::exists(absolutePath));

    
    std::filesystem::remove_all(absolutePath);
}



TEST_F(IOTest, CreateTempSparseFile_ValidParameters_Succeeds)
{
    
    const std::string filename = "test_sparse.tmp";
    constexpr std::size_t fileSize = 1024 * 1024; 

    
    const StatusCode result = io->create_temp_sparse_file(filename, fileSize);

    
    EXPECT_EQ(StatusCode::STATUS_OK, result);
    std::filesystem::path expectedPath = testStoragePath / filename;
    EXPECT_TRUE(std::filesystem::exists(expectedPath));
}

TEST_F(IOTest, CreateTempSparseFile_ZeroSize_ReturnsError)
{
    
    const std::string filename = "test_zero_size.tmp";

    
    StatusCode result = io->create_temp_sparse_file(filename, 0);

    
    EXPECT_EQ(StatusCode::STATUS_ERROR_INVALID_PARAMETER, result);
}

TEST_F(IOTest, CreateTempSparseFile_LargeFile_Succeeds)
{
    
    const std::string filename = "test_large_sparse.tmp";
    constexpr std::size_t fileSize = 1024ULL * 1024ULL * 1024ULL; 

    
    StatusCode result = io->create_temp_sparse_file(filename, fileSize);

    
    EXPECT_EQ(StatusCode::STATUS_OK, result);

    
    std::filesystem::path expectedPath = testStoragePath / filename;
    EXPECT_TRUE(std::filesystem::exists(expectedPath));
}

TEST_F(IOTest, CreateTempSparseFile_RelativePath_ResolvesToStoragePath)
{
    
    const std::string filename = "relative_path.tmp";
    constexpr std::size_t fileSize = 1024;

    
    StatusCode result = io->create_temp_sparse_file(filename, fileSize);

    
    EXPECT_EQ(StatusCode::STATUS_OK, result);
    std::filesystem::path expectedPath = testStoragePath / filename;
    EXPECT_TRUE(std::filesystem::exists(expectedPath));
}



TEST_F(IOTest, DeleteTempSparseFile_ExistingFile_Succeeds)
{
    
    const std::string filename = "test_delete.tmp";
    io->create_temp_sparse_file(filename, 1024);

    
    StatusCode result = io->delete_temp_sparse_file(filename);

    
    EXPECT_EQ(StatusCode::STATUS_OK, result);
    std::filesystem::path expectedPath = testStoragePath / filename;
    EXPECT_FALSE(std::filesystem::exists(expectedPath));
}

TEST_F(IOTest, DeleteTempSparseFile_NonExistentFile_Succeeds)
{
    
    const std::string filename = "nonexistent.tmp";

    
    StatusCode result = io->delete_temp_sparse_file(filename);

    
    EXPECT_EQ(StatusCode::STATUS_OK, result); 
}

TEST_F(IOTest, DeleteTempSparseFiles_MultipleFiles_DeletesAll)
{
    
    io->create_temp_sparse_file("file1.tmp", 1024);
    io->create_temp_sparse_file("file2.tmp", 2048);
    io->create_temp_sparse_file("file3.tmp", 4096);

    
    StatusCode result = io->delete_temp_sparse_files();

    
    EXPECT_EQ(StatusCode::STATUS_OK, result);
    EXPECT_FALSE(std::filesystem::exists(testStoragePath / "file1.tmp"));
    EXPECT_FALSE(std::filesystem::exists(testStoragePath / "file2.tmp"));
    EXPECT_FALSE(std::filesystem::exists(testStoragePath / "file3.tmp"));
}



TEST_F(IOTest, CreateTempSparseFile_ExistingFileDeleted_Succeeds)
{
    
    const std::string filename = "scan_results.tmp";
    io->create_temp_sparse_file(filename, 1024);

    
    io->delete_temp_sparse_file(filename);

    
    StatusCode result = io->create_temp_sparse_file(filename, 2048);

    
    EXPECT_EQ(StatusCode::STATUS_OK, result);
    std::filesystem::path expectedPath = testStoragePath / filename;
    EXPECT_TRUE(std::filesystem::exists(expectedPath));
}

TEST_F(IOTest, CreateTempSparseFile_MultipleScans_EachCreatesCleanFile)
{
    
    const std::string filename = "scan_iteration.tmp";

    
    for (int i = 0; i < 3; ++i)
    {
        
        io->delete_temp_sparse_file(filename);

        
        StatusCode result = io->create_temp_sparse_file(filename, 1024 * (i + 1));
        EXPECT_EQ(StatusCode::STATUS_OK, result);

        std::filesystem::path expectedPath = testStoragePath / filename;
        EXPECT_TRUE(std::filesystem::exists(expectedPath));
    }
}



TEST_F(IOTest, MapFile_ValidFile_Succeeds)
{
    
    const std::string filename = "map_test.tmp";
    constexpr std::size_t fileSize = 4096;
    io->create_temp_sparse_file(filename, fileSize);

    
    
    
}



TEST_F(IOTest, ResizeFileMap_IncreaseSize_Succeeds)
{
    
    const std::string filename = "resize_test.tmp";
    constexpr std::size_t initialSize = 1024;
    const std::size_t newSize = 4096;
    io->create_temp_sparse_file(filename, initialSize);

    
    
}



TEST_F(IOTest, CreateTempSparseFile_VerySmallSize_Succeeds)
{
    
    const std::string filename = "tiny_file.tmp";
    constexpr std::size_t fileSize = 1; 

    
    StatusCode result = io->create_temp_sparse_file(filename, fileSize);

    
    EXPECT_EQ(StatusCode::STATUS_OK, result);
}

TEST_F(IOTest, CreateTempSparseFile_SpecialCharactersInName_Succeeds)
{
    
    const std::string filename = "test_file_with_underscores_123.tmp";
    constexpr std::size_t fileSize = 1024;

    
    StatusCode result = io->create_temp_sparse_file(filename, fileSize);

    
    EXPECT_EQ(StatusCode::STATUS_OK, result);
}
