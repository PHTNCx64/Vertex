//
// Copyright (C) 2026 PHTNC<>.
// Licensed under GPLv3.0 with Plugin Interface exceptions.
//
#pragma once

#include <wx/dialog.h>
#include <wx/textctrl.h>
#include <wx/combobox.h>
#include <wx/button.h>
#include <wx/sizer.h>
#include <wx/stattext.h>

#include <string>
#include <vector>
#include <cstdint>
#include <functional>
#include <optional>

#include <vertex/language/language.hh>
#include <vertex/log/ilog.hh>

namespace Vertex::CustomWidgets
{
    class AddAddressDialog final : public wxDialog
    {
    public:
        using AddressExistsCheck = std::function<bool(std::uint64_t)>;

        explicit AddAddressDialog(
            wxWindow* parent,
            Language::ILanguage& languageService,
            const std::vector<std::string>& valueTypeNames,
            int defaultTypeIndex,
            AddressExistsCheck addressExistsCheck,
            std::optional<std::reference_wrapper<Log::ILog>> logService = std::nullopt
        );

        [[nodiscard]] std::uint64_t get_address() const;
        [[nodiscard]] int get_value_type_index() const;

    private:
        void create_controls(
            const std::vector<std::string>& valueTypeNames,
            int defaultTypeIndex
        );
        void on_ok(wxCommandEvent& event);
        void on_cancel(wxCommandEvent& event);
        void on_text_enter(wxCommandEvent& event);
        void on_address_changed(wxCommandEvent& event);
        void update_validation_state();
        [[nodiscard]] bool validate_and_parse();
        [[nodiscard]] static bool is_valid_hex_string(std::string_view input);

        wxTextCtrl* m_addressInput{};
        wxComboBox* m_valueTypeCombo{};
        wxStaticText* m_statusLabel{};
        wxButton* m_okButton{};
        wxButton* m_cancelButton{};

        Language::ILanguage& m_languageService;
        AddressExistsCheck m_addressExistsCheck;
        std::optional<std::reference_wrapper<Log::ILog>> m_logService {};
        std::uint64_t m_parsedAddress{};
    };
}
