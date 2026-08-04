// Copyright (c) winTerm contributors.
// Licensed under the MIT license.

#pragma once

#include "VisualProgressModel.h"

#include <array>
#include <atomic>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <mutex>
#include <optional>
#include <string_view>

namespace winTerm::VisualProgress
{
    // Recognition runs on fresh terminal output only. The caller remains
    // responsible for preserving the original chunk unless suppressInput is
    // true. Suppression is deliberately stricter than recognition.
    struct RecognitionOptions
    {
        bool replacementEnabled{};
        bool rendererEnabled{};
        bool normalScreen{ true };
        bool parserHealthy{ true };
    };

    struct RecognitionResult
    {
        std::optional<ProviderProgress> progress;
        bool suppressInput{};
        bool overflow{};
        bool healthy{ true };
        bool accepted{ true };
    };

    class RecognitionEngine final
    {
    public:
        static constexpr size_t MaxChunkCodeUnits = 32 * 1024;
        static constexpr size_t MaxCurrentLineCodeUnits = 2048;
        static constexpr size_t MaxAnsiSequenceCodeUnits = 64;
        static constexpr size_t RecentProgressCapacity = 8;
        static constexpr size_t DockerLayerCapacity = 32;
        static constexpr size_t BuildKitStepCapacity = 32;
        static constexpr uint64_t PublicationIntervalMilliseconds = 50;

        RecognitionEngine() noexcept = default;
        RecognitionEngine(const RecognitionEngine&) = delete;
        RecognitionEngine& operator=(const RecognitionEngine&) = delete;

        // This is a non-blocking ingress boundary. If another thread owns the
        // state lock, the chunk is not inspected and must be rendered normally.
        // Missing one chunk desynchronizes the bounded parser until Reset/Clear.
        RecognitionResult Consume(const std::wstring_view chunk,
                                  const uint64_t timestampMilliseconds,
                                  const RecognitionOptions options = {}) noexcept
        {
            std::unique_lock lock{ _mutex, std::try_to_lock };
            if (!lock.owns_lock())
            {
                _desynchronized.store(true, std::memory_order_release);
                return { std::nullopt, false, false, false, false };
            }

            RecognitionResult result;
            CallState call;
            call.cleanStart = _lineLength == 0 &&
                              _ansiState == AnsiState::Ground &&
                              !_pendingHighSurrogate;
            call.columnKnownAtStart = _columnKnown;
            call.columnZeroAtStart = _atColumnZero;

            if (_desynchronized.exchange(false, std::memory_order_acq_rel))
            {
                _healthy = false;
            }

            if (chunk.size() > MaxChunkCodeUnits)
            {
                _healthy = false;
                _clearRecord();
                _ansiState = AnsiState::Ground;
                _ansiLength = 0;
                _pendingHighSurrogate = false;
                _previousChunkEndedWithCarriageReturn = false;
                result.overflow = true;
                result.healthy = false;
                return result;
            }

            size_t index{};
            if (_previousChunkEndedWithCarriageReturn)
            {
                if (!chunk.empty() && chunk.front() == L'\n')
                {
                    // The preceding CR was the first half of a split CRLF.
                    // Publish its structure as a newline record, but never as
                    // transient or suppressible output.
                    if (_pendingCarriageReturnProgress)
                    {
                        _pendingCarriageReturnProgress->transient = false;
                        _pendingCarriageReturnProgress->suppressible = false;
                        _rememberProgress(*_pendingCarriageReturnProgress);
                        ++call.recognizedRecords;
                        _pendingCarriageReturnProgress.reset();
                    }
                    _previousChunkEndedWithCarriageReturn = false;
                    _columnKnown = true;
                    _atColumnZero = true;
                    index = 1;
                    call.cleanStart = false;
                    call.sawNewline = true;
                }
                else if (!chunk.empty())
                {
                    _acceptHeldCarriageReturn(call);
                    _previousChunkEndedWithCarriageReturn = false;
                }
            }

            for (; index < chunk.size(); ++index)
            {
                const auto codeUnit = chunk[index];
                const auto atEnd = index + 1 == chunk.size();

                if (_pendingHighSurrogate)
                {
                    if (_isLowSurrogate(codeUnit))
                    {
                        if (_ansiState == AnsiState::Escape || _ansiState == AnsiState::Csi)
                        {
                            // UTF-16 surrogate pairs are valid OSC payload,
                            // but cannot occur in an escape introducer or CSI.
                            _recordMalformed = true;
                            call.malformed = true;
                            call.onlySafeContent = false;
                            _healthy = false;
                            _ansiState = _ansiState == AnsiState::Csi ? AnsiState::CsiDiscard : AnsiState::Ground;
                            _ansiLength = 0;
                        }
                        else
                        {
                            _consumeCodeUnit(_pendingHighSurrogateValue, false, call);
                            _consumeCodeUnit(codeUnit, atEnd, call);
                        }
                        _pendingHighSurrogate = false;
                        continue;
                    }

                    _pendingHighSurrogate = false;
                    _recordMalformed = true;
                    call.malformed = true;
                    call.onlySafeContent = false;
                    _healthy = false;
                }

                if (_isHighSurrogate(codeUnit))
                {
                    _pendingHighSurrogate = true;
                    _pendingHighSurrogateValue = codeUnit;
                    continue;
                }
                if (_isLowSurrogate(codeUnit))
                {
                    _recordMalformed = true;
                    call.malformed = true;
                    call.onlySafeContent = false;
                    _healthy = false;
                    continue;
                }

                _consumeCodeUnit(codeUnit, atEnd, call);
            }

            if (_pendingHighSurrogate)
            {
                // A split pair is healthy but makes this callback ineligible
                // for immediate suppression.
                call.onlySafeContent = false;
            }

            result.progress = _takePublication(timestampMilliseconds);
            result.overflow = call.overflow;
            result.healthy = _healthy && !call.malformed && !call.overflow;

            const auto completeAtEnd = _lineLength == 0 &&
                                       _ansiState == AnsiState::Ground &&
                                       !_pendingHighSurrogate;
            const auto immediateWholeChunk = call.cleanStart &&
                                             completeAtEnd &&
                                             call.nonEmptyRecords == 1 &&
                                             call.recognizedRecords == 1 &&
                                             call.onlySafeContent &&
                                             !call.sawNewline &&
                                             !call.ambiguousCarriageReturn &&
                                             call.suppressionCandidate.has_value();

            result.suppressInput = immediateWholeChunk &&
                                   options.replacementEnabled &&
                                   options.rendererEnabled &&
                                   options.normalScreen &&
                                   options.parserHealthy &&
                                   result.healthy &&
                                   call.columnKnownAtStart &&
                                   call.columnZeroAtStart;

            if (result.suppressInput)
            {
                // The caller will not apply the chunk to the terminal, so keep
                // our conservative cursor model aligned with the terminal.
                _columnKnown = call.columnKnownAtStart;
                _atColumnZero = call.columnZeroAtStart;
            }

            return result;
        }

        void Reset() noexcept
        {
            std::scoped_lock lock{ _mutex };
            _resetUnderLock();
        }

        void Clear() noexcept
        {
            Reset();
        }

        bool TryReset() noexcept
        {
            std::unique_lock lock{ _mutex, std::try_to_lock };
            if (!lock.owns_lock())
            {
                _desynchronized.store(true, std::memory_order_release);
                return false;
            }
            _resetUnderLock();
            return true;
        }

    private:
        enum class AnsiState : uint8_t
        {
            Ground,
            Escape,
            Csi,
            CsiDiscard,
            Osc,
            OscEscape,
            OscDiscard,
            OscDiscardEscape,
        };

        enum class RecordEnding : uint8_t
        {
            CarriageReturn,
            Newline,
        };

        struct CallState
        {
            std::optional<ProviderProgress> suppressionCandidate;
            size_t nonEmptyRecords{};
            size_t recognizedRecords{};
            bool cleanStart{};
            bool columnKnownAtStart{};
            bool columnZeroAtStart{};
            bool onlySafeContent{ true };
            bool sawNewline{};
            bool ambiguousCarriageReturn{};
            bool malformed{};
            bool overflow{};
        };

        struct Match
        {
            ProviderProgress progress;
            bool matched{};
            bool preserveOnly{};
            bool pendingCandidate{};
        };

        struct LayerState
        {
            uint64_t key{};
            bool used{};
            bool complete{};
        };

        struct StepState
        {
            uint32_t id{};
            bool used{};
            bool complete{};
        };

        struct Quantity
        {
            uint64_t milliBytes{};
            bool valid{};
        };

        static constexpr bool _isHighSurrogate(const wchar_t value) noexcept
        {
            return value >= 0xD800 && value <= 0xDBFF;
        }

        static constexpr bool _isLowSurrogate(const wchar_t value) noexcept
        {
            return value >= 0xDC00 && value <= 0xDFFF;
        }

        static constexpr wchar_t _asciiLower(const wchar_t value) noexcept
        {
            return value >= L'A' && value <= L'Z' ? value + (L'a' - L'A') : value;
        }

        static bool _equalsInsensitive(const std::wstring_view left, const std::wstring_view right) noexcept
        {
            if (left.size() != right.size())
            {
                return false;
            }
            for (size_t i = 0; i < left.size(); ++i)
            {
                if (_asciiLower(left[i]) != _asciiLower(right[i]))
                {
                    return false;
                }
            }
            return true;
        }

        static bool _startsWithInsensitive(const std::wstring_view value, const std::wstring_view prefix) noexcept
        {
            return value.size() >= prefix.size() && _equalsInsensitive(value.substr(0, prefix.size()), prefix);
        }

        static bool _containsInsensitive(const std::wstring_view value, const std::wstring_view needle) noexcept
        {
            if (needle.empty())
            {
                return true;
            }
            if (needle.size() > value.size())
            {
                return false;
            }
            for (size_t i = 0; i + needle.size() <= value.size(); ++i)
            {
                if (_equalsInsensitive(value.substr(i, needle.size()), needle))
                {
                    return true;
                }
            }
            return false;
        }

