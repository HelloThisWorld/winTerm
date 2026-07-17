// Copyright (c) Microsoft Corporation
// Licensed under the MIT license.

#include "pch.h"
#include <WilErrorReporting.h>

// winTerm deliberately leaves its provider unregistered. The distinct name and
// identifier prevent the binary from presenting itself as a Microsoft product.
#if defined(WT_BRANDING_WINTERM)
TRACELOGGING_DEFINE_PROVIDER(
    g_hTerminalAppProvider,
    "winTerm.App",
    // {21379562-c011-418c-9ae0-67a61f4b16c9}
    (0x21379562, 0xc011, 0x418c, 0x9a, 0xe0, 0x67, 0xa6, 0x1f, 0x4b, 0x16, 0xc9));
#else
// Note: Generate GUID using TlgGuid.exe tool
TRACELOGGING_DEFINE_PROVIDER(
    g_hTerminalAppProvider,
    "Microsoft.Windows.Terminal.App",
    // {24a1622f-7da7-5c77-3303-d850bd1ab2ed}
    (0x24a1622f, 0x7da7, 0x5c77, 0x33, 0x03, 0xd8, 0x50, 0xbd, 0x1a, 0xb2, 0xed),
    TraceLoggingOptionMicrosoftTelemetry());
#endif

BOOL WINAPI DllMain(HINSTANCE hInstDll, DWORD reason, LPVOID /*reserved*/)
{
    switch (reason)
    {
    case DLL_PROCESS_ATTACH:
        DisableThreadLibraryCalls(hInstDll);
#if !defined(WT_BRANDING_WINTERM)
        TraceLoggingRegister(g_hTerminalAppProvider);
        Microsoft::Console::ErrorReporting::EnableFallbackFailureReporting(g_hTerminalAppProvider);
#endif
        break;
    case DLL_PROCESS_DETACH:
#if !defined(WT_BRANDING_WINTERM)
        if (g_hTerminalAppProvider)
        {
            TraceLoggingUnregister(g_hTerminalAppProvider);
        }
#endif
        break;
    }

    return TRUE;
}

UTILS_DEFINE_LIBRARY_RESOURCE_SCOPE(L"TerminalApp/Resources")
