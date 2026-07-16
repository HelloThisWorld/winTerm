// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#include "pch.h"
#include "ColorSchemes.h"
#include "ColorTableEntry.g.cpp"
#include "ColorSchemes.g.cpp"
#include "../WinRTUtils/inc/Utils.h"
#include "../../winterm/Appearance/Importers/WindowsTerminalSchemeImporter.h"
#include "../../winterm/Appearance/Themes/ThemeLoader.h"

using namespace winrt;
using namespace winrt::Windows::UI;
using namespace winrt::Windows::UI::Xaml;
using namespace winrt::Windows::UI::Xaml::Navigation;
using namespace winrt::Windows::UI::Xaml::Controls;
using namespace winrt::Windows::UI::Xaml::Media;
using namespace winrt::Windows::Foundation;
using namespace winrt::Windows::Foundation::Collections;
using namespace winrt::Microsoft::UI::Xaml::Controls;

namespace winrt
{
    namespace MUX = Microsoft::UI::Xaml;
    namespace WUX = Windows::UI::Xaml;
}

namespace
{
    std::wstring Lowercase(const winrt::hstring& value)
    {
        std::wstring result{ value.c_str(), value.size() };
        std::transform(result.begin(), result.end(), result.begin(), [](const wchar_t character) {
            return static_cast<wchar_t>(std::towlower(character));
        });
        return result;
    }

    std::wstring ExportFileName(const winrt::hstring& displayName)
    {
        std::wstring result{ displayName.c_str(), displayName.size() };
        for (auto& character : result)
        {
            if (std::wstring_view{ L"<>:\"/\\|?*" }.find(character) != std::wstring_view::npos)
            {
                character = L'-';
            }
        }
        if (result.empty())
        {
            result = L"theme";
        }
        result.append(L".winterm-theme.json");
        return result;
    }
}

namespace winrt::Microsoft::Terminal::Settings::Editor::implementation
{
    ColorSchemes::ColorSchemes()
    {
        InitializeComponent();

        Automation::AutomationProperties::SetName(AddNewButton(), RS_(L"ColorScheme_AddNewButton/Text"));
    }

    void ColorSchemes::OnNavigatedTo(const NavigationEventArgs& e)
    {
        const auto args = e.Parameter().as<Editor::NavigateToPageArgs>();
        _ViewModel = args.ViewModel().as<Editor::ColorSchemesPageViewModel>();
        _weakWindowRoot = args.WindowRoot();
        _ViewModel.CurrentPage(ColorSchemesSubPage::Base);
        BringIntoViewWhenLoaded(args.ElementToFocus());

        _layoutUpdatedRevoker = LayoutUpdated(winrt::auto_revoke, [this](auto /*s*/, auto /*e*/) {
            // Only let this succeed once.
            _layoutUpdatedRevoker.revoke();

            ColorSchemeListView().Focus(FocusState::Programmatic);
        });

        TraceLoggingWrite(
            g_hTerminalSettingsEditorProvider,
            "NavigatedToPage",
            TraceLoggingDescription("Event emitted when the user navigates to a page in the settings UI"),
            TraceLoggingValue("colorSchemes", "PageId", "The identifier of the page that was navigated to"),
            TraceLoggingKeyword(MICROSOFT_KEYWORD_MEASURES),
            TelemetryPrivacyDataTag(PDT_ProductAndServiceUsage));
    }

    void ColorSchemes::AddNew_Click(const IInspectable& /*sender*/, const RoutedEventArgs& /*e*/)
    {
        if (const auto newSchemeVM{ _ViewModel.RequestAddNew() })
        {
            ColorSchemeListView().SelectedItem(newSchemeVM);
            _ViewModel.RequestEditSelectedScheme();
        }
    }

