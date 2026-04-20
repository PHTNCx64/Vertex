//
// Copyright (C) 2026 PHTNC<>.
// Licensed under GPLv3.0 with Plugin Interface exceptions.
//
#include <vertex/factory.hh>
#include <vertex/view/processlistview.hh>

#include <vertex/configuration/filesystem.hh>
#include <vertex/runtime/loader.hh>
#include <vertex/vertex.hh>
#include <vertex/di.hh>

#include <fmt/format.h>

#include <unordered_set>

#include <wx/mstream.h>
#include <logo.hh>

struct VertexApp::Impl final
{
    Vertex::DI::Injector injector{Vertex::DI::create_injector()};
};

wxIMPLEMENT_APP(VertexApp);

VertexApp::VertexApp() = default;
VertexApp::~VertexApp() = default;

bool VertexApp::OnInit()
{
#if defined (__WXGTK__)
    GTKSuppressDiagnostics();
#endif

    wxInitAllImageHandlers();

    m_impl = std::make_unique<Impl>();
    auto& log = m_impl->injector.create<Vertex::Log::ILog&>();

    log.log_info("Vertex starting");

    if (initialize_filesystem() != StatusCode::STATUS_OK)
    {
        return false;
    }

    auto& loader = m_impl->injector.create<Vertex::Runtime::ILoader&>();
    auto& language = m_impl->injector.create<Vertex::Language::ILanguage&>();
    auto& iconManager = m_impl->injector.create<Vertex::Gui::IIconManager&>();
    auto& settings = m_impl->injector.create<Vertex::Configuration::ISettings&>();
    auto& pluginConfig = m_impl->injector.create<Vertex::Configuration::IPluginConfig&>();
    auto& scanner = m_impl->injector.create<Vertex::Scanner::IMemoryScanner&>();
    auto& scannerService = m_impl->injector.create<Vertex::Scanner::IScannerRuntimeService&>();
    auto& eventBus = m_impl->injector.create<Vertex::Event::EventBus&>();
    auto& dispatcher = m_impl->injector.create<Vertex::Thread::IThreadDispatcher&>();
    auto& scripting = m_impl->injector.create<Vertex::Scripting::IAngelScript&>();
    auto& debuggerRuntime = m_impl->injector.create<Vertex::Debugger::IDebuggerRuntimeService&>();

    apply_language_settings();
    apply_plugin_settings();
    apply_appearance_settings();

    const Vertex::ViewFactory factory { eventBus, loader, log, language, iconManager, settings, pluginConfig, scanner, scannerService, scripting, dispatcher, debuggerRuntime };
    auto* mainView = factory.create_mainview(ApplicationName " " ApplicationVersion " by " ApplicationVendor);

    std::ignore = factory.create_processlistview();
    std::ignore = factory.create_settingsview();
    std::ignore = factory.create_memoryattributeview();
    std::ignore = factory.create_analyticsview();
    std::ignore = factory.create_injectorview();
    std::ignore = factory.create_scriptingview();

    auto* debuggerView = factory.create_debuggerview();
    auto* accessTrackerView = factory.create_accesstrackerview();

    mainView->set_view_in_disassembly_callback([debuggerView](const std::uint64_t address)
    {
        debuggerView->navigate_to_address(address);
    });

    mainView->set_debugger_callbacks(
        [debuggerView]() { return debuggerView->is_attached(); },
        [debuggerView]() { debuggerView->attach_debugger(); }
    );

    mainView->set_find_access_callback([mainView, accessTrackerView](const std::uint64_t address, const std::uint32_t size)
    {
        if (mainView->ensure_debugger_attached())
        {
            accessTrackerView->open_and_track(address, size);
        }
    });

    std::ignore = scripting.start();
    apply_script_settings();

    log.log_info("Vertex initialized");

    wxMemoryInputStream logoStream{Vertex::Gui::logo_ico, Vertex::Gui::logo_ico_size};
    const wxIconBundle iconBundle{logoStream, wxBITMAP_TYPE_ICO};
    if (!iconBundle.IsEmpty())
    {
        mainView->SetIcons(iconBundle);
    }

    return mainView->Show();
}

