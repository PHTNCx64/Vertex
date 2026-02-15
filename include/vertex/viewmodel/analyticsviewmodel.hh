//
// Copyright (C) 2026 PHTNC<>.
// Licensed under GPLv3.0 with Plugin Interface exceptions.
//
#pragma once

#include <vertex/model/analyticsmodel.hh>
#include <vertex/event/eventbus.hh>
#include <vertex/event/types/viewevent.hh>
#include <vertex/log/log.hh>
#include <memory>
#include <vector>
#include <string>
#include <string_view>
#include <functional>

namespace Vertex::ViewModel
{
    class AnalyticsViewModel final
    {
    public:
        AnalyticsViewModel(std::unique_ptr<Model::AnalyticsModel> model, Event::EventBus& eventBus, std::string name);
        ~AnalyticsViewModel();

        void subscribe_to_events() const;
        void unsubscribe_from_events() const;
        void set_event_callback(const std::function<void(Event::EventId, const Event::VertexEvent&)>& eventCallback);

        [[nodiscard]] std::vector<Log::LogEntry> get_log_entries(std::size_t maxEntries = 1000) const;
        void clear_logs() const;
        [[nodiscard]] bool save_logs_to_file(std::string_view filePath) const;

    private:
        std::string m_viewModelName {};
        std::unique_ptr<Model::AnalyticsModel> m_model {};
        std::function<void(Event::EventId, const Event::VertexEvent&)> m_eventCallback {};

        Event::EventBus& m_eventBus;
    };
}
