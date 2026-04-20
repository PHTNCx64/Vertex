//
// Copyright (C) 2026 PHTNC<>.
// Licensed under GPLv3.0 with Plugin Interface exceptions.
//
#include <vertex/scanner/addressmonitor.hh>
#include <vertex/scanner/plugin_value_format.hh>
#include <vertex/scanner/valueconverter.hh>

#include <ranges>

namespace Vertex::Scanner
{
    void AddressMonitor::set_memory_reader(MemoryReadCallback reader)
    {
        std::scoped_lock<std::mutex> lock(m_mutex);
        m_memoryReader = std::move(reader);
    }

    void AddressMonitor::set_bulk_memory_reader(BulkMemoryReadCallback reader)
    {
        std::scoped_lock<std::mutex> lock(m_mutex);
        m_bulkMemoryReader = std::move(reader);
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
        entry->valueSize = static_cast<std::uint32_t>(get_value_type_size(valueType));
        entry->isValid = true;

        m_registry[key] = entry;
        return entry;
    }

    MonitoredAddressPtr AddressMonitor::get_or_create_plugin(const std::uint64_t address,
                                                               std::shared_ptr<const TypeSchema> schema)
    {
        if (!schema || schema->kind != TypeKind::PluginDefined || schema->valueSize == 0)
        {
            return nullptr;
        }

        std::scoped_lock<std::mutex> lock(m_mutex);

        const auto key = make_key(address, schema->id);
        const auto it = m_registry.find(key);

        if (it != m_registry.end())
        {
            it->second->schema = schema;
            return it->second;
        }

        auto entry = std::make_shared<MonitoredAddress>();
        entry->address = address;
        entry->valueType = ValueType::COUNT;
        entry->endianness = Endianness::Little;
        entry->schema = schema;
        entry->valueSize = schema->valueSize;
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
        if (addresses.empty())
        {
            return;
        }

        if (m_bulkMemoryReader)
        {
            struct PendingBulkRead final
            {
                MonitoredAddressPtr entry{};
                std::size_t valueSize{};
                std::vector<std::uint8_t> buffer{};
            };

            std::vector<PendingBulkRead> pendingReads{};
            pendingReads.reserve(addresses.size());

            for (const auto& entry : addresses)
            {
                if (!entry)
                {
                    continue;
                }

                const std::size_t valueSize = entry->valueSize;
                if (valueSize == 0)
                {
                    continue;
                }

                pendingReads.push_back(PendingBulkRead{
                    .entry = entry,
                    .valueSize = valueSize,
                    .buffer = std::vector<std::uint8_t>(valueSize)
                });
            }

            if (!pendingReads.empty())
            {
                std::vector<BulkReadRequest> requests(pendingReads.size());
                std::vector<BulkReadResult> results(pendingReads.size());
                for (std::size_t i{}; i < pendingReads.size(); ++i)
                {
                    requests[i] = {
                        pendingReads[i].entry->address,
                        pendingReads[i].valueSize,
                        pendingReads[i].buffer.data()
                    };
                }

                const StatusCode bulkStatus = m_bulkMemoryReader(requests, results);
                if (bulkStatus == StatusCode::STATUS_OK)
                {
                    for (std::size_t i{}; i < pendingReads.size(); ++i)
                    {
                        auto& entry = *pendingReads[i].entry;
                        const bool success = results[i].status == StatusCode::STATUS_OK && !pendingReads[i].buffer.empty();

                        if (success)
                        {
                            if (!entry.currentValue.empty())
                            {
                                entry.previousValue = entry.currentValue;
                            }

                            if (entry.firstValue.empty())
                            {
                                entry.firstValue = pendingReads[i].buffer;
                            }

                            entry.currentValue = std::move(pendingReads[i].buffer);
                            entry.isValid = true;
                        }
                        else
                        {
                            entry.isValid = false;
                        }

                        update_formatted_values(entry, hexDisplay);
                    }

                    return;
                }
            }
        }

        if (!m_memoryReader)
        {
            return;
        }

        for (const auto& entry : addresses)
        {
            if (!entry)
            {
                continue;
            }

            const std::size_t valueSize = entry->valueSize;
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

    AddressMonitor::AddressKey AddressMonitor::make_key(const std::uint64_t address, const ValueType valueType)
    {
        return make_key(address, builtin_type_id(valueType));
    }

    AddressMonitor::AddressKey AddressMonitor::make_key(const std::uint64_t address, const TypeId typeId)
    {
        return AddressKey{address, static_cast<std::uint32_t>(typeId)};
    }

    void AddressMonitor::update_formatted_values(MonitoredAddress& entry, const bool hexDisplay)
    {
        const bool isPlugin = entry.schema && entry.schema->kind == TypeKind::PluginDefined;

        if (entry.isValid && !entry.currentValue.empty())
        {
            entry.formattedValue = isPlugin
                ? format_plugin_bytes(*entry.schema, entry.currentValue.data(), entry.currentValue.size())
                : ValueConverter::format(entry.valueType, entry.currentValue.data(), entry.currentValue.size(), hexDisplay, entry.endianness);
        }
        else
        {
            entry.formattedValue = "???";
        }

        if (!entry.previousValue.empty())
        {
            entry.formattedPreviousValue = isPlugin
                ? format_plugin_bytes(*entry.schema, entry.previousValue.data(), entry.previousValue.size())
                : ValueConverter::format(entry.valueType, entry.previousValue.data(), entry.previousValue.size(), hexDisplay, entry.endianness);
        }

        if (!entry.firstValue.empty())
        {
            entry.formattedFirstValue = isPlugin
                ? format_plugin_bytes(*entry.schema, entry.firstValue.data(), entry.firstValue.size())
                : ValueConverter::format(entry.valueType, entry.firstValue.data(), entry.firstValue.size(), hexDisplay, entry.endianness);
        }
    }

} // namespace Vertex::Scanner