int VertexApp::OnExit()
{
    if (m_impl)
    {
        auto& settings = m_impl->injector.create<Vertex::Configuration::ISettings&>();
        auto& log = m_impl->injector.create<Vertex::Log::ILog&>();
        auto& dispatcher = m_impl->injector.create<Vertex::Thread::IThreadDispatcher&>();
        auto& loader = m_impl->injector.create<Vertex::Runtime::ILoader&>();
        auto& scripting = m_impl->injector.create<Vertex::Scripting::IAngelScript&>();

        std::ignore = scripting.stop();

        const auto stopStatus = dispatcher.stop();
        if (stopStatus != StatusCode::STATUS_OK &&
            stopStatus != StatusCode::STATUS_ERROR_THREAD_IS_NOT_RUNNING)
        {
            log.log_warn(fmt::format("Failed to stop thread dispatcher on exit (status={})", static_cast<int>(stopStatus)));
        }

        const auto& plugins = loader.get_plugins();
        for (std::size_t i{}; i < plugins.size(); ++i)
        {
            if (!plugins[i].is_loaded())
            {
                continue;
            }

            const auto unloadStatus = loader.unload_plugin(i);
            if (unloadStatus != StatusCode::STATUS_OK)
            {
                log.log_warn(fmt::format("Failed to unload plugin {} during exit (status={})",
                    plugins[i].get_filename(), static_cast<int>(unloadStatus)));
            }
        }

        const auto settingsPath = Vertex::Configuration::Filesystem::get_configuration_path() / "Settings.json";
        const StatusCode status = settings.save_to_file(settingsPath);

        if (status == StatusCode::STATUS_OK)
        {
            log.log_info("Settings saved on exit");
        }
        else
        {
            log.log_warn("Failed to save settings on exit");
        }

        m_impl.reset();
    }

    return wxAppBase::OnExit();
}

StatusCode VertexApp::initialize_filesystem() const
{
    const StatusCode status = Vertex::Configuration::Filesystem::construct_runtime_filesystem();
    if (status != StatusCode::STATUS_OK)
    {
        wxMessageBox("Failed to construct filesystem data, Vertex cannot proceed.\nPlease ensure that you have appropriate permissions on the path where Vertex is placed.", "Critical Error", wxOK | wxICON_ERROR);
        return status;
    }
    return status;
}

void VertexApp::apply_language_settings() const
{
    auto& log = m_impl->injector.create<Vertex::Log::ILog&>();
    const auto& settings = m_impl->injector.create<Vertex::Configuration::ISettings&>();
    auto& language = m_impl->injector.create<Vertex::Language::ILanguage&>();

    auto languagePath = Vertex::Configuration::Filesystem::resolve_path(settings.get_path("language.languagePath"));
    if (languagePath.empty())
    {
        languagePath = Vertex::Configuration::Filesystem::get_language_path();
    }

    auto activeLanguage = settings.get_string("language.activeLanguage");

    if (activeLanguage.empty())
    {
        const auto availableLanguages = language.fetch_all_languages();

        const auto englishIt = std::ranges::find_if(availableLanguages, [](const auto& entry) {
            return entry.second.stem() == "English_US";
        });
        if (englishIt != availableLanguages.end())
        {
            activeLanguage = englishIt->second.filename().string();
        }
        else if (!availableLanguages.empty())
        {
            activeLanguage = availableLanguages.begin()->second.filename().string();
        }
    }

    if (!activeLanguage.empty())
    {
        const auto translationPath = languagePath / activeLanguage;
        language.load_translation(translationPath);
        log.log_info(fmt::format("Language loaded: {}", activeLanguage));
    }
    else
    {
        log.log_warn("No languages available");
    }
}

