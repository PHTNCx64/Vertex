//
// Copyright (C) 2026 PHTNC<>.
// Licensed under GPLv3.0 with Plugin Interface exceptions.
//
#pragma once

#include <sdk/statuscode.h>
#include <vertex/scanner/pointerscanner/pointerscannerconfig.hh>

#include <cstdint>
#include <filesystem>
#include <limits>
#include <span>
#include <string>
#include <unordered_set>
#include <vector>

namespace Vertex::Scanner
{
    class IPointerScanner;

    struct PointerPathSignature final
    {
        std::string rootModuleName{};
        std::uint64_t rootModuleOffset{};
        std::vector<std::int32_t> offsets{};

        [[nodiscard]] bool operator==(const PointerPathSignature&) const = default;
    };

    struct PointerPathSignatureHash final
    {
        [[nodiscard]] std::size_t operator()(const PointerPathSignature& sig) const noexcept;
    };

    struct GraphSnapshot final
    {
        std::vector<PointerNodeRecord> nodes{};
        std::vector<PointerEdgeRecord> edges{};
        std::vector<ParentEdgeRange> parentRanges{};
        std::vector<ModuleRecord> modules{};
        std::vector<std::uint32_t> rootNodeIds{};
        PointerScanConfig config{};
    };

    class PointerScanComparator final
    {
    public:
        static constexpr std::uint32_t NO_MODULE{std::numeric_limits<std::uint32_t>::max()};
        static constexpr std::size_t DEFAULT_MAX_PATHS{500'000};

        [[nodiscard]] static StatusCode load_snapshot(
            const std::filesystem::path& filePath,
            GraphSnapshot& snapshot);

        [[nodiscard]] static std::vector<PointerPathSignature> extract_paths(
            const IPointerScanner& scanner,
            std::size_t maxPaths = DEFAULT_MAX_PATHS);

        [[nodiscard]] static std::vector<PointerPathSignature> extract_paths(
            const GraphSnapshot& snapshot,
            std::size_t maxPaths = DEFAULT_MAX_PATHS);

        [[nodiscard]] static std::vector<PointerPathSignature> find_stable_paths(
            std::span<const std::vector<PointerPathSignature>> allPathSets);

        [[nodiscard]] static bool modules_compatible(
            std::span<const ModuleRecord> lhs,
            std::span<const ModuleRecord> rhs);

    private:
        static void extract_paths_dfs(
            const GraphSnapshot& snapshot,
            std::uint32_t nodeId,
            const std::string& rootModuleName,
            std::uint64_t rootModuleOffset,
            std::vector<std::int32_t>& currentOffsets,
            std::vector<PointerPathSignature>& results,
            std::size_t maxPaths,
            std::vector<bool>& pathVisited,
            std::unordered_set<PointerPathSignature, PointerPathSignatureHash>& uniqueSignatures);

        [[nodiscard]] static const ModuleRecord* find_module_by_id(
            const std::vector<ModuleRecord>& modules,
            std::uint32_t moduleId);

        static void identify_roots(GraphSnapshot& snapshot);
    };
} // namespace Vertex::Scanner
