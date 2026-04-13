//
// Copyright (C) 2026 PHTNC<>.
// Licensed under GPLv3.0 with Plugin Interface exceptions.
//
#include <vertex/view/pointerscanconfigdialog.hh>
#include <vertex/utility.hh>

#include <algorithm>
#include <charconv>
#include <cmath>
#include <fmt/format.h>
#include <wx/msgdlg.h>
#include <wx/stattext.h>

namespace Vertex::View
{
    PointerScanConfigDialog::PointerScanConfigDialog(
        wxWindow* parent,
        Language::ILanguage& languageService,
        const std::uint64_t targetAddress
    )
        : wxDialog(parent, wxID_ANY,
                   wxString::FromUTF8(languageService.fetch_translation("pointerScanConfigDialog.title")),
                   wxDefaultPosition,
                   wxSize(FromDIP(PointerScanConfigDialogValues::DIALOG_WIDTH),
                          FromDIP(PointerScanConfigDialogValues::DIALOG_HEIGHT)),
                   wxDEFAULT_DIALOG_STYLE | wxRESIZE_BORDER)
        , m_languageService{languageService}
        , m_targetAddress{targetAddress}
    {
        create_controls();
        layout_controls();
        bind_events();
        CenterOnParent();
    }

    void PointerScanConfigDialog::create_controls()
    {
        m_scanParamsBox = new wxStaticBox(this, wxID_ANY,
            wxString::FromUTF8(m_languageService.fetch_translation("pointerScanConfigDialog.scanParameters")));
        m_rootFilterBox = new wxStaticBox(this, wxID_ANY,
            wxString::FromUTF8(m_languageService.fetch_translation("pointerScanConfigDialog.rootFiltering")));
        m_offsetFilterBox = new wxStaticBox(this, wxID_ANY,
            wxString::FromUTF8(m_languageService.fetch_translation("pointerScanConfigDialog.offsetFiltering")));
        m_limitsBox = new wxStaticBox(this, wxID_ANY,
            wxString::FromUTF8(m_languageService.fetch_translation("pointerScanConfigDialog.limits")));

        m_targetAddressInput = new wxTextCtrl(m_scanParamsBox, wxID_ANY,
            wxString::FromUTF8(fmt::format("{:X}", m_targetAddress)));

        m_maxDepthInput = new wxSpinCtrl(m_scanParamsBox, wxID_ANY, wxEmptyString,
            wxDefaultPosition, wxDefaultSize, wxSP_ARROW_KEYS, 1, 20, 5);

        m_maxOffsetInput = new wxTextCtrl(m_scanParamsBox, wxID_ANY, "1000");

        m_alignmentInput = new wxSpinCtrl(m_scanParamsBox, wxID_ANY, wxEmptyString,
            wxDefaultPosition, wxDefaultSize, wxSP_ARROW_KEYS, 1, 16, 8);

        m_allowNegativeOffsetsCheckbox = new wxCheckBox(m_rootFilterBox, wxID_ANY,
            wxString::FromUTF8(m_languageService.fetch_translation("pointerScanConfigDialog.allowNegativeOffsets")));

        m_staticRootsOnlyCheckbox = new wxCheckBox(m_rootFilterBox, wxID_ANY,
            wxString::FromUTF8(m_languageService.fetch_translation("pointerScanConfigDialog.staticRootsOnly")));
        m_staticRootsOnlyCheckbox->SetValue(true);

        m_offsetFilterInput = new wxTextCtrl(m_offsetFilterBox, wxID_ANY, wxEmptyString,
            wxDefaultPosition, wxDefaultSize, wxTE_PROCESS_ENTER);
        m_addOffsetFilterButton = new wxButton(m_offsetFilterBox, wxID_ANY,
            wxString::FromUTF8(m_languageService.fetch_translation("pointerScanConfigDialog.addOffsetFilter")));
        m_removeOffsetFilterButton = new wxButton(m_offsetFilterBox, wxID_ANY,
            wxString::FromUTF8(m_languageService.fetch_translation("pointerScanConfigDialog.removeOffsetFilter")));
        m_offsetFilterList = new wxListCtrl(m_offsetFilterBox, wxID_ANY,
            wxDefaultPosition,
            wxSize(-1, FromDIP(PointerScanConfigDialogValues::OFFSET_FILTER_LIST_HEIGHT)),
            wxLC_REPORT | wxLC_SINGLE_SEL);
        m_offsetFilterList->AppendColumn(
            wxString::FromUTF8(m_languageService.fetch_translation("pointerScanConfigDialog.offsetEndingColumn")),
            wxLIST_FORMAT_LEFT, -1);

        m_maxParentsPerNodeInput = new wxSpinCtrl(m_limitsBox, wxID_ANY, wxEmptyString,
            wxDefaultPosition, wxDefaultSize, wxSP_ARROW_KEYS, 1, 65536, 4096);

        m_maxNodesInput = new wxTextCtrl(m_limitsBox, wxID_ANY, "20000000");

        m_maxEdgesInput = new wxTextCtrl(m_limitsBox, wxID_ANY, "200000000");

        m_startButton = new wxButton(this, wxID_OK,
            wxString::FromUTF8(m_languageService.fetch_translation("pointerScanConfigDialog.startScan")));
        m_cancelButton = new wxButton(this, wxID_CANCEL,
            wxString::FromUTF8(m_languageService.fetch_translation("general.cancel")));
    }

