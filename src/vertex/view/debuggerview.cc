//
// Copyright (C) 2026 PHTNC<>.
// Licensed under GPLv3.0 with Plugin Interface exceptions.
//
#include <vertex/view/debuggerview.hh>
#include <vertex/view/debugger/disassemblypanel.hh>
#include <vertex/view/debugger/breakpointspanel.hh>
#include <vertex/view/debugger/watchpointspanel.hh>
#include <vertex/view/debugger/importexportpanel.hh>
#include <vertex/view/debugger/registerspanel.hh>
#include <vertex/view/debugger/stackpanel.hh>
#include <vertex/view/debugger/memorypanel.hh>
#include <vertex/view/debugger/hexeditorpanel.hh>
#include <vertex/view/debugger/threadspanel.hh>
#include <vertex/view/debugger/watchpanel.hh>
#include <vertex/view/debugger/consolepanel.hh>
#include <vertex/event/types/viewupdateevent.hh>
#include <vertex/utility.hh>

#include <wx/msgdlg.h>
#include <wx/textdlg.h>
#include <fmt/format.h>

#include <algorithm>
#include <array>
#include <utility>

namespace
{
    constexpr bool is_paused_state(const Vertex::Debugger::DebuggerState state)
    {
        return state == Vertex::Debugger::DebuggerState::Paused ||
               state == Vertex::Debugger::DebuggerState::BreakpointHit ||
               state == Vertex::Debugger::DebuggerState::Stepping ||
               state == Vertex::Debugger::DebuggerState::Exception;
    }
}

namespace Vertex::View
{
    DebuggerView::DebuggerView(
        const wxString& title,
        std::unique_ptr<ViewModel::DebuggerViewModel> viewModel,
        Language::ILanguage& languageService,
        Gui::IIconManager& iconManager
    )
        : wxFrame(nullptr, wxID_ANY, title,
                  wxDefaultPosition,
                  wxSize(StandardWidgetValues::STANDARD_X_DIP, StandardWidgetValues::STANDARD_Y_DIP))
        , m_viewModel(std::move(viewModel))
        , m_languageService(languageService)
        , m_iconManager(iconManager)
    {
        m_auiManager.SetManagedWindow(this);

        m_viewModel->set_event_callback([this](const Event::EventId eventId, const Event::VertexEvent& event)
        {
            vertex_event_callback(eventId, event);
        });

        m_viewModel->start_worker();

        create_controls();
        create_menu_bar();
        create_toolbar();
        create_status_bar();
        create_panels();
        layout_controls();
        setup_aui_layout();
        bind_events();
        setup_panel_callbacks();

        update_toolbar_state();
        update_status_bar();

        Hide();
    }

    DebuggerView::~DebuggerView()
    {
        if (m_refreshTimer)
        {
            m_refreshTimer->Stop();
            delete m_refreshTimer;
        }
        m_auiManager.UnInit();
    }

    void DebuggerView::create_controls()
    {
        m_refreshTimer = new wxTimer(this, wxID_ANY);
    }

