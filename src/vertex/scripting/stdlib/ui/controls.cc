//
// Copyright (C) 2026 PHTNC<>.
// Licensed under GPLv3.0 with Plugin Interface exceptions.
//

#include <vertex/scripting/stdlib/ui.hh>

#include <angelscript.h>

#include <wx/button.h>
#include <wx/textctrl.h>
#include <wx/checkbox.h>
#include <wx/combobox.h>
#include <wx/stattext.h>

#define VTX_CHECK(expr) \
    if ((expr) < 0) return StatusCode::STATUS_ERROR_SCRIPT_ENGINE_INIT_FAILED

namespace Vertex::Scripting::Stdlib
{
    // ---- ScriptButton ----

    void ScriptButton::add_ref() { m_refCount.fetch_add(1, std::memory_order_relaxed); }

    void ScriptButton::release()
    {
        if (m_refCount.fetch_sub(1, std::memory_order_acq_rel) == 1)
        {
            delete this;
        }
    }

    void ScriptButton::set_label(const std::string& label) const
    {
        if (!m_button) return;
        dispatch_to_main_sync([this, &label]()
        {
            if (m_button) m_button->SetLabel(wxString::FromUTF8(label));
        });
    }

    std::string ScriptButton::get_label() const
    {
        if (!m_button) return {};
        std::string result;
        dispatch_to_main_sync([this, &result]()
        {
            if (m_button) result = m_button->GetLabel().ToStdString();
        });
        return result;
    }

    void ScriptButton::enable(bool enabled) const
    {
        if (!m_button) return;
        dispatch_to_main_sync([this, enabled]()
        {
            if (m_button) m_button->Enable(enabled);
        });
    }

    bool ScriptButton::is_enabled() const
    {
        if (!m_button) return false;
        bool result{};
        dispatch_to_main_sync([this, &result]()
        {
            if (m_button) result = m_button->IsEnabled();
        });
        return result;
    }

    // ---- ScriptTextInput ----

    void ScriptTextInput::add_ref() { m_refCount.fetch_add(1, std::memory_order_relaxed); }

    void ScriptTextInput::release()
    {
        if (m_refCount.fetch_sub(1, std::memory_order_acq_rel) == 1)
        {
            delete this;
        }
    }

    void ScriptTextInput::set_value(const std::string& value)
    {
        if (!m_textCtrl) return;
        dispatch_to_main_sync([this, &value]()
        {
            if (m_textCtrl) m_textCtrl->SetValue(wxString::FromUTF8(value));
        });
    }

    std::string ScriptTextInput::get_value() const
    {
        if (!m_textCtrl) return {};
        std::string result;
        dispatch_to_main_sync([this, &result]()
        {
            if (m_textCtrl) result = m_textCtrl->GetValue().ToStdString();
        });
        return result;
    }

    void ScriptTextInput::set_read_only(bool readOnly) const
    {
        if (!m_textCtrl) return;
        dispatch_to_main_sync([this, readOnly]()
        {
            if (m_textCtrl) m_textCtrl->SetEditable(!readOnly);
        });
    }

    void ScriptTextInput::enable(bool enabled) const
    {
        if (!m_textCtrl) return;
        dispatch_to_main_sync([this, enabled]()
        {
            if (m_textCtrl) m_textCtrl->Enable(enabled);
        });
    }

    bool ScriptTextInput::is_enabled() const
    {
        if (!m_textCtrl) return false;
        bool result{};
        dispatch_to_main_sync([this, &result]()
        {
            if (m_textCtrl) result = m_textCtrl->IsEnabled();
        });
        return result;
    }

    // ---- ScriptCheckBox ----

    void ScriptCheckBox::add_ref() { m_refCount.fetch_add(1, std::memory_order_relaxed); }

    void ScriptCheckBox::release()
    {
        if (m_refCount.fetch_sub(1, std::memory_order_acq_rel) == 1)
        {
            delete this;
        }
    }

    void ScriptCheckBox::set_checked(bool checked) const
    {
        if (!m_checkBox) return;
        dispatch_to_main_sync([this, checked]()
        {
            if (m_checkBox) m_checkBox->SetValue(checked);
        });
    }

    bool ScriptCheckBox::is_checked() const
    {
        if (!m_checkBox) return false;
        bool result{};
        dispatch_to_main_sync([this, &result]()
        {
            if (m_checkBox) result = m_checkBox->GetValue();
        });
        return result;
    }

