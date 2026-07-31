// Copyright (c) winTerm contributors.
// Licensed under the MIT license.

#pragma once

#include "RainbowArcVisualConstants.h"
#include "VisualProgressRenderModel.h"

#include <winrt/Windows.Foundation.h>
#include <winrt/Windows.Foundation.Collections.h>
#include <winrt/Windows.Foundation.Numerics.h>
#include <winrt/Windows.UI.h>
#include <winrt/Windows.UI.Composition.h>
#include <winrt/Windows.UI.Core.h>
#include <winrt/Windows.UI.ViewManagement.h>
#include <winrt/Windows.UI.Xaml.h>
#include <winrt/Windows.UI.Xaml.Automation.h>
#include <winrt/Windows.UI.Xaml.Automation.Peers.h>
#include <winrt/Windows.UI.Xaml.Controls.h>
#include <winrt/Windows.UI.Xaml.Hosting.h>
#include <winrt/Windows.UI.Xaml.Media.h>

#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <memory>
#include <optional>

namespace winTerm::VisualProgress
{
    namespace WU = winrt::Windows::UI;
    namespace WUC = winrt::Windows::UI::Composition;
    namespace WUCore = winrt::Windows::UI::Core;
    namespace WUVM = winrt::Windows::UI::ViewManagement;
    namespace WUX = winrt::Windows::UI::Xaml;
    namespace WUXA = winrt::Windows::UI::Xaml::Automation;
    namespace WUXAP = winrt::Windows::UI::Xaml::Automation::Peers;
    namespace WUXC = winrt::Windows::UI::Xaml::Controls;
    namespace WUXH = winrt::Windows::UI::Xaml::Hosting;
    namespace WUXM = winrt::Windows::UI::Xaml::Media;
    namespace Numerics = winrt::Windows::Foundation::Numerics;

    // A fail-open, pane-local composition renderer. It owns no CPU frame loop
    // and no timer. Continuous motion, comet travel, breathing, fades, and
    // particle movement are all executed by Windows.UI.Composition.
    //
    // TryCreate and all public mutators must be called on the XAML UI thread.
    // Any failure degrades decorative output without escaping to terminal code.
    class RainbowArcRenderer final : public std::enable_shared_from_this<RainbowArcRenderer>
    {
    public:
        static std::shared_ptr<RainbowArcRenderer> TryCreate(const winrt::Windows::UI::Xaml::Controls::Grid& host) noexcept
        {
            if (!host)
            {
                return nullptr;
            }

            try
            {
                auto renderer = std::shared_ptr<RainbowArcRenderer>{ new RainbowArcRenderer{} };
                if (!renderer->_initialize(host))
                {
                    return nullptr;
                }
                renderer->_subscribeEvents();
                renderer->RefreshEnvironment();
                return renderer;
            }
            catch (...)
            {
                return nullptr;
            }
        }

        ~RainbowArcRenderer()
        {
            Close();
        }

        RainbowArcRenderer(const RainbowArcRenderer&) = delete;
        RainbowArcRenderer& operator=(const RainbowArcRenderer&) = delete;

        void Apply(const ProgressSnapshot& snapshot) noexcept
        {
            if (_closed || _faulted)
            {
                return;
            }

            ++_presentationGeneration;
            _terminalPresentationCompleted = false;
            _snapshot = snapshot;
            _refreshRuntimeEnvironment();
            const auto now = _now();
            auto plan = _renderState.Apply(snapshot, _environment, now);
            _applyWithDegradation(plan, true);
        }

        void SetPaneActive(const bool active) noexcept
        {
            if (_closed || _faulted || _environment.paneActive == active)
            {
                return;
            }

            _environment.paneActive = active;
            const auto plan = _renderState.RefreshEnvironment(_environment, _now());
            _applyWithDegradation(plan, false);
        }

        void RefreshEnvironment() noexcept
        {
            if (_closed || _faulted)
            {
                return;
            }

            _refreshRuntimeEnvironment();
            const auto plan = _renderState.RefreshEnvironment(_environment, _now());
            _applyWithDegradation(plan, false);
        }

        void Close() noexcept
        {
            if (_closed)
            {
                return;
            }
            _closed = true;

            try
            {
                _unsubscribeEvents();
                _stopAllAnimations();
                _releaseAllSparks();
                _clearTerminalBatch();
                _renderState.Close();

                if (_host)
                {
                    try
                    {
                        winrt::Windows::UI::Xaml::Hosting::ElementCompositionPreview::SetElementChildVisual(
                            _host,
                            winrt::Windows::UI::Composition::Visual{ nullptr });
                    }
                    catch (...)
                    {
                    }
                    _removeFallbackVisuals();
                }
            }
            catch (...)
            {
            }

            _releaseCompositionHandles();
            _host = nullptr;
            _coreWindow = nullptr;
            _accessibilitySettings = nullptr;
            _uiSettings = nullptr;
        }

        bool Faulted() const noexcept
        {
            return _faulted;
        }

        RenderTier Tier() const noexcept
        {
            return _renderState.Tier();
        }

        bool UsesStaticFallback() const noexcept
        {
            return _environment.UsesStaticFallback() || _renderState.Tier() >= RenderTier::StaticGradient;
        }

        uint8_t LiveSparkCount() const noexcept
        {
            return _sparkPool.Live();
        }

        static uint8_t SharedLiveSparkCount() noexcept
        {
            return _sharedSparkBudget.Live();
        }

    private:
        struct SparkVisual
        {
            WUC::SpriteVisual visual{ nullptr };
            WUC::CompositionColorBrush brush{ nullptr };
            WUC::Vector3KeyFrameAnimation ambientMovement{ nullptr };
            WUC::ScalarKeyFrameAnimation ambientOpacity{ nullptr };
            WUC::ColorKeyFrameAnimation ambientColor{ nullptr };
            WUC::Vector3KeyFrameAnimation burstMovement{ nullptr };
            WUC::ScalarKeyFrameAnimation burstOpacity{ nullptr };
            WUC::ColorKeyFrameAnimation burstColor{ nullptr };
            WUC::CompositionScopedBatch batch{ nullptr };
            winrt::event_token completedToken{};
            bool completedSubscribed{};
            std::optional<SparkHandle> handle;
            bool ambient{};
        };

        RainbowArcRenderer() noexcept :
            _sparkPool{ _sharedSparkBudget }
        {
        }

        static RenderTimestamp _now() noexcept
        {
            return std::chrono::duration_cast<RenderTimestamp>(std::chrono::steady_clock::now().time_since_epoch());
        }

        static winrt::Windows::Foundation::TimeSpan _timeSpan(const std::chrono::milliseconds value) noexcept
        {
            return winrt::Windows::Foundation::TimeSpan{ value };
        }

        static WU::Color _color(const uint32_t argb) noexcept
        {
            return WU::ColorHelper::FromArgb(
                static_cast<uint8_t>((argb >> 24u) & 0xffu),
                static_cast<uint8_t>((argb >> 16u) & 0xffu),
                static_cast<uint8_t>((argb >> 8u) & 0xffu),
                static_cast<uint8_t>(argb & 0xffu));
        }

        static WU::Color _withAlpha(WU::Color value, const uint8_t alpha) noexcept
        {
            value.A = alpha;
            return value;
        }

        bool _initialize(const WUXC::Grid& host) noexcept
        {
            _host = host;
            _environment.featureEnabled = true;
            _environment.rendererAvailable = true;

            try
            {
                _host.IsHitTestVisible(false);
                WUXA::AutomationProperties::SetAccessibilityView(_host, WUXAP::AccessibilityView::Raw);
                _initializeSolidFallback();
            }
            catch (...)
            {
                _renderState.Tier(RenderTier::Disabled);
                _faulted = true;
                return false;
            }

            _renderState.Tier(RenderTier::Solid);

            try
            {
                _uiSettings = WUVM::UISettings{};
                _accessibilitySettings = WUVM::AccessibilitySettings{};
            }
            catch (...)
            {
                // Static solid fallback does not depend on either object.
            }

            try
            {
                _initializeCompositionBase();
                _renderState.Tier(RenderTier::Solid);
            }
            catch (...)
            {
                _releaseCompositionHandles();
                _showFallback(true);
                return true;
            }

            try
            {
                _initializeGradientStage();
                _renderState.Tier(RenderTier::StaticGradient);
            }
            catch (...)
            {
                _renderState.Tier(RenderTier::Solid);
                _detachCompositionForFallback();
                return true;
            }

            try
            {
                _initializeHeadStage();
                _renderState.Tier(RenderTier::NoSparks);
            }
            catch (...)
            {
                _renderState.Tier(RenderTier::StaticGradient);
                return true;
            }

            try
            {
                _initializeSparkStage();
                _renderState.Tier(RenderTier::Full);
            }
            catch (...)
            {
                _releaseAllSparks();
                _renderState.Tier(RenderTier::NoSparks);
            }

            _updateGeometry();
            _showFallback(false);
            return true;
        }

        void _initializeSolidFallback()
        {
            _fallbackTrack = WUXC::Border{};
            _fallbackFill = WUXC::Border{};
            _fallbackTrackBrush = WUXM::SolidColorBrush{ _trackColor };
            _fallbackFillBrush = WUXM::SolidColorBrush{ _runningColor };

            const auto radius = WUX::CornerRadiusHelper::FromUniformRadius(RainbowArcVisualConstants::TrackCornerRadius);
            const auto margin = WUX::ThicknessHelper::FromLengths(
                RainbowArcVisualConstants::HorizontalInset,
                0.0,
                RainbowArcVisualConstants::HorizontalInset,
                RainbowArcVisualConstants::BottomInset);

            _fallbackTrack.Height(RainbowArcVisualConstants::TrackHeight);
            _fallbackTrack.Margin(margin);
            _fallbackTrack.VerticalAlignment(WUX::VerticalAlignment::Bottom);
            _fallbackTrack.HorizontalAlignment(WUX::HorizontalAlignment::Stretch);
            _fallbackTrack.CornerRadius(radius);
            _fallbackTrack.IsHitTestVisible(false);
            _fallbackTrack.Background(_fallbackTrackBrush);

            _fallbackFill.Height(RainbowArcVisualConstants::TrackHeight);
            _fallbackFill.Margin(margin);
            _fallbackFill.VerticalAlignment(WUX::VerticalAlignment::Bottom);
            _fallbackFill.HorizontalAlignment(WUX::HorizontalAlignment::Left);
            _fallbackFill.CornerRadius(radius);
            _fallbackFill.IsHitTestVisible(false);
            _fallbackFill.Width(0.0);
            _fallbackFill.Background(_fallbackFillBrush);

            _host.Children().Append(_fallbackTrack);
            _host.Children().Append(_fallbackFill);
            _showFallback(true);
        }

