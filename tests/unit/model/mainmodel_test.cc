//
// Unit tests for MainModel
//

#include <gtest/gtest.h>
#include <gmock/gmock.h>
#include <vertex/model/mainmodel.hh>
#include "../../mocks/MockISettings.hh"
#include "../../mocks/MockIMemoryScanner.hh"
#include "../../mocks/MockILoader.hh"
#include "../../mocks/MockILog.hh"

using ::testing::Return;
using ::testing::_;
using ::testing::NiceMock;
using ::testing::StrictMock;

class MainModelTest : public ::testing::Test
{
protected:
    void SetUp() override
    {
        mockSettings = std::make_unique<NiceMock<Vertex::Testing::Mocks::MockISettings>>();
        mockScanner = std::make_unique<NiceMock<Vertex::Testing::Mocks::MockIMemoryScanner>>();
        mockLoader = std::make_unique<NiceMock<Vertex::Testing::Mocks::MockILoader>>();
        mockLogger = std::make_unique<NiceMock<Vertex::Testing::Mocks::MockILog>>();

        model = std::make_unique<Vertex::Model::MainModel>(
            *mockSettings,
            *mockScanner,
            *mockLoader,
            *mockLogger
        );
    }

    void TearDown() override
    {
    }

    std::unique_ptr<NiceMock<Vertex::Testing::Mocks::MockISettings>> mockSettings;
    std::unique_ptr<NiceMock<Vertex::Testing::Mocks::MockIMemoryScanner>> mockScanner;
    std::unique_ptr<NiceMock<Vertex::Testing::Mocks::MockILoader>> mockLoader;
    std::unique_ptr<NiceMock<Vertex::Testing::Mocks::MockILog>> mockLogger;
    std::unique_ptr<Vertex::Model::MainModel> model;
};

// ==================== Theme Tests ====================

TEST_F(MainModelTest, GetTheme_ReturnsSettingsValue)
{
    // Arrange
    EXPECT_CALL(*mockSettings, get_int("general.theme", _))
        .WillOnce(Return(2));

    // Act
    Vertex::Theme result = model->get_theme();

    // Assert
    EXPECT_EQ(static_cast<Vertex::Theme>(2), result);
}

// ==================== Scan Control Tests ====================

TEST_F(MainModelTest, StopScan_DelegatesToScanner)
{
    // Arrange
    EXPECT_CALL(*mockScanner, stop_scan())
        .WillOnce(Return(StatusCode::STATUS_OK));

    // Act
    const StatusCode result = model->stop_scan();

    // Assert
    EXPECT_EQ(StatusCode::STATUS_OK, result);
}

TEST_F(MainModelTest, IsScanComplete_DelegatesToScanner)
{
    // Arrange
    EXPECT_CALL(*mockScanner, is_scan_complete())
        .WillOnce(Return(true));

    // Act
    const bool result = model->is_scan_complete();

    // Assert
    EXPECT_TRUE(result);
}

TEST_F(MainModelTest, UndoScan_DelegatesToScanner)
{
    // Arrange
    EXPECT_CALL(*mockScanner, undo_scan())
        .WillOnce(Return(StatusCode::STATUS_OK));

    // Act
    const StatusCode result = model->undo_scan();

    // Assert
    EXPECT_EQ(StatusCode::STATUS_OK, result);
}

TEST_F(MainModelTest, CanUndoScan_DelegatesToScanner)
{
    // Arrange
    EXPECT_CALL(*mockScanner, can_undo())
        .WillOnce(Return(true));

    // Act
    const bool result = model->can_undo_scan();

    // Assert
    EXPECT_TRUE(result);
}

// ==================== Progress Tracking Tests ====================

TEST_F(MainModelTest, GetScanProgressCurrent_DelegatesToScanner)
{
    // Arrange
    EXPECT_CALL(*mockScanner, get_regions_scanned())
        .WillOnce(Return(42));

    // Act
    std::uint64_t result = model->get_scan_progress_current();

    // Assert
    EXPECT_EQ(42, result);
}

TEST_F(MainModelTest, GetScanProgressTotal_DelegatesToScanner)
{
    // Arrange
    EXPECT_CALL(*mockScanner, get_total_regions())
        .WillOnce(Return(100));

    // Act
    std::uint64_t result = model->get_scan_progress_total();

    // Assert
    EXPECT_EQ(100, result);
}

TEST_F(MainModelTest, GetScanResultsCount_DelegatesToScanner)
{
    // Arrange
    EXPECT_CALL(*mockScanner, get_results_count())
        .WillOnce(Return(256));

    // Act
    std::uint64_t result = model->get_scan_results_count();

    // Assert
    EXPECT_EQ(256, result);
}

// ==================== Process Management Tests ====================

TEST_F(MainModelTest, IsProcessOpened_NoPluginLoaded_ReturnsError)
{
    // Arrange
    EXPECT_CALL(*mockLoader, has_plugin_loaded())
        .WillOnce(Return(StatusCode::STATUS_ERROR_PLUGIN_NOT_ACTIVE));

    EXPECT_CALL(*mockLogger, log_error(_))
        .Times(1);

    // Act
    StatusCode result = model->is_process_opened();

    // Assert
    EXPECT_EQ(StatusCode::STATUS_ERROR_PLUGIN_NOT_ACTIVE, result);
}

