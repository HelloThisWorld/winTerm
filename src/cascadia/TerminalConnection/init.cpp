// Copyright (c) Microsoft Corporation
// Licensed under the MIT license.

#include "pch.h"
#include <WilErrorReporting.h>

#if defined(WT_BRANDING_WINTERM)
#pragma warning(suppress : 26477) // One of the macros uses 0/NULL. We don't have control to make it nullptr.
TRACELOGGING_DEFINE_PROVIDER(
    g_hTerminalConnectionProvider,
    "winTerm.Connection",
    // {8bebe71b-1ac1-4ff4-a9e2-f5dccae0918e}
    (0x8bebe71b, 0x1ac1, 0x4ff4, 0xa9, 0xe2, 0xf5, 0xdc, 0xca, 0xe0, 0x91, 0x8e));
#else
// Note: Generate GUID using TlgGuid.exe tool
#pragma warning(suppress : 26477) // One of the macros uses 0/NULL. We don't have control to make it nullptr.
TRACELOGGING_DEFINE_PROVIDER(
    g_hTerminalConnectionProvider,
    "Microsoft.Windows.Terminal.Connection",
    // {e912fe7b-eeb6-52a5-c628-abe388e5f792}
    (0xe912fe7b, 0xeeb6, 0x52a5, 0xc6, 0x28, 0xab, 0xe3, 0x88, 0xe5, 0xf7, 0x92),
    TraceLoggingOptionMicrosoftTelemetry());
#endif

#pragma warning(suppress : 26440) // Not interested in changing the specification of DllMain to make it noexcept given it's an interface to the OS.
BOOL WINAPI DllMain(HINSTANCE hInstDll, DWORD reason, LPVOID /*reserved*/)
{
    switch (reason)
    {
    case DLL_PROCESS_ATTACH:
        DisableThreadLibraryCalls(hInstDll);
#if !defined(WT_BRANDING_WINTERM)
        TraceLoggingRegister(g_hTerminalConnectionProvider);
        Microsoft::Console::ErrorReporting::EnableFallbackFailureReporting(g_hTerminalConnectionProvider);
#endif
        break;
    case DLL_PROCESS_DETACH:
#if !defined(WT_BRANDING_WINTERM)
        if (g_hTerminalConnectionProvider)
        {
            TraceLoggingUnregister(g_hTerminalConnectionProvider);
        }
#endif
        break;
    default:
        break;
    }

    return TRUE;
}

UTILS_DEFINE_LIBRARY_RESOURCE_SCOPE(L"Microsoft.Terminal.TerminalConnection/Resources");
