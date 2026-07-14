// Copyright (c) winTerm contributors.
// Licensed under the MIT license.

#include "pch.h"

#include "../../winterm/Appearance/Fonts/FontDiscovery.h"
#include "../../winterm/Appearance/Fonts/FontFallbackResolver.h"
#include "../../winterm/Appearance/Fonts/PrivateFontCollection.h"
#include "../../winterm/Appearance/Licensing/AssetManifestValidator.h"
#include "../../winterm/Appearance/Licensing/ThirdPartyNoticeGenerator.h"

#include <algorithm>
#include <array>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <memory>
#include <optional>
#include <span>
#include <stdexcept>
#include <string>
#include <string_view>
#include <system_error>
#include <vector>

using namespace WEX::TestExecution;
using namespace winTerm::Appearance;
using namespace winTerm::Appearance::Licensing;

namespace SettingsModelUnitTests
{
    namespace
    {
        bool IsRepositoryRoot(const std::filesystem::path& path)
        {
            std::error_code error;
            return std::filesystem::is_regular_file(path / "assets/winterm/fonts/manifest.json", error) &&
                   !error &&
                   std::filesystem::is_regular_file(path / "assets/winterm/themes/manifest.json", error) &&
                   !error &&
                   std::filesystem::is_regular_file(path / "src/cascadia/UnitTests_SettingsModel/SettingsModel.UnitTests.vcxproj", error) &&
                   !error;
        }

        std::optional<std::filesystem::path> SearchForRepositoryRoot(std::filesystem::path seed)
        {
            std::error_code error;
            seed = std::filesystem::absolute(seed, error).lexically_normal();
            if (error)
            {
                return std::nullopt;
            }

            error.clear();
            if (std::filesystem::is_regular_file(seed, error) || seed.has_extension())
            {
                seed = seed.parent_path();
            }

            for (size_t depth = 0; depth < 16 && !seed.empty(); ++depth)
            {
                if (IsRepositoryRoot(seed))
                {
                    return seed;
                }
                const auto parent = seed.parent_path();
                if (parent == seed)
                {
                    break;
                }
                seed = parent;
            }
            return std::nullopt;
        }

        std::filesystem::path RepositoryRoot()
        {
            std::vector<std::filesystem::path> seeds;
            seeds.emplace_back(__FILE__);

            std::error_code error;
            if (const auto current = std::filesystem::current_path(error); !error)
            {
                seeds.emplace_back(current);
            }

            std::array<wchar_t, 32768> modulePath{};
            const auto modulePathLength = GetModuleFileNameW(nullptr,
                                                              modulePath.data(),
                                                              static_cast<DWORD>(modulePath.size()));
            if (modulePathLength != 0 && modulePathLength < modulePath.size())
            {
                seeds.emplace_back(std::wstring{ modulePath.data(), modulePathLength });
            }

            for (const auto& seed : seeds)
            {
                if (const auto root = SearchForRepositoryRoot(seed))
                {
                    return *root;
                }
            }
            throw std::runtime_error("The winTerm repository root could not be located for asset tests.");
        }

        Json::Value ReadJsonFile(const std::filesystem::path& path)
        {
            std::ifstream input{ path, std::ios::binary };
            if (!input)
            {
                throw std::runtime_error("The test JSON file could not be opened.");
            }
            const std::string content{ std::istreambuf_iterator<char>{ input }, std::istreambuf_iterator<char>{} };

            Json::CharReaderBuilder builder;
            builder["allowComments"] = false;
            builder["allowSpecialFloats"] = false;
            builder["collectComments"] = false;
            builder["failIfExtra"] = true;
            builder["rejectDupKeys"] = true;
            builder["strictRoot"] = true;
            const auto reader = std::unique_ptr<Json::CharReader>{ builder.newCharReader() };

            Json::Value root;
            std::string errors;
            if (!reader->parse(content.data(), content.data() + content.size(), &root, &errors))
            {
                throw std::runtime_error("The test JSON file is invalid: " + errors);
            }
            return root;
        }

        Json::Value SingleFontManifest(Json::Value entry)
        {
            Json::Value manifest{ Json::objectValue };
            manifest["schemaVersion"] = CurrentFontManifestSchemaVersion;
            manifest["pathBase"] = "repository";
            manifest["fonts"] = Json::Value{ Json::arrayValue };
            manifest["fonts"].append(entry);
            return manifest;
        }

