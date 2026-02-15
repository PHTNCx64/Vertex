//
// Copyright (C) 2026 PHTNC<>.
// Licensed under GPLv3.0 with Plugin Interface exceptions.
//
#include <sdk/log.h>
#include <sdk/registry.h>
#include <vertex/runtime/loader.hh>

#include "vertex/runtime/caller.hh"

#include <vertex/runtime/function_registry.hh>
#include <vertex/log/log.hh>
#include <vertex/utility.hh>
#include <plugin_function_registration.hh>

#include <unordered_set>
#include <fmt/format.h>
#include <algorithm>

namespace Vertex::Runtime
{

    Loader::Loader(Configuration::ISettings& settingsService, Log::ILog& loggerService)
        : m_settingsService(settingsService), m_loggerService(loggerService)
    {
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
        if (!m_plugins.empty())
        {
            m_loggerService.log_info(fmt::format("Unloading {} plugins", m_plugins.size()));
            m_plugins.clear();
        }
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
            path = std::filesystem::current_path() / path;
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
            path = std::filesystem::current_path() / path;
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

        const std::size_t pluginIndex = (existingPlugin != m_plugins.end())
            ? static_cast<std::size_t>(std::distance(m_plugins.begin(), existingPlugin))
            : (m_plugins.emplace_back(), m_plugins.size() - 1);

        Plugin& plugin = m_plugins[pluginIndex];
        plugin.set_path(canonicalPath);

        auto removePluginOnFailure = [this, pluginIndex](const StatusCode status) -> StatusCode
        {
            m_loggerService.log_info("[Plugin Load] Removing failed plugin entry...");
            m_plugins.erase(m_plugins.begin() + static_cast<std::ptrdiff_t>(pluginIndex));
            return status;
        };

        m_loggerService.log_info(fmt::format("[Plugin Load] Loading library: {}", canonicalPath.string()));
        try
        {
            auto library = std::make_unique<Library>(canonicalPath);
            m_loggerService.log_info(fmt::format("[Plugin Load] Library loaded successfully, handle: 0x{:X}",
                reinterpret_cast<uintptr_t>(library->handle())));

            plugin.set_plugin_handle(library->handle());
            library.release(); // NOLINT - We want to keep the library loaded, the plugin will manage its lifetime
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

        plugin.m_runtime.vertex_register_datatype = nullptr;
        plugin.m_runtime.vertex_unregister_datatype = nullptr;

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

        m_loggerService.log_info("[Plugin Load] Resolving plugin functions...");
        const StatusCode status = resolve_functions(plugin);

        if (status != STATUS_OK)
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

        m_loggerService.log_info(fmt::format("Unloading plugin at index: {}", pluginIndex));
        m_plugins.erase(m_plugins.begin() + pluginIndex); // NOLINT
        return StatusCode::STATUS_OK;
    }

    StatusCode Loader::resolve_functions(Plugin& plugin)
    {
        if (!plugin.get_plugin_handle())
        {
            m_loggerService.log_error("[Function Resolution] Plugin handle is null");
            return StatusCode::STATUS_ERROR_PLUGIN_RESOLVE_FAILURE;
        }

        m_loggerService.log_info(fmt::format("[Function Resolution] Module handle: 0x{:X}",
            reinterpret_cast<uintptr_t>(plugin.get_plugin_handle())));

        try
        {
            auto libraryWrapper = Library::from_handle(plugin.get_plugin_handle());
            FunctionRegistry registry;

            m_loggerService.log_info("[Function Resolution] Starting automated function resolution...");

            register_all_plugin_functions(registry, plugin);

            auto resolveResult = registry.resolve_all(libraryWrapper);
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

            m_loggerService.log_info("[Function Resolution] Calling vertex_init...");
            const auto initResult = Runtime::safe_call(plugin.internal_vertex_init, &plugin.get_plugin_info(), &plugin.m_runtime);
            const auto initStatus = Runtime::get_status(initResult);

            if (!initResult.has_value())
            {
                m_loggerService.log_error("[Function Resolution] vertex_init function pointer is null");
            }
            else if (initStatus != StatusCode::STATUS_OK)
            {
                m_loggerService.log_error(fmt::format("[Function Resolution] vertex_init failed with code: {} ({})",
                    static_cast<int>(initStatus), status_code_to_string(initStatus)));
            }
            else
            {
                m_loggerService.log_info("[Function Resolution] vertex_init completed successfully");
            }

            return initStatus;
        }
        catch (const LibraryError& e)
        {
            m_loggerService.log_error(fmt::format("[Function Resolution] Library error: {}", e.what()));
            return StatusCode::STATUS_ERROR_PLUGIN_RESOLVE_FAILURE;
        }
        catch (const std::exception& e)
        {
            m_loggerService.log_error(fmt::format("[Function Resolution] Unexpected error: {}", e.what()));
            return StatusCode::STATUS_ERROR_PLUGIN_RESOLVE_FAILURE;
        }
    }

    StatusCode Loader::has_plugin_loaded()
    {
        return m_activePlugin.has_value() ? StatusCode::STATUS_OK : StatusCode::STATUS_ERROR_PLUGIN_RESOLVE_FAILURE;
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

        m_loggerService.log_info(fmt::format("Scanning {} plugin directories...", paths.size()));

        for (const std::filesystem::path& path : paths)
        {
            if (!std::filesystem::exists(path) || !std::filesystem::is_directory(path))
            {
                m_loggerService.log_warn(fmt::format("Plugin path does not exist or is not a directory: {}", path.string()));
                continue;
            }

            m_loggerService.log_info(fmt::format("Scanning directory: {}", path.string()));
            std::size_t foundCount{};

            for (std::filesystem::directory_iterator dir(path); dir != std::filesystem::directory_iterator(); ++dir)
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

                if (loadedPluginPaths.contains(filePath))
                {
                    m_loggerService.log_info(fmt::format("  Skipping already loaded plugin: {}", filePath.filename().string()));
                    continue;
                }

                Plugin plugin{};
                plugin.set_path(filePath);

                m_loggerService.log_info(fmt::format("Found plugin: {}", filePath.filename().string()));
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
        m_activePlugin = m_plugins[index];
        return StatusCode::STATUS_OK;
    }

    StatusCode Loader::set_active_plugin(Plugin& plugin)
    {
        m_loggerService.log_info(fmt::format("Active plugin set: {}", plugin.get_path().filename().string()));
        m_activePlugin = plugin;
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
