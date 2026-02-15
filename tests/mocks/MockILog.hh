#pragma once

#include <gmock/gmock.h>
#include <string_view>
#include <vertex/log/ilog.hh>

namespace Vertex::Testing::Mocks
{
    class MockILog : public Log::ILog
    {
    public:
        ~MockILog() override = default;

        MOCK_METHOD(StatusCode, log_error, (std::string_view msg), (override));
        MOCK_METHOD(StatusCode, log_warn, (std::string_view msg), (override));
        MOCK_METHOD(StatusCode, log_info, (std::string_view msg), (override));
        MOCK_METHOD(StatusCode, log_clear, (), (override));
        MOCK_METHOD(StatusCode, flush_to_disk, (), (override));
        MOCK_METHOD(StatusCode, set_logging_status, (bool status), (override));
        MOCK_METHOD(StatusCode, set_logging_interval, (int minutes), (override));
    };
}
