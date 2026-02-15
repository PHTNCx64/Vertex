//
// Copyright (C) 2026 PHTNC<>.
// Licensed under GPLv3.0 with Plugin Interface exceptions.
//
#pragma once

#include <string>
#include <utility>
#include <vertex/event/vertexevent.hh>

namespace Vertex::Event
{
    class ProcessOpenEvent final : public VertexEvent
    {
    public:
        ProcessOpenEvent(const EventId id, const std::uint32_t processId, std::string processName)
            : VertexEvent(id), m_processName(std::move(processName)), m_processId(processId)
        {}

        [[nodiscard]] const std::string& get_process_name() const noexcept
        {
            return m_processName;
        }

        [[nodiscard]] std::uint32_t get_process_id() const noexcept
        {
            return m_processId;
        }

    private:
        std::string m_processName {};
        std::uint32_t m_processId {};
    };
}
