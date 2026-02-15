//
// Copyright (C) 2026 PHTNC<>.
// Licensed under GPLv3.0 with Plugin Interface exceptions.
//
#pragma once

#include <vertex/event/vertexevent.hh>

namespace Vertex::Event
{
    class ProcessCloseEvent final : public VertexEvent
    {
    public:
        explicit ProcessCloseEvent(const EventId id) : VertexEvent(id) {}
    };
}
