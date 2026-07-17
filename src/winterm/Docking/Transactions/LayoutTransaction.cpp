// Copyright (c) winTerm contributors.
// Licensed under the MIT license.

#include "pch.h"
#include "LayoutTransaction.h"

#include "../Layout/LayoutTree.h"

#include <stdexcept>

using namespace winTerm::Docking;

namespace
{
    [[noreturn]] void Fail(const std::string& message)
    {
        throw std::runtime_error(message);
    }
}

LayoutTransactionCoordinator::LayoutTransactionCoordinator(SessionOwnershipRegistry& ownership) :
    _ownership{ ownership }
{
}

LayoutTransactionResult LayoutTransactionCoordinator::Execute(LayoutTransactionRequest request)
{
    LayoutTransactionResult result;
    result.docking.transactionId = request.transactionId;
    result.docking.status = DockingStatus::Failed;
    bool ownershipTransferred = false;
    SessionTransferLease lease;

    try
    {
        if (request.transactionId.empty())
        {
            Fail("The layout transaction ID is missing.");
        }
        if (request.plan.status != DockingStatus::Ready)
        {
            Fail(request.plan.invalidReason.empty() ? "The docking plan is not ready." : request.plan.invalidReason);
        }
        if (request.sessionIds.empty() ||
            request.sessionIds.size() != request.targetOwners.size())
        {
            Fail("The session transfer request is incomplete.");
        }
        if (!_invoke(request.callbacks.sourceAvailable) ||
            !_invoke(request.callbacks.targetAvailable))
        {
            Fail("The source or target is no longer available.");
        }

        if (request.plan.proposedSourceLayout)
        {
            const auto sourceValidation = LayoutValidator::Validate(request.plan.proposedSourceLayout);
            if (!sourceValidation.IsValid())
            {
                Fail(sourceValidation.ErrorMessages().front());
            }
        }
        if (request.plan.proposedTargetLayout)
        {
            const auto targetValidation = LayoutValidator::Validate(request.plan.proposedTargetLayout);
            if (!targetValidation.IsValid())
            {
                Fail(targetValidation.ErrorMessages().front());
            }
        }
        result.state = LayoutTransactionState::Validated;

        request.snapshot.sourceLayout = LayoutTree::Clone(request.snapshot.sourceLayout);
        request.snapshot.targetLayout = LayoutTree::Clone(request.snapshot.targetLayout);
        request.snapshot.sessionOwners = _ownership.Snapshot(request.sessionIds);
        if (request.snapshot.sessionOwners.size() != request.sessionIds.size())
        {
            Fail("A live terminal session is missing or has duplicate ownership.");
        }

        lease = _ownership.AcquireLease(request.sessionIds);
        if (!lease)
        {
            Fail("The terminal sessions could not be reserved for transfer.");
        }
        result.state = LayoutTransactionState::SessionsLeased;

        if (!request.callbacks.prepareTarget ||
            !request.callbacks.prepareTarget(request.plan))
        {
            Fail("The target view could not be prepared.");
        }
        result.state = LayoutTransactionState::TargetPrepared;

        if (!_invoke(request.callbacks.sourceAvailable) ||
            !_invoke(request.callbacks.targetAvailable))
        {
            Fail("The source or target closed while the drop was being prepared.");
        }

        if (!_ownership.Transfer(request.snapshot.sessionOwners, request.targetOwners))
        {
            Fail("Session ownership changed before the layout could be committed.");
        }
        ownershipTransferred = true;
        result.state = LayoutTransactionState::OwnershipReserved;

        if (!request.callbacks.commitModel ||
            !request.callbacks.commitModel(request.plan))
        {
            Fail("The layout model could not be committed.");
        }
        result.state = LayoutTransactionState::ModelCommitted;

        if (!request.callbacks.commitVisualTree ||
            !request.callbacks.commitVisualTree(request.plan))
        {
            Fail("The visual tree could not be committed.");
        }
        result.state = LayoutTransactionState::VisualCommitted;

        if (!_ownership.ValidateExactlyOneOwner(request.sessionIds))
        {
            Fail("Session ownership validation failed after the layout commit.");
        }
        if (!_invoke(request.callbacks.markWorkspaceDirty))
        {
            Fail("The Workspace coordinator could not accept the layout change.");
        }

        _invoke(request.callbacks.restoreFocus);
        _invoke(request.callbacks.closeOverlay);
        _invoke(request.callbacks.invalidateDragToken);
        result.state = LayoutTransactionState::Completed;
        result.docking.status = DockingStatus::Committed;
        result.docking.workspaceDirty = true;
        result.docking.message = "The layout was changed.";
        return result;
    }
    catch (const std::exception& exception)
    {
        result.docking.message = exception.what();
    }
    catch (...)
    {
        result.docking.message = "An unexpected error prevented the layout change.";
    }

    result.state = LayoutTransactionState::RollingBack;
    if (ownershipTransferred)
    {
        result.ownershipRestored = _ownership.Restore(request.snapshot.sessionOwners);
    }
    else
    {
        result.ownershipRestored = true;
    }

    try
    {
        result.layoutRestored = request.callbacks.rollback &&
                                request.callbacks.rollback(request.snapshot);
    }
    catch (...)
    {
        result.layoutRestored = false;
    }

    if (!result.ownershipRestored || !result.layoutRestored)
    {
        try
        {
            result.recoveryLayoutUsed = request.callbacks.recoverSessions &&
                                        request.callbacks.recoverSessions(request.snapshot);
        }
        catch (...)
        {
            result.recoveryLayoutUsed = false;
        }
    }

    _invoke(request.callbacks.restoreFocus);
    _invoke(request.callbacks.closeOverlay);
    _invoke(request.callbacks.invalidateDragToken);

    if ((result.ownershipRestored && result.layoutRestored) || result.recoveryLayoutUsed)
    {
        result.state = LayoutTransactionState::RolledBack;
        result.docking.status = DockingStatus::RolledBack;
        result.docking.message = "The layout could not be changed. The previous layout was restored.";
    }
    else
    {
        result.state = LayoutTransactionState::Failed;
        result.docking.status = DockingStatus::Failed;
        result.docking.message = "The layout could not be restored automatically. A docking health report is available.";
    }
    return result;
}

bool LayoutTransactionCoordinator::_invoke(
    const std::function<bool()>& callback,
    const bool defaultValue)
{
    return callback ? callback() : defaultValue;
}

void LayoutTransactionCoordinator::_invoke(const std::function<void()>& callback) noexcept
{
    if (callback)
    {
        try
        {
            callback();
        }
        catch (...)
        {
        }
    }
}
