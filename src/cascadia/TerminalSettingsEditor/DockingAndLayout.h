// Copyright (c) winTerm contributors.
// Licensed under the MIT license.

#pragma once

#include "DockingAndLayout.g.h"
#include "Utils.h"

namespace winrt::Microsoft::Terminal::Settings::Editor::implementation
{
    struct DockingAndLayout : public HasScrollViewer<DockingAndLayout>, DockingAndLayoutT<DockingAndLayout>
    {
        DockingAndLayout();

        void OnNavigatedTo(const winrt::Windows::UI::Xaml::Navigation::NavigationEventArgs& e);

        til::property_changed_event PropertyChanged;
        WINRT_OBSERVABLE_PROPERTY(Editor::DockingAndLayoutViewModel, ViewModel, PropertyChanged.raise, nullptr);
    };
}

namespace winrt::Microsoft::Terminal::Settings::Editor::factory_implementation
{
    BASIC_FACTORY(DockingAndLayout);
}
