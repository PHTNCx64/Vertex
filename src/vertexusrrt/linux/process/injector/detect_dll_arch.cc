//
// Copyright (C) 2026 PHTNC<>.
// Licensed under LGPLv3.0+
//
#include <sdk/api.h>

#include <vertexusrrt/native_handle.hh>

#include <elf.h>

#include <array>
#include <filesystem>
#include <format>
#include <fstream>
#include <optional>

extern Runtime* g_pluginRuntime;

[[nodiscard]] std::optional<ProcessArchitecture> detect_dll_architecture(
    const std::filesystem::path& dllPath)
{
    std::ifstream file{dllPath, std::ios::binary};
    if (!file)
    {
        g_pluginRuntime->vertex_log_error("[Injection] Failed to open shared object for architecture detection");
        return std::nullopt;
    }

    std::array<std::uint8_t, EI_NIDENT> ident{};
    file.read(reinterpret_cast<char*>(ident.data()), EI_NIDENT);
    if (!file)
    {
        g_pluginRuntime->vertex_log_error("[Injection] Failed to read ELF ident");
        return std::nullopt;
    }

    if (ident[EI_MAG0] != ELFMAG0 || ident[EI_MAG1] != ELFMAG1 ||
        ident[EI_MAG2] != ELFMAG2 || ident[EI_MAG3] != ELFMAG3)
    {
        g_pluginRuntime->vertex_log_error("[Injection] Invalid ELF magic in shared object");
        return std::nullopt;
    }

    if (ident[EI_CLASS] == ELFCLASS64)
    {
        Elf64_Ehdr ehdr{};
        file.seekg(0);
        file.read(reinterpret_cast<char*>(&ehdr), sizeof(ehdr));
        if (!file)
        {
            g_pluginRuntime->vertex_log_error("[Injection] Failed to read 64-bit ELF header");
            return std::nullopt;
        }

        switch (ehdr.e_machine)
        {
        case EM_X86_64:  return ProcessArchitecture::X86_64;
        case EM_AARCH64: return ProcessArchitecture::ARM64;
        default:
            g_pluginRuntime->vertex_log_error(
                std::format("[Injection] Unrecognized 64-bit ELF machine type: {:#06x}", ehdr.e_machine).c_str());
            return std::nullopt;
        }
    }

    if (ident[EI_CLASS] == ELFCLASS32)
    {
        Elf32_Ehdr ehdr{};
        file.seekg(0);
        file.read(reinterpret_cast<char*>(&ehdr), sizeof(ehdr));
        if (!file)
        {
            g_pluginRuntime->vertex_log_error("[Injection] Failed to read 32-bit ELF header");
            return std::nullopt;
        }

        switch (ehdr.e_machine)
        {
        case EM_386: return ProcessArchitecture::X86;
        default:
            g_pluginRuntime->vertex_log_error(
                std::format("[Injection] Unrecognized 32-bit ELF machine type: {:#06x}", ehdr.e_machine).c_str());
            return std::nullopt;
        }
    }

    g_pluginRuntime->vertex_log_error(
        std::format("[Injection] Unknown ELF class: {:#04x}", ident[EI_CLASS]).c_str());
    return std::nullopt;
}
