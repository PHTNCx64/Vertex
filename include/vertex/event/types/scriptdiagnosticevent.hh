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
    enum class ScriptDiagnosticSeverity
    {
        Info,
        Warning,
        Error
    };

    class ScriptDiagnosticEvent final : public VertexEvent
    {
    public:
        ScriptDiagnosticEvent(ScriptDiagnosticSeverity severity, std::string section, int row, int col, std::string message)
            : VertexEvent(SCRIPT_DIAGNOSTIC_EVENT),
              m_severity{severity},
              m_section{std::move(section)},
              m_row{row},
              m_col{col},
              m_message{std::move(message)}
        {
        }

        [[nodiscard]] ScriptDiagnosticSeverity get_severity() const noexcept
        {
            return m_severity;
        }

        [[nodiscard]] const std::string& get_section() const noexcept
        {
            return m_section;
        }

        [[nodiscard]] int get_row() const noexcept
        {
            return m_row;
        }

        [[nodiscard]] int get_col() const noexcept
        {
            return m_col;
        }

        [[nodiscard]] const std::string& get_message() const noexcept
        {
            return m_message;
        }

    private:
        ScriptDiagnosticSeverity m_severity{};
        std::string m_section{};
        int m_row{};
        int m_col{};
        std::string m_message{};
    };
}