        static bool _containsTokenInsensitive(const std::wstring_view value, const std::wstring_view token) noexcept
        {
            if (token.empty() || token.size() > value.size())
            {
                return false;
            }
            const auto isWord = [](const wchar_t ch) noexcept {
                return (ch >= L'a' && ch <= L'z') ||
                       (ch >= L'A' && ch <= L'Z') ||
                       (ch >= L'0' && ch <= L'9') ||
                       ch == L'_';
            };
            for (size_t i = 0; i + token.size() <= value.size(); ++i)
            {
                if ((i != 0 && isWord(value[i - 1])) ||
                    (i + token.size() != value.size() && isWord(value[i + token.size()])))
                {
                    continue;
                }
                if (_equalsInsensitive(value.substr(i, token.size()), token))
                {
                    return true;
                }
            }
            return false;
        }

        static bool _containsFileExtensionInsensitive(const std::wstring_view value,
                                                      const std::wstring_view extension) noexcept
        {
            if (extension.empty() || extension.size() > value.size())
            {
                return false;
            }
            for (size_t i = 0; i + extension.size() <= value.size(); ++i)
            {
                if (!_equalsInsensitive(value.substr(i, extension.size()), extension))
                {
                    continue;
                }
                const auto after = i + extension.size();
                if (after == value.size() ||
                    value[after] == L' ' || value[after] == L'\t' ||
                    value[after] == L')' || value[after] == L'?' ||
                    value[after] == L'#' || value[after] == L'\'' ||
                    value[after] == L'"')
                {
                    return true;
                }
            }
            return false;
        }

        static std::wstring_view _trim(const std::wstring_view value) noexcept
        {
            size_t first{};
            while (first < value.size() && (value[first] == L' ' || value[first] == L'\t'))
            {
                ++first;
            }
            size_t last = value.size();
            while (last > first && (value[last - 1] == L' ' || value[last - 1] == L'\t'))
            {
                --last;
            }
            return value.substr(first, last - first);
        }

        static bool _parseUnsigned(const std::wstring_view value,
                                   const size_t first,
                                   const size_t last,
                                   uint64_t& parsed) noexcept
        {
            if (first >= last || last > value.size())
            {
                return false;
            }
            uint64_t result{};
            for (size_t i = first; i < last; ++i)
            {
                const auto ch = value[i];
                if (ch < L'0' || ch > L'9')
                {
                    return false;
                }
                const auto digit = static_cast<uint64_t>(ch - L'0');
                if (result > (std::numeric_limits<uint64_t>::max() - digit) / 10)
                {
                    return false;
                }
                result = result * 10 + digit;
            }
            parsed = result;
            return true;
        }

        static std::optional<uint8_t> _findPercent(const std::wstring_view value) noexcept
        {
            for (size_t percent = 1; percent < value.size(); ++percent)
            {
                if (value[percent] != L'%')
                {
                    continue;
                }
                auto first = percent;
                while (first > 0 && value[first - 1] >= L'0' && value[first - 1] <= L'9')
                {
                    --first;
                }
                if (first > 0 &&
                    (value[first - 1] == L'+' || value[first - 1] == L'-' || value[first - 1] == L'.'))
                {
                    continue;
                }
                uint64_t parsed{};
                if (_parseUnsigned(value, first, percent, parsed) && parsed <= 100)
                {
                    return static_cast<uint8_t>(parsed);
                }
            }
            return std::nullopt;
        }

        static std::optional<uint8_t> _findIntegerFraction(const std::wstring_view value) noexcept
        {
            for (size_t slash = 1; slash + 1 < value.size(); ++slash)
            {
                if (value[slash] != L'/')
                {
                    continue;
                }
                auto leftFirst = slash;
                while (leftFirst > 0 && value[leftFirst - 1] >= L'0' && value[leftFirst - 1] <= L'9')
                {
                    --leftFirst;
                }
                auto rightLast = slash + 1;
                while (rightLast < value.size() && value[rightLast] >= L'0' && value[rightLast] <= L'9')
                {
                    ++rightLast;
                }
                // A chained digit/digit/digit shape is a slashed date or a
                // path segment, never a completed/total meter.
                if ((leftFirst > 0 && value[leftFirst - 1] == L'/') ||
                    (rightLast < value.size() && value[rightLast] == L'/'))
                {
                    continue;
                }
                uint64_t current{};
                uint64_t total{};
                if (_parseUnsigned(value, leftFirst, slash, current) &&
                    _parseUnsigned(value, slash + 1, rightLast, total) &&
                    total > 0 && current <= total)
                {
                    return static_cast<uint8_t>((static_cast<long double>(current) * 100.0L) /
                                                static_cast<long double>(total));
                }
            }
            return std::nullopt;
        }

        static uint64_t _unitMultiplier(const std::wstring_view unit) noexcept
        {
            if (unit.empty() || _equalsInsensitive(unit, L"b"))
            {
                return 1;
            }
            if (_equalsInsensitive(unit, L"kb") || _equalsInsensitive(unit, L"kib"))
            {
                return 1024ull;
            }
            if (_equalsInsensitive(unit, L"mb") || _equalsInsensitive(unit, L"mib"))
            {
                return 1024ull * 1024ull;
            }
            if (_equalsInsensitive(unit, L"gb") || _equalsInsensitive(unit, L"gib"))
            {
                return 1024ull * 1024ull * 1024ull;
            }
            if (_equalsInsensitive(unit, L"tb") || _equalsInsensitive(unit, L"tib"))
            {
                return 1024ull * 1024ull * 1024ull * 1024ull;
            }
            return 0;
        }

        static Quantity _parseQuantity(const std::wstring_view value,
                                       const size_t numberFirst,
                                       const size_t numberLast,
                                       const size_t unitFirst,
                                       const size_t unitLast) noexcept
        {
            if (numberFirst >= numberLast || numberLast > value.size() || unitLast > value.size())
            {
                return {};
            }

            uint64_t whole{};
            uint64_t fraction{};
            uint64_t fractionScale{ 1 };
            bool decimalSeen{};
            bool digitSeen{};
            for (size_t i = numberFirst; i < numberLast; ++i)
            {
                const auto ch = value[i];
                if (ch == L'.' && !decimalSeen)
                {
                    decimalSeen = true;
                    continue;
                }
                if (ch < L'0' || ch > L'9')
                {
                    return {};
                }
                digitSeen = true;
                const auto digit = static_cast<uint64_t>(ch - L'0');
                if (!decimalSeen)
                {
                    if (whole > (std::numeric_limits<uint64_t>::max() - digit) / 10)
                    {
                        return {};
                    }
                    whole = whole * 10 + digit;
                }
                else if (fractionScale < 1000)
                {
                    fraction = fraction * 10 + digit;
                    fractionScale *= 10;
                }
            }
            if (!digitSeen)
            {
                return {};
            }

            const auto multiplier = _unitMultiplier(value.substr(unitFirst, unitLast - unitFirst));
            if (multiplier == 0 || whole > std::numeric_limits<uint64_t>::max() / multiplier / 1000)
            {
                return {};
            }
            const auto wholeMilliBytes = whole * multiplier * 1000;
            const auto fractionalMilliBytes = fractionScale > 1 ?
                                                  (fraction * multiplier * 1000) / fractionScale :
                                                  0;
            if (wholeMilliBytes > std::numeric_limits<uint64_t>::max() - fractionalMilliBytes)
            {
                return {};
            }
            return { wholeMilliBytes + fractionalMilliBytes, true };
        }

        static std::optional<uint8_t> _findQuantityFraction(const std::wstring_view value) noexcept
        {
            for (size_t slash = 1; slash + 1 < value.size(); ++slash)
            {
                if (value[slash] != L'/')
                {
                    continue;
                }

                auto leftUnitFirst = slash;
                while (leftUnitFirst > 0 && ((value[leftUnitFirst - 1] >= L'A' && value[leftUnitFirst - 1] <= L'Z') ||
                                             (value[leftUnitFirst - 1] >= L'a' && value[leftUnitFirst - 1] <= L'z')))
                {
                    --leftUnitFirst;
                }
                auto leftNumberFirst = leftUnitFirst;
                while (leftNumberFirst > 0 && ((value[leftNumberFirst - 1] >= L'0' && value[leftNumberFirst - 1] <= L'9') ||
                                               value[leftNumberFirst - 1] == L'.'))
                {
                    --leftNumberFirst;
                }

                auto rightNumberLast = slash + 1;
                while (rightNumberLast < value.size() && ((value[rightNumberLast] >= L'0' && value[rightNumberLast] <= L'9') ||
                                                          value[rightNumberLast] == L'.'))
                {
                    ++rightNumberLast;
                }
                auto rightUnitLast = rightNumberLast;
                while (rightUnitLast < value.size() && ((value[rightUnitLast] >= L'A' && value[rightUnitLast] <= L'Z') ||
                                                        (value[rightUnitLast] >= L'a' && value[rightUnitLast] <= L'z')))
                {
                    ++rightUnitLast;
                }

                if (leftUnitFirst == slash || rightUnitLast == rightNumberLast)
                {
                    continue;
                }

                const auto current = _parseQuantity(value, leftNumberFirst, leftUnitFirst, leftUnitFirst, slash);
                const auto total = _parseQuantity(value, slash + 1, rightNumberLast, rightNumberLast, rightUnitLast);
                if (current.valid && total.valid && total.milliBytes > 0 && current.milliBytes <= total.milliBytes)
                {
                    return static_cast<uint8_t>((static_cast<long double>(current.milliBytes) * 100.0L) /
                                                static_cast<long double>(total.milliBytes));
                }
            }
            return std::nullopt;
        }

