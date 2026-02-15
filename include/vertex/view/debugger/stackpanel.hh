//
// Copyright (C) 2026 PHTNC<>.
// Licensed under GPLv3.0 with Plugin Interface exceptions.
//
#pragma once

#include <wx/panel.h>
#include <wx/listctrl.h>
#include <wx/sizer.h>

#include <functional>
#include <vertex/debugger/debuggertypes.hh>
#include <vertex/language/language.hh>

namespace Vertex::View::Debugger
{
    class StackPanel final : public wxPanel
    {
    public:
        using SelectFrameCallback = std::function<void(std::uint32_t frameIndex)>;

        StackPanel(wxWindow* parent, Language::ILanguage& languageService);

        void update_call_stack(const ::Vertex::Debugger::CallStack& stack);
        void set_selected_frame(std::uint32_t frameIndex);

        void set_select_frame_callback(SelectFrameCallback callback);

    private:
        void create_controls();
        void layout_controls();
        void bind_events();

        void on_item_selected(wxListEvent& event);

        wxListCtrl* m_stackList{};
        wxBoxSizer* m_mainSizer{};

        ::Vertex::Debugger::CallStack m_callStack{};
        SelectFrameCallback m_selectFrameCallback{};

        Language::ILanguage& m_languageService;
    };
}
