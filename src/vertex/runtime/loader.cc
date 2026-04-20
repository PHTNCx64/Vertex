//
// Copyright (C) 2026 PHTNC<>.
// Licensed under GPLv3.0 with Plugin Interface exceptions.
//
#include <sdk/log.h>
#include <sdk/registry.h>
#include <sdk/ui.h>
#include <vertex/runtime/loader.hh>
#include <vertex/runtime/caller.hh>
#include <vertex/runtime/function_registry.hh>
#include <vertex/scanner/scanner_interop.hh>
#include <vertex/utility.hh>
#include <vertex/configuration/filesystem.hh>
#include <plugin_function_registration.hh>

#include <unordered_set>
#include <span>
#include <exception>
#include <fmt/format.h>
#include <algorithm>

namespace Vertex::Runtime
{

    Loader::Loader(Configuration::ISettings& settingsService, Configuration::IPluginConfig& pluginConfigService, Log::ILog& loggerService, Thread::IThreadDispatcher& threadDispatcher, Scanner::IScannerRuntimeService& scannerService)
        : m_settingsService(settingsService), m_pluginConfigService(pluginConfigService), m_loggerService(loggerService), m_threadDispatcher(threadDispatcher), m_scannerService(scannerService)
    {
        Scanner::interop::set_scanner_service(&m_scannerService);
        m_loggerService.log_info("Initializing plugin loader...");

        auto pluginPaths = m_settingsService.get_settings()["plugins"]["pluginPaths"];
        std::vector<std::filesystem::path> paths;

        if (pluginPaths.is_array())
        {
            m_loggerService.log_info(fmt::format("Found {} plugin paths in configuration", pluginPaths.size()));

            for (const auto& pathJson : pluginPaths)
            {
                if (pathJson.is_string())
                {
                    const auto path = pathJson.get<std::string>();
                    m_loggerService.log_info(fmt::format("Registering plugin path: {}", path));
                    paths.emplace_back(path);
                }
                else
                {
                    m_loggerService.log_warn("Skipping non-string plugin path entry");
                }
            }
        }
        else
        {
            m_loggerService.log_warn("Plugin paths configuration is not an array");
        }

        std::vector<Plugin> temporaryPlugins{};
        const StatusCode discoveryResult = Loader::get_plugins_from_fs(paths, temporaryPlugins);

        if (discoveryResult != StatusCode::STATUS_OK)
        {
            m_loggerService.log_error(fmt::format("Plugin discovery failed with code: {}", static_cast<int>(discoveryResult)));
        }
        else
        {
            m_loggerService.log_info(fmt::format("Discovered {} plugins", temporaryPlugins.size()));
        }

        m_plugins = std::move(temporaryPlugins);
    }

    Loader::~Loader()
    {
        m_activePlugin.reset();
        m_activePluginInitialized = false;
        m_plugins.clear();
    }

    const std::vector<Plugin>& Loader::get_plugins() noexcept
    {
        return m_plugins;
    }

