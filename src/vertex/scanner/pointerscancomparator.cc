//
// Copyright (C) 2026 PHTNC<>.
// Licensed under GPLv3.0 with Plugin Interface exceptions.
//
#include <vertex/scanner/pointerscanner/pointerscancomparator.hh>
#include <vertex/scanner/pointerscanner/pointerscanfileformat.hh>
#include <vertex/scanner/pointerscanner/ipointerscanner.hh>

#include <algorithm>
#include <fstream>
#include <numeric>
#include <string_view>
#include <unordered_map>
#include <unordered_set>
#include <fmt/format.h>

namespace Vertex::Scanner
{
    std::size_t PointerPathSignatureHash::operator()(const PointerPathSignature& sig) const noexcept
    {
        constexpr std::uint64_t INITIAL_HASH = 0x9E3779B97F4A7C15ULL;
        constexpr std::uint64_t SEED_BIAS = 0x9E3779B97F4A7C15ULL;

        const auto avalanche = [](std::uint64_t value) -> std::uint64_t
        {
            value ^= value >> 30;
            value *= 0xBF58476D1CE4E5B9ULL;
            value ^= value >> 27;
            value *= 0x94D049BB133111EBULL;
            value ^= value >> 31;
            return value;
        };

        std::uint64_t hash{INITIAL_HASH};

        const auto mix = [&hash, &avalanche](const std::uint64_t value)
        {
            hash ^= avalanche(value + SEED_BIAS + (hash << 6) + (hash >> 2));
            hash = avalanche(hash);
        };

        for (const auto ch : sig.rootModuleName)
        {
            mix(static_cast<std::uint64_t>(static_cast<unsigned char>(ch)));
        }

        mix(sig.rootModuleOffset);
        mix(sig.offsets.size());

        for (const auto offset : sig.offsets)
        {
            mix(static_cast<std::uint64_t>(static_cast<std::uint32_t>(offset)));
        }

        return static_cast<std::size_t>(hash);
    }

    StatusCode PointerScanComparator::load_snapshot(
        const std::filesystem::path& filePath,
        GraphSnapshot& snapshot)
    {
        std::ifstream file{filePath, std::ios::binary | std::ios::ate};
        if (!file.is_open())
        {
            return StatusCode::STATUS_ERROR_FILE_NOT_FOUND;
        }

        const auto fileSize = static_cast<std::uint64_t>(file.tellg());
        file.seekg(0, std::ios::beg);

        if (fileSize < VXP_FILE_PREFIX_SIZE)
        {
            return StatusCode::STATUS_ERROR_FILE_READ_FAILED;
        }

        VxpFileHeader header{};
        if (!read_vxp_file_header(file, header))
        {
            return StatusCode::STATUS_ERROR_FILE_READ_FAILED;
        }

        if (!validate_vxp_header(header, fileSize))
        {
            return StatusCode::STATUS_ERROR_FILE_READ_FAILED;
        }

        snapshot = {};
        snapshot.config = snapshot_to_config(header.config);
        snapshot.modules.reserve(header.moduleCount);
        std::uint64_t moduleNameBytesRead{};

        for (std::uint32_t i{}; i < header.moduleCount; ++i)
        {
            VxpModuleEntryHeader moduleHeader{};
            if (!read_vxp_module_entry_header(file, header.version, moduleHeader))
            {
                return StatusCode::STATUS_ERROR_FILE_READ_FAILED;
            }

            std::string moduleName(moduleHeader.nameLength, '\0');
            file.read(moduleName.data(), moduleHeader.nameLength);
            if (!file.good())
            {
                return StatusCode::STATUS_ERROR_FILE_READ_FAILED;
            }
            moduleNameBytesRead += moduleHeader.nameLength;

            snapshot.modules.emplace_back(ModuleRecord{
                .moduleId = moduleHeader.moduleId,
                .moduleBase = moduleHeader.moduleBase,
                .moduleSpan = moduleHeader.moduleSpan,
                .moduleName = std::move(moduleName)});
        }

        if (moduleNameBytesRead != static_cast<std::uint64_t>(header.moduleNameBytes))
        {
            return StatusCode::STATUS_ERROR_FILE_READ_FAILED;
        }

        const auto nodeCount = static_cast<std::size_t>(header.nodeCount);
        const auto edgeCount = static_cast<std::size_t>(header.edgeCount);

        snapshot.nodes.resize(nodeCount);
        if (nodeCount > 0)
        {
            file.read(reinterpret_cast<char*>(snapshot.nodes.data()),
                       static_cast<std::streamsize>(nodeCount * sizeof(PointerNodeRecord)));
            if (!file.good())
            {
                return StatusCode::STATUS_ERROR_FILE_READ_FAILED;
            }
        }

        snapshot.edges.resize(edgeCount);
        if (edgeCount > 0)
        {
            file.read(reinterpret_cast<char*>(snapshot.edges.data()),
                       static_cast<std::streamsize>(edgeCount * sizeof(PointerEdgeRecord)));
            if (!file.good())
            {
                return StatusCode::STATUS_ERROR_FILE_READ_FAILED;
            }
        }

        snapshot.parentRanges.resize(nodeCount);
        if (nodeCount > 0)
        {
            file.read(reinterpret_cast<char*>(snapshot.parentRanges.data()),
                       static_cast<std::streamsize>(nodeCount * sizeof(ParentEdgeRange)));
            if (!file.good())
            {
                return StatusCode::STATUS_ERROR_FILE_READ_FAILED;
            }
        }

        identify_roots(snapshot);

        return StatusCode::STATUS_OK;
    }

