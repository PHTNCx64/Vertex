//
// Copyright (C) 2026 PHTNC<>.
// Licensed under GPLv3.0 with Plugin Interface exceptions.
//
#pragma once

#include <wx/bitmap.h>
#include <string_view>

#include <vertex/theme.hh>

namespace Vertex::Gui
{
    class IIconManager
    {
    public:
        virtual ~IIconManager() = default;

        [[nodiscard]] virtual wxBitmap get_icon(std::string_view iconName, int size, Theme theme) = 0;
        virtual void set_theme(Theme theme) = 0;
        [[nodiscard]] virtual Theme get_current_theme() const = 0;
        [[nodiscard]] virtual bool is_dark_mode() const = 0;
    };
}
