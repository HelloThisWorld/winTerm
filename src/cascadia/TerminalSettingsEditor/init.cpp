// Copyright (c) Microsoft Corporation
// Licensed under the MIT license.

#include "pch.h"
#include <LibraryResources.h>
#include <WilErrorReporting.h>

#if defined(WT_BRANDING_WINTERM)
TRACELOGGING_DEFINE_PROVIDER(
    g_hTerminalSettingsEditorProvider,
    "winTerm.Settings.Editor",
    // {5bc90042-5aa3-40a3-b340-a4d3facb06ae}
    (0x5bc90042, 0x5aa3, 0x40a3, 0xb3, 0x40, 0xa4, 0xd3, 0xfa, 0xcb, 0x06, 0xae));
#else
// Note: Generate GUID using TlgGuid.exe tool
TRACELOGGING_DEFINE_PROVIDER(
    g_hTerminalSettingsEditorProvider,
    "Microsoft.Windows.Terminal.Settings.Editor",
    // {1b16317d-b594-51f8-c552-5d50572b5efc}
    (0x1b16317d, 0xb594, 0x51f8, 0xc5, 0x52, 0x5d, 0x50, 0x57, 0x2b, 0x5e, 0xfc),
    TraceLoggingOptionMicrosoftTelemetry());
#endif

BOOL WINAPI DllMain(HINSTANCE hInstDll, DWORD reason, LPVOID /*reserved*/)
{
    switch (reason)
    {
    case DLL_PROCESS_ATTACH:
        DisableThreadLibraryCalls(hInstDll);
#if !defined(WT_BRANDING_WINTERM)
        TraceLoggingRegister(g_hTerminalSettingsEditorProvider);
        Microsoft::Console::ErrorReporting::EnableFallbackFailureReporting(g_hTerminalSettingsEditorProvider);
#endif
        break;
    case DLL_PROCESS_DETACH:
#if !defined(WT_BRANDING_WINTERM)
        if (g_hTerminalSettingsEditorProvider)
        {
            TraceLoggingUnregister(g_hTerminalSettingsEditorProvider);
        }
#endif
        break;
    }

    return TRUE;
}

UTILS_DEFINE_LIBRARY_RESOURCE_SCOPE(L"Microsoft.Terminal.Settings.Editor/Resources");
