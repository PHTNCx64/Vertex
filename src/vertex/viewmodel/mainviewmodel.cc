//
// Copyright (C) 2026 PHTNC<>.
// Licensed under GPLv3.0 with Plugin Interface exceptions.
//
#include <vertex/viewmodel/mainviewmodel.hh>

#include <algorithm>
#include <chrono>
#include <limits>
#include <ranges>
#include <unordered_map>
#include <utility>

#include <fmt/format.h>

#include <vertex/event/eventid.hh>
#include <vertex/event/types/processcloseevent.hh>
#include <vertex/event/types/viewevent.hh>
#include <vertex/event/types/viewupdateevent.hh>
#include <vertex/model/mainmodel.hh>
#include <vertex/scanner/memoryscanner/imemoryscanner.hh>
#include <vertex/scanner/valueconverter.hh>
#include <vertex/thread/threadchannel.hh>

namespace Vertex::ViewModel
{
    MainViewModel::MainViewModel(std::unique_ptr<Model::MainModel> model, Event::EventBus& eventBus, Thread::IThreadDispatcher& dispatcher, std::string name)
        : m_viewModelName{std::move(name)},
          m_model{std::move(model)},
          m_eventBus{eventBus},
          m_dispatcher{dispatcher}
    {
        m_processInformation = "No process attached";
        m_scanProgress = {0, 0, "Ready"};

        m_addressMonitor.set_memory_reader(
            [this](const std::uint64_t address, const std::size_t size, std::vector<std::uint8_t>& output) -> bool
            {
                std::vector<char> buffer;
                const StatusCode status = m_model->read_process_memory(address, size, buffer);
                if (status == StatusCode::STATUS_OK && !buffer.empty())
                {
                    output.assign(
                        reinterpret_cast<const std::uint8_t*>(buffer.data()),
                        reinterpret_cast<const std::uint8_t*>(buffer.data()) + buffer.size()
                    );
                    return true;
                }
                return false;
            }
        );

        subscribe_to_events();
        load_ui_state_from_settings();
        update_available_scan_modes();
    }

    void MainViewModel::load_ui_state_from_settings()
    {
        m_valueTypeIndex = m_model->get_ui_state_int("uiState.mainView.valueTypeIndex", 2);
        m_scanTypeIndex = m_model->get_ui_state_int("uiState.mainView.scanTypeIndex", 0);
        m_endiannessTypeIndex = m_model->get_ui_state_int("uiState.mainView.endiannessTypeIndex", 0);
        m_isHexadecimal = m_model->get_ui_state_bool("uiState.mainView.hexadecimalEnabled", false);
        m_alignmentEnabled = m_model->get_ui_state_bool("uiState.mainView.alignmentEnabled", true);
        m_alignmentValue = m_model->get_ui_state_int("uiState.mainView.alignmentValue", 4);
    }

    MainViewModel::~MainViewModel()
    {
        stop_freeze_timer();
        unsubscribe_from_events();
    }

    void MainViewModel::subscribe_to_events()
    {
        m_eventBus.subscribe<Event::ProcessOpenEvent>(m_viewModelName, Event::PROCESS_OPEN_EVENT,
                                                      [this](const Event::ProcessOpenEvent& evt)
                                                      {
                                                          m_isInitialScanAvailable = true;
                                                          on_process_opened(evt);
                                                      });
    }

    void MainViewModel::unsubscribe_from_events() const { m_eventBus.unsubscribe_all(m_viewModelName); }

    void MainViewModel::set_event_callback(std::move_only_function<void(Event::EventId, const Event::VertexEvent&) const> callback)
    {
        m_eventCallback = std::move(callback);
    }

    void MainViewModel::notify_property_changed() const
    {
        notify_view_update(ViewUpdateFlags::DATATYPES);
    }

    void MainViewModel::notify_view_update(const ViewUpdateFlags flags) const
    {
        if (m_eventCallback)
        {
            const Event::ViewUpdateEvent event(flags);
            m_eventCallback(Event::VIEW_UPDATE_EVENT, event);
        }
    }

    bool MainViewModel::is_scan_complete() const
    {
        return m_model->is_scan_complete();
    }

    void MainViewModel::kill_process() const
    {
        std::ignore = m_model->kill_process();
    }

    Scanner::ValueType MainViewModel::get_current_value_type() const
    {
        return static_cast<Scanner::ValueType>(m_valueTypeIndex);
    }

    Scanner::ValueType MainViewModel::get_scanned_value_type() const
    {
        return static_cast<Scanner::ValueType>(m_scannedValueTypeIndex);
    }

    std::vector<std::string> MainViewModel::get_value_type_names() const
    {
        auto indices = std::views::iota(0, static_cast<int>(Scanner::ValueType::COUNT));
        return indices
            | std::views::transform([](const int i) {
                return Scanner::get_value_type_name(static_cast<Scanner::ValueType>(i));
            })
            | std::ranges::to<std::vector>();
    }

    std::vector<std::string> MainViewModel::get_scan_mode_names() const
    {
        const auto valueType = get_current_value_type();

        if (Scanner::is_string_type(valueType))
        {
            auto indices = std::views::iota(0, static_cast<int>(Scanner::StringScanMode::COUNT));
            return indices
                | std::views::transform([](const int i) {
                    return Scanner::get_string_scan_mode_name(static_cast<Scanner::StringScanMode>(i));
                })
                | std::ranges::to<std::vector>();
        }

        return m_availableNumericModes
            | std::views::transform([](const Scanner::NumericScanMode mode) {
                return Scanner::get_numeric_scan_mode_name(mode);
            })
            | std::ranges::to<std::vector>();
    }

