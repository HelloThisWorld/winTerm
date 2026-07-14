// Copyright (c) winTerm contributors.
// Licensed under the MIT license.

#include "pch.h"

#include "../../winterm/Appearance/Importers/AppleTerminalImporter.h"
#include "../../winterm/Appearance/Importers/ITermColorsImporter.h"
#include "../../winterm/Appearance/Importers/PlistParser.h"
#include "../../winterm/Appearance/Importers/WindowsTerminalSchemeImporter.h"
#include "../../winterm/Appearance/Themes/ThemePreviewService.h"
#include "../../winterm/Appearance/Themes/ThemeSerializer.h"
#include "../../winterm/Appearance/Themes/ThemeValidator.h"

using namespace WEX::TestExecution;
using namespace winTerm::Appearance;

namespace SettingsModelUnitTests
{
    namespace
    {
        ThemeDescriptor MakeTheme(std::string id, const ThemeSourceType sourceType = ThemeSourceType::Imported)
        {
            ThemeDescriptor theme;
            theme.id = std::move(id);
            theme.name = "Test Theme";
            theme.displayName = "Test Theme";
            theme.variant = ThemeVariant::Dark;
            theme.source.type = sourceType;
            theme.source.project = "winTerm appearance tests";
            theme.source.author = "winTerm contributors";
            theme.source.homepage = "https://example.invalid/theme";
            theme.source.license = "MIT";
            theme.source.revision = "0123456789abcdef";
            theme.source.sourceFile = "themes/test.json";
            theme.terminal.foreground = "#CCCCCC";
            theme.terminal.background = "#0C0C0C";
            theme.terminal.cursorColor = "#FFFFFF";
            theme.terminal.cursorTextColor = "#0C0C0C";
            theme.terminal.selectionBackground = "#264F78";
            theme.terminal.selectionForeground = "#FFFFFF";
            theme.terminal.ansi = { "#0C0C0C", "#C50F1F", "#13A10E", "#C19C00", "#0037DA", "#881798", "#3A96DD", "#CCCCCC" };
            theme.terminal.bright = { "#767676", "#E74856", "#16C60C", "#F9F1A5", "#3B78FF", "#B4009E", "#61D6D6", "#F2F2F2" };
            theme.window.opacity = 0.85;
            theme.window.useAcrylic = true;
            theme.window.tabBarBackground = "#0C0C0C";
            theme.window.inactiveTabBackground = "#101010";
            theme.window.paneBorderColor = "#767676";
            return theme;
        }

        std::string CompactJson(const Json::Value& value)
        {
            Json::StreamWriterBuilder builder;
            builder["commentStyle"] = "None";
            builder["indentation"] = "";
            return Json::writeString(builder, value);
        }

        Json::Value MakeWindowsTerminalScheme(const std::string& name, const std::string& background)
        {
            Json::Value scheme{ Json::objectValue };
            scheme["name"] = name;
            scheme["foreground"] = "#AABBCC";
            scheme["background"] = background;
            scheme["cursorColor"] = "#DDEEFF";
            scheme["selectionBackground"] = "#445566";

            constexpr std::array<const char*, ThemePaletteSize> ansiKeys{ "black", "red", "green", "yellow", "blue", "purple", "cyan", "white" };
            constexpr std::array<const char*, ThemePaletteSize> brightKeys{ "brightBlack", "brightRed", "brightGreen", "brightYellow", "brightBlue", "brightPurple", "brightCyan", "brightWhite" };
            constexpr std::array<const char*, ThemePaletteSize> ansiColors{ "#010101", "#020202", "#030303", "#040404", "#050505", "#060606", "#070707", "#080808" };
            constexpr std::array<const char*, ThemePaletteSize> brightColors{ "#111111", "#121212", "#131313", "#141414", "#151515", "#161616", "#171717", "#181818" };
            for (size_t i = 0; i < ThemePaletteSize; ++i)
            {
                scheme[ansiKeys[i]] = ansiColors[i];
                scheme[brightKeys[i]] = brightColors[i];
            }
            return scheme;
        }

