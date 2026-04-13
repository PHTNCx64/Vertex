//
// Copyright (C) 2026 PHTNC<>.
// Licensed under GPLv3.0 with Plugin Interface exceptions.
//
#pragma once

#include <vertex/language/ilanguage.hh>
#include <vertex/scanner/pointerscanner/pointerscannerconfig.hh>

#include <cstdint>

#include <wx/dialog.h>
#include <wx/textctrl.h>
#include <wx/spinctrl.h>
#include <wx/checkbox.h>
#include <wx/button.h>
#include <wx/listctrl.h>
#include <wx/sizer.h>
#include <wx/statbox.h>

namespace Vertex::View
{
    class PointerScanConfigDialog final : public wxDialog
    {
    public:
        PointerScanConfigDialog(
            wxWindow* parent,
            Language::ILanguage& languageService,
            std::uint64_t targetAddress
        );

        [[nodiscard]] Scanner::PointerScanConfig get_config() const;

    private:
        void create_controls();
        void layout_controls();
        void bind_events();
        [[nodiscard]] bool validate_inputs();

        Language::ILanguage& m_languageService;
        std::uint64_t m_targetAddress{};

        void on_add_offset_filter();
        void on_remove_offset_filter() const;

        wxStaticBox* m_scanParamsBox{};
        wxStaticBox* m_rootFilterBox{};
        wxStaticBox* m_offsetFilterBox{};
        wxStaticBox* m_limitsBox{};

        wxStaticBoxSizer* m_scanParamsSizer{};
        wxStaticBoxSizer* m_rootFilterSizer{};
        wxStaticBoxSizer* m_offsetFilterSizer{};
        wxStaticBoxSizer* m_limitsBoxSizer{};

        wxTextCtrl* m_targetAddressInput{};
        wxSpinCtrl* m_maxDepthInput{};
        wxTextCtrl* m_maxOffsetInput{};
        wxSpinCtrl* m_alignmentInput{};
        wxCheckBox* m_allowNegativeOffsetsCheckbox{};
        wxCheckBox* m_staticRootsOnlyCheckbox{};
        wxSpinCtrl* m_maxParentsPerNodeInput{};
        wxTextCtrl* m_maxNodesInput{};
        wxTextCtrl* m_maxEdgesInput{};

        wxTextCtrl* m_offsetFilterInput{};
        wxButton* m_addOffsetFilterButton{};
        wxButton* m_removeOffsetFilterButton{};
        wxListCtrl* m_offsetFilterList{};

        wxButton* m_startButton{};
        wxButton* m_cancelButton{};
    };
}