    void DebuggerView::create_menu_bar()
    {
        m_menuBar = new wxMenuBar();

        m_debugMenu = new wxMenu();
        m_debugMenu->Append(ID_ATTACH, wxString::FromUTF8(m_languageService.fetch_translation("debugger.menu.attach")) + "\tF5", wxString::FromUTF8(m_languageService.fetch_translation("debugger.menu.attachTooltip")));
        m_debugMenu->Append(ID_DETACH, wxString::FromUTF8(m_languageService.fetch_translation("debugger.menu.detach")), wxString::FromUTF8(m_languageService.fetch_translation("debugger.menu.detachTooltip")));
        m_debugMenu->AppendSeparator();
        m_debugMenu->Append(ID_CONTINUE, wxString::FromUTF8(m_languageService.fetch_translation("debugger.menu.continue")) + "\tF5", wxString::FromUTF8(m_languageService.fetch_translation("debugger.menu.continueTooltip")));
        m_debugMenu->Append(ID_PAUSE, wxString::FromUTF8(m_languageService.fetch_translation("debugger.menu.pause")) + "\tCtrl+P", wxString::FromUTF8(m_languageService.fetch_translation("debugger.menu.pauseTooltip")));
        m_debugMenu->AppendSeparator();
        m_debugMenu->Append(ID_STEP_INTO, wxString::FromUTF8(m_languageService.fetch_translation("debugger.menu.stepInto")) + "\tF11", wxString::FromUTF8(m_languageService.fetch_translation("debugger.menu.stepIntoTooltip")));
        m_debugMenu->Append(ID_STEP_OVER, wxString::FromUTF8(m_languageService.fetch_translation("debugger.menu.stepOver")) + "\tF10", wxString::FromUTF8(m_languageService.fetch_translation("debugger.menu.stepOverTooltip")));
        m_debugMenu->Append(ID_STEP_OUT, wxString::FromUTF8(m_languageService.fetch_translation("debugger.menu.stepOut")) + "\tShift+F11", wxString::FromUTF8(m_languageService.fetch_translation("debugger.menu.stepOutTooltip")));
        m_debugMenu->AppendSeparator();
        m_debugMenu->Append(ID_TOGGLE_BREAKPOINT, wxString::FromUTF8(m_languageService.fetch_translation("debugger.menu.toggleBreakpoint")) + "\tF9", wxString::FromUTF8(m_languageService.fetch_translation("debugger.menu.toggleBreakpointTooltip")));
        m_debugMenu->Append(ID_RUN_TO_CURSOR, wxString::FromUTF8(m_languageService.fetch_translation("debugger.menu.runToCursor")) + "\tF4", wxString::FromUTF8(m_languageService.fetch_translation("debugger.menu.runToCursorTooltip")));
        m_debugMenu->AppendSeparator();
        m_debugMenu->Append(ID_GOTO_ADDRESS, wxString::FromUTF8(m_languageService.fetch_translation("debugger.menu.gotoAddress")) + "\tCtrl+G", wxString::FromUTF8(m_languageService.fetch_translation("debugger.menu.gotoAddressTooltip")));

        m_viewMenu = new wxMenu();
        m_viewMenu->AppendCheckItem(ID_VIEW_DISASSEMBLY, wxString::FromUTF8(m_languageService.fetch_translation("debugger.menu.viewDisassembly")), wxString::FromUTF8(m_languageService.fetch_translation("debugger.menu.viewDisassemblyTooltip")));
        m_viewMenu->AppendCheckItem(ID_VIEW_BREAKPOINTS, wxString::FromUTF8(m_languageService.fetch_translation("debugger.menu.viewBreakpoints")), wxString::FromUTF8(m_languageService.fetch_translation("debugger.menu.viewBreakpointsTooltip")));
        m_viewMenu->AppendCheckItem(ID_VIEW_WATCHPOINTS, wxString::FromUTF8(m_languageService.fetch_translation("debugger.menu.viewWatchpoints")), wxString::FromUTF8(m_languageService.fetch_translation("debugger.menu.viewWatchpointsTooltip")));
        m_viewMenu->AppendCheckItem(ID_VIEW_REGISTERS, wxString::FromUTF8(m_languageService.fetch_translation("debugger.menu.viewRegisters")), wxString::FromUTF8(m_languageService.fetch_translation("debugger.menu.viewRegistersTooltip")));
        m_viewMenu->AppendCheckItem(ID_VIEW_STACK, wxString::FromUTF8(m_languageService.fetch_translation("debugger.menu.viewStack")), wxString::FromUTF8(m_languageService.fetch_translation("debugger.menu.viewStackTooltip")));
        m_viewMenu->AppendCheckItem(ID_VIEW_THREADS, wxString::FromUTF8(m_languageService.fetch_translation("debugger.menu.viewThreads")), wxString::FromUTF8(m_languageService.fetch_translation("debugger.menu.viewThreadsTooltip")));
        m_viewMenu->AppendCheckItem(ID_VIEW_WATCH, wxString::FromUTF8(m_languageService.fetch_translation("debugger.menu.viewWatch")), wxString::FromUTF8(m_languageService.fetch_translation("debugger.menu.viewWatchTooltip")));
        m_viewMenu->AppendSeparator();
        m_viewMenu->AppendCheckItem(ID_VIEW_MEMORY, wxString::FromUTF8(m_languageService.fetch_translation("debugger.menu.viewMemory")), wxString::FromUTF8(m_languageService.fetch_translation("debugger.menu.viewMemoryTooltip")));
        m_viewMenu->AppendCheckItem(ID_VIEW_HEX_EDITOR, wxString::FromUTF8(m_languageService.fetch_translation("debugger.menu.viewHexEditor")), wxString::FromUTF8(m_languageService.fetch_translation("debugger.menu.viewHexEditorTooltip")));
        m_viewMenu->AppendCheckItem(ID_VIEW_IMPORTS_EXPORTS, wxString::FromUTF8(m_languageService.fetch_translation("debugger.menu.viewImportsExports")), wxString::FromUTF8(m_languageService.fetch_translation("debugger.menu.viewImportsExportsTooltip")));
        m_viewMenu->AppendSeparator();
        m_viewMenu->AppendCheckItem(ID_VIEW_CONSOLE, wxString::FromUTF8(m_languageService.fetch_translation("debugger.menu.viewConsole")), wxString::FromUTF8(m_languageService.fetch_translation("debugger.menu.viewConsoleTooltip")));

        m_viewMenu->Check(ID_VIEW_DISASSEMBLY, true);
        m_viewMenu->Check(ID_VIEW_BREAKPOINTS, true);
        m_viewMenu->Check(ID_VIEW_WATCHPOINTS, true);
        m_viewMenu->Check(ID_VIEW_REGISTERS, true);
        m_viewMenu->Check(ID_VIEW_STACK, true);
        m_viewMenu->Check(ID_VIEW_THREADS, true);
        m_viewMenu->Check(ID_VIEW_WATCH, true);
        m_viewMenu->Check(ID_VIEW_MEMORY, true);
        m_viewMenu->Check(ID_VIEW_HEX_EDITOR, true);
        m_viewMenu->Check(ID_VIEW_IMPORTS_EXPORTS, true);
        m_viewMenu->Check(ID_VIEW_CONSOLE, true);

        m_menuBar->Append(m_debugMenu, wxString::FromUTF8(m_languageService.fetch_translation("debugger.menu.debug")));
        m_menuBar->Append(m_viewMenu, wxString::FromUTF8(m_languageService.fetch_translation("debugger.menu.view")));
        SetMenuBar(m_menuBar);
    }

    void DebuggerView::create_toolbar()
    {
        m_toolbar = new wxAuiToolBar(this, wxID_ANY, wxDefaultPosition, wxDefaultSize,
                                      wxAUI_TB_DEFAULT_STYLE | wxAUI_TB_HORIZONTAL | wxAUI_TB_PLAIN_BACKGROUND);

        m_toolbar->SetToolBitmapSize(wxSize(StandardWidgetValues::ICON_SIZE, StandardWidgetValues::ICON_SIZE));
        const Theme theme = m_viewModel->get_theme();

        m_toolbar->AddTool(ID_ATTACH, wxString::FromUTF8(m_languageService.fetch_translation("debugger.toolbar.attach")),
                           m_iconManager.get_icon("play", FromDIP(StandardWidgetValues::ICON_SIZE), theme),
                           wxString::FromUTF8(m_languageService.fetch_translation("debugger.toolbar.attachTooltip")));
        m_toolbar->AddTool(ID_DETACH, wxString::FromUTF8(m_languageService.fetch_translation("debugger.toolbar.detach")),
                           m_iconManager.get_icon("stop", FromDIP(StandardWidgetValues::ICON_SIZE), theme),
                           wxString::FromUTF8(m_languageService.fetch_translation("debugger.toolbar.detachTooltip")));
        m_toolbar->AddSeparator();
        m_toolbar->AddTool(ID_CONTINUE, wxString::FromUTF8(m_languageService.fetch_translation("debugger.toolbar.continue")),
                           m_iconManager.get_icon("play", FromDIP(StandardWidgetValues::ICON_SIZE), theme),
                           wxString::FromUTF8(m_languageService.fetch_translation("debugger.toolbar.continueTooltip")));
        m_toolbar->AddTool(ID_PAUSE, wxString::FromUTF8(m_languageService.fetch_translation("debugger.toolbar.pause")),
                           m_iconManager.get_icon("pause", FromDIP(StandardWidgetValues::ICON_SIZE), theme),
                           wxString::FromUTF8(m_languageService.fetch_translation("debugger.toolbar.pauseTooltip")));
        m_toolbar->AddSeparator();
        m_toolbar->AddTool(ID_STEP_INTO, wxString::FromUTF8(m_languageService.fetch_translation("debugger.toolbar.stepInto")),
                           m_iconManager.get_icon("step_into", FromDIP(StandardWidgetValues::ICON_SIZE), theme),
                           wxString::FromUTF8(m_languageService.fetch_translation("debugger.toolbar.stepIntoTooltip")));
        m_toolbar->AddTool(ID_STEP_OVER, wxString::FromUTF8(m_languageService.fetch_translation("debugger.toolbar.stepOver")),
                           m_iconManager.get_icon("step_over", FromDIP(StandardWidgetValues::ICON_SIZE), theme),
                           wxString::FromUTF8(m_languageService.fetch_translation("debugger.toolbar.stepOverTooltip")));
        m_toolbar->AddTool(ID_STEP_OUT, wxString::FromUTF8(m_languageService.fetch_translation("debugger.toolbar.stepOut")),
                           m_iconManager.get_icon("step_out", FromDIP(StandardWidgetValues::ICON_SIZE), theme),
                           wxString::FromUTF8(m_languageService.fetch_translation("debugger.toolbar.stepOutTooltip")));
        m_toolbar->AddSeparator();
        m_toolbar->AddTool(ID_TOGGLE_BREAKPOINT, wxString::FromUTF8(m_languageService.fetch_translation("debugger.toolbar.breakpoint")),
                           m_iconManager.get_icon("breakpoint", FromDIP(StandardWidgetValues::ICON_SIZE), theme),
                           wxString::FromUTF8(m_languageService.fetch_translation("debugger.toolbar.breakpointTooltip")));

        m_toolbar->Realize();
    }

