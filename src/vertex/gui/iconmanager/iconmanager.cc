//
// Copyright (C) 2026 PHTNC<>.
// Licensed under GPLv3.0 with Plugin Interface exceptions.
//
#include <vertex/gui/iconmanager/iconmanager.hh>

#define NANOSVG_IMPLEMENTATION
#define NANOSVGRAST_IMPLEMENTATION
#include <nanosvg/nanosvg.h>
#include <nanosvg/nanosvgrast.h>

#include <Icons.hh>
#include <IconMap.hh>

#include <vector>

namespace Vertex::Gui
{
    IconManager::IconManager()
    {
        m_lightIcons = Vertex::Gui::LightIconMap;
        m_darkIcons = Vertex::Gui::DarkIconMap;
    }

    wxBitmap IconManager::get_icon(const std::string_view iconName, const int size, const Theme theme)
    {
        bool isDark{};
        switch (theme)
        {
        case Theme::Light: isDark = false; break;
        case Theme::Dark: isDark = true; break;
        case Theme::System: isDark = is_dark_mode(); break;
        }

        set_theme(theme);

        const char* svgData = get_svg_data(iconName, isDark);
        if (!svgData)
        {
            return wxNullBitmap;
        }

        return load_svg_from_data(svgData, size);
    }

    Theme IconManager::get_current_theme() const
    {
        return m_currentTheme;
    }

    wxBitmap IconManager::load_svg_from_data(const char* svgData, const int size)
    {
        if (!svgData)
        {
            return wxNullBitmap;
        }

        const std::size_t len = strlen(svgData);
        std::vector<char> mutableSvg(len + 1);
        memcpy(mutableSvg.data(), svgData, len);
        mutableSvg[len] = '\0';

        NSVGimage* image = nsvgParse(mutableSvg.data(), "px", DEFAULT_DPI_SCALE);
        if (!image)
        {
            return wxNullBitmap;
        }

        NSVGrasterizer* rast = nsvgCreateRasterizer();
        if (!rast)
        {
            nsvgDelete(image);
            return wxNullBitmap;
        }

        const float scale = static_cast<float>(size) / std::max(image->width, image->height);
        const int width = static_cast<int>(image->width * scale);
        const int height = static_cast<int>(image->height * scale);

        std::vector<unsigned char> buffer(width * height * 4);
        nsvgRasterize(rast, image, 0, 0, scale, buffer.data(), width, height, width * 4);

        wxImage img(width, height, false);
        img.SetAlpha();

        unsigned char* imgData = img.GetData();
        unsigned char* alphaData = img.GetAlpha();

        for (int i{}; i < width * height; ++i)
        {
            imgData[i * 3] = buffer[i * 4];
            imgData[i * 3 + 1] = buffer[i * 4 + 1];
            imgData[i * 3 + 2] = buffer[i * 4 + 2];
            alphaData[i] = buffer[i * 4 + 3];
        }

        if (width != size || height != size)
        {
            img = img.Scale(size, size, wxIMAGE_QUALITY_HIGH);
        }

        nsvgDeleteRasterizer(rast);
        nsvgDelete(image);

        return {img};
    }

    const char* IconManager::get_svg_data(const std::string_view iconName, const bool isDarkMode) const
    {
#ifdef VERTEX_HAS_GENERATED_ICONS
        const auto& iconMap = isDarkMode ? m_darkIcons : m_lightIcons;
        const auto it = iconMap.find(std::string{iconName});
        if (it != iconMap.end())
        {
            return it->second;
        }
#endif
        return nullptr;
    }

    bool IconManager::is_dark_mode() const
    {
        const wxSystemAppearance& appearance = wxSystemSettings::GetAppearance();
        return appearance.IsDark();
    }

    void IconManager::set_theme(Theme theme)
    {
        m_currentTheme = theme;
    }
}
