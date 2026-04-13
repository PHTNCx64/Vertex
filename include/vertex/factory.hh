//
// Copyright (C) 2026 PHTNC<>.
// Licensed under GPLv3.0 with Plugin Interface exceptions.
//
#pragma once

#include <vertex/view/memoryattributeview.hh>
#include <vertex/view/pointerscanconfigdialog.hh>
#include <vertex/view/processlistview.hh>
#include <vertex/view/mainview.hh>
#include <vertex/view/analyticsview.hh>
#include <vertex/view/settingsview.hh>
#include <vertex/view/debuggerview.hh>
#include <vertex/view/injectorview.hh>
#include <vertex/view/pluginconfigview.hh>
#include <vertex/view/pointerscanview.hh>
#include <vertex/view/scriptingview.hh>
#include <vertex/viewmodel/mainviewmodel.hh>
#include <vertex/configuration/isettings.hh>
#include <vertex/configuration/ipluginconfig.hh>
#include <vertex/event/eventbus.hh>
#include <vertex/gui/iconmanager/iiconmanager.hh>
#include <vertex/gui/theme/ithemeprovider.hh>
#include <vertex/language/ilanguage.hh>
#include <vertex/log/ilog.hh>
#include <vertex/runtime/iloader.hh>
#include <vertex/scanner/memoryscanner/imemoryscanner.hh>
#include <vertex/scanner/pointerscanner/ipointerscanner.hh>
#include <vertex/scripting/iangelscript.hh>
#include <vertex/thread/ithreaddispatcher.hh>
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
            Gui::IThemeProvider& themeService,
            Configuration::ISettings& settingsService,
            Configuration::IPluginConfig& pluginConfigService,
            Scanner::IMemoryScanner& memoryService,
            Scanner::IPointerScanner& pointerScanService,
            Scripting::IAngelScript& scriptingService,
            Thread::IThreadDispatcher& dispatcher
        )
            : m_eventBus{eventBus}
            , m_loaderService{loaderService}
            , m_loggerService{loggerService}
            , m_languageService{languageService}
            , m_iconService{iconService}
            , m_themeService{themeService}
            , m_settingsService{settingsService}
            , m_pluginConfigService{pluginConfigService}
            , m_memoryService{memoryService}
            , m_pointerScanService{pointerScanService}
            , m_scriptingService{scriptingService}
            , m_dispatcher{dispatcher}
        {}

        [[nodiscard]] View::MainView* create_mainview(std::string_view name = ViewModelName::MAIN) const;
        [[nodiscard]] View::ProcessListView* create_processlistview(std::string_view name = ViewModelName::PROCESSLIST) const;
        [[nodiscard]] View::SettingsView* create_settingsview(std::string_view name = ViewModelName::SETTINGS) const;
        [[nodiscard]] View::MemoryAttributeView* create_memoryattributeview(std::string_view name = ViewModelName::MEMORYATTRIBUTES) const;
        [[nodiscard]] View::AnalyticsView* create_analyticsview(std::string_view name = ViewModelName::ANALYTICS) const;
        [[nodiscard]] View::DebuggerView* create_debuggerview(std::string_view name = ViewModelName::DEBUGGER) const;
        [[nodiscard]] View::MemoryAttributeView* create_pointerscan_memoryattributeview(std::string_view name = ViewModelName::POINTERSCAN_MEMORYATTRIBUTES) const;
        [[nodiscard]] View::InjectorView* create_injectorview(std::string_view name = ViewModelName::INJECTOR) const;
        [[nodiscard]] View::PluginConfigView* create_pluginconfigview(wxWindow* parent) const;
        [[nodiscard]] View::PointerScanConfigDialog* create_pointerscanconfigdialog(wxWindow* parent, std::uint64_t targetAddress) const;
        [[nodiscard]] View::PointerScanView* create_pointerscanview(std::string_view name = ViewModelName::POINTERSCAN) const;
        [[nodiscard]] View::ScriptingView* create_scriptingview(std::string_view name = ViewModelName::SCRIPTING) const;

    private:
        Event::EventBus& m_eventBus;
        Runtime::ILoader& m_loaderService;
        Log::ILog& m_loggerService;
        Language::ILanguage& m_languageService;
        Gui::IIconManager& m_iconService;
        Gui::IThemeProvider& m_themeService;
        Configuration::ISettings& m_settingsService;
        Configuration::IPluginConfig& m_pluginConfigService;
        Scanner::IMemoryScanner& m_memoryService;
        Scanner::IPointerScanner& m_pointerScanService;
        Scripting::IAngelScript& m_scriptingService;
        Thread::IThreadDispatcher& m_dispatcher;
    };
}