//
// Copyright (C) 2026 PHTNC<>.
// Licensed under GPLv3.0 with Plugin Interface exceptions.
//
#include <vertex/gui/theme/themeprovider.hh>

#include <wx/settings.h>
#include <wx/window.h>
#include <wx/aui/aui.h>

namespace Vertex::Gui
{
    ThemeProvider::ThemeProvider()
    {
        refresh();
    }

    const ColorPalette& ThemeProvider::palette() const noexcept
    {
        return m_palette;
    }

    bool ThemeProvider::is_dark() const
    {
        switch (m_currentTheme)
        {
            case Theme::Dark:
                return true;
            case Theme::Light:
                return false;
            case Theme::System:
            default:
                return wxSystemSettings::GetAppearance().IsDark();
        }
    }

    void ThemeProvider::set_theme(const Theme theme)
    {
        m_currentTheme = theme;
        refresh();
    }

    Theme ThemeProvider::current_theme() const noexcept
    {
        return m_currentTheme;
    }

    void ThemeProvider::refresh()
    {
        switch (m_currentTheme)
        {
            case Theme::Dark:
                m_palette = make_dark_palette();
                break;
            case Theme::Light:
                m_palette = make_light_palette();
                break;
            case Theme::System:
            default:
                m_palette = make_system_palette(is_dark());
                break;
        }
    }

    ColorPalette ThemeProvider::make_dark_palette()
    {
        return ColorPalette{};
    }

    ColorPalette ThemeProvider::make_light_palette()
    {
        ColorPalette p;

        p.background        = {0xF5, 0xF5, 0xF5};
        p.backgroundAlt     = {0xEB, 0xEB, 0xEB};
        p.panel             = {0xE0, 0xE0, 0xE0};
        p.currentLine       = {0xD6, 0xEA, 0xF8};
        p.selection         = {0xAD, 0xD6, 0xFF};
        p.border            = {0xC8, 0xC8, 0xC8};
        p.text              = {0x1E, 0x1E, 0x1E};
        p.textSecondary     = {0x6E, 0x6E, 0x6E};
        p.textHeader        = {0x33, 0x33, 0x33};
        p.separator         = {0xB0, 0xB0, 0xB0};

        p.accent            = {0x00, 0x66, 0xB8};
        p.error             = {0xC0, 0x17, 0x17};
        p.warning           = {0xBF, 0x8C, 0x00};
        p.success           = {0x0E, 0x7A, 0x0D};

        p.syntaxKeyword     = {0xAF, 0x00, 0xDB};
        p.syntaxFlowJump    = {0xAF, 0x00, 0xDB};
        p.syntaxFlowCall    = {0x0E, 0x7A, 0x0D};
        p.syntaxFlowReturn  = {0x79, 0x5E, 0x26};
        p.syntaxMove        = {0x00, 0x1B, 0x80};
        p.syntaxArithmetic  = {0x09, 0x86, 0x58};
        p.syntaxRegister    = {0x00, 0x70, 0xC1};
        p.syntaxImmediate   = {0x09, 0x86, 0x58};
        p.syntaxMemory      = {0x79, 0x5E, 0x26};
        p.syntaxComment     = {0x00, 0x80, 0x00};
        p.syntaxType        = {0xAF, 0x00, 0xDB};
        p.syntaxSymbol      = {0x0E, 0x7A, 0x0D};
        p.syntaxOperand     = {0xA3, 0x15, 0x15};

        p.markerBreakpoint         = {0xE5, 0x1A, 0x1A};
        p.markerBreakpointDisabled = {0xCC, 0x66, 0x66};
        p.markerBreakpointPending  = {0xD4, 0xA0, 0x17};
        p.markerCurrent            = {0xEB, 0xA6, 0x00};
        p.markerChanged            = {0xC0, 0x17, 0x17};
        p.markerFrozen             = {0x0E, 0x7A, 0x0D};

        p.breakpointLine         = {0xFC, 0xE4, 0xE4};
        p.breakpointLineDisabled = {0xF5, 0xEB, 0xEB};
        p.breakpointLinePending  = {0xF5, 0xF0, 0xDC};

        p.arrowUnconditional = {0x00, 0x66, 0xB8};
        p.arrowConditional   = {0xAF, 0x00, 0xDB};
        p.arrowCall          = {0x0E, 0x7A, 0x0D};
        p.arrowLoop          = {0x79, 0x5E, 0x26};

        p.gutterBackground = {0xE0, 0xE0, 0xE0};
        p.gutterBorder     = {0xC8, 0xC8, 0xC8};
        p.dragIndicator    = {0x00, 0x66, 0xB8};
        p.draggedColumn    = {0xD0, 0xD0, 0xD0};

        p.loadingText       = {0x00, 0x66, 0xB8};
        p.endOfRangeText    = {0x00, 0x80, 0x00};
        p.errorRetryText    = {0x00, 0x66, 0xB8};
        p.edgeIndicatorBg   = {0xE8, 0xE8, 0xF0};
        p.functionEntryLine = {0xC8, 0xC8, 0xC8};

        p.logDebug   = {0x6E, 0x6E, 0x6E};
        p.logInfo    = {0x1E, 0x1E, 0x1E};
        p.logWarning = {0xBF, 0x8C, 0x00};
        p.logError   = {0xC0, 0x17, 0x17};
        p.logOutput  = {0x0E, 0x7A, 0x0D};

        return p;
    }

