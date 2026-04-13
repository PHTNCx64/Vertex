//
// Copyright (C) 2026 PHTNC<>.
// Licensed under GPLv3.0 with Plugin Interface exceptions.
//
#include <vertex/model/pointerscanmodel.hh>
#include <vertex/model/memoryattributemodel.hh>
#include <vertex/scanner/pluginmemoryreader.hh>
#include <vertex/runtime/caller.hh>

#include <fmt/format.h>
#include <ranges>
#include <span>
#include <unordered_map>
#include <unordered_set>

namespace Vertex::Model
{
    static constexpr auto* MODEL_NAME = "PointerScanModel";

    PointerScanModel::PointerScanModel(
        Scanner::IPointerScanner& pointerScanner,
        Runtime::ILoader& loaderService,
        Configuration::IPluginConfig& pluginConfig,
        Log::ILog& logService,
        Thread::IThreadDispatcher& dispatcher
    )
        : m_pointerScanner{pointerScanner}
        , m_loaderService{loaderService}
        , m_pluginConfig{pluginConfig}
        , m_logService{logService}
        , m_dispatcher{dispatcher}
    {
    }

    void PointerScanModel::set_process_identity(const std::uint32_t processId)
    {
        m_processId = processId;
    }

    StatusCode PointerScanModel::start_scan(const Scanner::PointerScanConfig& config) const
    {
        ensure_memory_reader_initialized();

        const auto attributeStatus = apply_pointer_scan_memory_attributes();
        if (attributeStatus != StatusCode::STATUS_OK)
        {
            m_logService.log_error(fmt::format("{}: Failed to apply pointer-scan memory attributes", MODEL_NAME));
            return attributeStatus;
        }

        std::vector<Scanner::ScanRegion> regions{};
        const auto regionStatus = query_memory_regions(regions);
        if (regionStatus != StatusCode::STATUS_OK)
        {
            m_logService.log_error(fmt::format("{}: Failed to query memory regions", MODEL_NAME));
            return regionStatus;
        }

        auto effectiveConfig = config;
        effectiveConfig.scanContextHash = compute_scan_context_hash();

        m_logService.log_info(fmt::format("{}: Starting pointer scan for target 0x{:X}, depth={}, regions={}",
            MODEL_NAME, effectiveConfig.targetAddress, effectiveConfig.maxDepth, regions.size()));

        return m_pointerScanner.initialize_scan(effectiveConfig, regions);
    }

    std::uint64_t PointerScanModel::compute_scan_context_hash() const
    {
        constexpr std::uint64_t FNV_OFFSET_BASIS{14695981039346656037ULL};
        constexpr std::uint64_t FNV_PRIME{1099511628211ULL};

        std::uint64_t hash{FNV_OFFSET_BASIS};

        const auto fnv_mix = [&hash](const std::uint64_t value)
        {
            hash ^= value;
            hash *= FNV_PRIME;
        };

        fnv_mix(m_processId);

        const auto pluginOpt = m_loaderService.get_active_plugin();
        if (pluginOpt.has_value())
        {
            for (const auto ch : pluginOpt.value().get().get_path().string())
            {
                fnv_mix(static_cast<std::uint64_t>(static_cast<unsigned char>(ch)));
            }
        }

        for (const auto& module : m_pluginConfig.get_excluded_modules())
        {
            for (const auto ch : module)
            {
                fnv_mix(static_cast<std::uint64_t>(static_cast<unsigned char>(ch)));
            }
        }

        const auto memoryAttributeSection = has_pointer_scan_memory_attribute_overrides()
                                                ? "pointerScanMemoryAttributes"
                                                : "memoryAttributes";

        for (const auto& attr : m_pluginConfig.get_enabled_memory_attributes(memoryAttributeSection, "protections"))
        {
            for (const auto ch : attr)
            {
                fnv_mix(static_cast<std::uint64_t>(static_cast<unsigned char>(ch)));
            }
        }

        for (const auto& attr : m_pluginConfig.get_enabled_memory_attributes(memoryAttributeSection, "states"))
        {
            for (const auto ch : attr)
            {
                fnv_mix(static_cast<std::uint64_t>(static_cast<unsigned char>(ch)));
            }
        }

        for (const auto& attr : m_pluginConfig.get_enabled_memory_attributes(memoryAttributeSection, "types"))
        {
            for (const auto ch : attr)
            {
                fnv_mix(static_cast<std::uint64_t>(static_cast<unsigned char>(ch)));
            }
        }

        return hash;
    }

