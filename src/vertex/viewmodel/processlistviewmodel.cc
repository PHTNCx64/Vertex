//
// Copyright (C) 2026 PHTNC<>.
// Licensed under GPLv3.0 with Plugin Interface exceptions.
//
#include <vertex/viewmodel/processlistviewmodel.hh>

#include <utility>

#include <vertex/event/types/processopenevent.hh>
#include <vertex/event/types/viewevent.hh>
#include <vertex/thread/threadchannel.hh>

namespace Vertex::ViewModel
{
    ProcessListViewModel::ProcessListViewModel(std::unique_ptr<Model::ProcessListModel> model, Event::EventBus& eventBus, Thread::IThreadDispatcher& dispatcher, std::string name)
        : m_viewModelName{std::move(name)},
          m_model{std::move(model)},
          m_eventBus{eventBus},
          m_dispatcher{dispatcher}
    {
        m_eventBus.subscribe<Event::ViewEvent>(m_viewModelName, Event::VIEW_EVENT,
                                               [this](const Event::ViewEvent& event)
                                               {
                                                   m_eventCallback(Event::VIEW_EVENT, event);
                                               });
    }

    void ProcessListViewModel::set_event_callback(std::move_only_function<void(Event::EventId, const Event::VertexEvent&) const> eventCallback) { m_eventCallback = std::move(eventCallback); }

    void ProcessListViewModel::update_process_list() const
    {
        if (m_dispatcher.is_channel_busy(Thread::ThreadChannel::ProcessList)) [[unlikely]]
        {
            return;
        }

        std::packaged_task<StatusCode()> task(
          [this]() -> StatusCode
          {
              const auto processesStatus = m_model->get_process_list();
              m_model->build_tree();
              m_model->filter_list();
              m_model->sort_list();

              return processesStatus;
          });

        std::ignore = m_dispatcher.dispatch_fire_and_forget(Thread::ThreadChannel::ProcessList, std::move(task));
    }

    void ProcessListViewModel::set_filter_type(const Enums::FilterType filterType) const
    {
        m_model->set_filter_type(filterType);
    }

    void ProcessListViewModel::set_sort_order() const
    {
        const Enums::SortOrder sortOrder = m_model->get_sort_order() == Enums::SortOrder::ASCENDING ? Enums::SortOrder::DESCENDING : Enums::SortOrder::ASCENDING;
        m_model->set_sort_order(sortOrder);
    }

    void ProcessListViewModel::set_clicked_column(const long column) const
    {
        m_model->set_clicked_column(column);
    }

    void ProcessListViewModel::set_should_filter(const bool shouldFilter) const
    {
        m_model->set_should_filter(shouldFilter);
    }

    void ProcessListViewModel::filter_list() const
    {
        m_model->filter_list();
    }

    void ProcessListViewModel::sort_list() const
    {
        m_model->sort_list();
    }

    void ProcessListViewModel::set_selected_process_from_node(const std::size_t nodeIndex) const
    {
        std::ignore = m_model->make_selected_process_from_node(nodeIndex);
    }

    void ProcessListViewModel::clear_selected_process() const
    {
        m_model->clear_selected_process();
    }

    void ProcessListViewModel::open_process() const
    {
        const Class::SelectedProcess process = m_model->get_selected_process();

        std::packaged_task<StatusCode()> task(
            [this, process]() -> StatusCode
            {
                const auto status = m_model->open_process();
                if (status != StatusCode::STATUS_OK) [[unlikely]]
                {
                    return status;
                }

                const Event::ProcessOpenEvent processOpenEvent{
                    Event::PROCESS_OPEN_EVENT,
                    process.get_selected_process_id().value(),
                    process.get_selected_process_name().value()};

                m_eventBus.broadcast_to(ViewModelName::PROCESSLIST, Event::ViewEvent(Event::VIEW_EVENT));
                m_eventBus.broadcast(processOpenEvent);

                return StatusCode::STATUS_OK;
            });

        std::ignore = m_dispatcher.dispatch_fire_and_forget(Thread::ThreadChannel::ProcessList, std::move(task));
    }

    void ProcessListViewModel::set_filter_text(const std::string_view text) const
    {
        m_model->set_filter_text(std::string{text});
    }

    std::size_t ProcessListViewModel::get_root_count() const
    {
        return m_model->get_root_count();
    }

    std::size_t ProcessListViewModel::get_child_count(const std::size_t nodeIndex) const
    {
        return m_model->get_child_count(nodeIndex);
    }

    std::size_t ProcessListViewModel::get_root_node_index(const std::size_t pos) const
    {
        return m_model->get_root_node_index(pos);
    }

    std::size_t ProcessListViewModel::get_child_node_index(const std::size_t parentNodeIndex, const std::size_t pos) const
    {
        return m_model->get_child_node_index(parentNodeIndex, pos);
    }

    std::string ProcessListViewModel::get_node_column_value(const std::size_t nodeIndex, const long col) const
    {
        return m_model->get_node_column_value(nodeIndex, col);
    }

    std::size_t ProcessListViewModel::get_parent_node_index(const std::size_t nodeIndex) const
    {
        return m_model->get_parent_node_index(nodeIndex);
    }

    bool ProcessListViewModel::node_has_parent(const std::size_t nodeIndex) const
    {
        return m_model->node_has_parent(nodeIndex);
    }

    bool ProcessListViewModel::node_is_visible(const std::size_t nodeIndex) const
    {
        return m_model->node_is_visible(nodeIndex);
    }

    bool ProcessListViewModel::consume_tree_dirty() const noexcept
    {
        return m_model->consume_tree_dirty();
    }

    int ProcessListViewModel::get_filter_type_index() const
    {
        return m_model->get_ui_state_int("uiState.processListView.filterTypeIndex", 1);
    }

    void ProcessListViewModel::set_filter_type_with_persist(const Enums::FilterType filterType) const
    {
        m_model->set_filter_type(filterType);
        m_model->set_ui_state_int("uiState.processListView.filterTypeIndex", static_cast<int>(filterType));
    }
} // namespace Vertex::ViewModel
