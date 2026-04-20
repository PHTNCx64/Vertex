//
// Copyright (C) 2026 PHTNC<>.
// Licensed under GPLv3.0 with Plugin Interface exceptions.
//
#pragma once

#include <vertex/customwidgets/accesstrackerpanel.hh>
#include <vertex/event/eventbus.hh>
#include <vertex/language/ilanguage.hh>
#include <vertex/viewmodel/accesstrackerviewmodel.hh>

#include <wx/frame.h>

#include <atomic>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <memory>

namespace Vertex::View
{
    class AccessTrackerView final : public wxFrame
    {
    public:
        using CloseCallback = std::function<void()>;

        AccessTrackerView(std::unique_ptr<ViewModel::AccessTrackerViewModel> viewModel,
            Language::ILanguage& languageService,
            Event::EventBus& eventBus);

        void open_and_track(std::uint64_t address, std::uint32_t size);
        void set_close_callback(CloseCallback callback);

    private:
        void bind_events();
        void refresh_from_view_model() const;
        void request_refresh();

        void on_stop_requested();
        void on_clear_requested();
        void on_close_requested();
        void on_close(wxCloseEvent& event);

        void on_view_in_disassembly(std::size_t rowIndex) const;
        void on_show_call_stack(std::size_t rowIndex);
        void on_copy_address(std::size_t rowIndex) const;
        void on_copy_registers(std::size_t rowIndex) const;

        void broadcast_navigate(std::uint64_t address) const;

        Language::ILanguage& m_languageService;
        Event::EventBus& m_eventBus;
        std::unique_ptr<ViewModel::AccessTrackerViewModel> m_viewModel {};
        CustomWidgets::AccessTrackerPanel* m_panel {};
        std::atomic<bool> m_refreshQueued {false};
        CloseCallback m_closeCallback {};
    };
}