        // Rich-style progress meters commonly put the unit after the total,
        // for example "1.5/3.0 MB". The shared unit is applied to both
        // operands so only structural numeric state is retained.
        static std::optional<uint8_t> _findSharedUnitQuantityFraction(const std::wstring_view value) noexcept
        {
            for (size_t slash = 1; slash + 1 < value.size(); ++slash)
            {
                if (value[slash] != L'/')
                {
                    continue;
                }

                auto leftFirst = slash;
                while (leftFirst > 0 && ((value[leftFirst - 1] >= L'0' && value[leftFirst - 1] <= L'9') ||
                                         value[leftFirst - 1] == L'.'))
                {
                    --leftFirst;
                }

                auto rightFirst = slash + 1;
                while (rightFirst < value.size() && (value[rightFirst] == L' ' || value[rightFirst] == L'\t'))
                {
                    ++rightFirst;
                }
                auto rightLast = rightFirst;
                while (rightLast < value.size() && ((value[rightLast] >= L'0' && value[rightLast] <= L'9') ||
                                                    value[rightLast] == L'.'))
                {
                    ++rightLast;
                }

                auto unitFirst = rightLast;
                while (unitFirst < value.size() && (value[unitFirst] == L' ' || value[unitFirst] == L'\t'))
                {
                    ++unitFirst;
                }
                auto unitLast = unitFirst;
                while (unitLast < value.size() && ((value[unitLast] >= L'A' && value[unitLast] <= L'Z') ||
                                                   (value[unitLast] >= L'a' && value[unitLast] <= L'z')))
                {
                    ++unitLast;
                }

                if (leftFirst == slash || rightFirst == rightLast || unitFirst == unitLast)
                {
                    continue;
                }

                const auto current = _parseQuantity(value, leftFirst, slash, unitFirst, unitLast);
                const auto total = _parseQuantity(value, rightFirst, rightLast, unitFirst, unitLast);
                if (current.valid && total.valid && total.milliBytes > 0 && current.milliBytes <= total.milliBytes)
                {
                    return static_cast<uint8_t>((static_cast<long double>(current.milliBytes) * 100.0L) /
                                                static_cast<long double>(total.milliBytes));
                }
            }
            return std::nullopt;
        }

        static std::optional<uint8_t> _realProgress(const std::wstring_view value) noexcept
        {
            if (const auto percent = _findPercent(value))
            {
                return percent;
            }
            if (const auto quantity = _findQuantityFraction(value))
            {
                return quantity;
            }
            if (const auto sharedUnitQuantity = _findSharedUnitQuantityFraction(value))
            {
                return sharedUnitQuantity;
            }
            return _findIntegerFraction(value);
        }

        static bool _hasTransferRateAndEta(const std::wstring_view value) noexcept
        {
            const auto hasRate = _containsInsensitive(value, L"kb/s") ||
                                 _containsInsensitive(value, L"mb/s") ||
                                 _containsInsensitive(value, L"gb/s") ||
                                 _containsInsensitive(value, L"kib/s") ||
                                 _containsInsensitive(value, L"mib/s") ||
                                 _containsInsensitive(value, L"gib/s");
            return hasRate && _containsInsensitive(value, L"eta");
        }

        static bool _hasCurlMeterColumns(const std::wstring_view value, size_t cursor) noexcept
        {
            size_t numericColumns{ 1 };
            while (cursor < value.size() && numericColumns < 5)
            {
                while (cursor < value.size() && (value[cursor] == L' ' || value[cursor] == L'\t'))
                {
                    ++cursor;
                }
                if (cursor == value.size() || value[cursor] < L'0' || value[cursor] > L'9')
                {
                    return false;
                }
                while (cursor < value.size() && value[cursor] >= L'0' && value[cursor] <= L'9')
                {
                    ++cursor;
                }
                ++numericColumns;
            }
            return numericColumns >= 5;
        }

        static bool _isPreserveOnly(const std::wstring_view value) noexcept
        {
            static constexpr std::array<std::wstring_view, 28> terms{
                L"warning", L"warn", L"audit", L"error", L"fatal", L"failed", L"failure", L"traceback", L"exception", L"stack trace", L"password", L"authentication", L"permission denied", L"conflict", L"prompt", L"build success", L"build failure", L"build successful", L"successfully installed", L"saved to", L"npm err!", L"done.", L"confirm", L"continue?", L"[y/n]", L"yes/no", L"passphrase", L"username"
            };
            for (const auto term : terms)
            {
                if (_containsInsensitive(value, term))
                {
                    return true;
                }
            }
            return false;
        }

        static bool _isInteractivePrompt(const std::wstring_view value) noexcept
        {
            static constexpr std::array<std::wstring_view, 7> terms{
                L"confirm", L"continue?", L"[y/n]", L"yes/no", L"password", L"passphrase", L"username"
            };
            for (const auto term : terms)
            {
                if (_containsInsensitive(value, term))
                {
                    return true;
                }
            }
            return false;
        }

        static ProviderProgress _makeProgress(const ProgressProvider provider,
                                              const ProgressMode mode,
                                              const ProgressStatus status,
                                              const uint8_t value,
                                              const ProviderConfidence confidence,
                                              const uint16_t stage) noexcept
        {
            ProviderProgress progress;
            progress.provider = provider;
            progress.mode = mode;
            progress.status = status;
            progress.value = value;
            progress.confidence = confidence;
            progress.visible = true;
            progress.stage = stage;
            return progress;
        }

        static Match _runningMatch(const ProgressProvider provider,
                                   const std::wstring_view line,
                                   const ProviderConfidence confidence,
                                   const uint16_t stage) noexcept
        {
            if (const auto value = _realProgress(line))
            {
                return { _makeProgress(provider, ProgressMode::Determinate, ProgressStatus::Running, *value, confidence, stage), true, false };
            }
            return { _makeProgress(provider, ProgressMode::Indeterminate, ProgressStatus::Running, 0, confidence, stage), true, false };
        }

        static uint64_t _hashTokenBeforeColon(const std::wstring_view line) noexcept
        {
            const auto colon = line.find(L':');
            if (colon == std::wstring_view::npos || colon == 0 || colon > 64)
            {
                return 0;
            }
            uint64_t hash{ 1469598103934665603ull };
            for (size_t i = 0; i < colon; ++i)
            {
                const auto ch = line[i];
                if (ch == L' ' || ch == L'\t')
                {
                    return 0;
                }
                hash ^= static_cast<uint16_t>(ch);
                hash *= 1099511628211ull;
            }
            return hash == 0 ? 1 : hash;
        }

        std::optional<uint8_t> _updateDockerLayer(const std::wstring_view line,
                                                  const bool complete,
                                                  CallState& call) noexcept
        {
            const auto key = _hashTokenBeforeColon(line);
            if (key == 0)
            {
                return std::nullopt;
            }

            LayerState* selected{};
            for (auto& layer : _dockerLayers)
            {
                if (layer.used && layer.key == key)
                {
                    selected = &layer;
                    break;
                }
                if (!layer.used && !selected)
                {
                    selected = &layer;
                }
            }
            if (!selected)
            {
                call.overflow = true;
                return std::nullopt;
            }
            selected->used = true;
            selected->key = key;
            selected->complete = selected->complete || complete;

            size_t used{};
            size_t completed{};
            for (const auto& layer : _dockerLayers)
            {
                if (layer.used)
                {
                    ++used;
                    completed += layer.complete ? 1 : 0;
                }
            }
            return used == 0 ? std::nullopt :
                               std::optional<uint8_t>{ static_cast<uint8_t>((completed * 100) / used) };
        }

        Match _matchDockerPull(const std::wstring_view line, CallState& call) noexcept
        {
            const auto preserve = _isPreserveOnly(line);
            const auto layerLine = _hashTokenBeforeColon(line) != 0;
            if (_containsInsensitive(line, L"error response from daemon") ||
                _startsWithInsensitive(line, L"docker: error"))
            {
                return { _makeProgress(ProgressProvider::DockerPull, ProgressMode::Determinate, ProgressStatus::Error, 0, ProviderConfidence::High, 8), true, true };
            }
            if (layerLine && _containsInsensitive(line, L"pulling fs layer"))
            {
                _updateDockerLayer(line, false, call);
                return _runningMatch(ProgressProvider::DockerPull, line, ProviderConfidence::High, 1);
            }
            if (layerLine && _containsInsensitive(line, L": waiting"))
            {
                _updateDockerLayer(line, false, call);
                auto match = _runningMatch(ProgressProvider::DockerPull, line, ProviderConfidence::High, 2);
                match.progress.status = ProgressStatus::Waiting;
                return match;
            }
            if (layerLine && _containsInsensitive(line, L": downloading"))
            {
                _updateDockerLayer(line, false, call);
                return _runningMatch(ProgressProvider::DockerPull, line, ProviderConfidence::High, 3);
            }
            if (layerLine && _containsInsensitive(line, L"verifying checksum"))
            {
                return _runningMatch(ProgressProvider::DockerPull, line, ProviderConfidence::High, 4);
            }
            if (layerLine && _containsInsensitive(line, L"download complete"))
            {
                return _runningMatch(ProgressProvider::DockerPull, line, ProviderConfidence::High, 5);
            }
            if (layerLine && _containsInsensitive(line, L": extracting"))
            {
                return _runningMatch(ProgressProvider::DockerPull, line, ProviderConfidence::High, 6);
            }
            if (layerLine && _containsInsensitive(line, L"pull complete"))
            {
                const auto aggregate = _updateDockerLayer(line, true, call);
                return { _makeProgress(ProgressProvider::DockerPull,
                                       aggregate ? ProgressMode::Determinate : ProgressMode::Indeterminate,
                                       ProgressStatus::Running,
                                       aggregate.value_or(0),
                                       ProviderConfidence::High,
                                       7),
                         true,
                         preserve };
            }
            if (_containsInsensitive(line, L"downloaded newer image") ||
                _containsInsensitive(line, L"image is up to date"))
            {
                return { _makeProgress(ProgressProvider::DockerPull, ProgressMode::Determinate, ProgressStatus::Success, 100, ProviderConfidence::High, 8), true, true };
            }
            return {};
        }

        static std::optional<uint32_t> _buildKitStepId(const std::wstring_view line) noexcept
        {
            const auto trimmed = _trim(line);
            if (trimmed.size() < 2 || trimmed.front() != L'#')
            {
                return std::nullopt;
            }
            size_t last = 1;
            while (last < trimmed.size() && trimmed[last] >= L'0' && trimmed[last] <= L'9')
            {
                ++last;
            }
            uint64_t parsed{};
            if (!_parseUnsigned(trimmed, 1, last, parsed) || parsed > std::numeric_limits<uint32_t>::max())
            {
                return std::nullopt;
            }
            return static_cast<uint32_t>(parsed);
        }

