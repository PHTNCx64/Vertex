//
// Copyright (C) 2026 PHTNC<>.
// Licensed under GPLv3.0 with Plugin Interface exceptions.
//
#pragma once

#include <vector>
#include <string_view>
#include <vertex/log/log.hh>

namespace Vertex::Model
{
    class AnalyticsModel
    {
      public:
        explicit AnalyticsModel(Log::ILog& logService);
        [[nodiscard]] std::vector<Log::LogEntry> get_logs(std::size_t max_entries = 1000) const;
        void clear_logs() const;
        [[nodiscard]] bool save_logs_to_file(std::string_view filePath, const std::vector<Log::LogEntry>& entries) const;

      private:
        Log::ILog& m_logService;
    };
}
