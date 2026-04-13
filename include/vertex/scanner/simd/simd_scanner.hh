//
// Copyright (C) 2026 PHTNC<>.
// Licensed under GPLv3.0 with Plugin Interface exceptions.
//
#pragma once

#include <vertex/scanner/scanresult.hh>
#include <vertex/scanner/valuetypes.hh>
#include <cstddef>

namespace Vertex::Scanner::Simd
{
    inline constexpr std::size_t BATCH_CHECK_INTERVAL = 50000;

    using SimdScanFn = std::size_t(*)(
        const std::uint8_t* buffer,
        std::size_t bufferSize,
        std::size_t alignment,
        std::size_t dataSize,
        const std::uint8_t* input,
        const std::uint8_t* input2,
        ScanResult& results,
        std::uint64_t baseAddress);

    struct SimdScanCapability final
    {
        SimdScanFn scanFn{};
        bool available{};
    };

    [[nodiscard]] SimdScanCapability resolve_simd_scanner(ValueType type, NumericScanMode mode);
} // namespace Vertex::Scanner::Simd
