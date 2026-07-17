// Copyright (c) winTerm contributors.
// Licensed under the MIT license.

#include "pch.h"

#include "../../winterm/Accessibility/KeyboardDockingController.h"
#include "../../winterm/Docking/Layout/LayoutTransformer.h"
#include "../../winterm/Docking/Overlay/DockingOverlayModel.h"
#include "../../winterm/Docking/Targets/DockTargetResolver.h"
#include "../../winterm/Settings/Docking/DockingSettingsModel.h"

using namespace WEX::TestExecution;
using namespace winTerm::Accessibility;
using namespace winTerm::Docking;
using namespace winTerm::Settings;
using namespace winTerm::Workspaces;

namespace
{
    DockTargetCandidate Candidate(
        const std::string& key,
        const DockTargetType type,
        const LayoutRect bounds,
        const DockZone zone = DockZone::Center)
    {
        DockTargetCandidate candidate;
        candidate.key = key;
        candidate.target.type = type;
        candidate.zone = zone;
        candidate.bounds = bounds;
        return candidate;
    }

    DockSource PaneSource()
    {
        DockSource source;
        source.type = DockSourceType::Pane;
        source.windowId = "window-source";
        source.tabId = "tab-source";
        source.paneId = "pane-source";
        return source;
    }

    DockTarget PaneTarget()
    {
        DockTarget target;
        target.type = DockTargetType::Pane;
        target.windowId = "window-target";
        target.tabId = "tab-target";
        target.nodeId = "pane-target";
        return target;
    }
}

namespace SettingsModelUnitTests
{
    class WinTermDockingPresentationTests
    {
        TEST_CLASS(WinTermDockingPresentationTests);

        TEST_METHOD(TargetResolverUsesDocumentedPriority);
        TEST_METHOD(TargetResolverUsesHysteresis);
        TEST_METHOD(OverlayHidesCornersAndDescribesDisabledZones);
        TEST_METHOD(PreviewUsesProposedLayoutGeometry);
        TEST_METHOD(KeyboardModeAnnouncesSelectionAndCancellation);
        TEST_METHOD(SettingsKeepRuntimeDockingBehindReadinessGate);
    };

    void WinTermDockingPresentationTests::TargetResolverUsesDocumentedPriority()
    {
        DockTargetResolver resolver;
        const std::vector candidates{
            Candidate("window", DockTargetType::WindowContent, { 0, 0, 100, 100 }),
            Candidate("pane", DockTargetType::Pane, { 0, 0, 100, 100 }),
            Candidate("slot", DockTargetType::EmptySlot, { 0, 0, 100, 100 }),
            Candidate("strip", DockTargetType::TabStrip, { 0, 0, 100, 100 }),
        };
        const auto resolved = resolver.Resolve(candidates, { 50, 50 });
        VERIFY_IS_TRUE(resolved.has_value());
        VERIFY_ARE_EQUAL(std::string{ "strip" }, resolved->key);
    }

    void WinTermDockingPresentationTests::TargetResolverUsesHysteresis()
    {
        DockTargetResolver resolver{ { std::chrono::milliseconds{ 75 }, 100.0 } };
        const auto now = std::chrono::steady_clock::now();
        const std::vector candidates{
            Candidate("left", DockTargetType::Pane, { 0, 0, 50, 100 }, DockZone::Left),
            Candidate("right", DockTargetType::Pane, { 50, 0, 50, 100 }, DockZone::Right),
        };
        VERIFY_ARE_EQUAL(std::string{ "left" }, resolver.Resolve(candidates, { 25, 50 }, now)->key);
        VERIFY_ARE_EQUAL(std::string{ "left" }, resolver.Resolve(candidates, { 75, 50 }, now)->key);
        VERIFY_ARE_EQUAL(
            std::string{ "right" },
            resolver.Resolve(candidates, { 75, 50 }, now + std::chrono::milliseconds{ 80 })->key);
    }

    void WinTermDockingPresentationTests::OverlayHidesCornersAndDescribesDisabledZones()
    {
        DockingCapabilities capabilities;
        capabilities.canDockEdges = false;
        const auto overlay = DockingOverlayModel::Build(
            { 0, 0, 400, 240 },
            PaneSource(),
            PaneTarget(),
            capabilities,
            true,
            DockZone::Left);
        VERIFY_ARE_EQUAL(size_t{ 5 }, overlay.size());
        const auto left = std::find_if(
            overlay.begin(),
            overlay.end(),
            [](const auto& item) { return item.zone == DockZone::Left; });
        VERIFY_IS_TRUE(left != overlay.end());
        VERIFY_IS_FALSE(left->enabled);
        VERIFY_IS_FALSE(left->disabledReason.empty());
        VERIFY_ARE_EQUAL(std::string{ "Button" }, left->automationRole);
    }

    void WinTermDockingPresentationTests::PreviewUsesProposedLayoutGeometry()
    {
        DockingCapabilities capabilities;
        capabilities.sourcePaneCount = 1;
        const auto plan = LayoutTransformer::BuildProposedLayout(
            PaneSource(),
            PaneTarget(),
            DockZone::Right,
            LayoutNodeDescriptor::Pane("pane-source"),
            LayoutNodeDescriptor::Pane("pane-target"),
            {},
            capabilities,
            true);
        const auto preview = DockPreview::Build(plan, { 0, 0, 1000, 600 });
        VERIFY_IS_TRUE(preview.valid);
        VERIFY_ARE_EQUAL(size_t{ 2 }, preview.regions.size());
        VERIFY_ARE_EQUAL(500.0, preview.regions[0].bounds.width);
        VERIFY_ARE_EQUAL(500.0, preview.regions[1].bounds.width);
        VERIFY_ARE_EQUAL(
            static_cast<int>(DockPreviewRegionRole::Source),
            static_cast<int>(preview.regions[1].role));
    }

    void WinTermDockingPresentationTests::KeyboardModeAnnouncesSelectionAndCancellation()
    {
        KeyboardDockingController controller;
        VERIFY_IS_TRUE(controller.Start(
            "PowerShell tab",
            "Titan Mind window",
            { DockZone::Center, DockZone::Left, DockZone::Right },
            DockZone::Center));
        VERIFY_IS_TRUE(controller.Handle(DockingNavigationKey::Right));
        VERIFY_IS_TRUE(controller.Announcement().find("right side of Titan Mind window") != std::string::npos);
        VERIFY_IS_TRUE(controller.Handle(DockingNavigationKey::Escape));
        VERIFY_ARE_EQUAL(
            static_cast<int>(KeyboardDockingState::Cancelled),
            static_cast<int>(controller.State()));
        VERIFY_IS_TRUE(controller.Announcement().find("original layout is unchanged") != std::string::npos);
        VERIFY_ARE_EQUAL(size_t{ 19 }, KeyboardDockingController::Commands().size());
    }

    void WinTermDockingPresentationTests::SettingsKeepRuntimeDockingBehindReadinessGate()
    {
        DockingSettingsModel settings;
        DockingRuntimeReadiness readiness;
        VERIFY_IS_FALSE(settings.RuntimeDockingAvailable(readiness));
        settings.enableRuntimeDocking = true;
        readiness.transactionRollbackVerified = true;
        readiness.runtimeUiVerified = true;
        VERIFY_IS_TRUE(settings.RuntimeDockingAvailable(readiness));
        VERIFY_IS_TRUE(settings.CrossWindowTransferAvailable(readiness, true));
        VERIFY_IS_FALSE(settings.CrossWindowTransferAvailable(readiness, false));
    }
}
