//
// Copyright (C) 2026 PHTNC<>.
// Licensed under GPLv3.0 with Plugin Interface exceptions.
//
#pragma once

#include <wx/panel.h>
#include <wx/treectrl.h>
#include <wx/listctrl.h>
#include <wx/combobox.h>
#include <wx/sizer.h>
#include <wx/notebook.h>
#include <wx/stattext.h>

#include <functional>
#include <string_view>
#include <vertex/debugger/debuggertypes.hh>
#include <vertex/language/language.hh>

namespace Vertex::View::Debugger
{
    class ImportExportPanel final : public wxPanel
    {
    public:
        using NavigateCallback = std::function<void(std::uint64_t address)>;
        using SelectModuleCallback = std::function<void(std::string_view moduleName)>;

        ImportExportPanel(wxWindow* parent, Language::ILanguage& languageService);

        void update_modules(const std::vector<::Vertex::Debugger::ModuleInfo>& modules);
        void update_imports(const std::vector<::Vertex::Debugger::ImportEntry>& imports);
        void update_exports(const std::vector<::Vertex::Debugger::ExportEntry>& exports);
        void set_selected_module(std::string_view moduleName);
        void clear_selection();
        void clear();

        void set_navigate_callback(NavigateCallback callback);
        void set_select_module_callback(SelectModuleCallback callback);

    private:
        void create_controls();
        void layout_controls();
        void bind_events();

        void on_module_selected(wxCommandEvent& event);
        void on_import_selected(wxListEvent& event);
        void on_export_selected(wxListEvent& event);
        void on_import_activated(const wxListEvent& event);
        void on_export_activated(const wxListEvent& event);

        wxComboBox* m_moduleComboBox{};
        wxNotebook* m_notebook{};
        wxListCtrl* m_importsList{};
        wxListCtrl* m_exportsList{};
        wxBoxSizer* m_mainSizer{};
        wxPanel* m_importsPanel {};
        wxBoxSizer* m_importsSizer {};
        wxPanel* m_exportsPanel {};
        wxBoxSizer* m_exportsSizer {};
        std::vector<Vertex::Debugger::ModuleInfo> m_modules{};
        std::vector<Vertex::Debugger::ImportEntry> m_imports{};
        std::vector<Vertex::Debugger::ExportEntry> m_exports{};
        NavigateCallback m_navigateCallback{};
        SelectModuleCallback m_selectModuleCallback{};

        Language::ILanguage& m_languageService;
    };
}