    safe_void_coroutine ColorSchemes::Import_Click(const IInspectable&, const RoutedEventArgs&)
    {
        auto lifetime = get_strong();
        std::string errorMessage;

        static constexpr COMDLG_FILTERSPEC supportedFileTypes[] = {
            { L"Theme files (*.winterm-theme.json, *.json, *.itermcolors, *.terminal)", L"*.winterm-theme.json;*.json;*.itermcolors;*.terminal" },
            { L"All files (*.*)", L"*.*" },
        };
        static constexpr winrt::guid clientGuidThemes{ 0x2574ebf0, 0x652d, 0x433b, { 0x85, 0x52, 0xd5, 0xcf, 0xf8, 0x73, 0xb4, 0xf1 } };

        try
        {
            const auto windowRoot{ _weakWindowRoot.get() };
            if (!windowRoot)
            {
                co_return;
            }

            const auto parentHwnd{ reinterpret_cast<HWND>(windowRoot.GetHostingWindow()) };
            const auto path = co_await OpenFilePicker(parentHwnd, [](auto&& dialog) {
                THROW_IF_FAILED(dialog->SetClientGuid(clientGuidThemes));
                THROW_IF_FAILED(dialog->SetFileTypes(ARRAYSIZE(supportedFileTypes), supportedFileTypes));
                THROW_IF_FAILED(dialog->SetFileTypeIndex(1));
            });
            if (path.empty())
            {
                co_return;
            }

            auto importResult{ winTerm::Appearance::ThemeLoader::ImportFile(std::filesystem::path{ path.c_str() }) };
            if (importResult.themes.empty())
            {
                throw std::runtime_error{ "The selected file does not contain a supported theme." };
            }

            ImportThemeChoice().Items().Clear();
            for (const auto& theme : importResult.themes)
            {
                ImportThemeChoice().Items().Append(box_value(winrt::to_hstring(theme.displayName)));
            }
            ImportThemeChoice().SelectedIndex(0);
            ImportThemeChoice().Visibility(importResult.themes.size() > 1 ? Visibility::Visible : Visibility::Collapsed);
            ImportSummaryText().Text(winrt::to_hstring(importResult.summary.ToDisplayText()));

            const auto dialogResult = co_await ImportThemeDialog().ShowAsync();
            if (dialogResult != ContentDialogResult::Primary)
            {
                co_return;
            }

            const auto selectedIndex{ ImportThemeChoice().SelectedIndex() };
            if (selectedIndex < 0 || static_cast<size_t>(selectedIndex) >= importResult.themes.size())
            {
                throw std::runtime_error{ "Select a theme to import." };
            }

            const auto viewModelImpl{ winrt::get_self<ColorSchemesPageViewModel>(_ViewModel) };
            const auto schemeViewModel{ viewModelImpl->RequestAddImported(importResult.themes[static_cast<size_t>(selectedIndex)].ToWindowsTerminalSchemeJson()) };
            ThemeSearchBox().Text(L"");
            ColorSchemeListView().ItemsSource(_ViewModel.AllColorSchemes());
            ColorSchemeListView().SelectedItem(schemeViewModel);
        }
        catch (const winrt::hresult_error& error)
        {
            errorMessage = winrt::to_string(error.message());
        }
        catch (const std::exception& error)
        {
            errorMessage = error.what();
        }
        if (!errorMessage.empty())
        {
            co_await _ShowThemeError(errorMessage);
        }
    }

