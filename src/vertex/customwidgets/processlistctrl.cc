//
// Copyright (C) 2026 PHTNC<>.
// Licensed under GPLv3.0 with Plugin Interface exceptions.
//
#include <vertex/customwidgets/processlistctrl.hh>

namespace Vertex::CustomWidgets
{
    ProcessListControl::ProcessListControl(wxWindow* parent, Language::ILanguage& languageService, const std::shared_ptr<ViewModel::ProcessListViewModel>& viewModel)
        : wxDataViewCtrl(parent, wxID_ANY, wxDefaultPosition, wxDefaultSize, wxDV_ROW_LINES | wxDV_SINGLE)
        , m_languageService(languageService)
        , m_viewModel(viewModel)
        , m_dataModel(new ProcessListDataModel(viewModel))
    {
        AssociateModel(m_dataModel.get());

        AppendTextColumn(
            wxString::FromUTF8(m_languageService.fetch_translation("processListView.columns.processId")),
            0, wxDATAVIEW_CELL_INERT, FromDIP(COLUMN_WIDTH_DEFAULT),
            wxALIGN_NOT, wxDATAVIEW_COL_SORTABLE | wxDATAVIEW_COL_RESIZABLE);

        AppendTextColumn(
            wxString::FromUTF8(m_languageService.fetch_translation("processListView.columns.processName")),
            1, wxDATAVIEW_CELL_INERT, FromDIP(COLUMN_WIDTH_DEFAULT),
            wxALIGN_NOT, wxDATAVIEW_COL_SORTABLE | wxDATAVIEW_COL_RESIZABLE);

        AppendTextColumn(
            wxString::FromUTF8(m_languageService.fetch_translation("processListView.columns.processOwner")),
            2, wxDATAVIEW_CELL_INERT, FromDIP(COLUMN_WIDTH_DEFAULT),
            wxALIGN_NOT, wxDATAVIEW_COL_SORTABLE | wxDATAVIEW_COL_RESIZABLE);
    }

    ProcessListControl::~ProcessListControl() = default;

    void ProcessListControl::refresh_list()
    {
        std::unordered_set<std::string> expandedPids{};
        std::string selectedPid{};

        save_ui_state(wxDataViewItem{nullptr}, expandedPids, selectedPid);

        if (m_dataModel->rebuild())
        {
            restore_ui_state(wxDataViewItem{nullptr}, expandedPids, selectedPid);
        }
    }

    void ProcessListControl::save_ui_state(const wxDataViewItem& parent, std::unordered_set<std::string>& expandedPids, std::string& selectedPid)
    {
        wxDataViewItemArray children{};
        m_dataModel->GetChildren(parent, children);

        const auto selection = GetSelection();

        for (const auto& child : children)
        {
            const auto nodeIndex = ProcessListDataModel::item_to_node_index(child);
            const auto pid = m_viewModel->get_node_column_value(nodeIndex, 0);

            if (child == selection)
            {
                selectedPid = pid;
            }

            if (IsExpanded(child))
            {
                expandedPids.insert(pid);
                save_ui_state(child, expandedPids, selectedPid);
            }
        }
    }

    void ProcessListControl::restore_ui_state(const wxDataViewItem& parent, const std::unordered_set<std::string>& expandedPids, const std::string& selectedPid)
    {
        wxDataViewItemArray children{};
        m_dataModel->GetChildren(parent, children);

        for (const auto& child : children)
        {
            const auto nodeIndex = ProcessListDataModel::item_to_node_index(child);
            const auto pid = m_viewModel->get_node_column_value(nodeIndex, 0);

            if (!selectedPid.empty() && pid == selectedPid)
            {
                Select(child);
            }

            if (expandedPids.contains(pid))
            {
                Expand(child);
                restore_ui_state(child, expandedPids, selectedPid);
            }
        }
    }

    std::size_t ProcessListControl::get_selected_node_index() const
    {
        const wxDataViewItem selection = GetSelection();
        if (!selection.IsOk())
        {
            return ViewModel::ProcessListViewModel::INVALID_NODE_INDEX;
        }
        return ProcessListDataModel::item_to_node_index(selection);
    }
}
