//
// Copyright (C) 2026 PHTNC<>.
// Licensed under GPLv3.0 with Plugin Interface exceptions.
//
#include <vertex/customwidgets/addaddressdialog.hh>
#include <vertex/utility.hh>

#include <fmt/format.h>
#include <wx/msgdlg.h>

#include <algorithm>

namespace Vertex::CustomWidgets
{
    AddAddressDialog::AddAddressDialog(
        wxWindow* parent,
        Language::ILanguage& languageService,
        const std::vector<std::string>& valueTypeNames,
        const int defaultTypeIndex,
        AddressExistsCheck addressExistsCheck,
        std::optional<std::reference_wrapper<Log::ILog>> logService
    )
        : wxDialog(parent, wxID_ANY,
                   wxString::FromUTF8(languageService.fetch_translation("mainWindow.dialog.addAddressTitle")),
                   wxDefaultPosition, wxDefaultSize,
                   wxDEFAULT_DIALOG_STYLE | wxRESIZE_BORDER)
        , m_languageService(languageService)
        , m_addressExistsCheck(std::move(addressExistsCheck))
        , m_logService(std::move(logService))
    {
        create_controls(valueTypeNames, defaultTypeIndex);

        SetMinSize(FromDIP(wxSize(AddAddressDialogValues::DIALOG_WIDTH, AddAddressDialogValues::DIALOG_HEIGHT)));
        Fit();
        CenterOnParent();

        m_addressInput->SetFocus();
    }

    void AddAddressDialog::create_controls(
        const std::vector<std::string>& valueTypeNames,
        const int defaultTypeIndex
    )
    {
        auto* mainSizer = new wxBoxSizer(wxVERTICAL);

        auto* addressLabel = new wxStaticText(this, wxID_ANY,
            wxString::FromUTF8(m_languageService.fetch_translation("mainWindow.dialog.addressLabel")));
        mainSizer->Add(addressLabel, StandardWidgetValues::NO_PROPORTION, wxALL, StandardWidgetValues::BORDER_TWICE);

        m_addressInput = new wxTextCtrl(this, wxID_ANY, wxEmptyString,
                                         wxDefaultPosition, wxDefaultSize,
                                         wxTE_PROCESS_ENTER);
        mainSizer->Add(m_addressInput, StandardWidgetValues::NO_PROPORTION, wxEXPAND | wxLEFT | wxRIGHT, StandardWidgetValues::BORDER_TWICE);

        m_statusLabel = new wxStaticText(this, wxID_ANY, wxEmptyString);
        m_statusLabel->SetForegroundColour(*wxRED);
        m_statusLabel->Hide();
        mainSizer->Add(m_statusLabel, StandardWidgetValues::NO_PROPORTION, wxLEFT | wxRIGHT | wxTOP, StandardWidgetValues::BORDER_TWICE);

        auto* typeLabel = new wxStaticText(this, wxID_ANY,
            wxString::FromUTF8(m_languageService.fetch_translation("mainWindow.dialog.addAddressTypeLabel")));
        mainSizer->Add(typeLabel, StandardWidgetValues::NO_PROPORTION, wxLEFT | wxRIGHT | wxTOP, StandardWidgetValues::BORDER_TWICE);

        wxArrayString typeChoices{};
        for (const auto& name : valueTypeNames)
        {
            typeChoices.Add(wxString::FromUTF8(name));
        }

        m_valueTypeCombo = new wxComboBox(this, wxID_ANY, wxEmptyString,
                                           wxDefaultPosition, wxDefaultSize,
                                           typeChoices, wxCB_READONLY);

        if (defaultTypeIndex >= 0 && defaultTypeIndex < static_cast<int>(valueTypeNames.size()))
        {
            m_valueTypeCombo->SetSelection(defaultTypeIndex);
        }
        else
        {
            m_valueTypeCombo->SetSelection(0);
        }

        mainSizer->Add(m_valueTypeCombo, StandardWidgetValues::NO_PROPORTION, wxEXPAND | wxLEFT | wxRIGHT, StandardWidgetValues::BORDER_TWICE);

        auto* buttonSizer = new wxBoxSizer(wxHORIZONTAL);
        m_okButton = new wxButton(this, wxID_OK, "OK");
        m_cancelButton = new wxButton(this, wxID_CANCEL, "Cancel");

        buttonSizer->AddStretchSpacer();
        buttonSizer->Add(m_okButton, StandardWidgetValues::NO_PROPORTION, wxRIGHT, StandardWidgetValues::STANDARD_BORDER);
        buttonSizer->Add(m_cancelButton, StandardWidgetValues::NO_PROPORTION);

        mainSizer->AddSpacer(StandardWidgetValues::BORDER_TWICE);
        mainSizer->Add(buttonSizer, StandardWidgetValues::NO_PROPORTION, wxEXPAND | wxALL, StandardWidgetValues::BORDER_TWICE);

        SetSizer(mainSizer);

        m_okButton->Bind(wxEVT_BUTTON, &AddAddressDialog::on_ok, this);
        m_cancelButton->Bind(wxEVT_BUTTON, &AddAddressDialog::on_cancel, this);
        m_addressInput->Bind(wxEVT_TEXT_ENTER, &AddAddressDialog::on_text_enter, this);
        m_addressInput->Bind(wxEVT_TEXT, &AddAddressDialog::on_address_changed, this);
    }

