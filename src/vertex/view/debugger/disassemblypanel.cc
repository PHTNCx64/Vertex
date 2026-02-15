//
// Copyright (C) 2026 PHTNC<>.
// Licensed under GPLv3.0 with Plugin Interface exceptions.
//
#include <vertex/view/debugger/disassemblypanel.hh>
#include <vertex/utility.hh>
#include <fmt/format.h>

namespace Vertex::View::Debugger
{
    DisassemblyPanel::DisassemblyPanel(wxWindow* parent,
                                        Language::ILanguage& languageService,
                                        Gui::IIconManager& iconManager)
        : wxPanel(parent, wxID_ANY)
        , m_languageService(languageService)
        , m_iconManager(iconManager)
    {
        create_controls();
        layout_controls();
        bind_events();
    }

    void DisassemblyPanel::create_controls()
    {
        m_mainSizer = new wxBoxSizer(wxVERTICAL);
        m_addressBarSizer = new wxBoxSizer(wxHORIZONTAL);

        m_addressInput = new wxTextCtrl(this, wxID_ANY, "0x", wxDefaultPosition, wxSize(FromDIP(150), -1), wxTE_PROCESS_ENTER);
        m_goButton = new wxButton(this, wxID_ANY, wxString::FromUTF8(m_languageService.fetch_translation("debugger.ui.go")));

        m_disassemblyHeader = new DisassemblyHeader(this, m_languageService);
        m_disassemblyControl = new DisassemblyControl(this, m_languageService, m_disassemblyHeader);
    }

    void DisassemblyPanel::layout_controls()
    {
        m_addressBarSizer->Add(new wxStaticText(this, wxID_ANY, wxString::FromUTF8(m_languageService.fetch_translation("debugger.ui.address"))),
                                0, wxALIGN_CENTER_VERTICAL | wxRIGHT, StandardWidgetValues::STANDARD_BORDER);
        m_addressBarSizer->Add(m_addressInput, 0, wxRIGHT, StandardWidgetValues::STANDARD_BORDER);
        m_addressBarSizer->Add(m_goButton, 0);

        m_mainSizer->Add(m_addressBarSizer, 0, wxEXPAND | wxALL, StandardWidgetValues::STANDARD_BORDER);

        m_mainSizer->Add(m_disassemblyHeader, 0, wxEXPAND | wxLEFT | wxRIGHT,
                          StandardWidgetValues::STANDARD_BORDER);

        m_mainSizer->Add(m_disassemblyControl, 1, wxEXPAND | wxLEFT | wxRIGHT | wxBOTTOM,
                          StandardWidgetValues::STANDARD_BORDER);

        SetSizer(m_mainSizer);
    }

    void DisassemblyPanel::bind_events()
    {
        m_goButton->Bind(wxEVT_BUTTON, &DisassemblyPanel::on_goto_address, this);
        m_addressInput->Bind(wxEVT_TEXT_ENTER, &DisassemblyPanel::on_goto_address, this);

        m_disassemblyHeader->set_column_resize_callback([this]()
        {
            on_columns_resized();
        });
        m_disassemblyHeader->set_column_reorder_callback([this]()
        {
            on_columns_reordered();
        });
    }

    void DisassemblyPanel::on_columns_resized() const
    {
        m_disassemblyControl->on_columns_changed();
    }

    void DisassemblyPanel::on_columns_reordered() const
    {
        m_disassemblyControl->on_columns_changed();
    }

    void DisassemblyPanel::update_disassembly(const ::Vertex::Debugger::DisassemblyRange& range) const
    {
        m_disassemblyControl->set_disassembly(range);
    }

    void DisassemblyPanel::highlight_address(const std::uint64_t address) const
    {
        m_disassemblyControl->set_current_instruction(address);
        m_disassemblyControl->scroll_to_address(address);
    }

    void DisassemblyPanel::set_breakpoints(const std::vector<::Vertex::Debugger::Breakpoint>& breakpoints) const
    {
        std::vector<std::uint64_t> addresses;
        addresses.reserve(breakpoints.size());
        for (const auto& bp : breakpoints)
        {
            addresses.push_back(bp.address);
        }
        m_disassemblyControl->set_breakpoints(addresses);
    }

    void DisassemblyPanel::scroll_to_address(const std::uint64_t address) const
    {
        m_disassemblyControl->scroll_to_address(address);
    }

    void DisassemblyPanel::set_navigate_callback(NavigateCallback callback)
    {
        m_navigateCallback = callback;
        m_disassemblyControl->set_navigate_callback(std::move(callback));
    }

    void DisassemblyPanel::set_breakpoint_toggle_callback(BreakpointToggleCallback callback)
    {
        m_breakpointToggleCallback = callback;
        m_disassemblyControl->set_breakpoint_toggle_callback(std::move(callback));
    }

    void DisassemblyPanel::set_run_to_cursor_callback(RunToCursorCallback callback)
    {
        m_runToCursorCallback = std::move(callback);
    }

    void DisassemblyPanel::set_scroll_boundary_callback(ScrollBoundaryCallback callback)
    {
        m_disassemblyControl->set_scroll_boundary_callback(std::move(callback));
    }

    std::uint64_t DisassemblyPanel::get_selected_address() const
    {
        return m_disassemblyControl->get_selected_address();
    }

    void DisassemblyPanel::on_goto_address([[maybe_unused]] wxCommandEvent& event)
    {
        const wxString input = m_addressInput->GetValue();
        unsigned long long address{};
        if (input.ToULongLong(&address, 16) && m_navigateCallback)
        {
            m_navigateCallback(address);
        }
    }

}
