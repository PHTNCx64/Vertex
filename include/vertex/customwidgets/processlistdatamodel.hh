//
// Copyright (C) 2026 PHTNC<>.
// Licensed under GPLv3.0 with Plugin Interface exceptions.
//
#pragma once

#include <array>
#include <cstdint>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#include <wx/dataview.h>

#include <vertex/customwidgets/base/ifilterabledataviewmodel.hh>
#include <vertex/viewmodel/processlistviewmodel.hh>

namespace Vertex::CustomWidgets
{
    class ProcessListDataModel final : public wxDataViewModel, public Base::IFilterableDataViewModel
    {
    public:
        explicit ProcessListDataModel(std::shared_ptr<ViewModel::ProcessListViewModel> viewModel);

        [[nodiscard]] unsigned int GetColumnCount() const override;
        [[nodiscard]] wxString GetColumnType(unsigned int col) const override;
        void GetValue(wxVariant& variant, const wxDataViewItem& item, unsigned int col) const override;
        bool SetValue(const wxVariant& variant, const wxDataViewItem& item, unsigned int col) override;
        [[nodiscard]] wxDataViewItem GetParent(const wxDataViewItem& item) const override;
        [[nodiscard]] bool IsContainer(const wxDataViewItem& item) const override;
        [[nodiscard]] bool HasContainerColumns(const wxDataViewItem& item) const override;
        unsigned int GetChildren(const wxDataViewItem& parent, wxDataViewItemArray& array) const override;
        [[nodiscard]] bool HasDefaultCompare() const override;
        int Compare(const wxDataViewItem& item1, const wxDataViewItem& item2, unsigned int column, bool ascending) const override;

        void set_filter_text(const wxString& filterText) override;
        [[nodiscard]] bool passes_filter(const wxDataViewItem& item) const override;

        void rebuild();

        [[nodiscard]] static std::uint32_t item_to_pid(const wxDataViewItem& item);
        [[nodiscard]] static wxDataViewItem pid_to_item(std::uint32_t pid);

    private:
        static constexpr std::size_t COLUMN_COUNT = 3;

        struct CachedNode final
        {
            std::uint32_t pid{};
            std::uint32_t parentPid{};
            std::vector<std::uint32_t> childPids{};
            std::array<std::string, COLUMN_COUNT> columns{};
        };

        struct Snapshot final
        {
            std::unordered_map<std::uint32_t, CachedNode> nodes{};
            std::vector<std::uint32_t> rootPids{};
        };

        [[nodiscard]] Snapshot take_snapshot() const;
        void apply_diff(Snapshot&& newSnapshot);

        void rebuild_visible_set();
        bool mark_visible_recursive(std::uint32_t pid);
        [[nodiscard]] bool node_self_matches(const CachedNode& node) const;
        [[nodiscard]] bool is_visible(std::uint32_t pid) const noexcept;

        std::shared_ptr<ViewModel::ProcessListViewModel> m_viewModel;
        Snapshot m_snapshot{};

        wxString m_filterTextLower{};
        std::unordered_set<std::uint32_t> m_visibleSet{};
    };
}
