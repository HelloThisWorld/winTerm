# Layout transactions

Every enabled Visual Docking drop is a two-sided transaction. Pointer hover and preview never run a transaction.

## State

```text
Created
→ Validated
→ SessionsLeased
→ TargetPrepared
→ OwnershipReserved
→ ModelCommitted
→ VisualCommitted
→ Completed
```

Failure enters `RollingBack`, followed by `RolledBack` or `Failed`. A `Failed` result means both normal rollback and the recovery layout failed; it never means that live sessions were intentionally terminated.

## Snapshot

The transaction snapshot contains cloned source and target layout roots, active window/tab/pane and zoom state, and generation-tagged session owners. It does not contain command text, terminal output, clipboard data, environment variables, raw process IDs, pointers, or drag tokens.

## Validation

Before target preparation:

- the plan is ready and capability-approved;
- source and target still exist;
- proposed layout trees are valid;
- pane and slot IDs are unique;
- ratios, depth, and pane limits are valid;
- every moved session is live and has one expected owner.

The source and target are rechecked after target preparation. Ownership is reserved only under active leases. After visual commit, the registry verifies one live owner per pane before Workspace state is marked dirty.

## Host adapter contract

The coordinator calls a narrow adapter:

- `prepareTarget` may create attachable views but cannot delete source content;
- `commitModel` atomically publishes source and target model roots;
- `commitVisualTree` attaches the prepared views;
- `rollback` restores the complete snapshot;
- `recoverSessions` places retained sessions in safe tabs if rollback fails;
- `markWorkspaceDirty` schedules post-commit persistence;
- focus, overlay, and drag-token cleanup run on success and failure.

Any adapter exception is treated as a failed step and enters rollback.

## Workspace timing

Workspace dirty state changes only after model, visual, and ownership validation succeed. Autosave serializes schema version 2 after the drop. Drag hover, target changes, and preview geometry do not mark dirty or write storage.