        bool ContainsIssue(const FontRegistryLoadResult& result, const std::string_view text)
        {
            return std::any_of(result.issues.begin(), result.issues.end(), [&](const auto& issue) {
                return issue.message.find(text) != std::string::npos;
            });
        }

        bool ContainsIssue(const PrivateFontCollectionLoadResult& result, const std::string_view text)
        {
            return std::any_of(result.issues.begin(), result.issues.end(), [&](const auto& issue) {
                return issue.message.find(text) != std::string::npos;
            });
        }

        SystemFontDiscoveryRecord MakeSystemRecord(std::string id,
                                                   std::string familyName,
                                                   std::string fullName,
                                                   const std::optional<bool> monospaced = true)
        {
            SystemFontDiscoveryRecord record;
            record.id = std::move(id);
            record.familyName = std::move(familyName);
            record.fullName = std::move(fullName);
            record.monospaced = monospaced;
            return record;
        }

        void VerifySameFontList(const std::vector<FontDescriptor>& expected,
                                const std::vector<FontDescriptor>& actual)
        {
            VERIFY_ARE_EQUAL(expected.size(), actual.size());
            for (size_t index = 0; index < expected.size(); ++index)
            {
                VERIFY_ARE_EQUAL(expected[index].id, actual[index].id);
                VERIFY_ARE_EQUAL(expected[index].familyName, actual[index].familyName);
                VERIFY_ARE_EQUAL(expected[index].fullName, actual[index].fullName);
                VERIFY_ARE_EQUAL(expected[index].recommended, actual[index].recommended);
                VERIFY_ARE_EQUAL(expected[index].monospaced, actual[index].monospaced);
            }
        }

        void VerifyStringList(const std::vector<std::string>& expected,
                              const std::vector<std::string>& actual)
        {
            VERIFY_ARE_EQUAL(expected.size(), actual.size());
            for (size_t index = 0; index < expected.size(); ++index)
            {
                VERIFY_ARE_EQUAL(expected[index], actual[index]);
            }
        }
    }

    class WinTermFontAssetTests
    {
        TEST_CLASS(WinTermFontAssetTests);

        TEST_METHOD(RepositoryFontManifestLoadsFourBundledFonts);
        TEST_METHOD(FontManifestRejectsMissingFileInvalidShaAndDuplicateId);
        TEST_METHOD(SystemSnapshotMergeIsDeterministic);
        TEST_METHOD(FallbackResolutionHandlesSelectedMissingAndEmojiOptions);
        TEST_METHOD(PrivateCollectionHandlesEnabledDisabledAndMissingFiles);
        TEST_METHOD(RepositoryThemeAndFontAssetManifestsValidate);
        TEST_METHOD(LicenseRegistryAndNoticesAreDeterministic);
    };

