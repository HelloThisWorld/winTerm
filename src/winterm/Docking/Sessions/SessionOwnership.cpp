// Copyright (c) winTerm contributors.
// Licensed under the MIT license.

#include "pch.h"
#include "SessionOwnership.h"

#include <algorithm>
#include <unordered_set>

using namespace winTerm::Docking;

SessionTransferLease::SessionTransferLease(
    SessionOwnershipRegistry& registry,
    std::vector<std::string> sessionIds) :
    _registry{ &registry },
    _sessionIds{ std::move(sessionIds) }
{
}

SessionTransferLease::SessionTransferLease(SessionTransferLease&& other) noexcept :
    _registry{ std::exchange(other._registry, nullptr) },
    _sessionIds{ std::move(other._sessionIds) }
{
}

SessionTransferLease& SessionTransferLease::operator=(SessionTransferLease&& other) noexcept
{
    if (this != &other)
    {
        Release();
        _registry = std::exchange(other._registry, nullptr);
        _sessionIds = std::move(other._sessionIds);
    }
    return *this;
}

SessionTransferLease::~SessionTransferLease()
{
    Release();
}

SessionTransferLease::operator bool() const noexcept
{
    return _registry != nullptr;
}

const std::vector<std::string>& SessionTransferLease::SessionIds() const noexcept
{
    return _sessionIds;
}

void SessionTransferLease::Release() noexcept
{
    if (const auto registry = std::exchange(_registry, nullptr))
    {
        registry->_releaseLease(_sessionIds);
    }
    _sessionIds.clear();
}

bool SessionOwnershipRegistry::Register(SessionIdentity identity, SessionOwner owner)
{
    if (identity.sessionId.empty() ||
        identity.sourceInstanceId.empty() ||
        owner.windowId.empty() ||
        owner.tabId.empty() ||
        owner.paneId.empty())
    {
        return false;
    }
    std::scoped_lock lock{ _mutex };
    for (const auto& [_, entry] : _entries)
    {
        if (entry.identity.live && entry.owner.paneId == owner.paneId)
        {
            return false;
        }
    }
    auto key = identity.sessionId;
    return _entries.emplace(std::move(key), Entry{ std::move(identity), std::move(owner) }).second;
}

bool SessionOwnershipRegistry::MarkExited(const std::string_view sessionId)
{
    std::scoped_lock lock{ _mutex };
    const auto found = _entries.find(std::string{ sessionId });
    if (found == _entries.end())
    {
        return false;
    }
    found->second.identity.live = false;
    ++found->second.generation;
    return true;
}

bool SessionOwnershipRegistry::Unregister(const std::string_view sessionId)
{
    std::scoped_lock lock{ _mutex };
    const auto found = _entries.find(std::string{ sessionId });
    if (found == _entries.end() || found->second.leaseCount != 0)
    {
        return false;
    }
    _entries.erase(found);
    return true;
}

std::optional<SessionIdentity> SessionOwnershipRegistry::Identity(const std::string_view sessionId) const
{
    std::scoped_lock lock{ _mutex };
    const auto found = _entries.find(std::string{ sessionId });
    return found == _entries.end() ? std::nullopt : std::optional{ found->second.identity };
}

std::optional<SessionOwner> SessionOwnershipRegistry::Owner(const std::string_view sessionId) const
{
    std::scoped_lock lock{ _mutex };
    const auto found = _entries.find(std::string{ sessionId });
    return found == _entries.end() ? std::nullopt : std::optional{ found->second.owner };
}

std::vector<SessionOwnershipSnapshot> SessionOwnershipRegistry::Snapshot(const std::vector<std::string>& sessionIds) const
{
    std::vector<SessionOwnershipSnapshot> result;
    std::unordered_set<std::string> seen;
    std::scoped_lock lock{ _mutex };
    result.reserve(sessionIds.size());
    for (const auto& sessionId : sessionIds)
    {
        if (!seen.emplace(sessionId).second)
        {
            return {};
        }
        const auto found = _entries.find(sessionId);
        if (found == _entries.end() || !found->second.identity.live)
        {
            return {};
        }
        result.emplace_back(SessionOwnershipSnapshot{
            sessionId,
            found->second.owner,
            found->second.generation,
        });
    }
    return result;
}

SessionTransferLease SessionOwnershipRegistry::AcquireLease(const std::vector<std::string>& sessionIds)
{
    std::unordered_set<std::string> seen;
    std::scoped_lock lock{ _mutex };
    for (const auto& sessionId : sessionIds)
    {
        const auto found = _entries.find(sessionId);
        if (!seen.emplace(sessionId).second ||
            found == _entries.end() ||
            !found->second.identity.live ||
            !found->second.identity.transferableWithinProcess)
        {
            return {};
        }
    }
    for (const auto& sessionId : sessionIds)
    {
        ++_entries.at(sessionId).leaseCount;
    }
    return SessionTransferLease{ *this, sessionIds };
}

