//
// Copyright (C) 2026 PHTNC<>.
// Licensed under GPLv3.0 with Plugin Interface exceptions.
//
#pragma once

#include <vertex/gui/theme/ithemeprovider.hh>

class wxWindow;
class wxAuiManager;

namespace Vertex::Gui
{
    class ThemeProvider final : public IThemeProvider
    {
    public:
        ThemeProvider();

        [[nodiscard]] const ColorPalette& palette() const noexcept override;
        [[nodiscard]] bool is_dark() const override;
        void set_theme(Theme theme) override;
        [[nodiscard]] Theme current_theme() const noexcept override;
        void refresh() override;

        static void apply_palette_to_tree(wxWindow* root, const ColorPalette& palette);
        static void apply_palette_to_aui(wxAuiManager& manager, const ColorPalette& palette);
        static void apply_theme_to_all_windows(IThemeProvider& themeProvider, bool dispatchSystemColorChanged = true);

    private:
        static void apply_palette_recursive(wxWindow* window, const ColorPalette& palette);
        [[nodiscard]] static ColorPalette make_dark_palette();
        [[nodiscard]] static ColorPalette make_light_palette();
        [[nodiscard]] static ColorPalette make_system_palette(bool isDark);

        Theme m_currentTheme{Theme::System};
        ColorPalette m_palette{};
    };
}
