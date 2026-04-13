//
// Copyright (C) 2026 PHTNC<>.
// Licensed under GPLv3.0 with Plugin Interface exceptions.
//
#include <vertex/customwidgets/processlistdatamodel.hh>

#include <algorithm>
#include <ranges>
#include <unordered_set>

namespace Vertex::CustomWidgets
{
    ProcessListDataModel::ProcessListDataModel(std::shared_ptr<ViewModel::ProcessListViewModel> viewModel)
        : m_viewModel(std::move(viewModel))
    {
    }

    unsigned int ProcessListDataModel::GetColumnCount() const
    {
        return COLUMN_COUNT;
    }

    wxString ProcessListDataModel::GetColumnType([[maybe_unused]] const unsigned int col) const
    {
        return "string";
    }

    void ProcessListDataModel::GetValue(wxVariant& variant, const wxDataViewItem& item, const unsigned int col) const
    {
        if (!item.IsOk() || col >= COLUMN_COUNT)
        {
            variant = wxString{};
            return;
        }

        const auto it = m_snapshot.nodes.find(item_to_pid(item));
        if (it == m_snapshot.nodes.end())
        {
            variant = wxString{};
            return;
        }

        variant = wxString::FromUTF8(it->second.columns[col]);
    }

    bool ProcessListDataModel::SetValue([[maybe_unused]] const wxVariant& variant, [[maybe_unused]] const wxDataViewItem& item, [[maybe_unused]] unsigned int col)
    {
        return false;
    }

    wxDataViewItem ProcessListDataModel::GetParent(const wxDataViewItem& item) const
    {
        if (!item.IsOk())
        {
            return wxDataViewItem{nullptr};
        }

        const auto it = m_snapshot.nodes.find(item_to_pid(item));
        if (it == m_snapshot.nodes.end() || it->second.parentPid == 0)
        {
            return wxDataViewItem{nullptr};
        }

        return pid_to_item(it->second.parentPid);
    }

    bool ProcessListDataModel::IsContainer(const wxDataViewItem& item) const
    {
        if (!item.IsOk())
        {
            return true;
        }

        const auto it = m_snapshot.nodes.find(item_to_pid(item));
        return it != m_snapshot.nodes.end() && !it->second.childPids.empty();
    }

    bool ProcessListDataModel::HasContainerColumns([[maybe_unused]] const wxDataViewItem& item) const
    {
        return true;
    }

    unsigned int ProcessListDataModel::GetChildren(const wxDataViewItem& parent, wxDataViewItemArray& array) const
    {
        if (!parent.IsOk())
        {
            for (const auto rootPid : m_snapshot.rootPids)
            {
                array.Add(pid_to_item(rootPid));
            }
            return static_cast<unsigned int>(array.GetCount());
        }

        const auto it = m_snapshot.nodes.find(item_to_pid(parent));
        if (it == m_snapshot.nodes.end())
        {
            return 0;
        }

        for (const auto childPid : it->second.childPids)
        {
            array.Add(pid_to_item(childPid));
        }

        return static_cast<unsigned int>(array.GetCount());
    }

    bool ProcessListDataModel::HasDefaultCompare() const
    {
        return true;
    }

    int ProcessListDataModel::Compare(const wxDataViewItem& item1, const wxDataViewItem& item2,
                                      [[maybe_unused]] unsigned int column, [[maybe_unused]] bool ascending) const
    {
        const auto pid1 = item_to_pid(item1);
        const auto pid2 = item_to_pid(item2);

        const auto it1 = m_snapshot.nodes.find(pid1);
        const auto it2 = m_snapshot.nodes.find(pid2);
        if (it1 == m_snapshot.nodes.end() || it2 == m_snapshot.nodes.end())
        {
            return 0;
        }

        const auto parentPid = it1->second.parentPid;
        if (it2->second.parentPid != parentPid)
        {
            return 0;
        }

        const auto& siblings = parentPid == 0
            ? m_snapshot.rootPids
            : m_snapshot.nodes.at(parentPid).childPids;

        const auto pos1 = std::ranges::find(siblings, pid1);
        const auto pos2 = std::ranges::find(siblings, pid2);

        if (pos1 < pos2)
        {
            return -1;
        }
        if (pos1 > pos2)
        {
            return 1;
        }
        return 0;
    }

    void ProcessListDataModel::rebuild()
    {
        if (!m_viewModel->consume_tree_dirty())
        {
            return;
        }

        apply_diff(take_snapshot());
    }

    std::uint32_t ProcessListDataModel::item_to_pid(const wxDataViewItem& item)
    {
        return static_cast<std::uint32_t>(reinterpret_cast<std::uintptr_t>(item.GetID()));
    }

    wxDataViewItem ProcessListDataModel::pid_to_item(const std::uint32_t pid)
    {
        return wxDataViewItem{reinterpret_cast<void*>(static_cast<std::uintptr_t>(pid))};
    }

