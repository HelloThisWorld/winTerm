// Copyright (c) winTerm contributors.
// Licensed under the MIT license.

#include "pch.h"

#include "../TerminalSettingsModel/GlobalAppSettings.h"
#include "../../winterm/VisualProgress/VisualProgressModel.h"

using namespace WEX::TestExecution;
using namespace winTerm::VisualProgress;
using namespace winrt::Microsoft::Terminal::Settings::Model;

namespace SettingsModelUnitTests
{
    class WinTermVisualProgressTests
    {
        TEST_CLASS(WinTermVisualProgressTests);

        TEST_METHOD(MapEveryTaskbarState);
        TEST_METHOD(ClampDeterminateValues);
        TEST_METHOD(SuppressDuplicateState);
        TEST_METHOD(StandardProgressPrecedesShellLifecycle);
        TEST_METHOD(CommandCompletionClearsAtNextPrompt);
        TEST_METHOD(EmergencyOverridePrecedesSetting);
        TEST_METHOD(DisabledFeatureIgnoresEvents);
        TEST_METHOD(MultiplePanesRemainIndependent);
        TEST_METHOD(CloseAndDetachCleanupStopsUpdates);
        TEST_METHOD(SplitOrDetachResetClearsReusableState);
        TEST_METHOD(FeatureReloadDisablesAndReenablesCleanly);
        TEST_METHOD(MailboxCoalescesRapidUpdatesAndReleasesOnClose);
        TEST_METHOD(UnexpectedEventsFailOpen);
        TEST_METHOD(SettingSerializesAndMissingSettingDefaultsOff);
    };

    void WinTermVisualProgressTests::MapEveryTaskbarState()
    {
        ProgressStateMachine state;
        state.SetEnabled(true);

        auto snapshot = state.ApplyTaskbar(1, 50);
        VERIFY_IS_TRUE(snapshot.has_value());
        VERIFY_ARE_EQUAL(static_cast<int>(ProgressMode::Determinate), static_cast<int>(snapshot->mode));
        VERIFY_ARE_EQUAL(static_cast<int>(ProgressStatus::Running), static_cast<int>(snapshot->status));
        VERIFY_ARE_EQUAL(uint8_t{ 50 }, snapshot->value);

        snapshot = state.ApplyTaskbar(3, 0);
        VERIFY_ARE_EQUAL(static_cast<int>(ProgressMode::Indeterminate), static_cast<int>(snapshot->mode));

        snapshot = state.ApplyTaskbar(4, 27);
        VERIFY_ARE_EQUAL(static_cast<int>(ProgressStatus::Waiting), static_cast<int>(snapshot->status));
        VERIFY_ARE_EQUAL(uint8_t{ 27 }, snapshot->value);

        snapshot = state.ApplyTaskbar(2, 0);
        VERIFY_ARE_EQUAL(static_cast<int>(ProgressStatus::Error), static_cast<int>(snapshot->status));
        VERIFY_ARE_EQUAL(uint8_t{ 27 }, snapshot->value);

        snapshot = state.ApplyTaskbar(0, 0);
        VERIFY_IS_TRUE(snapshot.has_value());
        VERIFY_IS_FALSE(snapshot->visible);
        VERIFY_ARE_EQUAL(static_cast<int>(ProgressMode::Hidden), static_cast<int>(snapshot->mode));
    }

    void WinTermVisualProgressTests::ClampDeterminateValues()
    {
        for (const auto value : { uint64_t{ 0 }, uint64_t{ 1 }, uint64_t{ 50 }, uint64_t{ 99 }, uint64_t{ 100 }, uint64_t{ 101 }, uint64_t{ 1000 } })
        {
            ProgressStateMachine state;
            state.SetEnabled(true);
            const auto snapshot = state.ApplyTaskbar(1, value);
            VERIFY_IS_TRUE(snapshot.has_value());
            VERIFY_ARE_EQUAL(static_cast<uint8_t>(std::min<uint64_t>(value, 100)), snapshot->value);
        }
    }