        void _initializeCompositionBase()
        {
            const auto elementVisual = WUXH::ElementCompositionPreview::GetElementVisual(_host);
            _compositor = elementVisual.Compositor();
            _root = _compositor.CreateContainerVisual();

            _trackBrush = _compositor.CreateColorBrush(_color(RainbowArcVisualConstants::DarkTrack));
            _statusBrush = _compositor.CreateColorBrush(_color(RainbowArcVisualConstants::RunningSolid));

            _trackGeometry = _compositor.CreateRoundedRectangleGeometry();
            _trackShape = _compositor.CreateSpriteShape(_trackGeometry);
            _trackShape.FillBrush(_trackBrush);
            _trackVisual = _compositor.CreateShapeVisual();
            _trackVisual.Shapes().Append(_trackShape);

            _solidFillGeometry = _compositor.CreateRoundedRectangleGeometry();
            _solidFillShape = _compositor.CreateSpriteShape(_solidFillGeometry);
            _solidFillShape.FillBrush(_statusBrush);
            _solidFillVisual = _compositor.CreateShapeVisual();
            _solidFillVisual.Shapes().Append(_solidFillShape);

            _fillBoundary = _compositor.CreateContainerVisual();
            _fillInsetClip = _compositor.CreateInsetClip();
            _fillBoundary.Clip(_fillInsetClip);
            _fillBoundary.Children().InsertAtTop(_solidFillVisual);

            _root.Children().InsertAtTop(_trackVisual);
            _root.Children().InsertAtTop(_fillBoundary);
            WUXH::ElementCompositionPreview::SetElementChildVisual(_host, _root);
        }

        void _initializeGradientStage()
        {
            _rainbowBrush = _createRainbowBrush();
            _rainbowFillGeometry = _compositor.CreateRoundedRectangleGeometry();
            _rainbowFillShape = _compositor.CreateSpriteShape(_rainbowFillGeometry);
            _rainbowFillShape.FillBrush(_rainbowBrush);
            _rainbowFillVisual = _compositor.CreateShapeVisual();
            _rainbowFillVisual.Shapes().Append(_rainbowFillShape);
            _fillBoundary.Children().InsertAtTop(_rainbowFillVisual);

            _cometBrush = _createCometBrush();
            _cometSolidBrush = _compositor.CreateColorBrush(_runningColor);
            _cometTail = _compositor.CreateSpriteVisual();
            _cometTail.Brush(_cometBrush);
            _cometContainer = _compositor.CreateContainerVisual();
            _cometClipGeometry = _compositor.CreateRoundedRectangleGeometry();
            _cometClip = _compositor.CreateGeometricClip(_cometClipGeometry);
            _cometContainer.Clip(_cometClip);
            _cometContainer.Children().InsertAtTop(_cometTail);
            _cometContainer.IsVisible(false);
            _root.Children().InsertAtTop(_cometContainer);

            _successSweep = _compositor.CreateSpriteVisual();
            _successSweep.Brush(_compositor.CreateColorBrush(_withAlpha(_color(RainbowArcVisualConstants::WhiteHot), 215)));
            _successSweep.Opacity(0.0f);
            _fillBoundary.Children().InsertAtTop(_successSweep);

            _rainbowMovementAnimation = _compositor.CreateVector2KeyFrameAnimation();
            _rainbowMovementAnimation.InsertKeyFrame(0.0f, { 0.0f, 0.0f });
            _rainbowMovementAnimation.InsertKeyFrame(1.0f, { 1.0f, 0.0f });
            _rainbowMovementAnimation.Duration(_timeSpan(RainbowArcVisualConstants::RainbowCycleDuration));
            _rainbowMovementAnimation.IterationBehavior(WUC::AnimationIterationBehavior::Forever);

            _fillProgressAnimation = _compositor.CreateScalarKeyFrameAnimation();
            _headProgressAnimation = _compositor.CreateVector3KeyFrameAnimation();
            _cometTailAnimation = _compositor.CreateVector3KeyFrameAnimation();
            _cometHeadAnimation = _compositor.CreateVector3KeyFrameAnimation();
            _cometHeadOpacityAnimation = _compositor.CreateScalarKeyFrameAnimation();
            _successSweepMovementAnimation = _compositor.CreateVector3KeyFrameAnimation();
            _successSweepOpacityAnimation = _compositor.CreateScalarKeyFrameAnimation();
            _successTintAnimation = _compositor.CreateScalarKeyFrameAnimation();
        }

        void _initializeHeadStage()
        {
            _headRoot = _compositor.CreateContainerVisual();
            _headRoot.IsVisible(false);

            _outerBloomBrush = _createRadialBrush(_withAlpha(_color(RainbowArcVisualConstants::RainbowMagenta), 100), 0);
            _innerGlowBrush = _createRadialBrush(_withAlpha(_color(RainbowArcVisualConstants::RainbowCyan), 205), 0);
            _headTrailBrush = _createTrailBrush();
            _errorOuterBloomBrush = _createRadialBrush(_withAlpha(_color(RainbowArcVisualConstants::ErrorSolid), 115), 0);
            _errorInnerGlowBrush = _createRadialBrush(_withAlpha(_color(RainbowArcVisualConstants::ErrorSolid), 220), 0);
            _errorTrailBrush = _createStatusTrailBrush(_color(RainbowArcVisualConstants::ErrorSolid));

            _outerBloom = _compositor.CreateSpriteVisual();
            _outerBloom.Brush(_outerBloomBrush);
            _outerBloom.Size({ RainbowArcVisualConstants::OuterBloomWidth, RainbowArcVisualConstants::OuterBloomHeight });
            _outerBloom.Offset({ -RainbowArcVisualConstants::OuterBloomWidth / 2.0f, -RainbowArcVisualConstants::OuterBloomHeight / 2.0f, 0.0f });

            _innerGlow = _compositor.CreateSpriteVisual();
            _innerGlow.Brush(_innerGlowBrush);
            _innerGlow.Size({ RainbowArcVisualConstants::InnerGlowWidth, RainbowArcVisualConstants::InnerGlowHeight });
            _innerGlow.Offset({ -RainbowArcVisualConstants::InnerGlowWidth / 2.0f, -RainbowArcVisualConstants::InnerGlowHeight / 2.0f, 0.0f });

            _headTrail = _compositor.CreateSpriteVisual();
            _headTrail.Brush(_headTrailBrush);
            _headTrail.Size({ RainbowArcVisualConstants::HeadTrailWidth, RainbowArcVisualConstants::TrackHeight });
            _headTrail.Offset({ -RainbowArcVisualConstants::HeadTrailWidth, -RainbowArcVisualConstants::TrackHeight / 2.0f, 0.0f });

            _warmCore = _compositor.CreateSpriteVisual();
            _warmCore.Brush(_compositor.CreateColorBrush(_color(RainbowArcVisualConstants::WarmWhite)));
            _warmCore.Size({ RainbowArcVisualConstants::WarmCoreWidth, RainbowArcVisualConstants::TrackHeight });
            _warmCore.Offset({ -RainbowArcVisualConstants::WarmCoreWidth / 2.0f, -RainbowArcVisualConstants::TrackHeight / 2.0f, 0.0f });

            _whiteCore = _compositor.CreateSpriteVisual();
            _whiteCore.Brush(_compositor.CreateColorBrush(_color(RainbowArcVisualConstants::WhiteHot)));
            _whiteCore.Size({ RainbowArcVisualConstants::WhiteCoreWidth, RainbowArcVisualConstants::TrackHeight });
            _whiteCore.Offset({ -RainbowArcVisualConstants::WhiteCoreWidth / 2.0f, -RainbowArcVisualConstants::TrackHeight / 2.0f, 0.0f });

            _headRoot.Children().InsertAtTop(_outerBloom);
            _headRoot.Children().InsertAtTop(_innerGlow);
            _headRoot.Children().InsertAtTop(_headTrail);
            _headRoot.Children().InsertAtTop(_warmCore);
            _headRoot.Children().InsertAtTop(_whiteCore);
            _root.Children().InsertAtTop(_headRoot);

            _waitingBreatheAnimation = _compositor.CreateScalarKeyFrameAnimation();
            _waitingBreatheAnimation.InsertKeyFrame(0.0f, 0.55f);
            _waitingBreatheAnimation.InsertKeyFrame(0.5f, 1.0f);
            _waitingBreatheAnimation.InsertKeyFrame(1.0f, 0.55f);
            _waitingBreatheAnimation.Duration(_timeSpan(RainbowArcVisualConstants::WaitingBreatheDuration));
            _waitingBreatheAnimation.IterationBehavior(WUC::AnimationIterationBehavior::Forever);

            _regressionDipAnimation = _compositor.CreateScalarKeyFrameAnimation();
            _regressionDipAnimation.InsertKeyFrame(0.0f, 1.0f);
            _regressionDipAnimation.InsertKeyFrame(0.45f, 0.32f);
            _regressionDipAnimation.InsertKeyFrame(1.0f, 1.0f);
            _regressionDipAnimation.Duration(_timeSpan(RainbowArcVisualConstants::RegressionOpacityDipDuration));

            _errorPulseAnimation = _compositor.CreateScalarKeyFrameAnimation();
            _errorPulseAnimation.InsertKeyFrame(0.0f, 1.0f);
            _errorPulseAnimation.InsertKeyFrame(0.45f, 0.42f);
            _errorPulseAnimation.InsertKeyFrame(1.0f, 1.0f);
            _errorPulseAnimation.Duration(_timeSpan(RainbowArcVisualConstants::ErrorPulseDuration));

            _successHeadScaleAnimation = _compositor.CreateVector3KeyFrameAnimation();
            _successHeadScaleAnimation.InsertKeyFrame(0.0f, { 1.0f, 1.0f, 1.0f });
            _successHeadScaleAnimation.InsertKeyFrame(0.55f, { 1.32f, 1.32f, 1.0f });
            _successHeadScaleAnimation.InsertKeyFrame(1.0f, { 1.0f, 1.0f, 1.0f });
            _successHeadScaleAnimation.Duration(_timeSpan(RainbowArcVisualConstants::SuccessIntensifyDuration));

            _terminalFadeAnimation = _compositor.CreateScalarKeyFrameAnimation();
        }

