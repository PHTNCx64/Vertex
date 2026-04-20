//
// Copyright (C) 2026 PHTNC<>.
// Licensed under GPLv3.0 with Plugin Interface exceptions.
//
#pragma once

#include <cstdint>

#include <wx/string.h>

namespace Vertex::CustomWidgets::Base
{
    enum class ColumnAlignment : std::uint8_t
    {
        Left,
        Right,
        Center
    };

    enum class SortDirection : std::uint8_t
    {
        None,
        Ascending,
        Descending
    };

    struct ColumnDefinition final
    {
        static constexpr int MIN_COLUMN_WIDTH_DEFAULT{32};

        wxString title{};
        int width{};
        int minWidth{MIN_COLUMN_WIDTH_DEFAULT};
        int maxWidth{};
        ColumnAlignment alignment{ColumnAlignment::Left};
        bool resizable{true};
        bool autoSizable{true};
        bool sortable{true};
        bool filterable{true};
    };
}