    bool MainViewModel::needs_input_value() const
    {
        const auto valueType = get_current_value_type();

        if (Scanner::is_string_type(valueType))
        {
            return true;
        }

        return Scanner::scan_mode_needs_input(get_actual_numeric_scan_mode());
    }

    void MainViewModel::initial_scan()
    {
        m_scannedValueTypeIndex = m_valueTypeIndex;
        m_scannedEndiannessIndex = m_endiannessTypeIndex;
        const auto valueType = get_current_value_type();
        std::vector<std::uint8_t> inputBuffer{};
        std::vector<std::uint8_t> inputBuffer2{};

        if (needs_input_value() && !m_valueInput.empty())
        {
            const StatusCode status = m_model->validate_input(valueType, m_isHexadecimal, m_valueInput, inputBuffer);
            if (status != StatusCode::STATUS_OK) [[unlikely]]
            {
                m_scanProgress = {0, 0, "Input validation failed"};
                notify_property_changed();
                return;
            }
        }

        const auto actualMode = get_actual_numeric_scan_mode();
        if (actualMode == Scanner::NumericScanMode::Between && !m_valueInput2.empty())
        {
            const StatusCode status = m_model->validate_input(valueType, m_isHexadecimal, m_valueInput2, inputBuffer2);
            if (status != StatusCode::STATUS_OK) [[unlikely]]
            {
                m_scanProgress = {0, 0, "Input2 validation failed"};
                notify_property_changed();
                return;
            }
        }

        const StatusCode status = m_model->initialize_scan(
            valueType,
            get_actual_scan_mode_value(),
            m_isHexadecimal,
            m_alignmentEnabled,
            static_cast<std::size_t>(m_alignmentValue),
            static_cast<Scanner::Endianness>(m_endiannessTypeIndex),
            inputBuffer,
            inputBuffer2
        );

        if (status != StatusCode::STATUS_OK) [[unlikely]]
        {
            m_scanProgress = {0, 0, "Scan initialization failed"};
            notify_property_changed();
            return;
        }

        if (!Scanner::is_string_type(valueType) && actualMode == Scanner::NumericScanMode::Unknown)
        {
            m_isUnknownScanMode = true;
            update_available_scan_modes();
            m_scanTypeIndex = 0;
        }

        m_scannedValues.clear();

        m_scanProgress = {0, 0, "Scanning..."};
        m_isNextScanAvailable = false;
        notify_property_changed();
    }

    void MainViewModel::next_scan()
    {
        m_scannedValueTypeIndex = m_valueTypeIndex;
        m_scannedEndiannessIndex = m_endiannessTypeIndex;
        const auto valueType = get_current_value_type();
        std::vector<std::uint8_t> inputBuffer{};
        std::vector<std::uint8_t> inputBuffer2{};

        if (needs_input_value() && !m_valueInput.empty())
        {
            const StatusCode status = m_model->validate_input(valueType, m_isHexadecimal, m_valueInput, inputBuffer);
            if (status != StatusCode::STATUS_OK) [[unlikely]]
            {
                m_scanProgress = {0, 0, "Input validation failed"};
                notify_property_changed();
                return;
            }
        }

        const auto actualMode = get_actual_numeric_scan_mode();
        if (actualMode == Scanner::NumericScanMode::Between && !m_valueInput2.empty())
        {
            const StatusCode status = m_model->validate_input(valueType, m_isHexadecimal, m_valueInput2, inputBuffer2);
            if (status != StatusCode::STATUS_OK) [[unlikely]]
            {
                m_scanProgress = {0, 0, "Input2 validation failed"};
                notify_property_changed();
                return;
            }
        }

        const StatusCode status = m_model->initialize_next_scan(
            valueType,
            get_actual_scan_mode_value(),
            m_isHexadecimal,
            m_alignmentEnabled,
            static_cast<std::size_t>(m_alignmentValue),
            static_cast<Scanner::Endianness>(m_endiannessTypeIndex),
            inputBuffer,
            inputBuffer2
        );

        if (status != StatusCode::STATUS_OK) [[unlikely]]
        {
            m_scanProgress = {0, 0, "Next scan initialization failed"};
            notify_property_changed();
            return;
        }

        m_scanProgress = {0, 0, "Scanning..."};
        m_isNextScanAvailable = false;
        notify_property_changed();
    }

    void MainViewModel::undo_scan() const
    {
        std::ignore = m_model->undo_scan();
        notify_property_changed();
    }

    void MainViewModel::update_scan_progress()
    {
        const auto current = m_model->get_scan_progress_current();
        const auto total = m_model->get_scan_progress_total();
        const auto results = m_model->get_scan_results_count();

        m_scanProgress.current = static_cast<std::int64_t>(current);
        m_scanProgress.total = static_cast<std::int64_t>(total);
        m_scanProgress.statusMessage = fmt::format("Scanning... {}/{} regions, {} results", current, total, results);

        if (current >= total && total > 0)
        {
            m_scanProgress.statusMessage = fmt::format("Scan complete! Found {} results", results);
            m_isNextScanAvailable = (results > 0);
        }
    }

