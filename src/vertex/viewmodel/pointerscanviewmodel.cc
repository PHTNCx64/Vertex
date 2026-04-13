//
// Copyright (C) 2026 PHTNC<>.
// Licensed under GPLv3.0 with Plugin Interface exceptions.
//
#include <vertex/viewmodel/pointerscanviewmodel.hh>
#include <vertex/event/types/viewevent.hh>

#include <fmt/format.h>

namespace Vertex::ViewModel
{
    PointerScanViewModel::PointerScanViewModel(
        std::unique_ptr<Model::PointerScanModel> model,
        Event::EventBus& eventBus,
        Log::ILog& logService,
        std::string name
    )
        : m_viewModelName{std::move(name)}
        , m_model{std::move(model)}
        , m_eventBus{eventBus}
        , m_logService{logService}
    {
        subscribe_to_events();
    }

    PointerScanViewModel::~PointerScanViewModel()
    {
        unsubscribe_from_events();
    }

    void PointerScanViewModel::subscribe_to_events() const
    {
        m_eventBus.subscribe<Event::ViewEvent>(m_viewModelName, Event::VIEW_EVENT,
            [this](const Event::ViewEvent& event)
            {
                if (m_eventCallback)
                {
                    m_eventCallback(Event::VIEW_EVENT, event);
                }
            });

        m_eventBus.subscribe<Event::ProcessOpenEvent>(m_viewModelName, Event::PROCESS_OPEN_EVENT,
            [this](const Event::ProcessOpenEvent& event)
            {
                m_model->set_process_identity(event.get_process_id());
            });
    }

    void PointerScanViewModel::unsubscribe_from_events() const
    {
        m_eventBus.unsubscribe(m_viewModelName, Event::VIEW_EVENT);
        m_eventBus.unsubscribe(m_viewModelName, Event::PROCESS_OPEN_EVENT);
    }

    void PointerScanViewModel::set_event_callback(
        std::move_only_function<void(Event::EventId, const Event::VertexEvent&) const> eventCallback)
    {
        m_eventCallback = std::move(eventCallback);
    }

    StatusCode PointerScanViewModel::start_scan(const Scanner::PointerScanConfig& config)
    {
        m_progressInfo = {};
        return m_model->start_scan(config);
    }

    StatusCode PointerScanViewModel::start_rescan()
    {
        m_progressInfo = {};
        return m_model->start_rescan();
    }

    StatusCode PointerScanViewModel::stop_scan()
    {
        return m_model->stop_scan();
    }

    bool PointerScanViewModel::is_scan_complete() const
    {
        return m_model->is_scan_complete();
    }

    void PointerScanViewModel::update_progress()
    {
        const auto progress = m_model->get_progress();
        m_progressInfo.phase = progress.phase;
        m_progressInfo.current = progress.current;
        m_progressInfo.total = progress.total;
        m_progressInfo.currentDepth = progress.currentDepth;
        m_progressInfo.maxDepth = progress.maxDepth;
        m_progressInfo.nodeCount = m_model->get_node_count();
        m_progressInfo.edgeCount = m_model->get_edge_count();

        switch (progress.phase)
        {
            case Scanner::PointerScanPhase::IndexBuild:
                m_progressInfo.statusMessage = fmt::format("Building index... {}/{} regions",
                    progress.current, progress.total);
                break;
            case Scanner::PointerScanPhase::IndexMergeSort:
                m_progressInfo.statusMessage = fmt::format("Merging index... {}/{}",
                    progress.current, progress.total);
                break;
            case Scanner::PointerScanPhase::BFSDepth:
                m_progressInfo.statusMessage = fmt::format("BFS depth {}/{}, {} nodes, {} edges",
                    progress.currentDepth, progress.maxDepth,
                    m_progressInfo.nodeCount, m_progressInfo.edgeCount);
                break;
            case Scanner::PointerScanPhase::FinalizeGraph:
                m_progressInfo.statusMessage = fmt::format("Finalizing graph... {}/{}",
                    progress.current, progress.total);
                break;
            case Scanner::PointerScanPhase::Rescan:
                m_progressInfo.statusMessage = fmt::format("Validating edges... {}/{}",
                    progress.current, progress.total);
                break;
            case Scanner::PointerScanPhase::Idle:
            default:
                if (m_model->is_scan_complete() && m_progressInfo.nodeCount > 0)
                {
                    m_progressInfo.statusMessage = fmt::format("Complete: {} nodes, {} edges",
                        m_progressInfo.nodeCount, m_progressInfo.edgeCount);
                }
                break;
        }
    }

