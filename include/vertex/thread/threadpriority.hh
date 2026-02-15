//
// Copyright (C) 2026 PHTNC<>.
// Licensed under GPLv3.0 with Plugin Interface exceptions.
//
#pragma once

namespace Vertex::Thread
{
    // TODO: Figure out how to best abstract this.
    // Specifically with Windows thread prio levels and linux nice values, we probably need dirty macro hacks here.
    // But custom thread priority settings for workers are not implemented anyways yet.
    enum class ThreadPriority
    {
        Lowest = 0,
        Low,
        Normal,
        High,
        Highest,
        Critical
    };
}