    StatusCode Loader::load_plugins(std::filesystem::path& path)
    {
        m_loggerService.log_info(fmt::format("Loading plugins from directory: {}", path.string()));

        if (!std::filesystem::exists(path))
        {
            m_loggerService.log_error(fmt::format("Plugin path does not exist: {}", path.string()));
            return StatusCode::STATUS_ERROR_PLUGIN_NOT_FOUND;
        }

        if (!std::filesystem::is_directory(path))
        {
            m_loggerService.log_error(fmt::format("Plugin path is not a directory: {}", path.string()));
            return StatusCode::STATUS_ERROR_INVALID_PARAMETER;
        }

        if (path.is_relative())
        {
            path = Configuration::Filesystem::resolve_path(path);
            m_loggerService.log_info(fmt::format("Converted relative path to absolute: {}", path.string()));
        }

        StatusCode overallStatus = StatusCode::STATUS_OK;
        std::size_t successfulLoads{};
        std::size_t failedLoads{};

        try
        {
            for (const auto& entry : std::filesystem::directory_iterator(path))
            {
                if (!entry.is_regular_file())
                {
                    continue;
                }

                if (entry.path().extension() == FileTypes::PLUGIN_EXTENSION)
                {
                    m_loggerService.log_info(fmt::format("Attempting to load plugin: {}", entry.path().string()));
                    const StatusCode loadResult = load_plugin(entry.path());

                    if (loadResult == StatusCode::STATUS_OK)
                    {
                        m_loggerService.log_info(fmt::format("Plugin '{}' loaded successfully", entry.path().filename().string()));
                        successfulLoads++;
                    }
                    else
                    {
                        m_loggerService.log_error(fmt::format("Failed to load plugin '{}', error code: {} ({})",
                            entry.path().filename().string(),
                            static_cast<int>(loadResult),
                            status_code_to_string(loadResult)));
                        failedLoads++;
                        if (overallStatus == StatusCode::STATUS_OK)
                        {
                            overallStatus = loadResult;
                        }
                    }
                }
            }
        }
        catch (const std::filesystem::filesystem_error& ex)
        {
            m_loggerService.log_error(fmt::format("Filesystem error while loading plugins: {}", ex.what()));
            return StatusCode::STATUS_ERROR_PLUGIN_LOAD_FAILED;
        }

        m_loggerService.log_info(fmt::format("Plugin loading complete: {} successful, {} failed", successfulLoads, failedLoads));

        if (successfulLoads == 0 && failedLoads > 0)
        {
            m_loggerService.log_error("All plugin load attempts failed");
            return overallStatus;
        }
        if (failedLoads > 0)
        {
            m_loggerService.log_warn(fmt::format("Some plugins failed to load ({}/{})", failedLoads, successfulLoads + failedLoads));
            return StatusCode::STATUS_OK;
        }

        return StatusCode::STATUS_OK;
    }