    const PointerScanProgressInfo& PointerScanViewModel::get_progress() const noexcept
    {
        return m_progressInfo;
    }

    std::uint64_t PointerScanViewModel::get_root_count() const noexcept
    {
        return m_model->get_root_count();
    }

    std::vector<PointerScanNodeInfo> PointerScanViewModel::get_roots(
        const std::size_t startIndex,
        const std::size_t count) const
    {
        std::vector<std::uint32_t> nodeIds{};
        std::vector<Scanner::PointerNodeRecord> roots{};

        if (m_model->get_root_node_ids(nodeIds, startIndex, count) != StatusCode::STATUS_OK ||
            m_model->get_roots_range(roots, startIndex, count) != StatusCode::STATUS_OK)
        {
            return {};
        }

        std::vector<PointerScanNodeInfo> result{};
        result.reserve(roots.size());

        for (std::size_t i{}; i < roots.size(); ++i)
        {
            result.emplace_back(PointerScanNodeInfo{
                .nodeId = nodeIds[i],
                .address = roots[i].address,
                .expression = format_node_expression(roots[i]),
                .offset = 0,
                .depth = roots[i].minDepthDiscovered,
                .isRoot = true
            });
        }

        return result;
    }

    std::vector<PointerScanNodeInfo> PointerScanViewModel::get_children(const std::uint32_t nodeId) const
    {
        std::vector<Scanner::PointerEdgeRecord> edges{};
        const auto maxChildren = static_cast<std::size_t>(m_model->get_config().maxParentsPerNode);
        if (m_model->get_children(nodeId, edges, 0, maxChildren) != StatusCode::STATUS_OK)
        {
            return {};
        }

        std::vector<PointerScanNodeInfo> result{};
        result.reserve(edges.size());

        for (const auto& edge : edges)
        {
            Scanner::PointerNodeRecord childNode{};
            if (m_model->get_node(edge.childNodeId, childNode) == StatusCode::STATUS_OK)
            {
                result.emplace_back(PointerScanNodeInfo{
                    .nodeId = edge.childNodeId,
                    .address = childNode.address,
                    .expression = format_node_expression(childNode),
                    .offset = edge.offset,
                    .depth = childNode.minDepthDiscovered,
                    .isRoot = false
                });
            }
        }

        return result;
    }

    std::string PointerScanViewModel::format_node_expression(const Scanner::PointerNodeRecord& node) const
    {
        const auto& modules = m_model->get_modules();

        for (const auto& mod : modules)
        {
            if (mod.moduleId == node.moduleId && !mod.moduleName.empty())
            {
                const auto relativeOffset = node.address - mod.moduleBase;
                return fmt::format("{}+0x{:X}", mod.moduleName, relativeOffset);
            }
        }

        return fmt::format("0x{:X}", node.address);
    }

    StatusCode PointerScanViewModel::save_graph(const std::filesystem::path& filePath) const
    {
        return m_model->save_graph(filePath);
    }

    StatusCode PointerScanViewModel::load_graph(const std::filesystem::path& filePath)
    {
        m_progressInfo = {};
        return m_model->load_graph(filePath);
    }

    bool PointerScanViewModel::has_graph_data() const noexcept
    {
        return m_model->has_graph_data();
    }

    const Scanner::PointerScanConfig& PointerScanViewModel::get_config() const noexcept
    {
        return m_model->get_config();
    }

    StatusCode PointerScanViewModel::compare_with_files(const std::vector<std::filesystem::path>& comparisonFiles)
    {
        m_stablePaths.clear();
        return m_model->compare_with_files(comparisonFiles, m_stablePaths);
    }

    const std::vector<Scanner::PointerPathSignature>& PointerScanViewModel::get_stable_paths() const noexcept
    {
        return m_stablePaths;
    }

    std::string PointerScanViewModel::format_path_signature(const Scanner::PointerPathSignature& sig)
    {
        std::string result = fmt::format("{}+0x{:X}", sig.rootModuleName, sig.rootModuleOffset);

        for (const auto offset : sig.offsets)
        {
            if (offset >= 0)
            {
                result += fmt::format(" -> [+0x{:X}]", offset);
            }
            else
            {
                result += fmt::format(" -> [-0x{:X}]", -offset);
            }
        }

        return result;
    }
}
