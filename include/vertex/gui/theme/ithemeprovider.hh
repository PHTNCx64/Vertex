//
// Copyright (C) 2026 PHTNC<>.
// Licensed under GPLv3.0 with Plugin Interface exceptions.
//
#pragma once

#include <vertex/gui/theme/colorpalette.hh>
#include <vertex/theme.hh>

namespace Vertex::Gui
{
    class IThemeProvider
    {
    public:
        virtual ~IThemeProvider() = default;

        [[nodiscard]] virtual const ColorPalette& palette() const noexcept = 0;
        [[nodiscard]] virtual bool is_dark() const = 0;
        virtual void set_theme(Theme theme) = 0;
        [[nodiscard]] virtual Theme current_theme() const noexcept = 0;
        virtual void refresh() = 0;
    };
}
