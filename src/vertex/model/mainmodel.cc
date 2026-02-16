//
// Copyright (C) 2026 PHTNC<>.
// Licensed under GPLv3.0 with Plugin Interface exceptions.
//
#include <fmt/format.h>
#include <vertex/model/mainmodel.hh>

#include <vertex/scanner/valueconverter.hh>
#include <vertex/scanner/pluginmemoryreader.hh>
#include <vertex/runtime/caller.hh>
#include <sdk/memory.h>
#include <ranges>
#include <algorithm>
#include <numeric>
#include <string_view>

namespace Vertex::Model
{
    MainModel::MainModel(Configuration::ISettings& settingsService,
                         Scanner::IMemoryScanner& memoryService,
                         Runtime::ILoader& loaderService,
                         Log::ILog& loggerService)
        : m_settingsService(settingsService),
          m_memoryService(memoryService),
          m_loaderService(loaderService),
          m_loggerService(loggerService)
    {
    }

    StatusCode MainModel::validate_input(const Scanner::ValueType type, const bool hexadecimal,
                                         const std::string_view input,
                                         std::vector<std::uint8_t>& output) const
    {
        if (input.empty())
        {
            m_loggerService.log_warn(fmt::format("{}: validate_input called with empty input", MODEL_NAME));
            output.clear();
            return StatusCode::STATUS_OK;
        }

        if (std::ranges::all_of(input, [](const char c) { return std::isspace(static_cast<unsigned char>(c)); }))
        {
            m_loggerService.log_warn(fmt::format("{}: Input contains only whitespace", MODEL_NAME));
            output.clear();
            return StatusCode::STATUS_OK;
        }

        const auto typeName = Scanner::get_value_type_name(type);

        m_loggerService.log_info(fmt::format("{}: validate_input: type={}, hex={}, input='{}'",
            MODEL_NAME, typeName, hexadecimal, input));

        auto result = Scanner::ValueConverter::parse(type, std::string{input}, hexadecimal);
        if (!result.has_value())
        {
            m_loggerService.log_error(fmt::format("{}: Failed to parse input '{}' as {} (hex={})",
                MODEL_NAME, input, typeName, hexadecimal));
            return StatusCode::STATUS_ERROR_INVALID_PARAMETER;
        }

        output = std::move(result.value());

        std::string bytesHex;
        for (std::size_t i = 0; i < std::min(output.size(), static_cast<std::size_t>(16)); ++i)
        {
            bytesHex += fmt::format("{:02X} ", output[i]);
        }
        m_loggerService.log_info(fmt::format("{}: Parsed {} bytes: [{}]", MODEL_NAME, output.size(), bytesHex));

        return StatusCode::STATUS_OK;
    }

    StatusCode MainModel::read_process_memory(const std::uint64_t address, const std::size_t size, std::vector<char>& output) const
    {
        if (m_loaderService.has_plugin_loaded() != StatusCode::STATUS_OK)
        {
            m_loggerService.log_error(fmt::format("{}: No active plugin", MODEL_NAME));
            return StatusCode::STATUS_ERROR_PLUGIN_NOT_ACTIVE;
        }

        const auto& plugin = m_loaderService.get_active_plugin().value().get();

        output.resize(size);
        const auto result = Runtime::safe_call(plugin.internal_vertex_memory_read_process, address, static_cast<std::uint32_t>(size), output.data());
        const auto status = Runtime::get_status(result);
        if (status == StatusCode::STATUS_ERROR_FUNCTION_NOT_FOUND)
        {
            m_loggerService.log_error(fmt::format("{}: internal_vertex_read_process_memory not implemented", MODEL_NAME));
            return StatusCode::STATUS_ERROR_PLUGIN_FUNCTION_NOT_IMPLEMENTED;
        }
        return status;
    }