    StatusCode Loader::load_plugin(std::filesystem::path path)
    {
        m_loggerService.log_info(fmt::format("[Plugin Load] Starting load process for: {}", path.string()));

        if (!path.has_filename() || path.extension() != FileTypes::PLUGIN_EXTENSION)
        {
            m_loggerService.log_error(fmt::format("[Plugin Load] Invalid plugin file extension: {}", path.string()));
            return StatusCode::STATUS_ERROR_PLUGIN_NOT_FOUND;
        }

        if (path.is_relative())
        {
            path = Configuration::Filesystem::resolve_path(path);
            m_loggerService.log_info(fmt::format("[Plugin Load] Converted to absolute path: {}", path.string()));
        }

        std::error_code ec;
        const auto canonicalPath = std::filesystem::canonical(path, ec);
        if (ec)
        {
            m_loggerService.log_error(fmt::format("[Plugin Load] Failed to get canonical path: {} ({})", path.string(), ec.message()));
            return StatusCode::STATUS_ERROR_PLUGIN_NOT_FOUND;
        }

        const auto existingPlugin = std::ranges::find_if(m_plugins,
                                                          [&canonicalPath](const Plugin& plugin)
                                                          {
                                                              return plugin.get_path() == canonicalPath;
                                                          });

        if (existingPlugin != m_plugins.end() && existingPlugin->is_loaded())
        {
            m_loggerService.log_warn(fmt::format("[Plugin Load] Plugin already loaded: {}", canonicalPath.string()));
            return StatusCode::STATUS_ERROR_PLUGIN_ALREADY_LOADED;
        }

        const bool wasNewlyAdded = (existingPlugin == m_plugins.end());
        const std::size_t pluginIndex = wasNewlyAdded
            ? (m_plugins.emplace_back(m_loggerService), m_plugins.size() - 1)
            : static_cast<std::size_t>(std::distance(m_plugins.begin(), existingPlugin));

        Plugin& plugin = m_plugins[pluginIndex];
        plugin.set_path(canonicalPath);

        auto removePluginOnFailure = [this, pluginIndex, wasNewlyAdded](const StatusCode status) -> StatusCode
        {
            if (wasNewlyAdded)
            {
                m_loggerService.log_info("[Plugin Load] Removing failed plugin entry...");
                m_plugins.erase(m_plugins.begin() + static_cast<std::ptrdiff_t>(pluginIndex));
            }
            else
            {
                m_loggerService.log_info("[Plugin Load] Leaving discovered plugin entry (unloaded) after failure");
                m_plugins[pluginIndex].release_library();
            }
            return status;
        };

        m_loggerService.log_info(fmt::format("[Plugin Load] Loading library: {}", canonicalPath.string()));
        try
        {
            const Library library{canonicalPath};
            m_loggerService.log_info(fmt::format("[Plugin Load] Library loaded successfully, handle: 0x{:X}",
                reinterpret_cast<uintptr_t>(library.handle())));

            plugin.set_library(library);
        }
        catch (const LibraryError& e)
        {
            m_loggerService.log_error(fmt::format("[Plugin Load] Library load failed: {}", e.what()));
            return removePluginOnFailure(StatusCode::STATUS_ERROR_PLUGIN_LOAD_FAILED);
        }

        vertex_log_set_instance(&m_loggerService);
        plugin.m_runtime.vertex_log_info = vertex_log_info;
        plugin.m_runtime.vertex_log_error = vertex_log_error;
        plugin.m_runtime.vertex_log_warn = vertex_log_warn;

        plugin.m_runtime.vertex_register_datatype = vertex_register_datatype;
        plugin.m_runtime.vertex_unregister_datatype = vertex_unregister_datatype;

        vertex_registry_set_instance(&m_registry);
        plugin.m_runtime.vertex_register_architecture = vertex_register_architecture;
        plugin.m_runtime.vertex_register_category = vertex_register_category;
        plugin.m_runtime.vertex_unregister_category = vertex_unregister_category;
        plugin.m_runtime.vertex_register_register = vertex_register_register;
        plugin.m_runtime.vertex_unregister_register = vertex_unregister_register;
        plugin.m_runtime.vertex_register_flag_bit = vertex_register_flag_bit;
        plugin.m_runtime.vertex_register_exception_type = vertex_register_exception_type;
        plugin.m_runtime.vertex_register_calling_convention = vertex_register_calling_convention;
        plugin.m_runtime.vertex_register_snapshot = vertex_register_snapshot;
        plugin.m_runtime.vertex_clear_registry = vertex_clear_registry;

        vertex_ui_registry_set_instance(&m_uiRegistry);
        plugin.m_runtime.vertex_register_ui_panel = vertex_register_ui_panel;
        plugin.m_runtime.vertex_get_ui_value = vertex_get_ui_value;
        plugin.m_runtime.vertex_set_ui_value = vertex_set_ui_value;

        m_loggerService.log_info("[Plugin Load] Resolving plugin functions...");
        const StatusCode status = resolve_functions(plugin);

        if (status != StatusCode::STATUS_OK)
        {
            m_loggerService.log_error(fmt::format("[Plugin Load] Function resolution failed with code: {} ({})",
                static_cast<int>(status), status_code_to_string(status)));
            return removePluginOnFailure(status);
        }

        m_loggerService.log_info(fmt::format("[Plugin Load] Plugin loaded successfully: {}", canonicalPath.filename().string()));
        return StatusCode::STATUS_OK;
    }

    StatusCode Loader::unload_plugin(const std::size_t pluginIndex)
    {
        if (pluginIndex >= m_plugins.size())
        {
            m_loggerService.log_error(fmt::format("Invalid plugin index for unload: {}", pluginIndex));
            return StatusCode::STATUS_ERROR_INVALID_PARAMETER;
        }

        auto& plugin = m_plugins[pluginIndex];

        if (!plugin.is_loaded())
        {
            m_loggerService.log_warn(fmt::format("Plugin at index {} is not loaded", pluginIndex));
            return StatusCode::STATUS_ERROR_PLUGIN_NOT_LOADED;
        }

        const bool isActivePlugin = m_activePlugin.has_value() &&
            &m_activePlugin.value().get() == &plugin;

        if (isActivePlugin)
        {
            const auto stopStatus = m_threadDispatcher.stop();
            if (stopStatus != StatusCode::STATUS_OK &&
                stopStatus != StatusCode::STATUS_ERROR_THREAD_IS_NOT_RUNNING)
            {
                m_loggerService.log_warn(fmt::format("Failed to stop thread dispatcher before unloading active plugin (status={})",
                    static_cast<int>(stopStatus)));
            }

            m_activePlugin.reset();
            m_activePluginInitialized = false;
            m_registry.clear();
            m_uiRegistry.clear();
        }

        m_loggerService.log_info(fmt::format("Unloading plugin: {}", plugin.get_filename()));

        const auto invalidatedCount = m_scannerService.invalidate_plugin_types(pluginIndex);
        if (invalidatedCount > 0)
        {
            m_loggerService.log_info(fmt::format("[Plugin Unload] Invalidated {} scanner datatypes for plugin index {}", invalidatedCount, pluginIndex));
        }

        {
            Scanner::interop::PluginRegistrationGuard guard{pluginIndex,
                                                             plugin.library_keepalive(),
                                                             Scanner::interop::RegistrationMode::Shutdown};
            plugin.release_library();
        }
        return StatusCode::STATUS_OK;
    }