    StatusCode PointerScanModel::start_rescan() const
    {
        ensure_memory_reader_initialized();

        m_logService.log_info(fmt::format("{}: Starting rescan", MODEL_NAME));
        return m_pointerScanner.initialize_rescan();
    }

    StatusCode PointerScanModel::stop_scan() const {
        return m_pointerScanner.stop_scan();
    }

    void PointerScanModel::finalize_scan() const {
        m_pointerScanner.finalize_scan();
    }

    bool PointerScanModel::is_scan_complete() const
    {
        return m_pointerScanner.is_scan_complete();
    }

    Scanner::PointerScanProgress PointerScanModel::get_progress() const noexcept
    {
        return m_pointerScanner.get_progress();
    }

    std::uint64_t PointerScanModel::get_node_count() const noexcept
    {
        return m_pointerScanner.get_node_count();
    }

    std::uint64_t PointerScanModel::get_edge_count() const noexcept
    {
        return m_pointerScanner.get_edge_count();
    }

    std::uint64_t PointerScanModel::get_root_count() const noexcept
    {
        return m_pointerScanner.get_root_count();
    }

    StatusCode PointerScanModel::get_root_node_ids(
        std::vector<std::uint32_t>& nodeIds,
        const std::size_t startIndex,
        const std::size_t count) const
    {
        return m_pointerScanner.get_root_node_ids(nodeIds, startIndex, count);
    }

    StatusCode PointerScanModel::get_roots_range(
        std::vector<Scanner::PointerNodeRecord>& roots,
        const std::size_t startIndex,
        const std::size_t count) const
    {
        return m_pointerScanner.get_roots_range(roots, startIndex, count);
    }

    StatusCode PointerScanModel::get_children(
        const std::uint32_t nodeId,
        std::vector<Scanner::PointerEdgeRecord>& edges,
        const std::size_t startIndex,
        const std::size_t count) const
    {
        return m_pointerScanner.get_children(nodeId, edges, startIndex, count);
    }

    StatusCode PointerScanModel::get_node(
        const std::uint32_t nodeId,
        Scanner::PointerNodeRecord& node) const
    {
        return m_pointerScanner.get_node(nodeId, node);
    }

    const std::vector<Scanner::ModuleRecord>& PointerScanModel::get_modules() const noexcept
    {
        return m_pointerScanner.get_modules();
    }

    StatusCode PointerScanModel::save_graph(const std::filesystem::path& filePath) const
    {
        return m_pointerScanner.save_graph(filePath);
    }

    StatusCode PointerScanModel::load_graph(const std::filesystem::path& filePath) const
    {
        ensure_memory_reader_initialized();
        return m_pointerScanner.load_graph(filePath);
    }

    bool PointerScanModel::has_graph_data() const noexcept
    {
        return m_pointerScanner.get_node_count() > 0 && m_pointerScanner.is_scan_complete();
    }

    const Scanner::PointerScanConfig& PointerScanModel::get_config() const noexcept
    {
        return m_pointerScanner.get_config();
    }