    void MainViewModel::open_project() const {}

    void MainViewModel::exit_application() const
    {
        m_eventBus.broadcast(Event::ViewEvent(Event::APPLICATION_SHUTDOWN_EVENT));
        wxTheApp->ExitMainLoop();
    }

    void MainViewModel::open_memory_view() const {}

    void MainViewModel::add_address_manually() const {}

    void MainViewModel::open_memory_region_settings() const
    {
        const Event::ViewEvent event{Event::VIEW_EVENT};
        m_eventBus.broadcast_to(ViewModelName::MEMORYATTRIBUTES, event);
    }

    void MainViewModel::open_process_list_window() const
    {
        const Event::ViewEvent event{Event::VIEW_EVENT};
        m_eventBus.broadcast_to(ViewModelName::PROCESSLIST, event);
    }

    void MainViewModel::open_settings_window() const
    {
        const Event::ViewEvent event{Event::VIEW_EVENT};
        m_eventBus.broadcast_to(ViewModelName::SETTINGS, event);
    }

    void MainViewModel::open_activity_window() const
    {
        const Event::ViewEvent event{Event::VIEW_EVENT};
        m_eventBus.broadcast_to(ViewModelName::ANALYTICS, event);
    }

    void MainViewModel::open_debugger_window() const
    {
        const Event::ViewEvent event{Event::VIEW_EVENT};
        m_eventBus.broadcast_to(ViewModelName::DEBUGGER, event);
    }

    void MainViewModel::open_injector_window() const
    {
        const Event::ViewEvent event{Event::VIEW_EVENT};
        m_eventBus.broadcast_to(ViewModelName::INJECTOR, event);
    }

    void MainViewModel::close_process_state()
    {
        m_isInitialScanAvailable = false;
        m_isNextScanAvailable = false;
        m_isUnknownScanMode = false;
        m_minProcessAddress = {};
        m_maxProcessAddress = {};
        update_available_scan_modes();
        m_scanTypeIndex = 0;

        stop_freeze_timer();

        const Event::ProcessCloseEvent event{ Event::PROCESS_CLOSED_EVENT };
        m_eventBus.broadcast(event);
    }

    void MainViewModel::get_file_executable_extensions(std::vector<std::string>& extensions) const
    {
        std::ignore = m_model->get_file_executable_extensions(extensions);
    }

    std::string MainViewModel::get_process_information() const { return m_processInformation; }

    void MainViewModel::set_process_information(const std::string_view informationText)
    {
        m_processInformation = informationText;
    }

    ScanProgress MainViewModel::get_scan_progress() const { return m_scanProgress; }

    std::vector<ScannedValue> MainViewModel::get_scanned_values() const
    {
        return m_scannedValues;
    }

    ScannedValue MainViewModel::get_scanned_value_at(const int index)
    {
        const auto it = m_visibleCache.find(index);
        if (it != m_visibleCache.end()) [[likely]]
        {
            return it->second;
        }

        if (m_cacheWindow.startIndex >= 0 && index >= m_cacheWindow.startIndex && index < m_cacheWindow.endIndex)
        {
            const int cacheIndex = index - m_cacheWindow.startIndex;
            if (cacheIndex >= 0 && cacheIndex < static_cast<int>(m_cacheWindow.addresses.size())) [[likely]]
            {
                const auto& entry = m_cacheWindow.addresses[cacheIndex];

                ScannedValue value{};
                value.address = fmt::format("0x{:X}", entry.address);

                const auto valueType = get_scanned_value_type();
                const auto scannedEndianness = static_cast<Scanner::Endianness>(m_scannedEndiannessIndex);
                if (!entry.value.empty())
                {
                    value.value = Scanner::ValueConverter::format(
                        valueType, entry.value.data(), entry.value.size(), m_isHexadecimal, scannedEndianness);
                }

                if (!entry.previousValue.empty())
                {
                    value.previousValue = Scanner::ValueConverter::format(
                        valueType, entry.previousValue.data(), entry.previousValue.size(), m_isHexadecimal, scannedEndianness);
                }

                if (!entry.firstValue.empty())
                {
                    value.firstValue = Scanner::ValueConverter::format(
                        valueType, entry.firstValue.data(), entry.firstValue.size(), m_isHexadecimal, scannedEndianness);
                }
                else if (!entry.previousValue.empty())
                {
                    value.firstValue = Scanner::ValueConverter::format(
                        valueType, entry.previousValue.data(), entry.previousValue.size(), m_isHexadecimal, scannedEndianness);
                }

                m_visibleCache[index] = value;

                return value;
            }
        }

        std::vector<Scanner::IMemoryScanner::ScanResultEntry> scanResults;
        const StatusCode status = m_model->get_scan_results_range(scanResults, index, 1);

        if (status != StatusCode::STATUS_OK || scanResults.empty()) [[unlikely]]
        {
            return ScannedValue{};
        }

        ScannedValue value{};
        value.address = fmt::format("0x{:X}", scanResults[0].address);

        const auto valueType = get_scanned_value_type();
        const auto scannedEndianness = static_cast<Scanner::Endianness>(m_scannedEndiannessIndex);
        if (!scanResults[0].value.empty())
        {
            value.value = Scanner::ValueConverter::format(
                valueType, scanResults[0].value.data(), scanResults[0].value.size(), m_isHexadecimal, scannedEndianness);
        }

        if (!scanResults[0].previousValue.empty())
        {
            value.previousValue = Scanner::ValueConverter::format(
                valueType, scanResults[0].previousValue.data(), scanResults[0].previousValue.size(), m_isHexadecimal, scannedEndianness);
        }

        if (!scanResults[0].firstValue.empty())
        {
            value.firstValue = Scanner::ValueConverter::format(
                valueType, scanResults[0].firstValue.data(), scanResults[0].firstValue.size(), m_isHexadecimal, scannedEndianness);
        }
        else if (!scanResults[0].previousValue.empty())
        {
            value.firstValue = Scanner::ValueConverter::format(
                valueType, scanResults[0].previousValue.data(), scanResults[0].previousValue.size(), m_isHexadecimal, scannedEndianness);
        }

        m_visibleCache[index] = value;

        return value;
    }

