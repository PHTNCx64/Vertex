#include <gtest/gtest.h>
#include <gmock/gmock.h>
#include <vertex/model/debuggermodel.hh>
#include <vertex/runtime/plugin.hh>
#include "../../mocks/MockISettings.hh"
#include "../../mocks/MockILoader.hh"
#include "../../mocks/MockILog.hh"
#include "../../mocks/MockIThreadDispatcher.hh"

#include <wx/app.h>

#include <expected>
#include <functional>
#include <future>
#include <optional>
#include <vector>

using namespace Vertex::Debugger;
using namespace Vertex::Model;
using namespace Vertex::Testing::Mocks;
using namespace Vertex::Thread;
using namespace testing;

namespace
{
    StatusCode VERTEX_API stub_disasm_empty(
        [[maybe_unused]] std::uint64_t address,
        [[maybe_unused]] std::uint32_t size,
        DisassemblerResults* results)
    {
        results->count = 0;
        return STATUS_OK;
    }

    StatusCode VERTEX_API stub_disasm_fail(
        [[maybe_unused]] std::uint64_t address,
        [[maybe_unused]] std::uint32_t size,
        [[maybe_unused]] DisassemblerResults* results)
    {
        return STATUS_ERROR_GENERAL;
    }

    StatusCode VERTEX_API stub_disasm_one_instruction(
        std::uint64_t address,
        [[maybe_unused]] std::uint32_t size,
        DisassemblerResults* results)
    {
        if (results->capacity > 0)
        {
            results->results[0].address = address;
            results->results[0].size = 1;
            results->results[0].mnemonic[0] = 'n';
            results->results[0].mnemonic[1] = 'o';
            results->results[0].mnemonic[2] = 'p';
            results->results[0].mnemonic[3] = '\0';
            results->results[0].operands[0] = '\0';
            results->count = 1;
        }
        return STATUS_OK;
    }
}

class DebuggerModelExtensionTest : public ::testing::Test
{
protected:
    static void SetUpTestSuite()
    {
        if (!wxTheApp)
        {
            wxInitialize();
        }
    }

    static void TearDownTestSuite()
    {
        wxUninitialize();
    }

    void SetUp() override
    {
        m_settings = std::make_unique<NiceMock<MockISettings>>();
        m_loader = std::make_unique<NiceMock<MockILoader>>();
        m_logger = std::make_unique<NiceMock<MockILog>>();
        m_dispatcher = std::make_unique<NiceMock<MockIThreadDispatcher>>();

        ON_CALL(*m_dispatcher, dispatch_with_priority(_, _, _))
            .WillByDefault(Invoke(
                []([[maybe_unused]] ThreadChannel ch,
                   [[maybe_unused]] DispatchPriority prio,
                   std::packaged_task<StatusCode()>&& task)
                    -> std::expected<std::future<StatusCode>, StatusCode>
                {
                    auto future = task.get_future();
                    task();
                    return future;
                }));

        ON_CALL(*m_dispatcher, dispatch(_, _))
            .WillByDefault(Invoke(
                []([[maybe_unused]] ThreadChannel ch,
                   std::packaged_task<StatusCode()>&& task)
                    -> std::expected<std::future<StatusCode>, StatusCode>
                {
                    auto future = task.get_future();
                    task();
                    return future;
                }));

        ON_CALL(*m_dispatcher, schedule_recurring(_, _, _, _, _, _))
            .WillByDefault(Return(std::expected<RecurringTaskHandle, StatusCode>{RecurringTaskHandle{1}}));

        ON_CALL(*m_dispatcher, cancel_recurring(_))
            .WillByDefault(Return(STATUS_OK));

        m_model = std::make_unique<DebuggerModel>(
            *m_settings, *m_loader, *m_logger, *m_dispatcher);
    }

    void TearDown() override
    {
        m_model.reset();
    }

    void flush_pending_wx_events() const
    {
        if (wxTheApp)
        {
            wxTheApp->ProcessPendingEvents();
        }
    }

    void setup_extension_result_capture()
    {
        m_model->set_extension_result_handler(
            [this](const bool isTop, const ExtensionResult result)
            {
                m_capturedIsTop = isTop;
                m_capturedResult = result;
                m_resultReceived = true;
            });
    }

    void setup_plugin(Vertex::Runtime::Plugin& plugin) const
    {
        ON_CALL(*m_loader, has_plugin_loaded())
            .WillByDefault(Return(STATUS_OK));
        ON_CALL(*m_loader, get_active_plugin())
            .WillByDefault(Return(std::optional<std::reference_wrapper<Vertex::Runtime::Plugin>>{std::ref(plugin)}));
    }

    void setup_no_plugin() const
    {
        ON_CALL(*m_loader, has_plugin_loaded())
            .WillByDefault(Return(STATUS_ERROR_PLUGIN_NOT_LOADED));
    }

    std::unique_ptr<NiceMock<MockISettings>> m_settings;
    std::unique_ptr<NiceMock<MockILoader>> m_loader;
    std::unique_ptr<NiceMock<MockILog>> m_logger;
    std::unique_ptr<NiceMock<MockIThreadDispatcher>> m_dispatcher;
    std::unique_ptr<DebuggerModel> m_model;

