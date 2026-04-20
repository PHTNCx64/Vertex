//
// Copyright (C) 2026 PHTNC<>.
// Licensed under GPLv3.0 with Plugin Interface exceptions.
//
#pragma once

#include <cstddef>

#include <sdk/macro.h>
#include <sdk/memory.h>
#include <sdk/statuscode.h>

#include <vertex/runtime/function_registry.hh>
#include <vertex/scanner/iscannerruntimeservice.hh>

#include <cstddef>
#include <memory>

namespace Vertex::Scanner::interop
{
    void set_scanner_service(IScannerRuntimeService* service) noexcept;
    [[nodiscard]] IScannerRuntimeService* get_scanner_service() noexcept;

    enum class RegistrationMode : std::uint8_t
    {
        Init,
        Shutdown
    };

    class PluginRegistrationGuard final
    {
      public:
        PluginRegistrationGuard(std::size_t pluginIndex,
                                std::shared_ptr<Runtime::LibraryHandle> keepalive,
                                RegistrationMode mode = RegistrationMode::Init) noexcept;
        ~PluginRegistrationGuard();

        PluginRegistrationGuard(const PluginRegistrationGuard&) = delete;
        PluginRegistrationGuard& operator=(const PluginRegistrationGuard&) = delete;
        PluginRegistrationGuard(PluginRegistrationGuard&&) = delete;
        PluginRegistrationGuard& operator=(PluginRegistrationGuard&&) = delete;
    };

    [[nodiscard]] bool has_active_context() noexcept;
    [[nodiscard]] bool is_shutdown_context() noexcept;
    [[nodiscard]] std::size_t current_plugin_index() noexcept;
    [[nodiscard]] std::shared_ptr<Runtime::LibraryHandle> current_library_keepalive() noexcept;
}

extern "C"
{
    StatusCode VERTEX_API vertex_scanner_set_instance(void* handle);
    [[nodiscard]] void* VERTEX_API vertex_scanner_get_instance();
    StatusCode VERTEX_API vertex_register_datatype(const DataType* type);
    StatusCode VERTEX_API vertex_unregister_datatype(const DataType* type);
}
