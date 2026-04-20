//
// Copyright (C) 2026 PHTNC<>.
// Licensed under GPLv3.0 with Plugin Interface exceptions.
//
#pragma once

#include <vertex/view/memoryattributeview.hh>
#include <vertex/view/processlistview.hh>
#include <vertex/view/mainview.hh>
#include <vertex/view/analyticsview.hh>
#include <vertex/view/accesstrackerview.hh>
#include <vertex/view/settingsview.hh>
#include <vertex/view/debuggerview.hh>
#include <vertex/view/injectorview.hh>
#include <vertex/view/pluginconfigview.hh>
#include <vertex/view/scriptingview.hh>
#include <vertex/viewmodel/mainviewmodel.hh>
#include <vertex/configuration/isettings.hh>
#include <vertex/configuration/ipluginconfig.hh>
#include <vertex/debugger/idebuggerruntimeservice.hh>
#include <vertex/event/eventbus.hh>
#include <vertex/gui/iconmanager/iiconmanager.hh>
#include <vertex/language/ilanguage.hh>
#include <vertex/log/ilog.hh>
#include <vertex/runtime/iloader.hh>
#include <vertex/scanner/iscannerruntimeservice.hh>
#include <vertex/scanner/memoryscanner/imemoryscanner.hh>
#include <vertex/scripting/iangelscript.hh>
#include <vertex/thread/ithreaddispatcher.hh>
#include <string>
#include <string_view>

namespace Vertex
{
    class ViewFactory final
    {
    public:
        explicit ViewFactory(
            Event::EventBus& eventBus,
            Runtime::ILoader& loaderService,
            Log::ILog& loggerService,
            Language::ILanguage& languageService,
            Gui::IIconManager& iconService,
            Configuration::ISettings& settingsService,
            Configuration::IPluginConfig& pluginConfigService,
            Scanner::IMemoryScanner& memoryService,
            Scanner::IScannerRuntimeService& scannerService,
            Scripting::IAngelScript& scriptingService,
            Thread::IThreadDispatcher& dispatcher,
            Debugger::IDebuggerRuntimeService& runtimeService
        )
            : m_eventBus{eventBus}
            , m_loaderService{loaderService}
            , m_loggerService{loggerService}
            , m_languageService{languageService}
            , m_iconService{iconService}
            , m_settingsService{settingsService}
            , m_pluginConfigService{pluginConfigService}
            , m_memoryService{memoryService}
            , m_scannerService{scannerService}
            , m_scriptingService{scriptingService}
            , m_dispatcher{dispatcher}
            , m_runtimeService{runtimeService}
        {}

        [[nodiscard]] View::MainView* create_mainview(std::string_view name = ViewModelName::MAIN) const;
        [[nodiscard]] View::ProcessListView* create_processlistview(std::string_view name = ViewModelName::PROCESSLIST) const;
        [[nodiscard]] View::SettingsView* create_settingsview(std::string_view name = ViewModelName::SETTINGS) const;
        [[nodiscard]] View::MemoryAttributeView* create_memoryattributeview(std::string_view name = ViewModelName::MEMORYATTRIBUTES) const;
        [[nodiscard]] View::AnalyticsView* create_analyticsview(std::string_view name = ViewModelName::ANALYTICS) const;
        [[nodiscard]] View::AccessTrackerView* create_accesstrackerview(std::string_view name = ViewModelName::ACCESS_TRACKER) const;
        [[nodiscard]] View::DebuggerView* create_debuggerview(std::string_view name = ViewModelName::DEBUGGER) const;
        [[nodiscard]] View::InjectorView* create_injectorview(std::string_view name = ViewModelName::INJECTOR) const;
        [[nodiscard]] View::PluginConfigView* create_pluginconfigview(wxWindow* parent) const;
        [[nodiscard]] View::ScriptingView* create_scriptingview(std::string_view name = ViewModelName::SCRIPTING) const;

    private:
        Event::EventBus& m_eventBus;
        Runtime::ILoader& m_loaderService;
        Log::ILog& m_loggerService;
        Language::ILanguage& m_languageService;
        Gui::IIconManager& m_iconService;
        Configuration::ISettings& m_settingsService;
        Configuration::IPluginConfig& m_pluginConfigService;
        Scanner::IMemoryScanner& m_memoryService;
        Scanner::IScannerRuntimeService& m_scannerService;
        Scripting::IAngelScript& m_scriptingService;
        Thread::IThreadDispatcher& m_dispatcher;
        Debugger::IDebuggerRuntimeService& m_runtimeService;
    };
}
