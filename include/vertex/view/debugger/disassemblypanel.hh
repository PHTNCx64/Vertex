//
// Copyright (C) 2026 PHTNC<>.
// Licensed under GPLv3.0 with Plugin Interface exceptions.
//
#pragma once

#include <wx/panel.h>
#include <wx/textctrl.h>
#include <wx/sizer.h>

#include <functional>
#include <vertex/debugger/debuggertypes.hh>
#include <vertex/language/language.hh>
#include <vertex/gui/iconmanager/iconmanager.hh>
#include <vertex/view/debugger/disassemblycontrol.hh>

namespace Vertex::View::Debugger
{
    class DisassemblyPanel final : public wxPanel
    {
    public:
        using NavigateCallback = std::function<void(std::uint64_t address)>;
        using BreakpointToggleCallback = std::function<void(std::uint64_t address)>;
        using RunToCursorCallback = std::function<void(std::uint64_t address)>;
        using ScrollBoundaryCallback = std::function<void(std::uint64_t boundaryAddress, bool isTop)>;

        DisassemblyPanel(
            wxWindow* parent,
            Language::ILanguage& languageService,
            Gui::IIconManager& iconManager
        );

        void update_disassembly(const ::Vertex::Debugger::DisassemblyRange& range) const;
        void highlight_address(std::uint64_t address) const;
        void set_breakpoints(const std::vector<::Vertex::Debugger::Breakpoint>& breakpoints) const;
        void scroll_to_address(std::uint64_t address) const;

        void set_navigate_callback(NavigateCallback callback);
        void set_breakpoint_toggle_callback(BreakpointToggleCallback callback);
        void set_run_to_cursor_callback(RunToCursorCallback callback);
        void set_scroll_boundary_callback(ScrollBoundaryCallback callback);

        [[nodiscard]] std::uint64_t get_selected_address() const;
        [[nodiscard]] DisassemblyHeader* get_header() const { return m_disassemblyHeader; }

    private:
        void create_controls();
        void layout_controls();
        void bind_events();

        void on_goto_address(wxCommandEvent& event);
        void on_columns_resized() const;
        void on_columns_reordered() const;

        DisassemblyHeader* m_disassemblyHeader{};
        DisassemblyControl* m_disassemblyControl{};
        wxTextCtrl* m_addressInput{};
        wxButton* m_goButton{};
        wxBoxSizer* m_mainSizer{};
        wxBoxSizer* m_addressBarSizer{};

        NavigateCallback m_navigateCallback{};
        BreakpointToggleCallback m_breakpointToggleCallback{};
        RunToCursorCallback m_runToCursorCallback{};

        Language::ILanguage& m_languageService;
        Gui::IIconManager& m_iconManager;
    };
}
