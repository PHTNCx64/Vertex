//
// Copyright (C) 2026 PHTNC<>.
// Licensed under GPLv3.0 with Plugin Interface exceptions.
//
#pragma once
#include <cstddef>
#include <new>

#ifdef _MSC_VER
#  define MSVC_SUPPRESS_PADDING_WARNING \
__pragma(warning(push))                \
__pragma(warning(disable : 4324))
#  define MSVC_END_WARNING_SUPPRESSION \
__pragma(warning(pop))
#else
#  define MSVC_SUPPRESS_PADDING_WARNING
#  define MSVC_END_WARNING_SUPPRESSION
#endif

#if defined(__GNUC__) && !defined(__clang__)
#  define GCC_SUPPRESS_INTERFERENCE_SIZE_WARNING \
_Pragma("GCC diagnostic push")                  \
_Pragma("GCC diagnostic ignored \"-Winterference-size\"")
#  define GCC_END_INTERFERENCE_SIZE_WARNING \
_Pragma("GCC diagnostic pop")
#else
#  define GCC_SUPPRESS_INTERFERENCE_SIZE_WARNING
#  define GCC_END_INTERFERENCE_SIZE_WARNING
#endif

#define START_PADDING_WARNING_SUPPRESSION \
MSVC_SUPPRESS_PADDING_WARNING    \
GCC_SUPPRESS_INTERFERENCE_SIZE_WARNING

#define END_PADDING_WARNING_SUPPRESSION \
GCC_END_INTERFERENCE_SIZE_WARNING \
MSVC_END_WARNING_SUPPRESSION