    void WinTermVisualProgressTests::SuppressDuplicateState()
    {
        ProgressStateMachine state;
        state.SetEnabled(true);
        VERIFY_IS_TRUE(state.ApplyTaskbar(1, 50).has_value());
        VERIFY_IS_FALSE(state.ApplyTaskbar(1, 50).has_value());
        VERIFY_IS_TRUE(state.ApplyTaskbar(1, 51).has_value());
    }

    void WinTermVisualProgressTests::StandardProgressPrecedesShellLifecycle()
    {
        ProgressStateMachine state;
        state.SetEnabled(true);
        state.ApplyShellLifecycle(ShellLifecycleState::CommandStart, -1);
        const auto explicitProgress = state.ApplyTaskbar(1, 40);
        VERIFY_ARE_EQUAL(static_cast<int>(ProgressSource::Taskbar), static_cast<int>(explicitProgress->source));
        VERIFY_IS_FALSE(state.ApplyShellLifecycle(ShellLifecycleState::CommandFinished, 1).has_value());

        const auto fallback = state.ApplyTaskbar(0, 0);
        VERIFY_IS_TRUE(fallback.has_value());
        VERIFY_ARE_EQUAL(static_cast<int>(ProgressSource::ShellIntegration), static_cast<int>(fallback->source));
        VERIFY_ARE_EQUAL(static_cast<int>(ProgressStatus::Error), static_cast<int>(fallback->status));
    }

    void WinTermVisualProgressTests::CommandCompletionClearsAtNextPrompt()
    {
        ProgressStateMachine state;
        state.SetEnabled(true);
        auto snapshot = state.ApplyShellLifecycle(ShellLifecycleState::CommandStart, -1);
        VERIFY_ARE_EQUAL(static_cast<int>(ProgressMode::Indeterminate), static_cast<int>(snapshot->mode));

        snapshot = state.ApplyShellLifecycle(ShellLifecycleState::CommandFinished, 0);
        VERIFY_ARE_EQUAL(static_cast<int>(ProgressStatus::Success), static_cast<int>(snapshot->status));
        VERIFY_ARE_EQUAL(uint8_t{ 100 }, snapshot->value);

        snapshot = state.ApplyShellLifecycle(ShellLifecycleState::Prompt, -1);
        VERIFY_IS_FALSE(snapshot->visible);
    }

    void WinTermVisualProgressTests::EmergencyOverridePrecedesSetting()
    {
        VERIFY_IS_TRUE(IsFeatureEnabled(true, L""));
        VERIFY_IS_TRUE(IsFeatureEnabled(true, L"0"));
        VERIFY_IS_FALSE(IsFeatureEnabled(true, L"1"));
        VERIFY_IS_FALSE(IsFeatureEnabled(false, L""));
    }

    void WinTermVisualProgressTests::DisabledFeatureIgnoresEvents()
    {
        ProgressStateMachine state;
        VERIFY_IS_FALSE(state.ApplyTaskbar(1, 50).has_value());
        VERIFY_IS_FALSE(state.ApplyShellLifecycle(ShellLifecycleState::CommandStart, -1).has_value());
        VERIFY_IS_FALSE(state.Current().visible);
    }

    void WinTermVisualProgressTests::MultiplePanesRemainIndependent()
    {
        ProgressStateMachine first;
        ProgressStateMachine second;
        first.SetEnabled(true);
        second.SetEnabled(true);
        first.ApplyTaskbar(1, 25);
        second.ApplyTaskbar(1, 75);
        VERIFY_ARE_EQUAL(uint8_t{ 25 }, first.Current().value);
        VERIFY_ARE_EQUAL(uint8_t{ 75 }, second.Current().value);
    }

    void WinTermVisualProgressTests::CloseAndDetachCleanupStopsUpdates()
    {
        ProgressStateMachine state;
        state.SetEnabled(true);
        state.ApplyTaskbar(1, 25);
        const auto closed = state.Close();
        VERIFY_IS_TRUE(closed.has_value());
        VERIFY_IS_FALSE(closed->visible);
        VERIFY_ARE_EQUAL(static_cast<int>(ProgressStatus::Cancelled), static_cast<int>(closed->status));
        VERIFY_IS_FALSE(state.ApplyTaskbar(1, 75).has_value());
        VERIFY_IS_FALSE(state.ApplyShellLifecycle(ShellLifecycleState::CommandStart, -1).has_value());
    }