    void AddAddressDialog::on_address_changed([[maybe_unused]] wxCommandEvent& event)
    {
        update_validation_state();
    }

    bool AddAddressDialog::is_valid_hex_string(const std::string_view input)
    {
        auto hexPart = input;
        if (hexPart.starts_with("0x") || hexPart.starts_with("0X"))
        {
            hexPart = hexPart.substr(2);
        }

        if (hexPart.empty())
        {
            return false;
        }

        return std::ranges::all_of(hexPart, [](const char c)
        {
            return (c >= '0' && c <= '9') ||
                   (c >= 'a' && c <= 'f') ||
                   (c >= 'A' && c <= 'F');
        });
    }

    void AddAddressDialog::update_validation_state()
    {
        const auto addressStr = m_addressInput->GetValue().ToStdString();
        if (addressStr.empty())
        {
            m_statusLabel->Hide();
            m_okButton->Enable();
            GetSizer()->Layout();
            return;
        }

        if (!is_valid_hex_string(addressStr))
        {
            m_statusLabel->SetLabel(
                wxString::FromUTF8(m_languageService.fetch_translation("mainWindow.dialog.addAddressInvalidError")));
            m_statusLabel->Show();
            m_okButton->Disable();

            if (m_logService.has_value())
            {
                m_logService->get().log_warn(fmt::format("[AddAddress] Invalid hex input: '{}'", addressStr));
            }

            GetSizer()->Layout();
            return;
        }

        try
        {
            const auto address = std::stoull(addressStr, nullptr, NumericSystem::HEXADECIMAL);

            if (m_addressExistsCheck && m_addressExistsCheck(address))
            {
                m_statusLabel->SetLabel(
                    wxString::FromUTF8(m_languageService.fetch_translation("mainWindow.dialog.addAddressDuplicateWarning")));
                m_statusLabel->Show();
                m_okButton->Disable();
            }
            else
            {
                m_statusLabel->Hide();
                m_okButton->Enable();
            }
        }
        catch (...)
        {
            m_statusLabel->SetLabel(
                wxString::FromUTF8(m_languageService.fetch_translation("mainWindow.dialog.addAddressInvalidError")));
            m_statusLabel->Show();
            m_okButton->Disable();

            if (m_logService.has_value())
            {
                m_logService->get().log_warn(fmt::format("[AddAddress] Failed to parse hex address: '{}'", addressStr));
            }
        }

        GetSizer()->Layout();
    }

    bool AddAddressDialog::validate_and_parse()
    {
        const auto addressStr = m_addressInput->GetValue().ToStdString();
        if (addressStr.empty())
        {
            wxMessageBox(
                wxString::FromUTF8(m_languageService.fetch_translation("mainWindow.dialog.addAddressEmptyError")),
                wxString::FromUTF8(m_languageService.fetch_translation("general.error")),
                wxOK | wxICON_ERROR, this);
            return false;
        }

        if (!is_valid_hex_string(addressStr))
        {
            if (m_logService.has_value())
            {
                m_logService->get().log_warn(fmt::format("[AddAddress] Rejected invalid hex address on submit: '{}'", addressStr));
            }

            wxMessageBox(
                wxString::FromUTF8(m_languageService.fetch_translation("mainWindow.dialog.addAddressInvalidError")),
                wxString::FromUTF8(m_languageService.fetch_translation("general.error")),
                wxOK | wxICON_ERROR, this);
            return false;
        }

        try
        {
            m_parsedAddress = std::stoull(addressStr, nullptr, NumericSystem::HEXADECIMAL);
        }
        catch (...)
        {
            if (m_logService.has_value())
            {
                m_logService->get().log_warn(fmt::format("[AddAddress] Failed to parse hex address on submit: '{}'", addressStr));
            }

            wxMessageBox(
                wxString::FromUTF8(m_languageService.fetch_translation("mainWindow.dialog.addAddressInvalidError")),
                wxString::FromUTF8(m_languageService.fetch_translation("general.error")),
                wxOK | wxICON_ERROR, this);
            return false;
        }

        return true;
    }

    std::uint64_t AddAddressDialog::get_address() const
    {
        return m_parsedAddress;
    }

    int AddAddressDialog::get_value_type_index() const
    {
        return m_valueTypeCombo->GetSelection();
    }

    void AddAddressDialog::on_ok([[maybe_unused]] wxCommandEvent& event)
    {
        if (validate_and_parse())
        {
            EndModal(wxID_OK);
        }
    }

    void AddAddressDialog::on_cancel([[maybe_unused]] wxCommandEvent& event)
    {
        EndModal(wxID_CANCEL);
    }

    void AddAddressDialog::on_text_enter([[maybe_unused]] wxCommandEvent& event)
    {
        if (validate_and_parse())
        {
            EndModal(wxID_OK);
        }
    }
}
