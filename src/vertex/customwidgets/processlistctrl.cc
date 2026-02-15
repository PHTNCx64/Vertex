//
// Copyright (C) 2026 PHTNC<>.
// Licensed under GPLv3.0 with Plugin Interface exceptions.
//
#include <vertex/customwidgets/processlistctrl.hh>

#include <fmt/format.h>

namespace Vertex::CustomWidgets
{
    ProcessListControl::ProcessListControl(wxWindow* parent, Language::ILanguage& languageService, const std::shared_ptr<ViewModel::ProcessListViewModel>& viewModel)
        : wxListCtrl(parent, wxID_ANY, wxDefaultPosition, wxDefaultSize, wxLC_REPORT | wxLC_VIRTUAL | wxLC_SINGLE_SEL)
        , m_languageService(languageService)
        , m_viewModel(viewModel)
    {
        InsertColumn(PROCESS_ID_COLUMN, wxString::FromUTF8(m_languageService.fetch_translation("processListView.columns.processId")), wxLIST_FORMAT_LEFT, FromDIP(100));
        InsertColumn(PROCESS_NAME_COLUMN, wxString::FromUTF8(m_languageService.fetch_translation("processListView.columns.processName")), wxLIST_FORMAT_LEFT, FromDIP(100));
        InsertColumn(PROCESS_OWNER_COLUMN, wxString::FromUTF8(m_languageService.fetch_translation("processListView.columns.processOwner")), wxLIST_FORMAT_LEFT, FromDIP(100));
    }

    ProcessListControl::~ProcessListControl() = default;

    void ProcessListControl::refresh_list()
    {
        const int count = m_viewModel->get_processes_count();
        SetItemCount(count);
        Refresh();
    }

    int ProcessListControl::GetItemCount() const
    {
        return m_viewModel->get_processes_count();
    }

    wxString ProcessListControl::OnGetItemText(const long item, const long column) const
    {
        return m_viewModel->get_process_item(item, column);
    }
}