    void MainViewModel::update_cache_window(const int visibleStart, const int visibleEnd)
    {
        constexpr int BUFFER_SIZE = 500;
        const int newStartIndex = std::max(0, visibleStart - BUFFER_SIZE);
        const int totalResults = static_cast<int>(std::min(m_model->get_scan_results_count(), static_cast<std::uint64_t>(std::numeric_limits<int>::max())));
        const int newEndIndex = std::min(totalResults, visibleEnd + BUFFER_SIZE);

        if (newStartIndex == m_cacheWindow.startIndex && newEndIndex == m_cacheWindow.endIndex) [[unlikely]]
        {
            return;
        }

        const int count = newEndIndex - newStartIndex;
        if (count <= 0) [[unlikely]]
        {
            return;
        }

        std::vector<Scanner::IMemoryScanner::ScanResultEntry> newAddresses;
        const StatusCode status = m_model->get_scan_results_range(newAddresses, newStartIndex, count);

        if (status == StatusCode::STATUS_OK)
        {
            m_cacheWindow.startIndex = newStartIndex;
            m_cacheWindow.endIndex = newEndIndex;
            m_cacheWindow.addresses = std::move(newAddresses);
        }
    }

    void MainViewModel::refresh_visible_range(const int startIndex, const int endIndex)
    {
        const int count = endIndex - startIndex + 1;
        if (count <= 0 || count > 500)
        {
            return;
        }

        if (startIndex < m_cacheWindow.startIndex || endIndex > m_cacheWindow.endIndex)
        {
            return;
        }

        const auto valueType = get_scanned_value_type();
        const auto scannedEndianness = static_cast<Scanner::Endianness>(m_scannedEndiannessIndex);

        for (const int i : std::views::iota(startIndex, endIndex + 1))
        {
            const int cacheIndex = i - m_cacheWindow.startIndex;
            if (cacheIndex < 0 || cacheIndex >= static_cast<int>(m_cacheWindow.addresses.size()))
            {
                continue;
            }

            const auto& entry = m_cacheWindow.addresses[cacheIndex];

            std::vector<char> currentValue;
            const StatusCode readStatus = m_model->read_process_memory(entry.address, entry.value.size(), currentValue);

            ScannedValue value{};
            value.address = fmt::format("0x{:X}", entry.address);

            if (readStatus == StatusCode::STATUS_OK && !currentValue.empty())
            {
                value.value = Scanner::ValueConverter::format(
                    valueType,
                    reinterpret_cast<const std::uint8_t*>(currentValue.data()),
                    currentValue.size(),
                    m_isHexadecimal,
                    scannedEndianness);
            }
            else
            {
                value.value = "???";
            }

            if (!entry.previousValue.empty())
            {
                value.previousValue = Scanner::ValueConverter::format(
                    valueType, entry.previousValue.data(), entry.previousValue.size(), m_isHexadecimal, scannedEndianness);
            }

            if (!entry.firstValue.empty())
            {
                value.firstValue = Scanner::ValueConverter::format(
                    valueType, entry.firstValue.data(), entry.firstValue.size(), m_isHexadecimal, scannedEndianness);
            }
            else if (!entry.previousValue.empty())
            {
                value.firstValue = Scanner::ValueConverter::format(
                    valueType, entry.previousValue.data(), entry.previousValue.size(), m_isHexadecimal, scannedEndianness);
            }

            m_visibleCache[i] = value;
        }
    }

    void MainViewModel::finalize_scan_results()
    {
        m_model->finalize_scan();

        m_visibleCache.clear();
        m_scannedValues.clear();

        m_cacheWindow.startIndex = -1;
        m_cacheWindow.endIndex = -1;
        m_cacheWindow.addresses.clear();

        notify_property_changed();
    }

    std::int64_t MainViewModel::get_scanned_values_count() const { return static_cast<std::int64_t>(m_model->get_scan_results_count()); }

    std::string MainViewModel::get_value_input() const { return m_valueInput; }

    void MainViewModel::set_value_input(const std::string_view value)
    {
        if (m_valueInput != value)
        {
            m_valueInput = value;
            notify_property_changed();
        }
    }

    std::string MainViewModel::get_value_input2() const { return m_valueInput2; }

    void MainViewModel::set_value_input2(const std::string_view value)
    {
        if (m_valueInput2 != value)
        {
            m_valueInput2 = value;
            notify_property_changed();
        }
    }

    bool MainViewModel::is_hexadecimal() const { return m_isHexadecimal; }

