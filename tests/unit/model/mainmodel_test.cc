//
// Copyright (C) 2026 PHTNC<>.
// Licensed under GPLv3.0 with Plugin Interface exceptions.
//




#include <gtest/gtest.h>
#include <gmock/gmock.h>
#include <vertex/model/mainmodel.hh>
#include <vertex/scanner/scanner_command.hh>
#include <vertex/utility.hh>
#include "../../mocks/MockISettings.hh"
#include "../../mocks/MockIMemoryScanner.hh"
#include "../../mocks/MockIScannerRuntimeService.hh"
#include "../../mocks/MockILoader.hh"
#include "../../mocks/MockILog.hh"
#include "../../mocks/MockIThreadDispatcher.hh"

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
        mockScannerService = std::make_unique<NiceMock<Vertex::Testing::Mocks::MockIScannerRuntimeService>>();
        mockLoader = std::make_unique<NiceMock<Vertex::Testing::Mocks::MockILoader>>();
        mockLogger = std::make_unique<NiceMock<Vertex::Testing::Mocks::MockILog>>();
        mockDispatcher = std::make_unique<NiceMock<Vertex::Testing::Mocks::MockIThreadDispatcher>>();

        ON_CALL(*mockDispatcher, dispatch(_, _))
            .WillByDefault([](Vertex::Thread::ThreadChannel, std::packaged_task<StatusCode()>&& task)
                -> std::expected<std::future<StatusCode>, StatusCode>
            {
                auto future = task.get_future();
                task();
                return future;
            });

        model = std::make_unique<Vertex::Model::MainModel>(
            *mockSettings,
            *mockScanner,
            *mockScannerService,
            *mockLoader,
            *mockLogger,
            *mockDispatcher
        );
    }

    void TearDown() override
    {
    }

    std::unique_ptr<NiceMock<Vertex::Testing::Mocks::MockISettings>> mockSettings;
    std::unique_ptr<NiceMock<Vertex::Testing::Mocks::MockIMemoryScanner>> mockScanner;
    std::unique_ptr<NiceMock<Vertex::Testing::Mocks::MockIScannerRuntimeService>> mockScannerService;
    std::unique_ptr<NiceMock<Vertex::Testing::Mocks::MockILoader>> mockLoader;
    std::unique_ptr<NiceMock<Vertex::Testing::Mocks::MockILog>> mockLogger;
    std::unique_ptr<NiceMock<Vertex::Testing::Mocks::MockIThreadDispatcher>> mockDispatcher;
    std::unique_ptr<Vertex::Model::MainModel> model;
};



TEST_F(MainModelTest, GetTheme_ReturnsSettingsValue)
{
    
    EXPECT_CALL(*mockSettings, get_int("general.theme", _))
        .WillOnce(Return(2));

    
    Vertex::Theme result = model->get_theme();

    
    EXPECT_EQ(static_cast<Vertex::Theme>(2), result);
}



TEST_F(MainModelTest, StopScan_DelegatesToScanner)
{
    
    EXPECT_CALL(*mockScannerService, send_command(_, _))
        .WillOnce(Return(Vertex::Runtime::CommandId{1}));
    EXPECT_CALL(*mockScannerService, await_result(Vertex::Runtime::CommandId{1}, _))
        .WillOnce(Return(Vertex::Scanner::service::CommandResult{
            .id = 1, .code = StatusCode::STATUS_TIMEOUT}));

    
    const StatusCode result = model->stop_scan();

    
    EXPECT_EQ(StatusCode::STATUS_OK, result);
}

TEST_F(MainModelTest, IsScanComplete_DelegatesToScanner)
{
    
    EXPECT_CALL(*mockScanner, is_scan_complete())
        .WillOnce(Return(true));

    
    const bool result = model->is_scan_complete();

    
    EXPECT_TRUE(result);
}

TEST_F(MainModelTest, UndoScan_DelegatesToScanner)
{
    
    EXPECT_CALL(*mockScannerService, send_command(_, _))
        .WillOnce(Return(Vertex::Runtime::CommandId{1}));
    EXPECT_CALL(*mockScannerService, await_result(Vertex::Runtime::CommandId{1}, _))
        .WillOnce(Return(Vertex::Scanner::service::CommandResult{
            .id = 1, .code = StatusCode::STATUS_TIMEOUT}));

    
    const StatusCode result = model->undo_scan();

    
    EXPECT_EQ(StatusCode::STATUS_OK, result);
}

TEST_F(MainModelTest, CanUndoScan_DelegatesToScanner)
{
    
    EXPECT_CALL(*mockScannerService, can_undo())
        .WillOnce(Return(true));

    
    const bool result = model->can_undo_scan();

    
    EXPECT_TRUE(result);
}



TEST_F(MainModelTest, GetScanProgressCurrent_DelegatesToScanner)
{
    
    EXPECT_CALL(*mockScanner, get_regions_scanned())
        .WillOnce(Return(42));

    
    std::uint64_t result = model->get_scan_progress_current();

    
    EXPECT_EQ(42, result);
}

TEST_F(MainModelTest, GetScanProgressTotal_DelegatesToScanner)
{
    
    EXPECT_CALL(*mockScanner, get_total_regions())
        .WillOnce(Return(100));

    
    std::uint64_t result = model->get_scan_progress_total();

    
    EXPECT_EQ(100, result);
}

TEST_F(MainModelTest, GetScanResultsCount_DelegatesToScanner)
{
    
    EXPECT_CALL(*mockScannerService, results_count())
        .WillOnce(Return(256));

    
    std::uint64_t result = model->get_scan_results_count();

    
    EXPECT_EQ(256, result);
}