bool SessionOwnershipRegistry::Transfer(
    const std::vector<SessionOwnershipSnapshot>& expected,
    const std::vector<SessionOwner>& targetOwners)
{
    if (expected.empty() || expected.size() != targetOwners.size())
    {
        return false;
    }
    std::scoped_lock lock{ _mutex };
    std::unordered_set<std::string> movingSessions;
    std::unordered_set<std::string> targetPaneIds;
    for (size_t index = 0; index < expected.size(); ++index)
    {
        const auto found = _entries.find(expected[index].sessionId);
        const auto& target = targetOwners[index];
        if (found == _entries.end() ||
            !found->second.identity.live ||
            found->second.leaseCount == 0 ||
            found->second.generation != expected[index].generation ||
            !(found->second.owner == expected[index].owner) ||
            target.windowId.empty() ||
            target.tabId.empty() ||
            target.paneId.empty())
        {
            return false;
        }
        if (!movingSessions.emplace(expected[index].sessionId).second ||
            !targetPaneIds.emplace(target.paneId).second)
        {
            return false;
        }
    }
    for (const auto& [sessionId, entry] : _entries)
    {
        if (entry.identity.live &&
            !movingSessions.contains(sessionId) &&
            targetPaneIds.contains(entry.owner.paneId))
        {
            return false;
        }
    }
    for (size_t index = 0; index < expected.size(); ++index)
    {
        auto& entry = _entries.at(expected[index].sessionId);
        entry.owner = targetOwners[index];
        ++entry.generation;
    }
    return true;
}

bool SessionOwnershipRegistry::Restore(const std::vector<SessionOwnershipSnapshot>& snapshot)
{
    std::scoped_lock lock{ _mutex };
    std::unordered_set<std::string> restoringSessions;
    std::unordered_set<std::string> restoredPaneIds;
    for (const auto& item : snapshot)
    {
        const auto found = _entries.find(item.sessionId);
        if (found == _entries.end() ||
            found->second.leaseCount == 0 ||
            !restoringSessions.emplace(item.sessionId).second ||
            !restoredPaneIds.emplace(item.owner.paneId).second)
        {
            return false;
        }
    }
    for (const auto& [sessionId, entry] : _entries)
    {
        if (entry.identity.live &&
            !restoringSessions.contains(sessionId) &&
            restoredPaneIds.contains(entry.owner.paneId))
        {
            return false;
        }
    }
    for (const auto& item : snapshot)
    {
        auto& entry = _entries.at(item.sessionId);
        entry.owner = item.owner;
        ++entry.generation;
    }
    return true;
}

bool SessionOwnershipRegistry::ValidateExactlyOneOwner(const std::vector<std::string>& sessionIds) const
{
    std::unordered_set<std::string> seenSessions;
    std::unordered_set<std::string> seenPanes;
    std::scoped_lock lock{ _mutex };
    for (const auto& [sessionId, entry] : _entries)
    {
        if (!entry.identity.live)
        {
            continue;
        }
        if (entry.owner.windowId.empty() ||
            entry.owner.tabId.empty() ||
            entry.owner.paneId.empty() ||
            !seenPanes.emplace(entry.owner.paneId).second)
        {
            return false;
        }
    }
    for (const auto& sessionId : sessionIds)
    {
        if (!seenSessions.emplace(sessionId).second)
        {
            return false;
        }
        const auto found = _entries.find(sessionId);
        if (found == _entries.end() ||
            !found->second.identity.live)
        {
            return false;
        }
    }
    return true;
}

bool SessionOwnershipRegistry::CanTransfer(
    const std::string_view sessionId,
    const std::string_view targetInstanceId) const
{
    std::scoped_lock lock{ _mutex };
    const auto found = _entries.find(std::string{ sessionId });
    return found != _entries.end() &&
           found->second.identity.live &&
           found->second.identity.transferableWithinProcess &&
           found->second.identity.sourceInstanceId == targetInstanceId;
}

size_t SessionOwnershipRegistry::LeaseCount(const std::string_view sessionId) const noexcept
{
    std::scoped_lock lock{ _mutex };
    const auto found = _entries.find(std::string{ sessionId });
    return found == _entries.end() ? 0 : found->second.leaseCount;
}

void SessionOwnershipRegistry::_releaseLease(const std::vector<std::string>& sessionIds) noexcept
{
    std::scoped_lock lock{ _mutex };
    for (const auto& sessionId : sessionIds)
    {
        if (const auto found = _entries.find(sessionId);
            found != _entries.end() && found->second.leaseCount != 0)
        {
            --found->second.leaseCount;
        }
    }
}
