//
// Copyright (C) 2026 PHTNC<>.
// Licensed under GPLv3.0 with Plugin Interface exceptions.
//
#pragma once

#include <array>
#include <cstdint>
#include <string>
#include <unordered_map>
#include <vector>

#include <wx/dataview.h>

#include <vertex/viewmodel/processlistviewmodel.hh>

namespace Vertex::CustomWidgets
{
    class ProcessListDataModel final : public wxDataViewModel
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

        std::shared_ptr<ViewModel::ProcessListViewModel> m_viewModel;
        Snapshot m_snapshot{};
    };
}
