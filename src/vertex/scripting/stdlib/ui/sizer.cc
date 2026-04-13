//
// Copyright (C) 2026 PHTNC<>.
// Licensed under GPLv3.0 with Plugin Interface exceptions.
//

#include <vertex/scripting/stdlib/ui.hh>

#include <angelscript.h>

#include <wx/panel.h>
#include <wx/button.h>
#include <wx/textctrl.h>
#include <wx/checkbox.h>
#include <wx/combobox.h>
#include <wx/stattext.h>
#include <wx/statbox.h>
#include <wx/sizer.h>

#define VTX_CHECK(expr) \
    if ((expr) < 0) return StatusCode::STATUS_ERROR_SCRIPT_ENGINE_INIT_FAILED

namespace Vertex::Scripting::Stdlib
{
    // ---- ScriptBoxSizer ----

    void ScriptBoxSizer::add_ref() { m_refCount.fetch_add(1, std::memory_order_relaxed); }

    void ScriptBoxSizer::release()
    {
        if (m_refCount.fetch_sub(1, std::memory_order_acq_rel) == 1)
        {
            delete this;
        }
    }

    void ScriptBoxSizer::add_panel(ScriptPanel* panel, int proportion, int flags, int border)
    {
        if (!m_sizer || !panel || !panel->m_panel) return;
        dispatch_to_main_sync([this, panel, proportion, flags, border]()
        {
            if (m_sizer && panel->m_panel) m_sizer->Add(panel->m_panel, proportion, flags, border);
        });
    }

    void ScriptBoxSizer::add_button(ScriptButton* button, int proportion, int flags, int border)
    {
        if (!m_sizer || !button || !button->m_button) return;
        dispatch_to_main_sync([this, button, proportion, flags, border]()
        {
            if (m_sizer && button->m_button) m_sizer->Add(button->m_button, proportion, flags, border);
        });
    }

    void ScriptBoxSizer::add_text_input(ScriptTextInput* input, int proportion, int flags, int border)
    {
        if (!m_sizer || !input || !input->m_textCtrl) return;
        dispatch_to_main_sync([this, input, proportion, flags, border]()
        {
            if (m_sizer && input->m_textCtrl) m_sizer->Add(input->m_textCtrl, proportion, flags, border);
        });
    }

    void ScriptBoxSizer::add_checkbox(ScriptCheckBox* checkbox, int proportion, int flags, int border)
    {
        if (!m_sizer || !checkbox || !checkbox->m_checkBox) return;
        dispatch_to_main_sync([this, checkbox, proportion, flags, border]()
        {
            if (m_sizer && checkbox->m_checkBox) m_sizer->Add(checkbox->m_checkBox, proportion, flags, border);
        });
    }

    void ScriptBoxSizer::add_combobox(ScriptComboBox* combobox, int proportion, int flags, int border)
    {
        if (!m_sizer || !combobox || !combobox->m_comboBox) return;
        dispatch_to_main_sync([this, combobox, proportion, flags, border]()
        {
            if (m_sizer && combobox->m_comboBox) m_sizer->Add(combobox->m_comboBox, proportion, flags, border);
        });
    }

    void ScriptBoxSizer::add_label(ScriptLabel* label, int proportion, int flags, int border)
    {
        if (!m_sizer || !label || !label->m_label) return;
        dispatch_to_main_sync([this, label, proportion, flags, border]()
        {
            if (m_sizer && label->m_label) m_sizer->Add(label->m_label, proportion, flags, border);
        });
    }

    void ScriptBoxSizer::add_box_sizer(ScriptBoxSizer* other, int proportion, int flags, int border)
    {
        if (!m_sizer || !other || !other->m_sizer) return;
        dispatch_to_main_sync([this, other, proportion, flags, border]()
        {
            if (m_sizer && other->m_sizer) m_sizer->Add(other->m_sizer, proportion, flags, border);
        });
    }

    void ScriptBoxSizer::add_group(ScriptGroupBox* group, int proportion, int flags, int border)
    {
        if (!m_sizer || !group || !group->m_sizer) return;
        dispatch_to_main_sync([this, group, proportion, flags, border]()
        {
            if (m_sizer && group->m_sizer) m_sizer->Add(group->m_sizer, proportion, flags, border);
        });
    }

