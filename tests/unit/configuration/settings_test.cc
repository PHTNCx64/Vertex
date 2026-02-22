//
// Unit tests for Settings configuration
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

// ==================== Load/Save Tests ====================

TEST_F(SettingsTest, LoadFromFile_ValidJSON_Succeeds)
{
    // Arrange
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

    // Act
    StatusCode result = settings->load_from_file(testFilePath);

    // Assert
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
    // Arrange
    std::ofstream file(testFilePath);
    file << "{ invalid json }";
    file.close();

    // Act
    StatusCode result = settings->load_from_file(testFilePath);

    // Assert
    EXPECT_EQ(StatusCode::STATUS_ERROR_FS_JSON_PARSE_FAILED, result);
}

TEST_F(SettingsTest, SaveToFile_CreatesValidJSON)
{
    // Arrange
    settings->set_value("memoryScan.readerThreads", 16);

    // Act
    StatusCode result = settings->save_to_file(testFilePath);

    // Assert
    EXPECT_EQ(StatusCode::STATUS_OK, result);
    EXPECT_TRUE(std::filesystem::exists(testFilePath));

    // Verify saved content
    std::ifstream file(testFilePath);
    nlohmann::json savedData;
    file >> savedData;
    EXPECT_EQ(16, savedData["memoryScan"]["readerThreads"]);
}

TEST_F(SettingsTest, SaveToFile_CreatesDirectoryIfNeeded)
{
    // Arrange
    std::filesystem::path nestedPath = testFilePath.parent_path() / "nested" / "settings.json";
    settings->set_value("test", "value");

    // Act
    StatusCode result = settings->save_to_file(nestedPath);

    // Assert
    EXPECT_EQ(StatusCode::STATUS_OK, result);
    EXPECT_TRUE(std::filesystem::exists(nestedPath));

    // Cleanup
    std::filesystem::remove_all(nestedPath.parent_path());
}

// ==================== Nested Value Tests ====================

TEST_F(SettingsTest, SetNestedValue_CreatesStructure)
{
    // Act
    settings->set_value("memoryScan.readerThreads", 16);
    settings->set_value("general.theme", 2);

    // Assert
    auto json = settings->get_settings();
    EXPECT_EQ(16, json["memoryScan"]["readerThreads"]);
    EXPECT_EQ(2, json["general"]["theme"]);
}

TEST_F(SettingsTest, SetNestedValue_DeepNesting_Works)
{
    // Act
    settings->set_value("level1.level2.level3.value", 42);

    // Assert
    auto json = settings->get_settings();
    EXPECT_EQ(42, json["level1"]["level2"]["level3"]["value"]);
}

TEST_F(SettingsTest, GetValue_ExistingKey_ReturnsValue)
{
    // Arrange
    settings->set_value("memoryScan.readerThreads", 8);

    // Act
    auto value = settings->get_value("memoryScan.readerThreads");

    // Assert
    EXPECT_TRUE(value.is_number());
    EXPECT_EQ(8, value.get<int>());
}

TEST_F(SettingsTest, GetValue_NonExistentKey_ReturnsEmpty)
{
    // Act
    auto value = settings->get_value("nonexistent.key");

    // Assert
    EXPECT_TRUE(value.empty());
}

// ==================== Convenience Getter Tests ====================

TEST_F(SettingsTest, GetInt_ExistingKey_ReturnsValue)
{
    // Arrange
    settings->set_value("memoryScan.readerThreads", 16);

    // Act
    int result = settings->get_int("memoryScan.readerThreads", 1);

    // Assert
    EXPECT_EQ(16, result);
}

TEST_F(SettingsTest, GetInt_NonExistentKey_ReturnsDefault)
{
    // Act
    const int result = settings->get_int("nonexistent.key", 42);

    // Assert
    EXPECT_EQ(42, result);
}

TEST_F(SettingsTest, GetBool_ExistingKey_ReturnsValue)
{
    // Arrange
    settings->set_value("general.enableLogging", true);

    // Act
    const bool result = settings->get_bool("general.enableLogging", false);

    // Assert
    EXPECT_TRUE(result);
}

TEST_F(SettingsTest, GetBool_NonExistentKey_ReturnsDefault)
{
    // Act
    bool result = settings->get_bool("nonexistent.key", true);

    // Assert
    EXPECT_TRUE(result);
}

TEST_F(SettingsTest, GetString_ExistingKey_ReturnsValue)
{
    // Arrange
    settings->set_value("language.activeLanguage", "English.json");

    // Act
    std::string result = settings->get_string("language.activeLanguage", Vertex::EMPTY_STRING);

    // Assert
    EXPECT_EQ("English.json", result);
}

TEST_F(SettingsTest, GetString_NonExistentKey_ReturnsDefault)
{
    // Act
    std::string result = settings->get_string("nonexistent.key", "default");

    // Assert
    EXPECT_EQ("default", result);
}

// ==================== Validation Tests ====================

TEST_F(SettingsTest, Validate_ValidSettings_ReturnsTrue)
{
    // Arrange
    settings->set_value("general.autoSaveInterval", 5);
    settings->set_value("memoryScan.readerThreads", 4);

    // Act
    bool result = settings->validate();

    // Assert
    EXPECT_TRUE(result);
}

TEST_F(SettingsTest, Validate_InvalidAutoSaveInterval_ReturnsFalse)
{
    // Arrange - autoSaveInterval should be between 1 and 3600
    settings->set_value("general.autoSaveInterval", 5000);

    // Act
    bool result = settings->validate();

    // Assert
    EXPECT_FALSE(result);
}

TEST_F(SettingsTest, Validate_InvalidThreadCount_ReturnsFalse)
{
    // Arrange - thread count should be between 1 and 64
    settings->set_value("memoryScan.readerThreads", 100);

    // Act
    bool result = settings->validate();

    // Assert
    EXPECT_FALSE(result);
}

// ==================== Reset to Defaults Tests ====================

TEST_F(SettingsTest, ResetToDefaults_SetsDefaultValues)
{
    // Arrange
    settings->set_value("memoryScan.readerThreads", 999);

    // Act
    settings->reset_to_defaults();

    // Assert
    int readerThreads = settings->get_int("memoryScan.readerThreads", 0);
    EXPECT_GT(readerThreads, 0);
    EXPECT_NE(999, readerThreads);
}
