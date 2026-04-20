//
// Copyright (C) 2026 PHTNC<>.
// Licensed under GPLv3.0 with Plugin Interface exceptions.
//
#pragma once

#include <vector>

#include <wx/colour.h>
#include <wx/dataview.h>

#include <vertex/viewmodel/accesstrackerviewmodel.hh>

namespace Vertex::CustomWidgets
{
    class AccessTrackerDataModel final : public wxDataViewVirtualListModel
    {
    public:
        static constexpr unsigned int COLUMN_COUNT{9};
        static constexpr unsigned int INSTRUCTION_COL{};
        static constexpr unsigned int MODULE_COL{1};
        static constexpr unsigned int FUNCTION_COL{2};
        static constexpr unsigned int MNEMONIC_COL{3};
        static constexpr unsigned int HITS_COL{4};
        static constexpr unsigned int ACCESS_COL{5};
        static constexpr unsigned int SIZE_COL{6};
        static constexpr unsigned int REGISTERS_COL{7};
        static constexpr unsigned int CALLER_COL{8};

        explicit AccessTrackerDataModel(const ViewModel::AccessTrackerViewModel& viewModel);

        void refresh_from_viewmodel();

        [[nodiscard]] unsigned int GetColumnCount() const override;
        [[nodiscard]] wxString GetColumnType(unsigned int col) const override;
        void GetValueByRow(wxVariant& variant, unsigned int row, unsigned int col) const override;
        bool GetAttrByRow(unsigned int row, unsigned int col, wxDataViewItemAttr& attr) const override;
        bool SetValueByRow(const wxVariant& variant, unsigned int row, unsigned int col) override;

    private:
        const ViewModel::AccessTrackerViewModel& m_viewModel;
        std::vector<ViewModel::AccessEntry> m_snapshot{};

        wxColour m_colorInstruction{0x56, 0x9C, 0xD6};
        wxColour m_colorModule{0xDC, 0xDC, 0xAA};
        wxColour m_colorFunction{0x9C, 0xDC, 0xFE};
        wxColour m_colorMnemonic{0xD4, 0xD4, 0xD4};
        wxColour m_colorHits{0xCE, 0x91, 0x78};
        wxColour m_colorAccess{0xC5, 0x86, 0xC0};
        wxColour m_colorSize{0xB5, 0xCE, 0xA8};
        wxColour m_colorRegisters{0xB5, 0xCE, 0xA8};
        wxColour m_colorCaller{0x9C, 0xDC, 0xFE};
        wxColour m_colorPlaceholder{0x6A, 0x6A, 0x6A};
    };
}