        bool HasIssueForField(const ThemeValidationResult& result, const std::string_view field)
        {
            return std::any_of(result.issues.begin(), result.issues.end(), [&](const auto& issue) {
                return issue.severity == ThemeIssueSeverity::Error && issue.field == field;
            });
        }

        bool Contains(const std::vector<std::string>& values, const std::string_view expected)
        {
            return std::find(values.begin(), values.end(), expected) != values.end();
        }

        template<typename TCallable>
        void VerifyRuntimeError(TCallable&& callable, const std::string_view expectedMessage)
        {
            std::string actualMessage;
            try
            {
                callable();
            }
            catch (const std::runtime_error& error)
            {
                actualMessage = error.what();
            }
            VERIFY_ARE_EQUAL(std::string{ expectedMessage }, actualMessage);
        }
    }

    class WinTermAppearanceTests
    {
        TEST_CLASS(WinTermAppearanceTests);

        TEST_METHOD(ThemeSerializationRoundTripsAndNormalizes);
        TEST_METHOD(ThemeValidationRejectsInvalidColorSchemaAndOpacity);
        TEST_METHOD(RegistryProtectsBuiltInThemesFromImportedCollisions);
        TEST_METHOD(RegistryResolvesPrecedenceAndCancelsPreview);
        TEST_METHOD(ITermImportDefaultsMissingAlphaAndIgnoresUnknownKeys);
        TEST_METHOD(PlistParserRejectsMalformedUnsafeOversizeAndBinaryInput);
        TEST_METHOD(WindowsTerminalImporterSupportsSingleArrayAndFullSettings);
        TEST_METHOD(AppleTerminalImporterIgnoresExecutableProfileFields);
        TEST_METHOD(ThemeSerializerRejectsInvalidJson);
    };

    void WinTermAppearanceTests::ThemeSerializationRoundTripsAndNormalizes()
    {
        auto original = MakeTheme("user.roundtrip");
        original.displayName = "Roundtrip Theme";
        original.terminal.foreground = "a1b2c3";
        original.terminal.cursorColor = "#abcd";
        original.originalFileName = R"(C:\Users\person\Downloads\roundtrip.winterm-theme.json)";

        const auto serialized = ThemeSerializer::Serialize(original);
        const auto restored = ThemeSerializer::Deserialize(serialized);
        VERIFY_ARE_EQUAL("user.roundtrip", restored.id);
        VERIFY_ARE_EQUAL("Roundtrip Theme", restored.displayName);
        VERIFY_ARE_EQUAL("#A1B2C3", restored.terminal.foreground);
        VERIFY_ARE_EQUAL("#DDAABBCC", restored.terminal.cursorColor);
        VERIFY_ARE_EQUAL(0.85, restored.window.opacity);
        VERIFY_IS_TRUE(restored.window.useAcrylic);
        VERIFY_IS_TRUE(restored.originalFileName.has_value());
        VERIFY_ARE_EQUAL("roundtrip.winterm-theme.json", *restored.originalFileName);
        VERIFY_ARE_EQUAL(ThemeSourceType::Imported, restored.source.type);
        VERIFY_ARE_EQUAL("0123456789abcdef", *restored.source.revision);

        const auto secondRoundtrip = ThemeSerializer::Deserialize(ThemeSerializer::Serialize(restored));
        VERIFY_ARE_EQUAL(restored.id, secondRoundtrip.id);
        VERIFY_ARE_EQUAL(restored.terminal.foreground, secondRoundtrip.terminal.foreground);
        VERIFY_ARE_EQUAL(restored.terminal.ansi[3], secondRoundtrip.terminal.ansi[3]);
        VERIFY_ARE_EQUAL(restored.window.paneBorderColor, secondRoundtrip.window.paneBorderColor);
    }

