//
// Copyright (C) 2026 PHTNC<>.
// Licensed under GPLv3.0 with Plugin Interface exceptions.
//
#include <vertex/view/debugger/importexportpanel.hh>
#include <vertex/utility.hh>
#include <fmt/format.h>
#include <ranges>
#include <algorithm>
#include <string_view>

namespace Vertex::View::Debugger
{
    ImportExportPanel::ImportExportPanel(wxWindow* parent, Language::ILanguage& languageService)
        : wxPanel(parent, wxID_ANY),
          m_languageService(languageService)
    {
        create_controls();
        layout_controls();
        bind_events();
    }

    void ImportExportPanel::create_controls()
    {
        m_mainSizer = new wxBoxSizer(wxVERTICAL);

        m_moduleComboBox = new wxComboBox(this, wxID_ANY, wxEmptyString, wxDefaultPosition, wxDefaultSize, 0, nullptr, wxCB_READONLY);

        m_notebook = new wxNotebook(this, wxID_ANY);

        m_importsPanel = new wxPanel(m_notebook);
        m_importsSizer = new wxBoxSizer(wxVERTICAL);
        m_importsList = new wxListCtrl(m_importsPanel, wxID_ANY, wxDefaultPosition, wxDefaultSize, wxLC_REPORT | wxLC_SINGLE_SEL);
        m_importsList->SetFont(wxFont(StandardWidgetValues::TELETYPE_FONT_SIZE, wxFONTFAMILY_TELETYPE, wxFONTSTYLE_NORMAL, wxFONTWEIGHT_NORMAL));
        m_importsList->InsertColumn(0, wxString::FromUTF8(m_languageService.fetch_translation("debugger.importsExports.columnFunction")), wxLIST_FORMAT_LEFT, FromDIP(StandardWidgetValues::COLUMN_WIDTH_FUNCTION));
        m_importsList->InsertColumn(1, wxString::FromUTF8(m_languageService.fetch_translation("debugger.importsExports.columnAddress")), wxLIST_FORMAT_LEFT, FromDIP(StandardWidgetValues::COLUMN_WIDTH_ADDRESS));
        m_importsList->InsertColumn(2, wxString::FromUTF8(m_languageService.fetch_translation("debugger.importsExports.columnModule")), wxLIST_FORMAT_LEFT, FromDIP(StandardWidgetValues::COLUMN_WIDTH_MODULE));
        m_importsSizer->Add(m_importsList, 1, wxEXPAND);
        m_importsPanel->SetSizer(m_importsSizer);
        m_notebook->AddPage(m_importsPanel, wxString::FromUTF8(m_languageService.fetch_translation("debugger.importsExports.tabImports")));

        m_exportsPanel = new wxPanel(m_notebook);
        m_exportsSizer = new wxBoxSizer(wxVERTICAL);
        m_exportsList = new wxListCtrl(m_exportsPanel, wxID_ANY, wxDefaultPosition, wxDefaultSize, wxLC_REPORT | wxLC_SINGLE_SEL);
        m_exportsList->SetFont(wxFont(StandardWidgetValues::TELETYPE_FONT_SIZE, wxFONTFAMILY_TELETYPE, wxFONTSTYLE_NORMAL, wxFONTWEIGHT_NORMAL));
        m_exportsList->InsertColumn(0, wxString::FromUTF8(m_languageService.fetch_translation("debugger.importsExports.columnFunction")), wxLIST_FORMAT_LEFT, FromDIP(StandardWidgetValues::COLUMN_WIDTH_FUNCTION));
        m_exportsList->InsertColumn(1, wxString::FromUTF8(m_languageService.fetch_translation("debugger.importsExports.columnAddress")), wxLIST_FORMAT_LEFT, FromDIP(StandardWidgetValues::COLUMN_WIDTH_ADDRESS));
        m_exportsList->InsertColumn(2, wxString::FromUTF8(m_languageService.fetch_translation("debugger.importsExports.columnOrdinal")), wxLIST_FORMAT_LEFT, FromDIP(StandardWidgetValues::COLUMN_WIDTH_ORDINAL));
        m_exportsSizer->Add(m_exportsList, 1, wxEXPAND);
        m_exportsPanel->SetSizer(m_exportsSizer);
        m_notebook->AddPage(m_exportsPanel, wxString::FromUTF8(m_languageService.fetch_translation("debugger.importsExports.tabExports")));
    }

    void ImportExportPanel::layout_controls()
    {
        m_mainSizer->Add(new wxStaticText(this, wxID_ANY, wxString::FromUTF8(m_languageService.fetch_translation("debugger.importsExports.module"))), 0, wxLEFT | wxTOP, StandardWidgetValues::STANDARD_BORDER);
        m_mainSizer->Add(m_moduleComboBox, 0, wxEXPAND | wxALL, StandardWidgetValues::STANDARD_BORDER);
        m_mainSizer->Add(m_notebook, 1, wxEXPAND | wxLEFT | wxRIGHT | wxBOTTOM, StandardWidgetValues::STANDARD_BORDER);

        SetSizer(m_mainSizer);
    }

