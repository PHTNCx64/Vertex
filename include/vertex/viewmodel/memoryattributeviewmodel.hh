//
// Copyright (C) 2026 PHTNC<>.
// Licensed under GPLv3.0 with Plugin Interface exceptions.
//
#pragma once

#include <memory>
#include <functional>
#include <string>
#include <vertex/model/memoryattributemodel.hh>
#include <vertex/event/eventbus.hh>
#include <vertex/event/eventid.hh>
#include <vertex/event/types/processopenevent.hh>
#include <vertex/utility.hh>

namespace Vertex::ViewModel
{
    class MemoryAttributeViewModel final
    {
    public:
        explicit MemoryAttributeViewModel(std::unique_ptr<Model::MemoryAttributeModel> model, Event::EventBus& eventBus, std::string name = ViewModelName::MEMORYATTRIBUTES, bool autoApplyOnProcessOpen = true);
        ~MemoryAttributeViewModel();

        [[nodiscard]] bool get_memory_attribute_options(std::vector<Model::MemoryAttributeOptionData>& options) const;
        [[nodiscard]] bool has_options() const;
        [[nodiscard]] bool save_memory_attribute_states(const std::vector<Model::MemoryAttributeOptionData>& options) const;

        void set_event_callback(const std::function<void(Event::EventId, const Event::VertexEvent&)>& eventCallback);

        void apply_saved_memory_attributes() const;

    private:
        void subscribe_to_events() const;
        void unsubscribe_from_events() const;
        void on_process_opened(const Event::ProcessOpenEvent& event) const;

        bool m_autoApplyOnProcessOpen {};
        std::string m_viewModelName {};
        std::unique_ptr<Model::MemoryAttributeModel> m_model {};
        std::function<void(Event::EventId, const Event::VertexEvent&)> m_eventCallback {};

        Event::EventBus& m_eventBus;
    };
}