    StatusCode PointerScanModel::compare_with_files(
        const std::vector<std::filesystem::path>& comparisonFiles,
        std::vector<Scanner::PointerPathSignature>& stablePaths) const
    {
        const auto& currentModules = m_pointerScanner.get_modules();
        const auto build_module_span_map = [](std::span<const Scanner::ModuleRecord> modules)
        {
            std::unordered_map<std::string_view, std::uint64_t> modulesByName{};
            modulesByName.reserve(modules.size());

            for (const auto& module : modules)
            {
                if (module.moduleName.empty())
                {
                    continue;
                }

                modulesByName.insert_or_assign(module.moduleName, module.moduleSpan);
            }

            return modulesByName;
        };

        const auto currentModulesByName = build_module_span_map(currentModules);
        std::vector<std::vector<Scanner::PointerPathSignature>> allPathSets{};
        allPathSets.reserve(comparisonFiles.size() + 1);

        allPathSets.push_back(Scanner::PointerScanComparator::extract_paths(m_pointerScanner));

        m_logService.log_info(fmt::format("{}: Extracted {} paths from current graph",
                                          MODEL_NAME, allPathSets.back().size()));

        for (const auto& file : comparisonFiles)
        {
            Scanner::GraphSnapshot snapshot{};
            const auto status = Scanner::PointerScanComparator::load_snapshot(file, snapshot);
            if (status != StatusCode::STATUS_OK)
            {
                m_logService.log_error(fmt::format("{}: Failed to load comparison file: {}",
                                                    MODEL_NAME, file.string()));
                return status;
            }

            if (!Scanner::PointerScanComparator::modules_compatible(currentModules, snapshot.modules))
            {
                const auto snapshotModulesByName = build_module_span_map(snapshot.modules);

                std::size_t commonModules{};
                std::size_t missingInComparison{};
                std::size_t missingInCurrent{};
                std::size_t spanMismatches{};

                std::string firstMissingInComparison{"<none>"};
                std::string firstMissingInCurrent{"<none>"};
                std::string firstSpanMismatch{"<none>"};

                for (const auto& [moduleName, currentSpan] : currentModulesByName)
                {
                    const auto it = snapshotModulesByName.find(moduleName);
                    if (it == snapshotModulesByName.end())
                    {
                        ++missingInComparison;
                        if (firstMissingInComparison == "<none>")
                        {
                            firstMissingInComparison = std::string{moduleName};
                        }
                        continue;
                    }

                    ++commonModules;
                    if (it->second != currentSpan)
                    {
                        ++spanMismatches;
                        if (firstSpanMismatch == "<none>")
                        {
                            firstSpanMismatch = fmt::format("{} (current=0x{:X}, comparison=0x{:X})",
                                moduleName, currentSpan, it->second);
                        }
                    }
                }

                for (const auto& moduleName : snapshotModulesByName | std::views::keys)
                {
                    if (currentModulesByName.contains(moduleName))
                    {
                        continue;
                    }

                    ++missingInCurrent;
                    if (firstMissingInCurrent == "<none>")
                    {
                        firstMissingInCurrent = std::string{moduleName};
                    }
                }

                m_logService.log_warn(fmt::format(
                    "{}: Module layout mismatch while comparing {} (common={}, missingInComparison={}, missingInCurrent={}, spanMismatches={}, firstMissingInComparison={}, firstMissingInCurrent={}, firstSpanMismatch={}). Continuing comparison using path intersection.",
                    MODEL_NAME, file.filename().string(), commonModules, missingInComparison, missingInCurrent, spanMismatches,
                    firstMissingInComparison, firstMissingInCurrent, firstSpanMismatch));
            }

            allPathSets.push_back(Scanner::PointerScanComparator::extract_paths(snapshot));
            m_logService.log_info(fmt::format("{}: Extracted {} paths from {}",
                                              MODEL_NAME, allPathSets.back().size(), file.filename().string()));
        }

        stablePaths = Scanner::PointerScanComparator::find_stable_paths(allPathSets);

        m_logService.log_info(fmt::format("{}: Found {} stable paths across {} scans",
                                          MODEL_NAME, stablePaths.size(), allPathSets.size()));

        return StatusCode::STATUS_OK;
    }

    void PointerScanModel::ensure_memory_reader_initialized() const
    {
        if (!m_pointerScanner.has_memory_reader())
        {
            auto reader = std::make_shared<Scanner::PluginMemoryReader>(m_loaderService);
            m_pointerScanner.set_memory_reader(std::move(reader));
        }
    }

    bool PointerScanModel::has_pointer_scan_memory_attribute_overrides() const
    {
        const auto protections = m_pluginConfig.get_enabled_memory_attributes("pointerScanMemoryAttributes", "protections");
        const auto states = m_pluginConfig.get_enabled_memory_attributes("pointerScanMemoryAttributes", "states");
        const auto types = m_pluginConfig.get_enabled_memory_attributes("pointerScanMemoryAttributes", "types");
        return !protections.empty() || !states.empty() || !types.empty();
    }