    void PointerScanConfigDialog::layout_controls()
    {
        auto* topSizer = new wxBoxSizer(wxVERTICAL);

        m_scanParamsSizer = new wxStaticBoxSizer(m_scanParamsBox, wxVERTICAL);
        {
            auto* grid = new wxFlexGridSizer(2, StandardWidgetValues::STANDARD_BORDER, StandardWidgetValues::STANDARD_BORDER);
            grid->AddGrowableCol(1, 1);

            grid->Add(new wxStaticText(m_scanParamsBox, wxID_ANY,
                wxString::FromUTF8(m_languageService.fetch_translation("pointerScanConfigDialog.targetAddress"))),
                StandardWidgetValues::NO_PROPORTION, wxALIGN_CENTER_VERTICAL);
            grid->Add(m_targetAddressInput, StandardWidgetValues::STANDARD_PROPORTION, wxEXPAND);

            grid->Add(new wxStaticText(m_scanParamsBox, wxID_ANY,
                wxString::FromUTF8(m_languageService.fetch_translation("pointerScanConfigDialog.maxDepth"))),
                StandardWidgetValues::NO_PROPORTION, wxALIGN_CENTER_VERTICAL);
            grid->Add(m_maxDepthInput, StandardWidgetValues::STANDARD_PROPORTION, wxEXPAND);

            grid->Add(new wxStaticText(m_scanParamsBox, wxID_ANY,
                wxString::FromUTF8(m_languageService.fetch_translation("pointerScanConfigDialog.maxOffset"))),
                StandardWidgetValues::NO_PROPORTION, wxALIGN_CENTER_VERTICAL);
            grid->Add(m_maxOffsetInput, StandardWidgetValues::STANDARD_PROPORTION, wxEXPAND);

            grid->Add(new wxStaticText(m_scanParamsBox, wxID_ANY,
                wxString::FromUTF8(m_languageService.fetch_translation("pointerScanConfigDialog.alignment"))),
                StandardWidgetValues::NO_PROPORTION, wxALIGN_CENTER_VERTICAL);
            grid->Add(m_alignmentInput, StandardWidgetValues::STANDARD_PROPORTION, wxEXPAND);

            m_scanParamsSizer->Add(grid, StandardWidgetValues::STANDARD_PROPORTION, wxEXPAND | wxALL, StandardWidgetValues::STANDARD_BORDER);
        }

        m_rootFilterSizer = new wxStaticBoxSizer(m_rootFilterBox, wxVERTICAL);
        {
            m_rootFilterSizer->Add(m_allowNegativeOffsetsCheckbox, StandardWidgetValues::NO_PROPORTION, wxEXPAND | wxALL, StandardWidgetValues::STANDARD_BORDER);
            m_rootFilterSizer->Add(m_staticRootsOnlyCheckbox, StandardWidgetValues::NO_PROPORTION, wxEXPAND | wxLEFT | wxRIGHT | wxBOTTOM, StandardWidgetValues::STANDARD_BORDER);
        }

        m_offsetFilterSizer = new wxStaticBoxSizer(m_offsetFilterBox, wxVERTICAL);
        {
            auto* inputRow = new wxBoxSizer(wxHORIZONTAL);
            inputRow->Add(m_offsetFilterInput, StandardWidgetValues::STANDARD_PROPORTION, wxEXPAND | wxRIGHT, StandardWidgetValues::STANDARD_BORDER);
            inputRow->Add(m_addOffsetFilterButton, StandardWidgetValues::NO_PROPORTION, wxRIGHT, StandardWidgetValues::STANDARD_BORDER);
            inputRow->Add(m_removeOffsetFilterButton, StandardWidgetValues::NO_PROPORTION);
            m_offsetFilterSizer->Add(inputRow, StandardWidgetValues::NO_PROPORTION, wxEXPAND | wxALL, StandardWidgetValues::STANDARD_BORDER);
            m_offsetFilterSizer->Add(m_offsetFilterList, StandardWidgetValues::STANDARD_PROPORTION, wxEXPAND | wxLEFT | wxRIGHT | wxBOTTOM, StandardWidgetValues::STANDARD_BORDER);
        }

        m_limitsBoxSizer = new wxStaticBoxSizer(m_limitsBox, wxVERTICAL);
        {
            auto* grid = new wxFlexGridSizer(2, StandardWidgetValues::STANDARD_BORDER, StandardWidgetValues::STANDARD_BORDER);
            grid->AddGrowableCol(1, 1);

            grid->Add(new wxStaticText(m_limitsBox, wxID_ANY,
                wxString::FromUTF8(m_languageService.fetch_translation("pointerScanConfigDialog.maxParentsPerNode"))),
                StandardWidgetValues::NO_PROPORTION, wxALIGN_CENTER_VERTICAL);
            grid->Add(m_maxParentsPerNodeInput, StandardWidgetValues::STANDARD_PROPORTION, wxEXPAND);

            grid->Add(new wxStaticText(m_limitsBox, wxID_ANY,
                wxString::FromUTF8(m_languageService.fetch_translation("pointerScanConfigDialog.maxNodes"))),
                StandardWidgetValues::NO_PROPORTION, wxALIGN_CENTER_VERTICAL);
            grid->Add(m_maxNodesInput, StandardWidgetValues::STANDARD_PROPORTION, wxEXPAND);

            grid->Add(new wxStaticText(m_limitsBox, wxID_ANY,
                wxString::FromUTF8(m_languageService.fetch_translation("pointerScanConfigDialog.maxEdges"))),
                StandardWidgetValues::NO_PROPORTION, wxALIGN_CENTER_VERTICAL);
            grid->Add(m_maxEdgesInput, StandardWidgetValues::STANDARD_PROPORTION, wxEXPAND);

            m_limitsBoxSizer->Add(grid, StandardWidgetValues::STANDARD_PROPORTION, wxEXPAND | wxALL, StandardWidgetValues::STANDARD_BORDER);
        }

        topSizer->Add(m_scanParamsSizer, StandardWidgetValues::NO_PROPORTION, wxEXPAND | wxALL, StandardWidgetValues::STANDARD_BORDER);
        topSizer->Add(m_rootFilterSizer, StandardWidgetValues::NO_PROPORTION, wxEXPAND | wxLEFT | wxRIGHT | wxBOTTOM, StandardWidgetValues::STANDARD_BORDER);
        topSizer->Add(m_offsetFilterSizer, StandardWidgetValues::NO_PROPORTION, wxEXPAND | wxLEFT | wxRIGHT | wxBOTTOM, StandardWidgetValues::STANDARD_BORDER);
        topSizer->Add(m_limitsBoxSizer, StandardWidgetValues::NO_PROPORTION, wxEXPAND | wxLEFT | wxRIGHT | wxBOTTOM, StandardWidgetValues::STANDARD_BORDER);

        auto* buttonSizer = new wxBoxSizer(wxHORIZONTAL);
        buttonSizer->AddStretchSpacer();
        buttonSizer->Add(m_startButton, StandardWidgetValues::NO_PROPORTION, wxRIGHT, StandardWidgetValues::STANDARD_BORDER);
        buttonSizer->Add(m_cancelButton, StandardWidgetValues::NO_PROPORTION);
        topSizer->Add(buttonSizer, StandardWidgetValues::NO_PROPORTION, wxEXPAND | wxALL, StandardWidgetValues::STANDARD_BORDER);

        SetSizer(topSizer);
    }