    StatusCode Loader::resolve_functions(Plugin& plugin)
    {
        if (!plugin.is_loaded())
        {
            m_loggerService.log_error("[Function Resolution] Plugin library is not loaded");
            return StatusCode::STATUS_ERROR_PLUGIN_RESOLVE_FAILURE;
        }

        m_loggerService.log_info(fmt::format("[Function Resolution] Module handle: 0x{:X}",
            reinterpret_cast<uintptr_t>(plugin.get_library().handle())));

        try
        {
            FunctionRegistry registry;

            m_loggerService.log_info("[Function Resolution] Starting automated function resolution...");

            register_all_plugin_functions(registry, plugin);

            auto resolveResult = registry.resolve_all(plugin.get_library());
            if (!resolveResult)
            {
                m_loggerService.log_error(fmt::format("[Function Resolution] Failed to resolve functions: {}",
                    resolveResult.error()));
                return StatusCode::STATUS_ERROR_PLUGIN_RESOLVE_FAILURE;
            }

            for (const auto& warning : resolveResult.value())
            {
                m_loggerService.log_warn(fmt::format("[Function Resolution] {}", warning));
            }

            m_loggerService.log_info(fmt::format("[Function Resolution] Successfully resolved functions. "
                "Registry size: {}, Warnings: {}", registry.size(), resolveResult.value().size()));

            return StatusCode::STATUS_OK;
        }
        catch (const std::exception& e)
        {
            m_loggerService.log_error(fmt::format("[Function Resolution] Unexpected error: {}", e.what()));
            return StatusCode::STATUS_ERROR_PLUGIN_RESOLVE_FAILURE;
        }
    }

    StatusCode Loader::has_plugin_loaded()
    {
        if (!m_activePlugin.has_value())
        {
            return StatusCode::STATUS_ERROR_PLUGIN_NOT_ACTIVE;
        }

        auto& plugin = m_activePlugin.value().get();
        if (!plugin.is_loaded())
        {
            m_activePlugin.reset();
            m_activePluginInitialized = false;
            m_registry.clear();
            m_uiRegistry.clear();
            return StatusCode::STATUS_ERROR_PLUGIN_NOT_LOADED;
        }

        return StatusCode::STATUS_OK;
    }