    void DebuggerView::create_status_bar()
    {
        m_statusPanel = new wxPanel(this, wxID_ANY);
        m_statusPanel->SetBackgroundColour(wxColour(0x2D, 0x2D, 0x2D));

        auto* statusSizer = new wxBoxSizer(wxHORIZONTAL);

        m_stateText = new wxStaticText(m_statusPanel, wxID_ANY, wxString::FromUTF8(m_languageService.fetch_translation("debugger.status.detached")),
                                        wxDefaultPosition, wxSize(FromDIP(100), -1));
        m_stateText->SetForegroundColour(wxColour(0xDC, 0xDC, 0xDC));
        m_stateText->SetFont(m_stateText->GetFont().Bold());

        m_addressText = new wxStaticText(m_statusPanel, wxID_ANY, EMPTY_STRING,
                                          wxDefaultPosition, wxSize(FromDIP(180), -1));
        m_addressText->SetForegroundColour(wxColour(0x56, 0x9C, 0xD6));
        m_addressText->SetFont(wxFont(9, wxFONTFAMILY_TELETYPE, wxFONTSTYLE_NORMAL, wxFONTWEIGHT_NORMAL));

        m_threadText = new wxStaticText(m_statusPanel, wxID_ANY, EMPTY_STRING, wxDefaultPosition, wxSize(FromDIP(100), -1));
        m_threadText->SetForegroundColour(wxColour(0xDC, 0xDC, 0xDC));

        m_infoText = new wxStaticText(m_statusPanel, wxID_ANY, wxString::FromUTF8(m_languageService.fetch_translation("debugger.status.ready")));
        m_infoText->SetForegroundColour(wxColour(0x80, 0x80, 0x80));

        auto createSeparator = [this]()
        {
            auto* sep = new wxStaticText(m_statusPanel, wxID_ANY, "|");
            sep->SetForegroundColour(wxColour(0x50, 0x50, 0x50));
            return sep;
        };

        statusSizer->Add(m_stateText, 0, wxALIGN_CENTER_VERTICAL | wxLEFT | wxRIGHT, FromDIP(8));
        statusSizer->Add(createSeparator(), 0, wxALIGN_CENTER_VERTICAL);
        statusSizer->Add(m_addressText, 0, wxALIGN_CENTER_VERTICAL | wxLEFT | wxRIGHT, FromDIP(8));
        statusSizer->Add(createSeparator(), 0, wxALIGN_CENTER_VERTICAL);
        statusSizer->Add(m_threadText, 0, wxALIGN_CENTER_VERTICAL | wxLEFT | wxRIGHT, FromDIP(8));
        statusSizer->Add(createSeparator(), 0, wxALIGN_CENTER_VERTICAL);
        statusSizer->Add(m_infoText, 1, wxALIGN_CENTER_VERTICAL | wxLEFT | wxRIGHT, FromDIP(8));

        m_statusPanel->SetSizer(statusSizer);
    }

    void DebuggerView::create_panels()
    {
        m_disassemblyPanel = new Debugger::DisassemblyPanel(this, m_languageService, m_iconManager);
        m_breakpointsPanel = new Debugger::BreakpointsPanel(this, m_languageService);
        m_watchpointsPanel = new Debugger::WatchpointsPanel(this, m_languageService);
        m_registersPanel = new Debugger::RegistersPanel(this, m_languageService);
        m_stackPanel = new Debugger::StackPanel(this, m_languageService);
        m_memoryPanel = new Debugger::MemoryPanel(this, m_languageService);
        m_hexEditorPanel = new Debugger::HexEditorPanel(this, m_languageService);
        m_importExportPanel = new Debugger::ImportExportPanel(this, m_languageService);
        m_threadsPanel = new Debugger::ThreadsPanel(this, m_languageService);
        m_watchPanel = new Debugger::WatchPanel(this, m_languageService);
        m_consolePanel = new Debugger::ConsolePanel(this, m_languageService);
    }

    void DebuggerView::layout_controls()
    {
        m_auiManager.AddPane(m_toolbar, wxAuiPaneInfo()
            .Name("DebuggerToolbar")
            .ToolbarPane()
            .Top()
            .Layer(10)
            .Row(0)
            .LeftDockable(false)
            .RightDockable(false)
            .BottomDockable(false)
            .Floatable(false)
            .Movable(false)
            .Gripper(false)
            .CaptionVisible(false)
            .CloseButton(false)
            .MaximizeButton(false)
            .MinimizeButton(false)
            .PinButton(false)
            .Resizable(false));
    }

