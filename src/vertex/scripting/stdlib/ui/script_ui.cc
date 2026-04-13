//
// Copyright (C) 2026 PHTNC<>.
// Licensed under GPLv3.0 with Plugin Interface exceptions.
//

#include <vertex/scripting/stdlib/ui.hh>

#include <angelscript.h>

#include <wx/app.h>
#include <wx/frame.h>
#include <wx/dialog.h>
#include <wx/msgdlg.h>
#include <wx/sizer.h>
#include <wx/statbox.h>

#include <future>

#define VTX_CHECK(expr) \
    if ((expr) < 0) return StatusCode::STATUS_ERROR_SCRIPT_ENGINE_INIT_FAILED

namespace
{
    int g_uiVertical{};
    int g_uiHorizontal{};
    int g_uiExpand{};
    int g_uiAll{};
    int g_uiLeft{};
    int g_uiRight{};
    int g_uiTop{};
    int g_uiBottom{};
    int g_uiCenter{};
    int g_uiOk{};
    int g_uiOkCancel{};
    int g_uiYesNo{};
    int g_uiIconInfo{};
    int g_uiIconWarning{};
    int g_uiIconError{};
    int g_uiIconQuestion{};
}

namespace Vertex::Scripting::Stdlib
{
    void dispatch_to_main_sync(const std::function<void()>& func)
    {
        if (wxIsMainThread())
        {
            func();
            return;
        }

        std::promise<void> promise;
        auto future = promise.get_future();
        auto funcCopy = func;
        wxTheApp->CallAfter([funcCopy, &promise]()
        {
            funcCopy();
            promise.set_value();
        });
        future.get();
    }

    StatusCode ScriptUI::register_api(asIScriptEngine& engine)
    {
        if (const auto status = declare_types(engine);
            status != StatusCode::STATUS_OK)
        {
            return status;
        }

        if (const auto status = register_frame_methods(engine);
            status != StatusCode::STATUS_OK)
        {
            return status;
        }

        if (const auto status = register_dialog_methods(engine);
            status != StatusCode::STATUS_OK)
        {
            return status;
        }

        if (const auto status = register_panel_methods(engine);
            status != StatusCode::STATUS_OK)
        {
            return status;
        }

        if (const auto status = register_button_methods(engine);
            status != StatusCode::STATUS_OK)
        {
            return status;
        }

        if (const auto status = register_text_input_methods(engine);
            status != StatusCode::STATUS_OK)
        {
            return status;
        }

        if (const auto status = register_checkbox_methods(engine);
            status != StatusCode::STATUS_OK)
        {
            return status;
        }

        if (const auto status = register_combobox_methods(engine);
            status != StatusCode::STATUS_OK)
        {
            return status;
        }

        if (const auto status = register_label_methods(engine);
            status != StatusCode::STATUS_OK)
        {
            return status;
        }

        if (const auto status = register_box_sizer_methods(engine);
            status != StatusCode::STATUS_OK)
        {
            return status;
        }

        if (const auto status = register_group_box_methods(engine);
            status != StatusCode::STATUS_OK)
        {
            return status;
        }

        if (const auto status = register_constants(engine);
            status != StatusCode::STATUS_OK)
        {
            return status;
        }

        return register_factories(engine);
    }

