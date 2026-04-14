//
// Copyright (C) 2026 PHTNC<>.
// Licensed under GPLv3.0 with Plugin Interface exceptions.
//
#include <vertex/view/pointerscanview.hh>
#include <vertex/utility.hh>
#include <vertex/event/types/viewevent.hh>

#include <wx/app.h>
#include <wx/filedlg.h>
#include <wx/msgdlg.h>

#include <fmt/format.h>
#include <limits>
#include <ranges>

namespace Vertex::View
{
    static constexpr std::size_t ROOT_PAGE_SIZE = 1000;
    static constexpr std::uint32_t DUMMY_NODE_ID = std::numeric_limits<std::uint32_t>::max();

    class TreeItemNodeData final : public wxTreeItemData
    {
    public:
        explicit TreeItemNodeData(const std::uint32_t nodeId, const bool hasChildren = true)
            : m_nodeId{nodeId}
            , m_hasChildren{hasChildren}
        {
        }

        [[nodiscard]] std::uint32_t get_node_id() const noexcept { return m_nodeId; }
        [[nodiscard]] bool has_children() const noexcept { return m_hasChildren; }

    private:
        std::uint32_t m_nodeId{};
        bool m_hasChildren{};
    };

    PointerScanView::PointerScanView(
        Language::ILanguage& languageService,
        std::unique_ptr<ViewModel::PointerScanViewModel> viewModel
    )
        : wxDialog(wxTheApp->GetTopWindow(),
                   wxID_ANY,
                   wxString::FromUTF8(languageService.fetch_translation("pointerScanView.title")),
                   wxDefaultPosition,
                   wxWindowBase::FromDIP(wxSize(StandardWidgetValues::STANDARD_X_DIP,
                                                StandardWidgetValues::STANDARD_Y_DIP),
                                         wxTheApp->GetTopWindow()),
                   wxDEFAULT_DIALOG_STYLE | wxRESIZE_BORDER)
        , m_languageService{languageService}
        , m_viewModel{std::move(viewModel)}
    {
        create_controls();
        layout_controls();
        bind_events();
        setup_event_callback();
    }

    void PointerScanView::create_controls()
    {
        m_mainSizer = new wxBoxSizer(wxVERTICAL);

        m_tree = new wxTreeCtrl(this, wxID_ANY, wxDefaultPosition, wxDefaultSize,
            wxTR_HAS_BUTTONS | wxTR_LINES_AT_ROOT | wxTR_HIDE_ROOT);

        m_progressBar = new wxGauge(this, wxID_ANY, StandardWidgetValues::GAUGE_MAX_VALUE,
            wxDefaultPosition, wxDefaultSize, wxGA_HORIZONTAL);

        m_statusText = new wxStaticText(this, wxID_ANY,
            wxString::FromUTF8(m_languageService.fetch_translation("pointerScanView.idle")));

        m_statsText = new wxStaticText(this, wxID_ANY, wxEmptyString);

        m_stopButton = new wxButton(this, wxID_ANY,
            wxString::FromUTF8(m_languageService.fetch_translation("pointerScanView.stopScan")));
        m_stopButton->Enable(false);

        m_saveButton = new wxButton(this, wxID_ANY,
            wxString::FromUTF8(m_languageService.fetch_translation("pointerScanView.save")));
        m_saveButton->Enable(false);

        m_loadButton = new wxButton(this, wxID_ANY,
            wxString::FromUTF8(m_languageService.fetch_translation("pointerScanView.load")));

        m_rescanButton = new wxButton(this, wxID_ANY,
            wxString::FromUTF8(m_languageService.fetch_translation("pointerScanView.rescan")));
        m_rescanButton->Enable(false);

        m_compareButton = new wxButton(this, wxID_ANY,
            wxString::FromUTF8(m_languageService.fetch_translation("pointerScanView.compare")));
        m_compareButton->Enable(false);

        m_progressTimer = new wxTimer(this, wxID_ANY);
    }

