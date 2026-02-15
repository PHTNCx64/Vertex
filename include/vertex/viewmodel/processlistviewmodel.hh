//
// Copyright (C) 2026 PHTNC<>.
// Licensed under GPLv3.0 with Plugin Interface exceptions.
//
#pragma once

#include <memory>
#include <string>
#include <string_view>

#include <vertex/utility.hh>
#include <vertex/event/eventbus.hh>
#include <vertex/model/processlistmodel.hh>
#include <vertex/thread/vertexspscthread.hh>

namespace Vertex::ViewModel
{
    class ProcessListViewModel final
    {
    public:
        ProcessListViewModel(
            std::unique_ptr<Model::ProcessListModel> model,
            Event::EventBus& eventBus,
            std::string name = ViewModelName::PROCESSLIST);

        [[nodiscard]] int get_processes_count() const;
        [[nodiscard]] std::string get_process_item(long item, long column) const;
        void set_event_callback(const std::function<void(Event::EventId, const Event::VertexEvent&)>& eventCallback);
        void update_process_list();
        void set_filter_type(Enums::FilterType filterType) const;
        void set_sort_order() const;
        void set_filter_text(std::string_view text) const;
        void set_should_filter(bool shouldFilter) const;
        void set_clicked_column(long column) const;
        void filter_list() const;
        void sort_list() const;
        void set_selected_process(long itemIndex) const;
        void clear_selected_process() const;
        void open_process() const;

        [[nodiscard]] int get_filter_type_index() const;
        void set_filter_type_with_persist(Enums::FilterType filterType) const;

    private:
        std::string m_viewModelName {};
        std::unique_ptr<Model::ProcessListModel> m_model {};
        std::function<void(Event::EventId, const Event::VertexEvent&)> m_eventCallback {};

        Event::EventBus& m_eventBus;

        Thread::VertexSPSCThread m_processListThread {};
    };
}
