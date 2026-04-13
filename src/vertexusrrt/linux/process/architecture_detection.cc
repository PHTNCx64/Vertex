//
// Copyright (C) 2026 PHTNC<>.
// Licensed under LGPLv3.0+
//
#include <vertexusrrt/native_handle.hh>

#include <elf.h>

#include <array>
#include <atomic>
#include <format>
#include <fstream>

extern native_handle& get_native_handle();

namespace
{
    std::atomic<ProcessArchitecture> g_cachedArchitecture{ProcessArchitecture::UNKNOWN};

    [[nodiscard]] ProcessArchitecture detect_process_architecture()
    {
        const native_handle pid = get_native_handle();
        if (pid == INVALID_HANDLE_VALUE)
        {
            return ProcessArchitecture::UNKNOWN;
        }

        const auto exePath = std::format("/proc/{}/exe", pid);
        std::ifstream file{exePath, std::ios::binary};
        if (!file)
        {
            return ProcessArchitecture::UNKNOWN;
        }

        std::array<std::uint8_t, EI_NIDENT> ident{};
        file.read(reinterpret_cast<char*>(ident.data()), EI_NIDENT);
        if (!file ||
            ident[EI_MAG0] != ELFMAG0 || ident[EI_MAG1] != ELFMAG1 ||
            ident[EI_MAG2] != ELFMAG2 || ident[EI_MAG3] != ELFMAG3)
        {
            return ProcessArchitecture::UNKNOWN;
        }

        const std::uint8_t elfClass = ident[EI_CLASS];

        std::uint16_t machine{};
        if (elfClass == ELFCLASS64)
        {
            Elf64_Ehdr ehdr{};
            file.seekg(0);
            file.read(reinterpret_cast<char*>(&ehdr), sizeof(ehdr));
            if (!file)
            {
                return ProcessArchitecture::UNKNOWN;
            }
            machine = ehdr.e_machine;
        }
        else if (elfClass == ELFCLASS32)
        {
            Elf32_Ehdr ehdr{};
            file.seekg(0);
            file.read(reinterpret_cast<char*>(&ehdr), sizeof(ehdr));
            if (!file)
            {
                return ProcessArchitecture::UNKNOWN;
            }
            machine = ehdr.e_machine;
        }
        else
        {
            return ProcessArchitecture::UNKNOWN;
        }

        // Although linux supports QUITE a lot of architectures, it's very unlikely that someone is running Vertex on anything but these.
        // Arm might be relevant due to Asahi Linux, so we'll quietly add it too here.
        switch (machine)
        {
        case EM_386:
            return ProcessArchitecture::X86;
        case EM_X86_64:
            return ProcessArchitecture::X86_64;
        case EM_AARCH64:
            return ProcessArchitecture::ARM64;
        default:
            return ProcessArchitecture::UNKNOWN;
        }
    }
}

void cache_process_architecture()
{
    const ProcessArchitecture arch = detect_process_architecture();
    g_cachedArchitecture.store(arch, std::memory_order_release);
}

void clear_process_architecture()
{
    g_cachedArchitecture.store(ProcessArchitecture::UNKNOWN, std::memory_order_release);
}

ProcessArchitecture get_process_architecture()
{
    const ProcessArchitecture cached = g_cachedArchitecture.load(std::memory_order_acquire);
    if (cached != ProcessArchitecture::UNKNOWN)
    {
        return cached;
    }
    return detect_process_architecture();
}
