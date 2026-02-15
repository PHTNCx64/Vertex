//
// Copyright (C) 2026 PHTNC<>.
// Licensed under GPLv3.0 with Plugin Interface exceptions.
//
#include <vertex/viewmodel/analyticsviewmodel.hh>
#include <vertex/event/eventbus.hh>
#include <vertex/event/types/viewevent.hh>

namespace Vertex::ViewModel
{
    AnalyticsViewModel::AnalyticsViewModel(std::unique_ptr<Model::AnalyticsModel> model, Event::EventBus& eventBus, std::string name)
        : m_viewModelName(std::move(name)),
          m_model(std::move(model)),
          m_eventBus(eventBus)
    {
        subscribe_to_events();
    }

    AnalyticsViewModel::~AnalyticsViewModel()
    {
        unsubscribe_from_events();
    }

    void AnalyticsViewModel::subscribe_to_events() const
    {
        m_eventBus.subscribe<Event::ViewEvent>(m_viewModelName, Event::VIEW_EVENT,
                                               [this](const Event::ViewEvent& event)
                                               {
                                                   if (m_eventCallback)
                                                   {
                                                       m_eventCallback(Event::VIEW_EVENT, event);
                                                   }
                                               });
    }

    void AnalyticsViewModel::unsubscribe_from_events() const { m_eventBus.unsubscribe(m_viewModelName, Event::VIEW_EVENT); }

    void AnalyticsViewModel::set_event_callback(const std::function<void(Event::EventId, const Event::VertexEvent&)>& eventCallback) { m_eventCallback = eventCallback; }

    std::vector<Log::LogEntry> AnalyticsViewModel::get_log_entries(const std::size_t maxEntries) const { return m_model->get_logs(maxEntries); }

    void AnalyticsViewModel::clear_logs() const { m_model->clear_logs(); }

    bool AnalyticsViewModel::save_logs_to_file(const std::string_view filePath) const
    {
        const auto entries = m_model->get_logs();
        return m_model->save_logs_to_file(filePath, entries);
    }
} // namespace Vertex::ViewModel
