//
// Copyright (C) 2026 PHTNC<>.
// Licensed under LGPLv3.0+
//

#include <sdk/api.h>

#include <optional>
#include <fstream>
#include <filesystem>

#include <vertexusrrt/native_handle.hh>

extern Runtime* g_pluginRuntime;

[[nodiscard]] std::optional<ProcessArchitecture> detect_dll_architecture(const std::filesystem::path& dllPath)
{
    std::ifstream file(dllPath, std::ios::binary);
    if (!file)
    {
        g_pluginRuntime->vertex_log_error("[Injection] Failed to open DLL file for architecture detection");
        return std::nullopt;
    }

    IMAGE_DOS_HEADER dosHeader{};
    file.read(reinterpret_cast<char*>(&dosHeader), sizeof(dosHeader));
    if (!file || dosHeader.e_magic != IMAGE_DOS_SIGNATURE)
    {
        g_pluginRuntime->vertex_log_error("[Injection] Invalid DOS signature in DLL");
        return std::nullopt;
    }

    file.seekg(dosHeader.e_lfanew);
    DWORD ntSignature = 0;
    file.read(reinterpret_cast<char*>(&ntSignature), sizeof(ntSignature));
    if (!file || ntSignature != IMAGE_NT_SIGNATURE)
    {
        g_pluginRuntime->vertex_log_error("[Injection] Invalid NT signature in DLL");
        return std::nullopt;
    }

    IMAGE_FILE_HEADER fileHeader{};
    file.read(reinterpret_cast<char*>(&fileHeader), sizeof(fileHeader));
    if (!file)
    {
        g_pluginRuntime->vertex_log_error("[Injection] Failed to read PE file header");
        return std::nullopt;
    }

    switch (fileHeader.Machine)
    {
    case IMAGE_FILE_MACHINE_I386:
        return ProcessArchitecture::X86;
    case IMAGE_FILE_MACHINE_AMD64:
        return ProcessArchitecture::X86_64;
    case IMAGE_FILE_MACHINE_ARM64:
        return ProcessArchitecture::ARM64;
    default:
        g_pluginRuntime->vertex_log_error("[Injection] Unrecognized PE machine type: 0x%04X", fileHeader.Machine);
        return std::nullopt;
    }
}