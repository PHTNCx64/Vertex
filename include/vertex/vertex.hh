//
// Copyright (C) 2026 PHTNC<>.
// Licensed under GPLv3.0 with Plugin Interface exceptions.
//
#pragma once

#include <vertex/di.hh>
#include <wx/wx.h>

class VertexApp final : public wxApp
{
public:
    bool OnInit() override;
    int OnExit() override;

private:
    [[nodiscard]] StatusCode initialize_filesystem() const;
    void apply_language_settings() const;
    void apply_plugin_settings() const;
    void apply_appearance_settings();

    std::unique_ptr<decltype(Vertex::DI::create_injector())> m_injector;
};