    void ScriptBoxSizer::add_spacer(int size)
    {
        if (!m_sizer) return;
        dispatch_to_main_sync([this, size]() { if (m_sizer) m_sizer->AddSpacer(size); });
    }

    void ScriptBoxSizer::add_stretch(int proportion)
    {
        if (!m_sizer) return;
        dispatch_to_main_sync([this, proportion]()
        {
            if (m_sizer) m_sizer->AddStretchSpacer(proportion);
        });
    }

    // ---- ScriptGroupBox ----

    void ScriptGroupBox::add_ref() { m_refCount.fetch_add(1, std::memory_order_relaxed); }

    void ScriptGroupBox::release()
    {
        if (m_refCount.fetch_sub(1, std::memory_order_acq_rel) == 1)
        {
            delete this;
        }
    }

    void ScriptGroupBox::add_panel(ScriptPanel* panel, int proportion, int flags, int border)
    {
        if (!m_sizer || !panel || !panel->m_panel) return;
        dispatch_to_main_sync([this, panel, proportion, flags, border]()
        {
            if (m_sizer && panel->m_panel) m_sizer->Add(panel->m_panel, proportion, flags, border);
        });
    }

    void ScriptGroupBox::add_button(ScriptButton* button, int proportion, int flags, int border)
    {
        if (!m_sizer || !button || !button->m_button) return;
        dispatch_to_main_sync([this, button, proportion, flags, border]()
        {
            if (m_sizer && button->m_button) m_sizer->Add(button->m_button, proportion, flags, border);
        });
    }

    void ScriptGroupBox::add_text_input(ScriptTextInput* input, int proportion, int flags, int border)
    {
        if (!m_sizer || !input || !input->m_textCtrl) return;
        dispatch_to_main_sync([this, input, proportion, flags, border]()
        {
            if (m_sizer && input->m_textCtrl) m_sizer->Add(input->m_textCtrl, proportion, flags, border);
        });
    }

    void ScriptGroupBox::add_checkbox(ScriptCheckBox* checkbox, int proportion, int flags, int border)
    {
        if (!m_sizer || !checkbox || !checkbox->m_checkBox) return;
        dispatch_to_main_sync([this, checkbox, proportion, flags, border]()
        {
            if (m_sizer && checkbox->m_checkBox) m_sizer->Add(checkbox->m_checkBox, proportion, flags, border);
        });
    }

    void ScriptGroupBox::add_combobox(ScriptComboBox* combobox, int proportion, int flags, int border)
    {
        if (!m_sizer || !combobox || !combobox->m_comboBox) return;
        dispatch_to_main_sync([this, combobox, proportion, flags, border]()
        {
            if (m_sizer && combobox->m_comboBox) m_sizer->Add(combobox->m_comboBox, proportion, flags, border);
        });
    }

    void ScriptGroupBox::add_label(ScriptLabel* label, int proportion, int flags, int border)
    {
        if (!m_sizer || !label || !label->m_label) return;
        dispatch_to_main_sync([this, label, proportion, flags, border]()
        {
            if (m_sizer && label->m_label) m_sizer->Add(label->m_label, proportion, flags, border);
        });
    }

    void ScriptGroupBox::add_box_sizer(ScriptBoxSizer* sizer, int proportion, int flags, int border)
    {
        if (!m_sizer || !sizer || !sizer->m_sizer) return;
        dispatch_to_main_sync([this, sizer, proportion, flags, border]()
        {
            if (m_sizer && sizer->m_sizer) m_sizer->Add(sizer->m_sizer, proportion, flags, border);
        });
    }

    void ScriptGroupBox::add_spacer(int size)
    {
        if (!m_sizer) return;
        dispatch_to_main_sync([this, size]() { if (m_sizer) m_sizer->AddSpacer(size); });
    }

    void ScriptGroupBox::add_stretch(int proportion)
    {
        if (!m_sizer) return;
        dispatch_to_main_sync([this, proportion]()
        {
            if (m_sizer) m_sizer->AddStretchSpacer(proportion);
        });
    }

    // ---- Registration ----