    void WinTermFontAssetTests::RepositoryFontManifestLoadsFourBundledFonts()
    {
        const auto root = RepositoryRoot();
        FontRegistry registry;
        const auto result = registry.LoadBundledManifestFile(root / "assets/winterm/fonts/manifest.json", root);

        VERIFY_IS_TRUE(result.Succeeded());
        VERIFY_ARE_EQUAL(size_t{ 4 }, result.loaded);
        VERIFY_ARE_EQUAL(size_t{ 0 }, result.ignored);
        VERIFY_ARE_EQUAL(size_t{ 0 }, result.issues.size());
        VERIFY_ARE_EQUAL(size_t{ 4 }, registry.Size());

        const auto code = registry.GetFontById("CASCADIA-CODE.REGULAR");
        VERIFY_IS_NOT_NULL(code);
        VERIFY_ARE_EQUAL("Cascadia Code", code->familyName);
        VERIFY_ARE_EQUAL(FontSourceType::Bundled, code->source);
        VERIFY_ARE_EQUAL(FontPropertyState::Yes, code->monospaced);
        VERIFY_ARE_EQUAL("2407.24", code->version);
        VERIFY_ARE_EQUAL("OFL-1.1", code->license);
        VERIFY_ARE_EQUAL("56bcca3f2c1e4cb19458954f0e2bb4635960df91", code->sourceRevision);
        VERIFY_ARE_EQUAL("54eae6e26961e980eb3e1ec4ee30f6d3ba7a932a2028832baceb91cc65844a65", code->sha256);
        VERIFY_IS_TRUE(code->file.has_value());
        VERIFY_IS_TRUE(std::filesystem::is_regular_file(*code->file));
        VERIFY_IS_TRUE(code->licenseFile.has_value());
        VERIFY_IS_TRUE(std::filesystem::is_regular_file(*code->licenseFile));

        VERIFY_IS_NOT_NULL(registry.GetFontById("cascadia-code.italic"));
        VERIFY_IS_NOT_NULL(registry.GetFontById("cascadia-mono.regular"));
        VERIFY_IS_NOT_NULL(registry.GetFontById("cascadia-mono.italic"));

        auto packagedManifest = ReadJsonFile(root / "assets/winterm/fonts/manifest.json");
        for (auto& entry : packagedManifest["fonts"])
        {
            entry["file"] = "res/fonts/winterm-test-missing-source-font.ttf";
            entry["licenseFile"] = "assets/winterm/licenses/fonts/winterm-test-missing-license.txt";
            entry["packageFile"] = entry["packageFile"].asString().substr(std::string{ "AppearanceAssets/fonts/bundled/" }.size());
            entry["packageFile"] = "res/fonts/" + entry["packageFile"].asString();
            entry["packageLicenseFile"] = "assets/winterm/licenses/fonts/cascadia-code.txt";
        }
        FontRegistry packagedRegistry;
        const auto packagedResult = packagedRegistry.LoadBundledManifest(packagedManifest,
                                                                          root / "assets/winterm/fonts",
                                                                          root);
        VERIFY_IS_TRUE(packagedResult.Succeeded());
        VERIFY_ARE_EQUAL(size_t{ 4 }, packagedResult.loaded);
        VERIFY_ARE_EQUAL(size_t{ 4 }, packagedRegistry.Size());
    }

    void WinTermFontAssetTests::FontManifestRejectsMissingFileInvalidShaAndDuplicateId()
    {
        const auto root = RepositoryRoot();
        const auto sourceManifest = ReadJsonFile(root / "assets/winterm/fonts/manifest.json");
        const auto manifestDirectory = root / "assets/winterm/fonts";
        const auto sourceEntry = sourceManifest["fonts"][Json::ArrayIndex{ 0 }];

        auto missingEntry = sourceEntry;
        missingEntry["file"] = "res/fonts/winterm-test-missing-font.ttf";
        FontRegistry missingRegistry;
        const auto missingResult = missingRegistry.LoadBundledManifest(SingleFontManifest(std::move(missingEntry)),
                                                                        manifestDirectory,
                                                                        root);
        VERIFY_IS_FALSE(missingResult.Succeeded());
        VERIFY_ARE_EQUAL(size_t{ 0 }, missingResult.loaded);
        VERIFY_ARE_EQUAL(size_t{ 1 }, missingResult.ignored);
        VERIFY_ARE_EQUAL(size_t{ 0 }, missingRegistry.Size());
        VERIFY_IS_TRUE(ContainsIssue(missingResult, "font file is missing"));

        auto invalidShaEntry = sourceEntry;
        invalidShaEntry["sha256"] = "not-a-sha256-value";
        FontRegistry invalidShaRegistry;
        const auto invalidShaResult = invalidShaRegistry.LoadBundledManifest(SingleFontManifest(std::move(invalidShaEntry)),
                                                                              manifestDirectory,
                                                                              root);
        VERIFY_IS_FALSE(invalidShaResult.Succeeded());
        VERIFY_ARE_EQUAL(size_t{ 0 }, invalidShaResult.loaded);
        VERIFY_ARE_EQUAL(size_t{ 1 }, invalidShaResult.ignored);
        VERIFY_ARE_EQUAL(size_t{ 0 }, invalidShaRegistry.Size());
        VERIFY_IS_TRUE(ContainsIssue(invalidShaResult, "SHA-256 value is invalid"));

        auto duplicateManifest = SingleFontManifest(sourceEntry);
        duplicateManifest["fonts"].append(sourceEntry);
        FontRegistry duplicateRegistry;
        const auto duplicateResult = duplicateRegistry.LoadBundledManifest(duplicateManifest, manifestDirectory, root);
        VERIFY_IS_FALSE(duplicateResult.Succeeded());
        VERIFY_ARE_EQUAL(size_t{ 1 }, duplicateResult.loaded);
        VERIFY_ARE_EQUAL(size_t{ 1 }, duplicateResult.ignored);
        VERIFY_ARE_EQUAL(size_t{ 1 }, duplicateRegistry.Size());
        VERIFY_IS_TRUE(ContainsIssue(duplicateResult, "Duplicate font ID"));
    }

