//
// Copyright (C) 2026 PHTNC<>.
// Licensed under GPLv3.0 with Plugin Interface exceptions.
//
#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace Vertex::Scanner
{
    enum class PointerScanPhase : std::uint8_t
    {
        Idle,
        IndexBuild,
        IndexMergeSort,
        BFSDepth,
        FinalizeGraph,
        Rescan
    };

    struct OffsetEndingFilter final
    {
        std::int32_t value{};
        std::int32_t mask{};
    };

    struct PointerScanConfig final
    {
        std::uint64_t targetAddress{};
        std::uint32_t maxDepth{5};
        std::uint64_t maxOffset{0x1000};
        std::uint32_t alignment{8};
        bool allowNegativeOffsets{};
        bool staticRootsOnly{true};
        std::uint32_t maxParentsPerNode{4096};
        std::uint64_t maxNodes{20000000};
        std::uint64_t maxEdges{200000000};
        std::uint32_t workerChunkSize{8192};
        std::uint64_t scanContextHash{};
        std::vector<OffsetEndingFilter> offsetEndingFilters{};
    };

    struct ReverseIndexEntry final
    {
        std::uint64_t pointerValue{};
        std::uint64_t sourceAddress{};
        std::uint32_t moduleId{};
        std::uint32_t flags{};
    };

    static_assert(sizeof(ReverseIndexEntry) == 24);

    struct PointerNodeRecord final
    {
        std::uint64_t address{};
        std::uint32_t moduleId{};
        std::uint16_t minDepthDiscovered{};
        std::uint16_t flags{};
    };

    static_assert(sizeof(PointerNodeRecord) == 16);

    struct PointerEdgeRecord final
    {
        std::uint32_t parentNodeId{};
        std::uint32_t childNodeId{};
        std::int32_t offset{};
        std::uint32_t flags{};
    };

    static_assert(sizeof(PointerEdgeRecord) == 16);

    struct ParentEdgeRange final
    {
        std::uint32_t firstEdgeIndex{};
        std::uint32_t count{};
    };

    static_assert(sizeof(ParentEdgeRange) == 8);

    struct PointerScanProgress final
    {
        PointerScanPhase phase{PointerScanPhase::Idle};
        std::uint64_t current{};
        std::uint64_t total{};
        std::uint32_t currentDepth{};
        std::uint32_t maxDepth{};
    };

    struct ModuleRecord final
    {
        std::uint32_t moduleId{};
        std::uint64_t moduleBase{};
        std::uint64_t moduleSpan{};
        std::string moduleName{};
    };

    struct IndexSignature final
    {
        std::uint64_t pointerSize{};
        std::uint32_t alignment{};
        std::uint64_t regionHash{};
        std::uint64_t contextHash{};

        [[nodiscard]] bool operator==(const IndexSignature&) const = default;
    };
} // namespace Vertex::Scanner