    StatusCode Loader::get_plugins_from_fs(const std::vector<std::filesystem::path>& paths, std::vector<Plugin>& pluginStates)
    {
        pluginStates.clear();

        std::unordered_set<std::filesystem::path> loadedPluginPaths;
        for (const auto& loadedPlugin : m_plugins)
        {
            if (loadedPlugin.is_loaded())
            {
                loadedPluginPaths.insert(loadedPlugin.get_path());
            }
        }

        std::unordered_set<std::filesystem::path> discoveredPluginPaths;

        m_loggerService.log_info(fmt::format("Scanning {} plugin directories...", paths.size()));

        for (const std::filesystem::path& path : paths)
        {
            auto resolvedPath = path;
            if (resolvedPath.is_relative())
            {
                resolvedPath = Configuration::Filesystem::resolve_path(resolvedPath);
            }

            if (!std::filesystem::exists(resolvedPath) || !std::filesystem::is_directory(resolvedPath))
            {
                m_loggerService.log_warn(fmt::format("Plugin path does not exist or is not a directory: {}", resolvedPath.string()));
                continue;
            }

            std::error_code dirEc;
            const auto canonicalDir = std::filesystem::canonical(resolvedPath, dirEc);
            if (dirEc)
            {
                m_loggerService.log_warn(fmt::format("Failed to canonicalize plugin directory '{}': {}", resolvedPath.string(), dirEc.message()));
                continue;
            }

            m_loggerService.log_info(fmt::format("Scanning directory: {}", canonicalDir.string()));
            std::size_t foundCount{};

            for (std::filesystem::directory_iterator dir(canonicalDir); dir != std::filesystem::directory_iterator(); ++dir)
            {
                const auto& entry = *dir;

                if (!entry.is_regular_file())
                {
                    continue;
                }

                const auto& filePath = entry.path();
                const std::string extension = filePath.extension().string();

                if (extension != FileTypes::PLUGIN_EXTENSION)
                {
                    continue;
                }

                std::error_code fileEc;
                const auto canonicalFilePath = std::filesystem::canonical(filePath, fileEc);
                if (fileEc)
                {
                    m_loggerService.log_warn(fmt::format("Failed to canonicalize plugin '{}': {}", filePath.string(), fileEc.message()));
                    continue;
                }

                if (loadedPluginPaths.contains(canonicalFilePath))
                {
                    m_loggerService.log_info(fmt::format("  Skipping already loaded plugin: {}", canonicalFilePath.filename().string()));
                    continue;
                }

                if (!discoveredPluginPaths.insert(canonicalFilePath).second)
                {
                    continue;
                }

                Plugin plugin{m_loggerService};
                plugin.set_path(canonicalFilePath);

                m_loggerService.log_info(fmt::format("Found plugin: {}", canonicalFilePath.filename().string()));
                pluginStates.push_back(std::move(plugin));
                foundCount++;
            }

            m_loggerService.log_info(fmt::format("Found {} new plugins in this directory", foundCount));
        }

        m_loggerService.log_info(fmt::format("Total plugins discovered: {}", pluginStates.size()));
        return StatusCode::STATUS_OK;
    }

    StatusCode Loader::set_active_plugin(const std::filesystem::path& path)
    {
        m_loggerService.log_info(fmt::format("Setting active plugin by path: {}", path.string()));

        StatusCode result = StatusCode::STATUS_ERROR_PLUGIN_NOT_FOUND;
        for (auto& plugin : m_plugins)
        {
            if (plugin.get_path() == path)
            {
                if (!plugin.is_loaded())
                {
                    m_loggerService.log_info("Plugin not loaded, loading now...");
                    result = load_plugin(plugin.get_path());
                    if (result != StatusCode::STATUS_OK)
                    {
                        m_loggerService.log_error(fmt::format("Failed to load plugin: {}", static_cast<int>(result)));
                        return result;
                    }
                }

                return set_active_plugin(plugin);
            }
        }

        m_loggerService.log_error(fmt::format("Plugin not found: {}", path.string()));
        return result;
    }

    StatusCode Loader::set_active_plugin(const std::size_t index)
    {
        if (index >= m_plugins.size())
        {
            m_loggerService.log_error(fmt::format("Invalid plugin index: {} (max: {})", index, m_plugins.size() - 1));
            m_activePlugin.reset();
            return StatusCode::STATUS_ERROR_GENERAL_OUT_OF_BOUNDS;
        }

        m_loggerService.log_info(fmt::format("Setting active plugin by index: {}", index));
        return set_active_plugin(m_plugins[index]);
    }

