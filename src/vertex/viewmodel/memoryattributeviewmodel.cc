//
// Copyright (C) 2026 PHTNC<>.
// Licensed under GPLv3.0 with Plugin Interface exceptions.
//
#include <vertex/viewmodel/memoryattributeviewmodel.hh>
#include <vertex/event/types/viewevent.hh>

namespace Vertex::ViewModel
{
    MemoryAttributeViewModel::MemoryAttributeViewModel(std::unique_ptr<Model::MemoryAttributeModel> model, Event::EventBus& eventBus, std::string name, const bool autoApplyOnProcessOpen)
        : m_autoApplyOnProcessOpen{autoApplyOnProcessOpen}
        , m_viewModelName{std::move(name)}
        , m_model{std::move(model)}
        , m_eventBus{eventBus}
    {
        subscribe_to_events();
    }

    MemoryAttributeViewModel::~MemoryAttributeViewModel()
    {
        unsubscribe_from_events();
    }

    void MemoryAttributeViewModel::subscribe_to_events() const
    {
        m_eventBus.subscribe<Event::ViewEvent>(m_viewModelName, Event::VIEW_EVENT,
                                               [this](const Event::ViewEvent& event)
                                               {
                                                   if (m_eventCallback)
                                                   {
                                                       m_eventCallback(Event::VIEW_EVENT, event);
                                                   }
                                               });

        if (m_autoApplyOnProcessOpen)
        {
            m_eventBus.subscribe<Event::ProcessOpenEvent>(m_viewModelName, Event::PROCESS_OPEN_EVENT,
                                                          [this](const Event::ProcessOpenEvent& event)
                                                          {
                                                              on_process_opened(event);
                                                          });
        }
    }

    void MemoryAttributeViewModel::unsubscribe_from_events() const
    {
        m_eventBus.unsubscribe(m_viewModelName, Event::VIEW_EVENT);
        if (m_autoApplyOnProcessOpen)
        {
            m_eventBus.unsubscribe(m_viewModelName, Event::PROCESS_OPEN_EVENT);
        }
    }

    void MemoryAttributeViewModel::on_process_opened([[maybe_unused]] const Event::ProcessOpenEvent& event) const
    {
        apply_saved_memory_attributes();
    }

    void MemoryAttributeViewModel::set_event_callback(std::move_only_function<void(Event::EventId, const Event::VertexEvent&) const> eventCallback)
    {
        m_eventCallback = std::move(eventCallback);
    }

    bool MemoryAttributeViewModel::get_memory_attribute_options(std::vector<Model::MemoryAttributeOptionData>& options) const
    {
        return m_model->fetch_memory_attribute_options(options) == StatusCode::STATUS_OK;
    }

    bool MemoryAttributeViewModel::has_options() const
    {
        return m_model->has_memory_attribute_options();
    }

    bool MemoryAttributeViewModel::save_memory_attribute_states(const std::vector<Model::MemoryAttributeOptionData>& options) const
    {
        return m_model->save_memory_attribute_states(options) == StatusCode::STATUS_OK;
    }

    void MemoryAttributeViewModel::apply_saved_memory_attributes() const
    {
        std::vector<Model::MemoryAttributeOptionData> options{};
        [[maybe_unused]] const bool success = m_model->fetch_memory_attribute_options(options) == StatusCode::STATUS_OK;
    }
} // namespace Vertex::ViewModel