        void _initializeSparkStage()
        {
            for (std::size_t index = 0; index < _sparks.size(); ++index)
            {
                auto& spark = _sparks[index];
                spark.brush = _compositor.CreateColorBrush(_color(RainbowArcVisualConstants::WhiteHot));
                spark.visual = _compositor.CreateSpriteVisual();
                spark.visual.Brush(spark.brush);
                const auto size = index + 1 == _sparks.size() ?
                                      RainbowArcVisualConstants::MaximumSparkSize :
                                      (index % 3 == 0 ? 2.0f : RainbowArcVisualConstants::TypicalSparkSize);
                spark.visual.Size({ size, size });
                spark.visual.Opacity(0.0f);
                _headRoot.Children().InsertAtTop(spark.visual);

                spark.ambientMovement = _compositor.CreateVector3KeyFrameAnimation();
                spark.ambientOpacity = _compositor.CreateScalarKeyFrameAnimation();
                spark.ambientColor = _compositor.CreateColorKeyFrameAnimation();
                spark.burstMovement = _compositor.CreateVector3KeyFrameAnimation();
                spark.burstOpacity = _compositor.CreateScalarKeyFrameAnimation();
                spark.burstColor = _compositor.CreateColorKeyFrameAnimation();

                spark.ambientOpacity.InsertKeyFrame(0.0f, 0.0f);
                spark.ambientOpacity.InsertKeyFrame(0.01f, 1.0f);
                spark.ambientOpacity.InsertKeyFrame(0.13f, 0.0f);
                spark.ambientOpacity.InsertKeyFrame(1.0f, 0.0f);
                spark.ambientColor.InsertKeyFrame(0.0f, _color(RainbowArcVisualConstants::WhiteHot));
                spark.ambientColor.InsertKeyFrame(0.04f, _color(RainbowArcVisualConstants::SparkYellow));
                spark.ambientColor.InsertKeyFrame(0.09f, _color(RainbowArcVisualConstants::SparkOrange));
                spark.ambientColor.InsertKeyFrame(0.13f, _withAlpha(_color(RainbowArcVisualConstants::SparkOrange), 0));
                spark.ambientColor.InsertKeyFrame(1.0f, _withAlpha(_color(RainbowArcVisualConstants::SparkOrange), 0));

                spark.burstOpacity.InsertKeyFrame(0.0f, 1.0f);
                spark.burstOpacity.InsertKeyFrame(0.72f, 0.72f);
                spark.burstOpacity.InsertKeyFrame(1.0f, 0.0f);
                spark.burstColor.InsertKeyFrame(0.0f, _color(RainbowArcVisualConstants::WhiteHot));
                spark.burstColor.InsertKeyFrame(0.32f, _color(RainbowArcVisualConstants::SparkYellow));
                spark.burstColor.InsertKeyFrame(0.72f, _color(RainbowArcVisualConstants::SparkOrange));
                spark.burstColor.InsertKeyFrame(1.0f, _withAlpha(_color(RainbowArcVisualConstants::SparkOrange), 0));
            }
        }

        WUC::CompositionLinearGradientBrush _createRainbowBrush()
        {
            auto brush = _compositor.CreateLinearGradientBrush();
            brush.MappingMode(WUC::CompositionMappingMode::Relative);
            brush.ExtendMode(WUC::CompositionGradientExtendMode::Wrap);
            brush.StartPoint({ 0.0f, 0.5f });
            brush.EndPoint({ 1.0f, 0.5f });
            brush.Scale({ 0.5f, 1.0f });
            const std::array<uint32_t, RainbowArcVisualConstants::RainbowStopCount> colors{
                RainbowArcVisualConstants::RainbowRed,
                RainbowArcVisualConstants::RainbowOrange,
                RainbowArcVisualConstants::RainbowYellow,
                RainbowArcVisualConstants::RainbowGreen,
                RainbowArcVisualConstants::RainbowCyan,
                RainbowArcVisualConstants::RainbowBlue,
                RainbowArcVisualConstants::RainbowViolet,
                RainbowArcVisualConstants::RainbowMagenta,
                RainbowArcVisualConstants::RainbowRed,
            };
            for (std::size_t index = 0; index < colors.size(); ++index)
            {
                const auto offset = static_cast<float>(index) / static_cast<float>(colors.size() - 1);
                brush.ColorStops().Append(_compositor.CreateColorGradientStop(offset, _color(colors[index])));
            }
            return brush;
        }

        WUC::CompositionLinearGradientBrush _createCometBrush()
        {
            auto brush = _compositor.CreateLinearGradientBrush();
            brush.MappingMode(WUC::CompositionMappingMode::Relative);
            brush.StartPoint({ 0.0f, 0.5f });
            brush.EndPoint({ 1.0f, 0.5f });
            brush.ColorStops().Append(_compositor.CreateColorGradientStop(0.0f, _withAlpha(_color(RainbowArcVisualConstants::RainbowViolet), 0)));
            brush.ColorStops().Append(_compositor.CreateColorGradientStop(0.28f, _withAlpha(_color(RainbowArcVisualConstants::RainbowBlue), 80)));
            brush.ColorStops().Append(_compositor.CreateColorGradientStop(0.58f, _withAlpha(_color(RainbowArcVisualConstants::RainbowCyan), 180)));
            brush.ColorStops().Append(_compositor.CreateColorGradientStop(0.82f, _color(RainbowArcVisualConstants::RainbowYellow)));
            brush.ColorStops().Append(_compositor.CreateColorGradientStop(1.0f, _color(RainbowArcVisualConstants::RainbowMagenta)));
            return brush;
        }

        WUC::CompositionLinearGradientBrush _createTrailBrush()
        {
            auto brush = _compositor.CreateLinearGradientBrush();
            brush.MappingMode(WUC::CompositionMappingMode::Relative);
            brush.StartPoint({ 0.0f, 0.5f });
            brush.EndPoint({ 1.0f, 0.5f });
            brush.ColorStops().Append(_compositor.CreateColorGradientStop(0.0f, _withAlpha(_color(RainbowArcVisualConstants::RainbowCyan), 0)));
            brush.ColorStops().Append(_compositor.CreateColorGradientStop(0.62f, _withAlpha(_color(RainbowArcVisualConstants::RainbowCyan), 95)));
            brush.ColorStops().Append(_compositor.CreateColorGradientStop(1.0f, _withAlpha(_color(RainbowArcVisualConstants::WarmWhite), 220)));
            return brush;
        }

        WUC::CompositionLinearGradientBrush _createStatusTrailBrush(const WU::Color color)
        {
            auto brush = _compositor.CreateLinearGradientBrush();
            brush.MappingMode(WUC::CompositionMappingMode::Relative);
            brush.StartPoint({ 0.0f, 0.5f });
            brush.EndPoint({ 1.0f, 0.5f });
            brush.ColorStops().Append(_compositor.CreateColorGradientStop(0.0f, _withAlpha(color, 0)));
            brush.ColorStops().Append(_compositor.CreateColorGradientStop(0.65f, _withAlpha(color, 110)));
            brush.ColorStops().Append(_compositor.CreateColorGradientStop(1.0f, _withAlpha(color, 230)));
            return brush;
        }

        WUC::CompositionRadialGradientBrush _createRadialBrush(const WU::Color center, const uint8_t edgeAlpha)
        {
            auto brush = _compositor.CreateRadialGradientBrush();
            brush.MappingMode(WUC::CompositionMappingMode::Relative);
            brush.EllipseCenter({ 0.5f, 0.5f });
            brush.EllipseRadius({ 0.5f, 0.5f });
            brush.ColorStops().Append(_compositor.CreateColorGradientStop(0.0f, center));
            brush.ColorStops().Append(_compositor.CreateColorGradientStop(0.38f, _withAlpha(center, static_cast<uint8_t>(center.A / 2u))));
            brush.ColorStops().Append(_compositor.CreateColorGradientStop(1.0f, _withAlpha(center, edgeAlpha)));
            return brush;
        }

        void _subscribeEvents()
        {
            const auto weak = weak_from_this();
            _loadedToken = _host.Loaded([weak](auto&&, auto&&) {
                if (const auto self = weak.lock())
                {
                    self->_environment.hostLoaded = true;
                    self->_environment.tabVisible = true;
                    self->_updateGeometry();
                    self->RefreshEnvironment();
                }
            });
            _loadedSubscribed = true;

            _unloadedToken = _host.Unloaded([weak](auto&&, auto&&) {
                if (const auto self = weak.lock())
                {
                    self->_environment.hostLoaded = false;
                    self->_environment.tabVisible = false;
                    self->_pauseForIneligibleHost();
                }
            });
            _unloadedSubscribed = true;

            _sizeChangedToken = _host.SizeChanged([weak](auto&&, auto&&) {
                if (const auto self = weak.lock())
                {
                    self->_updateGeometry();
                    self->RefreshEnvironment();
                }
            });
            _sizeChangedSubscribed = true;

            try
            {
                _themeChangedToken = _host.ActualThemeChanged([weak](auto&&, auto&&) {
                    if (const auto self = weak.lock())
                    {
                        self->RefreshEnvironment();
                    }
                });
                _themeChangedSubscribed = true;
            }
            catch (...)
            {
            }

            try
            {
                _coreWindow = WUCore::CoreWindow::GetForCurrentThread();
                if (_coreWindow)
                {
                    _environment.windowVisible = _coreWindow.Visible();
                    _coreActivatedToken = _coreWindow.Activated([weak](auto&&, const WUCore::WindowActivatedEventArgs& args) {
                        if (const auto self = weak.lock())
                        {
                            self->_environment.windowFocused = args.WindowActivationState() != WUCore::CoreWindowActivationState::Deactivated;
                            self->RefreshEnvironment();
                        }
                    });
                    _coreActivatedSubscribed = true;

                    _coreVisibilityToken = _coreWindow.VisibilityChanged([weak](auto&&, const WUCore::VisibilityChangedEventArgs& args) {
                        if (const auto self = weak.lock())
                        {
                            self->_environment.windowVisible = args.Visible();
                            self->RefreshEnvironment();
                        }
                    });
                    _coreVisibilitySubscribed = true;
                }
            }
            catch (...)
            {
            }

            try
            {
                if (_accessibilitySettings)
                {
                    _highContrastToken = _accessibilitySettings.HighContrastChanged([weak](auto&&, auto&&) {
                        if (const auto self = weak.lock())
                        {
                            self->RefreshEnvironment();
                        }
                    });
                    _highContrastSubscribed = true;
                }
            }
            catch (...)
            {
            }

            // AnimationsEnabledChanged is newer than the base UISettings API.
            // Registration is opportunistic; every Apply/Refresh also queries
            // AnimationsEnabled, so an unavailable event remains fail-open.
            try
            {
                if (_uiSettings)
                {
                    _animationsToken = _uiSettings.AnimationsEnabledChanged([weak](auto&&, auto&&) {
                        if (const auto self = weak.lock())
                        {
                            self->RefreshEnvironment();
                        }
                    });
                    _animationsSubscribed = true;
                }
            }
            catch (...)
            {
            }
        }