        void _updateBuildKitStep(const uint32_t id, const bool complete, CallState& call) noexcept
        {
            StepState* selected{};
            for (auto& step : _buildKitSteps)
            {
                if (step.used && step.id == id)
                {
                    selected = &step;
                    break;
                }
                if (!step.used && !selected)
                {
                    selected = &step;
                }
            }
            if (!selected)
            {
                call.overflow = true;
                return;
            }
            selected->used = true;
            selected->id = id;
            selected->complete = selected->complete || complete;
        }

        Match _matchBuildKit(const std::wstring_view line, CallState& call) noexcept
        {
            const auto id = _buildKitStepId(line);
            if (!id)
            {
                return {};
            }
            const auto isError = _containsInsensitive(line, L" error") || _containsInsensitive(line, L"error:");
            const auto isDone = _containsInsensitive(line, L" done") || _containsInsensitive(line, L" cached");
            _updateBuildKitStep(*id, isDone, call);
            if (isError)
            {
                return { _makeProgress(ProgressProvider::DockerBuildKit, ProgressMode::Determinate, ProgressStatus::Error, 0, ProviderConfidence::High, 8), true, true };
            }
            if (isDone)
            {
                return { _makeProgress(ProgressProvider::DockerBuildKit, ProgressMode::Determinate, ProgressStatus::Running, 100, ProviderConfidence::High, 7), true, true };
            }
            uint16_t stage{ 1 };
            if (_containsInsensitive(line, L"transferring context"))
                stage = 2;
            else if (_containsInsensitive(line, L"loading metadata"))
                stage = 3;
            else if (_containsInsensitive(line, L"exporting layers"))
                stage = 4;
            else if (_containsInsensitive(line, L"writing image"))
                stage = 5;
            auto match = _runningMatch(ProgressProvider::DockerBuildKit, line, ProviderConfidence::High, stage);
            match.preserveOnly = true;
            return match;
        }

        Match _matchPip(const std::wstring_view line) const noexcept
        {
            const auto trimmed = _trim(line);
            // Once a Python archive anchors the record, either a transfer rate
            // or an ETA is sufficient. Requiring both would drop legitimate
            // pip meters that append a warning in place of the ETA.
            const auto pipTransferSignal = _containsInsensitive(line, L"kb/s") ||
                                           _containsInsensitive(line, L"mb/s") ||
                                           _containsInsensitive(line, L"gb/s") ||
                                           _containsInsensitive(line, L"kib/s") ||
                                           _containsInsensitive(line, L"mib/s") ||
                                           _containsInsensitive(line, L"gib/s") ||
                                           _containsInsensitive(line, L"eta");
            const auto transferShape = pipTransferSignal &&
                                       (_findPercent(line).has_value() ||
                                        _findQuantityFraction(line).has_value() ||
                                        _findSharedUnitQuantityFraction(line).has_value() ||
                                        _findIntegerFraction(line).has_value());
            const auto pipSignature = _containsInsensitive(line, L"pip ") ||
                                      _startsWithInsensitive(trimmed, L"collecting ") ||
                                      _containsInsensitive(line, L"installing collected packages");
            const auto pipWheel = _startsWithInsensitive(trimmed, L"downloading ") &&
                                  _containsFileExtensionInsensitive(trimmed, L".whl");
            const auto pipArchive = pipWheel ||
                                    _containsFileExtensionInsensitive(trimmed, L".tar.gz") ||
                                    _containsFileExtensionInsensitive(trimmed, L".tar.bz2") ||
                                    _containsFileExtensionInsensitive(trimmed, L".tgz") ||
                                    _containsFileExtensionInsensitive(trimmed, L".zip");
            const auto pipSizedDownload = _startsWithInsensitive(trimmed, L"downloading ") &&
                                          pipArchive &&
                                          (_containsInsensitive(trimmed, L" kb)") ||
                                           _containsInsensitive(trimmed, L" mb)") ||
                                           _containsInsensitive(trimmed, L" gb)"));
            // Modern pip emits a bounded two-record shape: a sized archive
            // announcement followed by a Rich meter. Claim the structural
            // announcement only; no package name or URL is retained.
            // Wheel names are pip-specific enough to bootstrap ownership.
            // Generic archive extensions are accepted only after an explicit
            // pip signature or an existing pip claim establishes the stream.
            const auto pipContext = _claimedProvider == ProgressProvider::Pip || pipSignature || pipWheel;
            if (pipContext &&
                (_startsWithInsensitive(trimmed, L"error:") || _containsInsensitive(line, L"subprocess-exited-with-error")))
            {
                return { _makeProgress(ProgressProvider::Pip, ProgressMode::Determinate, ProgressStatus::Error, 0, ProviderConfidence::High, 6), true, true };
            }
            if (!pipContext || (!transferShape && !pipSignature && !pipSizedDownload))
            {
                return {};
            }
            const auto hasRealProgress = _realProgress(line).has_value();
            const auto confidence = transferShape || (pipSizedDownload && hasRealProgress) ?
                                        ProviderConfidence::High :
                                        ProviderConfidence::Medium;
            auto match = _runningMatch(ProgressProvider::Pip, line, confidence, _containsInsensitive(line, L"installing") ? 2 : 1);
            match.preserveOnly = pipSizedDownload && !transferShape;
            return match;
        }

        Match _matchGit(const std::wstring_view line) const noexcept
        {
            static constexpr std::array<std::wstring_view, 5> phases{
                L"counting objects:", L"compressing objects:", L"receiving objects:", L"resolving deltas:", L"updating files:"
            };
            const auto trimmed = _trim(line);
            if ((_claimedProvider == ProgressProvider::Git || _startsWithInsensitive(trimmed, L"fatal:")) &&
                (_startsWithInsensitive(trimmed, L"fatal:") || _containsInsensitive(line, L"authentication failed")))
            {
                return { _makeProgress(ProgressProvider::Git, ProgressMode::Determinate, ProgressStatus::Error, 0, ProviderConfidence::High, 7), true, true };
            }

            auto candidate = trimmed;
            const auto remote = _startsWithInsensitive(candidate, L"remote:");
            if (remote)
            {
                candidate = _trim(candidate.substr(7));
            }
            for (uint16_t i = 0; i < phases.size(); ++i)
            {
                if (_startsWithInsensitive(candidate, phases[i]))
                {
                    auto match = _runningMatch(ProgressProvider::Git, candidate, ProviderConfidence::High, static_cast<uint16_t>(i + 1));
                    match.preserveOnly = remote || _isPreserveOnly(candidate);
                    return match;
                }
            }
            return {};
        }

        Match _matchCurl(const std::wstring_view line) noexcept
        {
            if (_containsInsensitive(line, L"% total") && _containsInsensitive(line, L"% received"))
            {
                _curlHeaderSeen = true;
                return { _makeProgress(ProgressProvider::Curl, ProgressMode::Indeterminate, ProgressStatus::Running, 0, ProviderConfidence::High, 1), true, true };
            }
            if (_startsWithInsensitive(_trim(line), L"curl: ("))
            {
                _curlHeaderSeen = false;
                return { _makeProgress(ProgressProvider::Curl, ProgressMode::Determinate, ProgressStatus::Error, 0, ProviderConfidence::High, 3), true, true };
            }
            if (!_curlHeaderSeen)
            {
                return {};
            }

            const auto trimmed = _trim(line);
            size_t last{};
            while (last < trimmed.size() && trimmed[last] >= L'0' && trimmed[last] <= L'9')
            {
                ++last;
            }
            uint64_t percent{};
            if (last == 0 || !_hasCurlMeterColumns(trimmed, last) ||
                !_parseUnsigned(trimmed, 0, last, percent) || percent > 100)
            {
                return {};
            }
            if (percent == 100)
            {
                _curlHeaderSeen = false;
            }
            return { _makeProgress(ProgressProvider::Curl,
                                   ProgressMode::Determinate,
                                   percent == 100 ? ProgressStatus::Success : ProgressStatus::Running,
                                   static_cast<uint8_t>(percent),
                                   ProviderConfidence::High,
                                   2),
                     true,
                     false };
        }

        Match _matchWget(const std::wstring_view line) noexcept
        {
            const auto trimmed = _trim(line);
            const auto explicitAnchor = _containsInsensitive(line, L"wget ") ||
                                        _startsWithInsensitive(trimmed, L"wget:") ||
                                        _startsWithInsensitive(trimmed, L"saving to:");
            _wgetAnchorSeen = _wgetAnchorSeen || explicitAnchor;
            if ((_claimedProvider == ProgressProvider::Wget || _wgetAnchorSeen) &&
                (_containsInsensitive(line, L"unable to resolve") ||
                 _containsInsensitive(line, L"certificate") ||
                 _containsInsensitive(line, L"server returned error")))
            {
                _wgetAnchorSeen = false;
                return { _makeProgress(ProgressProvider::Wget, ProgressMode::Determinate, ProgressStatus::Error, 0, ProviderConfidence::High, 3), true, true };
            }
            const auto bracket = line.find(L"%[");
            const auto bracketMeter = bracket != std::wstring_view::npos &&
                                      line.find(L']', bracket + 2) != std::wstring_view::npos;
            const auto transferShape = _findPercent(line).has_value() &&
                                       (_containsInsensitive(line, L"eta") ||
                                        _containsInsensitive(line, L"kb/s") ||
                                        _containsInsensitive(line, L"mb/s"));
            const auto wgetContext = _claimedProvider == ProgressProvider::Wget ||
                                     _wgetAnchorSeen;
            // A bracket-shaped meter by itself is not provider ownership.
            // Require a strong wget anchor before this built-in provider can
            // classify the record; otherwise the generic overlay-only path
            // remains available and the terminal text is preserved.
            if (explicitAnchor && (!transferShape || !bracketMeter))
            {
                return { _makeProgress(ProgressProvider::Wget,
                                       ProgressMode::Indeterminate,
                                       ProgressStatus::Running,
                                       0,
                                       ProviderConfidence::High,
                                       1),
                         true,
                         true };
            }
            if (!transferShape || !bracketMeter || !wgetContext)
            {
                return {};
            }
            auto match = _runningMatch(ProgressProvider::Wget, line, ProviderConfidence::High, 2);
            if (match.progress.mode == ProgressMode::Determinate && match.progress.value == 100)
            {
                match.progress.status = ProgressStatus::Success;
                _wgetAnchorSeen = false;
            }
            match.preserveOnly = _isPreserveOnly(line);
            return match;
        }

