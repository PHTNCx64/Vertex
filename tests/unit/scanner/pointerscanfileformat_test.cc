#include <gtest/gtest.h>

#include <vertex/scanner/pointerscanner/pointerscanfileformat.hh>

#include <array>
#include <cstdint>
#include <sstream>

namespace
{
    using namespace Vertex::Scanner;

    TEST(PointerScanFileFormatTest, V1HeaderRoundTripUsesStableWireLayout)
    {
        VxpFileHeader header{};
        header.version = VXP_VERSION_1;
        header.endianness = native_vxp_endianness();
        header.pointerSize = 8;
        header.reserved0[0] = 0xAA;
        header.reserved0[1] = 0xBB;
        header.configSnapshotHash = 0x1122334455667788ULL;
        header.moduleCount = 7;
        header.moduleNameBytes = 99;
        header.nodeCount = 123;
        header.edgeCount = 456;
        header.config = VxpConfigSnapshot{
            .targetAddress = 0x1000,
            .maxOffset = 0x2000,
            .maxNodes = 321,
            .maxEdges = 654,
            .maxDepth = 5,
            .alignment = 8,
            .maxParentsPerNode = 4096,
            .workerChunkSize = 8192,
            .allowNegativeOffsets = 1,
            .staticRootsOnly = 0,
            .reserved = {1, 2, 3, 4, 5, 6}};

        std::stringstream stream{std::ios::in | std::ios::out | std::ios::binary};
        ASSERT_TRUE(write_vxp_file_header(stream, header));
        ASSERT_EQ(static_cast<std::streamoff>(VXP_FILE_HEADER_SIZE_V1), stream.tellp());

        stream.seekg(0, std::ios::beg);
        VxpFileHeader decoded{};
        ASSERT_TRUE(read_vxp_file_header(stream, decoded));

        EXPECT_EQ(header.magic, decoded.magic);
        EXPECT_EQ(header.version, decoded.version);
        EXPECT_EQ(header.endianness, decoded.endianness);
        EXPECT_EQ(header.pointerSize, decoded.pointerSize);
        EXPECT_EQ(header.reserved0[0], decoded.reserved0[0]);
        EXPECT_EQ(header.reserved0[1], decoded.reserved0[1]);
        EXPECT_EQ(header.configSnapshotHash, decoded.configSnapshotHash);
        EXPECT_EQ(header.moduleCount, decoded.moduleCount);
        EXPECT_EQ(header.moduleNameBytes, decoded.moduleNameBytes);
        EXPECT_EQ(header.nodeCount, decoded.nodeCount);
        EXPECT_EQ(header.edgeCount, decoded.edgeCount);
        EXPECT_EQ(header.config.targetAddress, decoded.config.targetAddress);
        EXPECT_EQ(header.config.maxOffset, decoded.config.maxOffset);
        EXPECT_EQ(header.config.maxNodes, decoded.config.maxNodes);
        EXPECT_EQ(header.config.maxEdges, decoded.config.maxEdges);
        EXPECT_EQ(header.config.maxDepth, decoded.config.maxDepth);
        EXPECT_EQ(header.config.alignment, decoded.config.alignment);
        EXPECT_EQ(header.config.maxParentsPerNode, decoded.config.maxParentsPerNode);
        EXPECT_EQ(header.config.workerChunkSize, decoded.config.workerChunkSize);
        EXPECT_EQ(header.config.allowNegativeOffsets, decoded.config.allowNegativeOffsets);
        EXPECT_EQ(header.config.staticRootsOnly, decoded.config.staticRootsOnly);
        EXPECT_EQ(header.config.reserved[0], decoded.config.reserved[0]);
        EXPECT_EQ(header.config.reserved[1], decoded.config.reserved[1]);
        EXPECT_EQ(header.config.reserved[2], decoded.config.reserved[2]);
        EXPECT_EQ(header.config.reserved[3], decoded.config.reserved[3]);
        EXPECT_EQ(header.config.reserved[4], decoded.config.reserved[4]);
        EXPECT_EQ(header.config.reserved[5], decoded.config.reserved[5]);
    }

    TEST(PointerScanFileFormatTest, V1ModuleHeaderRoundTripPersistsModuleSpan)
    {
        VxpModuleEntryHeader header{
            .moduleId = 42,
            .moduleBase = 0xABCDEF123456ULL,
            .moduleSpan = 0x8765,
            .nameLength = 17};

        std::stringstream stream{std::ios::in | std::ios::out | std::ios::binary};
        ASSERT_TRUE(write_vxp_module_entry_header(stream, VXP_VERSION_1, header));
        ASSERT_EQ(static_cast<std::streamoff>(VXP_MODULE_HEADER_SIZE_V1), stream.tellp());

        stream.seekg(0, std::ios::beg);
        VxpModuleEntryHeader decoded{};
        ASSERT_TRUE(read_vxp_module_entry_header(stream, VXP_VERSION_1, decoded));

        EXPECT_EQ(header.moduleId, decoded.moduleId);
        EXPECT_EQ(header.moduleBase, decoded.moduleBase);
        EXPECT_EQ(header.moduleSpan, decoded.moduleSpan);
        EXPECT_EQ(header.nameLength, decoded.nameLength);
    }

    TEST(PointerScanFileFormatTest, ReadHeaderRejectsUnsupportedVersion)
    {
        std::stringstream stream{std::ios::in | std::ios::out | std::ios::binary};

        stream.write(VXP_MAGIC.data(), static_cast<std::streamsize>(VXP_MAGIC.size()));
        const std::uint32_t unsupportedVersion = 2;
        stream.write(reinterpret_cast<const char*>(&unsupportedVersion), sizeof(unsupportedVersion));
        stream.seekg(0, std::ios::beg);

        VxpFileHeader decoded{};
        EXPECT_FALSE(read_vxp_file_header(stream, decoded));
    }

    TEST(PointerScanFileFormatTest, MinimumFileSizeIncludesModuleNameBytesForV1)
    {
        VxpFileHeader header{};
        header.version = VXP_VERSION_1;
        header.moduleCount = 2;
        header.moduleNameBytes = 15;
        header.nodeCount = 3;
        header.edgeCount = 4;

        const auto expected = static_cast<std::uint64_t>(VXP_FILE_HEADER_SIZE_V1) +
                              static_cast<std::uint64_t>(header.moduleCount) * VXP_MODULE_HEADER_SIZE_V1 +
                              static_cast<std::uint64_t>(header.moduleNameBytes) +
                              static_cast<std::uint64_t>(header.nodeCount) * sizeof(PointerNodeRecord) +
                              static_cast<std::uint64_t>(header.edgeCount) * sizeof(PointerEdgeRecord) +
                              static_cast<std::uint64_t>(header.nodeCount) * sizeof(ParentEdgeRange);

        EXPECT_EQ(expected, compute_minimum_file_size(header));
    }
} // namespace
