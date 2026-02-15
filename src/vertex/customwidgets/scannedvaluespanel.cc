//
// Copyright (C) 2026 PHTNC<>.
// Licensed under GPLv3.0 with Plugin Interface exceptions.
//
#include <vertex/customwidgets/scannedvaluespanel.hh>

namespace Vertex::CustomWidgets
{
    ScannedValuesPanel::ScannedValuesPanel(
        wxWindow* parent,
        Language::ILanguage& languageService,
        const std::shared_ptr<ViewModel::MainViewModel>& viewModel
    )
        : wxPanel(parent, wxID_ANY)
        , m_languageService(languageService)
        , m_viewModel(viewModel)
    {
        create_layout();
    }

    void ScannedValuesPanel::create_layout()
    {
        m_sizer = new wxBoxSizer(wxVERTICAL);

        m_header = new ScannedValuesHeader(this, m_languageService);
        m_control = new ScannedValuesControl(this, m_languageService, m_viewModel, m_header);

        m_header->set_column_resize_callback([this]()
        {
            if (m_control)
            {
                m_control->on_columns_resized();
            }
        });

        m_sizer->Add(m_header, 0, wxEXPAND);
        m_sizer->Add(m_control, 1, wxEXPAND);

        SetSizer(m_sizer);
    }

    void ScannedValuesPanel::refresh_list() const
    {
        if (m_control)
        {
            m_control->refresh_list();
        }
    }

    void ScannedValuesPanel::clear_list() const
    {
        if (m_control)
        {
            m_control->clear_list();
        }
    }

    void ScannedValuesPanel::start_auto_refresh() const
    {
        if (m_control)
        {
            m_control->start_auto_refresh();
        }
    }

    void ScannedValuesPanel::stop_auto_refresh() const
    {
        if (m_control)
        {
            m_control->stop_auto_refresh();
        }
    }

    void ScannedValuesPanel::set_selection_change_callback(ScannedValuesControl::SelectionChangeCallback callback) const
    {
        if (m_control)
        {
            m_control->set_selection_change_callback(std::move(callback));
        }
    }

    void ScannedValuesPanel::set_add_to_table_callback(ScannedValuesControl::AddToTableCallback callback) const
    {
        if (m_control)
        {
            m_control->set_add_to_table_callback(std::move(callback));
        }
    }

    int ScannedValuesPanel::get_selected_index() const
    {
        if (m_control)
        {
            return m_control->get_selected_index();
        }
        return -1;
    }

    std::optional<std::uint64_t> ScannedValuesPanel::get_selected_address() const
    {
        if (m_control)
        {
            return m_control->get_selected_address();
        }
        return std::nullopt;
    }
}