        Match _matchPackageManager(const std::wstring_view line, const ProgressProvider provider) const noexcept
        {
            std::wstring_view anchor;
            switch (provider)
            {
            case ProgressProvider::Npm:
                anchor = L"npm";
                break;
            case ProgressProvider::Pnpm:
                anchor = L"pnpm";
                break;
            case ProgressProvider::Yarn:
                anchor = L"yarn";
                break;
            default:
                return {};
            }
            const auto claimed = _claimedProvider == provider;
            if (!claimed && !_containsTokenInsensitive(line, anchor))
            {
                return {};
            }
            if (_containsInsensitive(line, L"err!") ||
                _containsInsensitive(line, L"failed") ||
                _containsInsensitive(line, L"error"))
            {
                return { _makeProgress(provider, ProgressMode::Determinate, ProgressStatus::Error, 0, ProviderConfidence::High, 8), true, true };
            }

            uint16_t stage{};
            if (_containsInsensitive(line, L"resolv"))
                stage = 1;
            else if (_containsInsensitive(line, L"fetch") || _containsInsensitive(line, L"download"))
                stage = 2;
            else if (_containsInsensitive(line, L"link"))
                stage = 3;
            else if (_containsInsensitive(line, L"build"))
                stage = 4;
            else if (_containsInsensitive(line, L"postinstall"))
                stage = 5;
            else if (_containsInsensitive(line, L"completed") || _containsInsensitive(line, L"done"))
                stage = 6;
            if (stage == 0)
            {
                return {};
            }
            auto match = _runningMatch(provider, line, claimed ? ProviderConfidence::High : ProviderConfidence::Medium, stage);
            if (stage == 6)
            {
                match.progress.mode = ProgressMode::Determinate;
                match.progress.status = ProgressStatus::Success;
                match.progress.value = 100;
            }
            match.preserveOnly = true;
            return match;
        }

        Match _matchNvm(const std::wstring_view line) const noexcept
        {
            const auto anchor = _containsInsensitive(line, L"nvm") ||
                                _containsInsensitive(line, L"downloading node.js") ||
                                _containsInsensitive(line, L"downloading npm") ||
                                _containsInsensitive(line, L"now using node");
            if (!anchor && _claimedProvider != ProgressProvider::Nvm)
            {
                return {};
            }
            if (_containsInsensitive(line, L"failed") || _containsInsensitive(line, L"error"))
            {
                return { _makeProgress(ProgressProvider::Nvm, ProgressMode::Determinate, ProgressStatus::Error, 0, ProviderConfidence::High, 7), true, true };
            }
            uint16_t stage{};
            if (_containsInsensitive(line, L"downloading node"))
                stage = 1;
            else if (_containsInsensitive(line, L"downloading npm"))
                stage = 2;
            else if (_containsInsensitive(line, L"extract"))
                stage = 3;
            else if (_containsInsensitive(line, L"install"))
                stage = 4;
            else if (_containsInsensitive(line, L"now using") || _containsInsensitive(line, L"switch"))
                stage = 5;
            if (stage == 0)
            {
                return {};
            }
            auto match = _runningMatch(ProgressProvider::Nvm, line, ProviderConfidence::High, stage);
            if (_containsInsensitive(line, L"installation complete") || _containsInsensitive(line, L"now using"))
            {
                match.progress.mode = ProgressMode::Determinate;
                match.progress.status = ProgressStatus::Success;
                match.progress.value = 100;
            }
            match.preserveOnly = true;
            return match;
        }

        Match _matchMaven(const std::wstring_view line) const noexcept
        {
            const auto trimmed = _trim(line);
            const auto taggedInfo = _startsWithInsensitive(trimmed, L"[info]");
            const auto taggedError = _startsWithInsensitive(trimmed, L"[error]");
            const auto resolverTransfer = _startsWithInsensitive(trimmed, L"downloading from ") ||
                                          _startsWithInsensitive(trimmed, L"downloaded from ") ||
                                          (taggedInfo &&
                                           (_containsInsensitive(trimmed, L"] downloading from ") ||
                                            _containsInsensitive(trimmed, L"] downloaded from ")));
            const auto resolverProgressPrefix = _startsWithInsensitive(trimmed, L"progress (") ||
                                                (taggedInfo && _containsInsensitive(trimmed, L"] progress ("));
            const auto resolverProgress = resolverProgressPrefix &&
                                          _containsInsensitive(trimmed, L"):") &&
                                          _realProgress(trimmed).has_value();
            const auto mavenAnchor = taggedInfo || taggedError || resolverTransfer || resolverProgress ||
                                     _claimedProvider == ProgressProvider::Maven;
            if (!mavenAnchor && _claimedProvider != ProgressProvider::Maven)
            {
                return {};
            }
            if (_containsInsensitive(line, L"build failure") || taggedError)
            {
                return { _makeProgress(ProgressProvider::Maven, ProgressMode::Determinate, ProgressStatus::Error, 0, ProviderConfidence::High, 8), true, true };
            }
            if (_containsInsensitive(line, L"build success"))
            {
                return { _makeProgress(ProgressProvider::Maven, ProgressMode::Determinate, ProgressStatus::Success, 100, ProviderConfidence::High, 7), true, true };
            }
            uint16_t stage{};
            if (resolverTransfer || resolverProgress)
                stage = 1;
            else if (_containsInsensitive(line, L"compile"))
                stage = 2;
            else if (_containsInsensitive(line, L"test"))
                stage = 3;
            else if (_containsInsensitive(line, L"package"))
                stage = 4;
            else if (_containsInsensitive(line, L"install"))
                stage = 5;
            if (stage == 0)
            {
                return {};
            }
            auto match = resolverProgress ?
                             _runningMatch(ProgressProvider::Maven, line, ProviderConfidence::High, stage) :
                             Match{ _makeProgress(ProgressProvider::Maven,
                                                  ProgressMode::Indeterminate,
                                                  ProgressStatus::Running,
                                                  0,
                                                  ProviderConfidence::High,
                                                  stage),
                                    true,
                                    true };
            match.preserveOnly = true;
            return match;
        }

        Match _matchGradle(const std::wstring_view line) const noexcept
        {
            const auto taskLine = _startsWithInsensitive(_trim(line), L"> task");
            const auto executingMeter = _containsInsensitive(line, L"executing") &&
                                        _realProgress(line).has_value();
            const auto anchor = executingMeter ||
                                taskLine ||
                                _containsInsensitive(line, L"gradle") ||
                                _containsInsensitive(line, L"build successful") ||
                                _containsInsensitive(line, L"build failed");
            if (!anchor && _claimedProvider != ProgressProvider::Gradle)
            {
                return {};
            }
            if (_containsInsensitive(line, L"build failed") || _containsInsensitive(line, L"failure:"))
            {
                return { _makeProgress(ProgressProvider::Gradle, ProgressMode::Determinate, ProgressStatus::Error, 0, ProviderConfidence::High, 5), true, true };
            }
            if (_containsInsensitive(line, L"build successful"))
            {
                return { _makeProgress(ProgressProvider::Gradle, ProgressMode::Determinate, ProgressStatus::Success, 100, ProviderConfidence::High, 4), true, true };
            }
            // Each record must carry build-tool evidence of its own: a status
            // meter with a real value, a wrapper download, or a task line. A
            // bare product mention, such as a directory listing entry that
            // happens to contain the word, must not start or refresh a bar,
            // and an established claim must not rematch on arbitrary later
            // records.
            uint16_t stage{};
            if (_containsInsensitive(line, L"download") && _containsInsensitive(line, L"gradle"))
            {
                stage = 2;
            }
            else if (taskLine)
            {
                stage = 3;
            }
            else if (executingMeter)
            {
                stage = 1;
            }
            if (stage == 0)
            {
                return {};
            }
            auto match = _runningMatch(ProgressProvider::Gradle, line, ProviderConfidence::High, stage);
            match.preserveOnly = true;
            return match;
        }

        static bool _isSpinner(const wchar_t value) noexcept
        {
            return value == L'|' || value == L'/' || value == L'-' || value == L'\\';
        }

        static uint64_t _genericTransientShape(const std::wstring_view value, bool& hasDigit) noexcept
        {
            uint64_t hash{ 1469598103934665603ull };
            hasDigit = false;
            for (const auto ch : value)
            {
                uint8_t category{};
                if (ch >= L'0' && ch <= L'9')
                {
                    category = 1;
                    hasDigit = true;
                }
                else if ((ch >= L'a' && ch <= L'z') || (ch >= L'A' && ch <= L'Z'))
                {
                    category = 2;
                }
                else if (ch == L' ' || ch == L'\t')
                {
                    category = 3;
                }
                else if (ch < 0x80)
                {
                    category = 4;
                }
                else
                {
                    category = 5;
                }
                hash ^= category;
                hash *= 1099511628211ull;
            }
            return hash == 0 ? 1 : hash;
        }

        ProviderConfidence _advanceGenericConfidence() noexcept
        {
            _genericMatchStreak = static_cast<uint8_t>(_genericMatchStreak < 2 ? _genericMatchStreak + 1 : 2);
            return _genericMatchStreak >= 2 ? ProviderConfidence::Medium : ProviderConfidence::Low;
        }

        void _resetGenericHeuristics() noexcept
        {
            _genericMatchStreak = 0;
            _genericTransientShapeValue = 0;
            _genericTransientShapeLength = 0;
            _genericTransientShapeStreak = 0;
        }

