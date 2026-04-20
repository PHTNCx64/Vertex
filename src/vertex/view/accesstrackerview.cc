//
// Copyright (C) 2026 PHTNC<>.
// Licensed under GPLv3.0 with Plugin Interface exceptions.
//
#include <vertex/view/accesstrackerview.hh>
#include <vertex/customwidgets/accesstrackercallstackdialog.hh>
#include <vertex/event/types/debuggerevent.hh>
#include <vertex/utility.hh>

#include <wx/app.h>
#include <wx/clipbrd.h>

#include <fmt/format.h>

#include <utility>

namespace Vertex::View
{
    AccessTrackerView::AccessTrackerView(std::unique_ptr<ViewModel::AccessTrackerViewModel> viewModel,
        Language::ILanguage& languageService,
        Event::EventBus& eventBus)
        : wxFrame(wxTheApp->GetTopWindow(), wxID_ANY,
            wxString::FromUTF8(languageService.fetch_translation("accessTracker.title")),
            wxDefaultPosition,
            wxSize(FromDIP(StandardWidgetValues::STANDARD_X_DIP), FromDIP(StandardWidgetValues::STANDARD_Y_DIP)))
        , m_languageService(languageService)
        , m_eventBus(eventBus)
        , m_viewModel(std::move(viewModel))
    {
        m_panel = new CustomWidgets::AccessTrackerPanel(this, m_languageService, *m_viewModel);

        m_viewModel->set_status_changed_callback(
            [this](const ViewModel::TrackingState&) { request_refresh(); });
        m_viewModel->set_entries_changed_callback(
            [this]() { request_refresh(); });

        bind_events();
        refresh_from_view_model();
        Hide();
    }

    void AccessTrackerView::open_and_track(const std::uint64_t address, const std::uint32_t size)
    {
        m_viewModel->start_tracking(address, size);
        request_refresh();

        if (!IsShown())
        {
            Show();
        }
        Raise();
    }

    void AccessTrackerView::set_close_callback(CloseCallback callback)
    {
        m_closeCallback = std::move(callback);
    }

    void AccessTrackerView::bind_events()
    {
        Bind(wxEVT_CLOSE_WINDOW, &AccessTrackerView::on_close, this);

        m_panel->set_stop_callback([this]()
        {
            on_stop_requested();
        });
        m_panel->set_clear_callback([this]()
        {
            on_clear_requested();
        });
        m_panel->set_close_callback([this]()
        {
            on_close_requested();
        });
        m_panel->set_view_in_disassembly_callback([this](const std::size_t row)
        {
            on_view_in_disassembly(row);
        });
        m_panel->set_show_call_stack_callback([this](const std::size_t row)
        {
            on_show_call_stack(row);
        });
        m_panel->set_copy_address_callback([this](const std::size_t row)
        {
            on_copy_address(row);
        });
        m_panel->set_copy_registers_callback([this](const std::size_t row)
        {
            on_copy_registers(row);
        });
    }

    void AccessTrackerView::refresh_from_view_model() const
    {
        m_panel->set_state(m_viewModel->get_state());
        m_panel->notify_entries_changed();
    }

    void AccessTrackerView::request_refresh()
    {
        bool expected {};
        if (!m_refreshQueued.compare_exchange_strong(expected, true, std::memory_order_acq_rel))
        {
            return;
        }

        CallAfter([this]()
        {
            m_refreshQueued.store(false, std::memory_order_release);
            refresh_from_view_model();
        });
    }

    void AccessTrackerView::on_stop_requested()
    {
        m_viewModel->stop_tracking();
        request_refresh();
    }

    void AccessTrackerView::on_clear_requested()
    {
        m_viewModel->clear();
        request_refresh();
    }

    void AccessTrackerView::on_close_requested()
    {
        Close();
    }

    void AccessTrackerView::on_close(wxCloseEvent& event)
    {
        if (m_closeCallback)
        {
            m_closeCallback();
        }

        const auto status = m_viewModel->get_state().status;
        if (status == ViewModel::TrackingStatus::Active
            || status == ViewModel::TrackingStatus::StopFailed)
        {
            m_viewModel->stop_tracking();
        }
        else if (status == ViewModel::TrackingStatus::StartFailed)
        {
            m_viewModel->acknowledge_start_failure();
        }

        if (event.CanVeto())
        {
            Hide();
            event.Veto();
            return;
        }

        Destroy();
    }

    void AccessTrackerView::on_view_in_disassembly(const std::size_t rowIndex) const
    {
        const auto entry = m_viewModel->snapshot_entry_at(rowIndex);
        if (!entry.has_value())
        {
            return;
        }
        broadcast_navigate(entry->instructionAddress);
    }

    void AccessTrackerView::on_show_call_stack(const std::size_t rowIndex)
    {
        const auto entry = m_viewModel->snapshot_entry_at(rowIndex);
        if (!entry.has_value())
        {
            return;
        }

        CustomWidgets::AccessTrackerCallStackDialog dialog {
            this,
            m_languageService,
            entry->lastCallStack,
            [this](const std::uint64_t address)
            {
                broadcast_navigate(address);
            }
        };
        std::ignore = dialog.ShowModal();
    }

    void AccessTrackerView::on_copy_address(const std::size_t rowIndex) const
    {
        const auto entry = m_viewModel->snapshot_entry_at(rowIndex);
        if (!entry.has_value())
        {
            return;
        }
        if (!wxTheClipboard->Open())
        {
            return;
        }
        wxTheClipboard->SetData(new wxTextDataObject(
            wxString::FromUTF8(fmt::format("{:X}", entry->instructionAddress))));
        wxTheClipboard->Close();
    }

    void AccessTrackerView::on_copy_registers(const std::size_t rowIndex) const
    {
        const auto entry = m_viewModel->snapshot_entry_at(rowIndex);
        if (!entry.has_value())
        {
            return;
        }

        std::string text {};
        text.reserve(256);
        for (const auto& reg : entry->lastRegisters.generalPurpose)
        {
            if (!text.empty())
            {
                text.append("\n");
            }
            text.append(fmt::format("{} = {:X}", reg.name, reg.value));
        }

        if (text.empty())
        {
            return;
        }

        if (!wxTheClipboard->Open())
        {
            return;
        }
        wxTheClipboard->SetData(new wxTextDataObject(wxString::FromUTF8(text)));
        wxTheClipboard->Close();
    }

    void AccessTrackerView::broadcast_navigate(const std::uint64_t address) const
    {
        const Event::DebuggerNavigateEvent navEvent {address, Event::NavigationTarget::Disassembly};
        m_eventBus.broadcast(navEvent);
    }
}
