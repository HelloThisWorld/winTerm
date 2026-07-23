// Copyright (c) winTerm contributors.
// Licensed under the MIT license.

#pragma once

#include "DockingAndLayoutViewModel.g.h"
#include "ViewModelHelpers.h"
#include "Utils.h"

namespace winrt::Microsoft::Terminal::Settings::Editor::implementation
{
    struct DockingAndLayoutViewModel : DockingAndLayoutViewModelT<DockingAndLayoutViewModel>, ViewModelHelper<DockingAndLayoutViewModel>
    {
    public:
        DockingAndLayoutViewModel(Model::GlobalAppSettings globalSettings);

        using ViewModelHelper<DockingAndLayoutViewModel>::PropertyChanged;

        GETSET_BINDABLE_ENUM_SETTING(PaneResizeSnapPoints, Model::PaneResizeSnapPreset, _GlobalSettings.PaneResizeSnapPoints);

        PERMANENT_OBSERVABLE_PROJECTED_SETTING(_GlobalSettings, PaneResizeSnapping);
        PERMANENT_OBSERVABLE_PROJECTED_SETTING(_GlobalSettings, PaneResizeCustomRatios);
        PERMANENT_OBSERVABLE_PROJECTED_SETTING(_GlobalSettings, PaneResizeSnapThreshold);
        PERMANENT_OBSERVABLE_PROJECTED_SETTING(_GlobalSettings, PaneResizeShowIndicator);
        PERMANENT_OBSERVABLE_PROJECTED_SETTING(_GlobalSettings, PaneResizeAltDisablesSnapping);

        void ResetPaneResizeDefaults(
            const winrt::Windows::Foundation::IInspectable& sender,
            const winrt::Windows::UI::Xaml::RoutedEventArgs& args);

    private:
        Model::GlobalAppSettings _GlobalSettings;
    };
}

namespace winrt::Microsoft::Terminal::Settings::Editor::factory_implementation
{
    BASIC_FACTORY(DockingAndLayoutViewModel);
}
