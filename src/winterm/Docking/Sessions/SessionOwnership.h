// Copyright (c) winTerm contributors.
// Licensed under the MIT license.

#pragma once

#include <cstddef>
#include <cstdint>
#include <mutex>
#include <optional>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

namespace winTerm::Docking
{
    struct SessionOwner
    {
        std::string windowId;
        std::string tabId;
        std::string paneId;

        bool operator==(const SessionOwner&) const noexcept = default;
    };

    struct SessionIdentity
    {
        std::string sessionId;
        std::string connectionType;
        std::string sourceInstanceId;
        bool live{ true };
        bool transferableWithinProcess{ true };
    };

    struct SessionOwnershipSnapshot
    {
        std::string sessionId;
        SessionOwner owner;
        uint64_t generation{};
    };

    class SessionOwnershipRegistry;

    class SessionTransferLease
    {
    public:
        SessionTransferLease() = default;
        SessionTransferLease(const SessionTransferLease&) = delete;
        SessionTransferLease& operator=(const SessionTransferLease&) = delete;
        SessionTransferLease(SessionTransferLease&& other) noexcept;
        SessionTransferLease& operator=(SessionTransferLease&& other) noexcept;
        ~SessionTransferLease();

        explicit operator bool() const noexcept;
        const std::vector<std::string>& SessionIds() const noexcept;
        void Release() noexcept;

    private:
        friend class SessionOwnershipRegistry;
        SessionTransferLease(SessionOwnershipRegistry& registry, std::vector<std::string> sessionIds);

        SessionOwnershipRegistry* _registry{};
        std::vector<std::string> _sessionIds;
    };

    class SessionOwnershipRegistry
    {
    public:
        bool Register(SessionIdentity identity, SessionOwner owner);
        bool MarkExited(std::string_view sessionId);
        bool Unregister(std::string_view sessionId);

        std::optional<SessionIdentity> Identity(std::string_view sessionId) const;
        std::optional<SessionOwner> Owner(std::string_view sessionId) const;
        std::vector<SessionOwnershipSnapshot> Snapshot(const std::vector<std::string>& sessionIds) const;

        SessionTransferLease AcquireLease(const std::vector<std::string>& sessionIds);
        bool Transfer(
            const std::vector<SessionOwnershipSnapshot>& expected,
            const std::vector<SessionOwner>& targetOwners);
        bool Restore(const std::vector<SessionOwnershipSnapshot>& snapshot);

        bool ValidateExactlyOneOwner(const std::vector<std::string>& sessionIds) const;
        bool CanTransfer(std::string_view sessionId, std::string_view targetInstanceId) const;
        size_t LeaseCount(std::string_view sessionId) const noexcept;

    private:
        friend class SessionTransferLease;

        struct Entry
        {
            SessionIdentity identity;
            SessionOwner owner;
            size_t leaseCount{};
            uint64_t generation{ 1 };
        };

        void _releaseLease(const std::vector<std::string>& sessionIds) noexcept;

        mutable std::mutex _mutex;
        std::unordered_map<std::string, Entry> _entries;
    };
}