    StatusCode Loader::set_active_plugin(Plugin& plugin)
    {
        const bool isSamePlugin = m_activePlugin.has_value() && &m_activePlugin->get() == &plugin;

        if (isSamePlugin && m_activePluginInitialized)
        {
            m_loggerService.log_info(fmt::format("Plugin already active and initialized: {}", plugin.get_path().filename().string()));
            return StatusCode::STATUS_OK;
        }

        if (m_activePlugin.has_value())
        {
            m_loggerService.log_info("Stopping thread dispatcher before plugin switch...");
            const auto stopStatus = m_threadDispatcher.stop();
            if (stopStatus != StatusCode::STATUS_OK) [[unlikely]]
            {
                m_loggerService.log_error(fmt::format("Failed to stop thread dispatcher (status={})", static_cast<int>(stopStatus)));
                return stopStatus;
            }
        }

        m_uiRegistry.clear();
        m_activePlugin = plugin;
        m_activePluginInitialized = false;

        const auto pluginIndex = static_cast<std::size_t>(&plugin - m_plugins.data());
        const StatusCode initStatus = initialize_plugin(plugin, pluginIndex);
        if (initStatus != StatusCode::STATUS_OK)
        {
            m_loggerService.log_error(fmt::format("Failed to initialize plugin: {} (status={})",
                plugin.get_path().filename().string(), static_cast<int>(initStatus)));
            m_activePlugin.reset();
            return initStatus;
        }

        const auto configureStatus = m_threadDispatcher.configure(plugin.get_plugin_info().featureCapability);
        if (configureStatus != StatusCode::STATUS_OK) [[unlikely]]
        {
            m_loggerService.log_error(fmt::format("Failed to configure thread dispatcher for plugin: {} (status={})",
                plugin.get_path().filename().string(), static_cast<int>(configureStatus)));
            m_activePlugin.reset();
            return configureStatus;
        }

        const auto startStatus = m_threadDispatcher.start();
        if (startStatus != StatusCode::STATUS_OK) [[unlikely]]
        {
            m_loggerService.log_error(fmt::format("Failed to start thread dispatcher for plugin: {} (status={})",
                plugin.get_path().filename().string(), static_cast<int>(startStatus)));
            m_activePlugin.reset();
            return startStatus;
        }

        if (plugin.has_feature(VERTEX_FEATURE_RUN_MODE_SINGLE_THREADED))
        {
            m_loggerService.log_info("[Plugin Init] Single-threaded mode detected, dispatching vertex_init to single thread...");

            auto& pluginRef = plugin;
            const auto keepaliveForGuard = plugin.library_keepalive();
            std::packaged_task<StatusCode()> singleThreadInit{
                [&pluginRef, pluginIndex, keepaliveForGuard]() -> StatusCode
                {
                    Scanner::interop::PluginRegistrationGuard guard{pluginIndex, keepaliveForGuard};
                    const auto result = Runtime::safe_call(
                        pluginRef.internal_vertex_init,
                        &pluginRef.get_plugin_info(),
                        &pluginRef.m_runtime,
                        true);
                    return Runtime::get_status(result);
                }};

            auto dispatchResult = m_threadDispatcher.dispatch(Thread::ThreadChannel::Freeze, std::move(singleThreadInit));
            if (!dispatchResult.has_value()) [[unlikely]]
            {
                m_loggerService.log_error(fmt::format("[Plugin Init] Failed to dispatch single-thread vertex_init (status={})",
                    static_cast<int>(dispatchResult.error())));
                m_activePlugin.reset();
                return dispatchResult.error();
            }

            const auto singleThreadStatus = dispatchResult.value().get();
            if (singleThreadStatus != StatusCode::STATUS_OK) [[unlikely]]
            {
                m_loggerService.log_error(fmt::format("[Plugin Init] Single-thread vertex_init failed (status={})",
                    static_cast<int>(singleThreadStatus)));
                m_activePlugin.reset();
                return singleThreadStatus;
            }

            m_loggerService.log_info("[Plugin Init] Single-thread vertex_init completed successfully");
        }

        const auto configStatus = restore_persisted_config(plugin);
        if (configStatus != StatusCode::STATUS_OK)
        {
            m_loggerService.log_warn(fmt::format("Failed to restore persisted config for plugin: {} (status={})",
                plugin.get_path().filename().string(), static_cast<int>(configStatus)));
        }

        m_activePluginInitialized = true;
        m_loggerService.log_info(fmt::format("Active plugin set: {}", plugin.get_path().filename().string()));
        return StatusCode::STATUS_OK;
    }