    void DebuggerView::setup_aui_layout()
    {
        m_auiManager.AddPane(m_disassemblyPanel, wxAuiPaneInfo()
            .Name("Disassembly")
            .Caption(wxString::FromUTF8(m_languageService.fetch_translation("debugger.pane.disassembly")))
            .CenterPane()
            .BestSize(FromDIP(600), FromDIP(400)));

        m_auiManager.AddPane(m_registersPanel, wxAuiPaneInfo()
            .Name("Registers")
            .Caption(wxString::FromUTF8(m_languageService.fetch_translation("debugger.pane.registers")))
            .Right()
            .Row(0)
            .Position(0)
            .BestSize(FromDIP(250), FromDIP(300))
            .MinSize(FromDIP(200), FromDIP(150))
            .CloseButton(true)
            .MaximizeButton(true));

        m_auiManager.AddPane(m_stackPanel, wxAuiPaneInfo()
            .Name("Stack")
            .Caption(wxString::FromUTF8(m_languageService.fetch_translation("debugger.pane.callStack")))
            .Right()
            .Row(0)
            .Position(1)
            .BestSize(FromDIP(250), FromDIP(200))
            .MinSize(FromDIP(200), FromDIP(100))
            .CloseButton(true)
            .MaximizeButton(true));

        m_auiManager.AddPane(m_breakpointsPanel, wxAuiPaneInfo()
            .Name("Breakpoints")
            .Caption(wxString::FromUTF8(m_languageService.fetch_translation("debugger.pane.breakpoints")))
            .Left()
            .Row(0)
            .Position(0)
            .BestSize(FromDIP(250), FromDIP(150))
            .MinSize(FromDIP(200), FromDIP(100))
            .CloseButton(true)
            .MaximizeButton(true));

        m_auiManager.AddPane(m_watchpointsPanel, wxAuiPaneInfo()
            .Name("Watchpoints")
            .Caption(wxString::FromUTF8(m_languageService.fetch_translation("debugger.pane.watchpoints")))
            .Left()
            .Row(0)
            .Position(1)
            .BestSize(FromDIP(250), FromDIP(150))
            .MinSize(FromDIP(200), FromDIP(100))
            .CloseButton(true)
            .MaximizeButton(true));

        m_auiManager.AddPane(m_importExportPanel, wxAuiPaneInfo()
            .Name("ImportsExports")
            .Caption(wxString::FromUTF8(m_languageService.fetch_translation("debugger.pane.importsExports")))
            .Left()
            .Row(0)
            .Position(2)
            .BestSize(FromDIP(250), FromDIP(250))
            .MinSize(FromDIP(200), FromDIP(100))
            .CloseButton(true)
            .MaximizeButton(true));

        m_auiManager.AddPane(m_memoryPanel, wxAuiPaneInfo()
            .Name("Memory")
            .Caption(wxString::FromUTF8(m_languageService.fetch_translation("debugger.pane.memory")))
            .Bottom()
            .Row(0)
            .Position(0)
            .BestSize(FromDIP(400), FromDIP(150))
            .MinSize(FromDIP(300), FromDIP(100))
            .CloseButton(true)
            .MaximizeButton(true));

        m_auiManager.AddPane(m_hexEditorPanel, wxAuiPaneInfo()
            .Name("HexEditor")
            .Caption(wxString::FromUTF8(m_languageService.fetch_translation("debugger.pane.hexEditor")))
            .Bottom()
            .Row(0)
            .Position(1)
            .BestSize(FromDIP(400), FromDIP(150))
            .MinSize(FromDIP(300), FromDIP(100))
            .CloseButton(true)
            .MaximizeButton(true));

        m_auiManager.AddPane(m_threadsPanel, wxAuiPaneInfo()
            .Name("Threads")
            .Caption(wxString::FromUTF8(m_languageService.fetch_translation("debugger.pane.threads")))
            .Right()
            .Row(0)
            .Position(2)
            .BestSize(FromDIP(250), FromDIP(150))
            .MinSize(FromDIP(200), FromDIP(100))
            .CloseButton(true)
            .MaximizeButton(true));

        m_auiManager.AddPane(m_watchPanel, wxAuiPaneInfo()
            .Name("Watch")
            .Caption(wxString::FromUTF8(m_languageService.fetch_translation("debugger.pane.watchLocals")))
            .Left()
            .Row(0)
            .Position(3)
            .BestSize(FromDIP(250), FromDIP(250))
            .MinSize(FromDIP(200), FromDIP(150))
            .CloseButton(true)
            .MaximizeButton(true));

        m_auiManager.AddPane(m_consolePanel, wxAuiPaneInfo()
            .Name("Console")
            .Caption(wxString::FromUTF8(m_languageService.fetch_translation("debugger.pane.console")))
            .Bottom()
            .Row(1)
            .Position(0)
            .BestSize(FromDIP(800), FromDIP(120))
            .MinSize(FromDIP(400), FromDIP(80))
            .CloseButton(true)
            .MaximizeButton(true));

        m_auiManager.AddPane(m_statusPanel, wxAuiPaneInfo()
            .Name("StatusBar")
            .Bottom()
            .Layer(10)
            .Row(2)
            .Fixed()
            .CaptionVisible(false)
            .CloseButton(false)
            .Floatable(false)
            .Movable(false)
            .Resizable(false)
            .DockFixed(true)
            .BestSize(-1, FromDIP(24))
            .MinSize(-1, FromDIP(24))
            .MaxSize(-1, FromDIP(24)));

        m_auiManager.Update();

        const std::string savedPerspective = m_viewModel->get_aui_perspective();
        if (!savedPerspective.empty())
        {
            m_auiManager.LoadPerspective(wxString::FromUTF8(savedPerspective), true);
            apply_pane_captions();
            m_auiManager.Update();
        }
    }

    void DebuggerView::apply_pane_captions()
    {
        static constexpr std::array<std::pair<const char*, const char*>, 11> panes
        {{
            {"Disassembly",    "debugger.pane.disassembly"},
            {"Registers",      "debugger.pane.registers"},
            {"Stack",          "debugger.pane.callStack"},
            {"Breakpoints",    "debugger.pane.breakpoints"},
            {"Watchpoints",    "debugger.pane.watchpoints"},
            {"ImportsExports", "debugger.pane.importsExports"},
            {"Memory",         "debugger.pane.memory"},
            {"HexEditor",      "debugger.pane.hexEditor"},
            {"Threads",        "debugger.pane.threads"},
            {"Watch",          "debugger.pane.watchLocals"},
            {"Console",        "debugger.pane.console"},
        }};

        for (const auto& [name, key] : panes)
        {
            auto& pane = m_auiManager.GetPane(name);
            if (pane.IsOk())
            {
                pane.Caption(wxString::FromUTF8(m_languageService.fetch_translation(key)));
            }
        }
    }

