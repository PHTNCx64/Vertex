//
// Copyright (C) 2026 PHTNC<>.
// Licensed under LGPLv3.0+
//

#pragma once

#if defined(_WIN32) || defined(_WIN64)
#include <vertexusrrt/windows/debugloopcontext_windows.hh>
#elif defined(__linux__) || defined(__linux) || defined(linux)
#include <vertexusrrt/linux/debugloopcontext_linux.hh>
#else
#error "Unsupported platform: debugloopcontext.hh requires Windows or Linux"
#endif
