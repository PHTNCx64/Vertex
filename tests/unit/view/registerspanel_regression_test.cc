#include <gtest/gtest.h>
#include <gmock/gmock.h>
#include <vertex/view/debugger/registerspanel.hh>
#include <vertex/debugger/debuggertypes.hh>
#include "../../mocks/MockILanguage.hh"
#include "../../mocks/MockIThemeProvider.hh"

#include <wx/app.h>
#include <wx/frame.h>
#include <wx/listctrl.h>
#include <wx/sizer.h>

#include <memory>

using namespace Vertex::View::Debugger;
using namespace Vertex::Testing::Mocks;
using namespace testing;

class RegistersPanelRegressionTest : public ::testing::Test
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
        ON_CALL(*m_language, fetch_translation(_))
            .WillByDefault(ReturnRef(m_emptyString));
        ON_CALL(*m_themeProvider, palette())
            .WillByDefault(ReturnRef(m_defaultPalette));
        ON_CALL(*m_themeProvider, is_dark())
            .WillByDefault(Return(true));

        m_frame = new wxFrame(nullptr, wxID_ANY, "RegistersPanelRegressionTest");
        m_frame->SetClientSize(wxSize(260, 180));

        auto* sizer = new wxBoxSizer(wxVERTICAL);
        m_panel = new RegistersPanel(m_frame, *m_language, *m_themeProvider);
        sizer->Add(m_panel, 1, wxEXPAND);
        m_frame->SetSizer(sizer);
        m_frame->Show();
        m_frame->Layout();

        if (wxTheApp)
        {
            wxTheApp->ProcessPendingEvents();
        }

        m_list = find_list_control();
        ASSERT_NE(m_list, nullptr);
    }

    void TearDown() override
    {
        m_frame->Destroy();
        if (wxTheApp)
        {
            wxTheApp->ProcessPendingEvents();
        }
    }

    static Vertex::Debugger::RegisterSet make_registers(const std::size_t count, const std::uint64_t delta)
    {
        Vertex::Debugger::RegisterSet registers{};
        registers.generalPurpose.reserve(count);

        for (std::size_t i = 0; i < count; ++i)
        {
            Vertex::Debugger::Register reg{};
            reg.name = "R" + std::to_string(i);
            reg.category = Vertex::Debugger::RegisterCategory::General;
            reg.value = 0x1000 + i + delta;
            reg.bitWidth = 64;
            reg.modified = (delta != 0 && (i % 3 == 0));
            registers.generalPurpose.push_back(std::move(reg));
        }

        registers.instructionPointer = 0x401000 + delta;
        registers.stackPointer = 0x7FF000 + delta;
        registers.basePointer = 0x7FF100 + delta;
        return registers;
    }

    wxListCtrl* find_list_control() const
    {
        for (wxWindow* child : m_panel->GetChildren())
        {
            if (auto* list = dynamic_cast<wxListCtrl*>(child))
            {
                return list;
            }
        }
        return nullptr;
    }

    std::unique_ptr<NiceMock<MockILanguage>> m_language;
    std::unique_ptr<NiceMock<MockIThemeProvider>> m_themeProvider;
    wxFrame* m_frame {};
    RegistersPanel* m_panel {};
    wxListCtrl* m_list {};
    std::string m_emptyString;
    Vertex::Gui::ColorPalette m_defaultPalette {};
};

TEST_F(RegistersPanelRegressionTest, RefreshKeepsSelectedRowInFallbackMode)
{
    m_panel->update_registers(make_registers(80, 0));
    m_panel->update_registers(make_registers(80, 1));

    constexpr long selectedIndex = 25;
    m_list->SetItemState(selectedIndex,
        wxLIST_STATE_SELECTED | wxLIST_STATE_FOCUSED,
        wxLIST_STATE_SELECTED | wxLIST_STATE_FOCUSED);

    ASSERT_EQ(m_list->GetNextItem(-1, wxLIST_NEXT_ALL, wxLIST_STATE_SELECTED), selectedIndex);

    m_panel->update_registers(make_registers(80, 2));

    EXPECT_EQ(m_list->GetNextItem(-1, wxLIST_NEXT_ALL, wxLIST_STATE_SELECTED), selectedIndex);
}

TEST_F(RegistersPanelRegressionTest, RefreshDoesNotJumpBackToTopInFallbackMode)
{
    m_panel->update_registers(make_registers(120, 0));
    m_panel->update_registers(make_registers(120, 1));

    m_list->EnsureVisible(100);
    if (wxTheApp)
    {
        wxTheApp->ProcessPendingEvents();
    }

    const long topBefore = m_list->GetTopItem();
    ASSERT_GT(topBefore, 0);

    m_panel->update_registers(make_registers(120, 2));

    const long topAfter = m_list->GetTopItem();
    EXPECT_GT(topAfter, 0);
}