    StatusCode MainModel::write_process_memory(const std::uint64_t address, const std::vector<std::uint8_t>& data) const
    {
        if (data.empty())
        {
            m_loggerService.log_warn(fmt::format("{}: write_process_memory called with empty data", MODEL_NAME));
            return StatusCode::STATUS_OK;
        }

        if (m_loaderService.has_plugin_loaded() != StatusCode::STATUS_OK)
        {
            m_loggerService.log_error(fmt::format("{}: No active plugin", MODEL_NAME));
            return StatusCode::STATUS_ERROR_PLUGIN_NOT_ACTIVE;
        }

        const auto& plugin = m_loaderService.get_active_plugin().value().get();

        m_loggerService.log_info(fmt::format("{}: Writing {} bytes to address 0x{:X}",
            MODEL_NAME, data.size(), address));

        const auto result = Runtime::safe_call(
            plugin.internal_vertex_memory_write_process,
            address,
            static_cast<std::uint64_t>(data.size()),
            reinterpret_cast<const char*>(data.data()));
        const auto status = Runtime::get_status(result);

        if (status == StatusCode::STATUS_ERROR_FUNCTION_NOT_FOUND)
        {
            m_loggerService.log_error(fmt::format("{}: internal_vertex_write_process_memory not implemented", MODEL_NAME));
            return StatusCode::STATUS_ERROR_PLUGIN_FUNCTION_NOT_IMPLEMENTED;
        }

        if (!Runtime::status_ok(result))
        {
            m_loggerService.log_error(fmt::format("{}: write_process_memory FAILED with status {}",
                MODEL_NAME, static_cast<int>(status)));
        }

        return status;
    }

    Theme MainModel::get_theme() const
    {
        return static_cast<Theme>(m_settingsService.get_int("general.theme"));
    }

    StatusCode MainModel::kill_process() const
    {
        if (m_loaderService.has_plugin_loaded() != StatusCode::STATUS_OK)
        {
            m_loggerService.log_error(fmt::format("{}: {}", MODEL_NAME, "There is no active plugin set!"));
            return StatusCode::STATUS_ERROR_PLUGIN_NOT_ACTIVE;
        }

        const auto& plugin = m_loaderService.get_active_plugin().value().get();

        if (!plugin.is_loaded())
        {
            m_loggerService.log_error(fmt::format("{}: {}", MODEL_NAME, "The active plugin is not loaded!"));
            return StatusCode::STATUS_ERROR_PLUGIN_NOT_LOADED;
        }

        const auto result = Runtime::safe_call(plugin.internal_vertex_process_kill);
        const auto status = Runtime::get_status(result);
        if (status == StatusCode::STATUS_ERROR_FUNCTION_NOT_FOUND)
        {
            m_loggerService.log_error(fmt::format("{}: {}", MODEL_NAME, "internal_vertex_is_process_valid is not implemented by plugin!"));
            return StatusCode::STATUS_ERROR_PLUGIN_FUNCTION_NOT_IMPLEMENTED;
        }
        return status;
    }

    bool MainModel::is_scan_complete() const
    {
        return m_memoryService.is_scan_complete();
    }

    StatusCode MainModel::is_process_opened() const
    {
        if (m_loaderService.has_plugin_loaded() != StatusCode::STATUS_OK)
        {
            m_loggerService.log_error(fmt::format("{}: {}", MODEL_NAME, "There is no active plugin set!"));
            return StatusCode::STATUS_ERROR_PLUGIN_NOT_ACTIVE;
        }

        const auto& plugin = m_loaderService.get_active_plugin().value().get();

        if (!plugin.is_loaded())
        {
            m_loggerService.log_error(fmt::format("{}: {}", MODEL_NAME, "The active plugin is not loaded!"));
            return StatusCode::STATUS_ERROR_PLUGIN_NOT_LOADED;
        }

        const auto result = Runtime::safe_call(plugin.internal_vertex_process_is_valid);
        const auto status = Runtime::get_status(result);
        if (status == StatusCode::STATUS_ERROR_FUNCTION_NOT_FOUND)
        {
            m_loggerService.log_error(fmt::format("{}: {}", MODEL_NAME, "internal_vertex_is_process_valid is not implemented by plugin!"));
            return StatusCode::STATUS_ERROR_PLUGIN_FUNCTION_NOT_IMPLEMENTED;
        }
        return status;
    }

    void MainModel::ensure_memory_reader_setup() const
    {
        if (!m_memoryService.has_memory_reader())
        {
            auto reader = std::make_shared<Scanner::PluginMemoryReader>(m_loaderService);
            m_memoryService.set_memory_reader(reader);
        }
    }

