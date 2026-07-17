// Copyright (c) Microsoft Corporation
// Licensed under the MIT license.

#include "pch.h"
#include <WilErrorReporting.h>

// For g_hCTerminalCoreProvider
#include "../../cascadia/TerminalCore/tracing.hpp"

#if defined(WT_BRANDING_WINTERM)
TRACELOGGING_DEFINE_PROVIDER(
    g_hTerminalControlProvider,
    "winTerm.Control",
    // {18965a29-3324-4078-a5e8-348aee483049}
    (0x18965a29, 0x3324, 0x4078, 0xa5, 0xe8, 0x34, 0x8a, 0xee, 0x48, 0x30, 0x49));
#else
// Note: Generate GUID using TlgGuid.exe tool
TRACELOGGING_DEFINE_PROVIDER(
    g_hTerminalControlProvider,
    "Microsoft.Windows.Terminal.Control",
    // {28c82e50-57af-5a86-c25b-e39cd990032b}
    (0x28c82e50, 0x57af, 0x5a86, 0xc2, 0x5b, 0xe3, 0x9c, 0xd9, 0x90, 0x03, 0x2b),
    TraceLoggingOptionMicrosoftTelemetry());
#endif

BOOL WINAPI DllMain(HINSTANCE hInstDll, DWORD reason, LPVOID /*reserved*/)
{
    switch (reason)
    {
    case DLL_PROCESS_ATTACH:
        DisableThreadLibraryCalls(hInstDll);
#if !defined(WT_BRANDING_WINTERM)
        TraceLoggingRegister(g_hTerminalControlProvider);
        TraceLoggingRegister(g_hCTerminalCoreProvider);
        Microsoft::Console::ErrorReporting::EnableFallbackFailureReporting(g_hTerminalControlProvider);
#endif
        break;
    case DLL_PROCESS_DETACH:
#if !defined(WT_BRANDING_WINTERM)
        if (g_hTerminalControlProvider)
        {
            TraceLoggingUnregister(g_hTerminalControlProvider);
        }
#endif
        break;
    }

    return TRUE;
}

UTILS_DEFINE_LIBRARY_RESOURCE_SCOPE(L"Microsoft.Terminal.Control/Resources");
