 //
// Unit tests for SettingsModel
//

#include <gtest/gtest.h>
#include <gmock/gmock.h>

#include <vertex/model/settingsmodel.hh>
#include "../../mocks/MockISettings.hh"
#include "../../mocks/MockILoader.hh"
#include "../../mocks/MockILog.hh"
#include "../../mocks/MockILanguage.hh"

using namespace Vertex;
using namespace Vertex::Testing::Mocks;
using ::testing::_;
using ::testing::Return;
using ::testing::DoAll;
using ::testing::SetArgReferee;
using ::testing::NiceMock;

class SettingsModelTest : public ::testing::Test
{
protected:
    void SetUp() override
    {
        mockSettings = std::make_unique<NiceMock<MockISettings>>();
        mockLoader = std::make_unique<NiceMock<MockILoader>>();
        mockLog = std::make_unique<NiceMock<MockILog>>();
        mockLanguage = std::make_unique<NiceMock<MockILanguage>>();

        model = std::make_unique<Model::SettingsModel>(
            *mockLoader, *mockLog, *mockLanguage, *mockSettings
        );
    }

    std::unique_ptr<MockISettings> mockSettings;
    std::unique_ptr<MockILoader> mockLoader;
    std::unique_ptr<MockILog> mockLog;
    std::unique_ptr<MockILanguage> mockLanguage;
    std::unique_ptr<Model::SettingsModel> model;
};

// ==================== Reader/Writer Threads Tests ====================

TEST_F(SettingsModelTest, GetReaderThreads_ReturnsCorrectValue)
{
    // Arrange
    constexpr int expectedCount = 8;
    EXPECT_CALL(*mockSettings, get_int("memoryScan.readerThreads", 1))
        .WillOnce(Return(expectedCount));

    // Act
    int count = 0;
    const StatusCode result = model->get_reader_threads(count);

    // Assert
    EXPECT_EQ(StatusCode::STATUS_OK, result);
    EXPECT_EQ(expectedCount, count);
}

TEST_F(SettingsModelTest, SetReaderThreads_ValidCount_Succeeds)
{
    // Arrange
    constexpr int threadCount = 16;
    EXPECT_CALL(*mockSettings, set_value("memoryScan.readerThreads", ::testing::_))
        .Times(1);

    // Act
    const StatusCode result = model->set_reader_threads(threadCount);

    // Assert
    EXPECT_EQ(StatusCode::STATUS_OK, result);
}

// ==================== Plugin Paths Tests ====================

TEST_F(SettingsModelTest, GetPluginPaths_EmptyArray_ReturnsEmptyVector)
{
    // Arrange
    nlohmann::json emptyArray = nlohmann::json::array();
    EXPECT_CALL(*mockSettings, get_value("plugins.pluginPaths"))
        .WillOnce(Return(emptyArray));

    // Act
    auto paths = model->get_plugin_paths();

    // Assert
    EXPECT_TRUE(paths.empty());
}

TEST_F(SettingsModelTest, GetPluginPaths_WithPaths_ReturnsCorrectPaths)
{
    // Arrange
    nlohmann::json pathsArray = nlohmann::json::array();
    pathsArray.push_back("/path/to/plugins1");
    pathsArray.push_back("/path/to/plugins2");

    EXPECT_CALL(*mockSettings, get_value("plugins.pluginPaths"))
        .WillOnce(Return(pathsArray));

    // Act
    auto paths = model->get_plugin_paths();

    // Assert
    EXPECT_EQ(2, paths.size());
    EXPECT_EQ("/path/to/plugins1", paths[0].string());
    EXPECT_EQ("/path/to/plugins2", paths[1].string());
}

TEST_F(SettingsModelTest, AddPluginPath_NewPath_ReturnsOK)
{
    // Arrange
    nlohmann::json existingPaths = nlohmann::json::array();
    existingPaths.push_back("/existing/path");

    EXPECT_CALL(*mockSettings, get_value("plugins.pluginPaths"))
        .WillOnce(Return(existingPaths));

    EXPECT_CALL(*mockSettings, set_value("plugins.pluginPaths", _))
        .Times(1);

    // Act
    StatusCode result = model->add_plugin_path("/new/path");

    // Assert
    EXPECT_EQ(StatusCode::STATUS_OK, result);
}

