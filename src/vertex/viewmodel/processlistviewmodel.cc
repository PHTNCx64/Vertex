//
// Copyright (C) 2026 PHTNC<>.
// Licensed under GPLv3.0 with Plugin Interface exceptions.
//
#include <vertex/viewmodel/processlistviewmodel.hh>

#include <utility>

#include <vertex/event/types/processopenevent.hh>
#include <vertex/event/types/viewevent.hh>

namespace Vertex::ViewModel
{
    ProcessListViewModel::ProcessListViewModel(std::unique_ptr<Model::ProcessListModel> model, Event::EventBus& eventBus, std::string name)
        : m_viewModelName(std::move(name)),
          m_model(std::move(model)),
          m_eventBus(eventBus)
    {
        m_eventBus.subscribe<Event::ViewEvent>(m_viewModelName, Event::VIEW_EVENT,
                                               [this](const Event::ViewEvent& event)
                                               {
                                                   m_eventCallback(Event::VIEW_EVENT, event);
                                               });
    }

    void ProcessListViewModel::set_event_callback(const std::function<void(Event::EventId, const Event::VertexEvent&)>& eventCallback) { m_eventCallback = eventCallback; }

    std::string ProcessListViewModel::get_process_item(const long item, const long column) const { return m_model->get_process_item(item, column); }

    int ProcessListViewModel::get_processes_count() const { return static_cast<int>(m_model->get_processes_count()); }

    void ProcessListViewModel::update_process_list()
    {
        if (m_processListThread.is_busy() == StatusCode::STATUS_ERROR_THREAD_IS_BUSY) [[unlikely]]
        {
            return;
        }

        std::packaged_task<StatusCode()> task(
          [this]() -> StatusCode
          {
              const auto processesStatus = m_model->get_process_list();
              m_model->filter_list();
              m_model->sort_list();

              return processesStatus;
          });

        m_processListThread.enqueue_task(std::move(task));
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

    void ProcessListViewModel::set_selected_process(const long itemIndex) const
    {
        m_model->make_selected_process_from_id(itemIndex);
    }

    void ProcessListViewModel::clear_selected_process() const
    {
        m_model->clear_selected_process();
    }

    void ProcessListViewModel::open_process() const
    {
        if (m_model->open_process() != StatusCode::STATUS_OK) [[unlikely]]
        {
            return;
        }

        const Class::SelectedProcess process = m_model->get_selected_process();
        const Event::ProcessOpenEvent processOpenEvent { Event::PROCESS_OPEN_EVENT, process.get_selected_process_id().value(), process.get_selected_process_name().value() };

        m_eventBus.broadcast_to(ViewModelName::PROCESSLIST, Event::ViewEvent(Event::VIEW_EVENT));
        m_eventBus.broadcast(processOpenEvent);
    }

    void ProcessListViewModel::set_filter_text(const std::string_view text) const
    {
        m_model->set_filter_text(std::string{text});
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
