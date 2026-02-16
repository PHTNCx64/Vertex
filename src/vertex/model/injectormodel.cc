//
// Copyright (C) 2026 PHTNC<>.
// Licensed under GPLv3.0 with Plugin Interface exceptions.
//
#include <vertex/model/injectormodel.hh>

#include <vertex/runtime/caller.hh>

#include <fmt/format.h>
#include <ranges>

namespace
{
    constexpr auto* MODEL_NAME = "InjectorModel";
}

namespace Vertex::Model
{
    StatusCode InjectorModel::get_injection_methods(std::vector<InjectionMethod>& methods) const
    {
        methods.clear();

        if (m_loaderService.has_plugin_loaded() != StatusCode::STATUS_OK)
        {
            m_loggerService.log_error(fmt::format("{}: No active plugin", MODEL_NAME));
            return StatusCode::STATUS_ERROR_PLUGIN_NOT_ACTIVE;
        }

        const auto& plugin = m_loaderService.get_active_plugin().value().get();

        std::uint32_t count{};
        const auto countResult = Runtime::safe_call(plugin.internal_vertex_process_get_injection_methods, nullptr, &count);
        const auto status = Runtime::get_status(countResult);
        if (status == StatusCode::STATUS_ERROR_FUNCTION_NOT_FOUND)
        {
            m_loggerService.log_error(fmt::format("{}: internal_vertex_process_get_injection_methods not implemented", MODEL_NAME));
            return StatusCode::STATUS_ERROR_PLUGIN_FUNCTION_NOT_IMPLEMENTED;
        }
        if (!Runtime::status_ok(countResult) || count == 0)
        {
            return status;
        }

        std::vector<InjectionMethod> buffer(count);
        InjectionMethod* bufferPtr = buffer.data();
        const auto listResult = Runtime::safe_call(plugin.internal_vertex_process_get_injection_methods, &bufferPtr, &count);
        if (!Runtime::status_ok(listResult))
        {
            m_loggerService.log_error(fmt::format("{}: internal_vertex_process_get_injection_methods failed", MODEL_NAME));
            return Runtime::get_status(listResult);
        }

        methods.assign(bufferPtr, bufferPtr + count);
        return StatusCode::STATUS_OK;
    }

    StatusCode InjectorModel::get_library_extensions(std::vector<std::string>& extensions) const
    {
        extensions.clear();

        if (m_loaderService.has_plugin_loaded() != StatusCode::STATUS_OK)
        {
            m_loggerService.log_error(fmt::format("{}: No active plugin", MODEL_NAME));
            return StatusCode::STATUS_ERROR_PLUGIN_NOT_ACTIVE;
        }

        const auto& plugin = m_loaderService.get_active_plugin().value().get();

        std::uint32_t count{};
        const auto countResult = Runtime::safe_call(plugin.internal_vertex_process_get_library_extensions, nullptr, &count);
        const auto status = Runtime::get_status(countResult);
        if (status == StatusCode::STATUS_ERROR_FUNCTION_NOT_FOUND)
        {
            m_loggerService.log_error(fmt::format("{}: internal_vertex_process_get_library_extensions not implemented", MODEL_NAME));
            return StatusCode::STATUS_ERROR_PLUGIN_FUNCTION_NOT_IMPLEMENTED;
        }
        if (!Runtime::status_ok(countResult) || count == 0)
        {
            return status;
        }

        std::vector<char*> extPtrs(count, nullptr);
        const auto extResult = Runtime::safe_call(plugin.internal_vertex_process_get_library_extensions, extPtrs.data(), &count);
        if (!Runtime::status_ok(extResult))
        {
            m_loggerService.log_error(fmt::format("{}: internal_vertex_process_get_library_extensions failed", MODEL_NAME));
            return Runtime::get_status(extResult);
        }

        std::ranges::for_each(extPtrs | std::views::filter([](const auto ptr) { return ptr != nullptr; }),
                              [&extensions](const auto ptr) { extensions.emplace_back(ptr); });
        return StatusCode::STATUS_OK;
    }

    StatusCode InjectorModel::inject(const InjectionMethod& method, const std::string_view libraryPath) const
    {
        if (!method.injectableFunction) [[unlikely]]
        {
            return StatusCode::STATUS_ERROR_INVALID_PARAMETER;
        }

        return method.injectableFunction(libraryPath.data());
    }
}