    ProcessListDataModel::Snapshot ProcessListDataModel::take_snapshot() const
    {
        Snapshot snap{};
        const auto rootCount = m_viewModel->get_root_count();

        struct StackEntry final
        {
            std::uint32_t parentPid{};
            std::size_t nodeIndex{};
        };

        std::vector<StackEntry> stack{};

        for (std::size_t i = rootCount; i > 0; --i)
        {
            const auto ni = m_viewModel->get_root_node_index(i - 1);
            if (ni != ViewModel::ProcessListViewModel::INVALID_NODE_INDEX)
            {
                stack.push_back({0, ni});
            }
        }

        while (!stack.empty())
        {
            const auto [parentPid, ni] = stack.back();
            stack.pop_back();

            CachedNode node{};
            node.columns[0] = m_viewModel->get_node_column_value(ni, 0);
            node.columns[1] = m_viewModel->get_node_column_value(ni, 1);
            node.columns[2] = m_viewModel->get_node_column_value(ni, 2);
            node.pid = static_cast<std::uint32_t>(std::stoul(node.columns[0]));
            node.parentPid = parentPid;

            if (parentPid == 0)
            {
                snap.rootPids.push_back(node.pid);
            }

            const auto childCount = m_viewModel->get_child_count(ni);
            node.childPids.reserve(childCount);

            for (std::size_t c = 0; c < childCount; ++c)
            {
                const auto childNi = m_viewModel->get_child_node_index(ni, c);
                if (childNi == ViewModel::ProcessListViewModel::INVALID_NODE_INDEX)
                {
                    continue;
                }
                const auto childPidStr = m_viewModel->get_node_column_value(childNi, 0);
                node.childPids.push_back(static_cast<std::uint32_t>(std::stoul(childPidStr)));
            }

            const auto currentPid = node.pid;
            snap.nodes.emplace(currentPid, std::move(node));

            for (std::size_t c = childCount; c > 0; --c)
            {
                const auto childNi = m_viewModel->get_child_node_index(ni, c - 1);
                if (childNi != ViewModel::ProcessListViewModel::INVALID_NODE_INDEX)
                {
                    stack.push_back({currentPid, childNi});
                }
            }
        }

        return snap;
    }

    void ProcessListDataModel::apply_diff(Snapshot&& newSnapshot)
    {
        std::unordered_set<std::uint32_t> oldPids{};
        std::unordered_set<std::uint32_t> newPids{};
        oldPids.reserve(m_snapshot.nodes.size());
        newPids.reserve(newSnapshot.nodes.size());

        for (const auto& pid : m_snapshot.nodes | std::views::keys)
        {
            oldPids.insert(pid);
        }
        for (const auto& pid : newSnapshot.nodes | std::views::keys)
        {
            newPids.insert(pid);
        }

        std::vector<std::uint32_t> removedPids{};
        std::vector<std::uint32_t> changedPids{};
        std::vector<std::uint32_t> movedPids{};

        for (const auto pid : oldPids)
        {
            if (!newPids.contains(pid))
            {
                removedPids.push_back(pid);
                continue;
            }

            const auto& oldNode = m_snapshot.nodes.at(pid);
            const auto& newNode = newSnapshot.nodes.at(pid);

            if (oldNode.parentPid != newNode.parentPid)
            {
                movedPids.push_back(pid);
            }
            else if (oldNode.columns != newNode.columns)
            {
                changedPids.push_back(pid);
            }
        }

        bool orderChanged = (m_snapshot.rootPids != newSnapshot.rootPids);
        if (!orderChanged)
        {
            for (const auto& [pid, newNode] : newSnapshot.nodes)
            {
                const auto oldIt = m_snapshot.nodes.find(pid);
                if (oldIt != m_snapshot.nodes.end() && oldIt->second.childPids != newNode.childPids)
                {
                    orderChanged = true;
                    break;
                }
            }
        }

        const bool hasNewPids = oldPids.size() != newPids.size() ||
            std::ranges::any_of(newPids, [&oldPids](const auto pid) { return !oldPids.contains(pid); });

        if (removedPids.empty() && movedPids.empty() && changedPids.empty() && !orderChanged && !hasNewPids)
        {
            m_snapshot = std::move(newSnapshot);
            return;
        }

        const std::unordered_set<std::uint32_t> removedSet(removedPids.begin(), removedPids.end());
        for (const auto pid : removedPids)
        {
            const auto& node = m_snapshot.nodes.at(pid);
            const bool parentAlsoRemoved = node.parentPid != 0 && removedSet.contains(node.parentPid);
            if (!parentAlsoRemoved)
            {
                const auto parentItem = node.parentPid == 0
                    ? wxDataViewItem{nullptr}
                    : pid_to_item(node.parentPid);
                ItemDeleted(parentItem, pid_to_item(pid));
            }
        }

        for (const auto pid : movedPids)
        {
            const auto& oldNode = m_snapshot.nodes.at(pid);
            const auto parentItem = oldNode.parentPid == 0
                ? wxDataViewItem{nullptr}
                : pid_to_item(oldNode.parentPid);
            ItemDeleted(parentItem, pid_to_item(pid));
        }

        m_snapshot = std::move(newSnapshot);

        std::unordered_set<std::uint32_t> addedPids{};
        for (const auto pid : newPids)
        {
            if (!oldPids.contains(pid))
            {
                addedPids.insert(pid);
            }
        }
        for (const auto pid : movedPids)
        {
            addedPids.insert(pid);
        }

        if (!addedPids.empty())
        {
            std::vector<std::uint32_t> bfsQueue(m_snapshot.rootPids.begin(), m_snapshot.rootPids.end());
            std::size_t bfsIdx{};

            while (bfsIdx < bfsQueue.size())
            {
                const auto pid = bfsQueue[bfsIdx++];
                if (addedPids.contains(pid))
                {
                    const auto& node = m_snapshot.nodes.at(pid);
                    const auto parentItem = node.parentPid == 0
                        ? wxDataViewItem{nullptr}
                        : pid_to_item(node.parentPid);
                    ItemAdded(parentItem, pid_to_item(pid));
                }

                const auto it = m_snapshot.nodes.find(pid);
                if (it != m_snapshot.nodes.end())
                {
                    for (const auto childPid : it->second.childPids)
                    {
                        bfsQueue.push_back(childPid);
                    }
                }
            }
        }

        for (const auto pid : changedPids)
        {
            ItemChanged(pid_to_item(pid));
        }

        if (orderChanged)
        {
            Resort();
        }
    }
}
