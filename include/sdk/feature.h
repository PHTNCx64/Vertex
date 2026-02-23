//
// Copyright (C) 2026 PHTNC<>.
// Licensed under Apache 2.0
//

#pragma once

#define VERTEX_FEATURE_RUN_MODE_STANDARD (0ULL << 0) // Default mode, no specific run mode requirements.

// Bit 0: Run Mode
// 1 = Single-threaded: Vertex runs the plugin exclusively in a single-threaded environment.
//     Useful for plugins that can't be safely used in a multithreaded context or for external libraries that aren't thread safe (e.g. libraries relying on TLS).
//     Reduces implementation complexity but increases overhead due to thread synchronization.
// 0 = Multi-threaded: Vertex runs the plugin in a multithreaded environment, allowing concurrent execution across multiple threads.
//     Better performance potential but plugin developers must ensure thread safety.

// NOTE (Single-threaded): Vertex will call vertex_init regularly from the main thread with the singleThreadCall param false,
// if the plugin specifies single thread mode in featureCapability then Vertex will call vertex_init again but from the reserved single thread,
// with the singleThreadCall param set to true, allowing the plugin to initialize separate resources for the single-threaded environment if needed.

// NOTE (Multi-threaded): vertex_init will only be called once from the main thread with the singleThreadCall param set to false,
// and all plugin code may be executed concurrently across multiple threads,
// so plugin developers must ensure thread safety for their entire plugin.
#define VERTEX_FEATURE_RUN_MODE_SINGLE_THREADED (1ULL << 0)

// Bit 1: Debugger Mode
// 1 = Debugger is tied to the run mode specified by bit 0 (single-threaded mode).
// 0 = Debugger can run independently in a separate thread regardless of the run mode in bit 0.
#define VERTEX_FEATURE_DEBUGGER_DEPENDENT (1ULL << 1)