        Match _matchGeneric(const std::wstring_view line, const bool transientRecord) noexcept
        {
            const auto trimmed = _trim(line);
            if (trimmed.empty() || _isPreserveOnly(trimmed))
            {
                _resetGenericHeuristics();
                return {};
            }

            if (transientRecord && trimmed.size() == 1 && _isSpinner(trimmed.front()))
            {
                _genericTransientShapeValue = 0;
                _genericTransientShapeLength = 0;
                _genericTransientShapeStreak = 0;
                return { _makeProgress(ProgressProvider::Generic,
                                       ProgressMode::Indeterminate,
                                       ProgressStatus::Running,
                                       0,
                                       _advanceGenericConfidence(),
                                       2),
                         true,
                         false };
            }

            size_t first{};
            if (trimmed.size() > 1 &&
                (trimmed.front() == L'|' || trimmed.front() == L'/' ||
                 trimmed.front() == L'-' || trimmed.front() == L'\\'))
            {
                const auto signedNumber = trimmed.front() == L'-' &&
                                          trimmed[1] >= L'0' && trimmed[1] <= L'9';
                if (!signedNumber)
                {
                    first = 1;
                    while (first < trimmed.size() && trimmed[first] == L' ')
                    {
                        ++first;
                    }
                }
            }

            bool anchored{};
            size_t digits = first;
            while (digits < trimmed.size() && trimmed[digits] >= L'0' && trimmed[digits] <= L'9')
            {
                ++digits;
            }
            anchored = digits > first && digits < trimmed.size() && trimmed[digits] == L'%';
            if (!anchored)
            {
                const auto slash = trimmed.find(L'/', first);
                anchored = slash != std::wstring_view::npos && slash > first &&
                           (_findIntegerFraction(trimmed.substr(first)).has_value() ||
                            _findQuantityFraction(trimmed.substr(first)).has_value() ||
                            _findSharedUnitQuantityFraction(trimmed.substr(first)).has_value());
            }
            const auto realProgress = _realProgress(trimmed);
            const auto transferRateAndEta = _hasTransferRateAndEta(trimmed);
            const auto bracketMeter = trimmed.find(L"%[") != std::wstring_view::npos &&
                                      trimmed.find(L']') != std::wstring_view::npos;
            if (anchored && realProgress)
            {
                _genericTransientShapeValue = 0;
                _genericTransientShapeLength = 0;
                _genericTransientShapeStreak = 0;
                auto match = Match{ _makeProgress(ProgressProvider::Generic,
                                                  ProgressMode::Determinate,
                                                  ProgressStatus::Running,
                                                  *realProgress,
                                                  _advanceGenericConfidence(),
                                                  1),
                                    true,
                                    false };
                match.progress.suppressible = false;
                return match;
            }

            // A bracket meter with rate and ETA is a wget-shaped record. It is
            // only trustworthy after wget has emitted its provider anchor; do
            // not let the generic fallback claim unrelated bracketed output.
            if (bracketMeter)
            {
                _resetGenericHeuristics();
                return {};
            }

            if (transferRateAndEta)
            {
                _genericTransientShapeValue = 0;
                _genericTransientShapeLength = 0;
                _genericTransientShapeStreak = 0;
                return { _makeProgress(ProgressProvider::Generic,
                                       realProgress ? ProgressMode::Determinate : ProgressMode::Indeterminate,
                                       ProgressStatus::Running,
                                       realProgress.value_or(0),
                                       _advanceGenericConfidence(),
                                       3),
                         true,
                         false };
            }

            if (transientRecord && trimmed.size() >= 3)
            {
                bool hasDigit{};
                const auto shape = _genericTransientShape(trimmed, hasDigit);
                if (hasDigit)
                {
                    if (_genericTransientShapeValue == shape && _genericTransientShapeLength == trimmed.size())
                    {
                        _genericTransientShapeStreak = static_cast<uint8_t>(
                            _genericTransientShapeStreak < 2 ? _genericTransientShapeStreak + 1 : 2);
                    }
                    else
                    {
                        _genericTransientShapeValue = shape;
                        _genericTransientShapeLength = static_cast<uint16_t>(trimmed.size());
                        _genericTransientShapeStreak = 1;
                    }

                    if (_genericTransientShapeStreak >= 2)
                    {
                        return { _makeProgress(ProgressProvider::Generic,
                                               ProgressMode::Indeterminate,
                                               ProgressStatus::Running,
                                               0,
                                               _advanceGenericConfidence(),
                                               4),
                                 true,
                                 false };
                    }
                    return { {}, false, false, true };
                }
            }

            _resetGenericHeuristics();
            return {};
        }

        bool _hasActiveHighConfidenceBuiltInClaim() const noexcept
        {
            return _claimedProvider != ProgressProvider::None &&
                   _claimedProvider != ProgressProvider::Generic &&
                   _claimedProviderConfidence == ProviderConfidence::High;
        }

        static ProviderProgress _providerClear() noexcept
        {
            ProviderProgress progress;
            progress.provider = ProgressProvider::None;
            progress.mode = ProgressMode::Hidden;
            progress.status = ProgressStatus::Cancelled;
            progress.confidence = ProviderConfidence::None;
            progress.visible = false;
            return progress;
        }

        Match _recognize(const std::wstring_view line,
                         CallState& call,
                         const bool transientRecord) noexcept
        {
            // Interactive text is an output barrier, even when it happens to
            // contain a percentage or transfer-rate shape.
            if (_isInteractivePrompt(line))
            {
                _resetGenericHeuristics();
                return {};
            }

            Match match;
            switch (_claimedProvider)
            {
            case ProgressProvider::DockerPull:
                match = _matchDockerPull(line, call);
                break;
            case ProgressProvider::DockerBuildKit:
                match = _matchBuildKit(line, call);
                break;
            case ProgressProvider::Pip:
                match = _matchPip(line);
                break;
            case ProgressProvider::Git:
                match = _matchGit(line);
                break;
            case ProgressProvider::Curl:
                match = _matchCurl(line);
                break;
            case ProgressProvider::Wget:
                match = _matchWget(line);
                break;
            case ProgressProvider::Npm:
            case ProgressProvider::Pnpm:
            case ProgressProvider::Yarn:
                match = _matchPackageManager(line, _claimedProvider);
                break;
            case ProgressProvider::Nvm:
                match = _matchNvm(line);
                break;
            case ProgressProvider::Maven:
                match = _matchMaven(line);
                break;
            case ProgressProvider::Gradle:
                match = _matchGradle(line);
                break;
            default:
                break;
            }
            if (match.matched)
            {
                return match;
            }

            if ((match = _matchBuildKit(line, call)).matched ||
                (match = _matchDockerPull(line, call)).matched ||
                (match = _matchGit(line)).matched ||
                (match = _matchCurl(line)).matched ||
                (match = _matchWget(line)).matched ||
                (match = _matchNvm(line)).matched ||
                (match = _matchPip(line)).matched ||
                (match = _matchMaven(line)).matched ||
                (match = _matchGradle(line)).matched ||
                (match = _matchPackageManager(line, ProgressProvider::Pnpm)).matched ||
                (match = _matchPackageManager(line, ProgressProvider::Yarn)).matched ||
                (match = _matchPackageManager(line, ProgressProvider::Npm)).matched)
            {
                return match;
            }

            auto generic = _matchGeneric(line, transientRecord);
            // Generic recognition is a fallback, not a new owner. A valid
            // high-confidence built-in claim survives another progress-shaped
            // record, but an ordinary record clears ownership below so a
            // later command cannot inherit suppression authority.
            if (_hasActiveHighConfidenceBuiltInClaim() &&
                (generic.matched || generic.pendingCandidate))
            {
                return { {}, false, false, true };
            }
            return generic;
        }

        static bool _sameProgress(const ProviderProgress& left, const ProviderProgress& right) noexcept
        {
            return left.provider == right.provider &&
                   left.mode == right.mode &&
                   left.status == right.status &&
                   left.value == right.value &&
                   left.confidence == right.confidence &&
                   left.visible == right.visible &&
                   left.transient == right.transient &&
                   left.suppressible == right.suppressible &&
                   left.stage == right.stage;
        }

        void _rememberProgress(ProviderProgress progress) noexcept
        {
            if (_lastSeen && _sameProgress(*_lastSeen, progress))
            {
                return;
            }
            progress.sequence = ++_sequence;
            _lastSeen = progress;
            _pendingPublication = progress;
            _recentProgress[_recentProgressNext] = progress;
            _recentProgressNext = (_recentProgressNext + 1) % _recentProgress.size();
            if (_recentProgressCount < _recentProgress.size())
            {
                ++_recentProgressCount;
            }
        }

        void _acceptHeldCarriageReturn(CallState& call) noexcept
        {
            if (_pendingCarriageReturnProgress)
            {
                _rememberProgress(*_pendingCarriageReturnProgress);
                ++call.recognizedRecords;
                _pendingCarriageReturnProgress.reset();
            }
        }

        std::optional<ProviderProgress> _takePublication(const uint64_t timestampMilliseconds) noexcept
        {
            if (!_pendingPublication)
            {
                return std::nullopt;
            }

            const auto terminalState = _pendingPublication->status == ProgressStatus::Success ||
                                       _pendingPublication->status == ProgressStatus::Error ||
                                       _pendingPublication->status == ProgressStatus::Cancelled;
            const auto due = !_hasPublished ||
                             timestampMilliseconds < _lastPublishedTimestamp ||
                             timestampMilliseconds - _lastPublishedTimestamp >= PublicationIntervalMilliseconds;
            if (!due && !terminalState)
            {
                return std::nullopt;
            }

            auto latest = _pendingPublication;
            _pendingPublication.reset();
            _lastPublishedTimestamp = timestampMilliseconds;
            _hasPublished = true;
            return latest;
        }

