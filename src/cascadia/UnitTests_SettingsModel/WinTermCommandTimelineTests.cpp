// Copyright (c) winTerm contributors.
// Licensed under the MIT license.

#include "pch.h"

#include "../TerminalSettingsModel/GlobalAppSettings.h"
#include "../../winterm/CommandTimeline/CommandTimelineModel.h"

using namespace WEX::TestExecution;
using namespace winTerm::CommandTimeline;
using namespace winrt::Microsoft::Terminal::Settings::Model;

namespace SettingsModelUnitTests
{
    class WinTermCommandTimelineTests
    {
        TEST_CLASS(WinTermCommandTimelineTests);

        TEST_METHOD(CommandTimelineSettingsUseStableDefaults);
        TEST_METHOD(CommandTimelineSettingsRoundTripThroughJson);
        TEST_METHOD(CommandTimelineHistoryLimitIsClampedAtRuntime);
        TEST_METHOD(CommandTimelineSettingsTolerateOutOfRangeAndOddValues);

    private:
        static winrt::com_ptr<implementation::GlobalAppSettings> FromJson(const Json::Value& json)
        {
            return implementation::GlobalAppSettings::FromJson(json);
        }
    };

    void WinTermCommandTimelineTests::CommandTimelineSettingsUseStableDefaults()
    {
        Json::Value missing{ Json::objectValue };
        const auto defaults = FromJson(missing);

        VERIFY_IS_TRUE(defaults->CommandTimelineEnabled());
        VERIFY_ARE_EQUAL(500, defaults->CommandTimelineHistoryLimit());

        // An absent setting must not be written back out, so an existing
        // settings file needs no migration.
        VERIFY_IS_FALSE(defaults->ToJson().isMember("commandTimeline.enabled"));
        VERIFY_IS_FALSE(defaults->ToJson().isMember("commandTimeline.historyLimit"));

        // The documented default matches the model's own default.
        VERIFY_ARE_EQUAL(DefaultCommandTimelineHistoryLimit,
                         static_cast<size_t>(defaults->CommandTimelineHistoryLimit()));
    }

    void WinTermCommandTimelineTests::CommandTimelineSettingsRoundTripThroughJson()
    {
        Json::Value json{ Json::objectValue };
        json["commandTimeline.enabled"] = false;
        json["commandTimeline.historyLimit"] = 1200;

        const auto settings = FromJson(json);
        VERIFY_IS_FALSE(settings->CommandTimelineEnabled());
        VERIFY_ARE_EQUAL(1200, settings->CommandTimelineHistoryLimit());

        const auto serialized = settings->ToJson();
        VERIFY_IS_FALSE(serialized["commandTimeline.enabled"].asBool());
        VERIFY_ARE_EQUAL(1200, serialized["commandTimeline.historyLimit"].asInt());

        // Reparsing the serialized form reproduces the same values.
        const auto reparsed = FromJson(serialized);
        VERIFY_IS_FALSE(reparsed->CommandTimelineEnabled());
        VERIFY_ARE_EQUAL(1200, reparsed->CommandTimelineHistoryLimit());

        Json::Value enabled{ Json::objectValue };
        enabled["commandTimeline.enabled"] = true;
        enabled["commandTimeline.historyLimit"] = 50;
        const auto minimum = FromJson(enabled);
        VERIFY_IS_TRUE(minimum->CommandTimelineEnabled());
        VERIFY_ARE_EQUAL(50, minimum->CommandTimelineHistoryLimit());
    }

    void WinTermCommandTimelineTests::CommandTimelineHistoryLimitIsClampedAtRuntime()
    {
        // Whatever the settings file says, the value the runtime uses is always
        // inside the supported range.
        for (const auto configured : { -5000, -1, 0, 1, 49 })
        {
            Json::Value json{ Json::objectValue };
            json["commandTimeline.historyLimit"] = configured;
            const auto settings = FromJson(json);
            VERIFY_ARE_EQUAL(MinCommandTimelineHistoryLimit,
                             ClampCommandTimelineHistoryLimit(settings->CommandTimelineHistoryLimit()));
        }

        for (const auto configured : { 5001, 100000 })
        {
            Json::Value json{ Json::objectValue };
            json["commandTimeline.historyLimit"] = configured;
            const auto settings = FromJson(json);
            VERIFY_ARE_EQUAL(MaxCommandTimelineHistoryLimit,
                             ClampCommandTimelineHistoryLimit(settings->CommandTimelineHistoryLimit()));
        }

        for (const auto configured : { 50, 500, 2500, 5000 })
        {
            Json::Value json{ Json::objectValue };
            json["commandTimeline.historyLimit"] = configured;
            const auto settings = FromJson(json);
            VERIFY_ARE_EQUAL(static_cast<size_t>(configured),
                             ClampCommandTimelineHistoryLimit(settings->CommandTimelineHistoryLimit()));
        }
    }

    void WinTermCommandTimelineTests::CommandTimelineSettingsTolerateOutOfRangeAndOddValues()
    {
        // An out-of-range value is accepted by the parser and clamped by the
        // runtime rather than failing the whole settings load.
        Json::Value extreme{ Json::objectValue };
        extreme["commandTimeline.historyLimit"] = 999999;
        const auto settings = FromJson(extreme);
        VERIFY_ARE_EQUAL(MaxCommandTimelineHistoryLimit,
                         ClampCommandTimelineHistoryLimit(settings->CommandTimelineHistoryLimit()));

        // A pane honors the clamped value: entries stay bounded even when the
        // configured limit was nonsense.
        CommandTimelineIndex index{ winrt::guid{} };
        index.SetHistoryLimit(ClampCommandTimelineHistoryLimit(settings->CommandTimelineHistoryLimit()));
        VERIFY_ARE_EQUAL(MaxCommandTimelineHistoryLimit, index.HistoryLimit());

        Json::Value tooSmall{ Json::objectValue };
        tooSmall["commandTimeline.historyLimit"] = 1;
        // Note: `small` is a Windows SDK macro, so this cannot be named that.
        const auto belowRange = FromJson(tooSmall);
        CommandTimelineIndex bounded{ winrt::guid{} };
        bounded.SetHistoryLimit(ClampCommandTimelineHistoryLimit(belowRange->CommandTimelineHistoryLimit()));
        VERIFY_ARE_EQUAL(MinCommandTimelineHistoryLimit, bounded.HistoryLimit());
    }
}
