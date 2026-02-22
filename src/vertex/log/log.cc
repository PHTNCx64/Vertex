//
// Copyright (C) 2026 PHTNC<>.
// Licensed under GPLv3.0 with Plugin Interface exceptions.
//
#include <vertex/log/log.hh>

#include <algorithm>
#include <chrono>
#include <cstdarg>
#include <ranges>
#include <vector>

#include <fmt/format.h>

#include <sdk/statuscode.h>
#include <sdk/macro.h>

namespace Vertex::Log
{
    namespace
    {
        StatusCode log_raw_impl(const char* level, const char* msg, va_list args, std::string& logBuffer)
        {
            va_list argsCopy;
            va_copy(argsCopy, args);
            const int size = vsnprintf(nullptr, 0, msg, argsCopy);
            va_end(argsCopy);

            if (size < 0)
            {
                return StatusCode::STATUS_ERROR_FMT_INVALID_CONVERSION;
            }

            std::vector<char> buffer(size + 1);
            vsnprintf(buffer.data(), buffer.size(), msg, args);

            const auto now = std::chrono::system_clock::now();
            std::array<char, 24> timestampBuf{};
            TimestampFormatter::format_into(now, timestampBuf.data(), timestampBuf.size());

            logBuffer = fmt::format("{}\n[{}] [{}] {}", logBuffer, timestampBuf.data(), level, buffer.data());
            return StatusCode::STATUS_OK;
        }

        struct TimestampCache final
        {
            std::chrono::seconds lastSecond{};
            std::array<char, 20> baseFormat{};
        };

        thread_local TimestampCache g_timestampCache;
        void write_3_digits(char* buf, const int value)
        {
            buf[0] = static_cast<char>('0' + (value / 100));
            buf[1] = static_cast<char>('0' + ((value / 10) % 10));
            buf[2] = static_cast<char>('0' + (value % 10));
        }

        std::atomic<bool> g_pluginLogStatus{true};
    }

    Log::Log() = default;
    Log::~Log() = default;

    void TimestampFormatter::format_into(const std::chrono::system_clock::time_point& timestamp,
                                         char* buffer, const std::size_t bufferSize)
    {
        if (bufferSize < 24)
        {
            return;
        }

        const auto seconds = std::chrono::duration_cast<std::chrono::seconds>(timestamp.time_since_epoch());

        if (g_timestampCache.lastSecond != seconds)
        {
            const auto localTime = std::chrono::zoned_time{std::chrono::current_zone(), timestamp};
            const auto dp = std::chrono::floor<std::chrono::days>(localTime.get_local_time());
            const auto ymd = std::chrono::year_month_day{dp};
            const auto tod = std::chrono::hh_mm_ss{localTime.get_local_time() - dp};

            fmt::format_to_n(g_timestampCache.baseFormat.data(), g_timestampCache.baseFormat.size() - 1,
                            "{:04d}-{:02d}-{:02d} {:02d}:{:02d}:{:02d}",
                            static_cast<int>(ymd.year()),
                            static_cast<unsigned>(ymd.month()),
                            static_cast<unsigned>(ymd.day()),
                            static_cast<int>(tod.hours().count()),
                            static_cast<int>(tod.minutes().count()),
                            static_cast<int>(tod.seconds().count()));

            g_timestampCache.lastSecond = seconds;
        }

        std::ranges::copy_n(g_timestampCache.baseFormat.data(), 19, buffer);

        const auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(
            timestamp.time_since_epoch()) % 1000;

        buffer[19] = '.';
        write_3_digits(buffer + 20, static_cast<int>(ms.count()));
        buffer[23] = '\0';
    }

    std::string TimestampFormatter::format(const std::chrono::system_clock::time_point& timestamp)
    {
        std::array<char, 24> buffer{};
        format_into(timestamp, buffer.data(), buffer.size());
        return {buffer.data()};
    }

    StatusCode Log::set_logging_status(const bool status)
    {
        m_loggingEnabled.store(status, std::memory_order_relaxed);
        g_pluginLogStatus.store(status, std::memory_order_relaxed);
        return StatusCode::STATUS_OK;
    }

    StatusCode Log::set_logging_interval(const int minutes)
    {
        m_loggingInterval.store(minutes, std::memory_order_relaxed);
        return StatusCode::STATUS_OK;
    }