    void PointerScanConfigDialog::bind_events()
    {
        m_startButton->Bind(wxEVT_BUTTON, [this]([[maybe_unused]] wxCommandEvent& event)
        {
            if (!validate_inputs())
            {
                return;
            }
            EndModal(wxID_OK);
        });

        m_cancelButton->Bind(wxEVT_BUTTON, [this]([[maybe_unused]] wxCommandEvent& event)
        {
            EndModal(wxID_CANCEL);
        });

        m_addOffsetFilterButton->Bind(wxEVT_BUTTON, [this]([[maybe_unused]] wxCommandEvent& event)
        {
            on_add_offset_filter();
        });

        m_offsetFilterInput->Bind(wxEVT_TEXT_ENTER, [this]([[maybe_unused]] wxCommandEvent& event)
        {
            on_add_offset_filter();
        });

        m_removeOffsetFilterButton->Bind(wxEVT_BUTTON, [this]([[maybe_unused]] wxCommandEvent& event)
        {
            on_remove_offset_filter();
        });
    }

    bool PointerScanConfigDialog::validate_inputs()
    {
        const auto addressStr = m_targetAddressInput->GetValue().utf8_string();
        std::uint64_t address{};
        const auto [ptr, ec] = std::from_chars(addressStr.data(), addressStr.data() + addressStr.size(), address, 16);
        if (ec != std::errc{} || address == 0)
        {
            wxMessageBox(
                wxString::FromUTF8(m_languageService.fetch_translation("pointerScanConfigDialog.invalidTargetAddress")),
                wxString::FromUTF8(m_languageService.fetch_translation("general.error")),
                wxOK | wxICON_ERROR, this);
            m_targetAddressInput->SetFocus();
            return false;
        }

        const auto offsetStr = m_maxOffsetInput->GetValue().utf8_string();
        std::uint64_t maxOffset{};
        const auto [optr, oec] = std::from_chars(offsetStr.data(), offsetStr.data() + offsetStr.size(), maxOffset, 16);
        if (oec != std::errc{} || maxOffset == 0)
        {
            wxMessageBox(
                wxString::FromUTF8(m_languageService.fetch_translation("pointerScanConfigDialog.invalidMaxOffset")),
                wxString::FromUTF8(m_languageService.fetch_translation("general.error")),
                wxOK | wxICON_ERROR, this);
            m_maxOffsetInput->SetFocus();
            return false;
        }

        return true;
    }

