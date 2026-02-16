//
// Copyright (C) 2026 PHTNC<>.
// Licensed under GPLv3.0 with Plugin Interface exceptions.
//
#pragma once

#include <memory>
#include <string>
#include <string_view>
#include <functional>
#include <vector>

#include <vertex/event/eventbus.hh>
#include <vertex/model/injectormodel.hh>
#include <vertex/log/ilog.hh>
#include <vertex/utility.hh>

#include <sdk/process.h>

namespace Vertex::ViewModel
{
    class InjectorViewModel final
    {
    public:
        InjectorViewModel(
            std::unique_ptr<Model::InjectorModel> model,
            Event::EventBus& eventBus,
            Log::ILog& logService,
            std::string name = ViewModelName::INJECTOR
        );

        ~InjectorViewModel();

        void set_event_callback(const std::function<void(Event::EventId, const Event::VertexEvent&)>& eventCallback);

        [[nodiscard]] const std::vector<InjectionMethod>& get_injection_methods() const noexcept;
        [[nodiscard]] const std::vector<std::string>& get_library_extensions() const noexcept;

        void load_injection_methods();
        void load_library_extensions();

        void set_selected_method_index(int index);
        [[nodiscard]] int get_selected_method_index() const noexcept;
        [[nodiscard]] std::string_view get_selected_method_description() const;

        [[nodiscard]] StatusCode inject(std::string_view libraryPath) const;

    private:
        void subscribe_to_events() const;
        void unsubscribe_from_events() const;

        std::string m_viewModelName {};
        std::unique_ptr<Model::InjectorModel> m_model {};
        std::function<void(Event::EventId, const Event::VertexEvent&)> m_eventCallback {};

        Event::EventBus& m_eventBus;
        Log::ILog& m_logService;

        std::vector<InjectionMethod> m_injectionMethods {};
        std::vector<std::string> m_libraryExtensions {};
        int m_selectedMethodIndex {-1};
    };
}
