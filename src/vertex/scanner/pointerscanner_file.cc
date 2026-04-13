//
// Copyright (C) 2026 PHTNC<>.
// Licensed under GPLv3.0 with Plugin Interface exceptions.
//
#include <vertex/scanner/pointerscanner/pointerscanner.hh>
#include <vertex/scanner/pointerscanner/pointerscanfileformat.hh>

#include <algorithm>
#include <fstream>
#include <fmt/format.h>

namespace Vertex::Scanner
{
    StatusCode PointerScanner::save_graph(const std::filesystem::path& filePath) const
    {
        if (!m_scanComplete.load(std::memory_order_acquire))
        {
            return StatusCode::STATUS_ERROR_POINTER_SCAN_NOT_COMPLETE;
        }

        const auto nodeCount = m_nodes.size();
        const auto edgeCount = edge_count();

        if (nodeCount == 0)
        {
            m_logService.log_error("[PointerScanner] No graph data to save");
            return StatusCode::STATUS_ERROR_POINTER_SCAN_NO_GRAPH;
        }

        if (parent_range_count() != nodeCount)
        {
            m_logService.log_error(fmt::format("[PointerScanner] Inconsistent graph state: {} nodes but {} parent edge ranges",
                                               nodeCount, parent_range_count()));
            return StatusCode::STATUS_ERROR_GENERAL_OUT_OF_BOUNDS;
        }

        std::ofstream file{filePath, std::ios::binary | std::ios::trunc};
        if (!file.is_open())
        {
            m_logService.log_error(fmt::format("[PointerScanner] Failed to open file for writing: {}", filePath.string()));
            return StatusCode::STATUS_ERROR_FILE_CREATION_FAILED;
        }

        VxpFileHeader header{};
        header.version = VXP_CURRENT_VERSION;
        header.pointerSize = static_cast<std::uint8_t>(m_pointerSize);
        header.configSnapshotHash = compute_config_hash(m_config);
        header.moduleCount = static_cast<std::uint32_t>(m_modules.size());
        std::uint64_t moduleNameBytes{};
        for (const auto& mod : m_modules)
        {
            const auto nameLength = static_cast<std::uint64_t>(
                std::min(mod.moduleName.size(), static_cast<std::size_t>(std::numeric_limits<std::uint16_t>::max())));
            if (moduleNameBytes > std::numeric_limits<std::uint32_t>::max() - nameLength)
            {
                m_logService.log_error("[PointerScanner] Module name data exceeds vxp header capacity");
                return StatusCode::STATUS_ERROR_FILE_WRITE_FAILED;
            }
            moduleNameBytes += nameLength;
        }
        header.moduleNameBytes = static_cast<std::uint32_t>(moduleNameBytes);
        header.nodeCount = nodeCount;
        header.edgeCount = edgeCount;
        header.config = config_to_snapshot(m_config);

        if (!write_vxp_file_header(file, header))
        {
            m_logService.log_error("[PointerScanner] Failed to write file header");
            return StatusCode::STATUS_ERROR_FILE_WRITE_FAILED;
        }

        for (const auto& module : m_modules)
        {
            VxpModuleEntryHeader moduleHeader{};
            moduleHeader.moduleId = module.moduleId;
            moduleHeader.moduleBase = module.moduleBase;
            moduleHeader.moduleSpan = module.moduleSpan;
            moduleHeader.nameLength = static_cast<std::uint16_t>(
                std::min(module.moduleName.size(), static_cast<std::size_t>(std::numeric_limits<std::uint16_t>::max())));

            if (!write_vxp_module_entry_header(file, header.version, moduleHeader))
            {
                m_logService.log_error(fmt::format("[PointerScanner] Failed to write module entry header for id {}", moduleHeader.moduleId));
                return StatusCode::STATUS_ERROR_FILE_WRITE_FAILED;
            }

            file.write(module.moduleName.data(), moduleHeader.nameLength);
            if (!file.good())
            {
                m_logService.log_error(fmt::format("[PointerScanner] Failed to write module name for id {}", moduleHeader.moduleId));
                return StatusCode::STATUS_ERROR_FILE_WRITE_FAILED;
            }
        }

        file.write(reinterpret_cast<const char*>(m_nodes.data()),
                   static_cast<std::streamsize>(nodeCount * sizeof(PointerNodeRecord)));

        if (edgeCount > 0)
        {
            const auto* allEdges = edge_data();
            if (allEdges == nullptr)
            {
                m_logService.log_error("[PointerScanner] Edge storage unavailable while saving graph");
                return StatusCode::STATUS_ERROR_FILE_WRITE_FAILED;
            }

            for (std::size_t written{}; written < edgeCount;)
            {
                constexpr std::size_t EDGE_WRITE_BATCH = 16384;
                const std::size_t batchCount = std::min(EDGE_WRITE_BATCH, edgeCount - written);
                file.write(reinterpret_cast<const char*>(allEdges + written),
                           static_cast<std::streamsize>(batchCount * sizeof(PointerEdgeRecord)));
                if (!file.good())
                {
                    m_logService.log_error("[PointerScanner] Failed to write edge table");
                    return StatusCode::STATUS_ERROR_FILE_WRITE_FAILED;
                }
                written += batchCount;
            }
        }

        if (nodeCount > 0)
        {
            const auto* parentRanges = parent_range_data();
            if (parentRanges == nullptr)
            {
                m_logService.log_error("[PointerScanner] Parent-range storage unavailable while saving graph");
                return StatusCode::STATUS_ERROR_FILE_WRITE_FAILED;
            }

            for (std::size_t written{}; written < nodeCount;)
            {
                constexpr std::size_t RANGE_WRITE_BATCH = 16384;
                const std::size_t batchCount = std::min(RANGE_WRITE_BATCH, nodeCount - written);
                file.write(reinterpret_cast<const char*>(parentRanges + written),
                           static_cast<std::streamsize>(batchCount * sizeof(ParentEdgeRange)));
                if (!file.good())
                {
                    m_logService.log_error("[PointerScanner] Failed to write parent edge ranges");
                    return StatusCode::STATUS_ERROR_FILE_WRITE_FAILED;
                }
                written += batchCount;
            }
        }

        if (!file.good())
        {
            m_logService.log_error("[PointerScanner] Failed to write graph data");
            return StatusCode::STATUS_ERROR_FILE_WRITE_FAILED;
        }

        m_logService.log_info(fmt::format("[PointerScanner] Saved graph to {}: {} modules, {} nodes, {} edges",
                                          filePath.string(), m_modules.size(), nodeCount, edgeCount));

        return StatusCode::STATUS_OK;
    }

