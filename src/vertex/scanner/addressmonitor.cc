//
// Copyright (C) 2026 PHTNC<>.
// Licensed under GPLv3.0 with Plugin Interface exceptions.
//
#include <vertex/scanner/addressmonitor.hh>
#include <vertex/scanner/valueconverter.hh>

#include <ranges>

namespace Vertex::Scanner
{
    void AddressMonitor::set_memory_reader(MemoryReadCallback reader)
    {
        std::scoped_lock<std::mutex> lock(m_mutex);
        m_memoryReader = std::move(reader);
    }

    MonitoredAddressPtr AddressMonitor::get_or_create(const std::uint64_t address, const ValueType valueType, const Endianness endianness)
    {
        std::scoped_lock<std::mutex> lock(m_mutex);

        const auto key = make_key(address, valueType);
        const auto it = m_registry.find(key);

        if (it != m_registry.end())
        {
            it->second->endianness = endianness;
            return it->second;
        }

        auto entry = std::make_shared<MonitoredAddress>();
        entry->address = address;
        entry->valueType = valueType;
        entry->endianness = endianness;
        entry->isValid = true;

        m_registry[key] = entry;
        return entry;
    }

    MonitoredAddressPtr AddressMonitor::get(const std::uint64_t address, const ValueType valueType) const
    {
        std::scoped_lock<std::mutex> lock(m_mutex);

        const auto key = make_key(address, valueType);
        const auto it = m_registry.find(key);

        if (it != m_registry.end())
        {
            return it->second;
        }

        return nullptr;
    }

    void AddressMonitor::remove(const std::uint64_t address, const ValueType valueType)
    {
        std::scoped_lock<std::mutex> lock(m_mutex);
        const auto key = make_key(address, valueType);
        m_registry.erase(key);
    }

    void AddressMonitor::refresh(const std::vector<MonitoredAddressPtr>& addresses, const bool hexDisplay) const
    {
        if (!m_memoryReader || addresses.empty())
        {
            return;
        }

        for (const auto& entry : addresses)
        {
            if (!entry)
            {
                continue;
            }

            const std::size_t valueSize = get_value_type_size(entry->valueType);
            if (valueSize == 0)
            {
                continue;
            }

            std::vector<std::uint8_t> buffer;
            const bool success = m_memoryReader(entry->address, valueSize, buffer);

            if (success && !buffer.empty())
            {
                if (!entry->currentValue.empty())
                {
                    entry->previousValue = entry->currentValue;
                }

                if (entry->firstValue.empty())
                {
                    entry->firstValue = buffer;
                }

                entry->currentValue = std::move(buffer);
                entry->isValid = true;
            }
            else
            {
                entry->isValid = false;
            }

            update_formatted_values(*entry, hexDisplay);
        }
    }

    void AddressMonitor::refresh_all(const bool hexDisplay)
    {
        std::vector<MonitoredAddressPtr> allAddresses;

        {
            std::scoped_lock<std::mutex> lock(m_mutex);
            allAddresses.reserve(m_registry.size());
            for (const auto& entry : m_registry | std::views::values)
            {
                allAddresses.push_back(entry);
            }
        }

        refresh(allAddresses, hexDisplay);
    }

    void AddressMonitor::clear()
    {
        std::scoped_lock<std::mutex> lock(m_mutex);
        m_registry.clear();
    }

    std::size_t AddressMonitor::size() const
    {
        std::scoped_lock<std::mutex> lock(m_mutex);
        return m_registry.size();
    }

    std::uint64_t AddressMonitor::make_key(const std::uint64_t address, const ValueType valueType)
    {
        return (static_cast<std::uint64_t>(valueType) << 56) | (address & 0x00FFFFFFFFFFFFFF);
    }

    void AddressMonitor::update_formatted_values(MonitoredAddress& entry, const bool hexDisplay)
    {
        if (entry.isValid && !entry.currentValue.empty())
        {
            entry.formattedValue = ValueConverter::format(
                entry.valueType,
                entry.currentValue.data(),
                entry.currentValue.size(),
                hexDisplay,
                entry.endianness
            );
        }
        else
        {
            entry.formattedValue = "???";
        }

        if (!entry.previousValue.empty())
        {
            entry.formattedPreviousValue = ValueConverter::format(
                entry.valueType,
                entry.previousValue.data(),
                entry.previousValue.size(),
                hexDisplay,
                entry.endianness
            );
        }

        if (!entry.firstValue.empty())
        {
            entry.formattedFirstValue = ValueConverter::format(
                entry.valueType,
                entry.firstValue.data(),
                entry.firstValue.size(),
                hexDisplay,
                entry.endianness
            );
        }
    }

} // namespace Vertex::Scanner