    void WinTermFontAssetTests::SystemSnapshotMergeIsDeterministic()
    {
        std::vector<SystemFontDiscoveryRecord> records;
        records.emplace_back(MakeSystemRecord({}, "Zeta Mono", "Zeta Mono Regular"));
        records.emplace_back(MakeSystemRecord({}, "Cascadia Mono", "Cascadia Mono Regular"));
        records.emplace_back(MakeSystemRecord("system.duplicate", "Zulu Duplicate", "Zulu Duplicate Regular"));
        records.emplace_back(MakeSystemRecord("SYSTEM.DUPLICATE", "Alpha Duplicate", "Alpha Duplicate Regular"));
        records.emplace_back(MakeSystemRecord({}, {}, "Invalid Font", std::nullopt));

        FontRegistry forwardRegistry;
        const auto forwardResult = FontDiscovery::MergeSystemSnapshot(
            forwardRegistry,
            std::span<const SystemFontDiscoveryRecord>{ records });

        auto reversedRecords = records;
        std::reverse(reversedRecords.begin(), reversedRecords.end());
        FontRegistry reverseRegistry;
        const auto reverseResult = FontDiscovery::MergeSystemSnapshot(
            reverseRegistry,
            std::span<const SystemFontDiscoveryRecord>{ reversedRecords });

        VERIFY_ARE_EQUAL(size_t{ 3 }, forwardResult.added);
        VERIFY_ARE_EQUAL(size_t{ 1 }, forwardResult.duplicates);
        VERIFY_ARE_EQUAL(size_t{ 1 }, forwardResult.invalid);
        VERIFY_ARE_EQUAL(forwardResult.added, reverseResult.added);
        VERIFY_ARE_EQUAL(forwardResult.duplicates, reverseResult.duplicates);
        VERIFY_ARE_EQUAL(forwardResult.invalid, reverseResult.invalid);

        const auto duplicate = forwardRegistry.GetFontById("system.duplicate");
        VERIFY_IS_NOT_NULL(duplicate);
        VERIFY_ARE_EQUAL("Alpha Duplicate", duplicate->familyName);

        const auto forwardFonts = forwardRegistry.ListFonts();
        const auto reverseFonts = reverseRegistry.ListFonts();
        VerifySameFontList(forwardFonts, reverseFonts);
        VERIFY_ARE_EQUAL(size_t{ 3 }, forwardFonts.size());
        VERIFY_ARE_EQUAL("Cascadia Mono", forwardFonts.front().familyName);
        VERIFY_IS_TRUE(forwardFonts.front().recommended);

        const auto repeatedResult = FontDiscovery::MergeSystemSnapshot(
            forwardRegistry,
            std::span<const SystemFontDiscoveryRecord>{ reversedRecords });
        VERIFY_ARE_EQUAL(size_t{ 0 }, repeatedResult.added);
        VERIFY_ARE_EQUAL(size_t{ 4 }, repeatedResult.duplicates);
        VERIFY_ARE_EQUAL(size_t{ 1 }, repeatedResult.invalid);
        VerifySameFontList(forwardFonts, forwardRegistry.ListFonts());
    }

