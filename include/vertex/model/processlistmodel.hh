//
// Copyright (C) 2026 PHTNC<>.
// Licensed under GPLv3.0 with Plugin Interface exceptions.
//
#pragma once

#include <vertex/language/language.hh>
#include <vertex/log/log.hh>
#include <vertex/configuration/settings.hh>
#include <vertex/gui/class/selectedprocess.hh>
#include <vertex/gui/enums/sortorder.hh>
#include <vertex/gui/enums/filtertype.hh>
#include <vertex/runtime/loader.hh>

#include <shared_mutex>
#include <string_view>
#include <unordered_map>

#include <sdk/process.h>

namespace Vertex::Model
{
    struct ProcessNode final
    {
        std::size_t processIndex{};
        std::vector<std::size_t> childNodeIndices{};
        bool matchesFilter{};
        bool visibleInFilter{};
    };

    class ProcessListModel final
    {
      public:
        explicit ProcessListModel(Runtime::ILoader& loaderService, Log::ILog& loggerService, Configuration::ISettings& settingsService)
            : m_loaderService{loaderService},
              m_loggerService{loggerService},
              m_settingsService{settingsService}
        {
            m_processes.reserve(INITIAL_PROCESS_LIST_SIZE);
        }

        [[nodiscard]] StatusCode open_process() const;

        void set_selected_process(const Class::SelectedProcess& process);
        void set_filter_type(Enums::FilterType filter);
        void set_filter_text(std::string_view text);
        void set_sort_order(Enums::SortOrder sortOrder);
        void set_clicked_column(long col) noexcept;
        void set_should_filter(bool shouldFilter) noexcept;
        void clear_selected_process() noexcept;

        [[nodiscard]] Class::SelectedProcess get_selected_process() const noexcept;
        [[nodiscard]] Enums::FilterType get_filter_type() const noexcept;
        [[nodiscard]] std::string get_filter_text() const noexcept;
        [[nodiscard]] Enums::SortOrder get_sort_order() const noexcept;
        [[nodiscard]] long get_clicked_column() const noexcept;
        [[nodiscard]] StatusCode get_process_list();
        [[nodiscard]] bool get_should_filter() const noexcept;

        void build_tree();
        void filter_list();
        void sort_list();
        [[nodiscard]] bool consume_tree_dirty() noexcept;

        [[nodiscard]] std::size_t get_root_count() const noexcept;
        [[nodiscard]] std::size_t get_child_count(std::size_t nodeIndex) const noexcept;
        [[nodiscard]] std::size_t get_root_node_index(std::size_t pos) const noexcept;
        [[nodiscard]] std::size_t get_child_node_index(std::size_t parentNodeIndex, std::size_t pos) const noexcept;
        [[nodiscard]] std::string get_node_column_value(std::size_t nodeIndex, long col) const;
        [[nodiscard]] std::size_t get_parent_node_index(std::size_t nodeIndex) const noexcept;
        [[nodiscard]] bool node_has_parent(std::size_t nodeIndex) const noexcept;
        [[nodiscard]] bool node_is_visible(std::size_t nodeIndex) const noexcept;
        [[nodiscard]] Class::SelectedProcess make_selected_process_from_node(std::size_t nodeIndex) noexcept;

        [[nodiscard]] int get_ui_state_int(std::string_view key, int defaultValue) const;
        void set_ui_state_int(std::string_view key, int value) const;

        static constexpr long PROCESS_ID_COLUMN = 0;
        static constexpr long PROCESS_NAME_COLUMN = 1;
        static constexpr long PROCESS_OWNER_COLUMN = 2;

        static constexpr std::size_t INVALID_NODE_INDEX = std::numeric_limits<std::size_t>::max();

      private:

        [[nodiscard]] bool compare_processes(std::size_t a, std::size_t b) const;
        [[nodiscard]] bool matches_filter(const ProcessInformation& process,
                              Enums::FilterType filterType,
                              const std::string& lowercaseFilterText,
                              const std::optional<std::boyer_moore_searcher<std::string::const_iterator>>& searcher);
        bool propagate_visibility(std::size_t nodeIndex);
        void sort_children(std::vector<std::size_t>& indices);
        void invalidate_cache();

        struct ProcessCache final
        {
            std::string lowercaseName;
            std::string lowercaseOwner;
            std::string idString;
            bool dirty{true};
        };

        std::unordered_map<const ProcessInformation*, ProcessCache> m_processCache{};

        static constexpr std::string_view MODEL_NAME{"ProcessListModel"};
        static constexpr int INITIAL_PROCESS_LIST_SIZE = 500;

        long m_selectedColumn{};
        std::atomic_bool m_shouldFilter{};
        std::atomic_bool m_filterDirty{true};
        std::atomic_bool m_treeDirty{false};

        Class::SelectedProcess m_selectedProcess{};
        Enums::SortOrder m_sortOrder{};
        Enums::FilterType m_filterType{};

        Runtime::ILoader& m_loaderService;
        Log::ILog& m_loggerService;
        Configuration::ISettings& m_settingsService;

        std::vector<ProcessInformation> m_processes{};

        std::vector<ProcessNode> m_treeNodes{};
        std::vector<std::size_t> m_rootNodeIndices{};
        std::unordered_map<std::uint32_t, std::size_t> m_pidToNodeIndex{};

        mutable std::shared_mutex m_stateMutex{};
        std::string m_filterText{};
    };
}
