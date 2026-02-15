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

#include <vertex/scanner/valuetypes.hh>

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

        std::string formattedValue{};
        std::string formattedPreviousValue{};
        std::string formattedFirstValue{};
    };

    using MonitoredAddressPtr = std::shared_ptr<MonitoredAddress>;

    using MemoryReadCallback = std::function<bool(std::uint64_t address, std::size_t size, std::vector<std::uint8_t>& output)>;

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

        [[nodiscard]] MonitoredAddressPtr get_or_create(std::uint64_t address, ValueType valueType, Endianness endianness = Endianness::Little);

        [[nodiscard]] MonitoredAddressPtr get(std::uint64_t address, ValueType valueType) const;

        void remove(std::uint64_t address, ValueType valueType);

        void refresh(const std::vector<MonitoredAddressPtr>& addresses, bool hexDisplay) const;

        void refresh_all(bool hexDisplay);

        void clear();

        [[nodiscard]] std::size_t size() const;

    private:
        [[nodiscard]] static std::uint64_t make_key(std::uint64_t address, ValueType valueType);

        static void update_formatted_values(MonitoredAddress& entry, bool hexDisplay);

        mutable std::mutex m_mutex;
        std::unordered_map<std::uint64_t, MonitoredAddressPtr> m_registry;
        MemoryReadCallback m_memoryReader;
    };

} // namespace Vertex::Scanner