    void WinTermAppearanceTests::ThemeValidationRejectsInvalidColorSchemaAndOpacity()
    {
        auto invalidColor = MakeTheme("user.invalid-color");
        invalidColor.terminal.foreground = "not-a-color";
        const auto invalidColorResult = ThemeValidator::Validate(invalidColor);
        VERIFY_IS_FALSE(invalidColorResult.IsValid());
        VERIFY_IS_TRUE(HasIssueForField(invalidColorResult, "terminal.foreground"));

        auto invalidSchema = MakeTheme("user.invalid-schema");
        invalidSchema.schemaVersion = CurrentThemeSchemaVersion + 1;
        const auto invalidSchemaResult = ThemeValidator::Validate(invalidSchema);
        VERIFY_IS_FALSE(invalidSchemaResult.IsValid());
        VERIFY_IS_TRUE(HasIssueForField(invalidSchemaResult, "schemaVersion"));

        auto invalidOpacity = MakeTheme("user.invalid-opacity");
        invalidOpacity.window.opacity = 1.01;
        const auto invalidOpacityResult = ThemeValidator::Validate(invalidOpacity);
        VERIFY_IS_FALSE(invalidOpacityResult.IsValid());
        VERIFY_IS_TRUE(HasIssueForField(invalidOpacityResult, "window.opacity"));

        auto json = ThemeSerializer::ToJson(MakeTheme("user.invalid-json-values"));
        json["terminal"]["foreground"] = "orange";
        VerifyRuntimeError([&] { ThemeSerializer::Deserialize(CompactJson(json)); }, "The theme contains an invalid color value.");

        json = ThemeSerializer::ToJson(MakeTheme("user.invalid-json-values"));
        json["schemaVersion"] = CurrentThemeSchemaVersion + 1;
        VerifyRuntimeError([&] { ThemeSerializer::Deserialize(CompactJson(json)); }, "schemaVersion: This theme uses an unsupported schema version.");

        json = ThemeSerializer::ToJson(MakeTheme("user.invalid-json-values"));
        json["window"]["opacity"] = 1.01;
        VerifyRuntimeError([&] { ThemeSerializer::Deserialize(CompactJson(json)); }, "window.opacity: Theme opacity must be a finite number from 0.0 through 1.0.");
    }

    void WinTermAppearanceTests::RegistryProtectsBuiltInThemesFromImportedCollisions()
    {
        ThemeRegistry registry{ "winterm.base" };
        auto builtIn = MakeTheme("winterm.base", ThemeSourceType::BuiltIn);
        builtIn.displayName = "Built-in Base";
        registry.RegisterBuiltIn(builtIn);

        auto imported = MakeTheme("winterm.base");
        imported.displayName = "Imported Collision";
        const auto importedId = registry.RegisterImported(std::move(imported));
        VERIFY_ARE_EQUAL("winterm.base.2", importedId);
        VERIFY_ARE_EQUAL("Built-in Base", registry.GetThemeById("winterm.base")->displayName);
        VERIFY_ARE_EQUAL("Imported Collision", registry.GetThemeById(importedId)->displayName);
        VERIFY_IS_FALSE(registry.RemoveImported("winterm.base"));
        VERIFY_IS_TRUE(registry.RemoveImported(importedId));
        VERIFY_IS_NULL(registry.GetThemeById(importedId));
    }

    void WinTermAppearanceTests::RegistryResolvesPrecedenceAndCancelsPreview()
    {
        ThemeRegistry registry{ "winterm.fallback" };
        registry.RegisterBuiltIn(MakeTheme("winterm.fallback", ThemeSourceType::BuiltIn));
        registry.RegisterBundled(MakeTheme("thirdparty.global", ThemeSourceType::Bundled));
        registry.RegisterImported(MakeTheme("user.profile"));

        const auto profileId = std::optional<std::string_view>{ "user.profile" };
        const auto globalId = std::optional<std::string_view>{ "thirdparty.global" };
        VERIFY_ARE_EQUAL("user.profile", registry.Resolve(profileId, globalId)->id);
        VERIFY_ARE_EQUAL("thirdparty.global", registry.Resolve(std::optional<std::string_view>{ "missing.profile" }, globalId)->id);
        VERIFY_ARE_EQUAL("winterm.fallback", registry.Resolve(std::nullopt, std::nullopt)->id);

        auto temporary = MakeTheme("user.preview");
        ThemePreviewService preview{ registry };
        preview.Begin(temporary);
        VERIFY_IS_TRUE(preview.IsActive());
        VERIFY_ARE_EQUAL("user.preview", registry.Resolve(profileId, globalId)->id);

        preview.Cancel();
        VERIFY_IS_FALSE(preview.IsActive());
        VERIFY_IS_FALSE(registry.TemporaryPreview().has_value());
        VERIFY_ARE_EQUAL("user.profile", registry.Resolve(profileId, globalId)->id);
    }