    void PointerScanView::layout_controls()
    {
        m_mainSizer->Add(m_tree, StandardWidgetValues::STANDARD_PROPORTION, wxEXPAND | wxALL,
            StandardWidgetValues::STANDARD_BORDER);

        auto* bottomSizer = new wxBoxSizer(wxVERTICAL);

        auto* statusSizer = new wxBoxSizer(wxHORIZONTAL);
        statusSizer->Add(m_statusText, StandardWidgetValues::STANDARD_PROPORTION, wxALIGN_CENTER_VERTICAL);
        statusSizer->Add(m_statsText, StandardWidgetValues::NO_PROPORTION,
            wxALIGN_CENTER_VERTICAL | wxLEFT, StandardWidgetValues::STANDARD_BORDER);

        bottomSizer->Add(m_progressBar, StandardWidgetValues::NO_PROPORTION,
            wxEXPAND | wxLEFT | wxRIGHT, StandardWidgetValues::STANDARD_BORDER);
        bottomSizer->Add(statusSizer, StandardWidgetValues::NO_PROPORTION,
            wxEXPAND | wxALL, StandardWidgetValues::STANDARD_BORDER);

        auto* buttonSizer = new wxBoxSizer(wxHORIZONTAL);
        buttonSizer->Add(m_loadButton, StandardWidgetValues::NO_PROPORTION);
        buttonSizer->Add(m_saveButton, StandardWidgetValues::NO_PROPORTION, wxLEFT, StandardWidgetValues::STANDARD_BORDER);
        buttonSizer->Add(m_rescanButton, StandardWidgetValues::NO_PROPORTION, wxLEFT, StandardWidgetValues::STANDARD_BORDER);
        buttonSizer->Add(m_compareButton, StandardWidgetValues::NO_PROPORTION, wxLEFT, StandardWidgetValues::STANDARD_BORDER);
        buttonSizer->AddStretchSpacer();
        buttonSizer->Add(m_stopButton, StandardWidgetValues::NO_PROPORTION);

        bottomSizer->Add(buttonSizer, StandardWidgetValues::NO_PROPORTION,
            wxEXPAND | wxLEFT | wxRIGHT | wxBOTTOM, StandardWidgetValues::STANDARD_BORDER);

        m_mainSizer->Add(bottomSizer, StandardWidgetValues::NO_PROPORTION, wxEXPAND);

        SetSizer(m_mainSizer);

        m_rootItem = m_tree->AddRoot(wxEmptyString);
    }

    void PointerScanView::bind_events()
    {
        Bind(wxEVT_TIMER, &PointerScanView::on_scan_progress_update, this, m_progressTimer->GetId());
        Bind(wxEVT_CLOSE_WINDOW, &PointerScanView::on_close, this);
        m_stopButton->Bind(wxEVT_BUTTON, &PointerScanView::on_stop_clicked, this);
        m_saveButton->Bind(wxEVT_BUTTON, &PointerScanView::on_save_clicked, this);
        m_loadButton->Bind(wxEVT_BUTTON, &PointerScanView::on_load_clicked, this);
        m_rescanButton->Bind(wxEVT_BUTTON, &PointerScanView::on_rescan_clicked, this);
        m_compareButton->Bind(wxEVT_BUTTON, &PointerScanView::on_compare_clicked, this);
        m_tree->Bind(wxEVT_TREE_ITEM_EXPANDING, &PointerScanView::on_tree_item_expanding, this);
    }

    void PointerScanView::setup_event_callback()
    {
        if (m_viewModel)
        {
            m_viewModel->set_event_callback(
                [this](const Event::EventId eventId, const Event::VertexEvent& event)
                {
                    vertex_event_callback(eventId, event);
                });
        }
    }

    void PointerScanView::vertex_event_callback(
        [[maybe_unused]] const Event::EventId eventId,
        [[maybe_unused]] const Event::VertexEvent& event)
    {
        std::ignore = toggle_view();
    }

    bool PointerScanView::toggle_view()
    {
        if (IsShown())
        {
            Hide();
            return false;
        }

        Show();
        Raise();
        return true;
    }

