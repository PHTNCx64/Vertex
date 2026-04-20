//
// Copyright (C) 2026 PHTNC<>.
// Licensed under GPLv3.0 with Plugin Interface exceptions.
//
#pragma once

namespace Vertex::CustomWidgets::Base
{
    class ISortableDataViewModel
    {
    public:
        ISortableDataViewModel() = default;
        ISortableDataViewModel(const ISortableDataViewModel&) = delete;
        ISortableDataViewModel& operator=(const ISortableDataViewModel&) = delete;
        ISortableDataViewModel(ISortableDataViewModel&&) = delete;
        ISortableDataViewModel& operator=(ISortableDataViewModel&&) = delete;
        virtual ~ISortableDataViewModel() = default;

        virtual void sort_by(unsigned int column, bool ascending) = 0;
        virtual void clear_sort() = 0;
        [[nodiscard]] virtual bool is_sort_active() const noexcept = 0;
        [[nodiscard]] virtual unsigned int get_source_row(unsigned int viewRow) const noexcept = 0;
    };
}