        void _finalizeRecord(const RecordEnding ending,
                             const bool ambiguousCarriageReturn,
                             CallState& call) noexcept
        {
            const auto line = _trim(std::wstring_view{ _line.data(), _lineLength });
            if (!line.empty())
            {
                ++call.nonEmptyRecords;
                // Once a record exceeds a hard bound or contains malformed
                // UTF-16, its retained prefix is not a valid recognition
                // candidate. Preserve it and resume only at the next record.
                const auto transientRecord = ending == RecordEnding::CarriageReturn || _recordHadEraseLine;
                auto match = _recordOverflow || _recordMalformed ? Match{} : _recognize(line, call, transientRecord);
                if (match.matched)
                {
                    _unmatchedRecordStreak = 0;
                    auto& progress = match.progress;
                    const auto preserveOnly = match.preserveOnly || _isPreserveOnly(line);
                    progress.transient = ending == RecordEnding::CarriageReturn || _recordHadEraseLine;
                    progress.suppressible = progress.transient &&
                                            progress.mode == ProgressMode::Determinate &&
                                            progress.confidence == ProviderConfidence::High &&
                                            !preserveOnly &&
                                            !_recordUnsafe &&
                                            !_recordMalformed &&
                                            !_recordOverflow &&
                                            (progress.provider == ProgressProvider::Pip ||
                                             progress.provider == ProgressProvider::Git ||
                                             progress.provider == ProgressProvider::Curl ||
                                             progress.provider == ProgressProvider::Wget);

                    const auto terminalState = progress.status == ProgressStatus::Success ||
                                               progress.status == ProgressStatus::Error ||
                                               progress.status == ProgressStatus::Cancelled;
                    const auto failedTerminalState = progress.status == ProgressStatus::Error ||
                                                     progress.status == ProgressStatus::Cancelled;
                    if (failedTerminalState)
                    {
                        // Preserve the failure publication below, but do not
                        // let ownership or bounded provider tables leak into a
                        // later command after Error/Cancelled.
                        _clearProviderContext();
                    }
                    else if (progress.provider != ProgressProvider::Generic && !terminalState)
                    {
                        _claimedProvider = progress.provider;
                        _claimedProviderConfidence = progress.confidence;
                    }
                    else
                    {
                        _claimedProvider = ProgressProvider::None;
                        _claimedProviderConfidence = ProviderConfidence::None;
                    }

                    if (ambiguousCarriageReturn)
                    {
                        _pendingCarriageReturnProgress = progress;
                        call.ambiguousCarriageReturn = true;
                    }
                    else
                    {
                        _rememberProgress(progress);
                        ++call.recognizedRecords;
                    }

                    if (progress.suppressible && !ambiguousCarriageReturn)
                    {
                        call.suppressionCandidate = progress;
                    }
                    else
                    {
                        call.onlySafeContent = false;
                    }
                }
                else
                {
                    call.onlySafeContent = false;
                    const auto lastVisible = _lastSeen && _lastSeen->visible;
                    const auto lastWasTerminal = lastVisible &&
                                                 (_lastSeen->status == ProgressStatus::Success ||
                                                  _lastSeen->status == ProgressStatus::Error ||
                                                  _lastSeen->status == ProgressStatus::Cancelled);
                    const auto lastWasGeneric = lastVisible &&
                                                _lastSeen->provider == ProgressProvider::Generic;
                    if (lastWasGeneric)
                    {
                        // A structural clear is terminal-state exempt from the
                        // publication throttle and contains no output text.
                        _rememberProgress(_providerClear());
                    }
                    if (!match.pendingCandidate)
                    {
                        // A progress-shaped record keeps a live claim; only a
                        // plainly ordinary record advances toward the clear.
                        _unmatchedRecordStreak = static_cast<uint8_t>(
                            _unmatchedRecordStreak < 2 ? _unmatchedRecordStreak + 1 : 2);
                        // A built-in provider tolerates one ordinary record so
                        // an informational line inside a live meter stream does
                        // not blank the bar, but a still-running bar whose
                        // stream has moved on must not animate indefinitely.
                        // Success and Error are final results and remain until
                        // a later publication replaces them.
                        if (lastVisible && !lastWasGeneric && !lastWasTerminal &&
                            _unmatchedRecordStreak >= 2)
                        {
                            _rememberProgress(_providerClear());
                        }
                        _resetGenericHeuristics();
                        _clearProviderContext();
                    }
                }
            }

            if (_recordUnsafe || _recordMalformed || _recordOverflow)
            {
                call.onlySafeContent = false;
            }
            if (_recordMalformed)
            {
                call.malformed = true;
            }
            if (_recordOverflow)
            {
                call.overflow = true;
            }
            if (ending == RecordEnding::Newline)
            {
                call.sawNewline = true;
                call.onlySafeContent = false;
            }
            _clearRecord();
        }

        void _appendCodeUnit(const wchar_t codeUnit, CallState& call) noexcept
        {
            if (_lineLength >= _line.size())
            {
                _recordOverflow = true;
                call.overflow = true;
                call.onlySafeContent = false;
                return;
            }
            _line[_lineLength++] = codeUnit;
            if (codeUnit >= L' ')
            {
                _atColumnZero = false;
            }
        }

        void _beginAnsi(const AnsiState state, const wchar_t introducer, CallState& call) noexcept
        {
            _ansiState = state;
            _ansiLength = 0;
            _ansi[_ansiLength++] = introducer;
            if (state == AnsiState::Osc)
            {
                _recordUnsafe = true;
                call.onlySafeContent = false;
            }
        }

        void _appendAnsi(const wchar_t codeUnit, CallState& call) noexcept
        {
            if (_ansiLength >= _ansi.size())
            {
                call.overflow = true;
                call.onlySafeContent = false;
                _recordOverflow = true;
                _ansiState = _ansiState == AnsiState::Osc || _ansiState == AnsiState::OscEscape ?
                                 AnsiState::OscDiscard :
                                 AnsiState::CsiDiscard;
                return;
            }
            _ansi[_ansiLength++] = codeUnit;
        }

        void _finishCsi(CallState& call) noexcept
        {
            if (_ansiLength == 0)
            {
                _recordMalformed = true;
                call.malformed = true;
                call.onlySafeContent = false;
                _healthy = false;
                _ansiState = AnsiState::Ground;
                return;
            }
            const auto final = _ansi[_ansiLength - 1];
            if (final == L'm')
            {
                // SGR changes presentation state. It remains recognizable,
                // but hiding the callback would change subsequent rendering.
                _recordUnsafe = true;
                call.onlySafeContent = false;
            }
            else if (final == L'K')
            {
                const auto parameterFirst = _ansi.front() == 0x1b ? 2u : 1u;
                const auto parameterCount = _ansiLength - parameterFirst - 1;
                const auto supported = parameterCount == 0 ||
                                       (parameterCount == 1 &&
                                        _ansi[parameterFirst] >= L'0' &&
                                        _ansi[parameterFirst] <= L'2');
                if (supported)
                {
                    _recordHadEraseLine = true;
                }
                else
                {
                    _recordUnsafe = true;
                    call.onlySafeContent = false;
                }
            }
            else
            {
                // Valid but cursor-affecting or otherwise unmodelled CSI is
                // overlay-only for the entire record.
                _recordUnsafe = true;
                call.onlySafeContent = false;
                _columnKnown = false;
                _atColumnZero = false;
            }
            _ansiState = AnsiState::Ground;
            _ansiLength = 0;
        }

        void _consumeCodeUnit(const wchar_t codeUnit, const bool atEnd, CallState& call) noexcept
        {
            switch (_ansiState)
            {
            case AnsiState::Ground:
                if (codeUnit == 0x1b)
                {
                    _beginAnsi(AnsiState::Escape, codeUnit, call);
                }
                else if (codeUnit == 0x9b)
                {
                    _beginAnsi(AnsiState::Csi, codeUnit, call);
                }
                else if (codeUnit == 0x9d)
                {
                    _beginAnsi(AnsiState::Osc, codeUnit, call);
                }
                else if (codeUnit == L'\r')
                {
                    _finalizeRecord(RecordEnding::CarriageReturn, atEnd, call);
                    _columnKnown = true;
                    _atColumnZero = true;
                    _previousChunkEndedWithCarriageReturn = atEnd;
                }
                else if (codeUnit == L'\n')
                {
                    _finalizeRecord(RecordEnding::Newline, false, call);
                    // LF does not necessarily imply carriage return in every
                    // terminal mode. Retain zero only when it was already known.
                    if (!_atColumnZero)
                    {
                        _columnKnown = false;
                    }
                }
                else if (codeUnit < L' ' || codeUnit == 0x7f)
                {
                    _recordUnsafe = true;
                    call.onlySafeContent = false;
                    if (codeUnit == L'\b' || codeUnit == L'\t')
                    {
                        _columnKnown = false;
                        _atColumnZero = false;
                    }
                }
                else
                {
                    _appendCodeUnit(codeUnit, call);
                }
                break;

            case AnsiState::Escape:
                _appendAnsi(codeUnit, call);
                if (codeUnit == L'[')
                {
                    _ansiState = AnsiState::Csi;
                }
                else if (codeUnit == L']')
                {
                    _ansiState = AnsiState::Osc;
                    _recordUnsafe = true;
                    call.onlySafeContent = false;
                }
                else
                {
                    _recordUnsafe = true;
                    call.onlySafeContent = false;
                    _columnKnown = false;
                    _atColumnZero = false;
                    _ansiState = AnsiState::Ground;
                    _ansiLength = 0;
                }
                break;

            case AnsiState::Csi:
                _appendAnsi(codeUnit, call);
                if (_ansiState == AnsiState::CsiDiscard)
                {
                    break;
                }
                if (codeUnit >= 0x40 && codeUnit <= 0x7e)
                {
                    _finishCsi(call);
                }
                else if (codeUnit < 0x20 || codeUnit > 0x3f)
                {
                    _recordMalformed = true;
                    call.malformed = true;
                    call.onlySafeContent = false;
                    _healthy = false;
                    _ansiState = AnsiState::Ground;
                    _ansiLength = 0;
                }
                break;

            case AnsiState::CsiDiscard:
                if (codeUnit >= 0x40 && codeUnit <= 0x7e)
                {
                    _ansiState = AnsiState::Ground;
                    _ansiLength = 0;
                }
                break;

            case AnsiState::Osc:
                _appendAnsi(codeUnit, call);
                if (_ansiState == AnsiState::OscDiscard)
                {
                    break;
                }
                if (codeUnit == 0x07)
                {
                    _ansiState = AnsiState::Ground;
                    _ansiLength = 0;
                }
                else if (codeUnit == 0x1b)
                {
                    _ansiState = AnsiState::OscEscape;
                }
                break;

            case AnsiState::OscEscape:
                _appendAnsi(codeUnit, call);
                if (_ansiState == AnsiState::OscDiscard)
                {
                    break;
                }
                if (codeUnit == L'\\')
                {
                    _ansiState = AnsiState::Ground;
                    _ansiLength = 0;
                }
                else
                {
                    _ansiState = AnsiState::Osc;
                }
                break;

            case AnsiState::OscDiscard:
                if (codeUnit == 0x07)
                {
                    _ansiState = AnsiState::Ground;
                    _ansiLength = 0;
                }
                else if (codeUnit == 0x1b)
                {
                    _ansiState = AnsiState::OscDiscardEscape;
                }
                break;

            case AnsiState::OscDiscardEscape:
                _ansiState = codeUnit == L'\\' ? AnsiState::Ground : AnsiState::OscDiscard;
                if (_ansiState == AnsiState::Ground)
                {
                    _ansiLength = 0;
                }
                break;
            }
        }