        void _unsubscribeEvents() noexcept
        {
            try
            {
                if (_loadedSubscribed && _host)
                {
                    _host.Loaded(_loadedToken);
                }
                if (_unloadedSubscribed && _host)
                {
                    _host.Unloaded(_unloadedToken);
                }
                if (_sizeChangedSubscribed && _host)
                {
                    _host.SizeChanged(_sizeChangedToken);
                }
                if (_themeChangedSubscribed && _host)
                {
                    _host.ActualThemeChanged(_themeChangedToken);
                }
                if (_coreActivatedSubscribed && _coreWindow)
                {
                    _coreWindow.Activated(_coreActivatedToken);
                }
                if (_coreVisibilitySubscribed && _coreWindow)
                {
                    _coreWindow.VisibilityChanged(_coreVisibilityToken);
                }
                if (_highContrastSubscribed && _accessibilitySettings)
                {
                    _accessibilitySettings.HighContrastChanged(_highContrastToken);
                }
                if (_animationsSubscribed && _uiSettings)
                {
                    _uiSettings.AnimationsEnabledChanged(_animationsToken);
                }
            }
            catch (...)
            {
            }
            _loadedSubscribed = false;
            _unloadedSubscribed = false;
            _sizeChangedSubscribed = false;
            _themeChangedSubscribed = false;
            _coreActivatedSubscribed = false;
            _coreVisibilitySubscribed = false;
            _highContrastSubscribed = false;
            _animationsSubscribed = false;
        }

        void _refreshRuntimeEnvironment() noexcept
        {
            try
            {
                _environment.hostLoaded = _host && _host.IsLoaded();
                _environment.tabVisible = _environment.hostLoaded;
                // Host.Visibility is renderer-owned and becomes Collapsed for
                // Hidden snapshots. Treating it as an external pane signal
                // would prevent the next visible snapshot from reopening it.
                _environment.paneVisible = _environment.hostLoaded;
            }
            catch (...)
            {
                _environment.hostLoaded = false;
                _environment.tabVisible = false;
                _environment.paneVisible = false;
            }

            try
            {
                if (_uiSettings)
                {
                    _environment.animationsEnabled = _uiSettings.AnimationsEnabled();
                }
            }
            catch (...)
            {
                _environment.animationsEnabled = false;
            }

            try
            {
                if (_accessibilitySettings)
                {
                    _environment.highContrast = _accessibilitySettings.HighContrast();
                }
            }
            catch (...)
            {
                _environment.highContrast = true;
            }

            try
            {
                if (_coreWindow)
                {
                    _environment.windowVisible = _coreWindow.Visible();
                }
            }
            catch (...)
            {
            }

            _environment.rendererAvailable = _renderState.Tier() != RenderTier::Disabled;
            _updatePalette();
        }

        void _updatePalette() noexcept
        {
            try
            {
                WU::Color track;
                WU::Color running;
                WU::Color hot = _color(RainbowArcVisualConstants::WhiteHot);
                if (_environment.highContrast && _uiSettings)
                {
                    const auto foreground = _uiSettings.GetColorValue(WUVM::UIColorType::Foreground);
                    track = _withAlpha(foreground, 110);
                    running = foreground;
                    hot = foreground;
                }
                else
                {
                    const auto light = _isLightTheme();
                    track = _color(light ? RainbowArcVisualConstants::LightTrack : RainbowArcVisualConstants::DarkTrack);
                    running = _color(RainbowArcVisualConstants::RunningSolid);
                }

                _trackColor = track;
                _runningColor = running;
                _hotColor = hot;
                if (_trackBrush)
                {
                    _trackBrush.Color(track);
                }
                if (_whiteCore && _whiteCore.Brush())
                {
                    _whiteCore.Brush().as<WUC::CompositionColorBrush>().Color(hot);
                }
                if (_fallbackTrack)
                {
                    _fallbackTrackBrush.Color(track);
                }
            }
            catch (...)
            {
            }
        }

        bool _isLightTheme() const noexcept
        {
            try
            {
                if (_host.ActualTheme() == WUX::ElementTheme::Light)
                {
                    return true;
                }
                if (_host.ActualTheme() == WUX::ElementTheme::Dark)
                {
                    return false;
                }
                if (_uiSettings)
                {
                    const auto background = _uiSettings.GetColorValue(WUVM::UIColorType::Background);
                    return (static_cast<uint32_t>(background.R) * 299u +
                            static_cast<uint32_t>(background.G) * 587u +
                            static_cast<uint32_t>(background.B) * 114u) > 127500u;
                }
            }
            catch (...)
            {
            }
            return false;
        }

        void _updateGeometry() noexcept
        {
            try
            {
                const auto width = std::max(0.0f, static_cast<float>(_host.ActualWidth()));
                const auto height = std::max(0.0f, static_cast<float>(_host.ActualHeight()));
                _trackWidth = std::max(0.0f, width - (2.0f * RainbowArcVisualConstants::HorizontalInset));
                _trackY = std::max(0.0f, height - RainbowArcVisualConstants::BottomInset - RainbowArcVisualConstants::TrackHeight);
                _headY = _trackY + (RainbowArcVisualConstants::TrackHeight / 2.0f);
                _drawable = _trackWidth >= RainbowArcVisualConstants::MinimumDrawableTrackWidth && height >= RainbowArcVisualConstants::TrackHeight;

                if (_root)
                {
                    _root.Size({ width, height });
                    _trackVisual.Size({ _trackWidth, RainbowArcVisualConstants::TrackHeight });
                    _trackVisual.Offset({ RainbowArcVisualConstants::HorizontalInset, _trackY, 0.0f });
                    _trackGeometry.Size({ _trackWidth, RainbowArcVisualConstants::TrackHeight });
                    _trackGeometry.CornerRadius({ RainbowArcVisualConstants::TrackCornerRadius, RainbowArcVisualConstants::TrackCornerRadius });

                    _fillBoundary.Size({ _trackWidth, RainbowArcVisualConstants::TrackHeight });
                    _fillBoundary.Offset({ RainbowArcVisualConstants::HorizontalInset, _trackY, 0.0f });
                    _solidFillVisual.Size({ _trackWidth, RainbowArcVisualConstants::TrackHeight });
                    _solidFillGeometry.Size({ _trackWidth, RainbowArcVisualConstants::TrackHeight });
                    _solidFillGeometry.CornerRadius({ RainbowArcVisualConstants::TrackCornerRadius, RainbowArcVisualConstants::TrackCornerRadius });

                    if (_rainbowFillVisual)
                    {
                        _rainbowFillVisual.Size({ _trackWidth, RainbowArcVisualConstants::TrackHeight });
                        _rainbowFillGeometry.Size({ _trackWidth, RainbowArcVisualConstants::TrackHeight });
                        _rainbowFillGeometry.CornerRadius({ RainbowArcVisualConstants::TrackCornerRadius, RainbowArcVisualConstants::TrackCornerRadius });
                        _cometContainer.Size({ _trackWidth, RainbowArcVisualConstants::TrackHeight });
                        _cometContainer.Offset({ RainbowArcVisualConstants::HorizontalInset, _trackY, 0.0f });
                        _cometClipGeometry.Size({ _trackWidth, RainbowArcVisualConstants::TrackHeight });
                        _cometClipGeometry.CornerRadius({ RainbowArcVisualConstants::TrackCornerRadius, RainbowArcVisualConstants::TrackCornerRadius });
                        _cometTail.Size({ _trackWidth * RainbowArcVisualConstants::IndeterminateTailFraction, RainbowArcVisualConstants::TrackHeight });
                        _successSweep.Size({ std::max(2.0f, _trackWidth * RainbowArcVisualConstants::SuccessSweepWidthFraction), RainbowArcVisualConstants::TrackHeight });
                    }
                }

                _updateFallback(_renderState.CurrentProgress(_now()), _snapshot.mode, _snapshot.status);
                _showFallback(_renderState.Tier() == RenderTier::Solid ||
                              !_root ||
                              _environment.UsesStaticFallback());
            }
            catch (...)
            {
                _drawable = false;
            }
        }

        void _applyWithDegradation(RenderTransitionPlan plan, const bool semanticUpdate) noexcept
        {
            for (uint8_t attempt = 0; attempt < 5 && !_closed; ++attempt)
            {
                try
                {
                    _render(plan, semanticUpdate);
                    return;
                }
                catch (...)
                {
                    _stopAllAnimations();
                    _releaseAllSparks();
                    const auto next = NextLowerRenderTier(_renderState.Tier());
                    _renderState.Tier(next);
                    _environment.rendererAvailable = next != RenderTier::Disabled;
                    if (next == RenderTier::Solid)
                    {
                        _detachCompositionForFallback();
                    }
                    else if (next == RenderTier::Disabled)
                    {
                        _faulted = true;
                        _showFallback(false);
                        return;
                    }
                    plan = _renderState.RefreshEnvironment(_environment, _now());
                }
            }
        }

