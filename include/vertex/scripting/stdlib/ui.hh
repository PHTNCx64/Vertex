//
// Copyright (C) 2026 PHTNC<>.
// Licensed under GPLv3.0 with Plugin Interface exceptions.
//

#pragma once

#include <sdk/statuscode.h>

#include <atomic>
#include <functional>
#include <string>

class asIScriptEngine;

class wxFrame;
class wxDialog;
class wxPanel;
class wxButton;
class wxTextCtrl;
class wxCheckBox;
class wxComboBox;
class wxStaticText;
class wxStaticBox;
class wxBoxSizer;
class wxStaticBoxSizer;

namespace Vertex::Scripting::Stdlib
{
    void dispatch_to_main_sync(const std::function<void()>& func);

    struct ScriptPanel;
    struct ScriptButton;
    struct ScriptTextInput;
    struct ScriptCheckBox;
    struct ScriptComboBox;
    struct ScriptLabel;
    struct ScriptBoxSizer;
    struct ScriptGroupBox;

    struct ScriptFrame final
    {
        wxFrame* m_frame{};
        std::atomic<int> m_refCount{1};

        void add_ref();
        void release();

        void show() const;
        void hide() const;
        void close() const;
        void set_title(const std::string& title) const;
        void set_size(int width, int height) const;
        void set_sizer(const ScriptBoxSizer* sizer) const;
        ScriptPanel* create_panel() const;
        int from_dip(int value) const;
        bool is_open() const;
    };

    struct ScriptDialog final
    {
        wxDialog* m_dialog{};
        std::atomic<int> m_refCount{1};

        void add_ref();
        void release();

        void show() const;
        void hide() const;
        int show_modal() const;
        void end_modal(int returnCode) const;
        void close() const;
        void set_title(const std::string& title) const;
        void set_size(int width, int height) const;
        void set_sizer(const ScriptBoxSizer* sizer) const;
        ScriptPanel* create_panel() const;
        int from_dip(int value) const;
        bool is_open() const;
    };

    struct ScriptPanel final
    {
        wxPanel* m_panel{};
        std::atomic<int> m_refCount{1};

        void add_ref();
        void release();

        ScriptButton* add_button(const std::string& label) const;
        ScriptTextInput* add_text_input(const std::string& value) const;
        ScriptCheckBox* add_checkbox(const std::string& label) const;
        ScriptComboBox* add_combobox() const;
        ScriptLabel* add_label(const std::string& text) const;
        ScriptPanel* add_panel() const;
        ScriptGroupBox* create_group_box(const std::string& title, int orientation) const;
        void set_sizer(const ScriptBoxSizer* sizer) const;
        int from_dip(int value) const;
    };

    struct ScriptButton final
    {
        wxButton* m_button{};
        std::atomic<int> m_refCount{1};

        void add_ref();
        void release();

        void set_label(const std::string& label) const;
        std::string get_label() const;
        void enable(bool enabled) const;
        bool is_enabled() const;
    };

    struct ScriptTextInput final
    {
        wxTextCtrl* m_textCtrl{};
        std::atomic<int> m_refCount{1};

        void add_ref();
        void release();

        void set_value(const std::string& value);
        std::string get_value() const;
        void set_read_only(bool readOnly) const;
        void enable(bool enabled) const;
        bool is_enabled() const;
    };

    struct ScriptCheckBox final
    {
        wxCheckBox* m_checkBox{};
        std::atomic<int> m_refCount{1};

        void add_ref();
        void release();

        void set_checked(bool checked) const;
        bool is_checked() const;
        void set_label(const std::string& label) const;
        std::string get_label() const;
        void enable(bool enabled) const;
        bool is_enabled() const;
    };

    struct ScriptComboBox final
    {
        wxComboBox* m_comboBox{};
        std::atomic<int> m_refCount{1};

        void add_ref();
        void release();

        void add_item(const std::string& item) const;
        void clear_items() const;
        int get_selection() const;
        void set_selection(int index) const;
        std::string get_value() const;
        void enable(bool enabled) const;
        bool is_enabled() const;
    };

    struct ScriptLabel final
    {
        wxStaticText* m_label{};
        std::atomic<int> m_refCount{1};

        void add_ref();
        void release();

        void set_text(const std::string& text) const;
        std::string get_text() const;
    };

    struct ScriptBoxSizer final
    {
        wxBoxSizer* m_sizer{};
        std::atomic<int> m_refCount{1};

        void add_ref();
        void release();

        void add_panel(ScriptPanel* panel, int proportion, int flags, int border);
        void add_button(ScriptButton* button, int proportion, int flags, int border);
        void add_text_input(ScriptTextInput* input, int proportion, int flags, int border);
        void add_checkbox(ScriptCheckBox* checkbox, int proportion, int flags, int border);
        void add_combobox(ScriptComboBox* combobox, int proportion, int flags, int border);
        void add_label(ScriptLabel* label, int proportion, int flags, int border);
        void add_box_sizer(ScriptBoxSizer* sizer, int proportion, int flags, int border);
        void add_group(ScriptGroupBox* group, int proportion, int flags, int border);
        void add_spacer(int size);
        void add_stretch(int proportion);
    };

    struct ScriptGroupBox final
    {
        wxStaticBox* m_staticBox{};
        wxStaticBoxSizer* m_sizer{};
        std::atomic<int> m_refCount{1};

        void add_ref();
        void release();

        void add_panel(ScriptPanel* panel, int proportion, int flags, int border);
        void add_button(ScriptButton* button, int proportion, int flags, int border);
        void add_text_input(ScriptTextInput* input, int proportion, int flags, int border);
        void add_checkbox(ScriptCheckBox* checkbox, int proportion, int flags, int border);
        void add_combobox(ScriptComboBox* combobox, int proportion, int flags, int border);
        void add_label(ScriptLabel* label, int proportion, int flags, int border);
        void add_box_sizer(ScriptBoxSizer* sizer, int proportion, int flags, int border);
        void add_spacer(int size);
        void add_stretch(int proportion);
    };

    class ScriptUI final
    {
    public:
        [[nodiscard]] StatusCode register_api(asIScriptEngine& engine);

    private:
        ScriptFrame* create_frame(const std::string& title, int width, int height);
        ScriptDialog* create_dialog(const std::string& title, int width, int height);
        ScriptBoxSizer* create_box_sizer(int orientation);
        void message_box(const std::string& message, const std::string& caption, int flags);

        [[nodiscard]] static StatusCode declare_types(asIScriptEngine& engine);
        [[nodiscard]] static StatusCode register_constants(asIScriptEngine& engine);
        [[nodiscard]] static StatusCode register_frame_methods(asIScriptEngine& engine);
        [[nodiscard]] static StatusCode register_dialog_methods(asIScriptEngine& engine);
        [[nodiscard]] static StatusCode register_panel_methods(asIScriptEngine& engine);
        [[nodiscard]] static StatusCode register_button_methods(asIScriptEngine& engine);
        [[nodiscard]] static StatusCode register_text_input_methods(asIScriptEngine& engine);
        [[nodiscard]] static StatusCode register_checkbox_methods(asIScriptEngine& engine);
        [[nodiscard]] static StatusCode register_combobox_methods(asIScriptEngine& engine);
        [[nodiscard]] static StatusCode register_label_methods(asIScriptEngine& engine);
        [[nodiscard]] static StatusCode register_box_sizer_methods(asIScriptEngine& engine);
        [[nodiscard]] static StatusCode register_group_box_methods(asIScriptEngine& engine);
        [[nodiscard]] StatusCode register_factories(asIScriptEngine& engine);
    };
}