    void ScriptCheckBox::set_label(const std::string& label) const
    {
        if (!m_checkBox) return;
        dispatch_to_main_sync([this, &label]()
        {
            if (m_checkBox) m_checkBox->SetLabel(wxString::FromUTF8(label));
        });
    }

    std::string ScriptCheckBox::get_label() const
    {
        if (!m_checkBox) return {};
        std::string result;
        dispatch_to_main_sync([this, &result]()
        {
            if (m_checkBox) result = m_checkBox->GetLabel().ToStdString();
        });
        return result;
    }

    void ScriptCheckBox::enable(bool enabled) const
    {
        if (!m_checkBox) return;
        dispatch_to_main_sync([this, enabled]()
        {
            if (m_checkBox) m_checkBox->Enable(enabled);
        });
    }

    bool ScriptCheckBox::is_enabled() const
    {
        if (!m_checkBox) return false;
        bool result{};
        dispatch_to_main_sync([this, &result]()
        {
            if (m_checkBox) result = m_checkBox->IsEnabled();
        });
        return result;
    }

    // ---- ScriptComboBox ----

    void ScriptComboBox::add_ref() { m_refCount.fetch_add(1, std::memory_order_relaxed); }

    void ScriptComboBox::release()
    {
        if (m_refCount.fetch_sub(1, std::memory_order_acq_rel) == 1)
        {
            delete this;
        }
    }

    void ScriptComboBox::add_item(const std::string& item) const
    {
        if (!m_comboBox) return;
        dispatch_to_main_sync([this, &item]()
        {
            if (m_comboBox) m_comboBox->Append(wxString::FromUTF8(item));
        });
    }

    void ScriptComboBox::clear_items() const
    {
        if (!m_comboBox) return;
        dispatch_to_main_sync([this]()
        {
            if (m_comboBox) m_comboBox->Clear();
        });
    }

    int ScriptComboBox::get_selection() const
    {
        if (!m_comboBox) return -1;
        int result{-1};
        dispatch_to_main_sync([this, &result]()
        {
            if (m_comboBox) result = m_comboBox->GetSelection();
        });
        return result;
    }

    void ScriptComboBox::set_selection(int index) const
    {
        if (!m_comboBox) return;
        dispatch_to_main_sync([this, index]()
        {
            if (m_comboBox) m_comboBox->SetSelection(index);
        });
    }

    std::string ScriptComboBox::get_value() const
    {
        if (!m_comboBox) return {};
        std::string result;
        dispatch_to_main_sync([this, &result]()
        {
            if (m_comboBox) result = m_comboBox->GetValue().ToStdString();
        });
        return result;
    }

    void ScriptComboBox::enable(bool enabled) const
    {
        if (!m_comboBox) return;
        dispatch_to_main_sync([this, enabled]()
        {
            if (m_comboBox) m_comboBox->Enable(enabled);
        });
    }

    bool ScriptComboBox::is_enabled() const
    {
        if (!m_comboBox) return false;
        bool result{};
        dispatch_to_main_sync([this, &result]()
        {
            if (m_comboBox) result = m_comboBox->IsEnabled();
        });
        return result;
    }

    // ---- ScriptLabel ----

    void ScriptLabel::add_ref() { m_refCount.fetch_add(1, std::memory_order_relaxed); }

    void ScriptLabel::release()
    {
        if (m_refCount.fetch_sub(1, std::memory_order_acq_rel) == 1)
        {
            delete this;
        }
    }

    void ScriptLabel::set_text(const std::string& text) const
    {
        if (!m_label) return;
        dispatch_to_main_sync([this, &text]()
        {
            if (m_label) m_label->SetLabel(wxString::FromUTF8(text));
        });
    }

    std::string ScriptLabel::get_text() const
    {
        if (!m_label) return {};
        std::string result;
        dispatch_to_main_sync([this, &result]()
        {
            if (m_label) result = m_label->GetLabel().ToStdString();
        });
        return result;
    }

    // ---- Registration ----

    StatusCode ScriptUI::register_button_methods(asIScriptEngine& engine)
    {
        VTX_CHECK(engine.RegisterObjectMethod("Button", "void set_label(const string &in)",
            asMETHOD(ScriptButton, set_label), asCALL_THISCALL));
        VTX_CHECK(engine.RegisterObjectMethod("Button", "string get_label()",
            asMETHOD(ScriptButton, get_label), asCALL_THISCALL));
        VTX_CHECK(engine.RegisterObjectMethod("Button", "void enable(bool)",
            asMETHOD(ScriptButton, enable), asCALL_THISCALL));
        VTX_CHECK(engine.RegisterObjectMethod("Button", "bool is_enabled()",
            asMETHOD(ScriptButton, is_enabled), asCALL_THISCALL));

        return StatusCode::STATUS_OK;
    }

