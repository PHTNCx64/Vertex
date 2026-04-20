//
// Copyright (C) 2026 PHTNC<>.
// Licensed under GPLv3.0 with Plugin Interface exceptions.
//
#pragma once

#include <cstdint>
#include <functional>
#include <memory>
#include <string>
#include <vector>

#include <wx/colour.h>
#include <wx/dataview.h>

#include <vertex/customwidgets/base/isortabledataviewmodel.hh>
#include <vertex/viewmodel/mainviewmodel.hh>

namespace Vertex::CustomWidgets
{
    class SavedAddressesDataModel final
        : public wxDataViewVirtualListModel
        , public Base::ISortableDataViewModel
    {
    public:
        using FreezeToggleCallback = std::function<void(int index, bool frozen)>;
        using ValueEditCallback = std::function<void(int index, const std::string& newValue)>;

        static constexpr unsigned int COLUMN_COUNT{4};
        static constexpr unsigned int FREEZE_COL{};
        static constexpr unsigned int ADDRESS_COL{1};
        static constexpr unsigned int TYPE_COL{2};
        static constexpr unsigned int VALUE_COL{3};

        explicit SavedAddressesDataModel(std::shared_ptr<ViewModel::MainViewModel> viewModel);

        void set_freeze_toggle_callback(FreezeToggleCallback callback);
        void set_value_edit_callback(ValueEditCallback callback);

        void reset_rows(unsigned int newCount);

        void sort_by(unsigned int column, bool ascending) override;
        void clear_sort() override;
        [[nodiscard]] bool is_sort_active() const noexcept override;
        [[nodiscard]] unsigned int get_source_row(unsigned int viewRow) const noexcept override;

        [[nodiscard]] unsigned int GetColumnCount() const override;
        [[nodiscard]] wxString GetColumnType(unsigned int col) const override;
        void GetValueByRow(wxVariant& variant, unsigned int row, unsigned int col) const override;
        bool GetAttrByRow(unsigned int row, unsigned int col, wxDataViewItemAttr& attr) const override;
        bool SetValueByRow(const wxVariant& variant, unsigned int row, unsigned int col) override;

    private:
        void rebuild_sort_map();

        std::shared_ptr<ViewModel::MainViewModel> m_viewModel;

        FreezeToggleCallback m_freezeToggleCallback{};
        ValueEditCallback m_valueEditCallback{};

        wxColour m_addressColor{0x56, 0x9C, 0xD6};
        wxColour m_typeColor{0xC5, 0x86, 0xC0};
        wxColour m_valueColor{0xB5, 0xCE, 0xA8};
        wxColour m_frozenValueColor{0x4E, 0xC9, 0xB0};

        bool m_sortActive{false};
        unsigned int m_sortColumn{};
        bool m_sortAscending{true};
        std::vector<unsigned int> m_viewToSource{};
    };
}