    void PointerScanView::start_scan(const Scanner::PointerScanConfig& config)
    {
        m_tree->DeleteChildren(m_rootItem);
        m_progressBar->SetValue(0);
        m_statusText->SetLabel(
            wxString::FromUTF8(m_languageService.fetch_translation("pointerScanView.scanning")));
        m_statsText->SetLabel(wxEmptyString);
        m_stopButton->Enable(true);
        m_saveButton->Enable(false);
        m_loadButton->Enable(false);
        m_rescanButton->Enable(false);
        m_compareButton->Enable(false);

        const auto status = m_viewModel->start_scan(config);
        if (status != StatusCode::STATUS_OK)
        {
            m_statusText->SetLabel(
                wxString::FromUTF8(m_languageService.fetch_translation("pointerScanView.scanFailed")));
            update_button_states();
            return;
        }

        m_progressTimer->Start(StandardWidgetValues::SCAN_PROGRESS_INTERVAL_MS);

        Show();
        Raise();
    }

    void PointerScanView::on_scan_progress_update([[maybe_unused]] wxTimerEvent& event)
    {
        m_viewModel->update_progress();
        const auto& progress = m_viewModel->get_progress();

        if (progress.total > 0)
        {
            constexpr std::int64_t GAUGE_MAX = 10000;
            const auto scaledTotal = static_cast<int>(std::min(static_cast<std::int64_t>(progress.total), GAUGE_MAX));
            const auto scaledCurrent = static_cast<int>(
                static_cast<std::int64_t>(progress.current) * scaledTotal / static_cast<std::int64_t>(progress.total));
            m_progressBar->SetRange(scaledTotal);
            m_progressBar->SetValue(std::min(scaledCurrent, scaledTotal));
        }

        m_statusText->SetLabel(wxString::FromUTF8(progress.statusMessage));
        m_statsText->SetLabel(wxString::FromUTF8(
            fmt::format(fmt::runtime(m_languageService.fetch_translation("pointerScanView.nodeEdgeStats")),
                progress.nodeCount, progress.edgeCount)));

        if (m_viewModel->is_scan_complete())
        {
            m_progressTimer->Stop();
            m_progressBar->SetRange(1);
            m_progressBar->SetValue(1);

            populate_roots();
            update_button_states();
        }
    }

    void PointerScanView::on_stop_clicked([[maybe_unused]] wxCommandEvent& event)
    {
        std::ignore = m_viewModel->stop_scan();
        m_statusText->SetLabel(
            wxString::FromUTF8(m_languageService.fetch_translation("pointerScanView.scanStopped")));
        update_button_states();
    }

    void PointerScanView::on_save_clicked([[maybe_unused]] wxCommandEvent& event)
    {
        wxFileDialog saveDialog(
            this,
            wxString::FromUTF8(m_languageService.fetch_translation("pointerScanView.saveDialog")),
            wxEmptyString,
            wxEmptyString,
            wxString::FromUTF8(m_languageService.fetch_translation("pointerScanView.fileFilter")),
            wxFD_SAVE | wxFD_OVERWRITE_PROMPT);

        if (saveDialog.ShowModal() != wxID_OK)
        {
            return;
        }

        const std::filesystem::path filePath{saveDialog.GetPath().ToStdString()};
        const auto status = m_viewModel->save_graph(filePath);

        if (status != StatusCode::STATUS_OK)
        {
            wxMessageBox(
                wxString::FromUTF8(m_languageService.fetch_translation("pointerScanView.saveFailed")),
                wxString::FromUTF8(m_languageService.fetch_translation("pointerScanView.title")),
                wxOK | wxICON_ERROR, this);
        }
    }

