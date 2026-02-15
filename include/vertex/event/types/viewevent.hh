//
// Copyright (C) 2026 PHTNC<>.
// Licensed under GPLv3.0 with Plugin Interface exceptions.
//
#pragma once

#include <vertex/event/vertexevent.hh>

namespace Vertex::Event
{
    class ViewEvent final : public VertexEvent
    {
    public:
        explicit ViewEvent(const EventId id) : VertexEvent(id) {}
    };
}