TEST_F(MainModelTest, IsProcessOpened_PluginNotLoaded_ReturnsError)
{
    // Arrange
    EXPECT_CALL(*mockLoader, has_plugin_loaded())
        .WillOnce(Return(StatusCode::STATUS_OK));

    Vertex::Runtime::Plugin mockPlugin{};
    // Don't set plugin handle - is_loaded() will return false

    EXPECT_CALL(*mockLoader, get_active_plugin())
        .WillOnce(Return(std::optional<std::reference_wrapper<Vertex::Runtime::Plugin>>(mockPlugin)));

    EXPECT_CALL(*mockLogger, log_error(_))
        .Times(1);

    // Act
    StatusCode result = model->is_process_opened();

    // Assert
    EXPECT_EQ(StatusCode::STATUS_ERROR_PLUGIN_NOT_LOADED, result);
}

TEST_F(MainModelTest, IsProcessOpened_FunctionNotImplemented_ReturnsError)
{
    // Arrange
    EXPECT_CALL(*mockLoader, has_plugin_loaded())
        .WillOnce(Return(StatusCode::STATUS_OK));

    Vertex::Runtime::Plugin mockPlugin{};
    mockPlugin.set_plugin_handle(reinterpret_cast<void*>(0x1)); // Make is_loaded() return true
    mockPlugin.internal_vertex_is_process_valid = nullptr; // Not implemented

    EXPECT_CALL(*mockLoader, get_active_plugin())
        .WillOnce(Return(std::optional<std::reference_wrapper<Vertex::Runtime::Plugin>>(mockPlugin)));

    EXPECT_CALL(*mockLogger, log_error(_))
        .Times(1);

    // Act
    StatusCode result = model->is_process_opened();

    // Assert
    EXPECT_EQ(StatusCode::STATUS_ERROR_PLUGIN_FUNCTION_NOT_IMPLEMENTED, result);
}

TEST_F(MainModelTest, IsProcessOpened_ValidPlugin_CallsPluginFunction)
{
    // Arrange
    EXPECT_CALL(*mockLoader, has_plugin_loaded())
        .WillOnce(Return(StatusCode::STATUS_OK));

    Vertex::Runtime::Plugin mockPlugin{};
    mockPlugin.set_plugin_handle(reinterpret_cast<void*>(0x1)); // Make is_loaded() return true
    mockPlugin.internal_vertex_is_process_valid = []() {
        return StatusCode::STATUS_OK;
    };

    EXPECT_CALL(*mockLoader, get_active_plugin())
        .WillOnce(Return(std::optional<std::reference_wrapper<Vertex::Runtime::Plugin>>(mockPlugin)));

    // Act
    StatusCode result = model->is_process_opened();

    // Assert
    EXPECT_EQ(StatusCode::STATUS_OK, result);
}

TEST_F(MainModelTest, KillProcess_ValidPlugin_CallsPluginFunction)
{
    // Arrange
    EXPECT_CALL(*mockLoader, has_plugin_loaded())
        .WillOnce(Return(StatusCode::STATUS_OK));

    Vertex::Runtime::Plugin mockPlugin{};
    mockPlugin.set_plugin_handle(reinterpret_cast<void*>(0x1)); // Make is_loaded() return true
    mockPlugin.internal_vertex_kill_process = []() {
        return StatusCode::STATUS_OK;
    };

    EXPECT_CALL(*mockLoader, get_active_plugin())
        .WillOnce(Return(std::optional<std::reference_wrapper<Vertex::Runtime::Plugin>>(mockPlugin)));

    // Act
    StatusCode result = model->kill_process();

    // Assert
    EXPECT_EQ(StatusCode::STATUS_OK, result);
}

TEST_F(MainModelTest, KillProcess_NoPluginLoaded_ReturnsError)
{
    // Arrange
    EXPECT_CALL(*mockLoader, has_plugin_loaded())
        .WillOnce(Return(StatusCode::STATUS_ERROR_PLUGIN_NOT_ACTIVE));

    EXPECT_CALL(*mockLogger, log_error(_))
        .Times(1);

    // Act
    StatusCode result = model->kill_process();

    // Assert
    EXPECT_EQ(StatusCode::STATUS_ERROR_PLUGIN_NOT_ACTIVE, result);
}

// ==================== Validate Input Tests ====================

TEST_F(MainModelTest, ValidateInput_EmptyInput_ReturnsOK)
{
    // Arrange
    std::vector<std::uint8_t> output;

    // Act
    StatusCode result = model->validate_input(Vertex::Scanner::ValueType::Int32, false, EMPTY_STRING, output);

    // Assert
    EXPECT_EQ(StatusCode::STATUS_OK, result);
    EXPECT_TRUE(output.empty());
}

TEST_F(MainModelTest, ValidateInput_ValidDecimalInt32_ReturnsOK)
{
    // Arrange
    std::vector<std::uint8_t> output;

    // Act
    StatusCode result = model->validate_input(Vertex::Scanner::ValueType::Int32, false, "100", output);

    // Assert
    EXPECT_EQ(StatusCode::STATUS_OK, result);
    EXPECT_EQ(4, output.size());
}

TEST_F(MainModelTest, ValidateInput_InvalidInput_ReturnsError)
{
    // Arrange
    std::vector<std::uint8_t> output;

    // Act
    StatusCode result = model->validate_input(Vertex::Scanner::ValueType::Int32, false, "not_a_number", output);

    // Assert
    EXPECT_EQ(StatusCode::STATUS_ERROR_INVALID_PARAMETER, result);
}