void VertexApp::apply_plugin_settings() const
{
    auto& log = m_impl->injector.create<Vertex::Log::ILog&>();
    const auto& settings = m_impl->injector.create<Vertex::Configuration::ISettings&>();
    auto& loader = m_impl->injector.create<Vertex::Runtime::ILoader&>();

    const auto activePluginPath = Vertex::Configuration::Filesystem::resolve_path(settings.get_path("plugins.activePluginPath"));
    if (!activePluginPath.empty())
    {
        const StatusCode status = loader.set_active_plugin(activePluginPath);
        if (status == StatusCode::STATUS_OK)
        {
            return;
        }

        log.log_warn(fmt::format("Failed to load configured active plugin '{}', status={}. Trying fallback plugin.",
            activePluginPath.string(),
            static_cast<int>(status)));
    }

    const auto& discoveredPlugins = loader.get_plugins();
    if (discoveredPlugins.empty())
    {
        log.log_warn("No plugin configured and no discovered plugins are available.");
        return;
    }

    const StatusCode fallbackStatus = loader.set_active_plugin(static_cast<std::size_t>(0));
    if (fallbackStatus != StatusCode::STATUS_OK)
    {
        log.log_warn(fmt::format("Failed to load fallback plugin '{}', status={}",
            discoveredPlugins.front().get_filename(),
            static_cast<int>(fallbackStatus)));
        return;
    }

    log.log_info(fmt::format("Loaded fallback active plugin: {}",
        discoveredPlugins.front().get_filename()));
}

void VertexApp::apply_appearance_settings()
{
    auto& log = m_impl->injector.create<Vertex::Log::ILog&>();
    const auto& settings = m_impl->injector.create<Vertex::Configuration::ISettings&>();

    const auto theme = settings.get_int("general.theme", Vertex::ApplicationAppearance::SYSTEM);
    SetAppearance(static_cast<Appearance>(theme));

    const bool loggingEnabled = settings.get_bool("general.enableLogging", true);
    if (!loggingEnabled)
    {
        log.log_warn("Logging disabled per settings.");
    }

    log.set_logging_status(loggingEnabled);
}

void VertexApp::apply_script_settings() const
{
    auto& log = m_impl->injector.create<Vertex::Log::ILog&>();
    const auto& settings = m_impl->injector.create<Vertex::Configuration::ISettings&>();
    auto& scripting = m_impl->injector.create<Vertex::Scripting::IAngelScript&>();

    const auto scriptPaths = settings.get_value("scripting.scriptPaths");
    if (!scriptPaths.is_array() || scriptPaths.empty())
    {
        return;
    }

    const auto autoStartJson = settings.get_value("scripting.autoStartScripts");
    if (!autoStartJson.is_array() || autoStartJson.empty())
    {
        return;
    }

    std::unordered_set<std::string> autoStartSet{};
    for (const auto& entry : autoStartJson)
    {
        if (entry.is_string())
        {
            autoStartSet.insert(entry.get<std::string>());
        }
    }

    if (autoStartSet.empty())
    {
        return;
    }

    for (const auto& pathEntry : scriptPaths)
    {
        if (!pathEntry.is_string())
        {
            continue;
        }

        const auto scriptDir = Vertex::Configuration::Filesystem::resolve_path(std::filesystem::path{pathEntry.get<std::string>()});
        if (!std::filesystem::exists(scriptDir) || !std::filesystem::is_directory(scriptDir))
        {
            log.log_warn(fmt::format("Script path does not exist: {}", scriptDir.string()));
            continue;
        }

        for (const auto& entry : std::filesystem::directory_iterator{scriptDir})
        {
            if (!entry.is_regular_file() || entry.path().extension() != Vertex::FileTypes::SCRIPTING_EXTENSION)
            {
                continue;
            }

            const auto relativePath = Vertex::Configuration::Filesystem::make_relative(entry.path()).string();
            if (!autoStartSet.contains(relativePath))
            {
                continue;
            }

            const auto moduleName = entry.path().stem().string();
            auto result = scripting.create_context(moduleName, entry.path(), Vertex::Scripting::UseActive{});

            if (result.has_value())
            {
                log.log_info(fmt::format("Script auto-started: {}", entry.path().filename().string()));
            }
            else
            {
                log.log_error(fmt::format("Failed to auto-start script: {}", entry.path().filename().string()));
            }
        }
    }
}