        void _render(const RenderTransitionPlan& plan, const bool semanticUpdate)
        {
            if (!_host || _renderState.Tier() == RenderTier::Disabled)
            {
                throw winrt::hresult_error{ winrt::hresult{ static_cast<int32_t>(0x80004005u) } };
            }

            // A terminal presentation owns its final hide. Theme, focus,
            // visibility, and accessibility refreshes must not replay the
            // retained success snapshot after that presentation has faded.
            // Only Apply, which represents a new semantic snapshot, clears
            // this latch.
            const auto terminalPresentationInFlight = _terminalBatchSubscribed && _activeTerminalBatchGeneration != 0;
            if (!semanticUpdate && (_terminalPresentationCompleted || terminalPresentationInFlight))
            {
                // A refresh in the middle of the bounded terminal animation
                // cannot safely rebuild its Composition batch. Complete it
                // immediately and latch the hidden state instead of cancelling
                // the fade and leaving a static success presentation behind.
                _terminalPresentationCompleted = true;
                _clearTerminalBatch();
                _pauseForIneligibleHost();
                return;
            }

            const auto terminalFade = !plan.visible &&
                                      plan.kind == RenderTransitionKind::Cancelled &&
                                      plan.fadeOut &&
                                      _root &&
                                      _renderState.Tier() < RenderTier::StaticGradient;
            const auto visible = (plan.visible || terminalFade) && _drawable;
            if (!visible)
            {
                _pauseForIneligibleHost();
                return;
            }

            if (_renderState.Tier() == RenderTier::Solid || !_root || _environment.UsesStaticFallback())
            {
                _showFallback(true);
                _updateFallback(plan.targetProgress, plan.mode, plan.status);
                _releaseAllSparks();
                return;
            }

            _showFallback(false);
            _root.IsVisible(true);
            _root.Opacity(1.0f);
            _clearTerminalBatch();
            _stopStatusAnimations();
            _setErrorHeadTreatment(false);

            if (terminalFade)
            {
                _renderCancelled(plan);
                return;
            }

            if (plan.staticFallback)
            {
                _renderStatic(plan);
                return;
            }

            switch (plan.status)
            {
            case ProgressStatus::Waiting:
                _renderWaiting(plan);
                break;
            case ProgressStatus::Success:
                _renderSuccess(plan);
                break;
            case ProgressStatus::Error:
                _renderError(plan);
                break;
            case ProgressStatus::Cancelled:
                _renderCancelled(plan);
                break;
            case ProgressStatus::Running:
            default:
                if (plan.mode == ProgressMode::Indeterminate)
                {
                    _renderIndeterminate(plan);
                }
                else
                {
                    _renderDeterminate(plan);
                }
                break;
            }

            if (plan.sparksEligible)
            {
                _startAmbientSparks();
                if (semanticUpdate && _snapshot.sequence != 0 && _snapshot.sequence != _lastBurstSequence && _snapshot.sequence % 13u == 0u)
                {
                    _lastBurstSequence = _snapshot.sequence;
                    _emitBurst(static_cast<uint8_t>(RainbowArcVisualConstants::StrongSparkBurstMinimum));
                }
            }
            else if (plan.status != ProgressStatus::Success)
            {
                _stopAmbientSparks();
            }
        }

        void _renderStatic(const RenderTransitionPlan& plan)
        {
            _stopContinuousAnimations();
            _releaseAllSparks();
            if (_renderState.Tier() == RenderTier::StaticGradient && !_environment.highContrast && plan.status == ProgressStatus::Running)
            {
                if (plan.mode == ProgressMode::Indeterminate)
                {
                    _showStaticComet();
                }
                else
                {
                    _showGradientFill(plan.targetProgress, false, plan.phaseReset, plan.duration);
                }
                return;
            }

            _showSolidFill(plan.targetProgress, plan.mode, _statusColor(plan.status));
        }

        void _renderDeterminate(const RenderTransitionPlan& plan)
        {
            _cometContainer.IsVisible(false);
            _showGradientFill(plan.targetProgress, plan.animateProgress, plan.phaseReset, plan.duration);
            _setRainbowMovement(plan.rainbowMoving);
        }

        void _renderIndeterminate(const RenderTransitionPlan& plan)
        {
            _fillBoundary.IsVisible(false);
            _cometContainer.IsVisible(true);
            _solidFillVisual.IsVisible(false);
            _rainbowFillVisual.IsVisible(false);
            _headRoot.IsVisible(true);
            _setRainbowMovement(false);

            if (!plan.indeterminateMoving)
            {
                _showStaticComet();
                return;
            }

            const auto tailWidth = _trackWidth * RainbowArcVisualConstants::IndeterminateTailFraction;
            const auto startX = -tailWidth;
            const auto endX = _trackWidth;
            _cometTail.Brush(_cometBrush);
            _cometTail.Offset({ startX, 0.0f, 0.0f });

            _cometTailAnimation.InsertKeyFrame(0.0f, { startX, 0.0f, 0.0f });
            _cometTailAnimation.InsertKeyFrame(1.0f, { endX, 0.0f, 0.0f });
            _cometTailAnimation.Duration(_timeSpan(RainbowArcVisualConstants::IndeterminateCycleDuration));
            _cometTailAnimation.IterationBehavior(WUC::AnimationIterationBehavior::Forever);

            _cometHeadAnimation.InsertKeyFrame(0.0f, { RainbowArcVisualConstants::HorizontalInset, _headY, 0.0f });
            _cometHeadAnimation.InsertKeyFrame(1.0f, { RainbowArcVisualConstants::HorizontalInset + _trackWidth + tailWidth, _headY, 0.0f });
            _cometHeadAnimation.Duration(_timeSpan(RainbowArcVisualConstants::IndeterminateCycleDuration));
            _cometHeadAnimation.IterationBehavior(WUC::AnimationIterationBehavior::Forever);

            _cometHeadOpacityAnimation.InsertKeyFrame(0.0f, 0.0f);
            _cometHeadOpacityAnimation.InsertKeyFrame(0.06f, 1.0f);
            _cometHeadOpacityAnimation.InsertKeyFrame(0.91f, 1.0f);
            _cometHeadOpacityAnimation.InsertKeyFrame(1.0f, 0.0f);
            _cometHeadOpacityAnimation.Duration(_timeSpan(RainbowArcVisualConstants::IndeterminateCycleDuration));
            _cometHeadOpacityAnimation.IterationBehavior(WUC::AnimationIterationBehavior::Forever);

            if (!_indeterminateRunning)
            {
                _cometTail.StartAnimation(L"Offset", _cometTailAnimation);
                _headRoot.StartAnimation(L"Offset", _cometHeadAnimation);
                _headRoot.StartAnimation(L"Opacity", _cometHeadOpacityAnimation);
                _indeterminateRunning = true;
            }
        }

        void _renderWaiting(const RenderTransitionPlan& plan)
        {
            _stopContinuousAnimations();
            _showSolidFill(plan.targetProgress, ProgressMode::Determinate, _statusColor(ProgressStatus::Waiting));
            if (plan.breathe && _headRoot && _headRoot.IsVisible())
            {
                _headRoot.StartAnimation(L"Opacity", _waitingBreatheAnimation);
                _waitingRunning = true;
            }
        }

        void _renderSuccess(const RenderTransitionPlan& plan)
        {
            _stopContinuousAnimations();
            _stopAmbientSparks();

            if (!plan.animateProgress && !plan.successSweep && !plan.finalSparkBurst && !plan.fadeOut)
            {
                _showSolidFill(plan.targetProgress, ProgressMode::Determinate, _statusColor(ProgressStatus::Success));
                return;
            }

            // Burst batches remain independent so the terminal presentation
            // batch contains only the bounded head/fill/sweep/fade sequence.
            if (plan.finalSparkBurst)
            {
                _emitBurst(static_cast<uint8_t>(RainbowArcVisualConstants::StrongSparkBurstMaximum));
            }

            _beginTerminalBatch();
            _showGradientFill(plan.targetProgress, plan.animateProgress, false, plan.duration);
            _statusBrush.Color(_statusColor(ProgressStatus::Success));
            _solidFillVisual.IsVisible(true);
            _solidFillVisual.Opacity(0.0f);
            _successTintAnimation.InsertKeyFrame(0.0f, 0.0f);
            _successTintAnimation.InsertKeyFrame(1.0f, 0.82f);
            _successTintAnimation.Duration(_timeSpan(RainbowArcVisualConstants::SuccessSweepDuration));
            _solidFillVisual.StartAnimation(L"Opacity", _successTintAnimation);

            if (_headRoot && _headRoot.IsVisible())
            {
                _headRoot.StartAnimation(L"Scale", _successHeadScaleAnimation);
            }

            if (plan.successSweep)
            {
                const auto sweepWidth = std::max(2.0f, _trackWidth * RainbowArcVisualConstants::SuccessSweepWidthFraction);
                _successSweepMovementAnimation.InsertKeyFrame(0.0f, { -sweepWidth, 0.0f, 0.0f });
                _successSweepMovementAnimation.InsertKeyFrame(1.0f, { _trackWidth, 0.0f, 0.0f });
                _successSweepMovementAnimation.Duration(_timeSpan(RainbowArcVisualConstants::SuccessSweepDuration));
                _successSweepOpacityAnimation.InsertKeyFrame(0.0f, 0.0f);
                _successSweepOpacityAnimation.InsertKeyFrame(0.18f, 0.92f);
                _successSweepOpacityAnimation.InsertKeyFrame(0.82f, 0.92f);
                _successSweepOpacityAnimation.InsertKeyFrame(1.0f, 0.0f);
                _successSweepOpacityAnimation.Duration(_timeSpan(RainbowArcVisualConstants::SuccessSweepDuration));
                _successSweep.StartAnimation(L"Offset", _successSweepMovementAnimation);
                _successSweep.StartAnimation(L"Opacity", _successSweepOpacityAnimation);
            }

            if (plan.fadeOut)
            {
                _terminalFadeAnimation.DelayTime(_timeSpan(RainbowArcVisualConstants::SuccessPresentationDuration));
                _terminalFadeAnimation.Duration(_timeSpan(RainbowArcVisualConstants::SuccessFadeDuration));
                _terminalFadeAnimation.InsertKeyFrame(0.0f, 1.0f);
                _terminalFadeAnimation.InsertKeyFrame(1.0f, 0.0f);
                _root.StartAnimation(L"Opacity", _terminalFadeAnimation);
            }
            _endTerminalBatch(true);
        }

