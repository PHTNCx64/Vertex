//
// Copyright (C) 2026 PHTNC<>.
// Licensed under GPLv3.0 with Plugin Interface exceptions.
//
#pragma once

#include <memory>
#include <string>
#include <string_view>
#include <vector>
#include <functional>
#include <thread>
#include <atomic>
#include <mutex>
#include <optional>

#include <vertex/thread/ithreaddispatcher.hh>
#include <vertex/event/eventbus.hh>
#include <vertex/event/types/processopenevent.hh>
#include <vertex/language/language.hh>
#include <vertex/runtime/subscription_guard.hh>
#include <vertex/scanner/iscannerruntimeservice.hh>
#include <vertex/scanner/memoryscanner/imemoryscanner.hh>
#include <vertex/scanner/scanner_typeschema.hh>
#include <vertex/scanner/valuetypes.hh>
#include <vertex/gui/iconmanager/iconmanager.hh>
#include <vertex/utility.hh>
#include <vertex/model/mainmodel.hh>
#include <vertex/theme.hh>
#include <vertex/scanner/addressmonitor.hh>

namespace Vertex::ViewModel
{
    struct ScanProgress final
    {
        std::int64_t current {};
        std::int64_t total {};
        std::string statusMessage {};
    };

    struct ScannedValue final
    {
        std::string address {};
        std::string value {};
        std::string firstValue {};
        std::string previousValue {};
    };

    struct SavedAddress final
    {
        bool frozen {};
        std::uint64_t address {};
        std::string addressStr {};
        std::string valueType {};
        std::string value {};
        int valueTypeIndex {};
        Scanner::TypeId typeId {Scanner::TypeId::Invalid};
        std::vector<std::uint8_t> frozenBytes {};

        Scanner::MonitoredAddressPtr monitoredAddress {};

        [[nodiscard]] Scanner::TypeId effective_type_id() const noexcept
        {
            if (typeId != Scanner::TypeId::Invalid)
            {
                return typeId;
            }
            return Scanner::builtin_type_id(static_cast<Scanner::ValueType>(valueTypeIndex));
        }
    };

    class MainViewModel final
    {
    public:
        explicit MainViewModel(
            std::unique_ptr<Model::MainModel> model,
            Event::EventBus& eventBus,
            Thread::IThreadDispatcher& dispatcher,
            Scanner::IScannerRuntimeService& scannerService,
            std::string name = ViewModelName::MAIN
        );

        ~MainViewModel();

        void set_event_callback(std::move_only_function<void(Event::EventId, const Event::VertexEvent&) const> callback);

        void initial_scan();
        void next_scan();
        void undo_scan() const;
        void update_scan_progress();
        void finalize_scan_results();
        void open_project() const;
        void exit_application() const;
        void open_memory_view() const;
        void open_memory_region_settings() const;
        void open_process_list_window() const;
        void open_settings_window() const;
        void close_process_state();
        void open_activity_window() const;
        void open_debugger_window() const;
        void open_injector_window() const;
        void open_scripting_window() const;
        void get_file_executable_extensions(std::vector<std::string>& extensions) const;
        [[nodiscard]] StatusCode open_new_process(std::string_view processPath, int argc, const char** argv) const;

        void set_process_information(std::string_view informationText);
        [[nodiscard]] std::string get_process_information() const;
        [[nodiscard]] ScanProgress get_scan_progress() const;
        [[nodiscard]] std::vector<ScannedValue> get_scanned_values() const;
        [[nodiscard]] std::int64_t get_scanned_values_count() const;
        [[nodiscard]] ScannedValue get_scanned_value_at(int index);
        [[nodiscard]] std::optional<std::uint64_t> get_scanned_result_address_at(int index) const;
        [[nodiscard]] std::uint32_t get_scanned_value_size() const;
        void refresh_visible_range(int startIndex, int endIndex);
        void update_cache_window(int visibleStart, int visibleEnd);
        [[nodiscard]] bool is_scan_complete() const;
        [[nodiscard]] bool has_scan_initialization_error() const;

        [[nodiscard]] std::vector<std::string> get_value_type_names() const;
        [[nodiscard]] std::vector<std::string> get_scan_mode_names() const;
        [[nodiscard]] Scanner::ValueType get_current_value_type() const;
        [[nodiscard]] Scanner::TypeId get_current_type_id() const;
        [[nodiscard]] const DataTypeUIHints* get_current_type_ui_hints() const;
        [[nodiscard]] bool current_type_supports_endianness() const;
        [[nodiscard]] ::NumericSystem get_plugin_numeric_base() const;
        void set_plugin_numeric_base(::NumericSystem base);

        [[nodiscard]] std::string get_value_input() const;
        void set_value_input(std::string_view value);

        [[nodiscard]] std::string get_value_input2() const;
        void set_value_input2(std::string_view value);

        [[nodiscard]] bool is_hexadecimal() const;
        void set_hexadecimal(bool value);

        [[nodiscard]] int get_value_type_index() const;
        void set_value_type_index(int index);

        [[nodiscard]] int get_scan_type_index() const;
        void set_scan_type_index(int index);

        [[nodiscard]] bool is_alignment_enabled() const;
        void set_alignment_enabled(bool value);

        [[nodiscard]] int get_alignment_value() const;
        void set_alignment_value(int value);

        [[nodiscard]] bool is_initial_scan_ready() const;
        [[nodiscard]] bool is_next_scan_ready() const;
        [[nodiscard]] bool is_undo_scan_ready() const;
        [[nodiscard]] bool is_value_input2_visible() const;
        [[nodiscard]] bool needs_input_value() const;

