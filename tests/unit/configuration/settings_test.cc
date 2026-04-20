//
// Copyright (C) 2026 PHTNC<>.
// Licensed under GPLv3.0 with Plugin Interface exceptions.
//




#include <gtest/gtest.h>
#include <gmock/gmock.h>
#include <vertex/configuration/settings.hh>
#include <vertex/utility.hh>
#include "../../mocks/MockILog.hh"
#include <fstream>
#include <filesystem>

using namespace Vertex::Configuration;
using namespace Vertex::Testing::Mocks;
using ::testing::NiceMock;

class SettingsTest : public ::testing::Test
{
protected:
    void SetUp() override
    {
        testFilePath = std::filesystem::temp_directory_path() / "vertex_test_settings.json";
        mockLog = std::make_unique<NiceMock<MockILog>>();
        settings = std::make_unique<Settings>(*mockLog);
    }

    void TearDown() override
    {
        if (std::filesystem::exists(testFilePath))
        {
            std::filesystem::remove(testFilePath);
        }
    }

    std::filesystem::path testFilePath;
    std::unique_ptr<MockILog> mockLog;
    std::unique_ptr<Settings> settings;

    void CreateTestJsonFile(const nlohmann::json& data)
    {
        std::ofstream file(testFilePath);
        file << data.dump(4);
        file.close();
    }
};



TEST_F(SettingsTest, LoadFromFile_ValidJSON_Succeeds)
{
    
    nlohmann::json testData = {
        {"general", {
            {"theme", 1},
            {"enableLogging", true}
        }},
        {"memoryScan", {
            {"readerThreads", 8}
        }}
    };
    CreateTestJsonFile(testData);

    
    StatusCode result = settings->load_from_file(testFilePath);

    
    EXPECT_EQ(StatusCode::STATUS_OK, result);
    EXPECT_EQ(1, settings->get_int("general.theme"));
    EXPECT_EQ(8, settings->get_int("memoryScan.readerThreads"));
}

TEST_F(SettingsTest, LoadFromFile_NonExistentFile_ReturnsError)
{
    const std::filesystem::path nonExistentPath = "/nonexistent/path/settings.json";
    const StatusCode result = settings->load_from_file(nonExistentPath);
    EXPECT_EQ(StatusCode::STATUS_ERROR_FILE_NOT_FOUND, result);
}

TEST_F(SettingsTest, LoadFromFile_InvalidJSON_ReturnsError)
{
    
    std::ofstream file(testFilePath);
    file << "{ invalid json }";
    file.close();

    
    StatusCode result = settings->load_from_file(testFilePath);

    
    EXPECT_EQ(StatusCode::STATUS_ERROR_FS_JSON_PARSE_FAILED, result);
}

TEST_F(SettingsTest, SaveToFile_CreatesValidJSON)
{
    
    settings->set_value("memoryScan.readerThreads", 16);

    
    StatusCode result = settings->save_to_file(testFilePath);

    
    EXPECT_EQ(StatusCode::STATUS_OK, result);
    EXPECT_TRUE(std::filesystem::exists(testFilePath));

    
    std::ifstream file(testFilePath);
    nlohmann::json savedData;
    file >> savedData;
    EXPECT_EQ(16, savedData["memoryScan"]["readerThreads"]);
}

TEST_F(SettingsTest, SaveToFile_CreatesDirectoryIfNeeded)
{
    
    std::filesystem::path nestedPath = testFilePath.parent_path() / "nested" / "settings.json";
    settings->set_value("test", "value");

    
    StatusCode result = settings->save_to_file(nestedPath);

    
    EXPECT_EQ(StatusCode::STATUS_OK, result);
    EXPECT_TRUE(std::filesystem::exists(nestedPath));

    
    std::filesystem::remove_all(nestedPath.parent_path());
}