        void _clearRecord() noexcept
        {
            _lineLength = 0;
            _recordHadEraseLine = false;
            _recordUnsafe = false;
            _recordMalformed = false;
            _recordOverflow = false;
        }

        void _clearProviderContext() noexcept
        {
            _claimedProvider = ProgressProvider::None;
            _claimedProviderConfidence = ProviderConfidence::None;
            _curlHeaderSeen = false;
            _wgetAnchorSeen = false;
            _resetGenericHeuristics();
            _dockerLayers = {};
            _buildKitSteps = {};
        }

        void _resetUnderLock() noexcept
        {
            _clearRecord();
            _ansiState = AnsiState::Ground;
            _ansiLength = 0;
            _pendingHighSurrogate = false;
            _previousChunkEndedWithCarriageReturn = false;
            _pendingCarriageReturnProgress.reset();
            _healthy = true;
            // A reset can follow skipped, malformed, or alternate-screen
            // output. The cursor is unknown until rendered output proves it.
            _columnKnown = false;
            _atColumnZero = false;
            _unmatchedRecordStreak = 0;
            _clearProviderContext();
            _recentProgress = {};
            _recentProgressNext = 0;
            _recentProgressCount = 0;
            _lastSeen.reset();
            _pendingPublication.reset();
            _lastPublishedTimestamp = 0;
            _hasPublished = false;
            _sequence = 0;
            _desynchronized.store(false, std::memory_order_release);
        }

        std::mutex _mutex;
        std::atomic<bool> _desynchronized{ false };

        std::array<wchar_t, MaxCurrentLineCodeUnits> _line{};
        size_t _lineLength{};
        std::array<wchar_t, MaxAnsiSequenceCodeUnits> _ansi{};
        size_t _ansiLength{};
        AnsiState _ansiState{ AnsiState::Ground };
        bool _recordHadEraseLine{};
        bool _recordUnsafe{};
        bool _recordMalformed{};
        bool _recordOverflow{};
        bool _pendingHighSurrogate{};
        wchar_t _pendingHighSurrogateValue{};
        bool _previousChunkEndedWithCarriageReturn{};
        std::optional<ProviderProgress> _pendingCarriageReturnProgress;
        bool _healthy{ true };
        bool _columnKnown{};
        bool _atColumnZero{};

        ProgressProvider _claimedProvider{ ProgressProvider::None };
        ProviderConfidence _claimedProviderConfidence{ ProviderConfidence::None };
        bool _curlHeaderSeen{};
        bool _wgetAnchorSeen{};
        uint8_t _genericMatchStreak{};
        uint64_t _genericTransientShapeValue{};
        uint16_t _genericTransientShapeLength{};
        uint8_t _genericTransientShapeStreak{};
        // Consecutive non-empty records that matched no provider. Survives
        // _clearProviderContext so the count can reach the structural-clear
        // threshold across the context drop that the first ordinary record
        // already performs.
        uint8_t _unmatchedRecordStreak{};
        std::array<LayerState, DockerLayerCapacity> _dockerLayers{};
        std::array<StepState, BuildKitStepCapacity> _buildKitSteps{};
        std::array<ProviderProgress, RecentProgressCapacity> _recentProgress{};
        size_t _recentProgressNext{};
        size_t _recentProgressCount{};

        std::optional<ProviderProgress> _lastSeen;
        std::optional<ProviderProgress> _pendingPublication;
        uint64_t _lastPublishedTimestamp{};
        bool _hasPublished{};
        uint32_t _sequence{};
    };

    // A bounded diagnostic/test adapter for callers that have UTF-8 rather
    // than the production UTF-16 TerminalOutput stream. It retains only the
    // scalar decoder state (at most three outstanding continuation bytes) and
    // stages at most one RecognitionEngine chunk. Decode errors and bound
    // violations reset recognition state and always fail open.
    class Utf8RecognitionAdapter final
    {
    public:
        static constexpr size_t MaxChunkBytes = RecognitionEngine::MaxChunkCodeUnits * 4;

        explicit Utf8RecognitionAdapter(RecognitionEngine& engine) noexcept :
            _engine{ engine }
        {
        }

        Utf8RecognitionAdapter(const Utf8RecognitionAdapter&) = delete;
        Utf8RecognitionAdapter& operator=(const Utf8RecognitionAdapter&) = delete;

        RecognitionResult Consume(const std::string_view bytes,
                                  const uint64_t timestampMilliseconds,
                                  const RecognitionOptions options = {}) noexcept
        {
            if (_failed)
            {
                return _failedResult(false);
            }
            if (bytes.size() > MaxChunkBytes)
            {
                return _failOpen(true);
            }

            const auto scalarSpansChunks = _continuationsRemaining != 0;
            size_t decodedLength{};
            for (const auto value : bytes)
            {
                const auto byte = static_cast<uint8_t>(static_cast<unsigned char>(value));
                if (_continuationsRemaining == 0)
                {
                    if (byte <= 0x7f)
                    {
                        if (!_appendScalar(byte, decodedLength))
                        {
                            return _failOpen(true);
                        }
                    }
                    else if (byte >= 0xc2 && byte <= 0xdf)
                    {
                        _scalar = byte & 0x1f;
                        _minimumScalar = 0x80;
                        _continuationsRemaining = 1;
                    }
                    else if (byte >= 0xe0 && byte <= 0xef)
                    {
                        _scalar = byte & 0x0f;
                        _minimumScalar = 0x800;
                        _continuationsRemaining = 2;
                    }
                    else if (byte >= 0xf0 && byte <= 0xf4)
                    {
                        _scalar = byte & 0x07;
                        _minimumScalar = 0x10000;
                        _continuationsRemaining = 3;
                    }
                    else
                    {
                        return _failOpen(false);
                    }
                    continue;
                }

                if ((byte & 0xc0) != 0x80)
                {
                    return _failOpen(false);
                }

                _scalar = (_scalar << 6) | (byte & 0x3f);
                --_continuationsRemaining;
                if (_continuationsRemaining == 0)
                {
                    const auto malformed = _scalar < _minimumScalar ||
                                           _scalar > 0x10ffff ||
                                           (_scalar >= 0xd800 && _scalar <= 0xdfff);
                    if (malformed)
                    {
                        return _failOpen(false);
                    }
                    if (!_appendScalar(_scalar, decodedLength))
                    {
                        return _failOpen(true);
                    }
                    _scalar = 0;
                    _minimumScalar = 0;
                }
            }

            if (decodedLength == 0)
            {
                // Empty input and a partial scalar are both healthy so far.
                // Finish() distinguishes a truncated scalar at end-of-stream.
                return {};
            }

            auto safeOptions = options;
            const auto scalarStillIncomplete = _continuationsRemaining != 0;
            if (scalarSpansChunks || scalarStillIncomplete)
            {
                // A scalar whose bytes cross callback boundaries can be
                // recognized, but never qualifies for whole-callback hiding.
                safeOptions.replacementEnabled = false;
            }

            auto result = _engine.Consume(std::wstring_view{ _decoded.data(), decodedLength },
                                          timestampMilliseconds,
                                          safeOptions);
            if (!result.accepted)
            {
                _failed = true;
                _resetDecoder();
            }
            if (scalarSpansChunks || scalarStillIncomplete)
            {
                result.suppressInput = false;
            }
            return result;
        }

        // Marks the end of a UTF-8 stream. An unfinished scalar is malformed
        // and therefore invalidates recognition while preserving terminal data.
        RecognitionResult Finish() noexcept
        {
            if (_failed)
            {
                return _failedResult(false);
            }
            if (_continuationsRemaining == 0)
            {
                return {};
            }
            return _failOpen(false);
        }

        void Reset() noexcept
        {
            _engine.Reset();
            _resetDecoder();
            _failed = false;
        }

        void Clear() noexcept
        {
            Reset();
        }

        bool TryReset() noexcept
        {
            if (!_engine.TryReset())
            {
                return false;
            }
            _resetDecoder();
            _failed = false;
            return true;
        }

    private:
        bool _appendScalar(const uint32_t scalar, size_t& length) noexcept
        {
            if (scalar <= 0xffff)
            {
                if (length >= _decoded.size())
                {
                    return false;
                }
                _decoded[length++] = static_cast<wchar_t>(scalar);
                return true;
            }

            if (length > _decoded.size() - 2)
            {
                return false;
            }
            const auto supplementary = scalar - 0x10000;
            _decoded[length++] = static_cast<wchar_t>(0xd800 + (supplementary >> 10));
            _decoded[length++] = static_cast<wchar_t>(0xdc00 + (supplementary & 0x3ff));
            return true;
        }

        RecognitionResult _failOpen(const bool overflow) noexcept
        {
            _failed = true;
            _resetDecoder();
            static_cast<void>(_engine.TryReset());

            return _failedResult(overflow);
        }

        static RecognitionResult _failedResult(const bool overflow) noexcept
        {
            RecognitionResult result;
            result.overflow = overflow;
            result.healthy = false;
            result.accepted = false;
            return result;
        }

        void _resetDecoder() noexcept
        {
            _scalar = 0;
            _minimumScalar = 0;
            _continuationsRemaining = 0;
        }

        RecognitionEngine& _engine;
        std::array<wchar_t, RecognitionEngine::MaxChunkCodeUnits> _decoded{};
        uint32_t _scalar{};
        uint32_t _minimumScalar{};
        uint8_t _continuationsRemaining{};
        bool _failed{};
    };
}