    void DebuggerView::setup_panel_callbacks()
    {
        m_disassemblyPanel->set_navigate_callback([this](std::uint64_t address)
        {
            m_viewModel->navigate_to_address(address);
            update_view(ViewUpdateFlags::DEBUGGER_DISASSEMBLY);
        });

        m_disassemblyPanel->set_breakpoint_toggle_callback([this](std::uint64_t address)
        {
            m_viewModel->toggle_breakpoint(address);
        });

        m_disassemblyPanel->set_run_to_cursor_callback([this](std::uint64_t address)
        {
            m_viewModel->run_to_cursor(address);
        });

        m_disassemblyPanel->set_scroll_boundary_callback([this](std::uint64_t boundaryAddress, bool isTop)
        {
            const auto status = isTop ? m_viewModel->disassemble_extend_up(boundaryAddress)
                                       : m_viewModel->disassemble_extend_down(boundaryAddress);

            if (status != StatusCode::STATUS_OK)
            {
                const auto errorKey = isTop ? "debugger.errors.failedExtendDisassemblyUpward"
                                             : "debugger.errors.failedExtendDisassemblyDownward";
                const auto errorMsg = wxString::Format(m_languageService.fetch_translation(errorKey).c_str(), boundaryAddress);
                const auto titleMsg = m_languageService.fetch_translation("debugger.errors.disassemblyError");

                wxMessageBox(wxString::FromUTF8(errorMsg), wxString::FromUTF8(titleMsg), wxOK | wxICON_ERROR, this);
            }
        });

        m_breakpointsPanel->set_goto_callback([this](std::uint64_t address)
        {
            m_viewModel->navigate_to_address(address);
            update_view(ViewUpdateFlags::DEBUGGER_DISASSEMBLY);
        });

        m_breakpointsPanel->set_remove_callback([this](std::uint32_t id)
        {
            m_viewModel->remove_breakpoint(id);
        });

        m_breakpointsPanel->set_enable_callback([this](std::uint32_t id, bool enable)
        {
            m_viewModel->enable_breakpoint(id, enable);
        });

        m_watchpointsPanel->set_goto_callback([this](std::uint64_t address)
        {
            m_viewModel->navigate_to_address(address);
            update_view(ViewUpdateFlags::DEBUGGER_DISASSEMBLY);
        });

        m_watchpointsPanel->set_remove_callback([this](std::uint32_t id)
        {
            m_viewModel->remove_watchpoint(id);
        });

        m_watchpointsPanel->set_enable_callback([this](std::uint32_t id, bool enable)
        {
            m_viewModel->enable_watchpoint(id, enable);
        });

        m_watchpointsPanel->set_goto_accessor_callback([this](std::uint64_t address)
        {
            m_viewModel->navigate_to_address(address);
            if (const auto status = m_viewModel->disassemble_at_address(address); status != StatusCode::STATUS_OK)
            {
                wxMessageBox(wxString::FromUTF8(wxString::Format(m_languageService.fetch_translation("debugger.errors.failedDisassembleAddress").c_str(), address)),
                             wxString::FromUTF8(m_languageService.fetch_translation("debugger.errors.disassemblyError")), wxOK | wxICON_ERROR, this);
            }
            update_view(ViewUpdateFlags::DEBUGGER_DISASSEMBLY);
        });

        m_registersPanel->set_register_callback([](const std::string&, std::uint64_t)
        {
        });

        m_registersPanel->set_refresh_callback([this]()
        {
            if (const auto status = m_viewModel->read_registers(); status != StatusCode::STATUS_OK)
            {
                wxMessageBox(wxString::FromUTF8(m_languageService.fetch_translation("debugger.errors.failedReadRegisters")),
                             wxString::FromUTF8(m_languageService.fetch_translation("debugger.errors.registerError")), wxOK | wxICON_ERROR, this);
            }
        });

        m_stackPanel->set_select_frame_callback([this](std::uint32_t frameIndex)
        {
            m_viewModel->select_stack_frame(frameIndex);
        });

        m_memoryPanel->set_navigate_callback([](std::uint64_t)
        {
        });

        m_hexEditorPanel->set_navigate_callback([](std::uint64_t)
        {
        });

        m_hexEditorPanel->set_write_callback([](std::uint64_t, const std::vector<std::uint8_t>&)
        {
        });

        m_importExportPanel->set_navigate_callback([this](std::uint64_t address)
        {
            m_viewModel->navigate_to_address(address);
            update_view(ViewUpdateFlags::DEBUGGER_DISASSEMBLY);
        });

        m_importExportPanel->set_select_module_callback([this](const std::string_view moduleName)
        {
            m_importExportPanel->clear_selection();

            if (m_viewModel->load_module_imports_exports(moduleName) == StatusCode::STATUS_OK)
            {
                m_importExportPanel->update_imports(m_viewModel->get_imports());
                m_importExportPanel->update_exports(m_viewModel->get_exports());
            }

            m_viewModel->select_module(moduleName);
            update_view(ViewUpdateFlags::DEBUGGER_DISASSEMBLY);
        });
    }

