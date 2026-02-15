//
// Copyright (C) 2026 PHTNC<>.
// Licensed under GPLv3.0 with Plugin Interface exceptions.
//
#include <vertex/customwidgets/savedaddressespanel.hh>

namespace Vertex::CustomWidgets
{
    SavedAddressesPanel::SavedAddressesPanel(
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

    void SavedAddressesPanel::create_layout()
    {
        m_sizer = new wxBoxSizer(wxVERTICAL);

        m_header = new SavedAddressesHeader(this, m_languageService);
        m_control = new SavedAddressesControl(this, m_languageService, m_viewModel, m_header);

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

    void SavedAddressesPanel::refresh_list() const
    {
        if (m_control)
        {
            m_control->refresh_list();
        }
    }

    void SavedAddressesPanel::clear_list() const
    {
        if (m_control)
        {
            m_control->clear_list();
        }
    }

    void SavedAddressesPanel::start_auto_refresh() const
    {
        if (m_control)
        {
            m_control->start_auto_refresh();
        }
    }

    void SavedAddressesPanel::stop_auto_refresh() const
    {
        if (m_control)
        {
            m_control->stop_auto_refresh();
        }
    }

    void SavedAddressesPanel::set_selection_change_callback(SavedAddressesControl::SelectionChangeCallback callback) const
    {
        if (m_control)
        {
            m_control->set_selection_change_callback(std::move(callback));
        }
    }

    void SavedAddressesPanel::set_freeze_toggle_callback(SavedAddressesControl::FreezeToggleCallback callback) const
    {
        if (m_control)
        {
            m_control->set_freeze_toggle_callback(std::move(callback));
        }
    }

    void SavedAddressesPanel::set_value_edit_callback(SavedAddressesControl::ValueEditCallback callback) const
    {
        if (m_control)
        {
            m_control->set_value_edit_callback(std::move(callback));
        }
    }

    void SavedAddressesPanel::set_delete_callback(SavedAddressesControl::DeleteCallback callback) const
    {
        if (m_control)
        {
            m_control->set_delete_callback(std::move(callback));
        }
    }

    void SavedAddressesPanel::set_pointer_scan_callback(SavedAddressesControl::PointerScanCallback callback) const
    {
        if (m_control)
        {
            m_control->set_pointer_scan_callback(std::move(callback));
        }
    }

    void SavedAddressesPanel::set_view_in_disassembly_callback(SavedAddressesControl::ViewInDisassemblyCallback callback) const
    {
        if (m_control)
        {
            m_control->set_view_in_disassembly_callback(std::move(callback));
        }
    }

    void SavedAddressesPanel::set_find_access_callback(SavedAddressesControl::FindAccessCallback callback) const
    {
        if (m_control)
        {
            m_control->set_find_access_callback(std::move(callback));
        }
    }

    int SavedAddressesPanel::get_selected_index() const
    {
        if (m_control)
        {
            return m_control->get_selected_index();
        }
        return -1;
    }
}
