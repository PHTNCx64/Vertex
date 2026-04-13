//
// Copyright (C) 2026 PHTNC<>.
// Licensed under GPLv3.0 with Plugin Interface exceptions.
//
#pragma once

#include <vertex/scanner/pointerscanner/pointerscannerconfig.hh>

#include <array>
#include <bit>
#include <cstddef>
#include <cstdint>
#include <ios>
#include <istream>
#include <limits>
#include <ostream>
#include <type_traits>

namespace Vertex::Scanner
{
    static constexpr std::array<char, 4> VXP_MAGIC{{'V', 'X', 'P', '\0'}};

    static constexpr std::uint32_t VXP_VERSION_1{1};
    static constexpr std::uint32_t VXP_CURRENT_VERSION{VXP_VERSION_1};

    static constexpr std::size_t VXP_FILE_HEADER_SIZE_V1{100};
    static constexpr std::size_t VXP_MODULE_HEADER_SIZE_V1{22};
    static constexpr std::size_t VXP_FILE_PREFIX_SIZE{VXP_MAGIC.size() + sizeof(std::uint32_t)};

    enum class VxpEndianness : std::uint8_t
    {
        Little = 0,
        Big = 1
    };

    [[nodiscard]] consteval VxpEndianness native_vxp_endianness() noexcept
    {
        if constexpr (std::endian::native == std::endian::little)
        {
            return VxpEndianness::Little;
        }
        else
        {
            return VxpEndianness::Big;
        }
    }

    // Keep an explicit config snapshot to avoid coupling file format to PointerScanConfig ABI.
    struct VxpConfigSnapshot final
    {
        std::uint64_t targetAddress{};
        std::uint64_t maxOffset{};
        std::uint64_t maxNodes{};
        std::uint64_t maxEdges{};
        std::uint32_t maxDepth{};
        std::uint32_t alignment{};
        std::uint32_t maxParentsPerNode{};
        std::uint32_t workerChunkSize{};
        std::uint8_t allowNegativeOffsets{};
        std::uint8_t staticRootsOnly{};
        std::uint8_t reserved[6]{};
    };

    [[nodiscard]] inline VxpConfigSnapshot config_to_snapshot(const PointerScanConfig& config) noexcept
    {
        return VxpConfigSnapshot{
            .targetAddress = config.targetAddress,
            .maxOffset = config.maxOffset,
            .maxNodes = config.maxNodes,
            .maxEdges = config.maxEdges,
            .maxDepth = config.maxDepth,
            .alignment = config.alignment,
            .maxParentsPerNode = config.maxParentsPerNode,
            .workerChunkSize = config.workerChunkSize,
            .allowNegativeOffsets = static_cast<std::uint8_t>(config.allowNegativeOffsets),
            .staticRootsOnly = static_cast<std::uint8_t>(config.staticRootsOnly),
        };
    }

    [[nodiscard]] inline PointerScanConfig snapshot_to_config(const VxpConfigSnapshot& snapshot) noexcept
    {
        return PointerScanConfig{
            .targetAddress = snapshot.targetAddress,
            .maxDepth = snapshot.maxDepth,
            .maxOffset = snapshot.maxOffset,
            .alignment = snapshot.alignment,
            .allowNegativeOffsets = snapshot.allowNegativeOffsets != 0,
            .staticRootsOnly = snapshot.staticRootsOnly != 0,
            .maxParentsPerNode = snapshot.maxParentsPerNode,
            .maxNodes = snapshot.maxNodes,
            .maxEdges = snapshot.maxEdges,
            .workerChunkSize = snapshot.workerChunkSize,
        };
    }

    struct VxpFileHeader final
    {
        std::array<char, 4> magic{VXP_MAGIC};
        std::uint32_t version{VXP_CURRENT_VERSION};
        VxpEndianness endianness{native_vxp_endianness()};
        std::uint8_t pointerSize{};
        std::uint8_t reserved0[2]{};
        std::uint64_t configSnapshotHash{};
        std::uint32_t moduleCount{};
        std::uint32_t moduleNameBytes{};
        std::uint64_t nodeCount{};
        std::uint64_t edgeCount{};
        VxpConfigSnapshot config{};
    };

    struct VxpModuleEntryHeader final
    {
        std::uint32_t moduleId{};
        std::uint64_t moduleBase{};
        std::uint64_t moduleSpan{};
        std::uint16_t nameLength{};
    };

    [[nodiscard]] inline std::size_t serialized_vxp_file_header_size(const std::uint32_t version) noexcept
    {
        return version == VXP_CURRENT_VERSION ? VXP_FILE_HEADER_SIZE_V1 : 0;
    }

    [[nodiscard]] inline std::size_t serialized_vxp_module_header_size(const std::uint32_t version) noexcept
    {
        return version == VXP_CURRENT_VERSION ? VXP_MODULE_HEADER_SIZE_V1 : 0;
    }

