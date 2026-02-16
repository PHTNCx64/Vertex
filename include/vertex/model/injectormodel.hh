//
// Copyright (C) 2026 PHTNC<>.
// Licensed under GPLv3.0 with Plugin Interface exceptions.
//
#pragma once

#include <vertex/runtime/iloader.hh>
#include <vertex/log/ilog.hh>

#include <string>
#include <vector>
#include <cstdint>

#include <sdk/process.h>
#include <sdk/statuscode.h>

namespace Vertex::Model
{
    class InjectorModel final
    {
    public:
        explicit InjectorModel(Runtime::ILoader& loaderService, Log::ILog& loggerService)
            : m_loaderService{loaderService},
              m_loggerService{loggerService}
        {
        }

        [[nodiscard]] StatusCode get_injection_methods(std::vector<InjectionMethod>& methods) const;
        [[nodiscard]] StatusCode get_library_extensions(std::vector<std::string>& extensions) const;
        [[nodiscard]] StatusCode inject(const InjectionMethod& method, std::string_view libraryPath) const;

    private:
        Runtime::ILoader& m_loaderService;
        Log::ILog& m_loggerService;
    };
}
