//
// Unit tests for SettingsViewModel
//

#include <gtest/gtest.h>
#include <gmock/gmock.h>

#include <vertex/viewmodel/settingsviewmodel.hh>
#include "../../mocks/MockSettingsModel.hh"
#include "../../mocks/MockEventBus.hh"

using namespace Vertex;
using namespace Vertex::Testing::Mocks;
using ::testing::_;
using ::testing::Return;
using ::testing::DoAll;
using ::testing::SetArgReferee;
using ::testing::NiceMock;

class SettingsViewModelTest : public ::testing::Test
{
protected:
    void SetUp() override
    {
        mockModel = std::make_unique<NiceMock<MockSettingsModel>>();
        mockEventBus = std::make_unique<NiceMock<MockEventBus>>();

        // Note: SettingsViewModel takes unique_ptr to model, so we need to pass raw pointer
        // For testing purposes, we'll need to modify SettingsViewModel or use a factory pattern
        // For now, this shows the testing structure
    }

    std::unique_ptr<MockSettingsModel> mockModel;
    std::unique_ptr<MockEventBus> mockEventBus;
};

// ==================== Thread Settings Tests ====================

TEST_F(SettingsViewModelTest, GetReaderThreads_ReturnsModelValue)
{
    // Arrange
    constexpr int expectedCount = 8;
    EXPECT_CALL(*mockModel, get_reader_threads(_))
        .WillOnce(DoAll(SetArgReferee<0>(expectedCount),
                       Return(StatusCode::STATUS_OK)));

    // Note: This test structure shows how it would work
    // Actual implementation requires dependency injection refactoring
    // int result = viewModel->get_reader_threads();
    // EXPECT_EQ(expectedCount, result);
}

TEST_F(SettingsViewModelTest, SetReaderThreads_CallsModel)
{
    // Arrange
    const int threadCount = 16;
    EXPECT_CALL(*mockModel, set_reader_threads(threadCount))
        .WillOnce(Return(StatusCode::STATUS_OK));

    // Act & Assert
    // viewModel->set_reader_threads(threadCount);
}

// ==================== Plugin Path Tests ====================

TEST_F(SettingsViewModelTest, AddPluginPath_Success_ReturnsTrue)
{
    // Arrange
    std::filesystem::path testPath = "/test/path";
    EXPECT_CALL(*mockModel, add_plugin_path(testPath))
        .WillOnce(Return(StatusCode::STATUS_OK));

    // Act & Assert
    // bool result = viewModel->add_plugin_path(testPath);
    // EXPECT_TRUE(result);
}

TEST_F(SettingsViewModelTest, AddPluginPath_Failure_ReturnsFalse)
{
    // Arrange
    std::filesystem::path testPath = "/test/path";
    EXPECT_CALL(*mockModel, add_plugin_path(testPath))
        .WillOnce(Return(StatusCode::STATUS_ERROR_GENERAL));

    // Act & Assert
    // bool result = viewModel->add_plugin_path(testPath);
    // EXPECT_FALSE(result);
}

TEST_F(SettingsViewModelTest, RemovePluginPath_Success_ReturnsTrue)
{
    // Arrange
    std::filesystem::path testPath = "/test/path";
    EXPECT_CALL(*mockModel, remove_plugin_path(testPath))
        .WillOnce(Return(StatusCode::STATUS_OK));

    // Act & Assert
    // bool result = viewModel->remove_plugin_path(testPath);
    // EXPECT_TRUE(result);
}

TEST_F(SettingsViewModelTest, GetPluginPaths_ReturnsModelPaths)
{
    // Arrange
    std::vector<std::filesystem::path> expectedPaths = {
        "/path1",
        "/path2"
    };
    EXPECT_CALL(*mockModel, get_plugin_paths())
        .WillOnce(Return(expectedPaths));

    // Act & Assert
    // auto result = viewModel->get_plugin_paths();
    // EXPECT_EQ(expectedPaths.size(), result.size());
}

// ==================== General Settings Tests ====================

TEST_F(SettingsViewModelTest, GetTheme_ReturnsModelValue)
{
    // Arrange
    const int expectedTheme = 1;
    EXPECT_CALL(*mockModel, get_theme(_))
        .WillOnce(DoAll(SetArgReferee<0>(expectedTheme),
                       Return(StatusCode::STATUS_OK)));

    // Act & Assert
    // int result = viewModel->get_theme();
    // EXPECT_EQ(expectedTheme, result);
}

TEST_F(SettingsViewModelTest, SetTheme_CallsModel)
{
    // Arrange
    const int theme = 2;
    EXPECT_CALL(*mockModel, set_theme(theme))
        .WillOnce(Return(StatusCode::STATUS_OK));

    // Act & Assert
    // viewModel->set_theme(theme);
}

TEST_F(SettingsViewModelTest, GetLoggingStatus_ReturnsModelValue)
{
    // Arrange
    EXPECT_CALL(*mockModel, get_logging_status(_))
        .WillOnce(DoAll(SetArgReferee<0>(true),
                       Return(StatusCode::STATUS_OK)));

    // Act & Assert
    // bool result = viewModel->get_logging_status();
    // EXPECT_TRUE(result);
}

TEST_F(SettingsViewModelTest, SaveSettings_CallsModel)
{
    // Arrange
    EXPECT_CALL(*mockModel, save_settings())
        .WillOnce(Return(StatusCode::STATUS_OK));

    // Act & Assert
    // viewModel->save_settings();
}

TEST_F(SettingsViewModelTest, ApplySettings_CallsModel)
{
    // Arrange
    EXPECT_CALL(*mockModel, save_settings())
        .WillOnce(Return(StatusCode::STATUS_OK));

    // Act & Assert
    // viewModel->apply_settings();
}

// ==================== Language Management Tests ====================

TEST_F(SettingsViewModelTest, GetAvailableLanguages_ReturnsModelLanguages)
{
    // Arrange
    std::unordered_map<std::string, std::filesystem::path> expectedLanguages = {
        {"English", "/languages/English.json"},
        {"German", "/languages/German.json"}
    };
    EXPECT_CALL(*mockModel, get_available_languages())
        .WillOnce(Return(expectedLanguages));

    // Act & Assert
    // auto result = viewModel->get_available_languages();
    // EXPECT_EQ(expectedLanguages.size(), result.size());
}

TEST_F(SettingsViewModelTest, SetActiveLanguage_CallsModel)
{
    // Arrange
    std::string languageKey = "German";
    EXPECT_CALL(*mockModel, set_active_language(languageKey))
        .WillOnce(Return(StatusCode::STATUS_OK));

    // Act & Assert
    // viewModel->set_active_language(languageKey);
}

// Note: These tests are templates showing the testing structure
// To make them fully functional, SettingsViewModel needs to accept
// an interface or mock for SettingsModel via dependency injection
