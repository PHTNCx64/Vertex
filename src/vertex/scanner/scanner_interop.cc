//
// Copyright (C) 2026 PHTNC<>.
// Licensed under GPLv3.0 with Plugin Interface exceptions.
//
#include <vertex/scanner/scanner_interop.hh>

#include <vertex/runtime/command.hh>
#include <vertex/scanner/scanner_command.hh>

#include <atomic>
#include <cassert>
#include <limits>
#include <string_view>
#include <utility>

namespace Vertex::Scanner::interop
{
    namespace
    {
        std::atomic<IScannerRuntimeService*> g_scannerInstance{};

        thread_local std::size_t tl_pluginIndex{std::numeric_limits<std::size_t>::max()};
        thread_local std::shared_ptr<Runtime::LibraryHandle> tl_libraryKeepalive{};
        thread_local int tl_depth{};
        thread_local RegistrationMode tl_mode{RegistrationMode::Init};
    }

    void set_scanner_service(IScannerRuntimeService* service) noexcept
    {
        g_scannerInstance.store(service, std::memory_order_release);
    }

    IScannerRuntimeService* get_scanner_service() noexcept
    {
        return g_scannerInstance.load(std::memory_order_acquire);
    }

    PluginRegistrationGuard::PluginRegistrationGuard(std::size_t pluginIndex,
                                                      std::shared_ptr<Runtime::LibraryHandle> keepalive,
                                                      RegistrationMode mode) noexcept
    {
        assert(tl_depth == 0);
        tl_pluginIndex = pluginIndex;
        tl_libraryKeepalive = std::move(keepalive);
        tl_mode = mode;
        ++tl_depth;
    }

    PluginRegistrationGuard::~PluginRegistrationGuard()
    {
        --tl_depth;
        tl_pluginIndex = std::numeric_limits<std::size_t>::max();
        tl_libraryKeepalive.reset();
        tl_mode = RegistrationMode::Init;
    }

    bool has_active_context() noexcept
    {
        return tl_depth > 0;
    }

    bool is_shutdown_context() noexcept
    {
        return tl_depth > 0 && tl_mode == RegistrationMode::Shutdown;
    }

    std::size_t current_plugin_index() noexcept
    {
        return tl_pluginIndex;
    }

    std::shared_ptr<Runtime::LibraryHandle> current_library_keepalive() noexcept
    {
        return tl_libraryKeepalive;
    }
}

extern "C"
{
    StatusCode VERTEX_API vertex_scanner_set_instance(void* handle)
    {
        Vertex::Scanner::interop::set_scanner_service(
            static_cast<Vertex::Scanner::IScannerRuntimeService*>(handle));
        return StatusCode::STATUS_OK;
    }

    void* VERTEX_API vertex_scanner_get_instance()
    {
        return static_cast<void*>(Vertex::Scanner::interop::get_scanner_service());
    }

    StatusCode VERTEX_API vertex_register_datatype(const DataType* type)
    {
        if (!type || !type->typeName || type->valueSize == 0)
        {
            return StatusCode::STATUS_ERROR_INVALID_PARAMETER;
        }
        if (!type->extractor || !type->formatter || !type->converter)
        {
            return StatusCode::STATUS_ERROR_INVALID_PARAMETER;
        }
        if (type->scanModeCount == 0 || type->scanModes == nullptr)
        {
            return StatusCode::STATUS_ERROR_INVALID_PARAMETER;
        }
        for (std::size_t i{}; i < type->scanModeCount; ++i)
        {
            const auto& mode = type->scanModes[i];
            if (!mode.scanModeName || !mode.comparator)
            {
                return StatusCode::STATUS_ERROR_INVALID_PARAMETER;
            }
        }

        if (!Vertex::Scanner::interop::has_active_context())
        {
            return StatusCode::STATUS_ERROR_INVALID_STATE;
        }
        if (Vertex::Scanner::interop::is_shutdown_context())
        {
            return StatusCode::STATUS_ERROR_INVALID_STATE;
        }

        auto* service = Vertex::Scanner::interop::get_scanner_service();
        if (!service)
        {
            return StatusCode::STATUS_ERROR_INVALID_HANDLE;
        }

        const auto id = service->send_command(Vertex::Scanner::service::CmdRegisterType{
            .sdkType = type,
            .sourcePluginIndex = Vertex::Scanner::interop::current_plugin_index(),
            .libraryKeepalive = Vertex::Scanner::interop::current_library_keepalive(),
        });
        if (id == Vertex::Runtime::INVALID_COMMAND_ID)
        {
            return StatusCode::STATUS_SHUTDOWN;
        }
        const auto result = service->await_result(id);
        return result.code;
    }

    StatusCode VERTEX_API vertex_unregister_datatype(const DataType* type)
    {
        if (!type || !type->typeName)
        {
            return StatusCode::STATUS_ERROR_INVALID_PARAMETER;
        }
        if (!Vertex::Scanner::interop::has_active_context())
        {
            return StatusCode::STATUS_ERROR_INVALID_STATE;
        }
        if (Vertex::Scanner::interop::is_shutdown_context())
        {
            return StatusCode::STATUS_ERROR_INVALID_STATE;
        }

        auto* service = Vertex::Scanner::interop::get_scanner_service();
        if (!service)
        {
            return StatusCode::STATUS_ERROR_INVALID_HANDLE;
        }

        const std::string_view targetName{type->typeName};
        auto target{Vertex::Scanner::TypeId::Invalid};
        for (const auto& schema : service->list_types())
        {
            if (schema.name == targetName)
            {
                target = schema.id;
                break;
            }
        }
        if (target == Vertex::Scanner::TypeId::Invalid)
        {
            return StatusCode::STATUS_ERROR_GENERAL_NOT_FOUND;
        }

        const auto id = service->send_command(Vertex::Scanner::service::CmdUnregisterType{.id = target});
        if (id == Vertex::Runtime::INVALID_COMMAND_ID)
        {
            return StatusCode::STATUS_SHUTDOWN;
        }
        const auto result = service->await_result(id);
        return result.code;
    }
}