TEST_F(SettingsModelTest, AddPluginPath_DuplicatePath_ReturnsError)
{
    // Arrange
    nlohmann::json existingPaths = nlohmann::json::array();
    existingPaths.push_back("/duplicate/path");

    EXPECT_CALL(*mockSettings, get_value("plugins.pluginPaths"))
        .WillOnce(Return(existingPaths));

    // Act
    StatusCode result = model->add_plugin_path("/duplicate/path");

    // Assert
    EXPECT_EQ(StatusCode::STATUS_ERROR_GENERAL_ALREADY_EXISTS, result);
}

TEST_F(SettingsModelTest, RemovePluginPath_ExistingPath_ReturnsOK)
{
    // Arrange
    nlohmann::json existingPaths = nlohmann::json::array();
    existingPaths.push_back("/path/to/remove");
    existingPaths.push_back("/path/to/keep");

    EXPECT_CALL(*mockSettings, get_value("plugins.pluginPaths"))
        .WillOnce(Return(existingPaths));

    EXPECT_CALL(*mockSettings, set_value("plugins.pluginPaths", _))
        .Times(1);

    // Act
    StatusCode result = model->remove_plugin_path("/path/to/remove");

    // Assert
    EXPECT_EQ(StatusCode::STATUS_OK, result);
}

TEST_F(SettingsModelTest, RemovePluginPath_NonExistentPath_ReturnsError)
{
    // Arrange
    nlohmann::json existingPaths = nlohmann::json::array();
    existingPaths.push_back("/existing/path");

    EXPECT_CALL(*mockSettings, get_value("plugins.pluginPaths"))
        .WillOnce(Return(existingPaths));

    // Act
    StatusCode result = model->remove_plugin_path("/nonexistent/path");

    // Assert
    EXPECT_EQ(StatusCode::STATUS_ERROR_FS_JSON_KEY_NOT_FOUND, result);
}

// ==================== General Settings Tests ====================

TEST_F(SettingsModelTest, SetTheme_ValidTheme_Succeeds)
{
    // Arrange
    constexpr int theme = 1;
    EXPECT_CALL(*mockSettings, set_value("general.theme", ::testing::_))
        .Times(1);

    // Act
    const StatusCode result = model->set_theme(theme);

    // Assert
    EXPECT_EQ(StatusCode::STATUS_OK, result);
}

TEST_F(SettingsModelTest, GetTheme_ReturnsCorrectValue)
{
    // Arrange
    constexpr int expectedTheme = 2;
    EXPECT_CALL(*mockSettings, get_int("general.theme", ::testing::_))
        .WillOnce(Return(expectedTheme));

    // Act
    int theme = 0;
    const StatusCode result = model->get_theme(theme);

    // Assert
    EXPECT_EQ(StatusCode::STATUS_OK, result);
    EXPECT_EQ(expectedTheme, theme);
}

TEST_F(SettingsModelTest, SetLoggingStatus_EnableLogging_Succeeds)
{
    // Arrange
    EXPECT_CALL(*mockSettings, set_value("general.enableLogging", ::testing::_))
        .Times(1);
    EXPECT_CALL(*mockLog, set_logging_status(true))
        .WillOnce(Return(StatusCode::STATUS_OK));

    // Act
    StatusCode result = model->set_logging_status(true);

    // Assert
    EXPECT_EQ(StatusCode::STATUS_OK, result);
}

TEST_F(SettingsModelTest, GetLoggingStatus_ReturnsCorrectValue)
{
    // Arrange
    EXPECT_CALL(*mockSettings, get_bool("general.enableLogging", ::testing::_))
        .WillOnce(Return(true));

    // Act
    bool status = false;
    StatusCode result = model->get_logging_status(status);

    // Assert
    EXPECT_EQ(StatusCode::STATUS_OK, result);
    EXPECT_TRUE(status);
}

TEST_F(SettingsModelTest, SaveSettings_CallsSettingsService)
{
    // Arrange
    EXPECT_CALL(*mockSettings, save_to_file(_))
        .WillOnce(Return(StatusCode::STATUS_OK));

    // Act
    StatusCode result = model->save_settings();

    // Assert
    EXPECT_EQ(StatusCode::STATUS_OK, result);
}