    safe_void_coroutine ColorSchemes::Export_Click(const IInspectable&, const RoutedEventArgs&)
    {
        auto lifetime = get_strong();
        std::string errorMessage;

        static constexpr COMDLG_FILTERSPEC supportedFileTypes[] = {
            { L"winTerm theme (*.winterm-theme.json)", L"*.winterm-theme.json" },
            { L"JSON file (*.json)", L"*.json" },
        };
        static constexpr winrt::guid clientGuidThemeExports{ 0xa81c19be, 0xe005, 0x47db, { 0x80, 0xa4, 0xd1, 0x55, 0x51, 0x25, 0xa9, 0x48 } };

        try
        {
            const auto windowRoot{ _weakWindowRoot.get() };
            if (!windowRoot)
            {
                co_return;
            }

            const auto viewModelImpl{ winrt::get_self<ColorSchemesPageViewModel>(_ViewModel) };
            const auto schemeJson{ viewModelImpl->CurrentSchemeJson() };
            Json::StreamWriterBuilder writer;
            writer["commentStyle"] = "None";
            writer["indentation"] = "";
            auto imported{ winTerm::Appearance::WindowsTerminalSchemeImporter{}.Import(Json::writeString(writer, schemeJson), "export.json") };
            if (imported.themes.empty())
            {
                throw std::runtime_error{ "The selected theme could not be exported." };
            }

            auto theme{ std::move(imported.themes.front()) };
            theme.source.project = "winTerm";
            theme.source.author = "winTerm user";
            theme.source.license = "User supplied";
            theme.source.revision.reset();
            theme.source.sourceFile.reset();
            theme.originalFileName.reset();

            const auto defaultFileName{ ExportFileName(_ViewModel.CurrentScheme().Name()) };
            const auto parentHwnd{ reinterpret_cast<HWND>(windowRoot.GetHostingWindow()) };
            const auto path = co_await SaveFilePicker(parentHwnd, [defaultFileName](auto&& dialog) {
                THROW_IF_FAILED(dialog->SetClientGuid(clientGuidThemeExports));
                THROW_IF_FAILED(dialog->SetFileTypes(ARRAYSIZE(supportedFileTypes), supportedFileTypes));
                THROW_IF_FAILED(dialog->SetFileTypeIndex(1));
                THROW_IF_FAILED(dialog->SetDefaultExtension(L"winterm-theme.json"));
                THROW_IF_FAILED(dialog->SetFileName(defaultFileName.c_str()));
            });
            if (!path.empty())
            {
                winTerm::Appearance::ThemeLoader::ExportFile(theme, std::filesystem::path{ path.c_str() });
            }
        }
        catch (const winrt::hresult_error& error)
        {
            errorMessage = winrt::to_string(error.message());
        }
        catch (const std::exception& error)
        {
            errorMessage = error.what();
        }
        if (!errorMessage.empty())
        {
            co_await _ShowThemeError(errorMessage);
        }
    }

    void ColorSchemes::SearchBox_TextChanged(const AutoSuggestBox& sender, const AutoSuggestBoxTextChangedEventArgs&)
    {
        const auto searchText{ Lowercase(sender.Text()) };
        if (searchText.empty())
        {
            ColorSchemeListView().ItemsSource(_ViewModel.AllColorSchemes());
            return;
        }

        std::vector<Editor::ColorSchemeViewModel> filteredSchemes;
        for (const auto& scheme : _ViewModel.AllColorSchemes())
        {
            if (Lowercase(scheme.Name()).find(searchText) != std::wstring::npos)
            {
                filteredSchemes.emplace_back(scheme);
            }
        }
        ColorSchemeListView().ItemsSource(single_threaded_observable_vector<Editor::ColorSchemeViewModel>(std::move(filteredSchemes)));
    }

    IAsyncAction ColorSchemes::_ShowThemeError(const std::string_view message)
    {
        auto lifetime = get_strong();
        ThemeFileErrorText().Text(winrt::to_hstring(message));
        co_await ThemeFileErrorDialog().ShowAsync();
    }

    void ColorSchemes::ListView_PreviewKeyDown(const IInspectable& /*sender*/, const winrt::Windows::UI::Xaml::Input::KeyRoutedEventArgs& e)
    {
        if (e.OriginalKey() == winrt::Windows::System::VirtualKey::Enter)
        {
            // Treat this as if 'edit' was clicked
            _ViewModel.RequestEditSelectedScheme();
            e.Handled(true);
        }
    }
}