    void WinTermAppearanceTests::ITermImportDefaultsMissingAlphaAndIgnoresUnknownKeys()
    {
        static constexpr std::string_view content{ R"xml(<?xml version="1.0" encoding="UTF-8"?>
<plist version="1.0">
<dict>
    <key>Foreground Color</key>
    <dict>
        <key>Red Component</key><real>0.1</real>
        <key>Green Component</key><real>0.2</real>
        <key>Blue Component</key><real>0.3</real>
    </dict>
    <key>Unknown Future Setting</key><string>ignored safely</string>
</dict>
</plist>)xml" };

        const ITermColorsImporter importer;
        const auto result = importer.Import(content, R"(C:\Themes\sample.itermcolors)");
        VERIFY_ARE_EQUAL(size_t{ 1 }, result.themes.size());
        const auto& theme = result.themes.front();
        VERIFY_ARE_EQUAL("sample", theme.displayName);
        VERIFY_ARE_EQUAL("#1A334D", theme.terminal.foreground);
        VERIFY_ARE_EQUAL("#0C0C0C", theme.terminal.background);
        VERIFY_ARE_EQUAL("sample.itermcolors", *theme.originalFileName);
        VERIFY_ARE_EQUAL(size_t{ 1 }, result.summary.warnings.size());
        VERIFY_IS_TRUE(ThemeValidator::Validate(theme).IsValid());
    }