    [[nodiscard]] inline bool write_exact(std::ostream& stream, const void* data, const std::size_t bytes)
    {
        stream.write(static_cast<const char*>(data), static_cast<std::streamsize>(bytes));
        return stream.good();
    }

    [[nodiscard]] inline bool read_exact(std::istream& stream, void* data, const std::size_t bytes)
    {
        stream.read(static_cast<char*>(data), static_cast<std::streamsize>(bytes));
        return stream.good();
    }

    template <class T>
    [[nodiscard]] bool write_trivial(std::ostream& stream, const T& value)
    {
        static_assert(std::is_trivially_copyable_v<T>);
        return write_exact(stream, &value, sizeof(T));
    }

    template <class T>
    [[nodiscard]] bool read_trivial(std::istream& stream, T& value)
    {
        static_assert(std::is_trivially_copyable_v<T>);
        return read_exact(stream, &value, sizeof(T));
    }

    [[nodiscard]] inline bool write_vxp_config_snapshot(std::ostream& stream, const VxpConfigSnapshot& snapshot)
    {
        return write_trivial(stream, snapshot.targetAddress) &&
               write_trivial(stream, snapshot.maxOffset) &&
               write_trivial(stream, snapshot.maxNodes) &&
               write_trivial(stream, snapshot.maxEdges) &&
               write_trivial(stream, snapshot.maxDepth) &&
               write_trivial(stream, snapshot.alignment) &&
               write_trivial(stream, snapshot.maxParentsPerNode) &&
               write_trivial(stream, snapshot.workerChunkSize) &&
               write_trivial(stream, snapshot.allowNegativeOffsets) &&
               write_trivial(stream, snapshot.staticRootsOnly) &&
               write_exact(stream, snapshot.reserved, sizeof(snapshot.reserved));
    }

    [[nodiscard]] inline bool read_vxp_config_snapshot(std::istream& stream, VxpConfigSnapshot& snapshot)
    {
        return read_trivial(stream, snapshot.targetAddress) &&
               read_trivial(stream, snapshot.maxOffset) &&
               read_trivial(stream, snapshot.maxNodes) &&
               read_trivial(stream, snapshot.maxEdges) &&
               read_trivial(stream, snapshot.maxDepth) &&
               read_trivial(stream, snapshot.alignment) &&
               read_trivial(stream, snapshot.maxParentsPerNode) &&
               read_trivial(stream, snapshot.workerChunkSize) &&
               read_trivial(stream, snapshot.allowNegativeOffsets) &&
               read_trivial(stream, snapshot.staticRootsOnly) &&
               read_exact(stream, snapshot.reserved, sizeof(snapshot.reserved));
    }

    [[nodiscard]] inline bool write_vxp_file_header(std::ostream& stream, const VxpFileHeader& header)
    {
        if (header.version != VXP_CURRENT_VERSION)
        {
            return false;
        }

        const auto endianness = static_cast<std::uint8_t>(header.endianness);

        return write_exact(stream, header.magic.data(), header.magic.size()) &&
               write_trivial(stream, header.version) &&
               write_trivial(stream, endianness) &&
               write_trivial(stream, header.pointerSize) &&
               write_exact(stream, header.reserved0, sizeof(header.reserved0)) &&
               write_trivial(stream, header.configSnapshotHash) &&
               write_trivial(stream, header.moduleCount) &&
               write_trivial(stream, header.moduleNameBytes) &&
               write_trivial(stream, header.nodeCount) &&
               write_trivial(stream, header.edgeCount) &&
               write_vxp_config_snapshot(stream, header.config);
    }

    [[nodiscard]] inline bool read_vxp_file_header(std::istream& stream, VxpFileHeader& header)
    {
        header = {};

        if (!read_exact(stream, header.magic.data(), header.magic.size()) ||
            !read_trivial(stream, header.version))
        {
            return false;
        }

        if (header.version != VXP_CURRENT_VERSION)
        {
            return false;
        }

        std::uint8_t endianness{};
        if (!read_trivial(stream, endianness) ||
            !read_trivial(stream, header.pointerSize) ||
            !read_exact(stream, header.reserved0, sizeof(header.reserved0)) ||
            !read_trivial(stream, header.configSnapshotHash) ||
            !read_trivial(stream, header.moduleCount) ||
            !read_trivial(stream, header.moduleNameBytes) ||
            !read_trivial(stream, header.nodeCount) ||
            !read_trivial(stream, header.edgeCount) ||
            !read_vxp_config_snapshot(stream, header.config))
        {
            return false;
        }

        header.endianness = static_cast<VxpEndianness>(endianness);
        return true;
    }

    [[nodiscard]] inline bool write_vxp_module_entry_header(std::ostream& stream,
                                                            const std::uint32_t version,
                                                            const VxpModuleEntryHeader& header)
    {
        if (version != VXP_CURRENT_VERSION)
        {
            return false;
        }

        return write_trivial(stream, header.moduleId) &&
               write_trivial(stream, header.moduleBase) &&
               write_trivial(stream, header.moduleSpan) &&
               write_trivial(stream, header.nameLength);
    }

