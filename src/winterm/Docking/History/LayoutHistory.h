// Copyright (c) winTerm contributors.
// Licensed under the MIT license.

#pragma once

#include "../Transactions/LayoutTransaction.h"

#include <cstdint>
#include <deque>
#include <functional>
#include <string>
#include <vector>

namespace winTerm::Docking
{
    struct LayoutHistoryEntry
    {
        uint64_t sequence{};
        std::string transactionId;
        DockOperation operation{ DockOperation::Move };
        LayoutTransactionSnapshot before;
        LayoutTransactionSnapshot after;
        std::vector<std::string> sessionIds;
        std::string description;
    };

    enum class LayoutHistoryStatus
    {
        Applied,
        Empty,
        SessionUnavailable,
        ApplyFailed,
    };

    struct LayoutHistoryResult
    {
        LayoutHistoryStatus status{ LayoutHistoryStatus::Empty };
        std::string message;
        std::optional<LayoutHistoryEntry> entry;

        bool Succeeded() const noexcept;
    };

    class LayoutHistory
    {
    public:
        using SessionsAvailableCallback = std::function<bool(const std::vector<std::string>&)>;
        using ApplyCallback = std::function<bool(const LayoutHistoryEntry&, bool useBefore)>;

        explicit LayoutHistory(size_t limit = 20);

        void Limit(size_t limit);
        size_t Limit() const noexcept;
        size_t UndoCount() const noexcept;
        size_t RedoCount() const noexcept;

        bool Record(LayoutHistoryEntry entry);
        bool CanUndo(const SessionsAvailableCallback& sessionsAvailable) const;
        bool CanRedo(const SessionsAvailableCallback& sessionsAvailable) const;
        LayoutHistoryResult Undo(
            const SessionsAvailableCallback& sessionsAvailable,
            const ApplyCallback& apply);
        LayoutHistoryResult Redo(
            const SessionsAvailableCallback& sessionsAvailable,
            const ApplyCallback& apply);
        void Clear() noexcept;

    private:
        static LayoutHistoryEntry _clone(const LayoutHistoryEntry& entry);
        static bool _available(const LayoutHistoryEntry& entry, const SessionsAvailableCallback& callback);
        void _trim();

        size_t _limit;
        uint64_t _nextSequence{ 1 };
        std::deque<LayoutHistoryEntry> _undo;
        std::deque<LayoutHistoryEntry> _redo;
    };
}
