//
// Copyright (C) 2026 PHTNC<>.
// Licensed under GPLv3.0 with Plugin Interface exceptions.
//
#pragma once

#include <wx/dataview.h>

#include <vertex/viewmodel/processlistviewmodel.hh>

namespace Vertex::CustomWidgets
{
    class ProcessListDataModel final : public wxDataViewModel
    {
    public:
        explicit ProcessListDataModel(std::shared_ptr<ViewModel::ProcessListViewModel> viewModel);

        [[nodiscard]] unsigned int GetColumnCount() const override;
        [[nodiscard]] wxString GetColumnType(unsigned int col) const override;
        void GetValue(wxVariant& variant, const wxDataViewItem& item, unsigned int col) const override;
        bool SetValue(const wxVariant& variant, const wxDataViewItem& item, unsigned int col) override;
        [[nodiscard]] wxDataViewItem GetParent(const wxDataViewItem& item) const override;
        [[nodiscard]] bool IsContainer(const wxDataViewItem& item) const override;
        [[nodiscard]] bool HasContainerColumns(const wxDataViewItem& item) const override;
        unsigned int GetChildren(const wxDataViewItem& parent, wxDataViewItemArray& array) const override;

        [[nodiscard]] bool rebuild();

        [[nodiscard]] static std::size_t item_to_node_index(const wxDataViewItem& item);
        [[nodiscard]] static wxDataViewItem node_index_to_item(std::size_t nodeIndex);

    private:
        std::shared_ptr<ViewModel::ProcessListViewModel> m_viewModel;
    };
}
