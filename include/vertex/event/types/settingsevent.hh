//
// Copyright (C) 2026 PHTNC<>.
// Licensed under GPLv3.0 with Plugin Interface exceptions.
//
#pragma once

#include <vertex/event/vertexevent.hh>
#include <string>

namespace Vertex::Event
{
    enum class SettingsChangeType
    {
        PLUGIN_LOADED,
        PLUGIN_LOAD_FAILED,
        PLUGIN_ACTIVATED,
        SETTINGS_APPLIED,
        LANGUAGE_CHANGED,
        PATHS_UPDATED,
        GENERAL_SETTING_CHANGED
    };

    class SettingsEvent final : public VertexEvent
    {
    public:
        SettingsEvent(const EventId id, SettingsChangeType changeType, std::string message = "")
            : VertexEvent(id), m_changeType(changeType), m_message(std::move(message))
        {}

        [[nodiscard]] SettingsChangeType get_change_type() const noexcept
        {
            return m_changeType;
        }

        [[nodiscard]] const std::string& get_message() const noexcept
        {
            return m_message;
        }

    private:
        SettingsChangeType m_changeType {};
        std::string m_message {};
    };
}