    void PointerScanConfigDialog::on_add_offset_filter()
    {
        const auto text = m_offsetFilterInput->GetValue().utf8_string();
        if (text.empty())
        {
            return;
        }

        std::uint32_t parsed{};
        const auto [ptr, ec] = std::from_chars(text.data(), text.data() + text.size(), parsed, 16);
        if (ec != std::errc{} || ptr != text.data() + text.size())
        {
            wxMessageBox(
                wxString::FromUTF8(m_languageService.fetch_translation("pointerScanConfigDialog.invalidOffsetFilter")),
                wxString::FromUTF8(m_languageService.fetch_translation("general.error")),
                wxOK | wxICON_ERROR, this);
            m_offsetFilterInput->SetFocus();
            return;
        }

        const auto upperHex = fmt::format("{:X}", parsed);

        const auto itemCount = m_offsetFilterList->GetItemCount();
        for (long i{}; i < itemCount; ++i)
        {
            if (m_offsetFilterList->GetItemText(i) == wxString::FromUTF8(upperHex))
            {
                m_offsetFilterInput->Clear();
                return;
            }
        }

        m_offsetFilterList->InsertItem(itemCount, wxString::FromUTF8(upperHex));
        m_offsetFilterInput->Clear();
        m_offsetFilterInput->SetFocus();
    }

    void PointerScanConfigDialog::on_remove_offset_filter() const
    {
        const auto selected = m_offsetFilterList->GetNextItem(-1, wxLIST_NEXT_ALL, wxLIST_STATE_SELECTED);
        if (selected == -1)
        {
            return;
        }
        m_offsetFilterList->DeleteItem(selected);
    }

    Scanner::PointerScanConfig PointerScanConfigDialog::get_config() const
    {
        Scanner::PointerScanConfig config{};

        const auto addressStr = m_targetAddressInput->GetValue().utf8_string();
        std::from_chars(addressStr.data(), addressStr.data() + addressStr.size(), config.targetAddress, 16);

        config.maxDepth = static_cast<std::uint32_t>(m_maxDepthInput->GetValue());

        const auto offsetStr = m_maxOffsetInput->GetValue().utf8_string();
        std::from_chars(offsetStr.data(), offsetStr.data() + offsetStr.size(), config.maxOffset, 16);

        config.alignment = static_cast<std::uint32_t>(m_alignmentInput->GetValue());
        config.allowNegativeOffsets = m_allowNegativeOffsetsCheckbox->GetValue();
        config.staticRootsOnly = m_staticRootsOnlyCheckbox->GetValue();
        config.maxParentsPerNode = static_cast<std::uint32_t>(m_maxParentsPerNodeInput->GetValue());

        const auto nodesStr = m_maxNodesInput->GetValue().utf8_string();
        std::from_chars(nodesStr.data(), nodesStr.data() + nodesStr.size(), config.maxNodes, 10);

        const auto edgesStr = m_maxEdgesInput->GetValue().utf8_string();
        std::from_chars(edgesStr.data(), edgesStr.data() + edgesStr.size(), config.maxEdges, 10);

        const auto filterCount = m_offsetFilterList->GetItemCount();
        for (long i{}; i < filterCount; ++i)
        {
            const auto filterStr = m_offsetFilterList->GetItemText(i).utf8_string();
            std::uint32_t filterValue{};
            std::from_chars(filterStr.data(), filterStr.data() + filterStr.size(), filterValue, 16);

            const auto hexDigits = filterStr.size();
            const auto mask = static_cast<std::int32_t>((1u << (hexDigits * 4)) - 1);

            config.offsetEndingFilters.emplace_back(Scanner::OffsetEndingFilter{
                .value = static_cast<std::int32_t>(filterValue),
                .mask = mask});
        }

        return config;
    }
}
