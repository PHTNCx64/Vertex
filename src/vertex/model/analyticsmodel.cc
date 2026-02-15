//
// Copyright (C) 2026 PHTNC<>.
// Licensed under GPLv3.0 with Plugin Interface exceptions.
//
#include <vertex/model/analyticsmodel.hh>
#include <fstream>
#include <fmt/format.h>
#include <ranges>
#include <algorithm>

namespace Vertex::Model
{
    namespace
    {
        [[nodiscard]] constexpr std::string_view get_log_level_string(const Log::LogLevel level)
        {
            switch (level)
            {
            case Log::LogLevel::INFO_LOG: return "INFO";
            case Log::LogLevel::WARN_LOG: return "WARN";
            case Log::LogLevel::ERROR_LOG: return "ERROR";
            }
            return "UNKNOWN";
        }
    }

    AnalyticsModel::AnalyticsModel(Log::ILog& logService)
        : m_logService(logService)
    {
    }

    std::vector<Log::LogEntry> AnalyticsModel::get_logs(const std::size_t max_entries) const
    {
        std::vector<Log::LogEntry> entries;
        auto* logImpl = dynamic_cast<Log::Log*>(&m_logService);
        if (logImpl)
        {
            std::ignore = logImpl->collect_logs_bulk(entries, max_entries);
        }
        return entries;
    }

    void AnalyticsModel::clear_logs() const { m_logService.log_clear(); }

    bool AnalyticsModel::save_logs_to_file(const std::string_view filePath, const std::vector<Log::LogEntry>& entries) const
    {
        try
        {
            std::ofstream file(std::string{filePath});
            if (!file.is_open())
            {
                m_logService.log_error("Failed to open file for writing logs");
                return false;
            }

            file.exceptions(std::ofstream::failbit | std::ofstream::badbit);

            std::ranges::for_each(entries, [&file](const auto& entry)
            {
                const auto timestamp = Log::TimestampFormatter::format(entry.timestamp);
                const auto levelStr = get_log_level_string(entry.level);
                file << "[" << timestamp << "] [" << levelStr << "] " << entry.message << "\n";
            });

            file.flush();
            return true;
        }
        catch (const std::ios_base::failure& e)
        {
            m_logService.log_error(fmt::format("File I/O error while saving logs: {}", e.what()));
            return false;
        }
        catch (const std::exception& e)
        {
            m_logService.log_error(fmt::format("Unexpected error while saving logs: {}", e.what()));
            return false;
        }
    }
} // namespace Vertex::Model
