//
// Copyright (C) 2026 PHTNC<>.
// Licensed under GPLv3.0 with Plugin Interface exceptions.
//
#include <vertex/viewmodel/mainviewmodel.hh>

#include <algorithm>
#include <chrono>
#include <limits>
#include <ranges>
#include <span>
#include <unordered_map>
#include <utility>

#include <fmt/format.h>

#include <vertex/event/eventid.hh>
#include <vertex/event/types/processcloseevent.hh>
#include <vertex/event/types/viewevent.hh>
#include <vertex/event/types/viewupdateevent.hh>
#include <vertex/model/mainmodel.hh>
#include <vertex/scanner/memoryscanner/imemoryscanner.hh>
#include <vertex/scanner/plugin_value_format.hh>
#include <vertex/scanner/valueconverter.hh>
#include <vertex/thread/threadchannel.hh>

namespace Vertex::ViewModel
{
    MainViewModel::MainViewModel(std::unique_ptr<Model::MainModel> model, Event::EventBus& eventBus, Thread::IThreadDispatcher& dispatcher, Scanner::IScannerRuntimeService& scannerService, std::string name)
        : m_viewModelName{std::move(name)},
          m_model{std::move(model)},
          m_eventBus{eventBus},
          m_dispatcher{dispatcher},
          m_scannerService{scannerService}
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
                  output.assign(reinterpret_cast<const std::uint8_t*>(buffer.data()), reinterpret_cast<const std::uint8_t*>(buffer.data()) + buffer.size());
                  return true;
              }
              return false;
          });

        m_addressMonitor.set_bulk_memory_reader(
          [this](const std::span<const Scanner::BulkReadRequest> requests, std::span<Scanner::BulkReadResult> results) -> StatusCode
          {
              if (requests.size() != results.size())
              {
                  return StatusCode::STATUS_ERROR_INVALID_PARAMETER;
              }

              if (!m_model->supports_bulk_read())
              {
                  return StatusCode::STATUS_ERROR_PLUGIN_FUNCTION_NOT_IMPLEMENTED;
              }

              std::vector<Model::BulkReadEntry> entries(requests.size());
              std::vector<BulkReadResult> modelResults(requests.size());

              for (std::size_t i{}; i < requests.size(); ++i)
              {
                  entries[i] = {requests[i].address, requests[i].size, requests[i].buffer};
              }

              const StatusCode status = m_model->read_process_memory_bulk(entries, modelResults);
              if (status != StatusCode::STATUS_OK)
              {
                  return status;
              }

              for (std::size_t i{}; i < results.size(); ++i)
              {
                  results[i].status = modelResults[i].status;
              }

              return StatusCode::STATUS_OK;
          });

        m_scanCompleteSub = Runtime::SubscriptionGuard<Scanner::IScannerRuntimeService>{
            m_scannerService,
            m_scannerService.subscribe(
                static_cast<Scanner::ScannerEventKindMask>(Scanner::ScannerEventKind::ScanComplete),
                [self = this,
                 weak = std::weak_ptr<std::atomic<bool>>{m_alive},
                 &dispatcher = m_dispatcher](const Scanner::ScannerEvent&)
                {
                    std::packaged_task<StatusCode()> task{
                        [self, weak]() -> StatusCode
                        {
                            const auto alive = weak.lock();
                            if (!alive || !alive->load(std::memory_order_acquire))
                            {
                                return STATUS_OK;
                            }
                            self->notify_view_update(ViewUpdateFlags::SCAN_COMPLETED);
                            return STATUS_OK;
                        }};
                    std::ignore = dispatcher.dispatch_fire_and_forget(Thread::ThreadChannel::UI, std::move(task));
                })};

        m_scanProgressSub = Runtime::SubscriptionGuard<Scanner::IScannerRuntimeService>{
            m_scannerService,
            m_scannerService.subscribe(
                static_cast<Scanner::ScannerEventKindMask>(Scanner::ScannerEventKind::ScanProgress),
                [self = this,
                 weak = std::weak_ptr<std::atomic<bool>>{m_alive},
                 &dispatcher = m_dispatcher](const Scanner::ScannerEvent&)
                {
                    std::packaged_task<StatusCode()> task{
                        [self, weak]() -> StatusCode
                        {
                            const auto alive = weak.lock();
                            if (!alive || !alive->load(std::memory_order_acquire))
                            {
                                return STATUS_OK;
                            }
                            self->notify_view_update(ViewUpdateFlags::SCAN_PROGRESS);
                            return STATUS_OK;
                        }};
                    std::ignore = dispatcher.dispatch_fire_and_forget(Thread::ThreadChannel::UI, std::move(task));
                })};

        m_valuesChangedSub = Runtime::SubscriptionGuard<Scanner::IScannerRuntimeService>{
            m_scannerService,
            m_scannerService.subscribe(
                static_cast<Scanner::ScannerEventKindMask>(Scanner::ScannerEventKind::ValuesChanged),
                [self = this,
                 weak = std::weak_ptr<std::atomic<bool>>{m_alive},
                 &dispatcher = m_dispatcher](const Scanner::ScannerEvent&)
                {
                    std::packaged_task<StatusCode()> task{
                        [self, weak]() -> StatusCode
                        {
                            const auto alive = weak.lock();
                            if (!alive || !alive->load(std::memory_order_acquire))
                            {
                                return STATUS_OK;
                            }
                            self->m_cacheWindow = {};
                            self->m_visibleCache.clear();
                            self->notify_view_update(ViewUpdateFlags::SCANNED_VALUES);
                            return STATUS_OK;
                        }};
                    std::ignore = dispatcher.dispatch_fire_and_forget(Thread::ThreadChannel::UI, std::move(task));
                })};

        subscribe_to_events();
        reload_type_entries();

        constexpr auto registryMask = static_cast<Scanner::ScannerEventKindMask>(
            static_cast<std::uint32_t>(Scanner::ScannerEventKind::TypeRegistered) |
            static_cast<std::uint32_t>(Scanner::ScannerEventKind::TypeUnregistered) |
            static_cast<std::uint32_t>(Scanner::ScannerEventKind::RegistryInvalidated));

        m_registrySub = Runtime::SubscriptionGuard<Scanner::IScannerRuntimeService>{
            m_scannerService,
            m_scannerService.subscribe(
                registryMask,
                [self = this,
                 weak = std::weak_ptr<std::atomic<bool>>{m_alive},
                 &dispatcher = m_dispatcher](const Scanner::ScannerEvent&)
                {
                    std::packaged_task<StatusCode()> task{
                        [self, weak]() -> StatusCode
                        {
                            const auto alive = weak.lock();
                            if (!alive || !alive->load(std::memory_order_acquire))
                            {
                                return STATUS_OK;
                            }
                            self->reload_type_entries();
                            self->notify_view_update(ViewUpdateFlags::DATATYPES | ViewUpdateFlags::SCAN_MODES);
                            return STATUS_OK;
                        }};
                    std::ignore = dispatcher.dispatch_fire_and_forget(Thread::ThreadChannel::UI, std::move(task));
                })};

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
        m_scanCompleteSub.reset();
        m_scanProgressSub.reset();
        m_valuesChangedSub.reset();
        m_registrySub.reset();
        m_alive->store(false, std::memory_order_release);
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

    void MainViewModel::set_event_callback(std::move_only_function<void(Event::EventId, const Event::VertexEvent&) const> callback) { m_eventCallback = std::move(callback); }

    void MainViewModel::notify_property_changed() const { notify_view_update(ViewUpdateFlags::DATATYPES); }

    void MainViewModel::notify_view_update(const ViewUpdateFlags flags) const
    {
        if (m_eventCallback)
        {
            const Event::ViewUpdateEvent event(flags);
            m_eventCallback(Event::VIEW_UPDATE_EVENT, event);
        }
    }

    bool MainViewModel::is_scan_complete() const { return m_model->is_scan_complete(); }

    bool MainViewModel::has_scan_initialization_error() const { return m_scanInitializationFailed; }

    void MainViewModel::kill_process() const { std::ignore = m_model->kill_process(); }

    void MainViewModel::reload_type_entries()
    {
        auto snapshot = m_model->list_scanner_types();
        std::scoped_lock lock{m_typeEntriesMutex};
        m_typeEntries = std::move(snapshot);
    }

    const Scanner::TypeSchema* MainViewModel::current_type_entry() const noexcept
    {
        std::scoped_lock lock{m_typeEntriesMutex};
        if (m_valueTypeIndex < 0 || static_cast<std::size_t>(m_valueTypeIndex) >= m_typeEntries.size())
        {
            return nullptr;
        }
        return &m_typeEntries[static_cast<std::size_t>(m_valueTypeIndex)];
    }

    bool MainViewModel::current_type_is_plugin() const noexcept
    {
        const auto* entry = current_type_entry();
        return entry && entry->kind == Scanner::TypeKind::PluginDefined;
    }

    Scanner::ValueType MainViewModel::get_current_value_type() const
    {
        const auto* entry = current_type_entry();
        if (!entry || entry->kind == Scanner::TypeKind::PluginDefined)
        {
            return Scanner::ValueType::COUNT;
        }
        const auto raw = static_cast<std::uint32_t>(entry->id);
        if (raw == 0)
        {
            return Scanner::ValueType::COUNT;
        }
        return static_cast<Scanner::ValueType>(raw - 1);
    }

    Scanner::TypeId MainViewModel::get_current_type_id() const
    {
        const auto* entry = current_type_entry();
        return entry ? entry->id : Scanner::TypeId::Invalid;
    }

    const DataTypeUIHints* MainViewModel::get_current_type_ui_hints() const
    {
        const auto* entry = current_type_entry();
        if (!entry || entry->kind != Scanner::TypeKind::PluginDefined || !entry->sdkType)
        {
            return nullptr;
        }
        return entry->sdkType->uiHints;
    }

    ::NumericSystem MainViewModel::get_plugin_numeric_base() const
    {
        return m_pluginNumericBase;
    }

    void MainViewModel::set_plugin_numeric_base(::NumericSystem base)
    {
        m_pluginNumericBase = base;
    }

    bool MainViewModel::current_type_supports_endianness() const
    {
        const auto* entry = current_type_entry();
        if (!entry)
        {
            return true;
        }
        if (entry->kind != Scanner::TypeKind::PluginDefined)
        {
            const auto raw = static_cast<std::uint32_t>(entry->id);
            if (raw == 0)
            {
                return true;
            }
            const auto valueType = static_cast<Scanner::ValueType>(raw - 1);
            return Scanner::is_numeric_type(valueType) || Scanner::string_type_has_endianness(valueType);
        }
        if (entry->sdkType && entry->sdkType->uiHints)
        {
            return entry->sdkType->uiHints->supportsEndianness != 0;
        }
        return false;
    }

    Scanner::ValueType MainViewModel::get_scanned_value_type() const { return static_cast<Scanner::ValueType>(m_scannedValueTypeIndex); }

    std::vector<std::string> MainViewModel::get_value_type_names() const
    {
        std::scoped_lock lock{m_typeEntriesMutex};
        return m_typeEntries |
               std::views::transform([](const Scanner::TypeSchema& s) { return s.name; }) |
               std::ranges::to<std::vector>();
    }

    std::vector<std::string> MainViewModel::get_scan_mode_names() const
    {
        const auto* entry = current_type_entry();
        if (entry && entry->kind == Scanner::TypeKind::PluginDefined && entry->sdkType)
        {
            std::vector<std::string> names{};
            names.reserve(entry->sdkType->scanModeCount);
            for (std::size_t i{}; i < entry->sdkType->scanModeCount; ++i)
            {
                const auto* modeName = entry->sdkType->scanModes[i].scanModeName;
                names.emplace_back(modeName ? modeName : "");
            }
            return names;
        }

        const auto valueType = get_current_value_type();

        if (Scanner::is_string_type(valueType))
        {
            auto indices = std::views::iota(0, static_cast<int>(Scanner::StringScanMode::COUNT));
            return indices |
                   std::views::transform(
                     [](const int i)
                     {
                         return Scanner::get_string_scan_mode_name(static_cast<Scanner::StringScanMode>(i));
                     }) |
                   std::ranges::to<std::vector>();
        }

        return m_availableNumericModes |
               std::views::transform(
                 [](const Scanner::NumericScanMode mode)
                 {
                     return Scanner::get_numeric_scan_mode_name(mode);
                 }) |
               std::ranges::to<std::vector>();
    }

    bool MainViewModel::needs_input_value() const
    {
        const auto* entry = current_type_entry();
        if (entry && entry->kind == Scanner::TypeKind::PluginDefined && entry->sdkType)
        {
            if (m_scanTypeIndex < 0 || static_cast<std::size_t>(m_scanTypeIndex) >= entry->sdkType->scanModeCount)
            {
                return false;
            }
            return entry->sdkType->scanModes[static_cast<std::size_t>(m_scanTypeIndex)].needsInput != 0;
        }

        const auto valueType = get_current_value_type();

        if (Scanner::is_string_type(valueType))
        {
            return true;
        }

        return Scanner::scan_mode_needs_input(get_actual_numeric_scan_mode());
    }

    void MainViewModel::initial_scan()
    {
        m_scanInitializationFailed = false;
        m_scannedValueTypeIndex = m_valueTypeIndex;
        m_scannedEndiannessIndex = m_endiannessTypeIndex;
        const auto typeId = get_current_type_id();
        if (typeId == Scanner::TypeId::Invalid)
        {
            m_scanInitializationFailed = true;
            m_scanProgress = {0, 0, "No type selected"};
            notify_property_changed();
            return;
        }
        const bool isPlugin = current_type_is_plugin();
        m_scannedTypeId = typeId;
        m_scannedTypeIsPlugin = isPlugin;
        m_scannedPluginSchema = isPlugin ? m_model->find_scanner_type(typeId) : std::nullopt;
        std::vector<std::uint8_t> inputBuffer{};
        std::vector<std::uint8_t> inputBuffer2{};

        if (needs_input_value() && !m_valueInput.empty())
        {
            const StatusCode status = isPlugin
                ? m_model->validate_input(typeId, m_pluginNumericBase, m_valueInput, inputBuffer)
                : m_model->validate_input(typeId, m_isHexadecimal, m_valueInput, inputBuffer);
            if (status != StatusCode::STATUS_OK) [[unlikely]]
            {
                m_scanInitializationFailed = true;
                m_scanProgress = {0, 0, "Input validation failed"};
                notify_property_changed();
                return;
            }
        }

        const auto actualMode = get_actual_numeric_scan_mode();
        if (!isPlugin && actualMode == Scanner::NumericScanMode::Between && !m_valueInput2.empty())
        {
            const StatusCode status = m_model->validate_input(typeId, m_isHexadecimal, m_valueInput2, inputBuffer2);
            if (status != StatusCode::STATUS_OK) [[unlikely]]
            {
                m_scanInitializationFailed = true;
                m_scanProgress = {0, 0, "Input2 validation failed"};
                notify_property_changed();
                return;
            }
        }

        const auto scanModeValue = isPlugin
            ? static_cast<std::uint32_t>(m_scanTypeIndex)
            : static_cast<std::uint32_t>(get_actual_scan_mode_value());
        const StatusCode status = m_model->initialize_scan(typeId, scanModeValue, m_isHexadecimal, m_alignmentEnabled, static_cast<std::size_t>(m_alignmentValue),
                                                           static_cast<Scanner::Endianness>(m_endiannessTypeIndex), inputBuffer, inputBuffer2);

        if (status != StatusCode::STATUS_OK) [[unlikely]]
        {
            m_scanInitializationFailed = true;
            m_scanProgress = {0, 0, "Scan initialization failed"};
            notify_property_changed();
            return;
        }

        if (!isPlugin)
        {
            const auto valueType = get_current_value_type();
            if (!Scanner::is_string_type(valueType) && actualMode == Scanner::NumericScanMode::Unknown)
            {
                m_isUnknownScanMode = true;
                update_available_scan_modes();
                m_scanTypeIndex = 0;
            }
        }

        m_scannedValues.clear();

        m_scanProgress = {0, 0, "Scanning..."};
        m_isNextScanAvailable = false;
        notify_property_changed();
    }

    void MainViewModel::next_scan()
    {
        if (m_nextScanInitFuture.valid())
        {
            return;
        }

        m_scanInitializationFailed = false;
        m_scannedValueTypeIndex = m_valueTypeIndex;
        m_scannedEndiannessIndex = m_endiannessTypeIndex;
        const auto typeId = get_current_type_id();
        if (typeId == Scanner::TypeId::Invalid)
        {
            m_scanInitializationFailed = true;
            m_scanProgress = {0, 0, "No type selected"};
            notify_property_changed();
            return;
        }
        const bool isPlugin = current_type_is_plugin();
        m_scannedTypeId = typeId;
        m_scannedTypeIsPlugin = isPlugin;
        m_scannedPluginSchema = isPlugin ? m_model->find_scanner_type(typeId) : std::nullopt;
        std::vector<std::uint8_t> inputBuffer{};
        std::vector<std::uint8_t> inputBuffer2{};

        if (needs_input_value() && !m_valueInput.empty())
        {
            const StatusCode status = isPlugin
                ? m_model->validate_input(typeId, m_pluginNumericBase, m_valueInput, inputBuffer)
                : m_model->validate_input(typeId, m_isHexadecimal, m_valueInput, inputBuffer);
            if (status != StatusCode::STATUS_OK) [[unlikely]]
            {
                m_scanInitializationFailed = true;
                m_scanProgress = {0, 0, "Input validation failed"};
                notify_property_changed();
                return;
            }
        }

        const auto actualMode = get_actual_numeric_scan_mode();
        if (!isPlugin && actualMode == Scanner::NumericScanMode::Between && !m_valueInput2.empty())
        {
            const StatusCode status = m_model->validate_input(typeId, m_isHexadecimal, m_valueInput2, inputBuffer2);
            if (status != StatusCode::STATUS_OK) [[unlikely]]
            {
                m_scanInitializationFailed = true;
                m_scanProgress = {0, 0, "Input2 validation failed"};
                notify_property_changed();
                return;
            }
        }

        const auto scanMode = isPlugin
            ? static_cast<std::uint32_t>(m_scanTypeIndex)
            : static_cast<std::uint32_t>(get_actual_scan_mode_value());
        const auto alignmentValue = static_cast<std::size_t>(m_alignmentValue);
        const auto endianness = static_cast<Scanner::Endianness>(m_endiannessTypeIndex);

        std::packaged_task<StatusCode()> task(
          [this, typeId, scanMode, hexDisplay = m_isHexadecimal, alignmentEnabled = m_alignmentEnabled, alignmentValue, endianness, input = std::move(inputBuffer), input2 = std::move(inputBuffer2)]() mutable -> StatusCode
          {
              const StatusCode status = m_model->initialize_next_scan(typeId, scanMode, hexDisplay, alignmentEnabled, alignmentValue, endianness, input, input2);

              notify_view_update(ViewUpdateFlags::SCAN_PROGRESS);
              return status;
          });

        auto dispatchResult = m_dispatcher.dispatch(Thread::ThreadChannel::Scanner, std::move(task));
        if (!dispatchResult.has_value())
        {
            m_scanInitializationFailed = true;
            m_scanProgress = {0, 0, "Next scan initialization failed"};
            m_isNextScanAvailable = (m_model->get_scan_results_count() > 0);
            notify_property_changed();
            return;
        }

        m_nextScanInitFuture = std::move(dispatchResult.value());
        m_scanProgress = {0, 0, "Preparing next scan index..."};
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
        if (m_nextScanInitFuture.valid())
        {
            if (m_nextScanInitFuture.wait_for(std::chrono::milliseconds(0)) != std::future_status::ready)
            {
                const auto pendingResults = m_model->get_scan_results_count();
                m_scanProgress.current = 0;
                m_scanProgress.total = 0;
                m_scanProgress.statusMessage = fmt::format("Preparing next scan index... {} results", pendingResults);
                m_isNextScanAvailable = false;
                return;
            }

            const StatusCode initStatus = m_nextScanInitFuture.get();
            if (initStatus != StatusCode::STATUS_OK)
            {
                m_scanInitializationFailed = true;
                m_scanProgress.current = 0;
                m_scanProgress.total = 0;
                m_scanProgress.statusMessage = "Next scan initialization failed";
                m_isNextScanAvailable = (m_model->get_scan_results_count() > 0);
                return;
            }
        }

        if (m_scanInitializationFailed)
        {
            return;
        }

        const auto current = m_model->get_scan_progress_current();
        const auto total = m_model->get_scan_progress_total();
        const auto results = m_model->get_scan_results_count();
        const bool scanComplete = m_model->is_scan_complete();

        m_scanInitializationFailed = false;
        m_scanProgress.current = static_cast<std::int64_t>(current);
        m_scanProgress.total = static_cast<std::int64_t>(total);
        if (scanComplete)
        {
            m_scanProgress.statusMessage = fmt::format("Scan complete! Found {} results", results);
            m_isNextScanAvailable = (results > 0);
            return;
        }

        if (current >= total && total > 0)
        {
            m_scanProgress.statusMessage = fmt::format("Finalizing... {} results", results);
            m_isNextScanAvailable = false;
            return;
        }

        m_scanProgress.statusMessage = fmt::format("Scanning... {}/{} regions, {} results", current, total, results);
        m_isNextScanAvailable = false;
    }

    void MainViewModel::open_project() const {}

    void MainViewModel::exit_application() const
    {
        m_eventBus.broadcast(Event::ViewEvent(Event::APPLICATION_SHUTDOWN_EVENT));
        wxTheApp->ExitMainLoop();
    }

    void MainViewModel::open_memory_view() const {}

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

    void MainViewModel::open_scripting_window() const
    {
        const Event::ViewEvent event{Event::VIEW_EVENT};
        m_eventBus.broadcast_to(ViewModelName::SCRIPTING, event);
    }

    void MainViewModel::close_process_state()
    {
        std::ignore = m_model->close_process();

        m_isInitialScanAvailable = false;
        m_isNextScanAvailable = false;
        m_isUnknownScanMode = false;
        m_scanInitializationFailed = false;
        m_minProcessAddress = {};
        m_maxProcessAddress = {};
        update_available_scan_modes();
        m_scanTypeIndex = 0;

        stop_freeze_timer();

        const Event::ProcessCloseEvent event{Event::PROCESS_CLOSED_EVENT};
        m_eventBus.broadcast(event);
    }

    void MainViewModel::get_file_executable_extensions(std::vector<std::string>& extensions) const { std::ignore = m_model->get_file_executable_extensions(extensions); }

    StatusCode MainViewModel::open_new_process(const std::string_view processPath, const int argc, const char** argv) const { return m_model->open_new_process(processPath, argc, argv); }

    std::string MainViewModel::get_process_information() const { return m_processInformation; }

    void MainViewModel::set_process_information(const std::string_view informationText) { m_processInformation = informationText; }

    ScanProgress MainViewModel::get_scan_progress() const { return m_scanProgress; }

    std::vector<ScannedValue> MainViewModel::get_scanned_values() const { return m_scannedValues; }

    std::uint32_t MainViewModel::get_scanned_value_size() const
    {
        if (m_scannedTypeIsPlugin)
        {
            if (m_scannedPluginSchema && m_scannedPluginSchema->valueSize > 0)
            {
                return static_cast<std::uint32_t>(m_scannedPluginSchema->valueSize);
            }
            return 0;
        }

        const auto valueType = static_cast<Scanner::ValueType>(m_scannedValueTypeIndex);
        const auto size = Scanner::get_value_type_size(valueType);
        return size > 0 ? static_cast<std::uint32_t>(size) : 0;
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
                value.address = fmt::format("{:016X}", entry.address);

                if (m_scannedTypeIsPlugin)
                {
                    if (m_scannedPluginSchema)
                    {
                        const auto& schema = *m_scannedPluginSchema;
                        if (!entry.value.empty())
                        {
                            value.value = Scanner::format_plugin_bytes(schema, entry.value.data(), entry.value.size());
                        }
                        if (!entry.previousValue.empty())
                        {
                            value.previousValue = Scanner::format_plugin_bytes(schema, entry.previousValue.data(), entry.previousValue.size());
                        }
                        if (!entry.firstValue.empty())
                        {
                            value.firstValue = Scanner::format_plugin_bytes(schema, entry.firstValue.data(), entry.firstValue.size());
                        }
                        else if (!entry.previousValue.empty())
                        {
                            value.firstValue = Scanner::format_plugin_bytes(schema, entry.previousValue.data(), entry.previousValue.size());
                        }
                    }
                    m_visibleCache[index] = value;
                    return value;
                }

                const auto valueType = get_scanned_value_type();
                const auto scannedEndianness = static_cast<Scanner::Endianness>(m_scannedEndiannessIndex);
                if (!entry.value.empty())
                {
                    value.value = Scanner::ValueConverter::format(valueType, entry.value.data(), entry.value.size(), m_isHexadecimal, scannedEndianness);
                }

                if (!entry.previousValue.empty())
                {
                    value.previousValue = Scanner::ValueConverter::format(valueType, entry.previousValue.data(), entry.previousValue.size(), m_isHexadecimal, scannedEndianness);
                }

                if (!entry.firstValue.empty())
                {
                    value.firstValue = Scanner::ValueConverter::format(valueType, entry.firstValue.data(), entry.firstValue.size(), m_isHexadecimal, scannedEndianness);
                }
                else if (!entry.previousValue.empty())
                {
                    value.firstValue = Scanner::ValueConverter::format(valueType, entry.previousValue.data(), entry.previousValue.size(), m_isHexadecimal, scannedEndianness);
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
        value.address = fmt::format("{:016X}", scanResults[0].address);

        if (m_scannedTypeIsPlugin)
        {
            if (m_scannedPluginSchema)
            {
                const auto& schema = *m_scannedPluginSchema;
                if (!scanResults[0].value.empty())
                {
                    value.value = Scanner::format_plugin_bytes(schema, scanResults[0].value.data(), scanResults[0].value.size());
                }
                if (!scanResults[0].previousValue.empty())
                {
                    value.previousValue = Scanner::format_plugin_bytes(schema, scanResults[0].previousValue.data(), scanResults[0].previousValue.size());
                }
                if (!scanResults[0].firstValue.empty())
                {
                    value.firstValue = Scanner::format_plugin_bytes(schema, scanResults[0].firstValue.data(), scanResults[0].firstValue.size());
                }
                else if (!scanResults[0].previousValue.empty())
                {
                    value.firstValue = Scanner::format_plugin_bytes(schema, scanResults[0].previousValue.data(), scanResults[0].previousValue.size());
                }
            }
            m_visibleCache[index] = value;
            return value;
        }

        const auto valueType = get_scanned_value_type();
        const auto scannedEndianness = static_cast<Scanner::Endianness>(m_scannedEndiannessIndex);
        if (!scanResults[0].value.empty())
        {
            value.value = Scanner::ValueConverter::format(valueType, scanResults[0].value.data(), scanResults[0].value.size(), m_isHexadecimal, scannedEndianness);
        }

        if (!scanResults[0].previousValue.empty())
        {
            value.previousValue = Scanner::ValueConverter::format(valueType, scanResults[0].previousValue.data(), scanResults[0].previousValue.size(), m_isHexadecimal, scannedEndianness);
        }

        if (!scanResults[0].firstValue.empty())
        {
            value.firstValue = Scanner::ValueConverter::format(valueType, scanResults[0].firstValue.data(), scanResults[0].firstValue.size(), m_isHexadecimal, scannedEndianness);
        }
        else if (!scanResults[0].previousValue.empty())
        {
            value.firstValue = Scanner::ValueConverter::format(valueType, scanResults[0].previousValue.data(), scanResults[0].previousValue.size(), m_isHexadecimal, scannedEndianness);
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

        const int expectedCount = newEndIndex - newStartIndex;
        const bool cacheWindowFullyPopulated = static_cast<int>(m_cacheWindow.addresses.size()) == expectedCount;

        if (newStartIndex == m_cacheWindow.startIndex && newEndIndex == m_cacheWindow.endIndex && cacheWindowFullyPopulated) [[unlikely]]
        {
            return;
        }

        if (expectedCount <= 0) [[unlikely]]
        {
            m_cacheWindow.startIndex = -1;
            m_cacheWindow.endIndex = -1;
            m_cacheWindow.addresses.clear();
            return;
        }

        std::vector<Scanner::IMemoryScanner::ScanResultEntry> newAddresses;
        const StatusCode status = m_model->get_scan_results_range(newAddresses, newStartIndex, expectedCount);
        if (status != StatusCode::STATUS_OK)
        {
            return;
        }

        const int actualCount = std::min(expectedCount, static_cast<int>(newAddresses.size()));
        newAddresses.resize(static_cast<std::size_t>(actualCount));

        if (actualCount <= 0)
        {
            m_cacheWindow.startIndex = -1;
            m_cacheWindow.endIndex = -1;
            m_cacheWindow.addresses.clear();
            return;
        }

        const int prevStart = m_cacheWindow.startIndex;
        const int prevEnd = m_cacheWindow.endIndex;

        m_cacheWindow.startIndex = newStartIndex;
        m_cacheWindow.endIndex = newStartIndex + actualCount;
        m_cacheWindow.addresses = std::move(newAddresses);

        if (prevStart >= 0 && prevEnd > prevStart)
        {
            std::erase_if(m_visibleCache,
                          [newStartIndex, newEnd = m_cacheWindow.endIndex](const auto& pair)
                          {
                              return pair.first < newStartIndex || pair.first >= newEnd;
                          });
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

        struct VisibleRefreshEntry final
        {
            int visibleIndex{};
            const Scanner::IMemoryScanner::ScanResultEntry* cacheEntry{};
            std::vector<char> currentValue{};
            bool readOk{};
        };

        std::vector<VisibleRefreshEntry> pendingReads{};
        pendingReads.reserve(static_cast<std::size_t>(count));

        for (const int i : std::views::iota(startIndex, endIndex + 1))
        {
            const int cacheIndex = i - m_cacheWindow.startIndex;
            if (cacheIndex < 0 || cacheIndex >= static_cast<int>(m_cacheWindow.addresses.size()))
            {
                continue;
            }

            const auto& entry = m_cacheWindow.addresses[cacheIndex];
            auto& pending = pendingReads.emplace_back();
            pending.visibleIndex = i;
            pending.cacheEntry = &entry;
            pending.currentValue.resize(entry.value.size());
        }

        bool usedBulkRead = false;
        if (!pendingReads.empty() && m_model->supports_bulk_read())
        {
            std::vector<Model::BulkReadEntry> bulkEntries(pendingReads.size());
            std::vector<BulkReadResult> bulkResults(pendingReads.size());

            for (std::size_t i{}; i < pendingReads.size(); ++i)
            {
                bulkEntries[i] = {pendingReads[i].cacheEntry->address, pendingReads[i].currentValue.size(), pendingReads[i].currentValue.data()};
            }

            const StatusCode bulkStatus = m_model->read_process_memory_bulk(bulkEntries, bulkResults);
            if (bulkStatus == StatusCode::STATUS_OK)
            {
                usedBulkRead = true;
                for (std::size_t i{}; i < pendingReads.size(); ++i)
                {
                    pendingReads[i].readOk = bulkResults[i].status == StatusCode::STATUS_OK && !pendingReads[i].currentValue.empty();
                }
            }
        }

        if (!usedBulkRead)
        {
            for (auto& pending : pendingReads)
            {
                const StatusCode readStatus = m_model->read_process_memory(pending.cacheEntry->address, pending.currentValue.size(), pending.currentValue);
                pending.readOk = readStatus == StatusCode::STATUS_OK && !pending.currentValue.empty();
            }
        }

        const bool isPlugin = m_scannedTypeIsPlugin && m_scannedPluginSchema.has_value();
        const Scanner::TypeSchema* pluginSchema = isPlugin ? &*m_scannedPluginSchema : nullptr;

        for (const auto& [visibleIndex, cacheEntry, currentValue, readOk] : pendingReads)
        {
            const auto& entry = *cacheEntry;

            ScannedValue value{};
            value.address = fmt::format("{:016X}", entry.address);

            if (isPlugin)
            {
                if (readOk)
                {
                    value.value = Scanner::format_plugin_bytes(*pluginSchema, currentValue.data(), currentValue.size());
                }
                else
                {
                    value.value = "???";
                }

                if (!entry.previousValue.empty())
                {
                    value.previousValue = Scanner::format_plugin_bytes(*pluginSchema, entry.previousValue.data(), entry.previousValue.size());
                }

                if (!entry.firstValue.empty())
                {
                    value.firstValue = Scanner::format_plugin_bytes(*pluginSchema, entry.firstValue.data(), entry.firstValue.size());
                }
                else if (!entry.previousValue.empty())
                {
                    value.firstValue = Scanner::format_plugin_bytes(*pluginSchema, entry.previousValue.data(), entry.previousValue.size());
                }
            }
            else
            {
                if (readOk)
                {
                    value.value = Scanner::ValueConverter::format(valueType, reinterpret_cast<const std::uint8_t*>(currentValue.data()), currentValue.size(), m_isHexadecimal, scannedEndianness);
                }
                else
                {
                    value.value = "???";
                }

                if (!entry.previousValue.empty())
                {
                    value.previousValue = Scanner::ValueConverter::format(valueType, entry.previousValue.data(), entry.previousValue.size(), m_isHexadecimal, scannedEndianness);
                }

                if (!entry.firstValue.empty())
                {
                    value.firstValue = Scanner::ValueConverter::format(valueType, entry.firstValue.data(), entry.firstValue.size(), m_isHexadecimal, scannedEndianness);
                }
                else if (!entry.previousValue.empty())
                {
                    value.firstValue = Scanner::ValueConverter::format(valueType, entry.previousValue.data(), entry.previousValue.size(), m_isHexadecimal, scannedEndianness);
                }
            }

            m_visibleCache[visibleIndex] = value;
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

    std::optional<std::uint64_t> MainViewModel::get_scanned_result_address_at(const int index) const
    {
        if (index < 0)
        {
            return std::nullopt;
        }

        if (m_cacheWindow.startIndex >= 0 && index >= m_cacheWindow.startIndex && index < m_cacheWindow.endIndex)
        {
            const int cacheIndex = index - m_cacheWindow.startIndex;
            if (cacheIndex >= 0 && cacheIndex < static_cast<int>(m_cacheWindow.addresses.size()))
            {
                return m_cacheWindow.addresses[cacheIndex].address;
            }
        }

        std::vector<Scanner::IMemoryScanner::ScanResultEntry> scanResults{};
        const StatusCode status = m_model->get_scan_results_range(scanResults, index, 1);
        if (status != StatusCode::STATUS_OK || scanResults.empty())
        {
            return std::nullopt;
        }

        return scanResults[0].address;
    }

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

    bool MainViewModel::is_initial_scan_ready() const { return m_isInitialScanAvailable; }

    bool MainViewModel::is_next_scan_ready() const { return m_isNextScanAvailable; }

    bool MainViewModel::is_undo_scan_ready() const { return m_model->can_undo_scan(); }

    bool MainViewModel::is_value_input2_visible() const
    {
        if (current_type_is_plugin())
        {
            return false;
        }
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

    bool MainViewModel::is_process_opened() const { return m_model->is_process_opened() == StatusCode::STATUS_OK; }

    std::optional<std::reference_wrapper<Log::ILog>> MainViewModel::get_log_service() const
    {
        auto* log = m_model->get_log_service();
        if (log)
        {
            return std::ref(*log);
        }
        return std::nullopt;
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

    std::uint32_t MainViewModel::get_saved_address_watch_size(const int index) const
    {
        SavedAddress saved{};
        {
            std::scoped_lock lock(m_savedAddressesMutex);
            if (index < 0 || index >= static_cast<int>(m_savedAddresses.size()))
            {
                return 0;
            }
            saved = m_savedAddresses[index];
        }

        if (saved.monitoredAddress && saved.monitoredAddress->valueSize > 0)
        {
            return saved.monitoredAddress->valueSize;
        }

        const bool isPlugin = saved.valueTypeIndex < 0
            || saved.valueTypeIndex >= static_cast<int>(Scanner::ValueType::COUNT);
        if (isPlugin)
        {
            const auto schema = m_model->find_scanner_type(saved.typeId);
            if (!schema || schema->valueSize == 0)
            {
                return 0;
            }
            return static_cast<std::uint32_t>(schema->valueSize);
        }

        const auto valueType = static_cast<Scanner::ValueType>(saved.valueTypeIndex);
        const auto size = Scanner::get_value_type_size(valueType);
        return size > 0 ? static_cast<std::uint32_t>(size) : 0;
    }

    bool MainViewModel::has_saved_address(const std::uint64_t address) const
    {
        std::scoped_lock lock(m_savedAddressesMutex);
        return std::ranges::any_of(m_savedAddresses,
                                   [address](const SavedAddress& saved)
                                   {
                                       return saved.address == address;
                                   });
    }

    void MainViewModel::add_saved_address(const std::uint64_t address)
    {
        const auto* entry = current_type_entry();
        if (!entry)
        {
            return;
        }

        SavedAddress saved{};
        saved.frozen = false;
        saved.address = address;
        saved.addressStr = fmt::format("{:016X}", address);
        saved.typeId = entry->id;
        saved.valueType = entry->name;

        if (entry->kind == Scanner::TypeKind::PluginDefined)
        {
            saved.valueTypeIndex = -1;
            auto schema = std::make_shared<const Scanner::TypeSchema>(*entry);
            saved.monitoredAddress = m_addressMonitor.get_or_create_plugin(address, schema);
            if (saved.monitoredAddress)
            {
                std::vector<Scanner::MonitoredAddressPtr> toRefresh = {saved.monitoredAddress};
                m_addressMonitor.refresh(toRefresh, m_isHexadecimal);
                saved.value = saved.monitoredAddress->formattedValue;
            }
            else
            {
                saved.value = "";
            }
        }
        else
        {
            const auto valueType = get_current_value_type();
            saved.valueTypeIndex = static_cast<int>(valueType);

            const auto endianness = static_cast<Scanner::Endianness>(m_endiannessTypeIndex);
            saved.monitoredAddress = m_addressMonitor.get_or_create(address, valueType, endianness);

            if (saved.monitoredAddress)
            {
                std::vector<Scanner::MonitoredAddressPtr> toRefresh = {saved.monitoredAddress};
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
                    saved.value = Scanner::ValueConverter::format(valueType, reinterpret_cast<const std::uint8_t*>(buffer.data()), buffer.size(), m_isHexadecimal);
                }
                else
                {
                    saved.value = "???";
                }
            }
        }

        {
            std::scoped_lock lock(m_savedAddressesMutex);
            m_savedAddresses.push_back(saved);
        }
        notify_property_changed();
    }

    void MainViewModel::add_saved_address(const std::uint64_t address, const int dropdownIndex)
    {
        Scanner::TypeSchema entrySnapshot{};
        {
            std::scoped_lock lock{m_typeEntriesMutex};
            if (dropdownIndex < 0 || static_cast<std::size_t>(dropdownIndex) >= m_typeEntries.size())
            {
                return;
            }
            entrySnapshot = m_typeEntries[static_cast<std::size_t>(dropdownIndex)];
        }

        SavedAddress saved{};
        saved.frozen = false;
        saved.address = address;
        saved.addressStr = fmt::format("{:016X}", address);
        saved.typeId = entrySnapshot.id;
        saved.valueType = entrySnapshot.name;

        if (entrySnapshot.kind == Scanner::TypeKind::PluginDefined)
        {
            saved.valueTypeIndex = -1;
            auto schema = std::make_shared<const Scanner::TypeSchema>(entrySnapshot);
            saved.monitoredAddress = m_addressMonitor.get_or_create_plugin(address, schema);
            if (saved.monitoredAddress)
            {
                std::vector<Scanner::MonitoredAddressPtr> toRefresh = {saved.monitoredAddress};
                m_addressMonitor.refresh(toRefresh, m_isHexadecimal);
                saved.value = saved.monitoredAddress->formattedValue;
            }
            else
            {
                saved.value = "";
            }
        }
        else
        {
            const auto raw = static_cast<std::uint32_t>(entrySnapshot.id);
            const auto valueType = static_cast<Scanner::ValueType>(raw - 1);
            saved.valueTypeIndex = static_cast<int>(valueType);

            const auto endianness = static_cast<Scanner::Endianness>(m_endiannessTypeIndex);
            saved.monitoredAddress = m_addressMonitor.get_or_create(address, valueType, endianness);

            if (saved.monitoredAddress)
            {
                std::vector<Scanner::MonitoredAddressPtr> toRefresh = {saved.monitoredAddress};
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
                    saved.value = Scanner::ValueConverter::format(valueType, reinterpret_cast<const std::uint8_t*>(buffer.data()), buffer.size(), m_isHexadecimal);
                }
                else
                {
                    saved.value = "???";
                }
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
                const auto effectiveId = saved.effective_type_id();
                if (effectiveId == Scanner::TypeId::Invalid)
                {
                    saved.frozen = false;
                    saved.frozenBytes.clear();
                    return;
                }

                const auto schema = m_model->find_scanner_type(effectiveId);
                std::size_t fallbackSize{};
                if (saved.valueTypeIndex >= 0 && saved.valueTypeIndex < static_cast<int>(Scanner::ValueType::COUNT))
                {
                    fallbackSize = Scanner::get_value_type_size(static_cast<Scanner::ValueType>(saved.valueTypeIndex));
                }
                else if (schema)
                {
                    fallbackSize = schema->valueSize;
                }

                const bool isPluginType = schema && schema->kind == Scanner::TypeKind::PluginDefined;
                std::vector<std::uint8_t> parsedBytes;
                const StatusCode parseStatus = isPluginType
                    ? m_model->validate_input(effectiveId, m_pluginNumericBase, saved.value, parsedBytes)
                    : m_model->validate_input(effectiveId, m_isHexadecimal, saved.value, parsedBytes);

                if (parseStatus == StatusCode::STATUS_OK && !parsedBytes.empty())
                {
                    saved.frozenBytes = parsedBytes;
                }
                else if (fallbackSize > 0)
                {
                    std::vector<char> buffer;
                    const StatusCode status = m_model->read_process_memory(saved.address, fallbackSize, buffer);

                    if (status == StatusCode::STATUS_OK && !buffer.empty())
                    {
                        saved.frozenBytes.assign(reinterpret_cast<const std::uint8_t*>(buffer.data()), reinterpret_cast<const std::uint8_t*>(buffer.data()) + buffer.size());
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
            std::ignore = m_model->write_process_memory(addressToWrite, bytesToWrite);
        }

        update_frozen_addresses_flag();

        notify_property_changed();
    }

    void MainViewModel::set_saved_address_value(const int index, const std::string_view value)
    {
        std::uint64_t addressToWrite{};
        std::vector<std::uint8_t> inputBuffer{};
        Scanner::TypeId typeId{Scanner::TypeId::Invalid};
        bool shouldWrite{};

        {
            std::scoped_lock lock(m_savedAddressesMutex);

            if (index < 0 || index >= static_cast<int>(m_savedAddresses.size()))
            {
                return;
            }

            auto& saved = m_savedAddresses[index];
            typeId = saved.effective_type_id();
            if (typeId == Scanner::TypeId::Invalid)
            {
                return;
            }
            addressToWrite = saved.address;

            const std::string valueStr{value};
            const auto schema = m_model->find_scanner_type(typeId);
            const bool isPluginType = schema && schema->kind == Scanner::TypeKind::PluginDefined;
            const StatusCode parseStatus = isPluginType
                ? m_model->validate_input(typeId, m_pluginNumericBase, valueStr, inputBuffer)
                : m_model->validate_input(typeId, m_isHexadecimal, valueStr, inputBuffer);

            auto* log = m_model->get_log_service();
            if (log)
            {
                log->log_info(fmt::format("[ValueWrite] Parsing value='{}' for typeId={}, plugin={}, hex={}, pluginBase={}, parseStatus={}", valueStr, static_cast<std::uint32_t>(typeId), isPluginType, m_isHexadecimal, static_cast<int>(m_pluginNumericBase), static_cast<int>(parseStatus)));
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
                    log->log_info(fmt::format("[ValueWrite] Writing {} bytes to 0x{:X}: [{}]", inputBuffer.size(), saved.address, bytesHex));
                }
            }
            else if (log)
            {
                log->log_error(fmt::format("[ValueWrite] Parse FAILED: status={}, bufferEmpty={}", static_cast<int>(parseStatus), inputBuffer.empty()));
            }
        }

        if (shouldWrite)
        {
            const StatusCode writeStatus = m_model->write_process_memory(addressToWrite, inputBuffer);

            auto* log = m_model->get_log_service();
            if (log)
            {
                log->log_info(fmt::format("[ValueWrite] Write result: status={}", static_cast<int>(writeStatus)));
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

                    if (saved.valueTypeIndex >= 0 && saved.valueTypeIndex < static_cast<int>(Scanner::ValueType::COUNT))
                    {
                        const auto valueType = static_cast<Scanner::ValueType>(saved.valueTypeIndex);
                        saved.value = Scanner::ValueConverter::format(valueType, inputBuffer.data(), inputBuffer.size(), m_isHexadecimal);
                    }
                    else
                    {
                        const auto schema = m_model->find_scanner_type(saved.typeId);
                        if (schema && schema->kind == Scanner::TypeKind::PluginDefined)
                        {
                            saved.value = Scanner::format_plugin_bytes(*schema, inputBuffer.data(), inputBuffer.size());
                        }
                    }
                }
            }
            else if (log)
            {
                log->log_error(fmt::format("[ValueWrite] Write FAILED with status {}", static_cast<int>(writeStatus)));
            }
        }

        notify_property_changed();
    }

    void MainViewModel::set_saved_address_address(const int index, const std::uint64_t newAddress)
    {
        if (index < 0 || index >= static_cast<int>(m_savedAddresses.size()))
        {
            return;
        }

        auto& saved = m_savedAddresses[index];
        saved.address = newAddress;
        saved.addressStr = fmt::format("{:016X}", newAddress);

        if (saved.valueTypeIndex < 0 ||
            saved.valueTypeIndex >= static_cast<int>(Scanner::ValueType::COUNT))
        {
            saved.monitoredAddress.reset();
            const auto schema = m_model->find_scanner_type(saved.typeId);
            if (schema && schema->kind == Scanner::TypeKind::PluginDefined)
            {
                auto schemaPtr = std::make_shared<const Scanner::TypeSchema>(*schema);
                saved.monitoredAddress = m_addressMonitor.get_or_create_plugin(newAddress, schemaPtr);
            }
        }
        else
        {
            const auto valueType = static_cast<Scanner::ValueType>(saved.valueTypeIndex);
            const auto endianness = static_cast<Scanner::Endianness>(m_endiannessTypeIndex);
            saved.monitoredAddress = m_addressMonitor.get_or_create(newAddress, valueType, endianness);
        }

        refresh_saved_address(index);
        notify_property_changed();
    }

    void MainViewModel::set_saved_address_type(const int index, const int dropdownIndex)
    {
        Scanner::TypeSchema entrySnapshot{};
        {
            std::scoped_lock lock{m_typeEntriesMutex};
            if (dropdownIndex < 0 || static_cast<std::size_t>(dropdownIndex) >= m_typeEntries.size())
            {
                return;
            }
            entrySnapshot = m_typeEntries[static_cast<std::size_t>(dropdownIndex)];
        }

        if (index < 0 || index >= static_cast<int>(m_savedAddresses.size()))
        {
            return;
        }

        auto& saved = m_savedAddresses[index];
        saved.typeId = entrySnapshot.id;
        saved.valueType = entrySnapshot.name;

        if (entrySnapshot.kind == Scanner::TypeKind::PluginDefined)
        {
            saved.valueTypeIndex = -1;
            auto schema = std::make_shared<const Scanner::TypeSchema>(entrySnapshot);
            saved.monitoredAddress = m_addressMonitor.get_or_create_plugin(saved.address, schema);
        }
        else
        {
            const auto raw = static_cast<std::uint32_t>(entrySnapshot.id);
            const auto valueType = static_cast<Scanner::ValueType>(raw - 1);
            saved.valueTypeIndex = static_cast<int>(valueType);

            const auto endianness = static_cast<Scanner::Endianness>(m_endiannessTypeIndex);
            saved.monitoredAddress = m_addressMonitor.get_or_create(saved.address, valueType, endianness);
        }

        refresh_saved_address(index);
        notify_property_changed();
    }

    void MainViewModel::refresh_saved_address(const int index)
    {
        if (index < 0 || index >= static_cast<int>(m_savedAddresses.size()))
        {
            return;
        }
        auto& saved = m_savedAddresses[index];

        if (saved.monitoredAddress)
        {
            std::vector<Scanner::MonitoredAddressPtr> toRefresh = {saved.monitoredAddress};
            m_addressMonitor.refresh(toRefresh, m_isHexadecimal);
            saved.value = saved.monitoredAddress->formattedValue;
            return;
        }

        const bool isPlugin = saved.valueTypeIndex < 0 ||
                              saved.valueTypeIndex >= static_cast<int>(Scanner::ValueType::COUNT);

        if (isPlugin)
        {
            const auto schema = m_model->find_scanner_type(saved.typeId);
            if (!schema || schema->kind != Scanner::TypeKind::PluginDefined || schema->valueSize == 0)
            {
                saved.value = "";
                return;
            }
            std::vector<char> buffer;
            const StatusCode status = m_model->read_process_memory(saved.address, schema->valueSize, buffer);
            if (status == StatusCode::STATUS_OK && !buffer.empty())
            {
                saved.value = Scanner::format_plugin_bytes(*schema, buffer.data(), buffer.size());
            }
            else
            {
                saved.value = "???";
            }
            return;
        }

        const auto valueType = static_cast<Scanner::ValueType>(saved.valueTypeIndex);
        const std::size_t valueSize = Scanner::get_value_type_size(valueType);

        std::vector<char> buffer;
        const StatusCode status = m_model->read_process_memory(saved.address, valueSize, buffer);

        if (status == StatusCode::STATUS_OK && !buffer.empty())
        {
            saved.value = Scanner::ValueConverter::format(valueType, reinterpret_cast<const std::uint8_t*>(buffer.data()), buffer.size(), m_isHexadecimal);
        }
        else
        {
            saved.value = "???";
        }
    }

    void MainViewModel::refresh_all_saved_addresses() { refresh_saved_addresses_range(0, static_cast<int>(m_savedAddresses.size()) - 1); }

    void MainViewModel::refresh_saved_addresses_range(const int startIndex, const int endIndex)
    {
        if (startIndex < 0 || endIndex < startIndex)
        {
            return;
        }

        process_frozen_addresses();

        int actualEnd{};
        std::vector<Scanner::MonitoredAddressPtr> monitoredAddresses;
        struct SavedReadRequest final
        {
            int index{};
            std::uint64_t address{};
            int valueTypeIndex{};
            Scanner::TypeId typeId{Scanner::TypeId::Invalid};
            std::size_t valueSize{};
            std::vector<char> buffer{};
            bool readOk{};
            bool isPlugin{};
        };
        std::vector<SavedReadRequest> nonMonitoredReads{};

        {
            std::scoped_lock lock(m_savedAddressesMutex);

            actualEnd = std::min(endIndex, static_cast<int>(m_savedAddresses.size()) - 1);
            if (actualEnd < startIndex)
            {
                return;
            }

            monitoredAddresses.reserve(actualEnd - startIndex + 1);
            nonMonitoredReads.reserve(actualEnd - startIndex + 1);

            for (const int i : std::views::iota(startIndex, actualEnd + 1))
            {
                auto& saved = m_savedAddresses[i];
                if (saved.monitoredAddress)
                {
                    monitoredAddresses.push_back(saved.monitoredAddress);
                    continue;
                }

                const bool isPlugin = saved.valueTypeIndex < 0 ||
                                       saved.valueTypeIndex >= static_cast<int>(Scanner::ValueType::COUNT);

                std::size_t valueSize{};
                if (isPlugin)
                {
                    const auto schema = m_model->find_scanner_type(saved.typeId);
                    if (!schema || schema->kind != Scanner::TypeKind::PluginDefined || schema->valueSize == 0)
                    {
                        continue;
                    }
                    valueSize = schema->valueSize;
                }
                else
                {
                    const auto valueType = static_cast<Scanner::ValueType>(saved.valueTypeIndex);
                    valueSize = Scanner::get_value_type_size(valueType);
                }

                auto& readRequest = nonMonitoredReads.emplace_back();
                readRequest.index = i;
                readRequest.address = saved.address;
                readRequest.valueTypeIndex = saved.valueTypeIndex;
                readRequest.typeId = saved.typeId;
                readRequest.valueSize = valueSize;
                readRequest.buffer.resize(valueSize);
                readRequest.isPlugin = isPlugin;
            }
        }

        if (!monitoredAddresses.empty())
        {
            m_addressMonitor.refresh(monitoredAddresses, m_isHexadecimal);
        }

        if (!nonMonitoredReads.empty())
        {
            bool usedBulkRead = false;

            if (m_model->supports_bulk_read())
            {
                std::vector<Model::BulkReadEntry> bulkEntries(nonMonitoredReads.size());
                std::vector<BulkReadResult> bulkResults(nonMonitoredReads.size());

                for (std::size_t i{}; i < nonMonitoredReads.size(); ++i)
                {
                    bulkEntries[i] = {nonMonitoredReads[i].address, nonMonitoredReads[i].valueSize, nonMonitoredReads[i].buffer.data()};
                }

                const StatusCode bulkStatus = m_model->read_process_memory_bulk(bulkEntries, bulkResults);
                if (bulkStatus == StatusCode::STATUS_OK)
                {
                    usedBulkRead = true;
                    for (std::size_t i{}; i < nonMonitoredReads.size(); ++i)
                    {
                        nonMonitoredReads[i].readOk = bulkResults[i].status == StatusCode::STATUS_OK && !nonMonitoredReads[i].buffer.empty();
                    }
                }
            }

            if (!usedBulkRead)
            {
                for (auto& readRequest : nonMonitoredReads)
                {
                    const StatusCode status = m_model->read_process_memory(readRequest.address, readRequest.valueSize, readRequest.buffer);
                    readRequest.readOk = status == StatusCode::STATUS_OK && !readRequest.buffer.empty();
                }
            }
        }

        std::unordered_map<int, std::size_t> readIndexBySavedIndex{};
        readIndexBySavedIndex.reserve(nonMonitoredReads.size());
        for (std::size_t i{}; i < nonMonitoredReads.size(); ++i)
        {
            readIndexBySavedIndex[nonMonitoredReads[i].index] = i;
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
                    continue;
                }

                const auto readIt = readIndexBySavedIndex.find(i);
                if (readIt == readIndexBySavedIndex.end())
                {
                    continue;
                }

                const auto& readRequest = nonMonitoredReads[readIt->second];
                if (saved.address != readRequest.address || saved.valueTypeIndex != readRequest.valueTypeIndex)
                {
                    continue;
                }

                if (!readRequest.readOk)
                {
                    saved.value = "???";
                    continue;
                }

                if (readRequest.isPlugin)
                {
                    const auto schema = m_model->find_scanner_type(readRequest.typeId);
                    if (schema && schema->kind == Scanner::TypeKind::PluginDefined)
                    {
                        saved.value = Scanner::format_plugin_bytes(*schema, readRequest.buffer.data(), readRequest.buffer.size());
                    }
                    else
                    {
                        saved.value = "";
                    }
                }
                else
                {
                    const auto valueType = static_cast<Scanner::ValueType>(saved.valueTypeIndex);
                    saved.value = Scanner::ValueConverter::format(valueType, reinterpret_cast<const std::uint8_t*>(readRequest.buffer.data()), readRequest.buffer.size(), m_isHexadecimal);
                }
            }
        }
    }

    void MainViewModel::update_available_scan_modes()
    {
        using Mode = Scanner::NumericScanMode;
        if (m_isUnknownScanMode)
        {
            m_availableNumericModes = {Mode::Exact,     Mode::GreaterThan, Mode::LessThan,  Mode::Between,     Mode::Changed,
                                       Mode::Unchanged, Mode::Increased,   Mode::Decreased, Mode::IncreasedBy, Mode::DecreasedBy};
        }
        else
        {
            m_availableNumericModes = {Mode::Exact, Mode::GreaterThan, Mode::LessThan, Mode::Between, Mode::Unknown};
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
        m_scanInitializationFailed = false;
        m_scannedValues.clear();
        m_visibleCache.clear();
        m_cacheWindow = CacheWindow{};
        update_available_scan_modes();
        m_model->set_ui_state_int("uiState.mainView.scanTypeIndex", 0);
        notify_view_update(ViewUpdateFlags::SCAN_MODES | ViewUpdateFlags::BUTTON_STATES | ViewUpdateFlags::SCANNED_VALUES);
    }

    void MainViewModel::process_frozen_addresses() const
    {
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

        if (m_model->supports_bulk_write())
        {
            if (m_dispatcher.is_channel_busy(Thread::ThreadChannel::Freeze))
            {
                return;
            }

            std::packaged_task<StatusCode()> task(
              [entries = std::move(entriesToWrite), this]() -> StatusCode
              {
                  std::vector<Model::BulkWriteEntry> bulkEntries{};
                  bulkEntries.reserve(entries.size());
                  for (const auto& entry : entries)
                  {
                      bulkEntries.push_back(Model::BulkWriteEntry{.address = entry.address, .bytes = std::span<const std::uint8_t>(entry.bytes)});
                  }

                  return m_model->write_process_memory_bulk(bulkEntries);
              });

            std::ignore = m_dispatcher.dispatch_fire_and_forget(Thread::ThreadChannel::Freeze, std::move(task));
            return;
        }

        for (const auto& [address, bytes] : entriesToWrite)
        {
            std::ignore = m_model->write_process_memory(address, bytes);
        }
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

    void MainViewModel::freeze_timer_loop() const
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
                                               [](const SavedAddress& addr)
                                               {
                                                   return addr.frozen && !addr.frozenBytes.empty();
                                               });
        }

        m_hasFrozenAddresses.store(hasAnyFrozen, std::memory_order_release);

        if (hasAnyFrozen)
        {
            start_freeze_timer();
        }
    }

} // namespace Vertex::ViewModel