TEST_F(MainModelTest, IsProcessOpened_NoPluginLoaded_ReturnsError)
{
    
    EXPECT_CALL(*mockLoader, has_plugin_loaded())
        .WillOnce(Return(StatusCode::STATUS_ERROR_PLUGIN_NOT_ACTIVE));

    EXPECT_CALL(*mockLogger, log_error(_))
        .Times(1);

    
    StatusCode result = model->is_process_opened();

    
    EXPECT_EQ(StatusCode::STATUS_ERROR_PLUGIN_NOT_ACTIVE, result);
}

TEST_F(MainModelTest, IsProcessOpened_PluginNotLoaded_ReturnsError)
{
    
    EXPECT_CALL(*mockLoader, has_plugin_loaded())
        .WillOnce(Return(StatusCode::STATUS_OK));

    Vertex::Runtime::Plugin mockPlugin{*mockLogger};
    

    EXPECT_CALL(*mockLoader, get_active_plugin())
        .WillOnce(Return(std::optional<std::reference_wrapper<Vertex::Runtime::Plugin>>(mockPlugin)));

    EXPECT_CALL(*mockLogger, log_error(_))
        .Times(1);

    
    StatusCode result = model->is_process_opened();

    
    EXPECT_EQ(StatusCode::STATUS_ERROR_PLUGIN_NOT_LOADED, result);
}

TEST_F(MainModelTest, IsProcessOpened_FunctionNotImplemented_ReturnsError)
{
    
    EXPECT_CALL(*mockLoader, has_plugin_loaded())
        .WillOnce(Return(StatusCode::STATUS_OK));

    Vertex::Runtime::Plugin mockPlugin{*mockLogger};
    mockPlugin.set_library(Vertex::Runtime::Library::create_stub());
    mockPlugin.internal_vertex_process_is_valid = nullptr;

    EXPECT_CALL(*mockLoader, get_active_plugin())
        .WillRepeatedly(Return(std::optional<std::reference_wrapper<Vertex::Runtime::Plugin>>(mockPlugin)));

    EXPECT_CALL(*mockLogger, log_error(_))
        .Times(::testing::AtLeast(1));

    
    StatusCode result = model->is_process_opened();

    
    EXPECT_EQ(StatusCode::STATUS_ERROR_PLUGIN_FUNCTION_NOT_IMPLEMENTED, result);
}

TEST_F(MainModelTest, IsProcessOpened_ValidPlugin_CallsPluginFunction)
{
    
    EXPECT_CALL(*mockLoader, has_plugin_loaded())
        .WillOnce(Return(StatusCode::STATUS_OK));

    Vertex::Runtime::Plugin mockPlugin{*mockLogger};
    mockPlugin.set_library(Vertex::Runtime::Library::create_stub());
    mockPlugin.internal_vertex_process_is_valid = []() {
        return StatusCode::STATUS_OK;
    };

    EXPECT_CALL(*mockLoader, get_active_plugin())
        .WillRepeatedly(Return(std::optional<std::reference_wrapper<Vertex::Runtime::Plugin>>(mockPlugin)));

    
    StatusCode result = model->is_process_opened();

    
    EXPECT_EQ(StatusCode::STATUS_OK, result);
}

TEST_F(MainModelTest, KillProcess_ValidPlugin_CallsPluginFunction)
{
    
    EXPECT_CALL(*mockLoader, has_plugin_loaded())
        .WillRepeatedly(Return(StatusCode::STATUS_OK));

    Vertex::Runtime::Plugin mockPlugin{*mockLogger};
    mockPlugin.set_library(Vertex::Runtime::Library::create_stub());
    mockPlugin.internal_vertex_process_kill = []() {
        return StatusCode::STATUS_OK;
    };

    EXPECT_CALL(*mockLoader, get_active_plugin())
        .WillRepeatedly(Return(std::optional<std::reference_wrapper<Vertex::Runtime::Plugin>>(mockPlugin)));

    
    StatusCode result = model->kill_process();

    
    EXPECT_EQ(StatusCode::STATUS_OK, result);
}

TEST_F(MainModelTest, KillProcess_NoPluginLoaded_ReturnsError)
{
    
    EXPECT_CALL(*mockLoader, has_plugin_loaded())
        .WillOnce(Return(StatusCode::STATUS_ERROR_PLUGIN_NOT_ACTIVE));

    EXPECT_CALL(*mockLogger, log_error(_))
        .Times(1);

    
    StatusCode result = model->kill_process();

    
    EXPECT_EQ(StatusCode::STATUS_ERROR_PLUGIN_NOT_ACTIVE, result);
}



TEST_F(MainModelTest, ValidateInput_EmptyInput_ReturnsOK)
{
    
    std::vector<std::uint8_t> output;

    
    StatusCode result = model->validate_input(Vertex::Scanner::ValueType::Int32, false, Vertex::EMPTY_STRING, output);

    
    EXPECT_EQ(StatusCode::STATUS_OK, result);
    EXPECT_TRUE(output.empty());
}

TEST_F(MainModelTest, ValidateInput_ValidDecimalInt32_ReturnsOK)
{
    
    std::vector<std::uint8_t> output;

    
    StatusCode result = model->validate_input(Vertex::Scanner::ValueType::Int32, false, "100", output);

    
    EXPECT_EQ(StatusCode::STATUS_OK, result);
    EXPECT_EQ(4, output.size());
}

TEST_F(MainModelTest, ValidateInput_InvalidInput_ReturnsError)
{
    
    std::vector<std::uint8_t> output;

    
    StatusCode result = model->validate_input(Vertex::Scanner::ValueType::Int32, false, "not_a_number", output);

    
    EXPECT_EQ(StatusCode::STATUS_ERROR_INVALID_PARAMETER, result);
}