        void _renderError(const RenderTransitionPlan& plan)
        {
            _stopContinuousAnimations();
            _releaseAllSparks();
            _showSolidFill(plan.targetProgress, ProgressMode::Determinate, _statusColor(ProgressStatus::Error));
            _setErrorHeadTreatment(true);
            if (plan.errorPulse && _headRoot && _headRoot.IsVisible())
            {
                _headRoot.StartAnimation(L"Opacity", _errorPulseAnimation);
            }
        }

        void _setErrorHeadTreatment(const bool enabled)
        {
            if (!_outerBloom || !_innerGlow || !_headTrail)
            {
                return;
            }
            _outerBloom.Brush(enabled ? _errorOuterBloomBrush : _outerBloomBrush);
            _innerGlow.Brush(enabled ? _errorInnerGlowBrush : _innerGlowBrush);
            _headTrail.Brush(enabled ? _errorTrailBrush : _headTrailBrush);
        }

        void _renderCancelled(const RenderTransitionPlan& plan)
        {
            _stopContinuousAnimations();
            _releaseAllSparks();
            if (!plan.fadeOut)
            {
                _hideImmediately();
                return;
            }

            _beginTerminalBatch();
            _terminalFadeAnimation.DelayTime(_timeSpan(std::chrono::milliseconds::zero()));
            _terminalFadeAnimation.Duration(_timeSpan(RainbowArcVisualConstants::CancelFadeDuration));
            _terminalFadeAnimation.InsertKeyFrame(0.0f, 1.0f);
            _terminalFadeAnimation.InsertKeyFrame(1.0f, 0.0f);
            _root.StartAnimation(L"Opacity", _terminalFadeAnimation);
            _endTerminalBatch(true);
        }

        void _showGradientFill(const float progress,
                               const bool animate,
                               const bool regression,
                               const std::chrono::milliseconds duration)
        {
            _fillBoundary.IsVisible(true);
            _cometContainer.IsVisible(false);
            _rainbowFillVisual.IsVisible(true);
            _rainbowFillVisual.Opacity(1.0f);
            _solidFillVisual.IsVisible(false);

            const auto target = std::clamp(progress, 0.0f, 1.0f);
            const auto current = std::clamp(_renderState.CurrentProgress(_now()), 0.0f, 1.0f);
            const auto targetInset = (1.0f - target) * _trackWidth;
            const auto currentInset = (1.0f - current) * _trackWidth;
            const auto targetHead = Numerics::float3{ RainbowArcVisualConstants::HorizontalInset + (target * _trackWidth), _headY, 0.0f };
            const auto currentHead = Numerics::float3{ RainbowArcVisualConstants::HorizontalInset + (current * _trackWidth), _headY, 0.0f };

            _fillInsetClip.StopAnimation(L"RightInset");
            _fillInsetClip.RightInset(targetInset);
            const auto canShowHead = _headRoot && _renderState.Tier() < RenderTier::StaticGradient;
            if (_headRoot)
            {
                _headRoot.IsVisible(canShowHead && target > 0.0f);
            }
            if (canShowHead)
            {
                _headRoot.StopAnimation(L"Offset");
                _headRoot.Offset(targetHead);
                _headRoot.Opacity(1.0f);
            }

            if (animate && duration > std::chrono::milliseconds::zero())
            {
                _fillProgressAnimation.InsertKeyFrame(0.0f, currentInset);
                _fillProgressAnimation.InsertKeyFrame(1.0f, targetInset);
                _fillProgressAnimation.Duration(_timeSpan(duration));
                _fillInsetClip.StartAnimation(L"RightInset", _fillProgressAnimation);
                if (canShowHead && target > 0.0f)
                {
                    _headProgressAnimation.InsertKeyFrame(0.0f, currentHead);
                    _headProgressAnimation.InsertKeyFrame(1.0f, targetHead);
                    _headProgressAnimation.Duration(_timeSpan(duration));
                    _headRoot.StartAnimation(L"Offset", _headProgressAnimation);
                }
            }

            if (regression && canShowHead && target > 0.0f)
            {
                _headRoot.StartAnimation(L"Opacity", _regressionDipAnimation);
            }
        }

        void _showSolidFill(const float progress, const ProgressMode mode, const WU::Color color)
        {
            _stopContinuousAnimations();
            _fillBoundary.IsVisible(mode != ProgressMode::Hidden);
            _cometContainer.IsVisible(false);
            _rainbowFillVisual.IsVisible(false);
            _solidFillVisual.IsVisible(mode != ProgressMode::Hidden);
            _solidFillVisual.Opacity(1.0f);
            _statusBrush.Color(color);

            if (mode == ProgressMode::Indeterminate)
            {
                _showStaticComet(color);
                return;
            }

            const auto target = std::clamp(progress, 0.0f, 1.0f);
            _fillInsetClip.RightInset((1.0f - target) * _trackWidth);
            if (_headRoot)
            {
                _headRoot.Offset({ RainbowArcVisualConstants::HorizontalInset + (target * _trackWidth), _headY, 0.0f });
                _headRoot.IsVisible(target > 0.0f && _renderState.Tier() < RenderTier::StaticGradient);
                _headRoot.Opacity(1.0f);
            }
        }

        void _showStaticComet()
        {
            _showStaticComet(_runningColor);
            if (_cometBrush && !_environment.highContrast)
            {
                _cometTail.Brush(_cometBrush);
            }
        }

        void _showStaticComet(const WU::Color color)
        {
            _stopContinuousAnimations();
            _fillBoundary.IsVisible(false);
            _cometContainer.IsVisible(true);
            const auto tailWidth = _trackWidth * RainbowArcVisualConstants::IndeterminateTailFraction;
            _cometTail.Size({ tailWidth, RainbowArcVisualConstants::TrackHeight });
            _cometTail.Offset({ (_trackWidth - tailWidth) / 2.0f, 0.0f, 0.0f });
            if (_environment.highContrast || _renderState.Tier() >= RenderTier::Solid)
            {
                _cometSolidBrush.Color(color);
                _cometTail.Brush(_cometSolidBrush);
            }
            if (_headRoot)
            {
                _headRoot.Offset({ RainbowArcVisualConstants::HorizontalInset + (_trackWidth / 2.0f) + (tailWidth / 2.0f), _headY, 0.0f });
                _headRoot.IsVisible(_renderState.Tier() < RenderTier::StaticGradient);
                _headRoot.Opacity(1.0f);
            }
        }

        void _setRainbowMovement(const bool enabled)
        {
            if (!_rainbowBrush)
            {
                return;
            }
            if (enabled && !_rainbowRunning)
            {
                _rainbowBrush.StartAnimation(L"Offset", _rainbowMovementAnimation);
                _rainbowRunning = true;
            }
            else if (!enabled && _rainbowRunning)
            {
                _rainbowBrush.StopAnimation(L"Offset");
                _rainbowRunning = false;
            }
        }

        void _startAmbientSparks()
        {
            if (_renderState.Tier() != RenderTier::Full || !_headRoot || _ambientSparksRunning)
            {
                return;
            }

            const auto now = _now();
            uint8_t started{};
            for (uint8_t ambientIndex = 0; ambientIndex < RainbowArcVisualConstants::AmbientSparkSlotCount; ++ambientIndex)
            {
                const auto handle = _sparkPool.Acquire(now, RainbowArcVisualConstants::MaximumSparkLifetime, true);
                if (!handle)
                {
                    break;
                }
                auto& spark = _sparks[handle->slot];
                spark.handle = handle;
                spark.ambient = true;
                _configureAmbientSpark(spark, handle->slot, ambientIndex);
                ++started;
            }
            _ambientSparksRunning = started != 0;
        }

        void _configureAmbientSpark(SparkVisual& spark, const uint8_t slot, const uint8_t ambientIndex)
        {
            const auto distance = RainbowArcVisualConstants::MinimumSparkTravel +
                                  static_cast<float>((slot * 5u + ambientIndex * 3u) % 11u);
            const auto vertical = ambientIndex % 2 == 0 ? -4.0f : 4.0f;
            const auto origin = Numerics::float3{ 0.0f, -spark.visual.Size().y / 2.0f, 0.0f };
            const auto end = Numerics::float3{ distance, vertical, 0.0f };
            const auto cycle = ambientIndex == 0 ?
                                   RainbowArcVisualConstants::AmbientSparkCycleOne :
                                   RainbowArcVisualConstants::AmbientSparkCycleTwo;
            const auto delay = RainbowArcVisualConstants::AmbientSparkStagger * ambientIndex;

            spark.ambientMovement.InsertKeyFrame(0.0f, origin);
            spark.ambientMovement.InsertKeyFrame(0.13f, end);
            spark.ambientMovement.InsertKeyFrame(1.0f, end);
            spark.ambientMovement.Duration(_timeSpan(cycle));
            spark.ambientMovement.DelayTime(_timeSpan(delay));
            spark.ambientMovement.IterationBehavior(WUC::AnimationIterationBehavior::Forever);
            spark.ambientOpacity.Duration(_timeSpan(cycle));
            spark.ambientOpacity.DelayTime(_timeSpan(delay));
            spark.ambientOpacity.IterationBehavior(WUC::AnimationIterationBehavior::Forever);
            spark.ambientColor.Duration(_timeSpan(cycle));
            spark.ambientColor.DelayTime(_timeSpan(delay));
            spark.ambientColor.IterationBehavior(WUC::AnimationIterationBehavior::Forever);
            spark.visual.StartAnimation(L"Offset", spark.ambientMovement);
            spark.visual.StartAnimation(L"Opacity", spark.ambientOpacity);
            spark.brush.StartAnimation(L"Color", spark.ambientColor);
        }

        void _stopAmbientSparks() noexcept
        {
            if (!_ambientSparksRunning)
            {
                return;
            }
            for (auto& spark : _sparks)
            {
                if (!spark.ambient || !spark.handle)
                {
                    continue;
                }
                _stopSparkAnimations(spark);
                _sparkPool.Release(*spark.handle);
                spark.handle.reset();
                spark.ambient = false;
            }
            _ambientSparksRunning = false;
        }

