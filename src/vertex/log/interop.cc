//
// Copyright (C) 2026 PHTNC<>.
// Licensed under GPLv3.0 with Plugin Interface exceptions.
//
#include <sdk/log.h>
#include <vertex/log/log.hh>

#include <atomic>
#include <cstdarg>
#include <cstdio>
#include <expected>
#include <string>
#include <string_view>
#include <vector>

namespace
{
    constinit Vertex::Log::Log* g_logInstanceStorage{};
    thread_local std::vector<char> t_formatBuffer{};

    [[nodiscard]] std::atomic_ref<Vertex::Log::Log*> log_instance_ref()
    {
        return std::atomic_ref(g_logInstanceStorage);
    }

    [[nodiscard]] std::expected<std::string, StatusCode> format_message(const char* msg, va_list args)
    {
        if (msg == nullptr)
        {
            return std::unexpected(StatusCode::STATUS_ERROR_INVALID_PARAMETER);
        }

        const std::string_view format{msg};

        va_list argsCopy;
        va_copy(argsCopy, args);
        const int size = vsnprintf(nullptr, 0, format.data(), argsCopy);
        va_end(argsCopy);

        if (size < 0)
        {
            return std::unexpected(StatusCode::STATUS_ERROR_FMT_INVALID_CONVERSION);
        }

        if (size == 0)
        {
            return std::string{};
        }

        const std::size_t requiredSize = static_cast<std::size_t>(size) + 1U;
        if (t_formatBuffer.size() < requiredSize)
        {
            t_formatBuffer.resize(requiredSize);
        }

        const int written = vsnprintf(t_formatBuffer.data(), t_formatBuffer.size(), format.data(), args);
        if (written < 0)
        {
            return std::unexpected(StatusCode::STATUS_ERROR_FMT_INVALID_CONVERSION);
        }

        std::string out{};
        out.assign(t_formatBuffer.data(), static_cast<std::size_t>(written));
        return out;
    }

    [[nodiscard]] inline Vertex::Log::Log* log_instance()
    {
        return log_instance_ref().load(std::memory_order_acquire);
    }
}

extern "C"
{
    VertexLogHandle VERTEX_API vertex_log_get_instance()
    {
        return log_instance();
    }

    StatusCode VERTEX_API vertex_log_set_instance(VertexLogHandle handle)
    {
        auto* log = static_cast<Vertex::Log::Log*>(handle);
        log_instance_ref().store(log, std::memory_order_release);
        return StatusCode::STATUS_OK;
    }

    StatusCode VERTEX_API vertex_log_info(const char* msg, ...)
    {
        auto* log = log_instance();
        if (log == nullptr)
        {
            return StatusCode::STATUS_ERROR_INVALID_HANDLE;
        }

        va_list args;
        va_start(args, msg);
        auto formatted = format_message(msg, args);
        va_end(args);

        if (!formatted.has_value())
        {
            return formatted.error();
        }

        return log->log_info(*formatted);
    }

    StatusCode VERTEX_API vertex_log_warn(const char* msg, ...)
    {
        auto* log = log_instance();
        if (log == nullptr)
        {
            return StatusCode::STATUS_ERROR_INVALID_HANDLE;
        }

        va_list args;
        va_start(args, msg);
        auto formatted = format_message(msg, args);
        va_end(args);

        if (!formatted.has_value())
        {
            return formatted.error();
        }

        return log->log_warn(*formatted);
    }

    StatusCode VERTEX_API vertex_log_error(const char* msg, ...)
    {
        auto* log = log_instance();
        if (log == nullptr)
        {
            return StatusCode::STATUS_ERROR_INVALID_HANDLE;
        }

        va_list args;
        va_start(args, msg);
        auto formatted = format_message(msg, args);
        va_end(args);

        if (!formatted.has_value())
        {
            return formatted.error();
        }

        return log->log_error(*formatted);
    }
}
