//
// Copyright (C) 2026 PHTNC<>.
// Licensed under GPLv3.0 with Plugin Interface exceptions.
//
#pragma once

#include <sdk/statuscode.h>
#include <string_view>

namespace Vertex::Log
{
    class ILog
    {
    public:
        virtual ~ILog() = default;
        virtual StatusCode log_error(std::string_view msg) = 0;
        virtual StatusCode log_warn(std::string_view msg) = 0;
        virtual StatusCode log_info(std::string_view msg) = 0;
        virtual StatusCode log_clear() = 0;
        virtual StatusCode flush_to_disk() = 0;
        virtual StatusCode set_logging_status(bool status) = 0;
        virtual StatusCode set_logging_interval(int minutes) = 0;
    };
}
