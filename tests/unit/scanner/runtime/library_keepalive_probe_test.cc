//
// Copyright (C) 2026 PHTNC<>.
// Licensed under GPLv3.0 with Plugin Interface exceptions.
//
#include <gtest/gtest.h>

#include <vertex/runtime/function_registry.hh>

#include <cstdlib>
#include <filesystem>
#include <memory>
#include <string>

#if defined(__linux__) || defined(__APPLE__)
  #include <dlfcn.h>
#endif

namespace
{
#if defined(__linux__) || defined(__APPLE__)
    [[nodiscard]] bool probe_mapped(const std::string& path)
    {
        void* h = ::dlopen(path.c_str(), RTLD_NOW | RTLD_NOLOAD);
        if (h == nullptr) return false;
        (void) ::dlclose(h);
        return true;
    }
#else
    [[nodiscard]] bool probe_mapped(const std::string& ) { return false; }
#endif

    [[nodiscard]] std::filesystem::path test_library_path()
    {
        std::filesystem::path exe{"/proc/self/exe"};
        std::error_code ec{};
        auto resolved = std::filesystem::read_symlink(exe, ec);
        if (ec) resolved = std::filesystem::current_path();
        auto dir = resolved.parent_path();
        return dir / "Plugins" / "liblibtest_dbg.so";
    }
}

#if defined(__linux__) || defined(__APPLE__)
TEST(LibraryKeepaliveProbeTest, RTLD_NOLOAD_TracksSharedRefLifetime)
{
    const auto path = test_library_path();
    if (!std::filesystem::exists(path))
    {
        GTEST_SKIP() << "Plugin artifact not present at: " << path.string();
    }

    
    
    const bool preMapped = probe_mapped(path.string());

    std::shared_ptr<Vertex::Runtime::LibraryHandle> keepalive{};
    {
        Vertex::Runtime::Library library{path};
        keepalive = library.keepalive();
        EXPECT_TRUE(keepalive);
        EXPECT_TRUE(probe_mapped(path.string()));
    }

    
    EXPECT_TRUE(probe_mapped(path.string())) << "library unmapped while keepalive outlives Library";

    keepalive.reset();

    
    
    
    if (!preMapped && probe_mapped(path.string()))
    {
        GTEST_SKIP() << "platform did not unmap after final dlclose (glibc may retain mapping)";
    }
}
#endif
