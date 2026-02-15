//
// Copyright (C) 2026 PHTNC<>.
// Licensed under GPLv3.0 with Plugin Interface exceptions.
//
#pragma once

namespace Vertex::Gui
{
    class WindowLayout final
    {
    public:
        WindowLayout(
            const int x,
            const int y,
            const int width,
            const int height)
            : m_x(x)
            , m_y(y)
            , m_width(width)
            , m_height(height)
        {
        }

        [[nodiscard]] int get_x() const noexcept
        {
            return m_x;
        }

        [[nodiscard]] int get_y() const noexcept
        {
            return m_y;
        }

        [[nodiscard]] int get_width() const noexcept
        {
            return m_width;
        }

        [[nodiscard]] int get_height() const noexcept
        {
            return m_height;
        }

    private:
        int m_x {};
        int m_y {};
        int m_width {};
        int m_height {};
    };
}
