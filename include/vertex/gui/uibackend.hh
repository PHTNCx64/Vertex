//
// Copyright (C) 2026 PHTNC<>.
// Licensed under GPLv3.0 with Plugin Interface exceptions.
//

#pragma once

namespace Vertex::Gui
{
    enum class UIBackend
    {
        MSW, // Windows UI
        GTK, // Anything running on GTK, mostly Linux
        COCOA, // macOS UI
    };

    consteval UIBackend get_ui_backend() noexcept
    {
#if defined (__WXGTK__)
        return UIBackend::GTK;
#elif defined (__WXMSW__) || defined (__WXWINE__)
        return UIBackend::MSW;
#elif defined (__WXOSX__) || defined (__WXMAC__)
        return UIBackend::COCOA;
#endif
    }
}