    void MainViewModel::set_hexadecimal(const bool value)
    {
        if (m_isHexadecimal != value)
        {
            m_isHexadecimal = value;
            m_model->set_ui_state_bool("uiState.mainView.hexadecimalEnabled", value);
            notify_property_changed();
        }
    }

    int MainViewModel::get_value_type_index() const { return m_valueTypeIndex; }

    void MainViewModel::set_value_type_index(const int index)
    {
        if (m_valueTypeIndex != index)
        {
            m_valueTypeIndex = index;
            m_scanTypeIndex = 0;
            m_model->set_ui_state_int("uiState.mainView.valueTypeIndex", index);
            m_model->set_ui_state_int("uiState.mainView.scanTypeIndex", 0);
            notify_property_changed();
        }
    }

    int MainViewModel::get_scan_type_index() const { return m_scanTypeIndex; }

    void MainViewModel::set_scan_type_index(const int index)
    {
        if (m_scanTypeIndex != index)
        {
            m_scanTypeIndex = index;
            m_model->set_ui_state_int("uiState.mainView.scanTypeIndex", index);
            notify_property_changed();
        }
    }

    bool MainViewModel::is_alignment_enabled() const { return m_alignmentEnabled; }

    void MainViewModel::set_alignment_enabled(const bool value)
    {
        if (m_alignmentEnabled != value)
        {
            m_alignmentEnabled = value;
            m_model->set_ui_state_bool("uiState.mainView.alignmentEnabled", value);
            notify_property_changed();
        }
    }

    int MainViewModel::get_alignment_value() const { return m_alignmentValue; }

    void MainViewModel::set_alignment_value(const int value)
    {
        if (m_alignmentValue != value)
        {
            m_alignmentValue = value;
            m_model->set_ui_state_int("uiState.mainView.alignmentValue", value);
            notify_property_changed();
        }
    }

    int MainViewModel::get_endianness_type_index() const { return m_endiannessTypeIndex; }

    void MainViewModel::set_endianness_type_index(const int index)
    {
        if (m_endiannessTypeIndex != index)
        {
            m_endiannessTypeIndex = index;
            m_model->set_ui_state_int("uiState.mainView.endiannessTypeIndex", index);
            notify_property_changed();
        }
    }

    bool MainViewModel::is_initial_scan_ready() const
    {
        return m_isInitialScanAvailable;
    }

    bool MainViewModel::is_next_scan_ready() const
    {
        return m_isNextScanAvailable;
    }

    bool MainViewModel::is_undo_scan_ready() const
    {
        return m_model->can_undo_scan();
    }

    bool MainViewModel::is_value_input2_visible() const
    {
        const auto valueType = get_current_value_type();
        if (Scanner::is_string_type(valueType))
        {
            return false;
        }
        return get_actual_numeric_scan_mode() == Scanner::NumericScanMode::Between;
    }

    Theme MainViewModel::get_theme() const { return m_model->get_theme(); }

    std::uint64_t MainViewModel::get_min_process_address() const { return m_minProcessAddress; }

    std::uint64_t MainViewModel::get_max_process_address() const { return m_maxProcessAddress; }

    void MainViewModel::on_process_opened(const Event::ProcessOpenEvent& event)
    {
        m_processInformation = fmt::format("{} [{}]", event.get_process_name(), event.get_process_id());

        m_minProcessAddress = {};
        m_maxProcessAddress = {};
        std::ignore = m_model->get_min_process_address(m_minProcessAddress);
        std::ignore = m_model->get_max_process_address(m_maxProcessAddress);

        notify_view_update(ViewUpdateFlags::PROCESS_INFO);
    }

    bool MainViewModel::is_process_opened() const
    {
        return m_model->is_process_opened() == StatusCode::STATUS_OK;
    }

    int MainViewModel::get_saved_addresses_count() const
    {
        std::scoped_lock lock(m_savedAddressesMutex);
        return static_cast<int>(m_savedAddresses.size());
    }

    SavedAddress MainViewModel::get_saved_address_at(const int index) const
    {
        std::scoped_lock lock(m_savedAddressesMutex);
        if (index >= 0 && index < static_cast<int>(m_savedAddresses.size()))
        {
            return m_savedAddresses[index];
        }
        return SavedAddress{};
    }

    bool MainViewModel::has_saved_address(const std::uint64_t address) const
    {
        std::scoped_lock lock(m_savedAddressesMutex);
        return std::ranges::any_of(m_savedAddresses,
            [address](const SavedAddress& saved) { return saved.address == address; });
    }

