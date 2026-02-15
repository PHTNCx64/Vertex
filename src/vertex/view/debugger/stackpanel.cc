//
// Copyright (C) 2026 PHTNC<>.
// Licensed under GPLv3.0 with Plugin Interface exceptions.
//
#include <vertex/view/debugger/stackpanel.hh>
#include <vertex/utility.hh>
#include <fmt/format.h>
#include <ranges>

namespace Vertex::View::Debugger
{
    StackPanel::StackPanel(wxWindow* parent, Language::ILanguage& languageService)
        : wxPanel(parent, wxID_ANY)
        , m_languageService(languageService)
    {
        create_controls();
        layout_controls();
        bind_events();
    }

    void StackPanel::create_controls()
    {
        m_mainSizer = new wxBoxSizer(wxVERTICAL);

        m_stackList = new wxListCtrl(this, wxID_ANY, wxDefaultPosition, wxDefaultSize,
                                      wxLC_REPORT | wxLC_SINGLE_SEL);
        m_stackList->SetFont(wxFont(9, wxFONTFAMILY_TELETYPE, wxFONTSTYLE_NORMAL, wxFONTWEIGHT_NORMAL));

        m_stackList->InsertColumn(0, wxString::FromUTF8(m_languageService.fetch_translation("debugger.stack.columnFrame")), wxLIST_FORMAT_LEFT, FromDIP(30));
        m_stackList->InsertColumn(1, wxString::FromUTF8(m_languageService.fetch_translation("debugger.stack.columnReturnAddress")), wxLIST_FORMAT_LEFT, FromDIP(120));
        m_stackList->InsertColumn(2, wxString::FromUTF8(m_languageService.fetch_translation("debugger.stack.columnFunction")), wxLIST_FORMAT_LEFT, FromDIP(150));
        m_stackList->InsertColumn(3, wxString::FromUTF8(m_languageService.fetch_translation("debugger.stack.columnModule")), wxLIST_FORMAT_LEFT, FromDIP(100));
    }

    void StackPanel::layout_controls()
    {
        m_mainSizer->Add(m_stackList, 1, wxEXPAND | wxALL, StandardWidgetValues::STANDARD_BORDER);
        SetSizer(m_mainSizer);
    }

    void StackPanel::bind_events()
    {
        m_stackList->Bind(wxEVT_LIST_ITEM_SELECTED, &StackPanel::on_item_selected, this);
    }

    void StackPanel::update_call_stack(const ::Vertex::Debugger::CallStack& stack)
    {
        m_callStack = stack;
        m_stackList->DeleteAllItems();

        for (const auto& [i, frame] : stack.frames | std::views::enumerate)
        {
            const long idx = m_stackList->InsertItem(static_cast<long>(i), std::to_string(frame.frameIndex));
            m_stackList->SetItem(idx, 1, fmt::format("0x{:X}", frame.returnAddress));
            m_stackList->SetItem(idx, 2, frame.functionName.empty() ? wxString::FromUTF8(m_languageService.fetch_translation("debugger.ui.unknown")) : wxString::FromUTF8(frame.functionName));
            m_stackList->SetItem(idx, 3, frame.moduleName.empty() ? wxString::FromUTF8(m_languageService.fetch_translation("debugger.ui.unknown")) : wxString::FromUTF8(frame.moduleName));
        }
    }

    void StackPanel::set_selected_frame(const std::uint32_t frameIndex)
    {
        if (frameIndex < static_cast<std::uint32_t>(m_stackList->GetItemCount()))
        {
            m_stackList->SetItemState(static_cast<long>(frameIndex),
                wxLIST_STATE_SELECTED | wxLIST_STATE_FOCUSED,
                wxLIST_STATE_SELECTED | wxLIST_STATE_FOCUSED);
        }
    }

    void StackPanel::set_select_frame_callback(SelectFrameCallback callback)
    {
        m_selectFrameCallback = std::move(callback);
    }

    void StackPanel::on_item_selected(wxListEvent& event)
    {
        const long idx = event.GetIndex();
        if (idx >= 0 && m_selectFrameCallback)
        {
            m_selectFrameCallback(static_cast<std::uint32_t>(idx));
        }
    }

}
