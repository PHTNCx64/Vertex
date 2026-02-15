//
// Copyright (C) 2026 PHTNC<>.
// Licensed under GPLv3.0 with Plugin Interface exceptions.
//
#pragma once

#include <wx/panel.h>
#include <wx/listctrl.h>
#include <wx/sizer.h>
#include <wx/stattext.h>

#include <functional>
#include <vertex/debugger/debuggertypes.hh>
#include <vertex/language/language.hh>

namespace Vertex::View::Debugger
{
    class ThreadsPanel final : public wxPanel
    {
    public:
        using SelectThreadCallback = std::function<void(std::uint32_t threadId)>;
        using SuspendThreadCallback = std::function<void(std::uint32_t threadId)>;
        using ResumeThreadCallback = std::function<void(std::uint32_t threadId)>;

        ThreadsPanel(wxWindow* parent, Language::ILanguage& languageService);

        void update_threads(const std::vector<::Vertex::Debugger::ThreadInfo>& threads);
        void set_current_thread(std::uint32_t threadId);
        void clear();

        void set_select_callback(SelectThreadCallback callback);
        void set_suspend_callback(SuspendThreadCallback callback);
        void set_resume_callback(ResumeThreadCallback callback);

    private:
        void create_controls();
        void layout_controls();
        void bind_events();

        void on_item_activated(const wxListEvent& event);
        void on_item_right_click(const wxListEvent& event);

        [[nodiscard]] wxString get_state_string(::Vertex::Debugger::ThreadState state) const;

        wxListCtrl* m_threadList{};
        wxBoxSizer* m_mainSizer{};

        std::vector<::Vertex::Debugger::ThreadInfo> m_threads{};
        std::uint32_t m_currentThreadId{};

        SelectThreadCallback m_selectCallback{};
        SuspendThreadCallback m_suspendCallback{};
        ResumeThreadCallback m_resumeCallback{};

        Language::ILanguage& m_languageService;
    };
}
