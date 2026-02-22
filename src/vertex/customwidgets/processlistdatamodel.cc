//
// Copyright (C) 2026 PHTNC<>.
// Licensed under GPLv3.0 with Plugin Interface exceptions.
//
#include <vertex/customwidgets/processlistdatamodel.hh>

namespace Vertex::CustomWidgets
{
    ProcessListDataModel::ProcessListDataModel(std::shared_ptr<ViewModel::ProcessListViewModel> viewModel)
        : m_viewModel(std::move(viewModel))
    {
    }

    unsigned int ProcessListDataModel::GetColumnCount() const
    {
        return 3;
    }

    wxString ProcessListDataModel::GetColumnType([[maybe_unused]] const unsigned int col) const
    {
        return "string";
    }

    void ProcessListDataModel::GetValue(wxVariant& variant, const wxDataViewItem& item, const unsigned int col) const
    {
        if (!item.IsOk())
        {
            variant = wxString{};
            return;
        }

        const auto nodeIndex = item_to_node_index(item);
        variant = wxString::FromUTF8(m_viewModel->get_node_column_value(nodeIndex, static_cast<long>(col)));
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

        const auto nodeIndex = item_to_node_index(item);
        const auto parentIndex = m_viewModel->get_parent_node_index(nodeIndex);

        if (parentIndex == ViewModel::ProcessListViewModel::INVALID_NODE_INDEX)
        {
            return wxDataViewItem{nullptr};
        }

        return node_index_to_item(parentIndex);
    }

    bool ProcessListDataModel::IsContainer(const wxDataViewItem& item) const
    {
        if (!item.IsOk())
        {
            return true;
        }

        const auto nodeIndex = item_to_node_index(item);
        return m_viewModel->get_child_count(nodeIndex) > 0;
    }

    bool ProcessListDataModel::HasContainerColumns([[maybe_unused]] const wxDataViewItem& item) const
    {
        return true;
    }

    unsigned int ProcessListDataModel::GetChildren(const wxDataViewItem& parent, wxDataViewItemArray& array) const
    {
        if (!parent.IsOk())
        {
            const auto rootCount = m_viewModel->get_root_count();
            for (std::size_t i = 0; i < rootCount; ++i)
            {
                const auto nodeIndex = m_viewModel->get_root_node_index(i);
                if (nodeIndex != ViewModel::ProcessListViewModel::INVALID_NODE_INDEX)
                {
                    array.Add(node_index_to_item(nodeIndex));
                }
            }
            return static_cast<unsigned int>(array.GetCount());
        }

        const auto parentNodeIndex = item_to_node_index(parent);
        const auto childCount = m_viewModel->get_child_count(parentNodeIndex);

        for (std::size_t i = 0; i < childCount; ++i)
        {
            const auto childNodeIndex = m_viewModel->get_child_node_index(parentNodeIndex, i);
            if (childNodeIndex != ViewModel::ProcessListViewModel::INVALID_NODE_INDEX)
            {
                array.Add(node_index_to_item(childNodeIndex));
            }
        }

        return static_cast<unsigned int>(array.GetCount());
    }

    bool ProcessListDataModel::rebuild()
    {
        if (m_viewModel->consume_tree_dirty())
        {
            Cleared();
            return true;
        }
        return false;
    }

    std::size_t ProcessListDataModel::item_to_node_index(const wxDataViewItem& item)
    {
        return reinterpret_cast<std::size_t>(item.GetID()) - 1;
    }

    wxDataViewItem ProcessListDataModel::node_index_to_item(const std::size_t nodeIndex)
    {
        return wxDataViewItem{reinterpret_cast<void*>(nodeIndex + 1)};
    }
}
