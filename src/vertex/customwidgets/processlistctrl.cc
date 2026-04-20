//
// Copyright (C) 2026 PHTNC<>.
// Licensed under GPLv3.0 with Plugin Interface exceptions.
//
#include <vertex/customwidgets/processlistctrl.hh>

namespace Vertex::CustomWidgets
{
    ProcessListControl::ProcessListControl(wxWindow* parent, Language::ILanguage& languageService, const std::shared_ptr<ViewModel::ProcessListViewModel>& viewModel)
        : VertexDataViewCtrl(parent, wxID_ANY, wxDV_ROW_LINES | wxDV_SINGLE)
        , m_languageService(languageService)
        , m_viewModel(viewModel)
        , m_dataModel(new ProcessListDataModel(viewModel))
    {
        AssociateModel(m_dataModel.get());

        append_text_column(
            wxString::FromUTF8(m_languageService.fetch_translation("processListView.columns.processId")),
            0, COLUMN_WIDTH_DEFAULT);

        append_text_column(
            wxString::FromUTF8(m_languageService.fetch_translation("processListView.columns.processName")),
            1, COLUMN_WIDTH_DEFAULT);

        append_text_column(
            wxString::FromUTF8(m_languageService.fetch_translation("processListView.columns.processOwner")),
            2, COLUMN_WIDTH_DEFAULT);
    }

    ProcessListControl::~ProcessListControl() = default;

    void ProcessListControl::refresh_list()
    {
        m_dataModel->rebuild();
    }

    std::size_t ProcessListControl::get_selected_node_index() const
    {
        const wxDataViewItem selection = GetSelection();
        if (!selection.IsOk())
        {
            return ViewModel::ProcessListViewModel::INVALID_NODE_INDEX;
        }

        const auto pid = ProcessListDataModel::item_to_pid(selection);
        return m_viewModel->get_node_index_for_pid(pid);
    }
}
