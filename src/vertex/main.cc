//
// Copyright (C) 2026 PHTNC<>.
// Licensed under GPLv3.0 with Plugin Interface exceptions.
//
#include <vertex/factory.hh>
#include <vertex/view/processlistview.hh>

#include <vertex/configuration/filesystem.hh>
#include <vertex/runtime/loader.hh>
#include <vertex/vertex.hh>

#include <fmt/format.h>

wxIMPLEMENT_APP(VertexApp);

bool VertexApp::OnInit()
{
#if defined (__WXGTK__)
    GTKSuppressDiagnostics();
#endif

    m_injector = std::make_unique<decltype(Vertex::DI::create_injector())>(Vertex::DI::create_injector());
    auto& log = m_injector->create<Vertex::Log::ILog&>();

    log.log_info("Vertex starting");

    if (initialize_filesystem() != StatusCode::STATUS_OK)
    {
        return false;
    }

    auto& loader = m_injector->create<Vertex::Runtime::ILoader&>();
    auto& language = m_injector->create<Vertex::Language::ILanguage&>();
    auto& iconManager = m_injector->create<Vertex::Gui::IIconManager&>();
    auto& settings = m_injector->create<Vertex::Configuration::ISettings&>();
    auto& pluginConfig = m_injector->create<Vertex::Configuration::IPluginConfig&>();
    auto& scanner = m_injector->create<Vertex::Scanner::IMemoryScanner&>();
    auto& eventBus = m_injector->create<Vertex::Event::EventBus&>();

    apply_language_settings();
    apply_plugin_settings();
    apply_appearance_settings();

    const Vertex::ViewFactory factory { eventBus, loader, log, language, iconManager, settings, pluginConfig, scanner };
    auto* mainView = factory.create_mainview(ApplicationName " " ApplicationVersion " by " ApplicationVendor);

    std::ignore = factory.create_processlistview();
    std::ignore = factory.create_settingsview();
    std::ignore = factory.create_memoryattributeview();
    std::ignore = factory.create_pointerscan_memoryattributeview();
    std::ignore = factory.create_analyticsview();
    std::ignore = factory.create_injectorview();

    auto* debuggerView = factory.create_debuggerview();

    mainView->set_view_in_disassembly_callback([debuggerView](const std::uint64_t address)
    {
        debuggerView->navigate_to_address(address);
    });

    mainView->set_find_access_callback([debuggerView](const std::uint64_t address, const std::uint32_t size)
    {
        debuggerView->set_watchpoint(address, size);
    });

    log.log_info("Vertex initialized");

    return mainView->Show();
}

int VertexApp::OnExit()
{
    if (m_injector)
    {
        auto& settings = m_injector->create<Vertex::Configuration::ISettings&>();
        auto& log = m_injector->create<Vertex::Log::ILog&>();

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
    auto& log = m_injector->create<Vertex::Log::ILog&>();
    const auto& settings = m_injector->create<Vertex::Configuration::ISettings&>();
    auto& language = m_injector->create<Vertex::Language::ILanguage&>();

    auto languagePath = settings.get_path("language.languagePath");
    if (languagePath.empty())
    {
        languagePath = Vertex::Configuration::Filesystem::get_language_path();
    }

    auto activeLanguage = settings.get_string("language.activeLanguage");

    if (activeLanguage.empty())
    {
        const auto availableLanguages = language.fetch_all_languages();

        if (availableLanguages.contains("English_US"))
        {
            activeLanguage = "English_US.json";
        }
        else if (!availableLanguages.empty())
        {
            activeLanguage = availableLanguages.begin()->first + ".json";
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
    auto& log = m_injector->create<Vertex::Log::ILog&>();
    const auto& settings = m_injector->create<Vertex::Configuration::ISettings&>();
    auto& loader = m_injector->create<Vertex::Runtime::ILoader&>();

    const auto activePluginPath = settings.get_path("plugins.activePluginPath");
    if (activePluginPath.empty())
    {
        return;
    }

    const StatusCode status = loader.set_active_plugin(activePluginPath);
    if (status != StatusCode::STATUS_OK)
    {
        log.log_warn("Failed to load active plugin");
    }
}

void VertexApp::apply_appearance_settings()
{
    auto& log = m_injector->create<Vertex::Log::ILog&>();
    const auto& settings = m_injector->create<Vertex::Configuration::ISettings&>();

    const auto theme = settings.get_int("general.theme", 0);
    SetAppearance(static_cast<Appearance>(theme));

    const bool loggingEnabled = settings.get_bool("general.enableLogging", true);
    if (!loggingEnabled)
    {
        log.log_warn("Logging disabled per settings");
    }

    log.set_logging_status(loggingEnabled);
}
