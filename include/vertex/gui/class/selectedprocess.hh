//
// Copyright (C) 2026 PHTNC<>.
// Licensed under GPLv3.0 with Plugin Interface exceptions.
//
#pragma once

#include <optional>
#include <string>
#include <string_view>

namespace Vertex::Class
{
    class SelectedProcess final
    {
    public:
        void set_selected_process_id(const std::uint32_t pid)
        {
            m_selectedProcessId = pid;
        }

        void set_selected_process_name(const std::string_view name)
        {
            m_selectedProcessName = name;
        }

        [[nodiscard]] const std::optional<std::uint32_t>& get_selected_process_id() const noexcept
        {
            return m_selectedProcessId;
        }

        [[nodiscard]] const std::optional<std::string>& get_selected_process_name() const noexcept
        {
            return m_selectedProcessName;
        }

    private:
        std::optional<std::uint32_t> m_selectedProcessId {};
        std::optional<std::string> m_selectedProcessName {};
    };
}
