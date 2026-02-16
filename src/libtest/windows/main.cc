//
// Copyright (C) 2026 PHTNC<>.
// Licensed under LGPLv3.0+
//

#include <Windows.h>

BOOL WINAPI DllMain(const HINSTANCE hDll, const DWORD fdwReason, [[maybe_unused]] LPVOID lpvReserved)
{
    switch (fdwReason)
    {
    case DLL_PROCESS_ATTACH:
        DisableThreadLibraryCalls(hDll);
        MessageBox(nullptr, TEXT("Hello from the remote target!"), TEXT("DLL Injected"), MB_OK | MB_ICONINFORMATION);
        break;
    case DLL_PROCESS_DETACH:
        break;
    default:;
    }
    return TRUE;
}