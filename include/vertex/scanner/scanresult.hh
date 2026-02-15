//
// Copyright (C) 2026 PHTNC<>.
// Licensed under GPLv3.0 with Plugin Interface exceptions.
//
#pragma once

#include <vertex/memory/scannerallocator.hh>
#include <cstring>
#include <vector>

namespace Vertex::Scanner
{
    struct ScanResult final
    {
        Memory::AlignedByteVector records{};

        std::uint64_t matchesFound{};
        std::size_t valueSize{};
        std::size_t firstValueSize{};
        std::size_t recordSize{};
        std::size_t m_writePos{};

        void reserve(std::size_t count, std::size_t valSize, std::size_t firstValSize = 0)
        {
            valueSize = valSize;
            firstValueSize = firstValSize;
            recordSize = sizeof(std::uint64_t) + valSize + firstValSize;
            records.resize(count * recordSize);
            m_writePos = 0;
            matchesFound = 0;
        }

        void add_match(std::uint64_t address, const std::uint8_t* value, std::size_t valSize,
                       const std::uint8_t* firstVal = nullptr, std::size_t firstValSize = 0)
        {
            const std::size_t needed = sizeof(std::uint64_t) + valSize + firstValSize;
            if (m_writePos + needed > records.size()) [[unlikely]]
            {
                records.resize(records.size() + records.size() / 2 + needed);
            }

            auto* dest = records.data() + m_writePos;
            std::memcpy(dest, &address, sizeof(std::uint64_t));
            dest += sizeof(std::uint64_t);
            std::memcpy(dest, value, valSize);
            if (firstVal && firstValSize > 0)
            {
                std::memcpy(dest + valSize, firstVal, firstValSize);
            }
            m_writePos += needed;
            ++matchesFound;
        }

        void clear() noexcept
        {
            m_writePos = 0;
            matchesFound = 0;
        }

        [[nodiscard]] const std::uint8_t* get_value_at(std::size_t index) const
        {
            if (recordSize == 0 || index >= matchesFound)
            {
                return nullptr;
            }
            return reinterpret_cast<const std::uint8_t*>(
                records.data() + (index * recordSize) + sizeof(std::uint64_t));
        }

        [[nodiscard]] const char* data() const noexcept
        {
            return records.data();
        }

        [[nodiscard]] std::size_t total_data_size() const noexcept
        {
            return m_writePos;
        }
    };
}