    void DebuggerView::bind_events()
    {
        Bind(wxEVT_MENU, &DebuggerView::on_attach_clicked, this, ID_ATTACH);
        Bind(wxEVT_MENU, &DebuggerView::on_detach_clicked, this, ID_DETACH);
        Bind(wxEVT_MENU, &DebuggerView::on_continue_clicked, this, ID_CONTINUE);
        Bind(wxEVT_MENU, &DebuggerView::on_pause_clicked, this, ID_PAUSE);
        Bind(wxEVT_MENU, &DebuggerView::on_step_into_clicked, this, ID_STEP_INTO);
        Bind(wxEVT_MENU, &DebuggerView::on_step_over_clicked, this, ID_STEP_OVER);
        Bind(wxEVT_MENU, &DebuggerView::on_step_out_clicked, this, ID_STEP_OUT);
        Bind(wxEVT_MENU, &DebuggerView::on_run_to_cursor_clicked, this, ID_RUN_TO_CURSOR);
        Bind(wxEVT_MENU, &DebuggerView::on_toggle_breakpoint_clicked, this, ID_TOGGLE_BREAKPOINT);
        Bind(wxEVT_MENU, &DebuggerView::on_goto_address_clicked, this, ID_GOTO_ADDRESS);

        Bind(wxEVT_MENU, &DebuggerView::on_view_pane_toggle, this, ID_VIEW_DISASSEMBLY);
        Bind(wxEVT_MENU, &DebuggerView::on_view_pane_toggle, this, ID_VIEW_BREAKPOINTS);
        Bind(wxEVT_MENU, &DebuggerView::on_view_pane_toggle, this, ID_VIEW_WATCHPOINTS);
        Bind(wxEVT_MENU, &DebuggerView::on_view_pane_toggle, this, ID_VIEW_REGISTERS);
        Bind(wxEVT_MENU, &DebuggerView::on_view_pane_toggle, this, ID_VIEW_STACK);
        Bind(wxEVT_MENU, &DebuggerView::on_view_pane_toggle, this, ID_VIEW_MEMORY);
        Bind(wxEVT_MENU, &DebuggerView::on_view_pane_toggle, this, ID_VIEW_HEX_EDITOR);
        Bind(wxEVT_MENU, &DebuggerView::on_view_pane_toggle, this, ID_VIEW_IMPORTS_EXPORTS);
        Bind(wxEVT_MENU, &DebuggerView::on_view_pane_toggle, this, ID_VIEW_THREADS);
        Bind(wxEVT_MENU, &DebuggerView::on_view_pane_toggle, this, ID_VIEW_WATCH);
        Bind(wxEVT_MENU, &DebuggerView::on_view_pane_toggle, this, ID_VIEW_CONSOLE);

        Bind(wxEVT_TIMER, &DebuggerView::on_refresh_timer, this, m_refreshTimer->GetId());

        Bind(wxEVT_CLOSE_WINDOW, &DebuggerView::on_close, this);

        Bind(wxEVT_SHOW, &DebuggerView::on_show, this);
    }

    void DebuggerView::vertex_event_callback(const Event::EventId eventId, const Event::VertexEvent& event)
    {
        if (eventId == Event::VIEW_EVENT)
        {
            CallAfter([this]()
            {
                std::ignore = toggle_view();
            });
        }
        else if (eventId == Event::VIEW_UPDATE_EVENT)
        {
            const auto& viewUpdateEvent = static_cast<const Event::ViewUpdateEvent&>(event);
            const auto flags = viewUpdateEvent.get_update_flags();

            CallAfter([this, flags]()
            {
                if (IsShown())
                {
                    update_view(flags);
                }
                else
                {
                    m_pendingUpdateFlags = static_cast<ViewUpdateFlags>(
                        std::to_underlying(m_pendingUpdateFlags) | std::to_underlying(flags));
                    m_hasPendingUpdate = true;
                }
            });
        }
    }

    bool DebuggerView::toggle_view()
    {
        if (IsShown())
        {
            Hide();
            return false;
        }

        Show();
        Raise();

        m_viewModel->ensure_data_loaded();

        update_view(ViewUpdateFlags::DEBUGGER_ALL);

        m_pendingUpdateFlags = ViewUpdateFlags::NONE;
        m_hasPendingUpdate = false;

        return true;
    }

    void DebuggerView::update_view(const ViewUpdateFlags flags)
    {
        const auto state = m_viewModel->get_state();
        const auto isPaused = m_viewModel->is_attached() &&
            (state == ::Vertex::Debugger::DebuggerState::Attached ||
             state == ::Vertex::Debugger::DebuggerState::Paused ||
             state == ::Vertex::Debugger::DebuggerState::BreakpointHit ||
             state == ::Vertex::Debugger::DebuggerState::Stepping ||
             state == ::Vertex::Debugger::DebuggerState::Exception);

        const auto enteredPausedState = isPaused && !is_paused_state(m_lastState) && is_paused_state(state);
        m_lastState = state;

        const auto currentAddress = m_viewModel->get_current_address();

        if (enteredPausedState && currentAddress != 0)
        {
            if (const auto status = m_viewModel->read_registers(); status != StatusCode::STATUS_OK)
            {
                wxMessageBox(wxString::FromUTF8(m_languageService.fetch_translation("debugger.errors.failedReadRegistersPause")),
                             wxString::FromUTF8(m_languageService.fetch_translation("debugger.errors.registerError")), wxOK | wxICON_ERROR, this);
            }

            const auto& disasm = m_viewModel->get_disassembly();
            const auto addressInRange = currentAddress >= disasm.startAddress &&
                currentAddress < disasm.endAddress;

            if (!addressInRange)
            {
                if (const auto status = m_viewModel->disassemble_at_address(currentAddress); status != StatusCode::STATUS_OK)
                {
                    wxMessageBox(wxString::FromUTF8(wxString::Format(m_languageService.fetch_translation("debugger.errors.failedDisassembleAddress").c_str(), currentAddress)),
                                 wxString::FromUTF8(m_languageService.fetch_translation("debugger.errors.disassemblyError")), wxOK | wxICON_ERROR, this);
                }
            }

            const auto& updatedDisasm = m_viewModel->get_disassembly();
            if (!updatedDisasm.lines.empty())
            {
                m_disassemblyPanel->update_disassembly(updatedDisasm);
                m_disassemblyPanel->set_breakpoints(m_viewModel->get_breakpoints());
                m_disassemblyPanel->highlight_address(currentAddress);
                m_lastHighlightedAddress = currentAddress;
            }

            update_status_bar();
        }

        if (has_flag(flags, ViewUpdateFlags::DEBUGGER_DISASSEMBLY))
        {
            const auto& disasm = m_viewModel->get_disassembly();

            if (!disasm.lines.empty())
            {
                m_disassemblyPanel->update_disassembly(disasm);
                m_disassemblyPanel->set_breakpoints(m_viewModel->get_breakpoints());

                if (currentAddress != m_lastHighlightedAddress && currentAddress != 0)
                {
                    m_disassemblyPanel->highlight_address(currentAddress);
                    m_lastHighlightedAddress = currentAddress;
                }
            }
        }

        if (has_flag(flags, ViewUpdateFlags::DEBUGGER_BREAKPOINTS))
        {
            m_breakpointsPanel->update_breakpoints(m_viewModel->get_breakpoints());
        }

        if (has_flag(flags, ViewUpdateFlags::DEBUGGER_WATCHPOINTS))
        {
            m_watchpointsPanel->update_watchpoints(m_viewModel->get_watchpoints());
        }

        if (has_flag(flags, ViewUpdateFlags::DEBUGGER_REGISTERS))
        {
            const auto& regs = m_viewModel->get_registers();
            if (isPaused || !regs.generalPurpose.empty() || regs.instructionPointer != 0)
            {
                m_registersPanel->update_registers(regs);
            }
            else
            {
                m_registersPanel->clear();
            }
        }

        if (has_flag(flags, ViewUpdateFlags::DEBUGGER_STACK))
        {
            if (isPaused)
            {
                m_stackPanel->update_call_stack(m_viewModel->get_call_stack());
                m_stackPanel->set_selected_frame(m_viewModel->get_selected_frame_index());
            }
        }

        if (has_flag(flags, ViewUpdateFlags::DEBUGGER_MEMORY))
        {
        }

        if (has_flag(flags, ViewUpdateFlags::DEBUGGER_IMPORTS_EXPORTS))
        {
            const auto& modules = m_viewModel->get_modules();
            if (!modules.empty())
            {
                m_importExportPanel->update_modules(modules);

                auto selectedModule = m_viewModel->get_selected_module();
                if (selectedModule.empty())
                {
                    selectedModule = modules[0].name;
                    m_viewModel->select_module(selectedModule);
                }

                m_importExportPanel->set_selected_module(selectedModule);

                if (m_viewModel->load_module_imports_exports(selectedModule) == StatusCode::STATUS_OK)
                {
                    m_importExportPanel->update_imports(m_viewModel->get_imports());
                    m_importExportPanel->update_exports(m_viewModel->get_exports());
                }
            }
            else
            {
                m_importExportPanel->clear();
            }
        }

        if (has_flag(flags, ViewUpdateFlags::DEBUGGER_THREADS))
        {
            const auto& threads = m_viewModel->get_threads();
            if (!threads.empty())
            {
                m_threadsPanel->update_threads(threads);
                m_threadsPanel->set_current_thread(m_viewModel->get_current_thread_id());
            }
            else
            {
                m_threadsPanel->clear();
            }
        }

        if (has_flag(flags, ViewUpdateFlags::DEBUGGER_STATE))
        {
            update_toolbar_state();
            update_status_bar();
        }
    }

