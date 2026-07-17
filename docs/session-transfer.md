# Session transfer

## Ownership boundary

Visual Docking treats the inherited `ControlInteractivity` retained by `ContentManager` as the live terminal content boundary. Its `ControlCore` owns the terminal buffer, renderer state, and `ITerminalConnection`. A `TermControl` is the attachable XAML view.

The winTerm ownership model records:

```text
SessionIdentity
├─ stable session ID
├─ connection type
├─ source application instance ID
├─ live state
└─ same-process transfer capability

SessionOwner
├─ window ID
├─ tab ID
└─ pane ID
```

There is one registry entry and one owner for every live session. A pane ID cannot own two live sessions.

## Session transfer lease

A `SessionTransferLease` is acquired for every session in a moved pane or complete tab subtree before a source view is detached. While leased, the session cannot be unregistered by an ordinary view-removal path.

The lease does not duplicate a session or grant cross-process access. It only retains and reserves the existing in-process content until commit or rollback ends.

## Same-window and same-process transfer

The host adapter follows:

1. validate source and target identities;
2. snapshot layout, focus, and ownership;
3. acquire leases;
4. detach or prepare attachable views through the existing content manager;
5. prepare the target without deleting the source;
6. reserve exactly-one target ownership;
7. commit model and visual trees;
8. validate ownership and focus;
9. mark Last Session dirty;
10. release leases.

The shell process, ConPTY connection, terminal buffer, scrollback, active alternate buffer, working-directory metadata, and running command remain in the retained core. Search and selection UI are view-transient and may close consistently during a move.

## Cross-process transfer

Cross-process live transfer is not supported. A destination must have the same source application instance ID and use the same in-process content registry. A process ID, session ID, content ID, pane ID, handle, pointer, command line, working directory, or startup action is not accepted as authority.

An elevated window or another winTerm process displays:

`Live terminal transfer is available only between windows in this winTerm process.`

winTerm v0.5 does not add a terminal broker, duplicate ConPTY handles, inject into another process, replay commands, or restart the shell as a fallback.

## Unsupported session types

A session is disabled for transfer when it has exited, its connection reports that retained attachment is unsafe, its source instance differs, or its ownership generation changed during preparation. The drop is rejected without closing or restarting it.

## Rollback

Ownership transfer is generation-checked and atomic for the moved set. If target preparation, attachment, model commit, visual commit, Workspace coordination, or post-commit validation fails, the registry restores the snapshot while leases keep sessions alive. The UI adapter restores the original trees and focus. If the original layout cannot be rebuilt, all retained sessions are placed in recovery tabs.
