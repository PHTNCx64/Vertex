//
// Copyright (C) 2026 PHTNC<>.
// Licensed under GPLv3.0 with Plugin Interface exceptions.
//
#pragma once

#include <cstdint>
#include <functional>

namespace Vertex::Thread
{
    enum class ThreadChannel : std::uint8_t
    {
        Freeze,
        ProcessList,
        Debugger,
        Scanner
    };
}

template <>
struct std::hash<Vertex::Thread::ThreadChannel>
{
    std::size_t operator()(const Vertex::Thread::ThreadChannel channel) const noexcept
    {
        return std::hash<std::uint8_t>{}(static_cast<std::uint8_t>(channel));
    }
};