    StatusCode Loader::restore_persisted_config(const Plugin& plugin)
    {
        const auto pluginFilename = plugin.get_filename();
        const auto loadResult = m_pluginConfigService.load_config(pluginFilename);
        if (loadResult != StatusCode::STATUS_OK)
        {
            m_loggerService.log_warn(fmt::format("[Plugin Config] Failed to load persisted config for plugin '{}' (status={})",
                pluginFilename,
                static_cast<int>(loadResult)));
            return loadResult;
        }

        const auto panels = m_uiRegistry.get_panels();
        if (panels.empty())
        {
            return StatusCode::STATUS_OK;
        }

        m_loggerService.log_info(fmt::format("[Plugin Config] Restoring persisted config for {} panel(s)", panels.size()));

        struct PendingApply final
        {
            VertexOnUIApply_t fn{};
            void* userData{};
            std::string fieldId{};
            UIValue value{};
        };

        std::vector<PendingApply> pendingApplies{};
        std::size_t restoredFieldCount{};

        for (const auto& snapshot : panels)
        {
            const std::string panelId{snapshot.panel.panelId};

            for (const auto& section : std::span{snapshot.panel.sections, snapshot.panel.sectionCount})
            {
                for (const auto& field : std::span{section.fields, section.fieldCount})
                {
                    auto persisted = m_pluginConfigService.get_ui_value(panelId, field.fieldId, field.type);
                    if (!persisted.has_value())
                    {
                        continue;
                    }

                    const auto setResult = m_uiRegistry.set_value(panelId, field.fieldId, *persisted);
                    if (setResult != StatusCode::STATUS_OK)
                    {
                        m_loggerService.log_warn(fmt::format("[Plugin Config] Failed to set restored value for panel='{}', field='{}' (status={})",
                            panelId,
                            field.fieldId,
                            static_cast<int>(setResult)));
                        continue;
                    }
                    restoredFieldCount++;

                    if (snapshot.panel.onApply)
                    {
                        pendingApplies.push_back(PendingApply{
                            .fn = snapshot.panel.onApply,
                            .userData = snapshot.panel.userData,
                            .fieldId = std::string{field.fieldId},
                            .value = *persisted
                        });
                    }
                }
            }
        }

        StatusCode overallStatus = StatusCode::STATUS_OK;

        for (auto& apply : pendingApplies)
        {
            const auto fn = apply.fn;
            const auto userData = apply.userData;
            const auto fieldId = apply.fieldId;
            const auto value = apply.value;

            auto dispatchResult = m_threadDispatcher.dispatch(
                Thread::ThreadChannel::ProcessList,
                std::packaged_task<StatusCode()>{
                    [fn, userData, fieldId, value]() mutable -> StatusCode
                    {
                        fn(fieldId.c_str(), &value, userData);
                        return StatusCode::STATUS_OK;
                    }});

            if (!dispatchResult.has_value())
            {
                if (overallStatus == StatusCode::STATUS_OK)
                {
                    overallStatus = dispatchResult.error();
                }

                m_loggerService.log_warn(fmt::format("[Plugin Config] Failed to dispatch onApply for field='{}' (status={})",
                    apply.fieldId,
                    static_cast<int>(dispatchResult.error())));
                continue;
            }

            try
            {
                const auto applyStatus = dispatchResult.value().get();
                if (applyStatus != StatusCode::STATUS_OK)
                {
                    if (overallStatus == StatusCode::STATUS_OK)
                    {
                        overallStatus = applyStatus;
                    }
                    m_loggerService.log_warn(fmt::format("[Plugin Config] onApply returned non-OK for field='{}' (status={})",
                        apply.fieldId,
                        static_cast<int>(applyStatus)));
                }
            }
            catch (const std::exception& e)
            {
                if (overallStatus == StatusCode::STATUS_OK)
                {
                    overallStatus = StatusCode::STATUS_ERROR_GENERAL;
                }
                m_loggerService.log_warn(fmt::format("[Plugin Config] onApply threw for field='{}': {}",
                    apply.fieldId, e.what()));
            }
            catch (...)
            {
                if (overallStatus == StatusCode::STATUS_OK)
                {
                    overallStatus = StatusCode::STATUS_ERROR_GENERAL;
                }
                m_loggerService.log_warn(fmt::format("[Plugin Config] onApply threw unknown exception for field='{}'",
                    apply.fieldId));
            }
        }

        if (overallStatus == StatusCode::STATUS_OK)
        {
            m_loggerService.log_info(fmt::format("[Plugin Config] Restored {} persisted field(s), applied {} callback(s)",
                restoredFieldCount, pendingApplies.size()));
        }
        else
        {
            m_loggerService.log_warn(fmt::format("[Plugin Config] Restore completed with warnings after restoring {} field(s) and applying {} callback(s) (status={})",
                restoredFieldCount,
                pendingApplies.size(),
                static_cast<int>(overallStatus)));
        }

        return overallStatus;
    }

