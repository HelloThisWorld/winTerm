// Copyright (c) Microsoft Corporation
// Licensed under the MIT license.

#include "pch.h"
#include <WilErrorReporting.h>

#if defined(WT_BRANDING_WINTERM)
TRACELOGGING_DEFINE_PROVIDER(
    g_hSettingsModelProvider,
    "winTerm.Settings.Model",
    // {b4bf582b-5ede-46cb-9d69-c5649e3d6e13}
    (0xb4bf582b, 0x5ede, 0x46cb, 0x9d, 0x69, 0xc5, 0x64, 0x9e, 0x3d, 0x6e, 0x13));
#else
// Note: Generate GUID using TlgGuid.exe tool
TRACELOGGING_DEFINE_PROVIDER(
    g_hSettingsModelProvider,
    "Microsoft.Windows.Terminal.Setting.Model",
    // {be579944-4d33-5202-e5d6-a7a57f1935cb}
    (0xbe579944, 0x4d33, 0x5202, 0xe5, 0xd6, 0xa7, 0xa5, 0x7f, 0x19, 0x35, 0xcb),
    TraceLoggingOptionMicrosoftTelemetry());
#endif

BOOL WINAPI DllMain(HINSTANCE hInstDll, DWORD reason, LPVOID /*reserved*/)
{
    switch (reason)
    {
    case DLL_PROCESS_ATTACH:
        DisableThreadLibraryCalls(hInstDll);
#if !defined(WT_BRANDING_WINTERM)
        TraceLoggingRegister(g_hSettingsModelProvider);
        Microsoft::Console::ErrorReporting::EnableFallbackFailureReporting(g_hSettingsModelProvider);
#endif
        break;
    case DLL_PROCESS_DETACH:
#if !defined(WT_BRANDING_WINTERM)
        if (g_hSettingsModelProvider)
        {
            TraceLoggingUnregister(g_hSettingsModelProvider);
        }
#endif
        break;
    }

    return TRUE;
}

UTILS_DEFINE_LIBRARY_RESOURCE_SCOPE(L"Microsoft.Terminal.Settings.Model/Resources")