    StatusCode ScriptUI::declare_types(asIScriptEngine& engine)
    {
        VTX_CHECK(engine.RegisterObjectType("Frame", 0, asOBJ_REF));
        VTX_CHECK(engine.RegisterObjectBehaviour("Frame", asBEHAVE_ADDREF, "void f()",
            asMETHOD(ScriptFrame, add_ref), asCALL_THISCALL));
        VTX_CHECK(engine.RegisterObjectBehaviour("Frame", asBEHAVE_RELEASE, "void f()",
            asMETHOD(ScriptFrame, release), asCALL_THISCALL));

        VTX_CHECK(engine.RegisterObjectType("Dialog", 0, asOBJ_REF));
        VTX_CHECK(engine.RegisterObjectBehaviour("Dialog", asBEHAVE_ADDREF, "void f()",
            asMETHOD(ScriptDialog, add_ref), asCALL_THISCALL));
        VTX_CHECK(engine.RegisterObjectBehaviour("Dialog", asBEHAVE_RELEASE, "void f()",
            asMETHOD(ScriptDialog, release), asCALL_THISCALL));

        VTX_CHECK(engine.RegisterObjectType("Panel", 0, asOBJ_REF));
        VTX_CHECK(engine.RegisterObjectBehaviour("Panel", asBEHAVE_ADDREF, "void f()",
            asMETHOD(ScriptPanel, add_ref), asCALL_THISCALL));
        VTX_CHECK(engine.RegisterObjectBehaviour("Panel", asBEHAVE_RELEASE, "void f()",
            asMETHOD(ScriptPanel, release), asCALL_THISCALL));

        VTX_CHECK(engine.RegisterObjectType("Button", 0, asOBJ_REF));
        VTX_CHECK(engine.RegisterObjectBehaviour("Button", asBEHAVE_ADDREF, "void f()",
            asMETHOD(ScriptButton, add_ref), asCALL_THISCALL));
        VTX_CHECK(engine.RegisterObjectBehaviour("Button", asBEHAVE_RELEASE, "void f()",
            asMETHOD(ScriptButton, release), asCALL_THISCALL));

        VTX_CHECK(engine.RegisterObjectType("TextInput", 0, asOBJ_REF));
        VTX_CHECK(engine.RegisterObjectBehaviour("TextInput", asBEHAVE_ADDREF, "void f()",
            asMETHOD(ScriptTextInput, add_ref), asCALL_THISCALL));
        VTX_CHECK(engine.RegisterObjectBehaviour("TextInput", asBEHAVE_RELEASE, "void f()",
            asMETHOD(ScriptTextInput, release), asCALL_THISCALL));

        VTX_CHECK(engine.RegisterObjectType("CheckBox", 0, asOBJ_REF));
        VTX_CHECK(engine.RegisterObjectBehaviour("CheckBox", asBEHAVE_ADDREF, "void f()",
            asMETHOD(ScriptCheckBox, add_ref), asCALL_THISCALL));
        VTX_CHECK(engine.RegisterObjectBehaviour("CheckBox", asBEHAVE_RELEASE, "void f()",
            asMETHOD(ScriptCheckBox, release), asCALL_THISCALL));

        VTX_CHECK(engine.RegisterObjectType("ComboBox", 0, asOBJ_REF));
        VTX_CHECK(engine.RegisterObjectBehaviour("ComboBox", asBEHAVE_ADDREF, "void f()",
            asMETHOD(ScriptComboBox, add_ref), asCALL_THISCALL));
        VTX_CHECK(engine.RegisterObjectBehaviour("ComboBox", asBEHAVE_RELEASE, "void f()",
            asMETHOD(ScriptComboBox, release), asCALL_THISCALL));

        VTX_CHECK(engine.RegisterObjectType("Label", 0, asOBJ_REF));
        VTX_CHECK(engine.RegisterObjectBehaviour("Label", asBEHAVE_ADDREF, "void f()",
            asMETHOD(ScriptLabel, add_ref), asCALL_THISCALL));
        VTX_CHECK(engine.RegisterObjectBehaviour("Label", asBEHAVE_RELEASE, "void f()",
            asMETHOD(ScriptLabel, release), asCALL_THISCALL));

        VTX_CHECK(engine.RegisterObjectType("BoxSizer", 0, asOBJ_REF));
        VTX_CHECK(engine.RegisterObjectBehaviour("BoxSizer", asBEHAVE_ADDREF, "void f()",
            asMETHOD(ScriptBoxSizer, add_ref), asCALL_THISCALL));
        VTX_CHECK(engine.RegisterObjectBehaviour("BoxSizer", asBEHAVE_RELEASE, "void f()",
            asMETHOD(ScriptBoxSizer, release), asCALL_THISCALL));

        VTX_CHECK(engine.RegisterObjectType("GroupBox", 0, asOBJ_REF));
        VTX_CHECK(engine.RegisterObjectBehaviour("GroupBox", asBEHAVE_ADDREF, "void f()",
            asMETHOD(ScriptGroupBox, add_ref), asCALL_THISCALL));
        VTX_CHECK(engine.RegisterObjectBehaviour("GroupBox", asBEHAVE_RELEASE, "void f()",
            asMETHOD(ScriptGroupBox, release), asCALL_THISCALL));

        return StatusCode::STATUS_OK;
    }