    void WinTermFontAssetTests::FallbackResolutionHandlesSelectedMissingAndEmojiOptions()
    {
        const std::vector<std::string> availableFamilies{
            "Selected Mono",
            "Cascadia Mono NF",
            "Cascadia Mono",
            "Segoe UI Emoji",
        };
        const auto available = std::span<const std::string>{ availableFamilies };

        const auto selected = FontFallbackResolver::Resolve(available, "selected mono");
        VerifyStringList(
            { "Selected Mono", "Cascadia Mono NF", "Cascadia Mono", "Segoe UI Symbol", "Segoe UI Emoji" },
            selected);

        FontFallbackOptions noEmoji;
        noEmoji.emojiFallbackEnabled = false;
        const auto missing = FontFallbackResolver::Resolve(available, "Missing Mono", noEmoji);
        VerifyStringList(
            { "Cascadia Mono NF", "Cascadia Mono", "Segoe UI Symbol" },
            missing);

        FontFallbackOptions onlyEmoji;
        onlyEmoji.fallbackEnabled = false;
        const auto emojiWithoutSymbolFallback = FontFallbackResolver::Resolve(available, "Selected Mono", onlyEmoji);
        VerifyStringList(
            { "Selected Mono", "Cascadia Mono", "Segoe UI Emoji" },
            emojiWithoutSymbolFallback);

        onlyEmoji.emojiFallbackEnabled = false;
        const auto noOptionalFallback = FontFallbackResolver::Resolve(available, "Missing Mono", onlyEmoji);
        VerifyStringList({ "Cascadia Mono" }, noOptionalFallback);
    }

    void WinTermFontAssetTests::PrivateCollectionHandlesEnabledDisabledAndMissingFiles()
    {
        const auto root = RepositoryRoot();
        FontRegistry registry;
        const auto manifestResult = registry.LoadBundledManifestFile(root / "assets/winterm/fonts/manifest.json", root);
        VERIFY_IS_TRUE(manifestResult.Succeeded());
        const auto bundledFonts = registry.ListFonts(FontFilter::Bundled);

        PrivateFontCollection collection;
        const auto enabledResult = collection.Load(std::span<const FontDescriptor>{ bundledFonts }, root, true);
        VERIFY_ARE_EQUAL(size_t{ 4 }, enabledResult.loaded);
        VERIFY_ARE_EQUAL(size_t{ 0 }, enabledResult.ignored);
        VERIFY_IS_TRUE(enabledResult.HasUsableFonts());
        VERIFY_IS_TRUE(collection.IsEnabled());
        VERIFY_IS_TRUE(collection.IsReady());
        VERIFY_ARE_EQUAL("Cascadia Code", collection.ResolveFamily("cascadia code"));

        const auto disabledResult = collection.Load(std::span<const FontDescriptor>{ bundledFonts }, root, false);
        VERIFY_ARE_EQUAL(size_t{ 0 }, disabledResult.loaded);
        VERIFY_ARE_EQUAL(size_t{ 0 }, disabledResult.ignored);
        VERIFY_IS_FALSE(disabledResult.HasUsableFonts());
        VERIFY_IS_FALSE(collection.IsEnabled());
        VERIFY_IS_FALSE(collection.IsReady());
        VERIFY_ARE_EQUAL("Cascadia Mono", collection.ResolveFamily("Cascadia Code"));

        FontDescriptor missingFont;
        missingFont.id = "winterm.test.missing";
        missingFont.familyName = "Missing Test Font";
        missingFont.fullName = "Missing Test Font Regular";
        missingFont.source = FontSourceType::Bundled;
        missingFont.file = root / "res/fonts/winterm-test-missing-private-font.ttf";
        missingFont.sha256 = std::string(64, '0');
        const std::array<FontDescriptor, 1> missingFonts{ missingFont };

        const auto missingResult = collection.Load(std::span<const FontDescriptor>{ missingFonts }, root, true);
        VERIFY_ARE_EQUAL(size_t{ 0 }, missingResult.loaded);
        VERIFY_ARE_EQUAL(size_t{ 1 }, missingResult.ignored);
        VERIFY_IS_FALSE(missingResult.HasUsableFonts());
        VERIFY_IS_TRUE(collection.IsEnabled());
        VERIFY_IS_FALSE(collection.IsReady());
        VERIFY_IS_TRUE(ContainsIssue(missingResult, "font file is missing"));
        VERIFY_ARE_EQUAL("Cascadia Mono", collection.ResolveFamily("Missing Test Font"));
    }