    void DebuggerView::update_toolbar_state()
    {
        const auto attached = m_viewModel->is_attached();
        const auto state = m_viewModel->get_state();

        m_toolbar->EnableTool(ID_ATTACH, !attached);
        m_toolbar->EnableTool(ID_DETACH, attached);

        const auto isPaused = attached && (state == ::Vertex::Debugger::DebuggerState::Attached ||
                                            state == ::Vertex::Debugger::DebuggerState::Paused ||
                                            state == ::Vertex::Debugger::DebuggerState::BreakpointHit ||
                                            state == ::Vertex::Debugger::DebuggerState::Stepping ||
                                            state == ::Vertex::Debugger::DebuggerState::Exception);
        const auto canContinue = isPaused;
        const auto canStep = isPaused;
        const auto canPause = attached && state == ::Vertex::Debugger::DebuggerState::Running;

        m_toolbar->EnableTool(ID_CONTINUE, canContinue);
        m_toolbar->EnableTool(ID_PAUSE, canPause);
        m_toolbar->EnableTool(ID_STEP_INTO, canStep);
        m_toolbar->EnableTool(ID_STEP_OVER, canStep);
        m_toolbar->EnableTool(ID_STEP_OUT, canStep);

        m_toolbar->Refresh();
    }

    void DebuggerView::update_status_bar()
    {
        const auto attached = m_viewModel->is_attached();
        const auto state = m_viewModel->get_state();

        wxString stateStr;
        switch (state)
        {
            case ::Vertex::Debugger::DebuggerState::Detached:
                stateStr = wxString::FromUTF8(m_languageService.fetch_translation("debugger.status.detached"));
                break;
            case ::Vertex::Debugger::DebuggerState::Attached:
                stateStr = wxString::FromUTF8(m_languageService.fetch_translation("debugger.status.attached"));
                break;
            case ::Vertex::Debugger::DebuggerState::Running:
                stateStr = wxString::FromUTF8(m_languageService.fetch_translation("debugger.status.running"));
                break;
            case ::Vertex::Debugger::DebuggerState::Paused:
                stateStr = wxString::FromUTF8(m_languageService.fetch_translation("debugger.status.paused"));
                break;
            case ::Vertex::Debugger::DebuggerState::Stepping:
                stateStr = wxString::FromUTF8(m_languageService.fetch_translation("debugger.status.stepping"));
                break;
            case ::Vertex::Debugger::DebuggerState::BreakpointHit:
                stateStr = wxString::FromUTF8(m_languageService.fetch_translation("debugger.status.breakpoint"));
                break;
            case ::Vertex::Debugger::DebuggerState::Exception:
                stateStr = wxString::FromUTF8(m_languageService.fetch_translation("debugger.status.exception"));
                break;
            default:
                stateStr = wxString::FromUTF8(m_languageService.fetch_translation("debugger.ui.unknown"));
                break;
        }
        m_stateText->SetLabel(stateStr);

        if (attached)
        {
            const auto currentAddress = m_viewModel->get_current_address();
            if (currentAddress != 0)
            {
                m_addressText->SetLabel(wxString::FromUTF8(fmt::format("RIP: 0x{:016X}", currentAddress)));
            }
            else
            {
                const auto& regs = m_viewModel->get_registers();
                m_addressText->SetLabel(wxString::FromUTF8(fmt::format("RIP: 0x{:016X}", regs.instructionPointer)));
            }
        }
        else
        {
            m_addressText->SetLabel(EMPTY_STRING);
        }

        if (attached)
        {
            const auto threadId = m_viewModel->get_current_thread_id();
            if (threadId != 0)
            {
                m_threadText->SetLabel(wxString::FromUTF8(fmt::format("{}: {}", m_languageService.fetch_translation("debugger.status.thread"), threadId)));
            }
            else
            {
                m_threadText->SetLabel(wxString::FromUTF8(fmt::format("{}: -", m_languageService.fetch_translation("debugger.status.thread"))));
            }
        }
        else
        {
            m_threadText->SetLabel(EMPTY_STRING);
        }

        if (!attached)
        {
            m_infoText->SetLabel(wxString::FromUTF8(m_languageService.fetch_translation("debugger.status.readySelectProcess")));
        }
        else if (state == ::Vertex::Debugger::DebuggerState::Running)
        {
            m_infoText->SetLabel(wxString::FromUTF8(m_languageService.fetch_translation("debugger.status.processRunning")));
        }
        else if (state == ::Vertex::Debugger::DebuggerState::Paused ||
                 state == ::Vertex::Debugger::DebuggerState::BreakpointHit)
        {
            m_infoText->SetLabel(wxString::FromUTF8(m_languageService.fetch_translation("debugger.status.processPausedStep")));
        }
        else
        {
            m_infoText->SetLabel(EMPTY_STRING);
        }

        m_statusPanel->Layout();
    }