    void PointerScanComparator::identify_roots(GraphSnapshot& snapshot)
    {
        const auto nodeCount = snapshot.nodes.size();
        if (nodeCount == 0 || nodeCount > std::numeric_limits<std::uint32_t>::max())
        {
            snapshot.rootNodeIds.clear();
            return;
        }

        const auto safeNodeCount = static_cast<std::uint32_t>(nodeCount);
        std::vector<bool> isChild(safeNodeCount, false);

        for (const auto& edge : snapshot.edges)
        {
            if (edge.childNodeId < safeNodeCount)
            {
                isChild[edge.childNodeId] = true;
            }
        }

        snapshot.rootNodeIds.clear();
        for (std::uint32_t i{1}; i < safeNodeCount; ++i)
        {
            if (isChild[i])
            {
                continue;
            }

            if (snapshot.config.staticRootsOnly && snapshot.nodes[i].moduleId == NO_MODULE)
            {
                continue;
            }

            snapshot.rootNodeIds.push_back(i);
        }

        std::ranges::sort(snapshot.rootNodeIds, [&snapshot](const std::uint32_t a, const std::uint32_t b)
                          {
                              return snapshot.nodes[a].address < snapshot.nodes[b].address;
                          });
    }

    std::vector<PointerPathSignature> PointerScanComparator::extract_paths(
        const IPointerScanner& scanner,
        const std::size_t maxPaths)
    {
        const auto rootCount = scanner.get_root_count();
        const auto nodeCount = scanner.get_node_count();
        if (rootCount == 0 || nodeCount == 0)
        {
            return {};
        }

        if (nodeCount > std::numeric_limits<std::uint32_t>::max())
        {
            return {};
        }

        GraphSnapshot snapshot{};
        snapshot.modules = scanner.get_modules();
        snapshot.config = scanner.get_config();

        if (scanner.get_nodes_range(snapshot.nodes, 0, static_cast<std::size_t>(nodeCount)) != StatusCode::STATUS_OK)
        {
            return {};
        }

        if (scanner.get_root_node_ids(snapshot.rootNodeIds, 0, static_cast<std::size_t>(rootCount)) != StatusCode::STATUS_OK)
        {
            return {};
        }

        if (scanner.get_edges_range(snapshot.edges, 0, static_cast<std::size_t>(scanner.get_edge_count())) != StatusCode::STATUS_OK)
        {
            return {};
        }

        if (scanner.get_parent_ranges(snapshot.parentRanges, 0, static_cast<std::size_t>(nodeCount)) != StatusCode::STATUS_OK)
        {
            return {};
        }

        if (snapshot.parentRanges.size() != static_cast<std::size_t>(nodeCount))
        {
            return {};
        }

        return extract_paths(snapshot, maxPaths);
    }