    void WinTermFontAssetTests::RepositoryThemeAndFontAssetManifestsValidate()
    {
        const auto root = RepositoryRoot();
        const auto themeResult = AssetManifestValidator::ValidateFile(
            root / "assets/winterm/themes/manifest.json",
            AssetCategory::Theme,
            root);
        const auto fontResult = AssetManifestValidator::ValidateFile(
            root / "assets/winterm/fonts/manifest.json",
            AssetCategory::Font,
            root);

        VERIFY_IS_TRUE(themeResult.IsValid());
        VERIFY_ARE_EQUAL(size_t{ 0 }, themeResult.ErrorMessages().size());
        VERIFY_IS_TRUE(fontResult.IsValid());
        VERIFY_ARE_EQUAL(size_t{ 0 }, fontResult.ErrorMessages().size());
    }

    void WinTermFontAssetTests::LicenseRegistryAndNoticesAreDeterministic()
    {
        const auto root = RepositoryRoot();
        const auto themeManifest = ReadJsonFile(root / "assets/winterm/themes/manifest.json");
        const auto fontManifest = ReadJsonFile(root / "assets/winterm/fonts/manifest.json");

        AssetLicenseRegistry themeFirst;
        const auto firstThemes = themeFirst.RegisterManifest(themeManifest, AssetCategory::Theme);
        const auto firstFonts = themeFirst.RegisterManifest(fontManifest, AssetCategory::Font);
        VERIFY_IS_TRUE(firstThemes.Succeeded());
        VERIFY_IS_TRUE(firstFonts.Succeeded());
        VERIFY_ARE_EQUAL(size_t{ 15 }, firstThemes.registered);
        VERIFY_ARE_EQUAL(size_t{ 4 }, firstFonts.registered);
        VERIFY_ARE_EQUAL(size_t{ 19 }, themeFirst.Size());

        AssetLicenseRegistry fontFirst;
        const auto secondFonts = fontFirst.RegisterManifest(fontManifest, AssetCategory::Font);
        const auto secondThemes = fontFirst.RegisterManifest(themeManifest, AssetCategory::Theme);
        VERIFY_IS_TRUE(secondFonts.Succeeded());
        VERIFY_IS_TRUE(secondThemes.Succeeded());
        VERIFY_ARE_EQUAL(size_t{ 19 }, fontFirst.Size());

        const auto themeFirstEntries = themeFirst.Entries();
        const auto fontFirstEntries = fontFirst.Entries();
        VERIFY_ARE_EQUAL(themeFirstEntries.size(), fontFirstEntries.size());
        for (size_t index = 0; index < themeFirstEntries.size(); ++index)
        {
            VERIFY_ARE_EQUAL(themeFirstEntries[index].category, fontFirstEntries[index].category);
            VERIFY_ARE_EQUAL(themeFirstEntries[index].id, fontFirstEntries[index].id);
            VERIFY_ARE_EQUAL(themeFirstEntries[index].name, fontFirstEntries[index].name);
            VERIFY_ARE_EQUAL(themeFirstEntries[index].sourceRevision, fontFirstEntries[index].sourceRevision);
            VERIFY_ARE_EQUAL(themeFirstEntries[index].sha256, fontFirstEntries[index].sha256);
        }

        const auto firstNotice = ThirdPartyNoticeGenerator::Generate(themeFirst);
        const auto repeatedNotice = ThirdPartyNoticeGenerator::Generate(themeFirst);
        const auto reorderedNotice = ThirdPartyNoticeGenerator::Generate(fontFirst);
        VERIFY_ARE_EQUAL(firstNotice, repeatedNotice);
        VERIFY_ARE_EQUAL(firstNotice, reorderedNotice);
        VERIFY_ARE_EQUAL(size_t{ 0 }, firstNotice.find("# Third-Party Notices\n\n"));

        const auto themesHeading = firstNotice.find("## Themes\n\n");
        const auto fontsHeading = firstNotice.find("## Fonts\n\n");
        VERIFY_IS_TRUE(themesHeading != std::string::npos);
        VERIFY_IS_TRUE(fontsHeading != std::string::npos);
        VERIFY_IS_TRUE(themesHeading < fontsHeading);
        VERIFY_IS_TRUE(firstNotice.find("- Source revision: 4d8bb2f00fb86927a98dd3502cdec74a76d25d7b") != std::string::npos);
        VERIFY_IS_TRUE(firstNotice.find("- License file: assets/winterm/licenses/fonts/cascadia-code.txt") != std::string::npos);
    }
}