TEST_F(SettingsTest, SetNestedValue_CreatesStructure)
{
    
    settings->set_value("memoryScan.readerThreads", 16);
    settings->set_value("general.theme", 2);

    
    auto json = settings->get_settings();
    EXPECT_EQ(16, json["memoryScan"]["readerThreads"]);
    EXPECT_EQ(2, json["general"]["theme"]);
}

TEST_F(SettingsTest, SetNestedValue_DeepNesting_Works)
{
    
    settings->set_value("level1.level2.level3.value", 42);

    
    auto json = settings->get_settings();
    EXPECT_EQ(42, json["level1"]["level2"]["level3"]["value"]);
}

TEST_F(SettingsTest, GetValue_ExistingKey_ReturnsValue)
{
    
    settings->set_value("memoryScan.readerThreads", 8);

    
    auto value = settings->get_value("memoryScan.readerThreads");

    
    EXPECT_TRUE(value.is_number());
    EXPECT_EQ(8, value.get<int>());
}

TEST_F(SettingsTest, GetValue_NonExistentKey_ReturnsEmpty)
{
    
    auto value = settings->get_value("nonexistent.key");

    
    EXPECT_TRUE(value.empty());
}



TEST_F(SettingsTest, GetInt_ExistingKey_ReturnsValue)
{
    
    settings->set_value("memoryScan.readerThreads", 16);

    
    int result = settings->get_int("memoryScan.readerThreads", 1);

    
    EXPECT_EQ(16, result);
}

TEST_F(SettingsTest, GetInt_NonExistentKey_ReturnsDefault)
{
    
    const int result = settings->get_int("nonexistent.key", 42);

    
    EXPECT_EQ(42, result);
}

TEST_F(SettingsTest, GetBool_ExistingKey_ReturnsValue)
{
    
    settings->set_value("general.enableLogging", true);

    
    const bool result = settings->get_bool("general.enableLogging", false);

    
    EXPECT_TRUE(result);
}

TEST_F(SettingsTest, GetBool_NonExistentKey_ReturnsDefault)
{
    
    bool result = settings->get_bool("nonexistent.key", true);

    
    EXPECT_TRUE(result);
}

TEST_F(SettingsTest, GetString_ExistingKey_ReturnsValue)
{
    
    settings->set_value("language.activeLanguage", "English.json");

    
    std::string result = settings->get_string("language.activeLanguage", Vertex::EMPTY_STRING);

    
    EXPECT_EQ("English.json", result);
}

TEST_F(SettingsTest, GetString_NonExistentKey_ReturnsDefault)
{
    
    std::string result = settings->get_string("nonexistent.key", "default");

    
    EXPECT_EQ("default", result);
}



TEST_F(SettingsTest, Validate_ValidSettings_ReturnsTrue)
{
    
    settings->set_value("general.autoSaveInterval", 5);
    settings->set_value("memoryScan.readerThreads", 4);

    
    bool result = settings->validate();

    
    EXPECT_TRUE(result);
}

TEST_F(SettingsTest, Validate_InvalidAutoSaveInterval_ReturnsFalse)
{
    
    settings->set_value("general.autoSaveInterval", 5000);

    
    bool result = settings->validate();

    
    EXPECT_FALSE(result);
}

TEST_F(SettingsTest, Validate_InvalidThreadCount_ReturnsFalse)
{
    
    settings->set_value("memoryScan.readerThreads", 100);

    
    bool result = settings->validate();

    
    EXPECT_FALSE(result);
}

TEST_F(SettingsTest, Validate_InvalidMemoryScanWorkerChunkSize_ReturnsFalse)
{
    settings->set_value("memoryScan.workerChunkSizeMB", 0);

    const bool result = settings->validate();

    EXPECT_FALSE(result);
}



TEST_F(SettingsTest, ResetToDefaults_SetsDefaultValues)
{
    
    settings->set_value("memoryScan.readerThreads", 999);

    
    settings->reset_to_defaults();

    
    int readerThreads = settings->get_int("memoryScan.readerThreads", 0);
    EXPECT_GT(readerThreads, 0);
    EXPECT_NE(999, readerThreads);
}
