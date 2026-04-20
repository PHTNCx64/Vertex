//
// Copyright (C) 2026 PHTNC<>.
// Licensed under LGPLv3.0+
//

#pragma once

#include "ps3tmapi.h"

namespace DECI3
{
    struct Module final
    {
        HTARGET targetNumber {};
        std::uint32_t processId {};
    };

    struct Deci3Context final
    {
        Module module {};

        Deci3Context()                               = default;
        Deci3Context(const Deci3Context&)            = delete;
        Deci3Context& operator=(const Deci3Context&) = delete;
        Deci3Context(Deci3Context&&)                  = default;
        Deci3Context& operator=(Deci3Context&&)       = default;
        ~Deci3Context()                               = default;
    };

    [[nodiscard]] SNRESULT initialize_communications();
    [[nodiscard]] Deci3Context* context() noexcept;
    void destroy_context() noexcept;

} // namespace DECI3
