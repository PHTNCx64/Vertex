//
// Copyright (C) 2026 PHTNC<>.
// Licensed under GPLv3.0 with Plugin Interface exceptions.
//
#include <vertex/factory.hh>

namespace Vertex
{
    View::MainView* ViewFactory::create_mainview(const std::string_view name) const
    {
        auto model = std::make_unique<Model::MainModel>(m_settingsService, m_memoryService, m_loaderService, m_loggerService, m_dispatcher);
        auto viewModel = std::make_unique<ViewModel::MainViewModel>(std::move(model), m_eventBus, m_dispatcher);
        return new View::MainView(wxString{std::string{name}}, std::move(viewModel), m_languageService, m_iconService);
    }

    View::ProcessListView* ViewFactory::create_processlistview(const std::string_view name) const
    {
        auto model = std::make_unique<Model::ProcessListModel>(m_loaderService, m_loggerService, m_settingsService);
        const auto viewModel = std::make_shared<ViewModel::ProcessListViewModel>(std::move(model), m_eventBus, m_dispatcher, std::string{name});
        return new View::ProcessListView(m_languageService, viewModel);
    }

    View::SettingsView* ViewFactory::create_settingsview(const std::string_view name) const
    {
        auto model = std::make_unique<Model::SettingsModel>(m_loaderService, m_loggerService, m_languageService, m_settingsService);
        auto viewModel = std::make_unique<ViewModel::SettingsViewModel>(std::move(model), m_eventBus, m_loggerService, std::string{name});

        View::PluginConfigViewFactory configFactory = [this](wxWindow* parent) -> View::PluginConfigView*
        {
            return create_pluginconfigview(parent);
        };

        return new View::SettingsView(m_languageService, std::move(viewModel), std::move(configFactory));
    }

    View::MemoryAttributeView* ViewFactory::create_memoryattributeview(const std::string_view name) const
    {
        auto model = std::make_unique<Model::MemoryAttributeModel>(m_loaderService, m_pluginConfigService);
        auto viewModel = std::make_unique<ViewModel::MemoryAttributeViewModel>(std::move(model), m_eventBus, std::string{name});
        return new View::MemoryAttributeView(std::move(viewModel), m_languageService);
    }

    View::AnalyticsView* ViewFactory::create_analyticsview(const std::string_view name) const
    {
        auto model = std::make_unique<Model::AnalyticsModel>(m_loggerService);
        auto viewModel = std::make_unique<ViewModel::AnalyticsViewModel>(std::move(model), m_eventBus, std::string{name});
        return new View::AnalyticsView(m_languageService, std::move(viewModel));
    }

    View::DebuggerView* ViewFactory::create_debuggerview(const std::string_view name) const
    {
        auto model = std::make_unique<Model::DebuggerModel>(m_settingsService, m_loaderService, m_loggerService, m_dispatcher);
        auto viewModel = std::make_unique<ViewModel::DebuggerViewModel>(std::move(model), m_eventBus, m_loggerService, std::string{name});
        return new View::DebuggerView(wxString{std::string{name}}, std::move(viewModel), m_languageService, m_iconService);
    }

    View::MemoryAttributeView* ViewFactory::create_pointerscan_memoryattributeview(const std::string_view name) const
    {
        auto model = std::make_unique<Model::MemoryAttributeModel>(m_loaderService, m_pluginConfigService, "pointerScanMemoryAttributes", false);
        auto viewModel = std::make_unique<ViewModel::MemoryAttributeViewModel>(std::move(model), m_eventBus, std::string{name}, false);
        return new View::MemoryAttributeView(std::move(viewModel), m_languageService);
    }

    View::InjectorView* ViewFactory::create_injectorview(const std::string_view name) const
    {
        auto model = std::make_unique<Model::InjectorModel>(m_loaderService, m_loggerService);
        auto viewModel = std::make_unique<ViewModel::InjectorViewModel>(std::move(model), m_eventBus, m_loggerService, std::string{name});
        return new View::InjectorView(m_languageService, std::move(viewModel));
    }

    View::PluginConfigView* ViewFactory::create_pluginconfigview(wxWindow* parent) const
    {
        auto model = std::make_unique<Model::PluginConfigModel>(m_loaderService.get_ui_registry(), m_pluginConfigService, m_loggerService);
        auto viewModel = std::make_unique<ViewModel::PluginConfigViewModel>(std::move(model), m_eventBus, m_loggerService);
        return new View::PluginConfigView(parent, m_languageService, std::move(viewModel));
    }
}