    bool m_resultReceived {};
    bool m_capturedIsTop {};
    ExtensionResult m_capturedResult {};
};

TEST_F(DebuggerModelExtensionTest, ExtendUpNoPluginReportsError)
{
    setup_no_plugin();
    setup_extension_result_capture();

    m_model->request_disassembly_extend_up(0x1000);
    flush_pending_wx_events();

    EXPECT_TRUE(m_resultReceived);
    EXPECT_TRUE(m_capturedIsTop);
    EXPECT_EQ(m_capturedResult, ExtensionResult::Error);
}

TEST_F(DebuggerModelExtensionTest, ExtendDownNoPluginReportsError)
{
    setup_no_plugin();
    setup_extension_result_capture();

    m_model->request_disassembly_extend_down(0x1000);
    flush_pending_wx_events();

    EXPECT_TRUE(m_resultReceived);
    EXPECT_FALSE(m_capturedIsTop);
    EXPECT_EQ(m_capturedResult, ExtensionResult::Error);
}

TEST_F(DebuggerModelExtensionTest, ExtendUpBoundaryExhaustedReportsEndOfRange)
{
    ON_CALL(*m_loader, has_plugin_loaded())
        .WillByDefault(Return(STATUS_OK));
    setup_extension_result_capture();

    m_model->request_disassembly_extend_up(0, 512);
    flush_pending_wx_events();

    EXPECT_TRUE(m_resultReceived);
    EXPECT_TRUE(m_capturedIsTop);
    EXPECT_EQ(m_capturedResult, ExtensionResult::EndOfRange);
}

TEST_F(DebuggerModelExtensionTest, ExtendUpDisasmFailureReportsError)
{
    Vertex::Runtime::Plugin plugin{*m_logger};
    plugin.internal_vertex_process_disassemble_range = stub_disasm_fail;
    setup_plugin(plugin);
    setup_extension_result_capture();

    m_model->request_disassembly_extend_up(0x1000);
    flush_pending_wx_events();

    EXPECT_TRUE(m_resultReceived);
    EXPECT_TRUE(m_capturedIsTop);
    EXPECT_EQ(m_capturedResult, ExtensionResult::Error);
}

TEST_F(DebuggerModelExtensionTest, ExtendDownDisasmFailureReportsError)
{
    Vertex::Runtime::Plugin plugin{*m_logger};
    plugin.internal_vertex_process_disassemble_range = stub_disasm_fail;
    setup_plugin(plugin);
    setup_extension_result_capture();

    m_model->request_disassembly_extend_down(0x1000);
    flush_pending_wx_events();

    EXPECT_TRUE(m_resultReceived);
    EXPECT_FALSE(m_capturedIsTop);
    EXPECT_EQ(m_capturedResult, ExtensionResult::Error);
}

TEST_F(DebuggerModelExtensionTest, ExtendUpZeroResultsReportsEndOfRange)
{
    Vertex::Runtime::Plugin plugin{*m_logger};
    plugin.internal_vertex_process_disassemble_range = stub_disasm_empty;
    setup_plugin(plugin);
    setup_extension_result_capture();

    m_model->request_disassembly_extend_up(0x1000);
    flush_pending_wx_events();

    EXPECT_TRUE(m_resultReceived);
    EXPECT_TRUE(m_capturedIsTop);
    EXPECT_EQ(m_capturedResult, ExtensionResult::EndOfRange);
}

TEST_F(DebuggerModelExtensionTest, ExtendDownZeroResultsReportsEndOfRange)
{
    Vertex::Runtime::Plugin plugin{*m_logger};
    plugin.internal_vertex_process_disassemble_range = stub_disasm_empty;
    setup_plugin(plugin);
    setup_extension_result_capture();

    m_model->request_disassembly_extend_down(0x1000);
    flush_pending_wx_events();

    EXPECT_TRUE(m_resultReceived);
    EXPECT_FALSE(m_capturedIsTop);
    EXPECT_EQ(m_capturedResult, ExtensionResult::EndOfRange);
}

TEST_F(DebuggerModelExtensionTest, ExtendUpSuccessfulDisasmReportsSuccess)
{
    Vertex::Runtime::Plugin plugin{*m_logger};
    plugin.internal_vertex_process_disassemble_range = stub_disasm_one_instruction;
    setup_plugin(plugin);
    setup_extension_result_capture();

    m_model->request_disassembly_extend_up(0x1000, 512);
    flush_pending_wx_events();

    EXPECT_TRUE(m_resultReceived);
    EXPECT_TRUE(m_capturedIsTop);
    EXPECT_EQ(m_capturedResult, ExtensionResult::Success);
}

