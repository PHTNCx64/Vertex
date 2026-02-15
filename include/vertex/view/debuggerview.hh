//
// Copyright (C) 2026 PHTNC<>.
// Licensed under GPLv3.0 with Plugin Interface exceptions.
//
#pragma once

#include <wx/frame.h>
#include <wx/panel.h>
#include <wx/sizer.h>
#include <wx/listctrl.h>
#include <wx/textctrl.h>
#include <wx/button.h>
#include <wx/menu.h>
#include <wx/aui/aui.h>
#include <wx/timer.h>
#include <wx/stattext.h>

#include <memory>
#include <vertex/viewmodel/debuggerviewmodel.hh>
#include <vertex/language/language.hh>
#include <vertex/gui/iconmanager/iconmanager.hh>
#include <vertex/event/eventid.hh>
#include <vertex/event/vertexevent.hh>

namespace Vertex::View::Debugger
{
    class DisassemblyPanel;
    class BreakpointsPanel;
    class WatchpointsPanel;
    class ImportExportPanel;
    class RegistersPanel;
    class StackPanel;
    class MemoryPanel;
    class HexEditorPanel;
    class ThreadsPanel;
    class WatchPanel;
    class ConsolePanel;
}

namespace Vertex::View
{
    class DebuggerView final : public wxFrame
    {
    public:
        DebuggerView(
            const wxString& title,
            std::unique_ptr<ViewModel::DebuggerViewModel> viewModel,
            Language::ILanguage& languageService,
            Gui::IIconManager& iconManager
        );

        ~DebuggerView() override;

        void navigate_to_address(std::uint64_t address);
        void set_watchpoint(std::uint64_t address, std::uint32_t size);

    private:
        enum MenuIds
        {
            ID_ATTACH = 7001,
            ID_DETACH,
            ID_RESTART,
            ID_CONTINUE,
            ID_PAUSE,
            ID_STEP_INTO,
            ID_STEP_OVER,
            ID_STEP_OUT,
            ID_RUN_TO_CURSOR,
            ID_TOGGLE_BREAKPOINT,
            ID_GOTO_ADDRESS,
            ID_VIEW_DISASSEMBLY,
            ID_VIEW_BREAKPOINTS,
            ID_VIEW_WATCHPOINTS,
            ID_VIEW_REGISTERS,
            ID_VIEW_STACK,
            ID_VIEW_MEMORY,
            ID_VIEW_HEX_EDITOR,
            ID_VIEW_IMPORTS_EXPORTS,
            ID_VIEW_THREADS,
            ID_VIEW_WATCH,
            ID_VIEW_CONSOLE,
        };

        enum StatusBarFields
        {
            FIELD_STATE = 0,
            FIELD_ADDRESS,
            FIELD_THREAD,
            FIELD_INFO,
            FIELD_COUNT
        };

        void create_controls();
        void create_menu_bar();
        void create_toolbar();
        void create_status_bar();
        void create_panels();
        void layout_controls();
        void bind_events();
        void setup_aui_layout();
        void apply_pane_captions();
        void setup_panel_callbacks();

        void vertex_event_callback(Event::EventId eventId, const Event::VertexEvent& event);
        void update_view(ViewUpdateFlags flags = ViewUpdateFlags::DEBUGGER_ALL);
        void update_toolbar_state();
        void update_status_bar();
        [[nodiscard]] bool toggle_view();

        void on_attach_clicked(wxCommandEvent& event);
        void on_detach_clicked(wxCommandEvent& event);
        void on_continue_clicked(wxCommandEvent& event);
        void on_pause_clicked(wxCommandEvent& event);
        void on_step_into_clicked(wxCommandEvent& event);
        void on_step_over_clicked(wxCommandEvent& event);
        void on_step_out_clicked(wxCommandEvent& event);
        void on_run_to_cursor_clicked(wxCommandEvent& event);
        void on_toggle_breakpoint_clicked(wxCommandEvent& event);
        void on_goto_address_clicked(wxCommandEvent& event);

        void on_view_pane_toggle(const wxCommandEvent& event);

        void on_refresh_timer(wxTimerEvent& event);

        void on_close(wxCloseEvent& event);

        void on_show(wxShowEvent& event);

        wxAuiManager m_auiManager{};

        wxAuiToolBar* m_toolbar{};

        wxMenuBar* m_menuBar{};
        wxMenu* m_debugMenu{};
        wxMenu* m_viewMenu{};

        Debugger::DisassemblyPanel* m_disassemblyPanel{};
        Debugger::BreakpointsPanel* m_breakpointsPanel{};
        Debugger::WatchpointsPanel* m_watchpointsPanel{};
        Debugger::ImportExportPanel* m_importExportPanel{};
        Debugger::RegistersPanel* m_registersPanel{};
        Debugger::StackPanel* m_stackPanel{};
        Debugger::MemoryPanel* m_memoryPanel{};
        Debugger::HexEditorPanel* m_hexEditorPanel{};
        Debugger::ThreadsPanel* m_threadsPanel{};
        Debugger::WatchPanel* m_watchPanel{};
        Debugger::ConsolePanel* m_consolePanel{};

        wxPanel* m_statusPanel{};
        wxStaticText* m_stateText{};
        wxStaticText* m_addressText{};
        wxStaticText* m_threadText{};
        wxStaticText* m_infoText{};

        wxTimer* m_refreshTimer{};

        std::unique_ptr<ViewModel::DebuggerViewModel> m_viewModel{};
        Language::ILanguage& m_languageService;
        Gui::IIconManager& m_iconManager;

        ViewUpdateFlags m_pendingUpdateFlags{ViewUpdateFlags::NONE};
        bool m_hasPendingUpdate{};

        std::uint64_t m_lastHighlightedAddress{};

        ::Vertex::Debugger::DebuggerState m_lastState{::Vertex::Debugger::DebuggerState::Detached};
    };
}
