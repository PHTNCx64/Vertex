//
// Copyright (C) 2026 PHTNC<>.
// Licensed under GPLv3.0 with Plugin Interface exceptions.
//
#include <vertex/viewmodel/injectorviewmodel.hh>

#include <utility>

#include <fmt/format.h>

#include <vertex/event/types/viewevent.hh>

namespace Vertex::ViewModel
{
    InjectorViewModel::InjectorViewModel(std::unique_ptr<Model::InjectorModel> model, Event::EventBus& eventBus, Log::ILog& logService, std::string name)
        : m_viewModelName{std::move(name)},
          m_model{std::move(model)},
          m_eventBus{eventBus},
          m_logService{logService}
    {
        subscribe_to_events();
    }

    InjectorViewModel::~InjectorViewModel()
    {
        unsubscribe_from_events();
    }

    void InjectorViewModel::subscribe_to_events() const
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

    void InjectorViewModel::unsubscribe_from_events() const
    {
        m_eventBus.unsubscribe(m_viewModelName, Event::VIEW_EVENT);
    }

    void InjectorViewModel::set_event_callback(const std::function<void(Event::EventId, const Event::VertexEvent&)>& eventCallback)
    {
        m_eventCallback = eventCallback;
    }

    const std::vector<InjectionMethod>& InjectorViewModel::get_injection_methods() const noexcept
    {
        return m_injectionMethods;
    }

    const std::vector<std::string>& InjectorViewModel::get_library_extensions() const noexcept
    {
        return m_libraryExtensions;
    }

    void InjectorViewModel::load_injection_methods()
    {
        if (const auto status = m_model->get_injection_methods(m_injectionMethods); status != StatusCode::STATUS_OK) [[unlikely]]
        {
            m_logService.log_error(fmt::format("InjectorViewModel: failed to load injection methods (status={})", static_cast<int>(status)));
        }
    }

    void InjectorViewModel::load_library_extensions()
    {
        if (const auto status = m_model->get_library_extensions(m_libraryExtensions); status != StatusCode::STATUS_OK) [[unlikely]]
        {
            m_logService.log_error(fmt::format("InjectorViewModel: failed to load library extensions (status={})", static_cast<int>(status)));
        }
    }

    void InjectorViewModel::set_selected_method_index(const int index)
    {
        m_selectedMethodIndex = index;
    }

    int InjectorViewModel::get_selected_method_index() const noexcept
    {
        return m_selectedMethodIndex;
    }

    std::string_view InjectorViewModel::get_selected_method_description() const
    {
        if (m_selectedMethodIndex < 0 || m_selectedMethodIndex >= static_cast<int>(m_injectionMethods.size()))
        {
            return {};
        }

        return m_injectionMethods[m_selectedMethodIndex].description;
    }

    StatusCode InjectorViewModel::inject(const std::string_view libraryPath) const
    {
        if (m_selectedMethodIndex < 0 || m_selectedMethodIndex >= static_cast<int>(m_injectionMethods.size())) [[unlikely]]
        {
            return StatusCode::STATUS_ERROR_INVALID_PARAMETER;
        }

        const auto& method = m_injectionMethods[m_selectedMethodIndex];
        const auto status = m_model->inject(method, libraryPath);

        if (status != StatusCode::STATUS_OK) [[unlikely]]
        {
            m_logService.log_error(fmt::format("InjectorViewModel: injection failed using '{}' (status={})", method.methodName, static_cast<int>(status)));
        }

        return status;
    }
}
