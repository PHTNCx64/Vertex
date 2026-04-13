//
// Copyright (C) 2026 PHTNC<>.
// Licensed under GPLv3.0 with Plugin Interface exceptions.
//
#pragma once

#include <wx/dataview.h>

#include <vertex/viewmodel/processlistviewmodel.hh>
#include <vertex/customwidgets/processlistdatamodel.hh>
#include <vertex/language/language.hh>

namespace Vertex::CustomWidgets
{
    class ProcessListControl final : public wxDataViewCtrl
    {
    public:
        explicit ProcessListControl(wxWindow* parent, Language::ILanguage& languageService, const std::shared_ptr<ViewModel::ProcessListViewModel>& viewModel);
        ~ProcessListControl() override;

        void refresh_list();
        [[nodiscard]] std::size_t get_selected_node_index() const;

    private:
        static constexpr int COLUMN_WIDTH_DEFAULT = 100;

        Language::ILanguage& m_languageService;
        std::shared_ptr<ViewModel::ProcessListViewModel> m_viewModel{};
        wxObjectDataPtr<ProcessListDataModel> m_dataModel;
    };
}
