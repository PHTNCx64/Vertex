//
// Copyright (C) 2026 PHTNC<>.
// Licensed under GPLv3.0 with Plugin Interface exceptions.
//
#include <vertex/customwidgets/scannedvaluesdatamodel.hh>

#include <algorithm>
#include <compare>
#include <ranges>
#include <utility>

namespace Vertex::CustomWidgets
{
    ScannedValuesDataModel::ScannedValuesDataModel(std::shared_ptr<ViewModel::MainViewModel> viewModel)
        : wxDataViewVirtualListModel(0)
        , m_viewModel(std::move(viewModel))
    {
    }

    void ScannedValuesDataModel::reset_rows(const unsigned int newCount)
    {
        Reset(newCount);
        if (m_sortActive)
        {
            rebuild_sort_map();
        }
    }

    void ScannedValuesDataModel::sort_by(const unsigned int column, const bool ascending)
    {
        m_sortActive = true;
        m_sortColumn = column;
        m_sortAscending = ascending;
        rebuild_sort_map();
    }

    void ScannedValuesDataModel::clear_sort()
    {
        m_sortActive = false;
        m_viewToSource.clear();
    }

    bool ScannedValuesDataModel::is_sort_active() const noexcept
    {
        return m_sortActive;
    }

    unsigned int ScannedValuesDataModel::get_source_row(const unsigned int viewRow) const noexcept
    {
        if (!m_sortActive || viewRow >= m_viewToSource.size())
        {
            return viewRow;
        }
        return m_viewToSource[viewRow];
    }

    void ScannedValuesDataModel::rebuild_sort_map()
    {
        const unsigned int count = GetCount();
        m_viewToSource.resize(count);
        for (unsigned int i{}; i < count; ++i)
        {
            m_viewToSource[i] = i;
        }

        if (!m_sortActive || count == 0)
        {
            return;
        }

        m_viewModel->update_cache_window(0, static_cast<int>(count));

        const unsigned int col = m_sortColumn;
        const bool asc = m_sortAscending;

        std::ranges::stable_sort(m_viewToSource,
            [this, col, asc](const unsigned int a, const unsigned int b)
            {
                const auto va = m_viewModel->get_scanned_value_at(static_cast<int>(a));
                const auto vb = m_viewModel->get_scanned_value_at(static_cast<int>(b));
                std::strong_ordering ordering{std::strong_ordering::equal};
                switch (col)
                {
                case ADDRESS_COL:
                    ordering = va.address <=> vb.address;
                    break;
                case VALUE_COL:
                    ordering = va.value <=> vb.value;
                    break;
                case FIRST_VALUE_COL:
                    ordering = va.firstValue <=> vb.firstValue;
                    break;
                case PREVIOUS_VALUE_COL:
                    ordering = va.previousValue <=> vb.previousValue;
                    break;
                default:
                    break;
                }
                return asc ? (ordering < 0) : (ordering > 0);
            });
    }

    unsigned int ScannedValuesDataModel::GetColumnCount() const
    {
        return COLUMN_COUNT;
    }

    wxString ScannedValuesDataModel::GetColumnType([[maybe_unused]] const unsigned int col) const
    {
        return "string";
    }

    void ScannedValuesDataModel::GetValueByRow(wxVariant& variant, const unsigned int row, const unsigned int col) const
    {
        const unsigned int sourceRow = get_source_row(row);
        const auto scannedValue = m_viewModel->get_scanned_value_at(static_cast<int>(sourceRow));

        switch (col)
        {
        case ADDRESS_COL:
            variant = wxString::FromUTF8(scannedValue.address);
            break;
        case VALUE_COL:
            variant = wxString::FromUTF8(scannedValue.value);
            break;
        case FIRST_VALUE_COL:
            variant = wxString::FromUTF8(scannedValue.firstValue);
            break;
        case PREVIOUS_VALUE_COL:
            variant = wxString::FromUTF8(scannedValue.previousValue);
            break;
        default:
            variant = wxString{};
            break;
        }
    }

    bool ScannedValuesDataModel::GetAttrByRow(const unsigned int row, const unsigned int col, wxDataViewItemAttr& attr) const
    {
        const unsigned int sourceRow = get_source_row(row);
        const auto scannedValue = m_viewModel->get_scanned_value_at(static_cast<int>(sourceRow));

        switch (col)
        {
        case ADDRESS_COL:
            attr.SetColour(m_addressColor);
            return true;
        case VALUE_COL:
        {
            const bool valueChanged = !scannedValue.previousValue.empty() &&
                                      scannedValue.value != scannedValue.previousValue;
            attr.SetColour(valueChanged ? m_changedValueColor : m_valueColor);
            return true;
        }
        case FIRST_VALUE_COL:
            attr.SetColour(m_firstValueColor);
            return true;
        case PREVIOUS_VALUE_COL:
            attr.SetColour(m_previousValueColor);
            return true;
        default:
            return false;
        }
    }

    bool ScannedValuesDataModel::SetValueByRow([[maybe_unused]] const wxVariant& variant,
                                                [[maybe_unused]] const unsigned int row,
                                                [[maybe_unused]] const unsigned int col)
    {
        return false;
    }
}