        void _emitBurst(const uint8_t requested)
        {
            if (_renderState.Tier() != RenderTier::Full || !_environment.AllowsContinuousAnimation() || !_environment.paneActive)
            {
                return;
            }

            const auto count = std::min<uint8_t>(requested, static_cast<uint8_t>(RainbowArcVisualConstants::StrongSparkBurstMaximum));
            const auto now = _now();
            for (uint8_t burstIndex = 0; burstIndex < count; ++burstIndex)
            {
                const auto lifetime = RainbowArcVisualConstants::MinimumSparkLifetime +
                                      std::chrono::milliseconds{ static_cast<int64_t>((burstIndex * 29u + _snapshot.sequence) % 141u) };
                const auto handle = _sparkPool.Acquire(now, lifetime, false);
                if (!handle)
                {
                    break;
                }
                auto& spark = _sparks[handle->slot];
                spark.handle = handle;
                spark.ambient = false;
                _configureBurstSpark(spark, *handle, burstIndex, lifetime);
            }
        }

        void _configureBurstSpark(SparkVisual& spark,
                                  const SparkHandle handle,
                                  const uint8_t burstIndex,
                                  const std::chrono::milliseconds lifetime)
        {
            _clearSparkBatch(spark);
            const auto distance = std::min(
                RainbowArcVisualConstants::MaximumSparkTravel,
                RainbowArcVisualConstants::MinimumSparkTravel + static_cast<float>((handle.slot * 7u + burstIndex * 3u) % 13u));
            constexpr std::array<float, 6> verticalPattern{ -6.0f, 4.5f, -2.5f, 6.0f, -4.0f, 2.0f };
            const auto vertical = verticalPattern[burstIndex % verticalPattern.size()];
            const auto origin = Numerics::float3{ 0.0f, -spark.visual.Size().y / 2.0f, 0.0f };
            const auto end = Numerics::float3{ distance, vertical, 0.0f };

            spark.burstMovement.InsertKeyFrame(0.0f, origin);
            spark.burstMovement.InsertKeyFrame(1.0f, end);
            spark.burstMovement.Duration(_timeSpan(lifetime));
            spark.burstMovement.DelayTime(_timeSpan(std::chrono::milliseconds{ burstIndex * 18 }));
            spark.burstMovement.IterationBehavior(WUC::AnimationIterationBehavior::Count);
            spark.burstMovement.IterationCount(1);
            spark.burstOpacity.Duration(_timeSpan(lifetime));
            spark.burstOpacity.DelayTime(_timeSpan(std::chrono::milliseconds{ burstIndex * 18 }));
            spark.burstOpacity.IterationBehavior(WUC::AnimationIterationBehavior::Count);
            spark.burstOpacity.IterationCount(1);
            spark.burstColor.Duration(_timeSpan(lifetime));
            spark.burstColor.DelayTime(_timeSpan(std::chrono::milliseconds{ burstIndex * 18 }));
            spark.burstColor.IterationBehavior(WUC::AnimationIterationBehavior::Count);
            spark.burstColor.IterationCount(1);

            spark.batch = _compositor.CreateScopedBatch(WUC::CompositionBatchTypes::Animation);
            spark.visual.StartAnimation(L"Offset", spark.burstMovement);
            spark.visual.StartAnimation(L"Opacity", spark.burstOpacity);
            spark.brush.StartAnimation(L"Color", spark.burstColor);
            spark.batch.End();

            const auto weak = weak_from_this();
            spark.completedToken = spark.batch.Completed([weak, handle](auto&&, auto&&) {
                if (const auto self = weak.lock())
                {
                    self->_completeSpark(handle);
                }
            });
            spark.completedSubscribed = true;
        }

        void _completeSpark(const SparkHandle handle) noexcept
        {
            if (!handle || handle.slot >= _sparks.size())
            {
                return;
            }
            auto& spark = _sparks[handle.slot];
            if (!spark.handle || spark.handle->generation != handle.generation)
            {
                return;
            }
            _stopSparkAnimations(spark);
            _sparkPool.Release(handle);
            spark.handle.reset();
            spark.ambient = false;
        }

        void _stopSparkAnimations(SparkVisual& spark) noexcept
        {
            try
            {
                if (spark.visual)
                {
                    spark.visual.StopAnimation(L"Offset");
                    spark.visual.StopAnimation(L"Opacity");
                    spark.visual.Opacity(0.0f);
                }
                if (spark.brush)
                {
                    spark.brush.StopAnimation(L"Color");
                }
            }
            catch (...)
            {
            }
        }

        void _clearSparkBatch(SparkVisual& spark) noexcept
        {
            try
            {
                if (spark.completedSubscribed && spark.batch)
                {
                    spark.batch.Completed(spark.completedToken);
                }
            }
            catch (...)
            {
            }
            spark.completedSubscribed = false;
            spark.batch = nullptr;
        }

        void _releaseAllSparks() noexcept
        {
            for (auto& spark : _sparks)
            {
                _stopSparkAnimations(spark);
                _clearSparkBatch(spark);
                spark.handle.reset();
                spark.ambient = false;
            }
            _sparkPool.ReleaseAll();
            _ambientSparksRunning = false;
        }

        void _beginTerminalBatch()
        {
            _clearTerminalBatch();
            _terminalBatch = _compositor.CreateScopedBatch(WUC::CompositionBatchTypes::Animation);
            _activeTerminalBatchGeneration = ++_nextTerminalBatchGeneration;
        }

        void _endTerminalBatch(const bool hideOnCompletion)
        {
            if (!_terminalBatch)
            {
                return;
            }
            _terminalBatch.End();
            const auto weak = weak_from_this();
            const auto presentationGeneration = _presentationGeneration;
            const auto terminalBatchGeneration = _activeTerminalBatchGeneration;
            _terminalBatchToken = _terminalBatch.Completed([weak, hideOnCompletion, presentationGeneration, terminalBatchGeneration](auto&&, auto&&) {
                if (const auto self = weak.lock())
                {
                    if (self->_presentationGeneration != presentationGeneration ||
                        self->_activeTerminalBatchGeneration != terminalBatchGeneration)
                    {
                        return;
                    }
                    self->_stopContinuousAnimations();
                    self->_releaseAllSparks();
                    if (hideOnCompletion && self->_root)
                    {
                        self->_terminalPresentationCompleted = true;
                        self->_root.IsVisible(false);
                    }
                }
            });
            _terminalBatchSubscribed = true;
        }

        void _clearTerminalBatch() noexcept
        {
            // Invalidate first so a queued callback from a replaced batch is
            // stale even when its replacement belongs to the same snapshot.
            _activeTerminalBatchGeneration = 0;
            try
            {
                if (_terminalBatchSubscribed && _terminalBatch)
                {
                    _terminalBatch.Completed(_terminalBatchToken);
                }
            }
            catch (...)
            {
            }
            _terminalBatchSubscribed = false;
            _terminalBatch = nullptr;
        }

        void _stopStatusAnimations() noexcept
        {
            try
            {
                if (_root)
                {
                    _root.StopAnimation(L"Opacity");
                    _root.Opacity(1.0f);
                }
                if (_headRoot)
                {
                    _headRoot.StopAnimation(L"Opacity");
                    _headRoot.StopAnimation(L"Scale");
                    _headRoot.Opacity(1.0f);
                    _headRoot.Scale({ 1.0f, 1.0f, 1.0f });
                }
                if (_solidFillVisual)
                {
                    _solidFillVisual.StopAnimation(L"Opacity");
                    _solidFillVisual.Opacity(1.0f);
                }
                if (_successSweep)
                {
                    _successSweep.StopAnimation(L"Offset");
                    _successSweep.StopAnimation(L"Opacity");
                    _successSweep.Opacity(0.0f);
                }
            }
            catch (...)
            {
            }
            _waitingRunning = false;
        }

        void _stopContinuousAnimations() noexcept
        {
            try
            {
                _setRainbowMovement(false);
                if (_cometTail)
                {
                    _cometTail.StopAnimation(L"Offset");
                }
                if (_headRoot)
                {
                    _headRoot.StopAnimation(L"Offset");
                    _headRoot.StopAnimation(L"Opacity");
                    _headRoot.Opacity(1.0f);
                }
            }
            catch (...)
            {
            }
            _indeterminateRunning = false;
            _waitingRunning = false;
        }

        void _stopAllAnimations() noexcept
        {
            _stopContinuousAnimations();
            _stopStatusAnimations();
            try
            {
                if (_fillInsetClip)
                {
                    _fillInsetClip.StopAnimation(L"RightInset");
                }
            }
            catch (...)
            {
            }
        }

        void _pauseForIneligibleHost() noexcept
        {
            _stopAllAnimations();
            _releaseAllSparks();
            _showFallback(false);
            try
            {
                if (_root)
                {
                    _root.IsVisible(false);
                }
            }
            catch (...)
            {
            }
        }

        void _hideImmediately() noexcept
        {
            _pauseForIneligibleHost();
        }

        void _showFallback(const bool visible) noexcept
        {
            try
            {
                if (_fallbackTrack)
                {
                    _fallbackTrack.Visibility(visible && _drawable ? WUX::Visibility::Visible : WUX::Visibility::Collapsed);
                }
                if (_fallbackFill)
                {
                    _fallbackFill.Visibility(visible && _drawable ? WUX::Visibility::Visible : WUX::Visibility::Collapsed);
                }
                if (_root)
                {
                    _root.IsVisible(!visible && _drawable);
                }
            }
            catch (...)
            {
            }
        }

        void _updateFallback(const float progress, const ProgressMode mode, const ProgressStatus status) noexcept
        {
            if (!_fallbackFill || !_fallbackTrack)
            {
                return;
            }
            try
            {
                auto width = std::clamp(progress, 0.0f, 1.0f) * _trackWidth;
                auto left = RainbowArcVisualConstants::HorizontalInset;
                if (mode == ProgressMode::Indeterminate)
                {
                    width = _trackWidth * RainbowArcVisualConstants::IndeterminateTailFraction;
                    left += (_trackWidth - width) / 2.0f;
                }
                else if (mode == ProgressMode::Hidden)
                {
                    width = 0.0f;
                }
                _fallbackFill.Width(width);
                _fallbackFill.Margin(WUX::ThicknessHelper::FromLengths(
                    left,
                    0.0,
                    0.0,
                    RainbowArcVisualConstants::BottomInset));
                _fallbackTrackBrush.Color(_trackColor);
                _fallbackFillBrush.Color(_statusColor(status));
                _fallbackFill.Visibility(width > 0.0f ? WUX::Visibility::Visible : WUX::Visibility::Collapsed);
            }
            catch (...)
            {
            }
        }

