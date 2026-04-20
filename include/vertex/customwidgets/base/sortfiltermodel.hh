//
// Copyright (C) 2026 PHTNC<>.
// Licensed under GPLv3.0 with Plugin Interface exceptions.
//
#pragma once

#include <compare>
#include <functional>
#include <optional>
#include <vector>

#include <vertex/customwidgets/base/columndefinition.hh>

namespace Vertex::CustomWidgets::Base
{
    class SortFilterModel final
    {
    public:
        using CompareFunction = std::function<std::strong_ordering(int, int)>;
        using FilterPredicate = std::function<bool(int)>;

        SortFilterModel() = default;

        void set_model_row_count(int count) noexcept;
        void set_sort(CompareFunction compare, SortDirection direction);
        void clear_sort() noexcept;
        void set_filter(FilterPredicate predicate);
        void clear_filter() noexcept;

        void rebuild();

        [[nodiscard]] int model_row_count() const noexcept;
        [[nodiscard]] int view_row_count() const noexcept;
        [[nodiscard]] bool is_identity() const noexcept;
        [[nodiscard]] bool has_sort() const noexcept;
        [[nodiscard]] bool has_filter() const noexcept;
        [[nodiscard]] SortDirection sort_direction() const noexcept;

        [[nodiscard]] int model_for_view(int viewIndex) const noexcept;
        [[nodiscard]] std::optional<int> view_for_model(int modelIndex) const noexcept;

    private:
        int m_modelRowCount{};
        SortDirection m_direction{SortDirection::None};
        CompareFunction m_compare{};
        FilterPredicate m_predicate{};

        std::vector<int> m_viewToModel{};
        std::vector<int> m_modelToView{};
    };
}