    StatusCode MainModel::initialize_scan(Scanner::ValueType valueType, std::uint8_t scanMode,
                                          bool hexDisplay, bool alignmentEnabled, std::size_t alignmentValue,
                                          Scanner::Endianness endianness,
                                          const std::vector<std::uint8_t>& input,
                                          const std::vector<std::uint8_t>& input2) const
    {
        std::vector<MemoryRegion> regions{};
        const auto queryStatus = query_memory_regions(regions);
        if (queryStatus != StatusCode::STATUS_OK)
        {
            m_loggerService.log_error(fmt::format("{}: Failed to query memory regions", MODEL_NAME));
            return queryStatus;
        }

        auto scanRegions = regions
            | std::views::transform([](const auto& region) {
                return Scanner::ScanRegion{
                    .baseAddress = region.baseAddress,
                    .size = region.regionSize
                };
            })
            | std::ranges::to<std::vector>();

        Scanner::ScanConfiguration config{};
        config.valueType = valueType;
        config.scanMode = scanMode;
        config.input = input;
        config.input2 = input2;
        config.dataSize = Scanner::get_value_type_size(valueType);
        config.alignmentRequired = alignmentEnabled;
        config.alignment = alignmentEnabled ? alignmentValue : 1;
        config.hexDisplay = hexDisplay;
        config.endianness = endianness;

        if (Scanner::is_string_type(valueType) && !input.empty())
        {
            config.dataSize = input.size();
        }

        m_memoryService.set_scan_abort_state(false);

        ensure_memory_reader_setup();

        return m_memoryService.initialize_scan(config, scanRegions);
    }

    StatusCode MainModel::initialize_next_scan(Scanner::ValueType valueType, std::uint8_t scanMode,
                                               bool hexDisplay, bool alignmentEnabled, std::size_t alignmentValue,
                                               Scanner::Endianness endianness,
                                               const std::vector<std::uint8_t>& input,
                                               const std::vector<std::uint8_t>& input2) const
    {
        Scanner::ScanConfiguration config{};
        config.valueType = valueType;
        config.scanMode = scanMode;
        config.input = input;
        config.input2 = input2;
        config.dataSize = Scanner::get_value_type_size(valueType);
        config.alignmentRequired = alignmentEnabled;
        config.alignment = alignmentEnabled ? alignmentValue : 1;
        config.hexDisplay = hexDisplay;
        config.endianness = endianness;

        if (Scanner::is_string_type(valueType) && !input.empty())
        {
            config.dataSize = input.size();
        }

        ensure_memory_reader_setup();

        return m_memoryService.initialize_next_scan(config);
    }

    StatusCode MainModel::undo_scan() const
    {
        return m_memoryService.undo_scan();
    }

    StatusCode MainModel::stop_scan() const
    {
        return m_memoryService.stop_scan();
    }

    bool MainModel::can_undo_scan() const
    {
        return m_memoryService.can_undo();
    }

    std::uint64_t MainModel::get_scan_progress_current() const
    {
        return m_memoryService.get_regions_scanned();
    }

    std::uint64_t MainModel::get_scan_progress_total() const
    {
        return m_memoryService.get_total_regions();
    }

    std::uint64_t MainModel::get_scan_results_count() const
    {
        return m_memoryService.get_results_count();
    }

    StatusCode MainModel::get_scan_results(std::vector<Scanner::IMemoryScanner::ScanResultEntry>& results, const std::size_t maxResults) const
    {
        return m_memoryService.get_scan_results(results, maxResults);
    }

    StatusCode MainModel::get_scan_results_range(std::vector<Scanner::IMemoryScanner::ScanResultEntry>& results, const std::size_t startIndex, const std::size_t count) const
    {
        return m_memoryService.get_scan_results_range(results, startIndex, count);
    }

