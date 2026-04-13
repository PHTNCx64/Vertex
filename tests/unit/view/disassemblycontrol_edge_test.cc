#include <gtest/gtest.h>
#include <gmock/gmock.h>
#include <vertex/view/debugger/disassemblycontrol.hh>
#include <vertex/debugger/debuggertypes.hh>
#include "../../mocks/MockILanguage.hh"
#include "../../mocks/MockIThemeProvider.hh"

#include <wx/app.h>
#include <wx/frame.h>

using namespace Vertex::View::Debugger;
using namespace Vertex::Debugger;
using namespace Vertex::Testing::Mocks;
using namespace testing;

class DisassemblyControlEdgeTest : public ::testing::Test
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
        m_language = std::make_unique<NiceMock<MockILanguage>>();
        m_themeProvider = std::make_unique<NiceMock<MockIThemeProvider>>();
        m_emptyString = "";
        ON_CALL(*m_language, fetch_translation(_))
            .WillByDefault(ReturnRef(m_emptyString));
        ON_CALL(*m_themeProvider, palette())
            .WillByDefault(ReturnRef(m_defaultPalette));
        ON_CALL(*m_themeProvider, is_dark())
            .WillByDefault(Return(true));

        m_frame = new wxFrame(nullptr, wxID_ANY, "Test");
        m_control = new DisassemblyControl(m_frame, *m_language, *m_themeProvider);
    }

    void TearDown() override
    {
        m_frame->Destroy();
        if (wxTheApp)
        {
            wxTheApp->ProcessPendingEvents();
        }
    }

    void populate_range(std::uint64_t startAddress, std::size_t lineCount)
    {
        DisassemblyRange range{};
        range.startAddress = startAddress;
        range.lines.reserve(lineCount);
        for (std::size_t i = 0; i < lineCount; ++i)
        {
            DisassemblyLine line{};
            line.address = startAddress + i;
            line.bytes = {0x90};
            line.mnemonic = "nop";
            range.lines.push_back(std::move(line));
        }
        if (!range.lines.empty())
        {
            const auto& last = range.lines.back();
            range.endAddress = last.address + last.bytes.size();
        }
        m_control->set_disassembly(range);
    }

    std::unique_ptr<NiceMock<MockILanguage>> m_language;
    std::unique_ptr<NiceMock<MockIThemeProvider>> m_themeProvider;
    wxFrame* m_frame {};
    DisassemblyControl* m_control {};
    std::string m_emptyString;
    Vertex::Gui::ColorPalette m_defaultPalette {};
};

TEST_F(DisassemblyControlEdgeTest, InitialEdgeStatesAreIdle)
{
    EXPECT_EQ(m_control->get_top_edge_state(), DisassemblyControl::EdgeState::Idle);
    EXPECT_EQ(m_control->get_bottom_edge_state(), DisassemblyControl::EdgeState::Idle);
    EXPECT_FALSE(m_control->is_fetching_more());
    EXPECT_FALSE(m_control->is_loading_timer_running());
}

TEST_F(DisassemblyControlEdgeTest, SetExtensionResultSuccessSetsIdle)
{
    m_control->set_extension_result(true, ExtensionResult::Success);

    EXPECT_EQ(m_control->get_top_edge_state(), DisassemblyControl::EdgeState::Idle);
    EXPECT_FALSE(m_control->is_fetching_more());
}

TEST_F(DisassemblyControlEdgeTest, SetExtensionResultEndOfRangeSetsEndOfRange)
{
    m_control->set_extension_result(true, ExtensionResult::EndOfRange);

    EXPECT_EQ(m_control->get_top_edge_state(), DisassemblyControl::EdgeState::EndOfRange);
    EXPECT_FALSE(m_control->is_fetching_more());
}

TEST_F(DisassemblyControlEdgeTest, SetExtensionResultErrorSetsError)
{
    m_control->set_extension_result(true, ExtensionResult::Error);

    EXPECT_EQ(m_control->get_top_edge_state(), DisassemblyControl::EdgeState::Error);
    EXPECT_FALSE(m_control->is_fetching_more());
}

TEST_F(DisassemblyControlEdgeTest, SetExtensionResultBottomEdge)
{
    m_control->set_extension_result(false, ExtensionResult::EndOfRange);

    EXPECT_EQ(m_control->get_top_edge_state(), DisassemblyControl::EdgeState::Idle);
    EXPECT_EQ(m_control->get_bottom_edge_state(), DisassemblyControl::EdgeState::EndOfRange);
}

TEST_F(DisassemblyControlEdgeTest, SetExtensionResultClearsFetchingMore)
{
    populate_range(0x1000, 50);

    bool callbackFired {};
    m_control->set_scroll_boundary_callback(
        [&callbackFired]([[maybe_unused]] std::uint64_t addr, [[maybe_unused]] bool isTop)
        {
            callbackFired = true;
        });

    m_control->set_extension_result(true, ExtensionResult::Error);

    EXPECT_FALSE(m_control->is_fetching_more());
}

TEST_F(DisassemblyControlEdgeTest, TimerStopsWhenNeitherEdgeLoading)
{
    m_control->set_extension_result(true, ExtensionResult::Error);

    EXPECT_FALSE(m_control->is_loading_timer_running());
}