    void MainViewModel::add_saved_address(const std::uint64_t address)
    {
        const auto valueType = get_current_value_type();

        SavedAddress saved{};
        saved.frozen = false;
        saved.address = address;
        saved.addressStr = fmt::format("{:016X}", address);
        saved.valueTypeIndex = m_valueTypeIndex;
        saved.valueType = Scanner::get_value_type_name(valueType);

        const auto endianness = static_cast<Scanner::Endianness>(m_endiannessTypeIndex);
        saved.monitoredAddress = m_addressMonitor.get_or_create(address, valueType, endianness);

        if (saved.monitoredAddress)
        {
            std::vector<Scanner::MonitoredAddressPtr> toRefresh = { saved.monitoredAddress };
            m_addressMonitor.refresh(toRefresh, m_isHexadecimal);

            saved.value = saved.monitoredAddress->formattedValue;
        }
        else
        {
            const std::size_t valueSize = Scanner::get_value_type_size(valueType);
            std::vector<char> buffer;
            const StatusCode status = m_model->read_process_memory(address, valueSize, buffer);

            if (status == StatusCode::STATUS_OK && !buffer.empty())
            {
                saved.value = Scanner::ValueConverter::format(
                    valueType,
                    reinterpret_cast<const std::uint8_t*>(buffer.data()),
                    buffer.size(),
                    m_isHexadecimal);
            }
            else
            {
                saved.value = "???";
            }
        }

        {
            std::scoped_lock lock(m_savedAddressesMutex);
            m_savedAddresses.push_back(saved);
        }
        notify_property_changed();
    }

    void MainViewModel::remove_saved_address(const int index)
    {
        {
            std::scoped_lock lock(m_savedAddressesMutex);
            if (index >= 0 && index < static_cast<int>(m_savedAddresses.size()))
            {
                m_savedAddresses.erase(m_savedAddresses.begin() + index);
            }
            else
            {
                return;
            }
        }

        update_frozen_addresses_flag();

        notify_property_changed();
    }

    void MainViewModel::set_saved_address_frozen(const int index, const bool frozen)
    {
        std::uint64_t addressToWrite{};
        std::vector<std::uint8_t> bytesToWrite;

        {
            std::scoped_lock lock(m_savedAddressesMutex);

            if (index < 0 || index >= static_cast<int>(m_savedAddresses.size()))
            {
                return;
            }

            auto& saved = m_savedAddresses[index];
            saved.frozen = frozen;

            if (frozen)
            {
                const auto valueType = static_cast<Scanner::ValueType>(saved.valueTypeIndex);
                std::vector<std::uint8_t> parsedBytes;

                const StatusCode parseStatus = m_model->validate_input(valueType, m_isHexadecimal, saved.value, parsedBytes);

                if (parseStatus == StatusCode::STATUS_OK && !parsedBytes.empty())
                {
                    saved.frozenBytes = parsedBytes;
                }
                else
                {
                    const std::size_t valueSize = Scanner::get_value_type_size(valueType);
                    std::vector<char> buffer;
                    const StatusCode status = m_model->read_process_memory(saved.address, valueSize, buffer);

                    if (status == StatusCode::STATUS_OK && !buffer.empty())
                    {
                        saved.frozenBytes.assign(
                            reinterpret_cast<const std::uint8_t*>(buffer.data()),
                            reinterpret_cast<const std::uint8_t*>(buffer.data()) + buffer.size());
                    }
                }

                if (!saved.frozenBytes.empty())
                {
                    addressToWrite = saved.address;
                    bytesToWrite = saved.frozenBytes;
                }
            }
            else
            {
                saved.frozenBytes.clear();
            }
        }

        if (!bytesToWrite.empty())
        {
            std::packaged_task<StatusCode()> task(
                [address = addressToWrite, bytes = std::move(bytesToWrite), this]() -> StatusCode
                {
                    return m_model->write_process_memory(address, bytes);
                });
            std::ignore = m_dispatcher.dispatch_fire_and_forget(Thread::ThreadChannel::Freeze, std::move(task));
        }

        update_frozen_addresses_flag();

        notify_property_changed();
    }

    void MainViewModel::set_saved_address_value(const int index, const std::string_view value)
    {
        std::uint64_t addressToWrite{};
        std::vector<std::uint8_t> inputBuffer{};
        Scanner::ValueType valueType{};
        bool shouldWrite{};

        {
            std::scoped_lock lock(m_savedAddressesMutex);

            if (index < 0 || index >= static_cast<int>(m_savedAddresses.size()))
            {
                return;
            }

            auto& saved = m_savedAddresses[index];
            valueType = static_cast<Scanner::ValueType>(saved.valueTypeIndex);
            addressToWrite = saved.address;

            const std::string valueStr{value};
            const StatusCode parseStatus = m_model->validate_input(valueType, m_isHexadecimal, valueStr, inputBuffer);

            auto* log = m_model->get_log_service();
            if (log)
            {
                log->log_info(fmt::format("[ValueWrite] Parsing value='{}' for type={}, hex={}, parseStatus={}",
                    valueStr, static_cast<int>(valueType), m_isHexadecimal, static_cast<int>(parseStatus)));
            }

            if (parseStatus == StatusCode::STATUS_OK && !inputBuffer.empty())
            {
                shouldWrite = true;

                if (log)
                {
                    std::string bytesHex{};
                    for (const auto byte : inputBuffer)
                    {
                        if (!bytesHex.empty())
                        {
                            bytesHex += ' ';
                        }
                        bytesHex += fmt::format("{:02X}", byte);
                    }
                    log->log_info(fmt::format("[ValueWrite] Writing {} bytes to 0x{:X}: [{}]",
                        inputBuffer.size(), saved.address, bytesHex));
                }
            }
            else if (log)
            {
                log->log_error(fmt::format("[ValueWrite] Parse FAILED: status={}, bufferEmpty={}",
                    static_cast<int>(parseStatus), inputBuffer.empty()));
            }
        }

        if (shouldWrite)
        {
            const StatusCode writeStatus = m_model->write_process_memory(addressToWrite, inputBuffer);

            auto* log = m_model->get_log_service();
            if (log)
            {
                log->log_info(fmt::format("[ValueWrite] Write result: status={}",
                    static_cast<int>(writeStatus)));
            }

            if (writeStatus == StatusCode::STATUS_OK)
            {
                std::scoped_lock lock(m_savedAddressesMutex);

                if (index < static_cast<int>(m_savedAddresses.size()))
                {
                    auto& saved = m_savedAddresses[index];

                    if (saved.frozen)
                    {
                        saved.frozenBytes = inputBuffer;
                    }

                    saved.value = Scanner::ValueConverter::format(
                        valueType,
                        inputBuffer.data(),
                        inputBuffer.size(),
                        m_isHexadecimal);
                }
            }
            else if (log)
            {
                log->log_error(fmt::format("[ValueWrite] Write FAILED with status {}",
                    static_cast<int>(writeStatus)));
            }
        }

        notify_property_changed();
    }