    StatusCode MainModel::get_file_executable_extensions(std::vector<std::string>& extensions) const
    {
        extensions.clear();
        if (m_loaderService.has_plugin_loaded() != StatusCode::STATUS_OK)
        {
            m_loggerService.log_error(fmt::format("{}: No active plugin", MODEL_NAME));
            return StatusCode::STATUS_ERROR_PLUGIN_NOT_ACTIVE;
        }

        const auto& plugin = m_loaderService.get_active_plugin().value().get();
        std::uint32_t count = 0;
        const auto countResult = Runtime::safe_call(plugin.internal_vertex_process_get_executable_extensions, nullptr, &count);
        const auto status = Runtime::get_status(countResult);
        if (status == StatusCode::STATUS_ERROR_FUNCTION_NOT_FOUND)
        {
            m_loggerService.log_error(fmt::format("{}: internal_vertex_get_process_extensions not implemented", MODEL_NAME));
            return StatusCode::STATUS_ERROR_PLUGIN_FUNCTION_NOT_IMPLEMENTED;
        }
        if (!Runtime::status_ok(countResult) || count == 0)
        {
            m_loggerService.log_error(fmt::format("{}: internal_vertex_get_process_extensions failed to get count", MODEL_NAME));
            return status;
        }

        std::vector<char*> ext_ptrs(count, nullptr);
        const auto extResult = Runtime::safe_call(plugin.internal_vertex_process_get_executable_extensions, ext_ptrs.data(), &count);
        const auto extStatus = Runtime::get_status(extResult);
        if (extStatus == StatusCode::STATUS_ERROR_FUNCTION_NOT_FOUND)
        {
            m_loggerService.log_error(fmt::format("{}: internal_vertex_get_process_extensions not implemented", MODEL_NAME));
            return StatusCode::STATUS_ERROR_PLUGIN_FUNCTION_NOT_IMPLEMENTED;
        }
        if (!Runtime::status_ok(extResult))
        {
            m_loggerService.log_error(fmt::format("{}: vertex_get_process_extensions failed to get extensions", MODEL_NAME));
            return extStatus;
        }

        std::ranges::for_each(ext_ptrs | std::views::filter([](const auto ptr) { return ptr != nullptr; }),
                              [&extensions](const auto ptr) { extensions.emplace_back(ptr); });
        return StatusCode::STATUS_OK;
    }

    StatusCode MainModel::query_memory_regions(std::vector<MemoryRegion>& regions) const
    {
        if (m_loaderService.has_plugin_loaded() != StatusCode::STATUS_OK)
        {
            m_loggerService.log_error(fmt::format("{}: No active plugin", MODEL_NAME));
            return StatusCode::STATUS_ERROR_PLUGIN_NOT_ACTIVE;
        }

        const auto& plugin = m_loaderService.get_active_plugin().value().get();

        MemoryRegion* internalRegions{};
        std::uint64_t internalRegionsSize{};

        const auto result = Runtime::safe_call(plugin.internal_vertex_memory_query_regions, &internalRegions, &internalRegionsSize);
        const auto status = Runtime::get_status(result);
        if (status == StatusCode::STATUS_ERROR_FUNCTION_NOT_FOUND)
        {
            m_loggerService.log_error(fmt::format("{}: internal_vertex_query_memory_regions not implemented", MODEL_NAME));
            return StatusCode::STATUS_ERROR_PLUGIN_FUNCTION_NOT_IMPLEMENTED;
        }
        if (!Runtime::status_ok(result))
        {
            return status;
        }

        regions.assign(internalRegions, internalRegions + internalRegionsSize);

        const auto totalSize = std::accumulate(regions.begin(), regions.end(), std::uint64_t{0},
                                                [](const auto sum, const auto& region) {
                                                    return sum + region.regionSize;
                                                });
        m_loggerService.log_info(fmt::format("{}: Queried {} memory regions, total size {} MB",
            MODEL_NAME, regions.size(), totalSize / (1024 * 1024)));

        std::ranges::for_each(
            regions | std::views::take(5) | std::views::enumerate,
            [this](const auto& entry) {
                const auto& [i, region] = entry;
                m_loggerService.log_info(fmt::format("{}: Region[{}]: base=0x{:X}, size={}",
                    MODEL_NAME, i, region.baseAddress, region.regionSize));
            });

        return status;
    }

    Log::ILog* MainModel::get_log_service() const
    {
        return &m_loggerService;
    }

    int MainModel::get_ui_state_int(const std::string_view key, const int defaultValue) const
    {
        const std::string keyStr{key};
        if (!m_settingsService.get_bool("general.guiSavingEnabled", true))
        {
            return defaultValue;
        }
        return m_settingsService.get_int(keyStr, defaultValue);
    }

    bool MainModel::get_ui_state_bool(const std::string_view key, const bool defaultValue) const
    {
        const std::string keyStr{key};
        if (!m_settingsService.get_bool("general.guiSavingEnabled", true))
        {
            return defaultValue;
        }
        return m_settingsService.get_bool(keyStr, defaultValue);
    }

    void MainModel::set_ui_state_int(const std::string_view key, const int value) const
    {
        const std::string keyStr{key};
        if (m_settingsService.get_bool("general.guiSavingEnabled", true))
        {
            m_settingsService.set_value(keyStr, value);
        }
    }

    void MainModel::set_ui_state_bool(const std::string_view key, const bool value) const
    {
        const std::string keyStr{key};
        if (m_settingsService.get_bool("general.guiSavingEnabled", true))
        {
            m_settingsService.set_value(keyStr, value);
        }
    }
}
