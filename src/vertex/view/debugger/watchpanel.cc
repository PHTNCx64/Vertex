//
// Copyright (C) 2026 PHTNC<>.
// Licensed under GPLv3.0 with Plugin Interface exceptions.
//
#include <vertex/view/debugger/watchpanel.hh>
#include <vertex/utility.hh>
#include <wx/clipbrd.h>
#include <wx/dataobj.h>
#include <wx/menu.h>
#include <wx/textdlg.h>
#include <wx/stattext.h>
#include <fmt/format.h>
#include <string_view>

namespace Vertex::View::Debugger
{
    class WatchItemData final : public wxTreeItemData
    {
    public:
        explicit WatchItemData(std::uint32_t id) : m_id(id) {}
        [[nodiscard]] std::uint32_t get_id() const { return m_id; }
    private:
        std::uint32_t m_id{};
    };

    WatchPanel::WatchPanel(wxWindow* parent, Language::ILanguage& languageService)
        : wxPanel(parent, wxID_ANY)
        , m_languageService(languageService)
    {
        create_controls();
        layout_controls();
        bind_events();
    }

    void WatchPanel::create_controls()
    {
        m_mainSizer = new wxBoxSizer(wxVERTICAL);

        auto* inputSizer = new wxBoxSizer(wxHORIZONTAL);
        m_expressionInput = new wxTextCtrl(this, wxID_ANY, EMPTY_STRING, wxDefaultPosition,
                                            wxDefaultSize, wxTE_PROCESS_ENTER);
        m_expressionInput->SetHint(wxString::FromUTF8(m_languageService.fetch_translation("debugger.watch.enterExpression")));
        m_addButton = new wxButton(this, wxID_ANY, "+", wxDefaultPosition, wxSize(FromDIP(30), -1));

        inputSizer->Add(m_expressionInput, StandardWidgetValues::STANDARD_PROPORTION, wxEXPAND | wxRIGHT, StandardWidgetValues::STANDARD_BORDER);
        inputSizer->Add(m_addButton, StandardWidgetValues::NO_PROPORTION);

        m_splitter = new wxSplitterWindow(this, wxID_ANY, wxDefaultPosition, wxDefaultSize,
                                           wxSP_3D | wxSP_LIVE_UPDATE);

        m_watchesPanel = new wxPanel(m_splitter);
        auto* watchesSizer = new wxBoxSizer(wxVERTICAL);
        auto* watchesLabel = new wxStaticText(m_watchesPanel, wxID_ANY, wxString::FromUTF8(m_languageService.fetch_translation("debugger.watch.watchTitle")));
        watchesLabel->SetFont(watchesLabel->GetFont().Bold());

        m_watchTree = new wxTreeCtrl(m_watchesPanel, wxID_ANY, wxDefaultPosition, wxDefaultSize,
                                      wxTR_HAS_BUTTONS | wxTR_LINES_AT_ROOT | wxTR_SINGLE);
        m_watchTree->SetFont(wxFont(9, wxFONTFAMILY_TELETYPE, wxFONTSTYLE_NORMAL, wxFONTWEIGHT_NORMAL));
        m_watchTree->AddRoot(wxString::FromUTF8(m_languageService.fetch_translation("debugger.watch.watches")));

        watchesSizer->Add(watchesLabel, StandardWidgetValues::NO_PROPORTION, wxEXPAND | wxALL, StandardWidgetValues::STANDARD_BORDER);
        watchesSizer->Add(m_watchTree, StandardWidgetValues::STANDARD_PROPORTION, wxEXPAND | wxLEFT | wxRIGHT | wxBOTTOM, StandardWidgetValues::STANDARD_BORDER);
        m_watchesPanel->SetSizer(watchesSizer);

        m_localsPanel = new wxPanel(m_splitter);
        auto* localsSizer = new wxBoxSizer(wxVERTICAL);
        auto* localsLabel = new wxStaticText(m_localsPanel, wxID_ANY, wxString::FromUTF8(m_languageService.fetch_translation("debugger.watch.localsTitle")));
        localsLabel->SetFont(localsLabel->GetFont().Bold());

        m_localsTree = new wxTreeCtrl(m_localsPanel, wxID_ANY, wxDefaultPosition, wxDefaultSize,
                                       wxTR_HAS_BUTTONS | wxTR_LINES_AT_ROOT | wxTR_SINGLE);
        m_localsTree->SetFont(wxFont(9, wxFONTFAMILY_TELETYPE, wxFONTSTYLE_NORMAL, wxFONTWEIGHT_NORMAL));
        m_localsTree->AddRoot(wxString::FromUTF8(m_languageService.fetch_translation("debugger.watch.locals")));

        localsSizer->Add(localsLabel, StandardWidgetValues::NO_PROPORTION, wxEXPAND | wxALL, StandardWidgetValues::STANDARD_BORDER);
        localsSizer->Add(m_localsTree, StandardWidgetValues::STANDARD_PROPORTION, wxEXPAND | wxLEFT | wxRIGHT | wxBOTTOM, StandardWidgetValues::STANDARD_BORDER);
        m_localsPanel->SetSizer(localsSizer);

        m_splitter->SplitHorizontally(m_watchesPanel, m_localsPanel);
        m_splitter->SetSashGravity(0.5);

        m_mainSizer->Add(inputSizer, StandardWidgetValues::NO_PROPORTION, wxEXPAND | wxALL, StandardWidgetValues::STANDARD_BORDER);
        m_mainSizer->Add(m_splitter, StandardWidgetValues::STANDARD_PROPORTION, wxEXPAND | wxLEFT | wxRIGHT | wxBOTTOM, StandardWidgetValues::STANDARD_BORDER);
    }