    void MainViewModel::set_saved_address_address(const int index, const std::uint64_t newAddress)
    {
        if (index >= 0 && index < static_cast<int>(m_savedAddresses.size()))
        {
            auto& saved = m_savedAddresses[index];
            saved.address = newAddress;
            saved.addressStr = fmt::format("{:016X}", newAddress);

            const auto valueType = static_cast<Scanner::ValueType>(saved.valueTypeIndex);
            const auto endianness = static_cast<Scanner::Endianness>(m_endiannessTypeIndex);
            saved.monitoredAddress = m_addressMonitor.get_or_create(newAddress, valueType, endianness);

            refresh_saved_address(index);
            notify_property_changed();
        }
    }

    void MainViewModel::set_saved_address_type(const int index, const int typeIndex)
    {
        if (index >= 0 && index < static_cast<int>(m_savedAddresses.size()))
        {
            auto& saved = m_savedAddresses[index];
            saved.valueTypeIndex = typeIndex;
            saved.valueType = Scanner::get_value_type_name(static_cast<Scanner::ValueType>(typeIndex));

            const auto valueType = static_cast<Scanner::ValueType>(typeIndex);
            const auto endianness = static_cast<Scanner::Endianness>(m_endiannessTypeIndex);
            saved.monitoredAddress = m_addressMonitor.get_or_create(saved.address, valueType, endianness);

            refresh_saved_address(index);
            notify_property_changed();
        }
    }

    void MainViewModel::refresh_saved_address(const int index)
    {
        if (index >= 0 && index < static_cast<int>(m_savedAddresses.size()))
        {
            auto& saved = m_savedAddresses[index];

            if (saved.monitoredAddress)
            {
                std::vector<Scanner::MonitoredAddressPtr> toRefresh = { saved.monitoredAddress };
                m_addressMonitor.refresh(toRefresh, m_isHexadecimal);
                saved.value = saved.monitoredAddress->formattedValue;
            }
            else
            {
                const auto valueType = static_cast<Scanner::ValueType>(saved.valueTypeIndex);
                const std::size_t valueSize = Scanner::get_value_type_size(valueType);

                std::vector<char> buffer;
                const StatusCode status = m_model->read_process_memory(saved.address, valueSize, buffer);

                if (status == StatusCode::STATUS_OK && !buffer.empty())
                {
                    saved.value = Scanner::ValueConverter::format(
                        valueType,
                        reinterpret_cast<const std::uint8_t*>(buffer.data()),
                        buffer.size(),
                        m_isHexadecimal);
                }
                else
                {
                    saved.value = "???";
                }
            }
        }
    }

    void MainViewModel::refresh_all_saved_addresses()
    {
        refresh_saved_addresses_range(0, static_cast<int>(m_savedAddresses.size()) - 1);
    }

    void MainViewModel::refresh_saved_addresses_range(const int startIndex, const int endIndex)
    {
        if (startIndex < 0 || endIndex < startIndex)
        {
            return;
        }

        process_frozen_addresses();

        int actualEnd{};
        std::vector<Scanner::MonitoredAddressPtr> monitoredAddresses;

        {
            std::scoped_lock lock(m_savedAddressesMutex);

            actualEnd = std::min(endIndex, static_cast<int>(m_savedAddresses.size()) - 1);
            if (actualEnd < startIndex)
            {
                return;
            }

            monitoredAddresses.reserve(actualEnd - startIndex + 1);

            for (const int i : std::views::iota(startIndex, actualEnd + 1))
            {
                auto& saved = m_savedAddresses[i];
                if (saved.monitoredAddress)
                {
                    monitoredAddresses.push_back(saved.monitoredAddress);
                }
            }
        }

        if (!monitoredAddresses.empty())
        {
            m_addressMonitor.refresh(monitoredAddresses, m_isHexadecimal);
        }

        {
            std::scoped_lock lock(m_savedAddressesMutex);

            actualEnd = std::min(endIndex, static_cast<int>(m_savedAddresses.size()) - 1);
            if (actualEnd < startIndex)
            {
                return;
            }

            for (const int i : std::views::iota(startIndex, actualEnd + 1))
            {
                auto& saved = m_savedAddresses[i];
                if (saved.monitoredAddress)
                {
                    saved.value = saved.monitoredAddress->formattedValue;
                }
                else
                {
                    const auto valueType = static_cast<Scanner::ValueType>(saved.valueTypeIndex);
                    const std::size_t valueSize = Scanner::get_value_type_size(valueType);

                    std::vector<char> buffer;
                    const StatusCode status = m_model->read_process_memory(saved.address, valueSize, buffer);

                    if (status == StatusCode::STATUS_OK && !buffer.empty())
                    {
                        saved.value = Scanner::ValueConverter::format(
                            valueType,
                            reinterpret_cast<const std::uint8_t*>(buffer.data()),
                            buffer.size(),
                            m_isHexadecimal);
                    }
                    else
                    {
                        saved.value = "???";
                    }
                }
            }
        }
    }

