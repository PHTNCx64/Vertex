//
// Unit tests for SettingsViewModel
//

#include <gtest/gtest.h>
#include <gmock/gmock.h>

#include <vertex/viewmodel/settingsviewmodel.hh>
#include "../../mocks/MockSettingsModel.hh"
#include "../../mocks/MockILog.hh"

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
        auto mockModelPtr = std::make_unique<NiceMock<MockSettingsModel>>();
        mockModel = mockModelPtr.get();
        mockLogger = std::make_unique<NiceMock<MockILog>>();

        viewModel = std::make_unique<ViewModel::SettingsViewModel>(
            std::move(mockModelPtr),
            eventBus,
            *mockLogger
        );
    }

    Vertex::Event::EventBus eventBus{};
    MockSettingsModel* mockModel{};
    std::unique_ptr<NiceMock<MockILog>> mockLogger;
    std::unique_ptr<ViewModel::SettingsViewModel> viewModel;
};

// ==================== Thread Settings Tests ====================

TEST_F(SettingsViewModelTest, GetReaderThreads_ReturnsModelValue)
{
    constexpr int expectedCount = 8;
    EXPECT_CALL(*mockModel, get_reader_threads(_))
        .WillOnce(DoAll(SetArgReferee<0>(expectedCount),
                       Return(StatusCode::STATUS_OK)));

    int result = viewModel->get_reader_threads();
    EXPECT_EQ(expectedCount, result);
}

TEST_F(SettingsViewModelTest, SetReaderThreads_CallsModel)
{
    constexpr int threadCount = 16;
    EXPECT_CALL(*mockModel, set_reader_threads(threadCount))
        .WillOnce(Return(StatusCode::STATUS_OK));

    viewModel->set_reader_threads(threadCount);
}

// ==================== Plugin Path Tests ====================

TEST_F(SettingsViewModelTest, AddPluginPath_Success_ReturnsTrue)
{
    std::filesystem::path testPath = "/test/path";
    EXPECT_CALL(*mockModel, add_plugin_path(testPath))
        .WillOnce(Return(StatusCode::STATUS_OK));

    bool result = viewModel->add_plugin_path(testPath);
    EXPECT_TRUE(result);
}

TEST_F(SettingsViewModelTest, AddPluginPath_Failure_ReturnsFalse)
{
    std::filesystem::path testPath = "/test/path";
    EXPECT_CALL(*mockModel, add_plugin_path(testPath))
        .WillOnce(Return(StatusCode::STATUS_ERROR_GENERAL));

    bool result = viewModel->add_plugin_path(testPath);
    EXPECT_FALSE(result);
}

TEST_F(SettingsViewModelTest, RemovePluginPath_Success_ReturnsTrue)
{
    std::filesystem::path testPath = "/test/path";
    EXPECT_CALL(*mockModel, remove_plugin_path(testPath))
        .WillOnce(Return(StatusCode::STATUS_OK));

    bool result = viewModel->remove_plugin_path(testPath);
    EXPECT_TRUE(result);
}

TEST_F(SettingsViewModelTest, GetPluginPaths_ReturnsModelPaths)
{
    std::vector<std::filesystem::path> expectedPaths = {
        "/path1",
        "/path2"
    };
    EXPECT_CALL(*mockModel, get_plugin_paths())
        .WillOnce(Return(expectedPaths));

    auto result = viewModel->get_plugin_paths();
    EXPECT_EQ(expectedPaths.size(), result.size());
}

// ==================== General Settings Tests ====================

TEST_F(SettingsViewModelTest, GetTheme_ReturnsModelValue)
{
    const int expectedTheme = 1;
    EXPECT_CALL(*mockModel, get_theme(_))
        .WillOnce(DoAll(SetArgReferee<0>(expectedTheme),
                       Return(StatusCode::STATUS_OK)));

    int result = viewModel->get_theme();
    EXPECT_EQ(expectedTheme, result);
}

TEST_F(SettingsViewModelTest, SetTheme_CallsModel)
{
    const int theme = 2;
    EXPECT_CALL(*mockModel, set_theme(theme))
        .WillOnce(Return(StatusCode::STATUS_OK));

    viewModel->set_theme(theme);
}

TEST_F(SettingsViewModelTest, GetLoggingStatus_ReturnsModelValue)
{
    EXPECT_CALL(*mockModel, get_logging_status(_))
        .WillOnce(DoAll(SetArgReferee<0>(true),
                       Return(StatusCode::STATUS_OK)));

    bool result = viewModel->get_logging_status();
    EXPECT_TRUE(result);
}

TEST_F(SettingsViewModelTest, SaveSettings_CallsModel)
{
    EXPECT_CALL(*mockModel, save_settings())
        .WillOnce(Return(StatusCode::STATUS_OK));

    viewModel->save_settings();
}

TEST_F(SettingsViewModelTest, ApplySettings_CallsModel)
{
    EXPECT_CALL(*mockModel, save_settings())
        .WillOnce(Return(StatusCode::STATUS_OK));

    viewModel->apply_settings();
}

// ==================== Language Management Tests ====================

TEST_F(SettingsViewModelTest, GetAvailableLanguages_ReturnsModelLanguages)
{
    std::unordered_map<std::string, std::filesystem::path> expectedLanguages = {
        {"English", "/languages/English.json"},
        {"German", "/languages/German.json"}
    };
    EXPECT_CALL(*mockModel, get_available_languages())
        .WillOnce(Return(expectedLanguages));

    auto result = viewModel->get_available_languages();
    EXPECT_EQ(expectedLanguages.size(), result.size());
}

TEST_F(SettingsViewModelTest, SetActiveLanguage_CallsModel)
{
    std::string languageKey = "German";
    EXPECT_CALL(*mockModel, set_active_language(_))
        .WillOnce(Return(StatusCode::STATUS_OK));

    viewModel->set_active_language(languageKey);
}