    void ImportExportPanel::bind_events()
    {
        m_moduleComboBox->Bind(wxEVT_COMBOBOX, &ImportExportPanel::on_module_selected, this);
        m_importsList->Bind(wxEVT_LIST_ITEM_SELECTED, &ImportExportPanel::on_import_selected, this);
        m_exportsList->Bind(wxEVT_LIST_ITEM_SELECTED, &ImportExportPanel::on_export_selected, this);
        m_importsList->Bind(wxEVT_LIST_ITEM_ACTIVATED, &ImportExportPanel::on_import_activated, this);
        m_exportsList->Bind(wxEVT_LIST_ITEM_ACTIVATED, &ImportExportPanel::on_export_activated, this);
    }

    void ImportExportPanel::update_modules(const std::vector<::Vertex::Debugger::ModuleInfo>& modules)
    {
        m_modules = modules;
        m_moduleComboBox->Clear();

        std::ranges::for_each(modules,
                              [this](const auto& module)
                              {
                                  m_moduleComboBox->Append(module.name);
                              });

        if (!modules.empty())
        {
            m_moduleComboBox->SetSelection(0);
        }
    }

    void ImportExportPanel::update_imports(const std::vector<::Vertex::Debugger::ImportEntry>& imports)
    {
        m_imports = imports;
        m_importsList->DeleteAllItems();

        for (const auto& [i, imp] : imports | std::views::enumerate)
        {
            const long idx = m_importsList->InsertItem(static_cast<long>(i), imp.functionName);
            m_importsList->SetItem(idx, 1, fmt::format("{:X}", imp.address));
            m_importsList->SetItem(idx, 2, imp.moduleName);
        }
    }

    void ImportExportPanel::update_exports(const std::vector<::Vertex::Debugger::ExportEntry>& exports)
    {
        m_exports = exports;
        m_exportsList->DeleteAllItems();

        for (const auto& [i, exp] : exports | std::views::enumerate)
        {
            const long idx = m_exportsList->InsertItem(static_cast<long>(i), exp.functionName);
            m_exportsList->SetItem(idx, 1, fmt::format("{:X}", exp.address));
            m_exportsList->SetItem(idx, 2, std::to_string(exp.ordinal));
        }
    }

    void ImportExportPanel::set_selected_module(const std::string_view moduleName)
    {
        const int idx = m_moduleComboBox->FindString(moduleName.data());
        if (idx != wxNOT_FOUND)
        {
            m_moduleComboBox->SetSelection(idx);
        }
    }

    void ImportExportPanel::set_navigate_callback(NavigateCallback callback) { m_navigateCallback = std::move(callback); }

    void ImportExportPanel::set_select_module_callback(SelectModuleCallback callback) { m_selectModuleCallback = std::move(callback); }

    void ImportExportPanel::clear_selection()
    {
        auto deselect_items = [](wxListCtrl* listCtrl)
        {
            for (long item = listCtrl->GetNextItem(-1, wxLIST_NEXT_ALL, wxLIST_STATE_SELECTED); item != -1; item = listCtrl->GetNextItem(item, wxLIST_NEXT_ALL, wxLIST_STATE_SELECTED))
            {
                listCtrl->SetItemState(item, 0, wxLIST_STATE_SELECTED);
            }
        };

        deselect_items(m_importsList);
        deselect_items(m_exportsList);
    }

    void ImportExportPanel::clear()
    {
        m_moduleComboBox->Clear();
        m_importsList->DeleteAllItems();
        m_exportsList->DeleteAllItems();
        m_modules.clear();
        m_imports.clear();
        m_exports.clear();
    }

    void ImportExportPanel::on_module_selected([[maybe_unused]] wxCommandEvent& event)
    {
        const int idx = m_moduleComboBox->GetSelection();
        if (idx != wxNOT_FOUND && m_selectModuleCallback)
        {
            m_selectModuleCallback(m_moduleComboBox->GetString(idx).ToStdString());
        }
    }

    void ImportExportPanel::on_import_selected([[maybe_unused]] wxListEvent& event) {}

    void ImportExportPanel::on_export_selected([[maybe_unused]] wxListEvent& event) {}

    void ImportExportPanel::on_import_activated(const wxListEvent& event)
    {
        const long idx = event.GetIndex();
        if (idx >= 0 && static_cast<std::size_t>(idx) < m_imports.size() && m_navigateCallback)
        {
            m_navigateCallback(m_imports[idx].address);
        }
    }

    void ImportExportPanel::on_export_activated(const wxListEvent& event)
    {
        const long idx = event.GetIndex();
        if (idx >= 0 && static_cast<std::size_t>(idx) < m_exports.size() && m_navigateCallback)
        {
            m_navigateCallback(m_exports[idx].address);
        }
    }

} // namespace Vertex::View::Debugger
