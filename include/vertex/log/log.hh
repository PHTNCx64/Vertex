//
// Copyright (C) 2026 PHTNC<>.
// Licensed under GPLv3.0 with Plugin Interface exceptions.
//
#pragma once

#include <sdk/macro.h>
#include <vertex/log/ilog.hh>

#include <concurrentqueue/moodycamel/concurrentqueue.h>

#include <chrono>
#include <vector>

namespace Vertex::Log
{
    enum class LogLevel : std::uint8_t
    {
        INFO_LOG = 0,
        WARN_LOG = 1,
        ERROR_LOG = 2
    };

    struct LogEntry final
    {
        std::chrono::system_clock::time_point timestamp {};
        LogLevel level {};
        std::string message {};

        LogEntry() = default;

        LogEntry(const LogLevel lvl, std::string msg)
            : timestamp(std::chrono::system_clock::now())
            , level(lvl)
            , message(std::move(msg))
        {
        }
    };

    class Log final : public ILog
    {
    public:
        Log();
        ~Log() override;

        StatusCode log_error(std::string_view msg) override;
        StatusCode log_warn(std::string_view msg) override;
        StatusCode log_info(std::string_view msg) override;
        StatusCode log_clear() override;
        StatusCode flush_to_disk() override;
        StatusCode set_logging_status(bool status) override;
        StatusCode set_logging_interval(int minutes) override;

        [[nodiscard]] std::size_t collect_logs_bulk(std::vector<LogEntry>& outEntries, std::size_t maxEntries = 1000);
        [[nodiscard]] std::size_t get_approximate_queue_size() const;
        [[nodiscard]] std::vector<LogEntry> get_all_logs() const;

    private:
        void enqueue_log(LogLevel level, std::string_view msg);
        void drain_queue_to_history();

        moodycamel::ConcurrentQueue<LogEntry> m_logQueue {};

        mutable std::mutex m_historyMutex {};
        std::vector<LogEntry> m_logHistory {};
        static constexpr std::size_t MAX_HISTORY_SIZE {10000};

        std::atomic<bool> m_loggingEnabled {true};
        std::atomic<int> m_loggingInterval {60};
        std::atomic<std::size_t> m_approximateQueueSize {};
    };

    StatusCode VERTEX_API log_error_raw(const char* msg, ...);
    StatusCode VERTEX_API log_warn_raw(const char* msg, ...);
    StatusCode VERTEX_API log_info_raw(const char* msg, ...);

    class TimestampFormatter final
    {
    public:
        [[nodiscard]] static std::string format(const std::chrono::system_clock::time_point& timestamp);

        static void format_into(const std::chrono::system_clock::time_point& timestamp,
                               char* buffer, std::size_t bufferSize);
    };

    [[nodiscard]] std::string& plugin_log_buffer();
}