    StatusCode ScriptUI::register_box_sizer_methods(asIScriptEngine& engine)
    {
        VTX_CHECK(engine.RegisterObjectMethod("BoxSizer", "void add(Panel@, int, int, int)",
            asMETHOD(ScriptBoxSizer, add_panel), asCALL_THISCALL));
        VTX_CHECK(engine.RegisterObjectMethod("BoxSizer", "void add(Button@, int, int, int)",
            asMETHOD(ScriptBoxSizer, add_button), asCALL_THISCALL));
        VTX_CHECK(engine.RegisterObjectMethod("BoxSizer", "void add(TextInput@, int, int, int)",
            asMETHOD(ScriptBoxSizer, add_text_input), asCALL_THISCALL));
        VTX_CHECK(engine.RegisterObjectMethod("BoxSizer", "void add(CheckBox@, int, int, int)",
            asMETHOD(ScriptBoxSizer, add_checkbox), asCALL_THISCALL));
        VTX_CHECK(engine.RegisterObjectMethod("BoxSizer", "void add(ComboBox@, int, int, int)",
            asMETHOD(ScriptBoxSizer, add_combobox), asCALL_THISCALL));
        VTX_CHECK(engine.RegisterObjectMethod("BoxSizer", "void add(Label@, int, int, int)",
            asMETHOD(ScriptBoxSizer, add_label), asCALL_THISCALL));
        VTX_CHECK(engine.RegisterObjectMethod("BoxSizer", "void add(BoxSizer@, int, int, int)",
            asMETHOD(ScriptBoxSizer, add_box_sizer), asCALL_THISCALL));
        VTX_CHECK(engine.RegisterObjectMethod("BoxSizer", "void add(GroupBox@, int, int, int)",
            asMETHOD(ScriptBoxSizer, add_group), asCALL_THISCALL));
        VTX_CHECK(engine.RegisterObjectMethod("BoxSizer", "void add_spacer(int)",
            asMETHOD(ScriptBoxSizer, add_spacer), asCALL_THISCALL));
        VTX_CHECK(engine.RegisterObjectMethod("BoxSizer", "void add_stretch(int)",
            asMETHOD(ScriptBoxSizer, add_stretch), asCALL_THISCALL));

        return StatusCode::STATUS_OK;
    }

    StatusCode ScriptUI::register_group_box_methods(asIScriptEngine& engine)
    {
        VTX_CHECK(engine.RegisterObjectMethod("GroupBox", "void add(Panel@, int, int, int)",
            asMETHOD(ScriptGroupBox, add_panel), asCALL_THISCALL));
        VTX_CHECK(engine.RegisterObjectMethod("GroupBox", "void add(Button@, int, int, int)",
            asMETHOD(ScriptGroupBox, add_button), asCALL_THISCALL));
        VTX_CHECK(engine.RegisterObjectMethod("GroupBox", "void add(TextInput@, int, int, int)",
            asMETHOD(ScriptGroupBox, add_text_input), asCALL_THISCALL));
        VTX_CHECK(engine.RegisterObjectMethod("GroupBox", "void add(CheckBox@, int, int, int)",
            asMETHOD(ScriptGroupBox, add_checkbox), asCALL_THISCALL));
        VTX_CHECK(engine.RegisterObjectMethod("GroupBox", "void add(ComboBox@, int, int, int)",
            asMETHOD(ScriptGroupBox, add_combobox), asCALL_THISCALL));
        VTX_CHECK(engine.RegisterObjectMethod("GroupBox", "void add(Label@, int, int, int)",
            asMETHOD(ScriptGroupBox, add_label), asCALL_THISCALL));
        VTX_CHECK(engine.RegisterObjectMethod("GroupBox", "void add(BoxSizer@, int, int, int)",
            asMETHOD(ScriptGroupBox, add_box_sizer), asCALL_THISCALL));
        VTX_CHECK(engine.RegisterObjectMethod("GroupBox", "void add_spacer(int)",
            asMETHOD(ScriptGroupBox, add_spacer), asCALL_THISCALL));
        VTX_CHECK(engine.RegisterObjectMethod("GroupBox", "void add_stretch(int)",
            asMETHOD(ScriptGroupBox, add_stretch), asCALL_THISCALL));

        return StatusCode::STATUS_OK;
    }
}

#undef VTX_CHECK
