//
// Copyright (C) 2026 PHTNC<>.
// Licensed under GPLv3.0 with Plugin Interface exceptions.
//
#pragma once

#include <memory>
#include <sdk/statuscode.h>
#include <wx/wx.h>

class VertexApp final : public wxApp
{
public:
    VertexApp();
    ~VertexApp() override;

    bool OnInit() override;
    int OnExit() override;

private:
    [[nodiscard]] StatusCode initialize_filesystem() const;
    void apply_language_settings() const;
    void apply_plugin_settings() const;
    void apply_appearance_settings();
    void apply_script_settings() const;

    struct Impl;
    std::unique_ptr<Impl> m_impl;
};