    void WatchPanel::layout_controls()
    {
        SetSizer(m_mainSizer);
    }

    void WatchPanel::bind_events()
    {
        m_addButton->Bind(wxEVT_BUTTON, &WatchPanel::on_add_watch, this);
        m_expressionInput->Bind(wxEVT_TEXT_ENTER, &WatchPanel::on_add_watch, this);
        m_watchTree->Bind(wxEVT_TREE_ITEM_ACTIVATED, &WatchPanel::on_tree_item_activated, this);
        m_watchTree->Bind(wxEVT_TREE_ITEM_RIGHT_CLICK, &WatchPanel::on_tree_item_right_click, this);
        m_watchTree->Bind(wxEVT_TREE_ITEM_EXPANDING, &WatchPanel::on_tree_item_expanding, this);
        m_watchTree->Bind(wxEVT_TREE_ITEM_COLLAPSING, &WatchPanel::on_tree_item_collapsing, this);
    }

    void WatchPanel::update_watches(const std::vector<::Vertex::Debugger::WatchVariable>& watches)
    {
        m_watches = watches;
        m_rebuildingWatchTree = true;
        m_watchTree->DeleteAllItems();
        const wxTreeItemId root = m_watchTree->AddRoot(wxString::FromUTF8(m_languageService.fetch_translation("debugger.watch.watches")));

        populate_tree(m_watchTree, root, watches);
        m_watchTree->Expand(root);
        m_rebuildingWatchTree = false;
    }

    void WatchPanel::update_locals(const std::vector<::Vertex::Debugger::LocalVariable>& locals)
    {
        m_locals = locals;
        m_localsTree->DeleteAllItems();
        const wxTreeItemId root = m_localsTree->AddRoot(wxString::FromUTF8(m_languageService.fetch_translation("debugger.watch.locals")));

        populate_locals_tree(m_localsTree, root, locals);
        m_localsTree->Expand(root);
    }

    void WatchPanel::set_add_watch_callback(AddWatchCallback callback)
    {
        m_addWatchCallback = std::move(callback);
    }

    void WatchPanel::set_remove_watch_callback(RemoveWatchCallback callback)
    {
        m_removeWatchCallback = std::move(callback);
    }

    void WatchPanel::set_modify_watch_callback(ModifyWatchCallback callback)
    {
        m_modifyWatchCallback = std::move(callback);
    }

    void WatchPanel::set_expand_watch_callback(ExpandWatchCallback callback)
    {
        m_expandWatchCallback = std::move(callback);
    }

    void WatchPanel::set_show_in_memory_callback(ShowInMemoryCallback callback)
    {
        m_showInMemoryCallback = std::move(callback);
    }

    void WatchPanel::on_add_watch([[maybe_unused]] wxCommandEvent& event)
    {
        const wxString expr = m_expressionInput->GetValue().Trim();
        if (!expr.empty() && m_addWatchCallback)
        {
            m_addWatchCallback(expr.ToStdString());
            m_expressionInput->Clear();
        }
    }

    void WatchPanel::on_tree_item_activated(wxTreeEvent& event)
    {
        const wxTreeItemId item = event.GetItem();
        if (!item.IsOk() || item == m_watchTree->GetRootItem())
        {
            return;
        }

        event.Skip();
    }