    void WinTermAppearanceTests::PlistParserRejectsMalformedUnsafeOversizeAndBinaryInput()
    {
        VerifyRuntimeError(
            [] { PlistParser::Parse("<plist><dict><key>x</key><string>y</dict></plist>"); },
            "The selected file contains mismatched XML tags.");

        VerifyRuntimeError(
            [] { PlistParser::Parse(R"xml(<!DOCTYPE plist [<!ENTITY xxe SYSTEM "file:///etc/passwd">]><plist><dict><key>x</key><string>&xxe;</string></dict></plist>)xml"); },
            "XML document type definitions and entities are not supported.");

        const std::string oversize(MaximumThemeFileSize + 1, 'x');
        VerifyRuntimeError(
            [&] { PlistParser::Parse(oversize); },
            "The imported file exceeds the maximum allowed size.");

        VerifyRuntimeError(
            [] { PlistParser::Parse("bplist00unsupported-payload"); },
            "Binary plist is not supported in this version. Export the theme as XML plist and try again.");
    }

    void WinTermAppearanceTests::WindowsTerminalImporterSupportsSingleArrayAndFullSettings()
    {
        const WindowsTerminalSchemeImporter importer;
        const auto first = MakeWindowsTerminalScheme("First", "#010203");
        const auto second = MakeWindowsTerminalScheme("Second", "#F0F1F2");

        const auto singleResult = importer.Import(CompactJson(first), "single.json");
        VERIFY_ARE_EQUAL(size_t{ 1 }, singleResult.themes.size());
        VERIFY_ARE_EQUAL("First", singleResult.themes.front().displayName);
        VERIFY_ARE_EQUAL("#010203", singleResult.themes.front().terminal.background);
        VERIFY_ARE_EQUAL(ThemeVariant::Dark, singleResult.themes.front().variant);

        Json::Value array{ Json::arrayValue };
        array.append(first);
        array.append(second);
        const auto arrayResult = importer.Import(CompactJson(array), "schemes.json");
        VERIFY_ARE_EQUAL(size_t{ 2 }, arrayResult.themes.size());
        VERIFY_ARE_EQUAL("First", arrayResult.themes[0].displayName);
        VERIFY_ARE_EQUAL("Second", arrayResult.themes[1].displayName);
        VERIFY_ARE_EQUAL(ThemeVariant::Light, arrayResult.themes[1].variant);

        Json::Value settings{ Json::objectValue };
        settings["defaultProfile"] = "{00000000-0000-0000-0000-000000000000}";
        settings["profiles"] = Json::Value{ Json::objectValue };
        settings["profiles"]["defaults"]["commandline"] = "must-not-run.exe";
        settings["profiles"]["list"] = Json::Value{ Json::arrayValue };
        settings["actions"] = Json::Value{ Json::arrayValue };
        settings["keybindings"] = Json::Value{ Json::arrayValue };
        settings["commandline"] = "also-must-not-run.exe";
        settings["schemes"] = Json::Value{ Json::arrayValue };
        settings["schemes"].append(second);

        const auto fullSettingsResult = importer.Import(CompactJson(settings), "settings.json");
        VERIFY_ARE_EQUAL(size_t{ 1 }, fullSettingsResult.themes.size());
        VERIFY_ARE_EQUAL("Second", fullSettingsResult.themes.front().displayName);
        VERIFY_ARE_EQUAL(size_t{ 5 }, fullSettingsResult.summary.ignored.size());
        VERIFY_IS_TRUE(Contains(fullSettingsResult.summary.ignored, "profiles settings"));
        VERIFY_IS_TRUE(Contains(fullSettingsResult.summary.ignored, "actions settings"));
        VERIFY_IS_TRUE(Contains(fullSettingsResult.summary.ignored, "keybindings settings"));
        VERIFY_IS_TRUE(Contains(fullSettingsResult.summary.ignored, "commandline settings"));
        VERIFY_IS_TRUE(Contains(fullSettingsResult.summary.ignored, "defaultProfile settings"));
    }

    void WinTermAppearanceTests::AppleTerminalImporterIgnoresExecutableProfileFields()
    {
        static constexpr std::string_view content{ R"xml(<?xml version="1.0"?>
<plist version="1.0">
<dict>
    <key>name</key><string>Safe Colors Only</string>
    <key>TextColor</key>
    <dict>
        <key>Red Component</key><real>1</real>
        <key>Green Component</key><real>0.5</real>
        <key>Blue Component</key><real>0</real>
    </dict>
    <key>BackgroundColor</key>
    <dict>
        <key>Red Component</key><real>0</real>
        <key>Green Component</key><real>0</real>
        <key>Blue Component</key><real>0</real>
    </dict>
    <key>CommandString</key><string>powershell.exe -EncodedCommand must-not-run</string>
    <key>Shell</key><string>C:\untrusted\shell.exe</string>
    <key>EnvironmentVariables</key>
    <dict><key>PATH</key><string>C:\untrusted</string></dict>
    <key>KeyboardMappings</key><dict/>
</dict>
</plist>)xml" };

        const AppleTerminalImporter importer;
        const auto result = importer.Import(content, R"(C:\Themes\unsafe.terminal)");
        VERIFY_ARE_EQUAL(size_t{ 1 }, result.themes.size());
        const auto& theme = result.themes.front();
        VERIFY_ARE_EQUAL("Safe Colors Only", theme.displayName);
        VERIFY_ARE_EQUAL("#FF8000", theme.terminal.foreground);
        VERIFY_ARE_EQUAL("#000000", theme.terminal.background);
        VERIFY_ARE_EQUAL("unsafe.terminal", *theme.originalFileName);
        VERIFY_ARE_EQUAL(size_t{ 6 }, result.summary.ignored.size());
        VERIFY_IS_TRUE(Contains(result.summary.ignored, "Startup command"));
        VERIFY_IS_TRUE(Contains(result.summary.ignored, "Shell command"));
        VERIFY_IS_TRUE(Contains(result.summary.ignored, "Environment variables"));
        VERIFY_IS_TRUE(Contains(result.summary.ignored, "Keyboard mappings"));
        VERIFY_IS_TRUE(ThemeValidator::Validate(theme).IsValid());
    }

    void WinTermAppearanceTests::ThemeSerializerRejectsInvalidJson()
    {
        VerifyRuntimeError(
            [] { ThemeSerializer::ParseJsonDocument("{ invalid json }"); },
            "The selected file is not valid JSON.");

        VerifyRuntimeError(
            [] { ThemeSerializer::ParseJsonDocument(R"json({"id":"one","id":"two"})json"); },
            "The selected file is not valid JSON.");
    }
}
