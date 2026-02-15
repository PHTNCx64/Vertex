//
// Copyright (C) 2026 PHTNC<>.
// Licensed under GPLv3.0 with Plugin Interface exceptions.
//
#pragma once

#ifdef _MSC_VER
#  define MSVC_SUPPRESS_PADDING_WARNING \
__pragma(warning(push))          \
__pragma(warning(disable : 4324))
#  define MSVC_END_WARNING_SUPPRESSION \
__pragma(warning(pop))
#else
#  define MSVC_SUPPRESS_PADDING_WARNING
#  define MSVC_END_WARNING_SUPPRESSION
#endif