        [[nodiscard]] bool is_unknown_scan_mode() const;
        void reset_scan();
        [[nodiscard]] Scanner::NumericScanMode get_actual_numeric_scan_mode() const;

        [[nodiscard]] Theme get_theme() const;

        [[nodiscard]] std::uint64_t get_min_process_address() const;
        [[nodiscard]] std::uint64_t get_max_process_address() const;

        [[nodiscard]] bool is_process_opened() const;
        [[nodiscard]] std::optional<std::reference_wrapper<Log::ILog>> get_log_service() const;
        void kill_process() const;

        [[nodiscard]] int get_endianness_type_index() const;
        void set_endianness_type_index(int index);

        [[nodiscard]] int get_saved_addresses_count() const;
        [[nodiscard]] SavedAddress get_saved_address_at(int index) const;
        [[nodiscard]] std::uint32_t get_saved_address_watch_size(int index) const;
        [[nodiscard]] bool has_saved_address(std::uint64_t address) const;
        void add_saved_address(std::uint64_t address);
        void add_saved_address(std::uint64_t address, int valueTypeIndex);
        void remove_saved_address(int index);
        void set_saved_address_frozen(int index, bool frozen);
        void set_saved_address_value(int index, std::string_view value);
        void set_saved_address_address(int index, std::uint64_t newAddress);
        void set_saved_address_type(int index, int typeIndex);
        void refresh_saved_address(int index);
        void refresh_all_saved_addresses();
        void refresh_saved_addresses_range(int startIndex, int endIndex);
        void process_frozen_addresses() const;

    private:
        void load_ui_state_from_settings();
        void notify_property_changed() const;
        void on_process_opened(const Event::ProcessOpenEvent& event);
        void subscribe_to_events();
        void unsubscribe_from_events() const;
        void update_available_scan_modes();
        void notify_view_update(ViewUpdateFlags flags) const;
        void start_freeze_timer();
        void stop_freeze_timer();
        void freeze_timer_loop() const;
        void update_frozen_addresses_flag();

        [[nodiscard]] std::uint8_t get_actual_scan_mode_value() const;
        [[nodiscard]] Scanner::ValueType get_scanned_value_type() const;
        void reload_type_entries();
        [[nodiscard]] const Scanner::TypeSchema* current_type_entry() const noexcept;
        [[nodiscard]] bool current_type_is_plugin() const noexcept;

        bool m_isInitialScanAvailable {};
        bool m_isNextScanAvailable {};
        bool m_isHexadecimal {};
        bool m_isUnknownScanMode {};
        bool m_scanInitializationFailed {};
        bool m_alignmentEnabled {true};

        int m_valueTypeIndex {2};
        int m_scanTypeIndex {};
        int m_scannedValueTypeIndex {2};
        int m_endiannessTypeIndex {};
        int m_scannedEndiannessIndex {};
        int m_alignmentValue {4};
        Scanner::TypeId m_scannedTypeId {Scanner::TypeId::Invalid};
        bool m_scannedTypeIsPlugin {false};
        std::optional<Scanner::TypeSchema> m_scannedPluginSchema {};
        ::NumericSystem m_pluginNumericBase {VERTEX_DECIMAL};

        std::uint64_t m_minProcessAddress {};
        std::uint64_t m_maxProcessAddress {};

        std::string m_processInformation {};
        std::string m_valueInput {};
        std::string m_valueInput2 {};
        std::string m_viewModelName {};

        ScanProgress m_scanProgress {};
        std::vector<ScannedValue> m_scannedValues {};
        std::vector<Scanner::NumericScanMode> m_availableNumericModes {};
        std::vector<Scanner::TypeSchema> m_typeEntries {};
        mutable std::mutex m_typeEntriesMutex {};
        std::vector<SavedAddress> m_savedAddresses {};
        std::unordered_map<int, ScannedValue> m_visibleCache {};

        struct CacheWindow final
        {
            int startIndex {-1};
            int endIndex {-1};
            std::vector<Scanner::IMemoryScanner::ScanResultEntry> addresses {};
        } m_cacheWindow {};

        std::unique_ptr<Model::MainModel> m_model {};
        std::unique_ptr<std::thread> m_freezeTimerThread {};
        std::future<StatusCode> m_nextScanInitFuture {};
        std::move_only_function<void(Event::EventId, const Event::VertexEvent&) const> m_eventCallback {};

        std::atomic<bool> m_freezeTimerRunning {};
        std::atomic<bool> m_hasFrozenAddresses {};

        Event::EventBus& m_eventBus;
        Thread::IThreadDispatcher& m_dispatcher;
        Scanner::IScannerRuntimeService& m_scannerService;

        Scanner::AddressMonitor m_addressMonitor {};

        std::shared_ptr<std::atomic<bool>> m_alive{std::make_shared<std::atomic<bool>>(true)};
        Runtime::SubscriptionGuard<Scanner::IScannerRuntimeService> m_scanCompleteSub{};
        Runtime::SubscriptionGuard<Scanner::IScannerRuntimeService> m_scanProgressSub{};
        Runtime::SubscriptionGuard<Scanner::IScannerRuntimeService> m_valuesChangedSub{};
        Runtime::SubscriptionGuard<Scanner::IScannerRuntimeService> m_registrySub{};

        mutable std::mutex m_savedAddressesMutex {};
    };
}
