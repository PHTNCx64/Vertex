//
// Copyright (C) 2026 PHTNC<>.
// Licensed under GPLv3.0 with Plugin Interface exceptions.
//
#include <gtest/gtest.h>

#include <vertex/scanner/memoryscanner/memoryscanner.hh>

#include <chrono>

TEST(ProgressCoalescingTest, MinIntervalIsPositive)
{
    EXPECT_GT(Vertex::Scanner::MemoryScanner::SCAN_PROGRESS_MIN_INTERVAL.count(), 0);
}

TEST(ProgressCoalescingTest, MinIntervalPreventsUnboundedEventRate)
{
    
    EXPECT_GE(Vertex::Scanner::MemoryScanner::SCAN_PROGRESS_MIN_INTERVAL,
              std::chrono::milliseconds{10});
    EXPECT_LE(Vertex::Scanner::MemoryScanner::SCAN_PROGRESS_MIN_INTERVAL,
              std::chrono::milliseconds{250});
}