TEST_F(DebuggerModelExtensionTest, ExtendDownSuccessfulDisasmReportsSuccess)
{
    Vertex::Runtime::Plugin plugin{*m_logger};
    plugin.internal_vertex_process_disassemble_range = stub_disasm_one_instruction;
    setup_plugin(plugin);
    setup_extension_result_capture();

    m_model->request_disassembly_extend_down(0x1000, 512);
    flush_pending_wx_events();

    EXPECT_TRUE(m_resultReceived);
    EXPECT_FALSE(m_capturedIsTop);
    EXPECT_EQ(m_capturedResult, ExtensionResult::Success);
}

TEST_F(DebuggerModelExtensionTest, ExtendUpNullFunctionPointerReportsError)
{
    Vertex::Runtime::Plugin plugin{*m_logger};
    plugin.internal_vertex_process_disassemble_range = nullptr;
    setup_plugin(plugin);
    setup_extension_result_capture();

    m_model->request_disassembly_extend_up(0x1000);
    flush_pending_wx_events();

    EXPECT_TRUE(m_resultReceived);
    EXPECT_TRUE(m_capturedIsTop);
    EXPECT_EQ(m_capturedResult, ExtensionResult::Error);
}

TEST_F(DebuggerModelExtensionTest, ExtendDownNullFunctionPointerReportsError)
{
    Vertex::Runtime::Plugin plugin{*m_logger};
    plugin.internal_vertex_process_disassemble_range = nullptr;
    setup_plugin(plugin);
    setup_extension_result_capture();

    m_model->request_disassembly_extend_down(0x1000);
    flush_pending_wx_events();

    EXPECT_TRUE(m_resultReceived);
    EXPECT_FALSE(m_capturedIsTop);
    EXPECT_EQ(m_capturedResult, ExtensionResult::Error);
}

TEST_F(DebuggerModelExtensionTest, ExtendUpNoActivePluginReportsError)
{
    ON_CALL(*m_loader, has_plugin_loaded())
        .WillByDefault(Return(STATUS_OK));
    ON_CALL(*m_loader, get_active_plugin())
        .WillByDefault(Return(std::nullopt));
    setup_extension_result_capture();

    m_model->request_disassembly_extend_up(0x1000);
    flush_pending_wx_events();

    EXPECT_TRUE(m_resultReceived);
    EXPECT_TRUE(m_capturedIsTop);
    EXPECT_EQ(m_capturedResult, ExtensionResult::Error);
}

TEST_F(DebuggerModelExtensionTest, ExtendDownNoActivePluginReportsError)
{
    ON_CALL(*m_loader, has_plugin_loaded())
        .WillByDefault(Return(STATUS_OK));
    ON_CALL(*m_loader, get_active_plugin())
        .WillByDefault(Return(std::nullopt));
    setup_extension_result_capture();

    m_model->request_disassembly_extend_down(0x1000);
    flush_pending_wx_events();

    EXPECT_TRUE(m_resultReceived);
    EXPECT_FALSE(m_capturedIsTop);
    EXPECT_EQ(m_capturedResult, ExtensionResult::Error);
}

TEST_F(DebuggerModelExtensionTest, ExtendUpDispatchFailureReportsError)
{
    Vertex::Runtime::Plugin plugin{*m_logger};
    plugin.internal_vertex_process_disassemble_range = stub_disasm_one_instruction;
    setup_plugin(plugin);
    setup_extension_result_capture();

    ON_CALL(*m_dispatcher, dispatch_with_priority(_, _, _))
        .WillByDefault(Invoke(
            []([[maybe_unused]] ThreadChannel ch,
               [[maybe_unused]] DispatchPriority prio,
               [[maybe_unused]] std::packaged_task<StatusCode()>&& task)
                -> std::expected<std::future<StatusCode>, StatusCode>
            {
                return std::unexpected{StatusCode::STATUS_ERROR_GENERAL};
            }));

    m_model->request_disassembly_extend_up(0x1000, 512);
    flush_pending_wx_events();

    EXPECT_TRUE(m_resultReceived);
    EXPECT_TRUE(m_capturedIsTop);
    EXPECT_EQ(m_capturedResult, ExtensionResult::Error);
}

TEST_F(DebuggerModelExtensionTest, ExtendDownDispatchFailureReportsError)
{
    Vertex::Runtime::Plugin plugin{*m_logger};
    plugin.internal_vertex_process_disassemble_range = stub_disasm_one_instruction;
    setup_plugin(plugin);
    setup_extension_result_capture();

    ON_CALL(*m_dispatcher, dispatch_with_priority(_, _, _))
        .WillByDefault(Invoke(
            []([[maybe_unused]] ThreadChannel ch,
               [[maybe_unused]] DispatchPriority prio,
               [[maybe_unused]] std::packaged_task<StatusCode()>&& task)
                -> std::expected<std::future<StatusCode>, StatusCode>
            {
                return std::unexpected{StatusCode::STATUS_ERROR_GENERAL};
            }));

    m_model->request_disassembly_extend_down(0x1000, 512);
    flush_pending_wx_events();

    EXPECT_TRUE(m_resultReceived);
    EXPECT_FALSE(m_capturedIsTop);
    EXPECT_EQ(m_capturedResult, ExtensionResult::Error);
}
