//
// Copyright (C) 2026 PHTNC<>.
// Licensed under GPLv3.0 with Plugin Interface exceptions.
//
#pragma once

#include <vertex/debugger/debuggertypes.hh>
#include <vertex/language/ilanguage.hh>

#include <wx/button.h>
#include <wx/dialog.h>
#include <wx/listctrl.h>
#include <wx/sizer.h>

#include <cstdint>
#include <functional>
#include <vector>

namespace Vertex::CustomWidgets
{
    class AccessTrackerCallStackDialog final : public wxDialog
    {
    public:
        using FrameCallback = std::function<void(std::uint64_t address)>;

        AccessTrackerCallStackDialog(wxWindow* parent,
                                     Language::ILanguage& languageService,
                                     std::vector<Debugger::StackFrame> frames,
                                     FrameCallback onGoToFrame);

    private:
        void populate_frames() const;
        void trigger_go_to_frame(long selection);

        Language::ILanguage& m_languageService;
        std::vector<Debugger::StackFrame> m_frames {};
        FrameCallback m_onGoToFrame {};

        wxListCtrl* m_frameList {};
        wxButton* m_goButton {};
        wxButton* m_closeButton {};
    };
}
