//
// Copyright (C) 2026 PHTNC<>.
// Licensed under GPLv3.0 with Plugin Interface exceptions.
//
#pragma once

#include <cstdint>
#include <vector>
#include <unordered_map>
#include <memory>
#include <functional>
#include <mutex>
#include <span>

#include <vertex/scanner/valuetypes.hh>
#include <vertex/scanner/imemoryreader.hh>
#include <vertex/scanner/scanner_typeschema.hh>

namespace Vertex::Scanner
{
    struct MonitoredAddress final
    {
        std::uint64_t address{};
        ValueType valueType{ValueType::Int32};
        Endianness endianness{Endianness::Little};
        std::vector<std::uint8_t> currentValue{};
        std::vector<std::uint8_t> previousValue{};
        std::vector<std::uint8_t> firstValue{};
        bool isValid{true};

        std::shared_ptr<const TypeSchema> schema{};
        std::uint32_t valueSize{};

        std::string formattedValue{};
        std::string formattedPreviousValue{};
        std::string formattedFirstValue{};
    };

    using MonitoredAddressPtr = std::shared_ptr<MonitoredAddress>;

    using MemoryReadCallback = std::function<bool(std::uint64_t address, std::size_t size, std::vector<std::uint8_t>& output)>;
    using BulkMemoryReadCallback = std::function<StatusCode(std::span<const BulkReadRequest> requests, std::span<BulkReadResult> results)>;

    class AddressMonitor final
    {
    public:
        AddressMonitor() = default;
        ~AddressMonitor() = default;

        AddressMonitor(const AddressMonitor&) = delete;
        AddressMonitor& operator=(const AddressMonitor&) = delete;
        AddressMonitor(AddressMonitor&&) = delete;
        AddressMonitor& operator=(AddressMonitor&&) = delete;

        void set_memory_reader(MemoryReadCallback reader);
        void set_bulk_memory_reader(BulkMemoryReadCallback reader);

        [[nodiscard]] MonitoredAddressPtr get_or_create(std::uint64_t address, ValueType valueType, Endianness endianness = Endianness::Little);

        [[nodiscard]] MonitoredAddressPtr get_or_create_plugin(std::uint64_t address, std::shared_ptr<const TypeSchema> schema);

        [[nodiscard]] MonitoredAddressPtr get(std::uint64_t address, ValueType valueType) const;

        void remove(std::uint64_t address, ValueType valueType);

        void refresh(const std::vector<MonitoredAddressPtr>& addresses, bool hexDisplay) const;

        void refresh_all(bool hexDisplay);

        void clear();

        [[nodiscard]] std::size_t size() const;

    private:
        using AddressKey = std::pair<std::uint64_t, std::uint32_t>;

        struct AddressKeyHash final
        {
            [[nodiscard]] std::size_t operator()(const AddressKey& key) const noexcept
            {
                const std::size_t h1 = std::hash<std::uint64_t>{}(key.first);
                const std::size_t h2 = std::hash<std::uint32_t>{}(key.second);
                return h1 ^ (h2 + 0x9e3779b97f4a7c15ULL + (h1 << 6) + (h1 >> 2));
            }
        };

        [[nodiscard]] static AddressKey make_key(std::uint64_t address, ValueType valueType);
        [[nodiscard]] static AddressKey make_key(std::uint64_t address, TypeId typeId);

        static void update_formatted_values(MonitoredAddress& entry, bool hexDisplay);

        mutable std::mutex m_mutex;
        std::unordered_map<AddressKey, MonitoredAddressPtr, AddressKeyHash> m_registry;
        MemoryReadCallback m_memoryReader;
        BulkMemoryReadCallback m_bulkMemoryReader;
    };

} // namespace Vertex::Scanner