    StatusCode ScriptUI::register_text_input_methods(asIScriptEngine& engine)
    {
        VTX_CHECK(engine.RegisterObjectMethod("TextInput", "void set_value(const string &in)",
            asMETHOD(ScriptTextInput, set_value), asCALL_THISCALL));
        VTX_CHECK(engine.RegisterObjectMethod("TextInput", "string get_value()",
            asMETHOD(ScriptTextInput, get_value), asCALL_THISCALL));
        VTX_CHECK(engine.RegisterObjectMethod("TextInput", "void set_read_only(bool)",
            asMETHOD(ScriptTextInput, set_read_only), asCALL_THISCALL));
        VTX_CHECK(engine.RegisterObjectMethod("TextInput", "void enable(bool)",
            asMETHOD(ScriptTextInput, enable), asCALL_THISCALL));
        VTX_CHECK(engine.RegisterObjectMethod("TextInput", "bool is_enabled()",
            asMETHOD(ScriptTextInput, is_enabled), asCALL_THISCALL));

        return StatusCode::STATUS_OK;
    }

    StatusCode ScriptUI::register_checkbox_methods(asIScriptEngine& engine)
    {
        VTX_CHECK(engine.RegisterObjectMethod("CheckBox", "void set_checked(bool)",
            asMETHOD(ScriptCheckBox, set_checked), asCALL_THISCALL));
        VTX_CHECK(engine.RegisterObjectMethod("CheckBox", "bool is_checked()",
            asMETHOD(ScriptCheckBox, is_checked), asCALL_THISCALL));
        VTX_CHECK(engine.RegisterObjectMethod("CheckBox", "void set_label(const string &in)",
            asMETHOD(ScriptCheckBox, set_label), asCALL_THISCALL));
        VTX_CHECK(engine.RegisterObjectMethod("CheckBox", "string get_label()",
            asMETHOD(ScriptCheckBox, get_label), asCALL_THISCALL));
        VTX_CHECK(engine.RegisterObjectMethod("CheckBox", "void enable(bool)",
            asMETHOD(ScriptCheckBox, enable), asCALL_THISCALL));
        VTX_CHECK(engine.RegisterObjectMethod("CheckBox", "bool is_enabled()",
            asMETHOD(ScriptCheckBox, is_enabled), asCALL_THISCALL));

        return StatusCode::STATUS_OK;
    }

    StatusCode ScriptUI::register_combobox_methods(asIScriptEngine& engine)
    {
        VTX_CHECK(engine.RegisterObjectMethod("ComboBox", "void add_item(const string &in)",
            asMETHOD(ScriptComboBox, add_item), asCALL_THISCALL));
        VTX_CHECK(engine.RegisterObjectMethod("ComboBox", "void clear_items()",
            asMETHOD(ScriptComboBox, clear_items), asCALL_THISCALL));
        VTX_CHECK(engine.RegisterObjectMethod("ComboBox", "int get_selection()",
            asMETHOD(ScriptComboBox, get_selection), asCALL_THISCALL));
        VTX_CHECK(engine.RegisterObjectMethod("ComboBox", "void set_selection(int)",
            asMETHOD(ScriptComboBox, set_selection), asCALL_THISCALL));
        VTX_CHECK(engine.RegisterObjectMethod("ComboBox", "string get_value()",
            asMETHOD(ScriptComboBox, get_value), asCALL_THISCALL));
        VTX_CHECK(engine.RegisterObjectMethod("ComboBox", "void enable(bool)",
            asMETHOD(ScriptComboBox, enable), asCALL_THISCALL));
        VTX_CHECK(engine.RegisterObjectMethod("ComboBox", "bool is_enabled()",
            asMETHOD(ScriptComboBox, is_enabled), asCALL_THISCALL));

        return StatusCode::STATUS_OK;
    }

    StatusCode ScriptUI::register_label_methods(asIScriptEngine& engine)
    {
        VTX_CHECK(engine.RegisterObjectMethod("Label", "void set_text(const string &in)",
            asMETHOD(ScriptLabel, set_text), asCALL_THISCALL));
        VTX_CHECK(engine.RegisterObjectMethod("Label", "string get_text()",
            asMETHOD(ScriptLabel, get_text), asCALL_THISCALL));

        return StatusCode::STATUS_OK;
    }
}

#undef VTX_CHECK
