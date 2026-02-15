//
// Copyright (C) 2026 PHTNC<>.
// Licensed under GPLv3.0 with Plugin Interface exceptions.
//
#include <vertex/view/debugger/threadspanel.hh>
#include <vertex/utility.hh>
#include <wx/menu.h>
#include <wx/clipbrd.h>
#include <fmt/format.h>
#include <ranges>

namespace Vertex::View::Debugger
{
    ThreadsPanel::ThreadsPanel(wxWindow* parent, Language::ILanguage& languageService)
        : wxPanel(parent, wxID_ANY)
        , m_languageService(languageService)
    {
        create_controls();
        layout_controls();
        bind_events();
    }

    void ThreadsPanel::create_controls()
    {
        m_mainSizer = new wxBoxSizer(wxVERTICAL);

        m_threadList = new wxListCtrl(this, wxID_ANY, wxDefaultPosition, wxDefaultSize,
                                       wxLC_REPORT | wxLC_SINGLE_SEL);
        m_threadList->SetFont(wxFont(9, wxFONTFAMILY_TELETYPE, wxFONTSTYLE_NORMAL, wxFONTWEIGHT_NORMAL));

        m_threadList->InsertColumn(0, wxString::FromUTF8(m_languageService.fetch_translation("debugger.threads.columnId")), wxLIST_FORMAT_LEFT, FromDIP(50));
        m_threadList->InsertColumn(1, wxString::FromUTF8(m_languageService.fetch_translation("debugger.threads.columnName")), wxLIST_FORMAT_LEFT, FromDIP(100));
        m_threadList->InsertColumn(2, wxString::FromUTF8(m_languageService.fetch_translation("debugger.threads.columnState")), wxLIST_FORMAT_LEFT, FromDIP(70));
        m_threadList->InsertColumn(3, wxString::FromUTF8(m_languageService.fetch_translation("debugger.threads.columnInstructionPointer")), wxLIST_FORMAT_LEFT, FromDIP(140));
        m_threadList->InsertColumn(4, wxString::FromUTF8(m_languageService.fetch_translation("debugger.threads.columnPriority")), wxLIST_FORMAT_LEFT, FromDIP(60));
    }

    void ThreadsPanel::layout_controls()
    {
        m_mainSizer->Add(m_threadList, 1, wxEXPAND | wxALL, StandardWidgetValues::STANDARD_BORDER);
        SetSizer(m_mainSizer);
    }

    void ThreadsPanel::bind_events()
    {
        m_threadList->Bind(wxEVT_LIST_ITEM_ACTIVATED, &ThreadsPanel::on_item_activated, this);
        m_threadList->Bind(wxEVT_LIST_ITEM_RIGHT_CLICK, &ThreadsPanel::on_item_right_click, this);
    }

    void ThreadsPanel::update_threads(const std::vector<::Vertex::Debugger::ThreadInfo>& threads)
    {
        m_threads = threads;
        m_threadList->DeleteAllItems();

        for (const auto& [i, thread] : threads | std::views::enumerate)
        {
            const wxString idStr = thread.isCurrent ?
                fmt::format("> {}", thread.id) : fmt::format("  {}", thread.id);

            const long idx = m_threadList->InsertItem(static_cast<long>(i), idStr);
            m_threadList->SetItem(idx, 1, thread.name.empty()
                ? wxString::FromUTF8(m_languageService.fetch_translation("debugger.threads.unnamed"))
                : wxString(thread.name));
            m_threadList->SetItem(idx, 2, get_state_string(thread.state));
            m_threadList->SetItem(idx, 3, fmt::format("{:016X}", thread.instructionPointer));
            m_threadList->SetItem(idx, 4, thread.priorityString.empty() ? fmt::format("{}", thread.priority) : thread.priorityString);

            if (thread.isCurrent)
            {
                m_threadList->SetItemBackgroundColour(idx, wxColour(0x26, 0x4F, 0x78));
            }
        }
    }

    void ThreadsPanel::set_current_thread(std::uint32_t threadId)
    {
        m_currentThreadId = threadId;
    }

    void ThreadsPanel::set_select_callback(SelectThreadCallback callback)
    {
        m_selectCallback = std::move(callback);
    }

    void ThreadsPanel::set_suspend_callback(SuspendThreadCallback callback)
    {
        m_suspendCallback = std::move(callback);
    }

    void ThreadsPanel::set_resume_callback(ResumeThreadCallback callback)
    {
        m_resumeCallback = std::move(callback);
    }

    void ThreadsPanel::clear()
    {
        m_threadList->DeleteAllItems();
        m_threads.clear();
        m_currentThreadId = 0;
    }

    void ThreadsPanel::on_item_activated(const wxListEvent& event)
    {
        const long idx = event.GetIndex();
        if (idx >= 0 && static_cast<std::size_t>(idx) < m_threads.size() && m_selectCallback)
        {
            m_selectCallback(m_threads[idx].id);
        }
    }

    void ThreadsPanel::on_item_right_click(const wxListEvent& event)
    {
        const long idx = event.GetIndex();
        if (idx < 0 || static_cast<std::size_t>(idx) >= m_threads.size())
        {
            return;
        }

        const auto& thread = m_threads[idx];

        wxMenu menu;
        menu.Append(1001, wxString::FromUTF8(m_languageService.fetch_translation("debugger.threads.switchToThread")));
        menu.AppendSeparator();

        if (thread.state == Vertex::Debugger::ThreadState::Running)
        {
            menu.Append(1002, wxString::FromUTF8(m_languageService.fetch_translation("debugger.threads.suspendThread")));
        }
        else if (thread.state == Vertex::Debugger::ThreadState::Suspended)
        {
            menu.Append(1003, wxString::FromUTF8(m_languageService.fetch_translation("debugger.threads.resumeThread")));
        }

        menu.AppendSeparator();
        menu.Append(1004, wxString::FromUTF8(m_languageService.fetch_translation("debugger.threads.copyThreadId")));

        const int selection = GetPopupMenuSelectionFromUser(menu, event.GetPoint());
        switch (selection)
        {
            case 1001:
                if (m_selectCallback)
                {
                    m_selectCallback(thread.id);
                }
                break;
            case 1002:
                if (m_suspendCallback)
                {
                    m_suspendCallback(thread.id);
                }
                break;
            case 1003:
                if (m_resumeCallback)
                {
                    m_resumeCallback(thread.id);
                }
                break;
            case 1004:
                if (wxTheClipboard->Open())
                {
                    wxTheClipboard->SetData(new wxTextDataObject(fmt::format("{}", thread.id)));
                    wxTheClipboard->Close();
                }
                break;
            default:
                break;
        }
    }

    wxString ThreadsPanel::get_state_string(const Vertex::Debugger::ThreadState state) const
    {
        switch (state)
        {
            case Vertex::Debugger::ThreadState::Running:
                return wxString::FromUTF8(m_languageService.fetch_translation("debugger.threads.running"));
            case Vertex::Debugger::ThreadState::Suspended:
                return wxString::FromUTF8(m_languageService.fetch_translation("debugger.threads.suspended"));
            case Vertex::Debugger::ThreadState::Waiting:
                return wxString::FromUTF8(m_languageService.fetch_translation("debugger.threads.waiting"));
            case Vertex::Debugger::ThreadState::Terminated:
                return wxString::FromUTF8(m_languageService.fetch_translation("debugger.threads.terminated"));
            default:
                return wxString::FromUTF8(m_languageService.fetch_translation("debugger.threads.unknown"));
        }
    }
}
