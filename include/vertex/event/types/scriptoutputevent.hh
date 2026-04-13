//
// Copyright (C) 2026 PHTNC<>.
// Licensed under GPLv3.0 with Plugin Interface exceptions.
//
#pragma once

#include <vertex/event/eventid.hh>
#include <vertex/event/vertexevent.hh>

#include <string>
#include <utility>

namespace Vertex::Event
{
    enum class ScriptOutputSeverity
    {
        Info,
        Warning,
        Error
    };

    class ScriptOutputEvent final : public VertexEvent
    {
    public:
        ScriptOutputEvent(ScriptOutputSeverity severity, std::string message)
            : VertexEvent(SCRIPT_OUTPUT_EVENT),
              m_severity{severity},
              m_message{std::move(message)}
        {
        }

        [[nodiscard]] ScriptOutputSeverity get_severity() const noexcept
        {
            return m_severity;
        }

        [[nodiscard]] const std::string& get_message() const noexcept
        {
            return m_message;
        }

    private:
        ScriptOutputSeverity m_severity{};
        std::string m_message{};
    };
}