    StatusCode Loader::initialize_plugin(Plugin& plugin, std::size_t pluginIndex) const
    {
        m_loggerService.log_info(fmt::format("[Plugin Init] Calling vertex_init for: {}", plugin.get_path().filename().string()));

        Scanner::interop::PluginRegistrationGuard guard{pluginIndex, plugin.library_keepalive()};
        const auto initResult = Runtime::safe_call(plugin.internal_vertex_init, &plugin.get_plugin_info(), &plugin.m_runtime, false);

        const auto initStatus = Runtime::get_status(initResult);

        if (!initResult.has_value())
        {
            m_loggerService.log_error("[Plugin Init] vertex_init function pointer is null");
            return StatusCode::STATUS_ERROR_PLUGIN_RESOLVE_FAILURE;
        }

        if (initStatus != StatusCode::STATUS_OK)
        {
            m_loggerService.log_error(fmt::format("[Plugin Init] vertex_init failed with code: {} ({})",
                static_cast<int>(initStatus), status_code_to_string(initStatus)));
            return initStatus;
        }

        m_loggerService.log_info("[Plugin Init] vertex_init completed successfully");
        return StatusCode::STATUS_OK;
    }

    std::optional<std::reference_wrapper<Plugin>> Loader::get_active_plugin()
    {
        if (m_activePlugin.has_value())
        {
            return m_activePlugin;
        }
        return std::nullopt;
    }

    std::string Loader::status_code_to_string(const StatusCode code) const
    {
        switch (code)
        {
            case StatusCode::STATUS_OK:
                return "OK";
            case StatusCode::STATUS_ERROR_PLUGIN_NOT_FOUND:
                return "Plugin not found";
            case StatusCode::STATUS_ERROR_PLUGIN_LOAD_FAILED:
                return "Plugin load failed";
            case StatusCode::STATUS_ERROR_PLUGIN_RESOLVE_FAILURE:
                return "Function resolve failed";
            case StatusCode::STATUS_ERROR_PLUGIN_ALREADY_LOADED:
                return "Plugin already loaded";
            case StatusCode::STATUS_ERROR_INVALID_PARAMETER:
                return "Invalid parameter";
            case StatusCode::STATUS_ERROR_GENERAL_OUT_OF_BOUNDS:
                return "Out of bounds";
            default:
                return fmt::format("Unknown ({})", static_cast<int>(code));
        }
    }

    IRegistry& Loader::get_registry()
    {
        return m_registry;
    }

    const IRegistry& Loader::get_registry() const
    {
        return m_registry;
    }

    IUIRegistry& Loader::get_ui_registry()
    {
        return m_uiRegistry;
    }

    const IUIRegistry& Loader::get_ui_registry() const
    {
        return m_uiRegistry;
    }

    StatusCode Loader::dispatch_event(const VertexEvent event, const void* data)
    {
        if (!m_activePlugin.has_value())
        {
            return StatusCode::STATUS_ERROR_PLUGIN_NOT_ACTIVE;
        }

        const Plugin& plugin = m_activePlugin.value().get();

        if (!plugin.is_loaded())
        {
            return StatusCode::STATUS_ERROR_PLUGIN_NOT_LOADED;
        }

        const auto result = Runtime::safe_call(plugin.internal_vertex_event, event, data);
        return Runtime::get_status(result);
    }

}