    std::vector<PointerPathSignature> PointerScanComparator::extract_paths(
        const GraphSnapshot& snapshot,
        const std::size_t maxPaths)
    {
        std::vector<PointerPathSignature> results{};
        std::unordered_set<PointerPathSignature, PointerPathSignatureHash> uniqueSignatures{};
        std::vector<std::int32_t> currentOffsets{};
        std::vector<bool> pathVisited(snapshot.nodes.size(), false);

        if (maxPaths > 0 && maxPaths <= uniqueSignatures.max_size())
        {
            uniqueSignatures.reserve(maxPaths);
        }

        for (const auto rootId : snapshot.rootNodeIds)
        {
            if (results.size() >= maxPaths)
            {
                break;
            }

            if (rootId >= snapshot.nodes.size())
            {
                continue;
            }

            const auto& rootNode = snapshot.nodes[rootId];

            if (rootNode.moduleId == NO_MODULE)
            {
                continue;
            }

            const auto* mod = find_module_by_id(snapshot.modules, rootNode.moduleId);
            if (!mod || mod->moduleName.empty())
            {
                continue;
            }

            const auto rootOffset = rootNode.address - mod->moduleBase;

            currentOffsets.clear();
            extract_paths_dfs(snapshot, rootId, mod->moduleName, rootOffset,
                              currentOffsets, results, maxPaths, pathVisited, uniqueSignatures);
        }

        return results;
    }

    void PointerScanComparator::extract_paths_dfs(
        const GraphSnapshot& snapshot,
        const std::uint32_t nodeId,
        const std::string& rootModuleName,
        const std::uint64_t rootModuleOffset,
        std::vector<std::int32_t>& currentOffsets,
        std::vector<PointerPathSignature>& results,
        const std::size_t maxPaths,
        std::vector<bool>& pathVisited,
        std::unordered_set<PointerPathSignature, PointerPathSignatureHash>& uniqueSignatures)
    {
        if (results.size() >= maxPaths)
        {
            return;
        }

        if (nodeId >= snapshot.parentRanges.size())
        {
            return;
        }

        if (pathVisited[nodeId])
        {
            return;
        }

        const auto& range = snapshot.parentRanges[nodeId];

        if (range.count == 0 || currentOffsets.size() >= snapshot.config.maxDepth)
        {
            PointerPathSignature sig{
                .rootModuleName = rootModuleName,
                .rootModuleOffset = rootModuleOffset,
                .offsets = currentOffsets};

            if (uniqueSignatures.insert(sig).second)
            {
                results.emplace_back(std::move(sig));
            }
            return;
        }

        const auto endIdx = static_cast<std::size_t>(range.firstEdgeIndex) + range.count;
        if (endIdx > snapshot.edges.size())
        {
            return;
        }

        pathVisited[nodeId] = true;

        for (std::size_t i = range.firstEdgeIndex; i < endIdx; ++i)
        {
            if (results.size() >= maxPaths)
            {
                break;
            }

            const auto& edge = snapshot.edges[i];
            currentOffsets.push_back(edge.offset);
            extract_paths_dfs(snapshot, edge.childNodeId, rootModuleName, rootModuleOffset,
                              currentOffsets, results, maxPaths, pathVisited, uniqueSignatures);
            currentOffsets.pop_back();
        }

        pathVisited[nodeId] = false;
    }

