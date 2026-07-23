// Copyright (c) winTerm contributors.
// Licensed under the MIT license.

#include "pch.h"
#include "DockingAndLayoutViewModel.h"
#include "DockingAndLayoutViewModel.g.cpp"
#include "EnumEntry.h"

namespace winrt::Microsoft::Terminal::Settings::Editor::implementation
{
    DockingAndLayoutViewModel::DockingAndLayoutViewModel(Model::GlobalAppSettings globalSettings) :
        _GlobalSettings{ globalSettings }
    {
        INITIALIZE_BINDABLE_ENUM_SETTING(
            PaneResizeSnapPoints,
            PaneResizeSnapPreset,
            Model::PaneResizeSnapPreset,
            L"Globals_PaneResizeSnapPoints",
            L"Content");
    }

    void DockingAndLayoutViewModel::ResetPaneResizeDefaults(
        const winrt::Windows::Foundation::IInspectable&,
        const winrt::Windows::UI::Xaml::RoutedEventArgs&)
    {
        PaneResizeSnapping(true);
        CurrentPaneResizeSnapPoints(winrt::box_value(Model::PaneResizeSnapPreset::CommonRatios));
        PaneResizeCustomRatios(L"");
        PaneResizeSnapThreshold(8.0);
        PaneResizeShowIndicator(true);
        PaneResizeAltDisablesSnapping(true);
    }
}