    void PointerScanView::on_load_clicked([[maybe_unused]] wxCommandEvent& event)
    {
        wxFileDialog openDialog(
            this,
            wxString::FromUTF8(m_languageService.fetch_translation("pointerScanView.loadDialog")),
            wxEmptyString,
            wxEmptyString,
            wxString::FromUTF8(m_languageService.fetch_translation("pointerScanView.fileFilter")),
            wxFD_OPEN | wxFD_FILE_MUST_EXIST);

        if (openDialog.ShowModal() != wxID_OK)
        {
            return;
        }

        const std::filesystem::path filePath{openDialog.GetPath().ToStdString()};
        const auto status = m_viewModel->load_graph(filePath);

        if (status != StatusCode::STATUS_OK)
        {
            wxMessageBox(
                wxString::FromUTF8(m_languageService.fetch_translation("pointerScanView.loadFailed")),
                wxString::FromUTF8(m_languageService.fetch_translation("pointerScanView.title")),
                wxOK | wxICON_ERROR, this);
            return;
        }

        populate_roots();
        update_button_states();

        const auto& config = m_viewModel->get_config();
        m_statusText->SetLabel(wxString::FromUTF8(
            fmt::format(fmt::runtime(m_languageService.fetch_translation("pointerScanView.loadedTarget")),
                config.targetAddress)));
    }

    void PointerScanView::on_rescan_clicked([[maybe_unused]] wxCommandEvent& event)
    {
        m_tree->DeleteChildren(m_rootItem);
        m_progressBar->SetValue(0);
        m_statusText->SetLabel(
            wxString::FromUTF8(m_languageService.fetch_translation("pointerScanView.rescanning")));
        m_statsText->SetLabel(wxEmptyString);
        m_stopButton->Enable(true);
        m_saveButton->Enable(false);
        m_loadButton->Enable(false);
        m_rescanButton->Enable(false);
        m_compareButton->Enable(false);

        const auto status = m_viewModel->start_rescan();
        if (status != StatusCode::STATUS_OK)
        {
            m_statusText->SetLabel(
                wxString::FromUTF8(m_languageService.fetch_translation("pointerScanView.rescanFailed")));
            update_button_states();
            return;
        }

        m_progressTimer->Start(StandardWidgetValues::SCAN_PROGRESS_INTERVAL_MS);
    }

    void PointerScanView::on_compare_clicked([[maybe_unused]] wxCommandEvent& event)
    {
        wxFileDialog openDialog(
            this,
            wxString::FromUTF8(m_languageService.fetch_translation("pointerScanView.compareDialog")),
            wxEmptyString,
            wxEmptyString,
            wxString::FromUTF8(m_languageService.fetch_translation("pointerScanView.fileFilter")),
            wxFD_OPEN | wxFD_FILE_MUST_EXIST | wxFD_MULTIPLE);

        if (openDialog.ShowModal() != wxID_OK)
        {
            return;
        }

        wxArrayString selectedPaths{};
        openDialog.GetPaths(selectedPaths);

        if (selectedPaths.empty())
        {
            return;
        }

        auto comparisonFiles = selectedPaths
            | std::views::transform([](const wxString& p) { return std::filesystem::path{p.ToStdString()}; })
            | std::ranges::to<std::vector>();

        const auto status = m_viewModel->compare_with_files(comparisonFiles);
        if (status != StatusCode::STATUS_OK)
        {
            wxMessageBox(
                wxString::FromUTF8(m_languageService.fetch_translation("pointerScanView.compareFailed")),
                wxString::FromUTF8(m_languageService.fetch_translation("pointerScanView.title")),
                wxOK | wxICON_ERROR, this);
            return;
        }

        populate_stable_paths();
    }

    void PointerScanView::populate_stable_paths()
    {
        const auto& stablePaths = m_viewModel->get_stable_paths();

        m_tree->Freeze();
        m_tree->DeleteChildren(m_rootItem);

        for (const auto& sig : stablePaths)
        {
            const auto label = wxString::FromUTF8(
                ViewModel::PointerScanViewModel::format_path_signature(sig));
            m_tree->AppendItem(m_rootItem, label);
        }

        m_statusText->SetLabel(wxString::FromUTF8(
            fmt::format(fmt::runtime(m_languageService.fetch_translation("pointerScanView.stablePathsFound")),
                stablePaths.size())));
        m_statsText->SetLabel(wxEmptyString);

        m_tree->Thaw();
    }