    void WinTermVisualProgressTests::SplitOrDetachResetClearsReusableState()
    {
        ProgressStateMachine state;
        state.SetEnabled(true);
        state.ApplyTaskbar(1, 25);

        const auto reset = state.Reset();
        VERIFY_IS_TRUE(reset.has_value());
        VERIFY_IS_FALSE(reset->visible);
        VERIFY_ARE_EQUAL(static_cast<int>(ProgressMode::Hidden), static_cast<int>(reset->mode));

        const auto reused = state.ApplyShellLifecycle(ShellLifecycleState::CommandStart, -1);
        VERIFY_IS_TRUE(reused.has_value());
        VERIFY_ARE_EQUAL(static_cast<int>(ProgressMode::Indeterminate), static_cast<int>(reused->mode));
    }

    void WinTermVisualProgressTests::FeatureReloadDisablesAndReenablesCleanly()
    {
        ProgressStateMachine state;
        state.SetEnabled(true);
        state.ApplyTaskbar(1, 50);

        const auto disabled = state.SetEnabled(false);
        VERIFY_IS_TRUE(disabled.has_value());
        VERIFY_IS_FALSE(disabled->visible);
        VERIFY_IS_FALSE(state.ApplyTaskbar(1, 75).has_value());

        state.SetEnabled(true);
        const auto reenabled = state.ApplyTaskbar(1, 75);
        VERIFY_IS_TRUE(reenabled.has_value());
        VERIFY_ARE_EQUAL(uint8_t{ 75 }, reenabled->value);
    }

    void WinTermVisualProgressTests::MailboxCoalescesRapidUpdatesAndReleasesOnClose()
    {
        ProgressUpdateMailbox mailbox;
        bool allPublished = true;
        for (uint64_t value = 0; value < 10000; ++value)
        {
            allPublished = mailbox.Publish({ ProgressMode::Determinate, ProgressStatus::Running, static_cast<uint8_t>(value % 101), true, ProgressSource::Taskbar, value }) && allPublished;
        }
        VERIFY_IS_TRUE(allPublished);
        const auto latest = mailbox.TakeLatest();
        VERIFY_IS_TRUE(latest.has_value());
        VERIFY_ARE_EQUAL(uint64_t{ 9999 }, latest->sequence);
        VERIFY_IS_FALSE(mailbox.HasPending());

        mailbox.Close();
        VERIFY_IS_FALSE(mailbox.Publish({ ProgressMode::Indeterminate, ProgressStatus::Running, 0, true, ProgressSource::Taskbar, 10000 }));
        VERIFY_IS_FALSE(mailbox.TakeLatest().has_value());
    }

    void WinTermVisualProgressTests::UnexpectedEventsFailOpen()
    {
        ProgressStateMachine state;
        state.SetEnabled(true);
        VERIFY_IS_FALSE(state.ApplyTaskbar(99, UINT64_MAX).has_value());
        VERIFY_IS_FALSE(state.ApplyShellLifecycle(static_cast<ShellLifecycleState>(99), INT64_MAX).has_value());
        VERIFY_IS_FALSE(state.Current().visible);
    }

    void WinTermVisualProgressTests::SettingSerializesAndMissingSettingDefaultsOff()
    {
        Json::Value enabledJson{ Json::objectValue };
        enabledJson["visualProgress.enabled"] = true;
        const auto enabled = winrt::Microsoft::Terminal::Settings::Model::implementation::GlobalAppSettings::FromJson(enabledJson);
        VERIFY_IS_TRUE(enabled->VisualProgressEnabled());
        VERIFY_IS_TRUE(enabled->ToJson()["visualProgress.enabled"].asBool());

        Json::Value legacyJson{ Json::objectValue };
        const auto migrated = winrt::Microsoft::Terminal::Settings::Model::implementation::GlobalAppSettings::FromJson(legacyJson);
        VERIFY_IS_FALSE(migrated->VisualProgressEnabled());
        VERIFY_IS_FALSE(migrated->ToJson().isMember("visualProgress.enabled"));
    }
}
