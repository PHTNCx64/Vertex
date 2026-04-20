//
// Copyright (C) 2026 PHTNC<>.
// Licensed under GPLv3.0 with Plugin Interface exceptions.
//
#include <vertex/customwidgets/accesstrackercallstackdialog.hh>
#include <vertex/utility.hh>

#include <fmt/format.h>

#include <utility>

namespace Vertex::CustomWidgets
{
    AccessTrackerCallStackDialog::AccessTrackerCallStackDialog(wxWindow* parent,
                                                               Language::ILanguage& languageService,
                                                               std::vector<Debugger::StackFrame> frames,
                                                               FrameCallback onGoToFrame)
        : wxDialog(parent, wxID_ANY,
            wxString::FromUTF8(languageService.fetch_translation("accessTracker.callStackDialog.title")),
            wxDefaultPosition,
            wxSize(FromDIP(AccessTrackerCallStackDialogValues::DIALOG_WIDTH),
                   FromDIP(AccessTrackerCallStackDialogValues::DIALOG_HEIGHT)))
        , m_languageService(languageService)
        , m_frames(std::move(frames))
        , m_onGoToFrame(std::move(onGoToFrame))
    {
        auto* mainSizer = new wxBoxSizer(wxVERTICAL);

        m_frameList = new wxListCtrl(this, wxID_ANY, wxDefaultPosition, wxDefaultSize,
            wxLC_REPORT | wxLC_SINGLE_SEL);

        m_frameList->InsertColumn(0,
            wxString::FromUTF8(m_languageService.fetch_translation("accessTracker.callStackDialog.columnIndex")),
            wxLIST_FORMAT_LEFT, FromDIP(AccessTrackerCallStackDialogValues::COLUMN_WIDTH_INDEX));
        m_frameList->InsertColumn(1,
            wxString::FromUTF8(m_languageService.fetch_translation("accessTracker.callStackDialog.columnReturnAddress")),
            wxLIST_FORMAT_LEFT, FromDIP(AccessTrackerCallStackDialogValues::COLUMN_WIDTH_ADDRESS));
        m_frameList->InsertColumn(2,
            wxString::FromUTF8(m_languageService.fetch_translation("accessTracker.callStackDialog.columnModule")),
            wxLIST_FORMAT_LEFT, FromDIP(AccessTrackerCallStackDialogValues::COLUMN_WIDTH_MODULE));
        m_frameList->InsertColumn(3,
            wxString::FromUTF8(m_languageService.fetch_translation("accessTracker.callStackDialog.columnFunction")),
            wxLIST_FORMAT_LEFT, FromDIP(AccessTrackerCallStackDialogValues::COLUMN_WIDTH_FUNCTION));

        populate_frames();

        auto* buttonSizer = new wxBoxSizer(wxHORIZONTAL);
        m_goButton = new wxButton(this, wxID_ANY,
            wxString::FromUTF8(m_languageService.fetch_translation("accessTracker.callStackDialog.goToFrame")));
        m_closeButton = new wxButton(this, wxID_CLOSE,
            wxString::FromUTF8(m_languageService.fetch_translation("general.close")));

        buttonSizer->AddStretchSpacer();
        buttonSizer->Add(m_goButton, StandardWidgetValues::NO_PROPORTION,
            wxRIGHT, StandardWidgetValues::STANDARD_BORDER);
        buttonSizer->Add(m_closeButton, StandardWidgetValues::NO_PROPORTION);

        mainSizer->Add(m_frameList, StandardWidgetValues::STANDARD_PROPORTION,
            wxEXPAND | wxALL, StandardWidgetValues::STANDARD_BORDER);
        mainSizer->Add(buttonSizer, StandardWidgetValues::NO_PROPORTION,
            wxEXPAND | wxLEFT | wxRIGHT | wxBOTTOM, StandardWidgetValues::STANDARD_BORDER);

        SetSizer(mainSizer);

        m_goButton->Bind(wxEVT_BUTTON, [this](wxCommandEvent&)
        {
            const long selection = m_frameList->GetNextItem(-1, wxLIST_NEXT_ALL, wxLIST_STATE_SELECTED);
            trigger_go_to_frame(selection);
        });
        m_closeButton->Bind(wxEVT_BUTTON, [this](wxCommandEvent&)
        {
            EndModal(wxID_CLOSE);
        });
        m_frameList->Bind(wxEVT_LIST_ITEM_ACTIVATED,
            [this](wxListEvent& event)
            {
                trigger_go_to_frame(event.GetIndex());
            });

        if (!m_frames.empty())
        {
            m_frameList->SetItemState(0, wxLIST_STATE_SELECTED, wxLIST_STATE_SELECTED);
        }
    }

    void AccessTrackerCallStackDialog::populate_frames() const
    {
        for (std::size_t i = 0; i < m_frames.size(); ++i)
        {
            const auto& frame = m_frames[i];
            const long index = m_frameList->InsertItem(
                static_cast<long>(i), wxString::FromUTF8(std::to_string(frame.frameIndex)));
            m_frameList->SetItem(index, 1,
                wxString::FromUTF8(fmt::format("0x{:X}", frame.returnAddress)));
            m_frameList->SetItem(index, 2, wxString::FromUTF8(frame.moduleName));
            m_frameList->SetItem(index, 3, wxString::FromUTF8(frame.functionName));
        }
    }

    void AccessTrackerCallStackDialog::trigger_go_to_frame(const long selection)
    {
        if (selection < 0 || !m_onGoToFrame)
        {
            return;
        }
        const auto index = static_cast<std::size_t>(selection);
        if (index >= m_frames.size())
        {
            return;
        }
        m_onGoToFrame(m_frames[index].returnAddress);
    }
}