    [[nodiscard]] inline bool write_vxp_module_entry_header(std::ostream& stream, const VxpModuleEntryHeader& header)
    {
        return write_vxp_module_entry_header(stream, VXP_CURRENT_VERSION, header);
    }

    [[nodiscard]] inline bool read_vxp_module_entry_header(std::istream& stream,
                                                           const std::uint32_t version,
                                                           VxpModuleEntryHeader& header)
    {
        header = {};
        if (version != VXP_CURRENT_VERSION)
        {
            return false;
        }

        return read_trivial(stream, header.moduleId) &&
               read_trivial(stream, header.moduleBase) &&
               read_trivial(stream, header.moduleSpan) &&
               read_trivial(stream, header.nameLength);
    }

    [[nodiscard]] inline std::uint64_t compute_config_hash(const PointerScanConfig& config) noexcept
    {
        std::uint64_t hash{14695981039346656037ULL};

        const auto mix = [&hash](const std::uint64_t value)
        {
            hash ^= value;
            hash *= 1099511628211ULL;
        };

        mix(config.targetAddress);
        mix(config.maxDepth);
        mix(config.maxOffset);
        mix(config.alignment);
        mix(config.allowNegativeOffsets);
        mix(config.staticRootsOnly);
        mix(config.maxParentsPerNode);
        mix(config.maxNodes);
        mix(config.maxEdges);
        mix(config.workerChunkSize);

        return hash;
    }

    [[nodiscard]] inline std::uint64_t compute_minimum_file_size(const VxpFileHeader& header) noexcept
    {
        const auto fileHeaderSize = serialized_vxp_file_header_size(header.version);
        const auto moduleHeaderSize = serialized_vxp_module_header_size(header.version);
        if (fileHeaderSize == 0 || moduleHeaderSize == 0) // NOLINT
        {
            return std::numeric_limits<std::uint64_t>::max();
        }

        const auto nodeBytes = header.nodeCount * sizeof(PointerNodeRecord);
        const auto edgeBytes = header.edgeCount * sizeof(PointerEdgeRecord);
        const auto parentRangeBytes = header.nodeCount * sizeof(ParentEdgeRange);
        const auto moduleHeaderBytes = static_cast<std::uint64_t>(header.moduleCount) * moduleHeaderSize;
        const auto moduleNameBytes = static_cast<std::uint64_t>(header.moduleNameBytes);

        return static_cast<std::uint64_t>(fileHeaderSize) + moduleHeaderBytes + moduleNameBytes + nodeBytes + edgeBytes + parentRangeBytes;
    }

    [[nodiscard]] inline bool validate_vxp_header(const VxpFileHeader& header, const std::uint64_t fileSize) noexcept
    {
        if (header.magic != VXP_MAGIC)
        {
            return false;
        }
        if (header.version != VXP_CURRENT_VERSION)
        {
            return false;
        }
        if (header.endianness != VxpEndianness::Little && header.endianness != VxpEndianness::Big)
        {
            return false;
        }
        if (header.endianness != native_vxp_endianness())
        {
            return false;
        }
        if (fileSize < serialized_vxp_file_header_size(header.version))
        {
            return false;
        }
        if (header.pointerSize == 0)
        {
            return false;
        }
        if (header.config.maxDepth == 0 || header.config.maxOffset == 0 || header.config.alignment == 0)
        {
            return false;
        }
        if (header.config.maxOffset > static_cast<std::uint64_t>(std::numeric_limits<std::int32_t>::max()))
        {
            return false;
        }
        if (header.config.targetAddress == 0)
        {
            return false;
        }
        if (header.nodeCount > 0 && header.edgeCount == 0 && header.nodeCount > 1)
        {
            return false;
        }
        if (header.edgeCount > 0 && header.nodeCount < 2)
        {
            return false;
        }
        if (header.nodeCount > std::numeric_limits<std::uint32_t>::max() ||
            header.edgeCount > std::numeric_limits<std::uint32_t>::max())
        {
            return false;
        }

        if (header.moduleCount == 0 && header.moduleNameBytes != 0)
        {
            return false;
        }

        constexpr auto MAX_MODULE_NAME_BYTES = static_cast<std::uint64_t>(std::numeric_limits<std::uint16_t>::max());
        const auto maxPossibleNameBytes = static_cast<std::uint64_t>(header.moduleCount) * MAX_MODULE_NAME_BYTES;

        if (static_cast<std::uint64_t>(header.moduleNameBytes) > maxPossibleNameBytes)
        {
            return false;
        }

        const auto expectedHash = compute_config_hash(snapshot_to_config(header.config));
        if (header.configSnapshotHash != expectedHash)
        {
            return false;
        }
        if (fileSize < compute_minimum_file_size(header))
        {
            return false;
        }
        return true;
    }
} // namespace Vertex::Scanner