    ColorPalette ThemeProvider::make_system_palette(const bool isDark)
    {
        ColorPalette p = isDark ? make_dark_palette() : make_light_palette();

        const auto sys = [](const wxSystemColour id)
        {
            return wxSystemSettings::GetColour(id);
        };

        const auto blend = [](const wxColour& a, const wxColour& b, const double t) -> wxColour
        {
            return {
                static_cast<unsigned char>(a.Red()   + t * (b.Red()   - a.Red())),
                static_cast<unsigned char>(a.Green() + t * (b.Green() - a.Green())),
                static_cast<unsigned char>(a.Blue()  + t * (b.Blue()  - a.Blue()))
            };
        };

        const auto shift = [](const wxColour& base, const int amount) -> wxColour
        {
            return {
                static_cast<unsigned char>(std::clamp(base.Red()   + amount, 0, 255)),
                static_cast<unsigned char>(std::clamp(base.Green() + amount, 0, 255)),
                static_cast<unsigned char>(std::clamp(base.Blue()  + amount, 0, 255))
            };
        };

        p.background    = sys(wxSYS_COLOUR_WINDOW);
        p.panel         = sys(wxSYS_COLOUR_BTNFACE);
        p.selection     = sys(wxSYS_COLOUR_HIGHLIGHT);
        p.border        = sys(wxSYS_COLOUR_3DSHADOW);
        p.text          = sys(wxSYS_COLOUR_WINDOWTEXT);
        p.textSecondary = sys(wxSYS_COLOUR_GRAYTEXT);
        p.accent        = sys(wxSYS_COLOUR_HOTLIGHT);
        p.separator     = sys(wxSYS_COLOUR_3DLIGHT);

        const int nudge = isDark ? 10 : -10;
        p.backgroundAlt = shift(p.background, nudge);
        p.textHeader    = blend(p.text, p.textSecondary, 0.15);
        p.currentLine   = blend(p.background, p.selection, 0.2);

        p.gutterBackground = p.panel;
        p.gutterBorder     = p.border;

        p.dragIndicator    = p.accent;
        p.draggedColumn    = blend(p.panel, p.selection, 0.3);

        p.loadingText       = p.accent;
        p.errorRetryText    = p.accent;
        p.endOfRangeText    = p.success;
        p.edgeIndicatorBg   = shift(p.background, isDark ? -8 : 8);
        p.functionEntryLine = p.border;

        p.logDebug = p.textSecondary;
        p.logInfo  = p.text;

        return p;
    }

    void ThemeProvider::apply_palette_to_tree(wxWindow* root, const ColorPalette& palette)
    {
        if (!root)
        {
            return;
        }

        root->Freeze();
        apply_palette_recursive(root, palette);
        root->Thaw();
    }

    void ThemeProvider::apply_palette_recursive(wxWindow* window, const ColorPalette& palette)
    {
        if (!window)
        {
            return;
        }

        if (window->GetBackgroundStyle() != wxBG_STYLE_PAINT)
        {
            window->SetBackgroundColour(palette.panel);
            window->SetForegroundColour(palette.text);
        }

        for (auto* child : window->GetChildren())
        {
            apply_palette_recursive(child, palette);
        }
    }

    void ThemeProvider::apply_palette_to_aui(wxAuiManager& manager, const ColorPalette& palette)
    {
        auto* art = manager.GetArtProvider();
        if (!art)
        {
            return;
        }

        art->SetColour(wxAUI_DOCKART_BACKGROUND_COLOUR, palette.panel);
        art->SetColour(wxAUI_DOCKART_SASH_COLOUR, palette.border);
        art->SetColour(wxAUI_DOCKART_BORDER_COLOUR, palette.border);
        art->SetColour(wxAUI_DOCKART_GRIPPER_COLOUR, palette.panel);
        art->SetColour(wxAUI_DOCKART_ACTIVE_CAPTION_COLOUR, palette.backgroundAlt);
        art->SetColour(wxAUI_DOCKART_ACTIVE_CAPTION_GRADIENT_COLOUR, palette.backgroundAlt);
        art->SetColour(wxAUI_DOCKART_ACTIVE_CAPTION_TEXT_COLOUR, palette.text);
        art->SetColour(wxAUI_DOCKART_INACTIVE_CAPTION_COLOUR, palette.panel);
        art->SetColour(wxAUI_DOCKART_INACTIVE_CAPTION_GRADIENT_COLOUR, palette.panel);
        art->SetColour(wxAUI_DOCKART_INACTIVE_CAPTION_TEXT_COLOUR, palette.textSecondary);

        manager.Update();
    }

    void ThemeProvider::apply_theme_to_all_windows(IThemeProvider& themeProvider, const bool dispatchSystemColorChanged)
    {
        themeProvider.refresh();
        const auto& pal = themeProvider.palette();

        for (auto node = wxTopLevelWindows.GetFirst(); node; node = node->GetNext())
        {
            auto* window = node->GetData();
            if (!window)
            {
                continue;
            }

            apply_palette_to_tree(window, pal);

            if (dispatchSystemColorChanged)
            {
                wxSysColourChangedEvent event;
                window->GetEventHandler()->ProcessEvent(event);
            }

            window->Refresh();
        }
    }
}