    StatusCode PointerScanner::load_graph(const std::filesystem::path& filePath)
    {
        std::scoped_lock lifecycleLock{m_scanLifecycleMutex};

        if (!drain_active_scan())
        {
            return StatusCode::STATUS_ERROR_THREAD_IS_BUSY;
        }

        std::ifstream file{filePath, std::ios::binary | std::ios::ate};
        if (!file.is_open())
        {
            m_logService.log_error(fmt::format("[PointerScanner] Failed to open file for reading: {}", filePath.string()));
            return StatusCode::STATUS_ERROR_FILE_NOT_FOUND;
        }

        const auto fileSize = static_cast<std::uint64_t>(file.tellg());
        file.seekg(0, std::ios::beg);

        if (fileSize < VXP_FILE_PREFIX_SIZE)
        {
            m_logService.log_error("[PointerScanner] File too small for header");
            return StatusCode::STATUS_ERROR_FILE_READ_FAILED;
        }

        VxpFileHeader header{};
        if (!read_vxp_file_header(file, header))
        {
            m_logService.log_error("[PointerScanner] Failed to read file header");
            return StatusCode::STATUS_ERROR_FILE_READ_FAILED;
        }

        if (!validate_vxp_header(header, fileSize))
        {
            m_logService.log_error("[PointerScanner] Invalid file header");
            return StatusCode::STATUS_ERROR_FILE_READ_FAILED;
        }

        reset_state();

        m_config = snapshot_to_config(header.config);
        m_pointerSize = header.pointerSize;

        m_modules.clear();
        m_moduleNameToId.clear();
        m_modules.reserve(header.moduleCount);
        std::uint64_t moduleNameBytesRead{};

        for (std::uint32_t i{}; i < header.moduleCount; ++i)
        {
            VxpModuleEntryHeader moduleHeader{};
            if (!read_vxp_module_entry_header(file, header.version, moduleHeader))
            {
                m_logService.log_error(fmt::format("[PointerScanner] Failed to read module entry {}", i));
                reset_state();
                return StatusCode::STATUS_ERROR_FILE_READ_FAILED;
            }

            std::string moduleName(moduleHeader.nameLength, '\0');
            file.read(moduleName.data(), moduleHeader.nameLength);
            if (!file.good())
            {
                m_logService.log_error(fmt::format("[PointerScanner] Failed to read module name {}", i));
                reset_state();
                return StatusCode::STATUS_ERROR_FILE_READ_FAILED;
            }
            moduleNameBytesRead += moduleHeader.nameLength;

            m_modules.emplace_back(ModuleRecord{
                .moduleId = moduleHeader.moduleId,
                .moduleBase = moduleHeader.moduleBase,
                .moduleSpan = moduleHeader.moduleSpan,
                .moduleName = std::move(moduleName)});
            m_moduleNameToId[m_modules.back().moduleName] = moduleHeader.moduleId;
        }

        if (moduleNameBytesRead != static_cast<std::uint64_t>(header.moduleNameBytes))
        {
            m_logService.log_error("[PointerScanner] Module name byte count mismatch in file header");
            reset_state();
            return StatusCode::STATUS_ERROR_FILE_READ_FAILED;
        }

        const auto nodeCount = static_cast<std::size_t>(header.nodeCount);
        const auto edgeCount = static_cast<std::size_t>(header.edgeCount);

        m_nodes.resize(nodeCount);
        if (nodeCount > 0)
        {
            file.read(reinterpret_cast<char*>(m_nodes.data()),
                       static_cast<std::streamsize>(nodeCount * sizeof(PointerNodeRecord)));
            if (!file.good())
            {
                m_logService.log_error("[PointerScanner] Failed to read node table");
                reset_state();
                return StatusCode::STATUS_ERROR_FILE_READ_FAILED;
            }
        }

        if (edgeCount > 0)
        {
            IO::ScanResultStore edgeStore{};
            if (const StatusCode openStatus = edgeStore.open(); openStatus != StatusCode::STATUS_OK)
            {
                m_logService.log_error("[PointerScanner] Failed to open edge store while loading graph");
                reset_state();
                return StatusCode::STATUS_ERROR_FILE_READ_FAILED;
            }

            constexpr std::size_t EDGE_READ_BATCH = 16384;
            std::vector<PointerEdgeRecord> edgeChunk(EDGE_READ_BATCH);
            std::size_t remaining = edgeCount;

            while (remaining > 0)
            {
                const std::size_t batchCount = std::min(EDGE_READ_BATCH, remaining);
                file.read(reinterpret_cast<char*>(edgeChunk.data()),
                          static_cast<std::streamsize>(batchCount * sizeof(PointerEdgeRecord)));
                if (!file.good())
                {
                    m_logService.log_error("[PointerScanner] Failed to read edge table");
                    reset_state();
                    return StatusCode::STATUS_ERROR_FILE_READ_FAILED;
                }

                if (const StatusCode appendStatus = edgeStore.append(edgeChunk.data(), batchCount * sizeof(PointerEdgeRecord));
                    appendStatus != StatusCode::STATUS_OK)
                {
                    m_logService.log_error("[PointerScanner] Failed to append edge table to store");
                    reset_state();
                    return StatusCode::STATUS_ERROR_FILE_READ_FAILED;
                }

                remaining -= batchCount;
            }

            if (const StatusCode finalizeStatus = edgeStore.finalize(); finalizeStatus != StatusCode::STATUS_OK)
            {
                m_logService.log_error("[PointerScanner] Failed to finalize edge store while loading graph");
                reset_state();
                return StatusCode::STATUS_ERROR_FILE_READ_FAILED;
            }

            m_edgesStore = std::move(edgeStore);
            m_edgeCount = edgeCount;
        }
        else
        {
            m_edgesStore = {};
            m_edgeCount = 0;
        }

        if (nodeCount > 0)
        {
            IO::ScanResultStore parentRangeStore{};
            if (const StatusCode openStatus = parentRangeStore.open(); openStatus != StatusCode::STATUS_OK)
            {
                m_logService.log_error("[PointerScanner] Failed to open parent-range store while loading graph");
                reset_state();
                return StatusCode::STATUS_ERROR_FILE_READ_FAILED;
            }

            constexpr std::size_t RANGE_READ_BATCH = 16384;
            std::vector<ParentEdgeRange> rangeChunk(RANGE_READ_BATCH);
            std::size_t remaining = nodeCount;

            while (remaining > 0)
            {
                const std::size_t batchCount = std::min(RANGE_READ_BATCH, remaining);
                file.read(reinterpret_cast<char*>(rangeChunk.data()),
                          static_cast<std::streamsize>(batchCount * sizeof(ParentEdgeRange)));
                if (!file.good())
                {
                    m_logService.log_error("[PointerScanner] Failed to read parent edge ranges");
                    reset_state();
                    return StatusCode::STATUS_ERROR_FILE_READ_FAILED;
                }

                if (const StatusCode appendStatus = parentRangeStore.append(rangeChunk.data(), batchCount * sizeof(ParentEdgeRange));
                    appendStatus != StatusCode::STATUS_OK)
                {
                    m_logService.log_error("[PointerScanner] Failed to append parent edge ranges to store");
                    reset_state();
                    return StatusCode::STATUS_ERROR_FILE_READ_FAILED;
                }

                remaining -= batchCount;
            }

            if (const StatusCode finalizeStatus = parentRangeStore.finalize(); finalizeStatus != StatusCode::STATUS_OK)
            {
                m_logService.log_error("[PointerScanner] Failed to finalize parent-range store while loading graph");
                reset_state();
                return StatusCode::STATUS_ERROR_FILE_READ_FAILED;
            }

            m_parentRangesStore = std::move(parentRangeStore);
            m_parentRangeCount = nodeCount;
        }
        else
        {
            m_parentRangesStore = {};
            m_parentRangeCount = 0;
        }

        m_nodesDiscovered.store(nodeCount, std::memory_order_release);
        m_edgesCreated.store(m_edgeCount, std::memory_order_release);

        for (std::uint32_t i{}; i < nodeCount; ++i)
        {
            const std::size_t shardIdx = (m_nodes[i].address >> 12) % VISITED_SHARD_COUNT;
            std::scoped_lock lock{m_visitedShards[shardIdx].mutex};
            m_visitedShards[shardIdx].addressToNodeId.insert_or_assign(m_nodes[i].address, i);
        }

        identify_roots();

        m_scanStatus.store(StatusCode::STATUS_OK, std::memory_order_release);
        m_scanComplete.store(true, std::memory_order_release);

        m_logService.log_info(fmt::format("[PointerScanner] Loaded graph from {}: {} modules, {} nodes, {} edges, {} roots",
                                          filePath.string(), m_modules.size(), nodeCount, edgeCount, m_rootNodeIds.size()));

        return StatusCode::STATUS_OK;
    }

    const PointerScanConfig& PointerScanner::get_config() const noexcept
    {
        return m_config;
    }
} // namespace Vertex::Scanner
