//
// Copyright (C) 2026 PHTNC<>.
// Licensed under GPLv3.0 with Plugin Interface exceptions.
//
#pragma once

#include <memory>
#include <string>
#include <string_view>
#include <functional>

#include <vertex/utility.hh>
#include <vertex/event/eventbus.hh>
#include <vertex/model/processlistmodel.hh>
#include <vertex/thread/ithreaddispatcher.hh>

namespace Vertex::ViewModel
{
    class ProcessListViewModel final
    {
    public:
        ProcessListViewModel(
            std::unique_ptr<Model::ProcessListModel> model,
            Event::EventBus& eventBus,
            Thread::IThreadDispatcher& dispatcher,
            std::string name = ViewModelName::PROCESSLIST);

        void set_event_callback(std::move_only_function<void(Event::EventId, const Event::VertexEvent&) const> eventCallback);
        void update_process_list() const;
        void set_filter_type(Enums::FilterType filterType) const;
        void set_sort_order() const;
        void set_filter_text(std::string_view text) const;
        void set_should_filter(bool shouldFilter) const;
        void set_clicked_column(long column) const;
        void filter_list() const;
        void sort_list() const;
        void set_selected_process_from_node(std::size_t nodeIndex) const;
        void clear_selected_process() const;
        void open_process() const;

        [[nodiscard]] std::size_t get_root_count() const;
        [[nodiscard]] std::size_t get_child_count(std::size_t nodeIndex) const;
        [[nodiscard]] std::size_t get_root_node_index(std::size_t pos) const;
        [[nodiscard]] std::size_t get_child_node_index(std::size_t parentNodeIndex, std::size_t pos) const;
        [[nodiscard]] std::string get_node_column_value(std::size_t nodeIndex, long col) const;
        [[nodiscard]] std::size_t get_parent_node_index(std::size_t nodeIndex) const;
        [[nodiscard]] bool node_has_parent(std::size_t nodeIndex) const;
        [[nodiscard]] bool node_is_visible(std::size_t nodeIndex) const;
        [[nodiscard]] bool consume_tree_dirty() const noexcept;

        [[nodiscard]] int get_filter_type_index() const;
        void set_filter_type_with_persist(Enums::FilterType filterType) const;

        static constexpr std::size_t INVALID_NODE_INDEX = Model::ProcessListModel::INVALID_NODE_INDEX;

    private:
        std::string m_viewModelName {};
        std::unique_ptr<Model::ProcessListModel> m_model {};
        std::move_only_function<void(Event::EventId, const Event::VertexEvent&) const> m_eventCallback {};

        Event::EventBus& m_eventBus;
        Thread::IThreadDispatcher& m_dispatcher;
    };
}
