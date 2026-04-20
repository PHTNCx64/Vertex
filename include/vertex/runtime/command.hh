//
// Copyright (C) 2026 PHTNC<>.
// Licensed under GPLv3.0 with Plugin Interface exceptions.
//
#pragma once

#include <sdk/statuscode.h>

#include <cstdint>

namespace Vertex::Runtime
{
    using CommandId = std::uint64_t;
    using SubscriptionId = std::uint64_t;

    inline constexpr CommandId INVALID_COMMAND_ID {0};
    inline constexpr SubscriptionId INVALID_SUBSCRIPTION_ID {0};

    template<class TPayload>
    struct CommandResult final
    {
        CommandId id {INVALID_COMMAND_ID};
        StatusCode code {STATUS_OK};
        TPayload payload {};
    };
}
