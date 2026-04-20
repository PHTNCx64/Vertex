//
// Copyright (C) 2026 PHTNC<>.
// Licensed under GPLv3.0 with Plugin Interface exceptions.
//
#pragma once

#include <wx/dataview.h>
#include <wx/string.h>

namespace Vertex::CustomWidgets::Base
{
    class IFilterableDataViewModel
    {
    public:
        IFilterableDataViewModel() = default;
        IFilterableDataViewModel(const IFilterableDataViewModel&) = delete;
        IFilterableDataViewModel& operator=(const IFilterableDataViewModel&) = delete;
        IFilterableDataViewModel(IFilterableDataViewModel&&) = delete;
        IFilterableDataViewModel& operator=(IFilterableDataViewModel&&) = delete;
        virtual ~IFilterableDataViewModel() = default;

        virtual void set_filter_text(const wxString& filterText) = 0;
        [[nodiscard]] virtual bool passes_filter(const wxDataViewItem& item) const = 0;
    };
}