    void Log::enqueue_log(const LogLevel level, const std::string_view msg)
    {
        if (!m_loggingEnabled.load(std::memory_order_relaxed))
        {
            return;
        }

        LogEntry entry(level, std::string{msg});

        if (m_logQueue.enqueue(std::move(entry)))
        {
            m_approximateQueueSize.fetch_add(1, std::memory_order_relaxed);
        }
    }

    StatusCode Log::log_error(const std::string_view msg)
    {
        enqueue_log(LogLevel::ERROR_LOG, msg);
        return StatusCode::STATUS_OK;
    }

    StatusCode Log::log_warn(const std::string_view msg)
    {
        enqueue_log(LogLevel::WARN_LOG, msg);
        return StatusCode::STATUS_OK;
    }

    StatusCode Log::log_info(const std::string_view msg)
    {
        enqueue_log(LogLevel::INFO_LOG, msg);
        return StatusCode::STATUS_OK;
    }

    StatusCode Log::log_clear()
    {
        LogEntry temp{};
        while (m_logQueue.try_dequeue(temp))
        {
        }
        m_approximateQueueSize.store(0, std::memory_order_relaxed);

        std::scoped_lock lock(m_historyMutex);
        m_logHistory.clear();

        return StatusCode::STATUS_OK;
    }

    void Log::drain_queue_to_history()
    {
        std::vector<LogEntry> newEntries{};
        newEntries.reserve(1000);

        const std::size_t dequeued = m_logQueue.try_dequeue_bulk(std::back_inserter(newEntries), 1000);

        if (dequeued > 0)
        {
            std::ranges::sort(newEntries, {}, &LogEntry::timestamp);

            m_approximateQueueSize.fetch_sub(dequeued, std::memory_order_relaxed);

            std::scoped_lock lock(m_historyMutex);
            m_logHistory.insert(m_logHistory.end(),
                std::make_move_iterator(newEntries.begin()),
                std::make_move_iterator(newEntries.end()));

            if (m_logHistory.size() > MAX_HISTORY_SIZE)
            {
                const auto excess = static_cast<std::ptrdiff_t>(m_logHistory.size() - MAX_HISTORY_SIZE);
                m_logHistory.erase(m_logHistory.begin(), m_logHistory.begin() + excess);
            }
        }
    }

    std::size_t Log::collect_logs_bulk(std::vector<LogEntry>& outEntries, const std::size_t maxEntries)
    {
        drain_queue_to_history();

        std::scoped_lock lock(m_historyMutex);
        outEntries.clear();

        const auto count = std::min(maxEntries, m_logHistory.size());
        outEntries.reserve(count);

        auto tail = m_logHistory | std::views::drop(m_logHistory.size() - count);
        std::ranges::copy(tail, std::back_inserter(outEntries));

        return outEntries.size();
    }

    std::vector<LogEntry> Log::get_all_logs() const
    {
        const_cast<Log*>(this)->drain_queue_to_history();

        std::scoped_lock lock(m_historyMutex);
        return m_logHistory;
    }

    std::size_t Log::get_approximate_queue_size() const
    {
        return m_approximateQueueSize.load(std::memory_order_relaxed);
    }

    std::string& plugin_log_buffer()
    {
        static std::string logBuffer{};
        return logBuffer;
    }

    StatusCode VERTEX_API log_error_raw(const char* msg, ...)
    {
        if (g_pluginLogStatus.load(std::memory_order_relaxed))
        {
            va_list args;
            va_start(args, msg);
            const StatusCode result = log_raw_impl("ERROR", msg, args, plugin_log_buffer());
            va_end(args);
            return result;
        }
        return StatusCode::STATUS_ERROR_FEATURE_DEACTIVATED;
    }

    StatusCode VERTEX_API log_warn_raw(const char* msg, ...)
    {
        if (g_pluginLogStatus.load(std::memory_order_relaxed))
        {
            va_list args;
            va_start(args, msg);
            const StatusCode result = log_raw_impl("WARN", msg, args, plugin_log_buffer());
            va_end(args);
            return result;
        }
        return StatusCode::STATUS_ERROR_FEATURE_DEACTIVATED;
    }

    StatusCode VERTEX_API log_info_raw(const char* msg, ...)
    {
        if (g_pluginLogStatus.load(std::memory_order_relaxed))
        {
            va_list args;
            va_start(args, msg);
            const StatusCode result = log_raw_impl("INFO", msg, args, plugin_log_buffer());
            va_end(args);
            return result;
        }
        return StatusCode::STATUS_ERROR_FEATURE_DEACTIVATED;
    }

    StatusCode Log::flush_to_disk()
    {
        return StatusCode::STATUS_OK;
    }
}