    void WatchPanel::on_tree_item_right_click(const wxTreeEvent& event)
    {
        const wxTreeItemId item = event.GetItem();
        if (!item.IsOk() || item == m_watchTree->GetRootItem())
        {
            return;
        }

        wxMenu menu;
        menu.Append(1001, wxString::FromUTF8(m_languageService.fetch_translation("debugger.watch.editValue")));
        menu.Append(1002, wxString::FromUTF8(m_languageService.fetch_translation("debugger.watch.copyValue")));
        menu.Append(1003, wxString::FromUTF8(m_languageService.fetch_translation("debugger.watch.copyExpression")));
        menu.Append(1005, wxString::FromUTF8("Show in Memory View"));
        menu.AppendSeparator();
        menu.Append(1004, wxString::FromUTF8(m_languageService.fetch_translation("debugger.watch.removeWatch")));

        const int selection = GetPopupMenuSelectionFromUser(menu, event.GetPoint());

        auto* data = dynamic_cast<WatchItemData*>(m_watchTree->GetItemData(item));
        if (!data)
        {
            return;
        }

        const std::uint32_t watchId = data->get_id();

        const auto find_watch_by_id = [](const auto& self,
                                         const std::vector<::Vertex::Debugger::WatchVariable>& vars,
                                         const std::uint32_t id) -> const ::Vertex::Debugger::WatchVariable*
        {
            for (const auto& var : vars)
            {
                if (var.id == id)
                {
                    return &var;
                }

                if (!var.children.empty())
                {
                    if (const auto* found = self(self, var.children, id); found != nullptr)
                    {
                        return found;
                    }
                }
            }

            return nullptr;
        };

        const auto copy_to_clipboard = [](const std::string_view text)
        {
            if (text.empty() || wxTheClipboard == nullptr)
            {
                return;
            }

            if (!wxTheClipboard->Open())
            {
                return;
            }

            wxTheClipboard->SetData(new wxTextDataObject(wxString::FromUTF8(std::string{text})));
            wxTheClipboard->Close();
        };

        switch (selection)
        {
            case 1001:
            {
                wxTextEntryDialog dialog(this,
                    wxString::FromUTF8(m_languageService.fetch_translation("debugger.watch.enterNewValue")),
                    wxString::FromUTF8(m_languageService.fetch_translation("debugger.watch.editWatchValueTitle")));
                if (dialog.ShowModal() == wxID_OK && m_modifyWatchCallback)
                {
                    m_modifyWatchCallback(watchId, dialog.GetValue().ToStdString());
                }
                break;
            }
            case 1002:
            {
                if (const auto* watch = find_watch_by_id(find_watch_by_id, m_watches, watchId); watch != nullptr)
                {
                    copy_to_clipboard(watch->value);
                }
                break;
            }
            case 1003:
            {
                if (const auto* watch = find_watch_by_id(find_watch_by_id, m_watches, watchId); watch != nullptr)
                {
                    copy_to_clipboard(watch->expression);
                }
                break;
            }
            case 1005:
            {
                if (const auto* watch = find_watch_by_id(find_watch_by_id, m_watches, watchId);
                    watch != nullptr && watch->address != 0 && m_showInMemoryCallback)
                {
                    m_showInMemoryCallback(watch->address);
                }
                break;
            }
            case 1004:
                if (m_removeWatchCallback)
                {
                    m_removeWatchCallback(watchId);
                }
                break;
            default:
                break;
        }
    }

    void WatchPanel::on_tree_item_expanding(const wxTreeEvent& event)
    {
        if (m_rebuildingWatchTree)
        {
            return;
        }

        const wxTreeItemId item = event.GetItem();
        if (!item.IsOk())
        {
            return;
        }

        auto* data = dynamic_cast<WatchItemData*>(m_watchTree->GetItemData(item));
        if (data && m_expandWatchCallback)
        {
            m_expandWatchCallback(data->get_id(), true);
        }
    }

    void WatchPanel::on_tree_item_collapsing(const wxTreeEvent& event)
    {
        if (m_rebuildingWatchTree)
        {
            return;
        }

        const wxTreeItemId item = event.GetItem();
        if (!item.IsOk())
        {
            return;
        }

        const auto* data = dynamic_cast<WatchItemData*>(m_watchTree->GetItemData(item));
        if (data && m_expandWatchCallback)
        {
            m_expandWatchCallback(data->get_id(), false);
        }
    }

    void WatchPanel::populate_tree(wxTreeCtrl* tree, wxTreeItemId parent,
                                    const std::vector<::Vertex::Debugger::WatchVariable>& vars)
    {
        for (const auto& var : vars)
        {
            wxString label;
            if (var.hasError)
            {
                label = fmt::format("{} = <error: {}>", var.name, var.errorMessage);
            }
            else
            {
                label = fmt::format("{} = {} ({})", var.name, var.value, var.typeName);
            }

            const wxTreeItemId item = tree->AppendItem(parent, label);
            tree->SetItemData(item, new WatchItemData(var.id));

            if (var.hasChildren && !var.children.empty())
            {
                populate_tree(tree, item, var.children);
            }
            else if (var.hasChildren)
            {
                tree->AppendItem(item, wxString::FromUTF8(m_languageService.fetch_translation("debugger.ui.loading")));
            }

            if (var.isExpanded && var.hasChildren)
            {
                tree->Expand(item);
            }
        }
    }

    void WatchPanel::populate_locals_tree(wxTreeCtrl* tree, wxTreeItemId parent,
                                           const std::vector<::Vertex::Debugger::LocalVariable>& vars)
    {
        for (const auto& var : vars)
        {
            const wxString label = fmt::format("{} = {} ({})", var.name, var.value, var.typeName);
            const wxTreeItemId item = tree->AppendItem(parent, label);

            if (var.hasChildren && !var.children.empty())
            {
                populate_locals_tree(tree, item, var.children);
            }
        }
    }

}