        WU::Color _statusColor(const ProgressStatus status) const noexcept
        {
            if (_environment.highContrast)
            {
                return _runningColor;
            }
            switch (status)
            {
            case ProgressStatus::Waiting:
                return _color(RainbowArcVisualConstants::WaitingSolid);
            case ProgressStatus::Success:
                return _color(RainbowArcVisualConstants::SuccessSolid);
            case ProgressStatus::Error:
                return _color(RainbowArcVisualConstants::ErrorSolid);
            case ProgressStatus::Running:
            case ProgressStatus::Cancelled:
            default:
                return _runningColor;
            }
        }

        void _detachCompositionForFallback() noexcept
        {
            try
            {
                if (_host)
                {
                    WUXH::ElementCompositionPreview::SetElementChildVisual(_host, WUC::Visual{ nullptr });
                }
            }
            catch (...)
            {
            }
            _releaseCompositionHandles();
            _showFallback(true);
        }

        void _removeFallbackVisuals() noexcept
        {
            try
            {
                uint32_t index{};
                if (_fallbackTrack && _host.Children().IndexOf(_fallbackTrack, index))
                {
                    _host.Children().RemoveAt(index);
                }
                if (_fallbackFill && _host.Children().IndexOf(_fallbackFill, index))
                {
                    _host.Children().RemoveAt(index);
                }
            }
            catch (...)
            {
            }
            _fallbackTrack = nullptr;
            _fallbackFill = nullptr;
            _fallbackTrackBrush = nullptr;
            _fallbackFillBrush = nullptr;
        }

        void _releaseCompositionHandles() noexcept
        {
            _clearTerminalBatch();
            _root = nullptr;
            _trackVisual = nullptr;
            _trackGeometry = nullptr;
            _trackShape = nullptr;
            _trackBrush = nullptr;
            _fillBoundary = nullptr;
            _fillInsetClip = nullptr;
            _solidFillVisual = nullptr;
            _solidFillGeometry = nullptr;
            _solidFillShape = nullptr;
            _statusBrush = nullptr;
            _rainbowFillVisual = nullptr;
            _rainbowFillGeometry = nullptr;
            _rainbowFillShape = nullptr;
            _rainbowBrush = nullptr;
            _cometContainer = nullptr;
            _cometClip = nullptr;
            _cometClipGeometry = nullptr;
            _cometTail = nullptr;
            _cometBrush = nullptr;
            _cometSolidBrush = nullptr;
            _successSweep = nullptr;
            _headRoot = nullptr;
            _outerBloom = nullptr;
            _outerBloomBrush = nullptr;
            _errorOuterBloomBrush = nullptr;
            _innerGlow = nullptr;
            _innerGlowBrush = nullptr;
            _errorInnerGlowBrush = nullptr;
            _headTrail = nullptr;
            _headTrailBrush = nullptr;
            _errorTrailBrush = nullptr;
            _warmCore = nullptr;
            _whiteCore = nullptr;
            _rainbowMovementAnimation = nullptr;
            _fillProgressAnimation = nullptr;
            _headProgressAnimation = nullptr;
            _cometTailAnimation = nullptr;
            _cometHeadAnimation = nullptr;
            _cometHeadOpacityAnimation = nullptr;
            _waitingBreatheAnimation = nullptr;
            _regressionDipAnimation = nullptr;
            _successSweepMovementAnimation = nullptr;
            _successSweepOpacityAnimation = nullptr;
            _successTintAnimation = nullptr;
            _errorPulseAnimation = nullptr;
            _successHeadScaleAnimation = nullptr;
            _terminalFadeAnimation = nullptr;
            for (auto& spark : _sparks)
            {
                _clearSparkBatch(spark);
                spark = {};
            }
            _compositor = nullptr;
        }

        inline static SparkBudget _sharedSparkBudget{};

        SparkPool _sparkPool;
        std::array<SparkVisual, RainbowArcVisualConstants::SparkPoolCapacityPerPane> _sparks{};
        VisualProgressRenderState _renderState{};
        RenderEnvironment _environment{};
        ProgressSnapshot _snapshot{};

        WUXC::Grid _host{ nullptr };
        WUXC::Border _fallbackTrack{ nullptr };
        WUXC::Border _fallbackFill{ nullptr };
        WUXM::SolidColorBrush _fallbackTrackBrush{ nullptr };
        WUXM::SolidColorBrush _fallbackFillBrush{ nullptr };

        WUC::Compositor _compositor{ nullptr };
        WUC::ContainerVisual _root{ nullptr };
        WUC::ShapeVisual _trackVisual{ nullptr };
        WUC::CompositionRoundedRectangleGeometry _trackGeometry{ nullptr };
        WUC::CompositionSpriteShape _trackShape{ nullptr };
        WUC::CompositionColorBrush _trackBrush{ nullptr };

        WUC::ContainerVisual _fillBoundary{ nullptr };
        WUC::InsetClip _fillInsetClip{ nullptr };
        WUC::ShapeVisual _solidFillVisual{ nullptr };
        WUC::CompositionRoundedRectangleGeometry _solidFillGeometry{ nullptr };
        WUC::CompositionSpriteShape _solidFillShape{ nullptr };
        WUC::CompositionColorBrush _statusBrush{ nullptr };
        WUC::ShapeVisual _rainbowFillVisual{ nullptr };
        WUC::CompositionRoundedRectangleGeometry _rainbowFillGeometry{ nullptr };
        WUC::CompositionSpriteShape _rainbowFillShape{ nullptr };
        WUC::CompositionLinearGradientBrush _rainbowBrush{ nullptr };

        WUC::ContainerVisual _cometContainer{ nullptr };
        WUC::CompositionGeometricClip _cometClip{ nullptr };
        WUC::CompositionRoundedRectangleGeometry _cometClipGeometry{ nullptr };
        WUC::SpriteVisual _cometTail{ nullptr };
        WUC::CompositionLinearGradientBrush _cometBrush{ nullptr };
        WUC::CompositionColorBrush _cometSolidBrush{ nullptr };
        WUC::SpriteVisual _successSweep{ nullptr };

        WUC::ContainerVisual _headRoot{ nullptr };
        WUC::SpriteVisual _outerBloom{ nullptr };
        WUC::CompositionRadialGradientBrush _outerBloomBrush{ nullptr };
        WUC::CompositionRadialGradientBrush _errorOuterBloomBrush{ nullptr };
        WUC::SpriteVisual _innerGlow{ nullptr };
        WUC::CompositionRadialGradientBrush _innerGlowBrush{ nullptr };
        WUC::CompositionRadialGradientBrush _errorInnerGlowBrush{ nullptr };
        WUC::SpriteVisual _headTrail{ nullptr };
        WUC::CompositionLinearGradientBrush _headTrailBrush{ nullptr };
        WUC::CompositionLinearGradientBrush _errorTrailBrush{ nullptr };
        WUC::SpriteVisual _warmCore{ nullptr };
        WUC::SpriteVisual _whiteCore{ nullptr };

        WUC::Vector2KeyFrameAnimation _rainbowMovementAnimation{ nullptr };
        WUC::ScalarKeyFrameAnimation _fillProgressAnimation{ nullptr };
        WUC::Vector3KeyFrameAnimation _headProgressAnimation{ nullptr };
        WUC::Vector3KeyFrameAnimation _cometTailAnimation{ nullptr };
        WUC::Vector3KeyFrameAnimation _cometHeadAnimation{ nullptr };
        WUC::ScalarKeyFrameAnimation _cometHeadOpacityAnimation{ nullptr };
        WUC::ScalarKeyFrameAnimation _waitingBreatheAnimation{ nullptr };
        WUC::ScalarKeyFrameAnimation _regressionDipAnimation{ nullptr };
        WUC::Vector3KeyFrameAnimation _successSweepMovementAnimation{ nullptr };
        WUC::ScalarKeyFrameAnimation _successSweepOpacityAnimation{ nullptr };
        WUC::ScalarKeyFrameAnimation _successTintAnimation{ nullptr };
        WUC::ScalarKeyFrameAnimation _errorPulseAnimation{ nullptr };
        WUC::Vector3KeyFrameAnimation _successHeadScaleAnimation{ nullptr };
        WUC::ScalarKeyFrameAnimation _terminalFadeAnimation{ nullptr };

        WUC::CompositionScopedBatch _terminalBatch{ nullptr };
        winrt::event_token _terminalBatchToken{};
        uint64_t _presentationGeneration{};
        uint64_t _nextTerminalBatchGeneration{};
        uint64_t _activeTerminalBatchGeneration{};
        bool _terminalBatchSubscribed{};
        bool _terminalPresentationCompleted{};

        WUVM::UISettings _uiSettings{ nullptr };
        WUVM::AccessibilitySettings _accessibilitySettings{ nullptr };
        WUCore::CoreWindow _coreWindow{ nullptr };

        winrt::event_token _loadedToken{};
        winrt::event_token _unloadedToken{};
        winrt::event_token _sizeChangedToken{};
        winrt::event_token _themeChangedToken{};
        winrt::event_token _coreActivatedToken{};
        winrt::event_token _coreVisibilityToken{};
        winrt::event_token _highContrastToken{};
        winrt::event_token _animationsToken{};
        bool _loadedSubscribed{};
        bool _unloadedSubscribed{};
        bool _sizeChangedSubscribed{};
        bool _themeChangedSubscribed{};
        bool _coreActivatedSubscribed{};
        bool _coreVisibilitySubscribed{};
        bool _highContrastSubscribed{};
        bool _animationsSubscribed{};

        WU::Color _trackColor{ _color(RainbowArcVisualConstants::DarkTrack) };
        WU::Color _runningColor{ _color(RainbowArcVisualConstants::RunningSolid) };
        WU::Color _hotColor{ _color(RainbowArcVisualConstants::WhiteHot) };
        uint64_t _lastBurstSequence{};
        float _trackWidth{};
        float _trackY{};
        float _headY{};
        bool _drawable{};
        bool _rainbowRunning{};
        bool _indeterminateRunning{};
        bool _waitingRunning{};
        bool _ambientSparksRunning{};
        bool _faulted{};
        bool _closed{};
    };
}
