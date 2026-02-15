//
// Copyright (C) 2026 PHTNC<>.
// Licensed under GPLv3.0 with Plugin Interface exceptions.
//
#pragma once

#include <wx/wx.h>
#include <string>
#include <string_view>
#include <vertex/gui/iconmanager/iiconmanager.hh>

namespace Vertex::Gui
{
    class IconManager final : public IIconManager
    {
    public:
        IconManager();

        [[nodiscard]] wxBitmap get_icon(std::string_view iconName, int size, Theme theme) override;
        void set_theme(Theme theme) override;
        [[nodiscard]] Theme get_current_theme() const override;
        [[nodiscard]] bool is_dark_mode() const override;

    private:
        [[nodiscard]] wxBitmap load_svg_from_data(const char* svgData, int size);
        [[nodiscard]] const char* get_svg_data(std::string_view iconName, bool isDarkMode) const;

        Theme m_currentTheme {Theme::System};

        std::unordered_map<std::string, const char*> m_lightIcons {};
        std::unordered_map<std::string, const char*> m_darkIcons {};

        static constexpr int MINIMUM_SVG_SIZE {8};
        static constexpr int MAXIMUM_SVG_SIZE {512};
        static constexpr float DEFAULT_DPI_SCALE {96.0f};
    };
}