    void DebuggerView::on_attach_clicked([[maybe_unused]] wxCommandEvent& event)
    {
        m_viewModel->attach_debugger();
        update_toolbar_state();
        update_status_bar();
    }

    void DebuggerView::on_detach_clicked([[maybe_unused]] wxCommandEvent& event)
    {
        m_viewModel->detach_debugger();
        update_toolbar_state();
        update_status_bar();
    }

    void DebuggerView::on_continue_clicked([[maybe_unused]] wxCommandEvent& event)
    {
        m_viewModel->continue_execution();
    }

    void DebuggerView::on_pause_clicked([[maybe_unused]] wxCommandEvent& event)
    {
        m_viewModel->pause_execution();
    }

    void DebuggerView::on_step_into_clicked([[maybe_unused]] wxCommandEvent& event)
    {
        m_viewModel->step_into();
    }

    void DebuggerView::on_step_over_clicked([[maybe_unused]] wxCommandEvent& event)
    {
        m_viewModel->step_over();
    }

    void DebuggerView::on_step_out_clicked([[maybe_unused]] wxCommandEvent& event)
    {
        m_viewModel->step_out();
    }

    void DebuggerView::on_run_to_cursor_clicked([[maybe_unused]] wxCommandEvent& event)
    {
        const auto address = m_disassemblyPanel->get_selected_address();
        if (address != 0)
        {
            m_viewModel->run_to_cursor(address);
        }
    }

    void DebuggerView::on_toggle_breakpoint_clicked([[maybe_unused]] wxCommandEvent& event)
    {
        const auto address = m_disassemblyPanel->get_selected_address();
        if (address != 0)
        {
            m_viewModel->toggle_breakpoint(address);
        }
    }

    void DebuggerView::on_goto_address_clicked([[maybe_unused]] wxCommandEvent& event)
    {
        wxTextEntryDialog dialog(this,
            wxString::FromUTF8(m_languageService.fetch_translation("debugger.dialog.enterAddressHex")),
            wxString::FromUTF8(m_languageService.fetch_translation("debugger.dialog.gotoAddressTitle")),
            "0x");
        if (dialog.ShowModal() == wxID_OK)
        {
            const auto input = dialog.GetValue();
            std::uint64_t address{};
            if (input.ToULongLong(&address, NumericSystem::HEXADECIMAL))
            {
                m_viewModel->navigate_to_address(address);
                update_view(ViewUpdateFlags::DEBUGGER_DISASSEMBLY);
            }
        }
    }

    void DebuggerView::on_view_pane_toggle(const wxCommandEvent& event)
    {
        static constexpr std::array paneMappings{
            std::pair{ID_VIEW_DISASSEMBLY, "Disassembly"},
            std::pair{ID_VIEW_BREAKPOINTS, "Breakpoints"},
            std::pair{ID_VIEW_WATCHPOINTS, "Watchpoints"},
            std::pair{ID_VIEW_REGISTERS, "Registers"},
            std::pair{ID_VIEW_STACK, "Stack"},
            std::pair{ID_VIEW_MEMORY, "Memory"},
            std::pair{ID_VIEW_HEX_EDITOR, "HexEditor"},
            std::pair{ID_VIEW_IMPORTS_EXPORTS, "ImportsExports"},
            std::pair{ID_VIEW_THREADS, "Threads"},
            std::pair{ID_VIEW_WATCH, "Watch"},
            std::pair{ID_VIEW_CONSOLE, "Console"}
        };

        const auto eventId = event.GetId();
        const auto it = std::ranges::find_if(paneMappings, [eventId](const auto& mapping)
        {
            return mapping.first == eventId;
        });

        if (it == paneMappings.end())
        {
            return;
        }

        auto& pane = m_auiManager.GetPane(it->second);
        if (pane.IsOk())
        {
            pane.Show(event.IsChecked());
            m_auiManager.Update();
        }
    }

    void DebuggerView::on_refresh_timer([[maybe_unused]] wxTimerEvent& event)
    {
        if (m_viewModel->is_attached())
        {
            update_view(ViewUpdateFlags::DEBUGGER_REGISTERS | ViewUpdateFlags::DEBUGGER_MEMORY);
        }
    }

    void DebuggerView::on_close(wxCloseEvent& event)
    {
        if (m_refreshTimer)
        {
            m_refreshTimer->Stop();
        }

        const std::string perspective = m_auiManager.SavePerspective().ToStdString();
        m_viewModel->set_aui_perspective(perspective);

        Hide();

        event.Veto();
    }

    void DebuggerView::on_show(wxShowEvent& event)
    {
        if (event.IsShown())
        {
            if (m_viewModel->get_disassembly().lines.empty())
            {
                if (const auto status = m_viewModel->load_modules_and_disassemble(); status != StatusCode::STATUS_OK)
                {
                    wxMessageBox(wxString::FromUTF8(m_languageService.fetch_translation("debugger.errors.failedLoadModulesDisassemble")),
                                 wxString::FromUTF8(m_languageService.fetch_translation("debugger.errors.debuggerError")), wxOK | wxICON_ERROR, this);
                }
            }

            m_viewModel->ensure_data_loaded();

            update_view(ViewUpdateFlags::DEBUGGER_ALL);

            m_pendingUpdateFlags = ViewUpdateFlags::NONE;
            m_hasPendingUpdate = false;
        }

        event.Skip();
    }

    void DebuggerView::navigate_to_address(std::uint64_t address)
    {
        Show();
        Raise();
        m_viewModel->navigate_to_address(address);
        update_view(ViewUpdateFlags::DEBUGGER_DISASSEMBLY);
    }

    void DebuggerView::set_watchpoint(std::uint64_t address, std::uint32_t size)
    {
        Show();
        Raise();
        m_viewModel->set_watchpoint(address, size);
        update_view(ViewUpdateFlags::DEBUGGER_WATCHPOINTS);
    }
}