    void PointerScanView::update_button_states() const
    {
        const bool complete = m_viewModel->is_scan_complete();
        const bool hasData = m_viewModel->has_graph_data();

        m_stopButton->Enable(!complete);
        m_saveButton->Enable(complete && hasData);
        m_loadButton->Enable(complete);
        m_rescanButton->Enable(complete && hasData);
        m_compareButton->Enable(complete && hasData);
    }

    void PointerScanView::on_close([[maybe_unused]] wxCloseEvent& event)
    {
        if (m_progressTimer->IsRunning())
        {
            m_progressTimer->Stop();
            std::ignore = m_viewModel->stop_scan();
        }
        Hide();
    }

    void PointerScanView::on_tree_item_expanding(const wxTreeEvent& event)
    {
        const wxTreeItemId item = event.GetItem();
        if (!item.IsOk())
        {
            return;
        }

        wxTreeItemIdValue cookie{};
        const wxTreeItemId firstChild = m_tree->GetFirstChild(item, cookie);
        if (!firstChild.IsOk())
        {
            return;
        }

        const auto* firstData = dynamic_cast<TreeItemNodeData*>(m_tree->GetItemData(firstChild));
        if (firstData && firstData->get_node_id() == DUMMY_NODE_ID)
        {
            m_tree->DeleteChildren(item);

            const auto* parentData = dynamic_cast<TreeItemNodeData*>(m_tree->GetItemData(item));
            if (parentData)
            {
                add_children_to_item(item, parentData->get_node_id());
            }
        }
    }

    void PointerScanView::populate_roots()
    {
        m_tree->Freeze();
        m_tree->DeleteChildren(m_rootItem);

        const auto rootCount = m_viewModel->get_root_count();
        const auto roots = m_viewModel->get_roots(0, std::min(rootCount, static_cast<std::uint64_t>(ROOT_PAGE_SIZE)));

        for (const auto& root : roots)
        {
            const wxString label = wxString::FromUTF8(
                fmt::format("{} [0x{:X}]", root.expression, root.address));

            const wxTreeItemId treeItem = m_tree->AppendItem(m_rootItem, label, -1, -1,
                new TreeItemNodeData(root.nodeId));

            m_tree->AppendItem(treeItem, wxString::FromUTF8(
                m_languageService.fetch_translation("pointerScanView.loading")),
                -1, -1, new TreeItemNodeData(DUMMY_NODE_ID, false));
        }

        if (rootCount > ROOT_PAGE_SIZE)
        {
            m_tree->AppendItem(m_rootItem,
                wxString::FromUTF8(fmt::format(
                    fmt::runtime(m_languageService.fetch_translation("pointerScanView.moreRoots")),
                    rootCount - ROOT_PAGE_SIZE)));
        }

        m_statusText->SetLabel(wxString::FromUTF8(
            fmt::format(fmt::runtime(m_languageService.fetch_translation("pointerScanView.rootsFound")),
                rootCount)));

        m_tree->Thaw();
    }

    void PointerScanView::add_children_to_item(const wxTreeItemId parentItem, const std::uint32_t nodeId)
    {
        const auto children = m_viewModel->get_children(nodeId);

        if (children.empty())
        {
            m_tree->SetItemHasChildren(parentItem, false);
            return;
        }

        for (const auto& child : children)
        {
            const auto offsetText = child.offset >= 0
                ? fmt::format("+0x{:X}", child.offset)
                : fmt::format("-0x{:X}", -child.offset);

            const wxString label = wxString::FromUTF8(
                fmt::format("[{}] {} [0x{:X}]", offsetText, child.expression, child.address));

            const wxTreeItemId treeItem = m_tree->AppendItem(parentItem, label, -1, -1,
                new TreeItemNodeData(child.nodeId));

            m_tree->AppendItem(treeItem, wxString::FromUTF8(
                m_languageService.fetch_translation("pointerScanView.loading")),
                -1, -1, new TreeItemNodeData(DUMMY_NODE_ID, false));
        }
    }
}
