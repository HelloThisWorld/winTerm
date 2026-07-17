// Copyright (c) winTerm contributors.
// Licensed under the MIT license.

#pragma once

#include <cstddef>
#include <string>
#include <vector>

namespace winTerm::Settings
{
    struct DockingRuntimeReadiness
    {
        bool sameProcessWindowHosting{ true };
        bool liveViewReattachment{ true };
        bool transactionRollbackVerified{ false };
        bool runtimeUiVerified{ false };
    };

    struct DockingSettingsModel
    {
        bool enableTabDragging{ true };
        bool enablePaneDragging{ true };
        bool allowTabTearOut{ true };
        bool allowPaneTearOut{ true };
        bool allowCrossWindowTransfer{ true };
        bool showDockingOverlay{ true };
        bool showLayoutPreview{ true };

        bool enableEdgeDocking{ true };
        bool enableCornerDocking{ true };
        bool enableEmptyLayoutSlots{ true };
        double cornerWidthRatio{ 0.35 };
        double defaultSplitRatio{ 0.5 };

        bool focusMovedContent{ true };
        bool keepEmptyTabInLastWindow{ true };
        bool closeEmptySourceWindow{ true };
        bool confirmLargeSubtreeMove{ true };

        bool enableLayoutUndo{ true };
        size_t layoutHistorySize{ 20 };
        bool includePaneResizeInHistory{ false };

        bool enableKeyboardDockingMode{ true };
        bool showZoneLabels{ true };
        bool useHighContrastOverlay{ false };

        // The UI adapter must opt in only after runtime build and rollback verification.
        bool enableRuntimeDocking{ false };

        void Normalize() noexcept;
        std::vector<std::string> Validate() const;
        bool RuntimeDockingAvailable(const DockingRuntimeReadiness& readiness) const noexcept;
        bool CrossWindowTransferAvailable(
            const DockingRuntimeReadiness& readiness,
            bool sameProcess) const noexcept;
    };
}
