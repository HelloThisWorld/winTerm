// Copyright (c) winTerm contributors.
// Licensed under the MIT license.

#include "pch.h"
#include "DockDragSession.h"

#include <array>
#include <bcrypt.h>
#include <cmath>
#include <iomanip>
#include <sstream>

#pragma comment(lib, "bcrypt.lib")

using namespace winTerm::Docking;

bool DragThreshold::Exceeded(
    const DragPoint pressed,
    const DragPoint current,
    const std::chrono::milliseconds held,
    const DragThresholdSettings& settings) noexcept
{
    if (!std::isfinite(pressed.x) || !std::isfinite(pressed.y) ||
        !std::isfinite(current.x) || !std::isfinite(current.y) ||
        !std::isfinite(settings.dpiScale) || settings.dpiScale <= 0)
    {
        return false;
    }
    if (settings.touch && held < settings.touchLongPress)
    {
        return false;
    }
    const auto horizontal = std::max(1.0, settings.horizontalPixels * settings.dpiScale);
    const auto vertical = std::max(1.0, settings.verticalPixels * settings.dpiScale);
    return std::abs(current.x - pressed.x) >= horizontal ||
           std::abs(current.y - pressed.y) >= vertical;
}

DragPayloadRegistry::DragPayloadRegistry(const std::chrono::milliseconds lifetime) :
    _lifetime{ lifetime > std::chrono::milliseconds::zero() ? lifetime : std::chrono::seconds{ 30 } }
{
}

std::string DragPayloadRegistry::Register(
    std::string sourceInstanceId,
    DockSource source,
    const DragCapability capability,
    const std::chrono::steady_clock::time_point now)
{
    if (sourceInstanceId.empty())
    {
        return {};
    }
    std::scoped_lock lock{ _mutex };
    for (size_t attempt = 0; attempt < 4; ++attempt)
    {
        auto token = _newToken();
        if (token.empty() || _records.contains(token))
        {
            continue;
        }
        if (_records.emplace(token, DragPayloadRecord{
                                        std::move(sourceInstanceId),
                                        std::move(source),
                                        capability,
                                        now + _lifetime })
                .second)
        {
            return token;
        }
    }
    return {};
}

std::optional<DragPayloadRecord> DragPayloadRegistry::Resolve(
    const std::string_view token,
    const std::string_view sourceInstanceId,
    const DragCapability capability,
    const std::chrono::steady_clock::time_point now) const
{
    std::scoped_lock lock{ _mutex };
    const auto found = _records.find(std::string{ token });
    if (found == _records.end() ||
        found->second.expiresAt <= now ||
        found->second.sourceInstanceId != sourceInstanceId ||
        found->second.capability != capability)
    {
        return std::nullopt;
    }
    return found->second;
}

std::optional<DragPayloadRecord> DragPayloadRegistry::Consume(
    const std::string_view token,
    const std::string_view sourceInstanceId,
    const DragCapability capability,
    const std::chrono::steady_clock::time_point now)
{
    std::scoped_lock lock{ _mutex };
    const auto found = _records.find(std::string{ token });
    if (found == _records.end() ||
        found->second.expiresAt <= now ||
        found->second.sourceInstanceId != sourceInstanceId ||
        found->second.capability != capability)
    {
        if (found != _records.end() && found->second.expiresAt <= now)
        {
            _records.erase(found);
        }
        return std::nullopt;
    }
    auto result = std::move(found->second);
    _records.erase(found);
    return result;
}

bool DragPayloadRegistry::Invalidate(const std::string_view token) noexcept
{
    std::scoped_lock lock{ _mutex };
    return _records.erase(std::string{ token }) != 0;
}

size_t DragPayloadRegistry::PurgeExpired(const std::chrono::steady_clock::time_point now) noexcept
{
    size_t removed{};
    std::scoped_lock lock{ _mutex };
    for (auto iterator = _records.begin(); iterator != _records.end();)
    {
        if (iterator->second.expiresAt <= now)
        {
            iterator = _records.erase(iterator);
            ++removed;
        }
        else
        {
            ++iterator;
        }
    }
    return removed;
}

size_t DragPayloadRegistry::Size() const noexcept
{
    std::scoped_lock lock{ _mutex };
    return _records.size();
}

std::string DragPayloadRegistry::_newToken()
{
    std::array<uint32_t, 8> nonce{};
    if (!BCRYPT_SUCCESS(BCryptGenRandom(
            nullptr,
            reinterpret_cast<PUCHAR>(nonce.data()),
            gsl::narrow<ULONG>(sizeof(nonce)),
            BCRYPT_USE_SYSTEM_PREFERRED_RNG)))
    {
        return {};
    }
    std::ostringstream stream;
    stream << std::hex << std::setfill('0');
    for (const auto value : nonce)
    {
        stream << std::setw(8) << value;
    }
    return stream.str();
}

DockDragState DockDragStateMachine::State() const noexcept
{
    return _state;
}

DragCancellationReason DockDragStateMachine::CancellationReason() const noexcept
{
    return _cancellationReason;
}

std::string_view DockDragStateMachine::FailureReason() const noexcept
{
    return _failureReason;
}

bool DockDragStateMachine::Transition(const DockDragState next)
{
    if (!CanTransition(_state, next))
    {
        return false;
    }
    _state = next;
    return true;
}

bool DockDragStateMachine::Cancel(const DragCancellationReason reason)
{
    if (!CanTransition(_state, DockDragState::Cancelled))
    {
        return false;
    }
    _cancellationReason = reason;
    _state = DockDragState::Cancelled;
    return true;
}

bool DockDragStateMachine::Fail(std::string reason)
{
    if (!CanTransition(_state, DockDragState::Failed))
    {
        return false;
    }
    _failureReason = std::move(reason);
    _state = DockDragState::Failed;
    return true;
}

bool DockDragStateMachine::Reset()
{
    if (!CanTransition(_state, DockDragState::Idle))
    {
        return false;
    }
    _state = DockDragState::Idle;
    _cancellationReason = DragCancellationReason::None;
    _failureReason.clear();
    return true;
}

bool DockDragStateMachine::CanTransition(
    const DockDragState current,
    const DockDragState next) noexcept
{
    switch (current)
    {
    case DockDragState::Idle:
        return next == DockDragState::PointerPressed;
    case DockDragState::PointerPressed:
        return next == DockDragState::DragPending ||
               next == DockDragState::Cancelled;
    case DockDragState::DragPending:
        return next == DockDragState::Dragging ||
               next == DockDragState::Cancelled;
    case DockDragState::Dragging:
        return next == DockDragState::TargetAcquired ||
               next == DockDragState::Cancelled;
    case DockDragState::TargetAcquired:
        return next == DockDragState::Dragging ||
               next == DockDragState::DropPending ||
               next == DockDragState::Cancelled;
    case DockDragState::DropPending:
        return next == DockDragState::Committing ||
               next == DockDragState::Cancelled;
    case DockDragState::Committing:
        return next == DockDragState::Completed ||
               next == DockDragState::Failed;
    case DockDragState::Failed:
        return next == DockDragState::Cancelled;
    case DockDragState::Completed:
    case DockDragState::Cancelled:
        return next == DockDragState::Idle;
    default:
        return false;
    }
}
