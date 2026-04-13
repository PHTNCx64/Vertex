//
// Copyright (C) 2026 PHTNC<>.
// Licensed under GPLv3.0 with Plugin Interface exceptions.
//

#include <vertex/scripting/stdlib/ui.hh>

#include <angelscript.h>

#include <wx/frame.h>
#include <wx/dialog.h>
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
    // ---- ScriptFrame ----

    void ScriptFrame::add_ref()
    {
        m_refCount.fetch_add(1, std::memory_order_relaxed);
    }

    void ScriptFrame::release()
    {
        if (m_refCount.fetch_sub(1, std::memory_order_acq_rel) == 1)
        {
            delete this;
        }
    }

    void ScriptFrame::show() const
    {
        if (!m_frame) return;
        dispatch_to_main_sync([this]() { if (m_frame) m_frame->Show(); });
    }

    void ScriptFrame::hide() const
    {
        if (!m_frame) return;
        dispatch_to_main_sync([this]() { if (m_frame) m_frame->Hide(); });
    }

    void ScriptFrame::close() const
    {
        if (!m_frame) return;
        dispatch_to_main_sync([this]() { if (m_frame) m_frame->Close(); });
    }

    void ScriptFrame::set_title(const std::string& title) const
    {
        if (!m_frame) return;
        dispatch_to_main_sync([this, &title]()
        {
            if (m_frame) m_frame->SetTitle(wxString::FromUTF8(title));
        });
    }

    void ScriptFrame::set_size(const int width, const int height) const
    {
        if (!m_frame) return;
        dispatch_to_main_sync([this, width, height]()
        {
            if (m_frame) m_frame->SetClientSize(width, height);
        });
    }

    void ScriptFrame::set_sizer(const ScriptBoxSizer* sizer) const
    {
        if (!m_frame || !sizer || !sizer->m_sizer) return;
        dispatch_to_main_sync([this, sizer]()
        {
            if (m_frame) m_frame->SetSizerAndFit(sizer->m_sizer);
        });
    }

    ScriptPanel* ScriptFrame::create_panel() const
    {
        if (!m_frame) return {};
        auto* wrapper = new ScriptPanel{};
        dispatch_to_main_sync([this, wrapper]()
        {
            if (!m_frame) return;
            wrapper->m_panel = new wxPanel{m_frame};
            wrapper->m_panel->Bind(wxEVT_DESTROY, [wrapper](wxWindowDestroyEvent& evt)
            {
                if (evt.GetEventObject() == wrapper->m_panel)
                {
                    wrapper->m_panel = nullptr;
                }
                evt.Skip();
            });
        });
        return wrapper;
    }

    int ScriptFrame::from_dip(const int value) const
    {
        if (!m_frame) return value;
        int result{value};
        dispatch_to_main_sync([this, &result, value]()
        {
            if (m_frame) result = m_frame->FromDIP(value);
        });
        return result;
    }

    bool ScriptFrame::is_open() const
    {
        return m_frame != nullptr;
    }

    // ---- ScriptDialog ----

    void ScriptDialog::add_ref()
    {
        m_refCount.fetch_add(1, std::memory_order_relaxed);
    }

    void ScriptDialog::release()
    {
        if (m_refCount.fetch_sub(1, std::memory_order_acq_rel) == 1)
        {
            delete this;
        }
    }

    void ScriptDialog::show() const
    {
        if (!m_dialog) return;
        dispatch_to_main_sync([this]() { if (m_dialog) m_dialog->Show(); });
    }

    void ScriptDialog::hide() const
    {
        if (!m_dialog) return;
        dispatch_to_main_sync([this]() { if (m_dialog) m_dialog->Hide(); });
    }

    int ScriptDialog::show_modal() const
    {
        if (!m_dialog) return -1;
        int result{-1};
        dispatch_to_main_sync([this, &result]()
        {
            if (m_dialog) result = m_dialog->ShowModal();
        });
        return result;
    }

    void ScriptDialog::end_modal(const int returnCode) const
    {
        if (!m_dialog) return;
        dispatch_to_main_sync([this, returnCode]()
        {
            if (m_dialog) m_dialog->EndModal(returnCode);
        });
    }

    void ScriptDialog::close() const
    {
        if (!m_dialog) return;
        dispatch_to_main_sync([this]() { if (m_dialog) m_dialog->Close(); });
    }

    void ScriptDialog::set_title(const std::string& title) const
    {
        if (!m_dialog) return;
        dispatch_to_main_sync([this, &title]()
        {
            if (m_dialog) m_dialog->SetTitle(wxString::FromUTF8(title));
        });
    }

    void ScriptDialog::set_size(const int width, const int height) const
    {
        if (!m_dialog) return;
        dispatch_to_main_sync([this, width, height]()
        {
            if (m_dialog) m_dialog->SetClientSize(width, height);
        });
    }

    void ScriptDialog::set_sizer(const ScriptBoxSizer* sizer) const
    {
        if (!m_dialog || !sizer || !sizer->m_sizer) return;
        dispatch_to_main_sync([this, sizer]()
        {
            if (m_dialog) m_dialog->SetSizerAndFit(sizer->m_sizer);
        });
    }

    ScriptPanel* ScriptDialog::create_panel() const
    {
        if (!m_dialog) return {};
        auto* wrapper = new ScriptPanel{};
        dispatch_to_main_sync([this, wrapper]()
        {
            if (!m_dialog) return;
            wrapper->m_panel = new wxPanel{m_dialog};
            wrapper->m_panel->Bind(wxEVT_DESTROY, [wrapper](wxWindowDestroyEvent& evt)
            {
                if (evt.GetEventObject() == wrapper->m_panel)
                {
                    wrapper->m_panel = nullptr;
                }
                evt.Skip();
            });
        });
        return wrapper;
    }

    int ScriptDialog::from_dip(const int value) const
    {
        if (!m_dialog) return value;
        int result{value};
        dispatch_to_main_sync([this, &result, value]()
        {
            if (m_dialog) result = m_dialog->FromDIP(value);
        });
        return result;
    }

    bool ScriptDialog::is_open() const
    {
        return m_dialog != nullptr;
    }

    // ---- ScriptPanel ----

    void ScriptPanel::add_ref()
    {
        m_refCount.fetch_add(1, std::memory_order_relaxed);
    }

    void ScriptPanel::release()
    {
        if (m_refCount.fetch_sub(1, std::memory_order_acq_rel) == 1)
        {
            delete this;
        }
    }

    ScriptButton* ScriptPanel::add_button(const std::string& label) const
    {
        if (!m_panel) return {};
        auto* wrapper = new ScriptButton{};
        dispatch_to_main_sync([this, wrapper, &label]()
        {
            if (!m_panel) return;
            wrapper->m_button = new wxButton{m_panel, wxID_ANY, wxString::FromUTF8(label)};
            wrapper->m_button->Bind(wxEVT_DESTROY, [wrapper](wxWindowDestroyEvent& evt)
            {
                if (evt.GetEventObject() == wrapper->m_button)
                {
                    wrapper->m_button = nullptr;
                }
                evt.Skip();
            });
        });
        return wrapper;
    }

    ScriptTextInput* ScriptPanel::add_text_input(const std::string& value) const
    {
        if (!m_panel) return {};
        auto* wrapper = new ScriptTextInput{};
        dispatch_to_main_sync([this, wrapper, &value]()
        {
            if (!m_panel) return;
            wrapper->m_textCtrl = new wxTextCtrl{m_panel, wxID_ANY, wxString::FromUTF8(value)};
            wrapper->m_textCtrl->Bind(wxEVT_DESTROY, [wrapper](wxWindowDestroyEvent& evt)
            {
                if (evt.GetEventObject() == wrapper->m_textCtrl)
                {
                    wrapper->m_textCtrl = nullptr;
                }
                evt.Skip();
            });
        });
        return wrapper;
    }

    ScriptCheckBox* ScriptPanel::add_checkbox(const std::string& label) const
    {
        if (!m_panel) return {};
        auto* wrapper = new ScriptCheckBox{};
        dispatch_to_main_sync([this, wrapper, &label]()
        {
            if (!m_panel) return;
            wrapper->m_checkBox = new wxCheckBox{m_panel, wxID_ANY, wxString::FromUTF8(label)};
            wrapper->m_checkBox->Bind(wxEVT_DESTROY, [wrapper](wxWindowDestroyEvent& evt)
            {
                if (evt.GetEventObject() == wrapper->m_checkBox)
                {
                    wrapper->m_checkBox = nullptr;
                }
                evt.Skip();
            });
        });
        return wrapper;
    }

    ScriptComboBox* ScriptPanel::add_combobox() const
    {
        if (!m_panel) return {};
        auto* wrapper = new ScriptComboBox{};
        dispatch_to_main_sync([this, wrapper]()
        {
            if (!m_panel) return;
            wrapper->m_comboBox = new wxComboBox{m_panel, wxID_ANY, wxEmptyString,
                wxDefaultPosition, wxDefaultSize, 0, nullptr, wxCB_READONLY};
            wrapper->m_comboBox->Bind(wxEVT_DESTROY, [wrapper](wxWindowDestroyEvent& evt)
            {
                if (evt.GetEventObject() == wrapper->m_comboBox)
                {
                    wrapper->m_comboBox = nullptr;
                }
                evt.Skip();
            });
        });
        return wrapper;
    }

    ScriptLabel* ScriptPanel::add_label(const std::string& text) const
    {
        if (!m_panel) return {};
        auto* wrapper = new ScriptLabel{};
        dispatch_to_main_sync([this, wrapper, &text]()
        {
            if (!m_panel) return;
            wrapper->m_label = new wxStaticText{m_panel, wxID_ANY, wxString::FromUTF8(text)};
            wrapper->m_label->Bind(wxEVT_DESTROY, [wrapper](wxWindowDestroyEvent& evt)
            {
                if (evt.GetEventObject() == wrapper->m_label)
                {
                    wrapper->m_label = nullptr;
                }
                evt.Skip();
            });
        });
        return wrapper;
    }

    ScriptPanel* ScriptPanel::add_panel() const
    {
        if (!m_panel) return {};
        auto* wrapper = new ScriptPanel{};
        dispatch_to_main_sync([this, wrapper]()
        {
            if (!m_panel) return;
            wrapper->m_panel = new wxPanel{m_panel};
            wrapper->m_panel->Bind(wxEVT_DESTROY, [wrapper](wxWindowDestroyEvent& evt)
            {
                if (evt.GetEventObject() == wrapper->m_panel)
                {
                    wrapper->m_panel = nullptr;
                }
                evt.Skip();
            });
        });
        return wrapper;
    }

    ScriptGroupBox* ScriptPanel::create_group_box(const std::string& title, const int orientation) const
    {
        if (!m_panel) return {};
        auto* wrapper = new ScriptGroupBox{};
        dispatch_to_main_sync([this, wrapper, &title, orientation]()
        {
            if (!m_panel) return;
            wrapper->m_staticBox = new wxStaticBox{m_panel, wxID_ANY, wxString::FromUTF8(title)};
            wrapper->m_sizer = new wxStaticBoxSizer{wrapper->m_staticBox, orientation};
            wrapper->m_staticBox->Bind(wxEVT_DESTROY, [wrapper](wxWindowDestroyEvent& evt)
            {
                if (evt.GetEventObject() == wrapper->m_staticBox)
                {
                    wrapper->m_staticBox = nullptr;
                    wrapper->m_sizer = nullptr;
                }
                evt.Skip();
            });
        });
        return wrapper;
    }

    void ScriptPanel::set_sizer(const ScriptBoxSizer* sizer) const
    {
        if (!m_panel || !sizer || !sizer->m_sizer) return;
        dispatch_to_main_sync([this, sizer]()
        {
            if (m_panel) m_panel->SetSizerAndFit(sizer->m_sizer);
        });
    }

    int ScriptPanel::from_dip(const int value) const
    {
        if (!m_panel) return value;
        int result{value};
        dispatch_to_main_sync([this, &result, value]()
        {
            if (m_panel) result = m_panel->FromDIP(value);
        });
        return result;
    }

    // ---- Registration ----

    StatusCode ScriptUI::register_frame_methods(asIScriptEngine& engine)
    {
        VTX_CHECK(engine.RegisterObjectMethod("Frame", "void show()",
            asMETHOD(ScriptFrame, show), asCALL_THISCALL));
        VTX_CHECK(engine.RegisterObjectMethod("Frame", "void hide()",
            asMETHOD(ScriptFrame, hide), asCALL_THISCALL));
        VTX_CHECK(engine.RegisterObjectMethod("Frame", "void close()",
            asMETHOD(ScriptFrame, close), asCALL_THISCALL));
        VTX_CHECK(engine.RegisterObjectMethod("Frame", "void set_title(const string &in)",
            asMETHOD(ScriptFrame, set_title), asCALL_THISCALL));
        VTX_CHECK(engine.RegisterObjectMethod("Frame", "void set_size(int, int)",
            asMETHOD(ScriptFrame, set_size), asCALL_THISCALL));
        VTX_CHECK(engine.RegisterObjectMethod("Frame", "void set_sizer(BoxSizer@)",
            asMETHOD(ScriptFrame, set_sizer), asCALL_THISCALL));
        VTX_CHECK(engine.RegisterObjectMethod("Frame", "Panel@ create_panel()",
            asMETHOD(ScriptFrame, create_panel), asCALL_THISCALL));
        VTX_CHECK(engine.RegisterObjectMethod("Frame", "int from_dip(int)",
            asMETHOD(ScriptFrame, from_dip), asCALL_THISCALL));
        VTX_CHECK(engine.RegisterObjectMethod("Frame", "bool is_open()",
            asMETHOD(ScriptFrame, is_open), asCALL_THISCALL));

        return StatusCode::STATUS_OK;
    }

    StatusCode ScriptUI::register_dialog_methods(asIScriptEngine& engine)
    {
        VTX_CHECK(engine.RegisterObjectMethod("Dialog", "void show()",
            asMETHOD(ScriptDialog, show), asCALL_THISCALL));
        VTX_CHECK(engine.RegisterObjectMethod("Dialog", "void hide()",
            asMETHOD(ScriptDialog, hide), asCALL_THISCALL));
        VTX_CHECK(engine.RegisterObjectMethod("Dialog", "int show_modal()",
            asMETHOD(ScriptDialog, show_modal), asCALL_THISCALL));
        VTX_CHECK(engine.RegisterObjectMethod("Dialog", "void end_modal(int)",
            asMETHOD(ScriptDialog, end_modal), asCALL_THISCALL));
        VTX_CHECK(engine.RegisterObjectMethod("Dialog", "void close()",
            asMETHOD(ScriptDialog, close), asCALL_THISCALL));
        VTX_CHECK(engine.RegisterObjectMethod("Dialog", "void set_title(const string &in)",
            asMETHOD(ScriptDialog, set_title), asCALL_THISCALL));
        VTX_CHECK(engine.RegisterObjectMethod("Dialog", "void set_size(int, int)",
            asMETHOD(ScriptDialog, set_size), asCALL_THISCALL));
        VTX_CHECK(engine.RegisterObjectMethod("Dialog", "void set_sizer(BoxSizer@)",
            asMETHOD(ScriptDialog, set_sizer), asCALL_THISCALL));
        VTX_CHECK(engine.RegisterObjectMethod("Dialog", "Panel@ create_panel()",
            asMETHOD(ScriptDialog, create_panel), asCALL_THISCALL));
        VTX_CHECK(engine.RegisterObjectMethod("Dialog", "int from_dip(int)",
            asMETHOD(ScriptDialog, from_dip), asCALL_THISCALL));
        VTX_CHECK(engine.RegisterObjectMethod("Dialog", "bool is_open()",
            asMETHOD(ScriptDialog, is_open), asCALL_THISCALL));

        return StatusCode::STATUS_OK;
    }

    StatusCode ScriptUI::register_panel_methods(asIScriptEngine& engine)
    {
        VTX_CHECK(engine.RegisterObjectMethod("Panel", "Button@ add_button(const string &in)",
            asMETHOD(ScriptPanel, add_button), asCALL_THISCALL));
        VTX_CHECK(engine.RegisterObjectMethod("Panel", "TextInput@ add_text_input(const string &in)",
            asMETHOD(ScriptPanel, add_text_input), asCALL_THISCALL));
        VTX_CHECK(engine.RegisterObjectMethod("Panel", "CheckBox@ add_checkbox(const string &in)",
            asMETHOD(ScriptPanel, add_checkbox), asCALL_THISCALL));
        VTX_CHECK(engine.RegisterObjectMethod("Panel", "ComboBox@ add_combobox()",
            asMETHOD(ScriptPanel, add_combobox), asCALL_THISCALL));
        VTX_CHECK(engine.RegisterObjectMethod("Panel", "Label@ add_label(const string &in)",
            asMETHOD(ScriptPanel, add_label), asCALL_THISCALL));
        VTX_CHECK(engine.RegisterObjectMethod("Panel", "Panel@ add_panel()",
            asMETHOD(ScriptPanel, add_panel), asCALL_THISCALL));
        VTX_CHECK(engine.RegisterObjectMethod("Panel", "GroupBox@ create_group_box(const string &in, int)",
            asMETHOD(ScriptPanel, create_group_box), asCALL_THISCALL));
        VTX_CHECK(engine.RegisterObjectMethod("Panel", "void set_sizer(BoxSizer@)",
            asMETHOD(ScriptPanel, set_sizer), asCALL_THISCALL));
        VTX_CHECK(engine.RegisterObjectMethod("Panel", "int from_dip(int)",
            asMETHOD(ScriptPanel, from_dip), asCALL_THISCALL));

        return StatusCode::STATUS_OK;
    }
}

#undef VTX_CHECK
