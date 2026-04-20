//
// Copyright (C) 2026 PHTNC<>.
// Licensed under GPLv3.0 with Plugin Interface exceptions.
//
#include <vertex/customwidgets/base/sortfiltermodel.hh>

#include <algorithm>
#include <ranges>
#include <utility>

namespace Vertex::CustomWidgets::Base
{
    void SortFilterModel::set_model_row_count(const int count) noexcept
    {
        m_modelRowCount = count < 0 ? 0 : count;
    }

    void SortFilterModel::set_sort(CompareFunction compare, const SortDirection direction)
    {
        m_compare = std::move(compare);
        m_direction = m_compare ? direction : SortDirection::None;
    }

    void SortFilterModel::clear_sort() noexcept
    {
        m_compare = nullptr;
        m_direction = SortDirection::None;
    }

    void SortFilterModel::set_filter(FilterPredicate predicate)
    {
        m_predicate = std::move(predicate);
    }

    void SortFilterModel::clear_filter() noexcept
    {
        m_predicate = nullptr;
    }

    void SortFilterModel::rebuild()
    {
        const bool sortActive = has_sort();
        const bool filterActive = has_filter();

        if (!sortActive && !filterActive)
        {
            m_viewToModel.clear();
            m_modelToView.clear();
            return;
        }

        m_viewToModel.clear();
        m_viewToModel.reserve(static_cast<std::size_t>(m_modelRowCount));

        for (int i{}; i < m_modelRowCount; ++i)
        {
            if (!filterActive || m_predicate(i))
            {
                m_viewToModel.push_back(i);
            }
        }

        if (sortActive)
        {
            const bool ascending = m_direction == SortDirection::Ascending;
            std::ranges::stable_sort(
                m_viewToModel,
                [this, ascending](const int lhs, const int rhs) noexcept
                {
                    const auto ordering = m_compare(lhs, rhs);
                    return ascending ? (ordering < 0) : (ordering > 0);
                }
            );
        }

        m_modelToView.assign(static_cast<std::size_t>(m_modelRowCount), -1);
        for (std::size_t viewIdx{}; viewIdx < m_viewToModel.size(); ++viewIdx)
        {
            m_modelToView[static_cast<std::size_t>(m_viewToModel[viewIdx])] = static_cast<int>(viewIdx);
        }
    }

    int SortFilterModel::model_row_count() const noexcept
    {
        return m_modelRowCount;
    }

    int SortFilterModel::view_row_count() const noexcept
    {
        if (is_identity())
        {
            return m_modelRowCount;
        }
        return static_cast<int>(m_viewToModel.size());
    }

    bool SortFilterModel::is_identity() const noexcept
    {
        return m_viewToModel.empty();
    }

    bool SortFilterModel::has_sort() const noexcept
    {
        return static_cast<bool>(m_compare) && m_direction != SortDirection::None;
    }

    bool SortFilterModel::has_filter() const noexcept
    {
        return static_cast<bool>(m_predicate);
    }

    SortDirection SortFilterModel::sort_direction() const noexcept
    {
        return m_direction;
    }

    int SortFilterModel::model_for_view(const int viewIndex) const noexcept
    {
        if (is_identity())
        {
            return viewIndex;
        }
        if (viewIndex < 0 || viewIndex >= static_cast<int>(m_viewToModel.size()))
        {
            return -1;
        }
        return m_viewToModel[static_cast<std::size_t>(viewIndex)];
    }

    std::optional<int> SortFilterModel::view_for_model(const int modelIndex) const noexcept
    {
        if (modelIndex < 0 || modelIndex >= m_modelRowCount)
        {
            return std::nullopt;
        }
        if (is_identity())
        {
            return modelIndex;
        }
        const int viewIdx = m_modelToView[static_cast<std::size_t>(modelIndex)];
        if (viewIdx < 0)
        {
            return std::nullopt;
        }
        return viewIdx;
    }
}
