//
// Copyright (C) 2026 PHTNC<>.
// Licensed under GPLv3.0 with Plugin Interface exceptions.
//
#pragma once

#include <concepts>
#include <expected>
#include <type_traits>
#include <utility>

namespace Vertex::Runtime
{
    // This code is practically quite simple, it just checks if a function is null or not.
    // So technically it protects just from calling null pointer functions, but can't outright prevent crashes.
    // At least under Windows, SEH / ExceptionFilters can be quite helpful for at least logging serious issues. I'm not sure how Unix-like signals work in this regard, need to check when porting.

    enum class CallerError
    {
        NullFunctionPointer
    };

    template <class Fn>
    concept StatusReturning = std::is_pointer_v<Fn> && std::is_function_v<std::remove_pointer_t<Fn>> && std::same_as<StatusCode, std::invoke_result_t<Fn>>;

    template <class Fn, class... Args>
    concept StatusReturningWith = std::is_pointer_v<Fn> && std::is_function_v<std::remove_pointer_t<Fn>> && std::invocable<Fn, Args...> && std::same_as<StatusCode, std::invoke_result_t<Fn, Args...>>;

    template <class Fn, class... Args>
    requires StatusReturningWith<Fn, Args...>
    [[nodiscard]] std::expected<StatusCode, CallerError> safe_call(Fn fn, Args&&... args) noexcept
    {
        if (!fn)
        {
            return std::unexpected{CallerError::NullFunctionPointer};
        }
        return fn(std::forward<Args>(args)...);
    }

    template <class Fn>
    requires StatusReturning<Fn>
    [[nodiscard]] std::expected<StatusCode, CallerError> safe_call(Fn fn) noexcept
    {
        if (!fn)
        {
            return std::unexpected{CallerError::NullFunctionPointer};
        }
        return fn();
    }

    [[nodiscard]] constexpr bool status_ok(const std::expected<StatusCode, CallerError>& result) noexcept
    {
        return result.has_value() && *result == STATUS_OK;
    }

    [[nodiscard]] constexpr StatusCode get_status(const std::expected<StatusCode, CallerError>& result) noexcept
    {
        if (result.has_value())
        {
            return *result;
        }
        return STATUS_ERROR_FUNCTION_NOT_FOUND;
    }
}
