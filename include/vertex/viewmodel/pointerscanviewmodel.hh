//
// Copyright (C) 2026 PHTNC<>.
// Licensed under GPLv3.0 with Plugin Interface exceptions.
//
#pragma once

#include <vertex/event/eventbus.hh>
#include <vertex/event/types/processopenevent.hh>
#include <vertex/model/pointerscanmodel.hh>
#include <vertex/scanner/pointerscanner/pointerscancomparator.hh>
#include <vertex/log/ilog.hh>
#include <vertex/utility.hh>

#include <filesystem>
#include <functional>
#include <memory>
#include <string>
#include <vector>

namespace Vertex::ViewModel
{
    struct PointerScanNodeInfo final
    {
        std::uint32_t nodeId{};
        std::uint64_t address{};
        std::string expression{};
        std::int32_t offset{};
        std::uint16_t depth{};
        bool isRoot{};
    };

    struct PointerScanProgressInfo final
    {
        Scanner::PointerScanPhase phase{Scanner::PointerScanPhase::Idle};
        std::uint64_t current{};
        std::uint64_t total{};
        std::uint32_t currentDepth{};
        std::uint32_t maxDepth{};
        std::uint64_t nodeCount{};
        std::uint64_t edgeCount{};
        std::string statusMessage{};
    };

    class PointerScanViewModel final
    {
    public:
        PointerScanViewModel(
            std::unique_ptr<Model::PointerScanModel> model,
            Event::EventBus& eventBus,
            Log::ILog& logService,
            std::string name = ViewModelName::POINTERSCAN
        );

        ~PointerScanViewModel();

        void set_event_callback(std::move_only_function<void(Event::EventId, const Event::VertexEvent&) const> eventCallback);

        [[nodiscard]] StatusCode start_scan(const Scanner::PointerScanConfig& config);
        [[nodiscard]] StatusCode start_rescan();
        [[nodiscard]] StatusCode stop_scan();

        [[nodiscard]] bool is_scan_complete() const;
        void update_progress();
        [[nodiscard]] const PointerScanProgressInfo& get_progress() const noexcept;

        [[nodiscard]] std::uint64_t get_root_count() const noexcept;
        [[nodiscard]] std::vector<PointerScanNodeInfo> get_roots(std::size_t startIndex, std::size_t count) const;
        [[nodiscard]] std::vector<PointerScanNodeInfo> get_children(std::uint32_t nodeId) const;

        [[nodiscard]] std::string format_node_expression(const Scanner::PointerNodeRecord& node) const;

        [[nodiscard]] StatusCode save_graph(const std::filesystem::path& filePath) const;
        [[nodiscard]] StatusCode load_graph(const std::filesystem::path& filePath);
        [[nodiscard]] bool has_graph_data() const noexcept;
        [[nodiscard]] const Scanner::PointerScanConfig& get_config() const noexcept;

        [[nodiscard]] StatusCode compare_with_files(const std::vector<std::filesystem::path>& comparisonFiles);
        [[nodiscard]] const std::vector<Scanner::PointerPathSignature>& get_stable_paths() const noexcept;
        [[nodiscard]] static std::string format_path_signature(const Scanner::PointerPathSignature& sig);

    private:
        void subscribe_to_events() const;
        void unsubscribe_from_events() const;

        std::string m_viewModelName{};
        std::unique_ptr<Model::PointerScanModel> m_model{};
        std::move_only_function<void(Event::EventId, const Event::VertexEvent&) const> m_eventCallback{};

        Event::EventBus& m_eventBus;
        Log::ILog& m_logService;

        PointerScanProgressInfo m_progressInfo{};
        std::vector<Scanner::PointerPathSignature> m_stablePaths{};
    };
}
