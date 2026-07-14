// Copyright (c) winTerm contributors.
// Licensed under the MIT license.

#include <windows.h>
#include <shellapi.h>

#include <iostream>
#include <string_view>

namespace
{
    constexpr int Success{ 0 };
    constexpr int GeneralError{ 1 };
    constexpr int InvalidArguments{ 2 };
    constexpr int TargetNotFound{ 3 };
    constexpr int AccessDenied{ 4 };
    constexpr int UnsupportedOperation{ 5 };

    int ExitCodeForWin32Error(const DWORD error) noexcept
    {
        switch (error)
        {
        case ERROR_FILE_NOT_FOUND:
        case ERROR_PATH_NOT_FOUND:
            return TargetNotFound;
        case ERROR_ACCESS_DENIED:
        case ERROR_SHARING_VIOLATION:
            return AccessDenied;
        default:
            return GeneralError;
        }
    }

    void WriteWin32Error(const std::wstring_view operation, const DWORD error)
    {
        std::wcerr << L"winterm-shim: " << operation << L" failed (Windows error " << error << L").\n";
    }

    int TouchPath(const wchar_t* const path)
    {
        const auto handle = CreateFileW(path,
                                        FILE_WRITE_ATTRIBUTES,
                                        FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
                                        nullptr,
                                        OPEN_EXISTING,
                                        FILE_ATTRIBUTE_NORMAL | FILE_FLAG_BACKUP_SEMANTICS,
                                        nullptr);
        if (handle != INVALID_HANDLE_VALUE)
        {
            FILETIME now{};
            GetSystemTimeAsFileTime(&now);
            if (!SetFileTime(handle, nullptr, nullptr, &now))
            {
                const auto error = GetLastError();
                CloseHandle(handle);
                WriteWin32Error(L"updating the target timestamp", error);
                return ExitCodeForWin32Error(error);
            }
            CloseHandle(handle);
            return Success;
        }

        const auto existingError = GetLastError();
        if (existingError != ERROR_FILE_NOT_FOUND && existingError != ERROR_PATH_NOT_FOUND)
        {
            WriteWin32Error(L"opening the target", existingError);
            return ExitCodeForWin32Error(existingError);
        }

        const auto created = CreateFileW(path,
                                         GENERIC_WRITE,
                                         FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
                                         nullptr,
                                         CREATE_NEW,
                                         FILE_ATTRIBUTE_NORMAL,
                                         nullptr);
        if (created == INVALID_HANDLE_VALUE)
        {
            const auto error = GetLastError();
            WriteWin32Error(L"creating the target", error);
            return ExitCodeForWin32Error(error);
        }
        CloseHandle(created);
        return Success;
    }

    int OpenTarget(const wchar_t* const target)
    {
        const auto result = reinterpret_cast<INT_PTR>(ShellExecuteW(nullptr, L"open", target, nullptr, nullptr, SW_SHOWNORMAL));
        if (result > 32)
        {
            return Success;
        }

        std::wcerr << L"winterm-shim: Windows could not open the requested target (ShellExecute error " << result << L").\n";
        return result == SE_ERR_FNF || result == SE_ERR_PNF ? TargetNotFound : GeneralError;
    }

    void PrintUsage()
    {
        std::wcerr << L"Usage: winterm-shim.exe <touch|open|version|doctor> [arguments]\n";
    }
}

int wmain(const int argc, wchar_t* argv[])
{
    if (argc < 2)
    {
        PrintUsage();
        return InvalidArguments;
    }

    const std::wstring_view command{ argv[1] };
    if (command == L"touch")
    {
        if (argc < 3)
        {
            PrintUsage();
            return InvalidArguments;
        }

        auto result = Success;
        for (auto index = 2; index < argc; ++index)
        {
            const auto touchResult = TouchPath(argv[index]);
            if (touchResult != Success)
            {
                result = touchResult;
            }
        }
        return result;
    }

    if (command == L"open")
    {
        if (argc != 3)
        {
            PrintUsage();
            return InvalidArguments;
        }
        return OpenTarget(argv[2]);
    }

    if (command == L"version")
    {
        std::wcout << L"winterm-shim 0.3.0\n";
        return Success;
    }

    if (command == L"doctor")
    {
        std::wcout << L"winterm-shim 0.3.0 is available. No network checks were performed.\n";
        return Success;
    }

    PrintUsage();
    return UnsupportedOperation;
}