    StatusCode PointerScanModel::apply_pointer_scan_memory_attributes() const
    {
        const bool hasOverrides = has_pointer_scan_memory_attribute_overrides();
        const auto* configSection = hasOverrides ? "pointerScanMemoryAttributes" : "memoryAttributes";

        MemoryAttributeModel attributeModel{
            m_loaderService,
            m_pluginConfig,
            m_dispatcher,
            configSection,
            !hasOverrides};

        std::vector<MemoryAttributeOptionData> options{};
        const auto fetchStatus = attributeModel.fetch_memory_attribute_options(options);
        if (fetchStatus == StatusCode::STATUS_ERROR_PLUGIN_NO_MEMORY_ATTRIBUTE_OPTIONS ||
            fetchStatus == StatusCode::STATUS_ERROR_PLUGIN_FUNCTION_NOT_IMPLEMENTED ||
            fetchStatus == StatusCode::STATUS_ERROR_FUNCTION_NOT_FOUND)
        {
            m_logService.log_info(fmt::format(
                "{}: Pointer-scan memory attribute options unavailable; using plugin defaults",
                MODEL_NAME));
            return StatusCode::STATUS_OK;
        }

        if (fetchStatus != StatusCode::STATUS_OK)
        {
            return fetchStatus;
        }

        std::packaged_task<StatusCode()> applyTask{
            [options]() -> StatusCode
            {
                for (const auto& option : options)
                {
                    if (!option.stateFunction)
                    {
                        continue;
                    }

                    option.stateFunction(static_cast<std::uint8_t>(option.currentState));
                }

                return StatusCode::STATUS_OK;
            }};

        auto dispatchResult = m_dispatcher.dispatch(
            Thread::ThreadChannel::ProcessList,
            std::move(applyTask));
        if (!dispatchResult.has_value())
        {
            return dispatchResult.error();
        }

        return dispatchResult.value().get();
    }

    StatusCode PointerScanModel::query_memory_regions(std::vector<Scanner::ScanRegion>& regions) const
    {
        if (m_loaderService.has_plugin_loaded() != StatusCode::STATUS_OK)
        {
            m_logService.log_error(fmt::format("{}: No active plugin", MODEL_NAME));
            return StatusCode::STATUS_ERROR_PLUGIN_NOT_ACTIVE;
        }

        MemoryRegion* internalRegions{};
        std::uint64_t internalRegionsSize{};
        StatusCode queryStatus{};

        std::packaged_task<StatusCode()> task(
            [this, &internalRegions, &internalRegionsSize, &queryStatus]() -> StatusCode
            {
                auto pluginRef = m_loaderService.get_active_plugin().value();
                auto& plugin = pluginRef.get();

                const auto result = Runtime::safe_call(plugin.internal_vertex_memory_query_regions, &internalRegions, &internalRegionsSize);
                queryStatus = Runtime::get_status(result);
                if (queryStatus == StatusCode::STATUS_ERROR_FUNCTION_NOT_FOUND)
                {
                    m_logService.log_error(fmt::format("{}: vertex_memory_query_regions not implemented", MODEL_NAME));
                    return StatusCode::STATUS_ERROR_PLUGIN_FUNCTION_NOT_IMPLEMENTED;
                }
                if (!Runtime::status_ok(result))
                {
                    return queryStatus;
                }
                return StatusCode::STATUS_OK;
            });

        auto dispatchResult = m_dispatcher.dispatch(Thread::ThreadChannel::Scanner, std::move(task));
        if (!dispatchResult.has_value())
        {
            return dispatchResult.error();
        }

        const auto status = dispatchResult.value().get();
        if (status != StatusCode::STATUS_OK)
        {
            return status;
        }

        const auto excludedModules = m_pluginConfig.get_excluded_modules();
        const std::unordered_set<std::string> excludedSet{excludedModules.begin(), excludedModules.end()};

        std::span rawRegions{internalRegions, static_cast<std::size_t>(internalRegionsSize)};
        regions = rawRegions
            | std::views::transform([](const MemoryRegion& region)
            {
                return Scanner::ScanRegion{
                    .moduleName = region.baseModuleName ? region.baseModuleName : std::string{},
                    .baseAddress = region.baseAddress,
                    .size = region.regionSize
                };
            })
            | std::views::filter([&excludedSet](const Scanner::ScanRegion& region)
            {
                return region.moduleName.empty() || !excludedSet.contains(region.moduleName);
            })
            | std::ranges::to<std::vector>();

        m_logService.log_info(fmt::format("{}: Queried {} memory regions ({} modules excluded)",
                                          MODEL_NAME, regions.size(), excludedSet.size()));
        return StatusCode::STATUS_OK;
    }
}
