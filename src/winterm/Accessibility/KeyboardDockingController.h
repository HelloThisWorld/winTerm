// Copyright (c) winTerm contributors.
// Licensed under the MIT license.

#pragma once

#include "../Docking/Model/DockingModel.h"

#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace winTerm::Accessibility
{
    enum class KeyboardDockingState
    {
        Inactive,
        SelectingZone,
        CommitRequested,
        Cancelled,
    };

    enum class DockingNavigationKey
    {
        Left,
        Right,
        Up,
        Down,
        Enter,
        Escape,
    };

    struct DockingCommandDescriptor
    {
        std::string id;
        std::string name;
        std::optional<Docking::DockZone> zone;
        Docking::DockSourceType sourceType{ Docking::DockSourceType::Tab };
    };

    class KeyboardDockingController
    {
    public:
        bool Start(
            std::string sourceName,
            std::string targetName,
            std::vector<Docking::DockZone> availableZones,
            Docking::DockZone initialZone = Docking::DockZone::Center);
        bool Handle(DockingNavigationKey key);
        void Reset() noexcept;

        KeyboardDockingState State() const noexcept;
        std::optional<Docking::DockZone> SelectedZone() const noexcept;
        std::string Announcement() const;

        static std::string_view AccessibleZoneName(Docking::DockZone zone) noexcept;
        static const std::vector<DockingCommandDescriptor>& Commands();

    private:
        bool _move(int columnDelta, int rowDelta);
        bool _select(Docking::DockZone zone);

        KeyboardDockingState _state{ KeyboardDockingState::Inactive };
        std::string _sourceName;
        std::string _targetName;
        std::vector<Docking::DockZone> _availableZones;
        std::optional<Docking::DockZone> _selectedZone;
    };
}