    StatusCode ScriptUI::register_constants(asIScriptEngine& engine)
    {
        g_uiVertical = wxVERTICAL;
        g_uiHorizontal = wxHORIZONTAL;
        g_uiExpand = wxEXPAND;
        g_uiAll = wxALL;
        g_uiLeft = wxLEFT;
        g_uiRight = wxRIGHT;
        g_uiTop = wxTOP;
        g_uiBottom = wxBOTTOM;
        g_uiCenter = wxCENTER;
        g_uiOk = wxOK;
        g_uiOkCancel = wxOK | wxCANCEL;
        g_uiYesNo = wxYES_NO;
        g_uiIconInfo = wxICON_INFORMATION;
        g_uiIconWarning = wxICON_WARNING;
        g_uiIconError = wxICON_ERROR;
        g_uiIconQuestion = wxICON_QUESTION;

        VTX_CHECK(engine.RegisterGlobalProperty("const int UI_VERTICAL", &g_uiVertical));
        VTX_CHECK(engine.RegisterGlobalProperty("const int UI_HORIZONTAL", &g_uiHorizontal));
        VTX_CHECK(engine.RegisterGlobalProperty("const int UI_EXPAND", &g_uiExpand));
        VTX_CHECK(engine.RegisterGlobalProperty("const int UI_ALL", &g_uiAll));
        VTX_CHECK(engine.RegisterGlobalProperty("const int UI_LEFT", &g_uiLeft));
        VTX_CHECK(engine.RegisterGlobalProperty("const int UI_RIGHT", &g_uiRight));
        VTX_CHECK(engine.RegisterGlobalProperty("const int UI_TOP", &g_uiTop));
        VTX_CHECK(engine.RegisterGlobalProperty("const int UI_BOTTOM", &g_uiBottom));
        VTX_CHECK(engine.RegisterGlobalProperty("const int UI_CENTER", &g_uiCenter));
        VTX_CHECK(engine.RegisterGlobalProperty("const int UI_OK", &g_uiOk));
        VTX_CHECK(engine.RegisterGlobalProperty("const int UI_OK_CANCEL", &g_uiOkCancel));
        VTX_CHECK(engine.RegisterGlobalProperty("const int UI_YES_NO", &g_uiYesNo));
        VTX_CHECK(engine.RegisterGlobalProperty("const int UI_ICON_INFO", &g_uiIconInfo));
        VTX_CHECK(engine.RegisterGlobalProperty("const int UI_ICON_WARNING", &g_uiIconWarning));
        VTX_CHECK(engine.RegisterGlobalProperty("const int UI_ICON_ERROR", &g_uiIconError));
        VTX_CHECK(engine.RegisterGlobalProperty("const int UI_ICON_QUESTION", &g_uiIconQuestion));

        return StatusCode::STATUS_OK;
    }

    StatusCode ScriptUI::register_factories(asIScriptEngine& engine)
    {
        VTX_CHECK(engine.RegisterGlobalFunction(
            "Frame@ ui_create_frame(const string &in, int, int)",
            asMETHOD(ScriptUI, create_frame), asCALL_THISCALL_ASGLOBAL, this));

        VTX_CHECK(engine.RegisterGlobalFunction(
            "Dialog@ ui_create_dialog(const string &in, int, int)",
            asMETHOD(ScriptUI, create_dialog), asCALL_THISCALL_ASGLOBAL, this));

        VTX_CHECK(engine.RegisterGlobalFunction(
            "BoxSizer@ ui_create_box_sizer(int)",
            asMETHOD(ScriptUI, create_box_sizer), asCALL_THISCALL_ASGLOBAL, this));

        VTX_CHECK(engine.RegisterGlobalFunction(
            "void ui_message_box(const string &in, const string &in, int)",
            asMETHOD(ScriptUI, message_box), asCALL_THISCALL_ASGLOBAL, this));

        return StatusCode::STATUS_OK;
    }

    ScriptFrame* ScriptUI::create_frame(const std::string& title, int width, int height)
    {
        auto* wrapper = new ScriptFrame{};
        dispatch_to_main_sync([&]()
        {
            wrapper->m_frame = new wxFrame{wxTheApp->GetTopWindow(), wxID_ANY,
                wxString::FromUTF8(title), wxDefaultPosition, wxSize{width, height}};
            wrapper->m_frame->Bind(wxEVT_DESTROY, [wrapper](wxWindowDestroyEvent& evt)
            {
                if (evt.GetEventObject() == wrapper->m_frame)
                {
                    wrapper->m_frame = nullptr;
                }
                evt.Skip();
            });
        });
        return wrapper;
    }

    ScriptDialog* ScriptUI::create_dialog(const std::string& title, int width, int height)
    {
        auto* wrapper = new ScriptDialog{};
        dispatch_to_main_sync([&]()
        {
            wrapper->m_dialog = new wxDialog{wxTheApp->GetTopWindow(), wxID_ANY,
                wxString::FromUTF8(title), wxDefaultPosition, wxSize{width, height},
                wxDEFAULT_DIALOG_STYLE | wxRESIZE_BORDER};
            wrapper->m_dialog->Bind(wxEVT_DESTROY, [wrapper](wxWindowDestroyEvent& evt)
            {
                if (evt.GetEventObject() == wrapper->m_dialog)
                {
                    wrapper->m_dialog = nullptr;
                }
                evt.Skip();
            });
        });
        return wrapper;
    }

    ScriptBoxSizer* ScriptUI::create_box_sizer(int orientation)
    {
        auto* wrapper = new ScriptBoxSizer{};
        dispatch_to_main_sync([&]()
        {
            wrapper->m_sizer = new wxBoxSizer{orientation};
        });
        return wrapper;
    }

    void ScriptUI::message_box(const std::string& message, const std::string& caption, int flags)
    {
        dispatch_to_main_sync([&]()
        {
            wxMessageBox(wxString::FromUTF8(message), wxString::FromUTF8(caption), flags);
        });
    }
}

#undef VTX_CHECK
