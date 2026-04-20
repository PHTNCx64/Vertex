//
// Copyright (C) 2026 PHTNC<>.
// Licensed under GPLv3.0 with Plugin Interface exceptions.
//
#include <vertex/customwidgets/savedaddressesdatamodel.hh>

#include <algorithm>
#include <compare>
#include <ranges>
#include <utility>

namespace Vertex::CustomWidgets
{
    SavedAddressesDataModel::SavedAddressesDataModel(std::shared_ptr<ViewModel::MainViewModel> viewModel)
        : wxDataViewVirtualListModel(0)
        , m_viewModel(std::move(viewModel))
    {
    }

    void SavedAddressesDataModel::set_freeze_toggle_callback(FreezeToggleCallback callback)
    {
        m_freezeToggleCallback = std::move(callback);
    }

    void SavedAddressesDataModel::set_value_edit_callback(ValueEditCallback callback)
    {
        m_valueEditCallback = std::move(callback);
    }

    void SavedAddressesDataModel::reset_rows(const unsigned int newCount)
    {
        Reset(newCount);
        if (m_sortActive)
        {
            rebuild_sort_map();
        }
    }

    void SavedAddressesDataModel::sort_by(const unsigned int column, const bool ascending)
    {
        m_sortActive = true;
        m_sortColumn = column;
        m_sortAscending = ascending;
        rebuild_sort_map();
    }

    void SavedAddressesDataModel::clear_sort()
    {
        m_sortActive = false;
        m_viewToSource.clear();
    }

    bool SavedAddressesDataModel::is_sort_active() const noexcept
    {
        return m_sortActive;
    }

    unsigned int SavedAddressesDataModel::get_source_row(const unsigned int viewRow) const noexcept
    {
        if (!m_sortActive || viewRow >= m_viewToSource.size())
        {
            return viewRow;
        }
        return m_viewToSource[viewRow];
    }

    void SavedAddressesDataModel::rebuild_sort_map()
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

        const unsigned int col = m_sortColumn;
        const bool asc = m_sortAscending;

        std::ranges::stable_sort(m_viewToSource,
            [this, col, asc](const unsigned int a, const unsigned int b)
            {
                const auto va = m_viewModel->get_saved_address_at(static_cast<int>(a));
                const auto vb = m_viewModel->get_saved_address_at(static_cast<int>(b));
                std::strong_ordering ordering{std::strong_ordering::equal};
                switch (col)
                {
                case FREEZE_COL:
                    ordering = va.frozen <=> vb.frozen;
                    break;
                case ADDRESS_COL:
                    ordering = va.address <=> vb.address;
                    break;
                case TYPE_COL:
                    ordering = va.valueTypeIndex <=> vb.valueTypeIndex;
                    break;
                case VALUE_COL:
                    ordering = va.value <=> vb.value;
                    break;
                default:
                    break;
                }
                return asc ? (ordering < 0) : (ordering > 0);
            });
    }

    unsigned int SavedAddressesDataModel::GetColumnCount() const
    {
        return COLUMN_COUNT;
    }

    wxString SavedAddressesDataModel::GetColumnType(const unsigned int col) const
    {
        switch (col)
        {
        case FREEZE_COL: return "bool";
        case TYPE_COL:   return "long";
        default:         return "string";
        }
    }

    void SavedAddressesDataModel::GetValueByRow(wxVariant& variant, const unsigned int row, const unsigned int col) const
    {
        const unsigned int sourceRow = get_source_row(row);
        const auto saved = m_viewModel->get_saved_address_at(static_cast<int>(sourceRow));

        switch (col)
        {
        case FREEZE_COL:
            variant = saved.frozen;
            break;
        case ADDRESS_COL:
            variant = wxString::FromUTF8(saved.addressStr);
            break;
        case TYPE_COL:
        {
            const long rawIndex = static_cast<long>(saved.valueTypeIndex);
            const long typeCount = static_cast<long>(m_viewModel->get_value_type_names().size());
            const long clampedIndex = (rawIndex < 0 || typeCount == 0)
                ? 0
                : std::min(rawIndex, typeCount - 1);
            variant = clampedIndex;
            break;
        }
        case VALUE_COL:
            variant = wxString::FromUTF8(saved.value);
            break;
        default:
            variant = wxString{};
            break;
        }
    }

    bool SavedAddressesDataModel::GetAttrByRow(const unsigned int row, const unsigned int col, wxDataViewItemAttr& attr) const
    {
        const unsigned int sourceRow = get_source_row(row);
        switch (col)
        {
        case ADDRESS_COL:
            attr.SetColour(m_addressColor);
            return true;
        case TYPE_COL:
            attr.SetColour(m_typeColor);
            return true;
        case VALUE_COL:
        {
            const auto saved = m_viewModel->get_saved_address_at(static_cast<int>(sourceRow));
            attr.SetColour(saved.frozen ? m_frozenValueColor : m_valueColor);
            return true;
        }
        default:
            return false;
        }
    }

    bool SavedAddressesDataModel::SetValueByRow(const wxVariant& variant, const unsigned int row, const unsigned int col)
    {
        const unsigned int sourceRow = get_source_row(row);
        const int rowIndex = static_cast<int>(sourceRow);

        switch (col)
        {
        case FREEZE_COL:
        {
            const bool frozen = variant.GetBool();
            m_viewModel->set_saved_address_frozen(rowIndex, frozen);
            if (m_freezeToggleCallback)
            {
                m_freezeToggleCallback(rowIndex, frozen);
            }
            RowChanged(row);
            return true;
        }
        case ADDRESS_COL:
        {
            const wxString newAddressStr = variant.GetString();
            try
            {
                const std::uint64_t newAddress = std::stoull(newAddressStr.ToStdString(), nullptr, 16);
                m_viewModel->set_saved_address_address(rowIndex, newAddress);
                RowChanged(row);
                return true;
            }
            catch (...)
            {
                return false;
            }
        }
        case TYPE_COL:
        {
            const int newTypeIndex = static_cast<int>(variant.GetLong());
            if (newTypeIndex < 0)
            {
                return false;
            }
            m_viewModel->set_saved_address_type(rowIndex, newTypeIndex);
            RowChanged(row);
            return true;
        }
        case VALUE_COL:
        {
            const std::string newValue = variant.GetString().ToStdString();
            m_viewModel->set_saved_address_value(rowIndex, newValue);
            if (m_valueEditCallback)
            {
                m_valueEditCallback(rowIndex, newValue);
            }
            RowChanged(row);
            return true;
        }
        default:
            return false;
        }
    }
}
