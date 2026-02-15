//
// Copyright (C) 2026 PHTNC<>.
// Licensed under Apache 2.0
//

#pragma once

// ===============================================================================================================//
// EXPORT MACROS                                                                                                  //
// ===============================================================================================================//

#ifndef VERTEX_API
    #if defined(_WIN32) || defined(_WIN64)
        #define VERTEX_API __cdecl
    #elif defined(__APPLE__) || defined(__MACH__)
        #define VERTEX_API __attribute__((cdecl))
    #else
        #define VERTEX_API
    #endif
#endif

#ifndef VERTEX_EXPORT
    #if defined(_WIN32) || defined(_WIN64)
        #define VERTEX_EXPORT __declspec(dllexport)
    #elif defined(__GNUC__) || defined(__clang__)
        #define VERTEX_EXPORT __attribute__((visibility("default")))
    #else
        #define VERTEX_EXPORT
    #endif
#endif