    std::vector<PointerPathSignature> PointerScanComparator::find_stable_paths(
        std::span<const std::vector<PointerPathSignature>> allPathSets)
    {
        if (allPathSets.empty())
        {
            return {};
        }

        if (allPathSets.size() == 1)
        {
            return allPathSets[0];
        }

        std::vector<std::size_t> order(allPathSets.size());
        std::iota(order.begin(), order.end(), 0);
        std::ranges::sort(order, [&allPathSets](const std::size_t lhs, const std::size_t rhs)
        {
            return allPathSets[lhs].size() < allPathSets[rhs].size();
        });

        std::unordered_set<PointerPathSignature, PointerPathSignatureHash> candidates{};
        candidates.max_load_factor(0.7F);
        candidates.reserve(allPathSets[order.front()].size());
        candidates.insert(allPathSets[order.front()].begin(), allPathSets[order.front()].end());

        for (std::size_t i{1}; i < order.size(); ++i)
        {
            const auto& currentPaths = allPathSets[order[i]];
            std::unordered_set<PointerPathSignature, PointerPathSignatureHash> currentSet{};
            currentSet.max_load_factor(0.7F);
            currentSet.reserve(currentPaths.size());
            currentSet.insert(currentPaths.begin(), currentPaths.end());

            std::erase_if(candidates, [&currentSet](const auto& sig)
            {
                return !currentSet.contains(sig);
            });

            if (candidates.empty())
            {
                break;
            }
        }

        std::vector<PointerPathSignature> rankedPaths{candidates.begin(), candidates.end()};
        std::ranges::sort(rankedPaths, [](const PointerPathSignature& lhs, const PointerPathSignature& rhs)
        {
            if (lhs.offsets.size() != rhs.offsets.size())
            {
                return lhs.offsets.size() < rhs.offsets.size();
            }

            if (lhs.rootModuleName != rhs.rootModuleName)
            {
                return lhs.rootModuleName < rhs.rootModuleName;
            }

            if (lhs.rootModuleOffset != rhs.rootModuleOffset)
            {
                return lhs.rootModuleOffset < rhs.rootModuleOffset;
            }

            return std::ranges::lexicographical_compare(lhs.offsets, rhs.offsets);
        });

        return rankedPaths;
    }

    bool PointerScanComparator::modules_compatible(const std::span<const ModuleRecord> lhs, const std::span<const ModuleRecord> rhs)
    {
        std::unordered_map<std::string_view, std::uint64_t> lhsByName{};
        lhsByName.reserve(lhs.size());

        for (const auto& module : lhs)
        {
            if (module.moduleName.empty())
            {
                continue;
            }

            lhsByName.insert_or_assign(module.moduleName, module.moduleSpan);
        }

        std::unordered_map<std::string_view, std::uint64_t> rhsByName{};
        rhsByName.reserve(rhs.size());

        for (const auto& module : rhs)
        {
            if (module.moduleName.empty())
            {
                continue;
            }

            rhsByName.insert_or_assign(module.moduleName, module.moduleSpan);
        }

        if (lhsByName.size() != rhsByName.size())
        {
            return false;
        }

        for (const auto& [moduleName, lhsSpan] : lhsByName)
        {
            const auto it = rhsByName.find(moduleName);
            if (it == rhsByName.end())
            {
                return false;
            }

            const auto rhsSpan = it->second;
            if (lhsSpan != rhsSpan)
            {
                return false;
            }
        }

        return true;
    }

    const ModuleRecord* PointerScanComparator::find_module_by_id(
        const std::vector<ModuleRecord>& modules,
        const std::uint32_t moduleId)
    {
        for (const auto& mod : modules)
        {
            if (mod.moduleId == moduleId)
            {
                return &mod;
            }
        }
        return nullptr;
    }
} // namespace Vertex::Scanner