TEST_F(DisassemblyControlEdgeTest, FullReloadResetsEdgesToIdle)
{
    m_control->set_extension_result(true, ExtensionResult::EndOfRange);
    m_control->set_extension_result(false, ExtensionResult::Error);

    EXPECT_EQ(m_control->get_top_edge_state(), DisassemblyControl::EdgeState::EndOfRange);
    EXPECT_EQ(m_control->get_bottom_edge_state(), DisassemblyControl::EdgeState::Error);

    populate_range(0x2000, 50);

    EXPECT_EQ(m_control->get_top_edge_state(), DisassemblyControl::EdgeState::Idle);
    EXPECT_EQ(m_control->get_bottom_edge_state(), DisassemblyControl::EdgeState::Idle);
}

TEST_F(DisassemblyControlEdgeTest, EmptyRangeResetsEdgesToIdle)
{
    m_control->set_extension_result(true, ExtensionResult::Error);
    m_control->set_extension_result(false, ExtensionResult::EndOfRange);

    DisassemblyRange emptyRange{};
    m_control->set_disassembly(emptyRange);

    EXPECT_EQ(m_control->get_top_edge_state(), DisassemblyControl::EdgeState::Idle);
    EXPECT_EQ(m_control->get_bottom_edge_state(), DisassemblyControl::EdgeState::Idle);
}

TEST_F(DisassemblyControlEdgeTest, RetryExtensionDispatchesCorrectEdge)
{
    populate_range(0x1000, 50);

    std::uint64_t capturedAddress {};
    bool capturedIsTop {};
    bool callbackFired {};
    m_control->set_scroll_boundary_callback(
        [&](std::uint64_t addr, bool isTop)
        {
            capturedAddress = addr;
            capturedIsTop = isTop;
            callbackFired = true;
        });

    m_control->set_extension_result(true, ExtensionResult::Error);
    EXPECT_EQ(m_control->get_top_edge_state(), DisassemblyControl::EdgeState::Error);

    wxMouseEvent clickEvent(wxEVT_LEFT_DOWN);
    clickEvent.SetPosition(wxPoint(100, 2));
    m_control->GetEventHandler()->ProcessEvent(clickEvent);

    EXPECT_TRUE(callbackFired);
    EXPECT_TRUE(capturedIsTop);
    EXPECT_EQ(capturedAddress, static_cast<std::uint64_t>(0x1000));
    EXPECT_EQ(m_control->get_top_edge_state(), DisassemblyControl::EdgeState::Loading);
}

TEST_F(DisassemblyControlEdgeTest, RetryBottomExtensionDispatchesCorrectEdge)
{
    populate_range(0x1000, 50);

    std::uint64_t capturedAddress {};
    bool capturedIsTop {true};
    bool callbackFired {};
    m_control->set_scroll_boundary_callback(
        [&](const std::uint64_t addr, const bool isTop)
        {
            capturedAddress = addr;
            capturedIsTop = isTop;
            callbackFired = true;
        });

    m_control->set_extension_result(false, ExtensionResult::Error);
    EXPECT_EQ(m_control->get_bottom_edge_state(), DisassemblyControl::EdgeState::Error);

    const int clientHeight = m_control->GetClientSize().GetHeight();
    wxMouseEvent clickEvent(wxEVT_LEFT_DOWN);
    clickEvent.SetPosition(wxPoint(100, clientHeight - 2));
    m_control->GetEventHandler()->ProcessEvent(clickEvent);

    EXPECT_TRUE(callbackFired);
    EXPECT_FALSE(capturedIsTop);
    EXPECT_EQ(m_control->get_bottom_edge_state(), DisassemblyControl::EdgeState::Loading);
}

TEST_F(DisassemblyControlEdgeTest, ClickOnTopErrorDoesNotAffectBottomEdge)
{
    populate_range(0x1000, 50);

    m_control->set_scroll_boundary_callback(
        []([[maybe_unused]] std::uint64_t addr, [[maybe_unused]] bool isTop) {});

    m_control->set_extension_result(true, ExtensionResult::Error);
    m_control->set_extension_result(false, ExtensionResult::EndOfRange);

    wxMouseEvent clickEvent(wxEVT_LEFT_DOWN);
    clickEvent.SetPosition(wxPoint(100, 2));
    m_control->GetEventHandler()->ProcessEvent(clickEvent);

    EXPECT_EQ(m_control->get_top_edge_state(), DisassemblyControl::EdgeState::Loading);
    EXPECT_EQ(m_control->get_bottom_edge_state(), DisassemblyControl::EdgeState::EndOfRange);
}

TEST_F(DisassemblyControlEdgeTest, ClickOnIdleTopDoesNotTriggerRetry)
{
    populate_range(0x1000, 50);

    bool callbackFired {};
    m_control->set_scroll_boundary_callback(
        [&callbackFired]([[maybe_unused]] std::uint64_t addr, [[maybe_unused]] bool isTop)
        {
            callbackFired = true;
        });

    wxMouseEvent clickEvent(wxEVT_LEFT_DOWN);
    clickEvent.SetPosition(wxPoint(100, 2));
    m_control->GetEventHandler()->ProcessEvent(clickEvent);

    EXPECT_FALSE(callbackFired);
}

TEST_F(DisassemblyControlEdgeTest, TopAddressZeroSetsEndOfRangeImmediately)
{
    populate_range(0, 50);

    m_control->set_scroll_boundary_callback(
        []([[maybe_unused]] std::uint64_t addr, [[maybe_unused]] bool isTop) {});

    m_control->Scroll(0, 0);
    wxScrollWinEvent scrollEvent(wxEVT_SCROLLWIN_TOP);
    m_control->GetEventHandler()->ProcessEvent(scrollEvent);

    EXPECT_EQ(m_control->get_top_edge_state(), DisassemblyControl::EdgeState::EndOfRange);
}