    void MainViewModel::update_available_scan_modes()
    {
        using Mode = Scanner::NumericScanMode;
        if (m_isUnknownScanMode)
        {
            m_availableNumericModes = {
                Mode::Exact, Mode::GreaterThan, Mode::LessThan, Mode::Between,
                Mode::Changed, Mode::Unchanged, Mode::Increased, Mode::Decreased,
                Mode::IncreasedBy, Mode::DecreasedBy
            };
        }
        else
        {
            m_availableNumericModes = {
                Mode::Exact, Mode::GreaterThan, Mode::LessThan, Mode::Between, Mode::Unknown
            };
        }
    }

    bool MainViewModel::is_unknown_scan_mode() const { return m_isUnknownScanMode; }

    Scanner::NumericScanMode MainViewModel::get_actual_numeric_scan_mode() const
    {
        if (m_scanTypeIndex >= 0 && m_scanTypeIndex < static_cast<int>(m_availableNumericModes.size()))
        {
            return m_availableNumericModes[m_scanTypeIndex];
        }
        return Scanner::NumericScanMode::Exact;
    }

    std::uint8_t MainViewModel::get_actual_scan_mode_value() const
    {
        const auto valueType = get_current_value_type();
        if (Scanner::is_string_type(valueType))
        {
            return static_cast<std::uint8_t>(m_scanTypeIndex);
        }
        return static_cast<std::uint8_t>(get_actual_numeric_scan_mode());
    }

    void MainViewModel::reset_scan()
    {
        m_isUnknownScanMode = false;
        m_scanTypeIndex = 0;
        m_isNextScanAvailable = false;
        m_scannedValues.clear();
        m_visibleCache.clear();
        m_cacheWindow = CacheWindow{};
        update_available_scan_modes();
        m_model->set_ui_state_int("uiState.mainView.scanTypeIndex", 0);
        notify_view_update(ViewUpdateFlags::SCAN_MODES | ViewUpdateFlags::BUTTON_STATES | ViewUpdateFlags::SCANNED_VALUES);
    }

    void MainViewModel::process_frozen_addresses()
    {
        if (m_dispatcher.is_channel_busy(Thread::ThreadChannel::Freeze))
        {
            return;
        }

        struct FreezeEntry final
        {
            std::uint64_t address{};
            std::vector<std::uint8_t> bytes{};
        };
        std::vector<FreezeEntry> entriesToWrite;

        {
            std::scoped_lock lock(m_savedAddressesMutex);
            for (const auto& saved : m_savedAddresses)
            {
                if (saved.frozen && !saved.frozenBytes.empty())
                {
                    entriesToWrite.push_back({saved.address, saved.frozenBytes});
                }
            }
        }

        if (entriesToWrite.empty())
        {
            return;
        }

        std::packaged_task<StatusCode()> task(
            [entries = std::move(entriesToWrite), this]() -> StatusCode
            {
                for (const auto& [address, bytes] : entries)
                {
                    std::ignore = m_model->write_process_memory(address, bytes);
                }
                return StatusCode::STATUS_OK;
            });

        std::ignore = m_dispatcher.dispatch_fire_and_forget(Thread::ThreadChannel::Freeze, std::move(task));
    }

    void MainViewModel::start_freeze_timer()
    {
        if (m_freezeTimerRunning.load(std::memory_order_acquire))
        {
            return;
        }

        m_freezeTimerRunning.store(true, std::memory_order_release);
        m_freezeTimerThread = std::make_unique<std::thread>(&MainViewModel::freeze_timer_loop, this);
    }

    void MainViewModel::stop_freeze_timer()
    {
        m_freezeTimerRunning.store(false, std::memory_order_release);

        if (m_freezeTimerThread && m_freezeTimerThread->joinable())
        {
            m_freezeTimerThread->join();
        }

        m_freezeTimerThread.reset();
    }

    void MainViewModel::freeze_timer_loop()
    {
        using namespace std::chrono_literals;

        while (m_freezeTimerRunning.load(std::memory_order_acquire))
        {
            if (m_hasFrozenAddresses.load(std::memory_order_acquire))
            {
                process_frozen_addresses();
            }

            std::this_thread::sleep_for(50ms);
        }
    }

    void MainViewModel::update_frozen_addresses_flag()
    {
        bool hasAnyFrozen{};

        {
            std::scoped_lock lock(m_savedAddressesMutex);
            hasAnyFrozen = std::ranges::any_of(m_savedAddresses,
                [](const SavedAddress& addr) { return addr.frozen && !addr.frozenBytes.empty(); }
            );
        }

        m_hasFrozenAddresses.store(hasAnyFrozen, std::memory_order_release);

        if (hasAnyFrozen)
        {
            start_freeze_timer();
        }
    }

} // namespace Vertex::ViewModel
