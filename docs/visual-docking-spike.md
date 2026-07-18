# Visual docking technical spike

## Decision

The v0.5 live-transfer prototype is a **go for windows hosted by the current winTerm process** and a **no-go for arbitrary cross-process pane transfer**.

The Microsoft Terminal baseline already keeps every normal window in one `WindowEmperor` process and shares one `TerminalApp::ContentManager` through `AppLogic`. A `TermControl` is a XAML view; its framework-independent `ControlInteractivity` and `ControlCore` can be detached from one control and attached to another. `ControlCore` owns the terminal buffer, renderer state, and `ITerminalConnection`, so the existing `ContentId` transfer path preserves a running shell instead of launching a replacement.

That path is a useful live-session primitive, but it is not a v0.5 transaction. The current tab and pane move implementations detach the source, serialize startup actions containing internal content IDs, request a target window, and then remove the source. They do not receive a target-ready acknowledgement and cannot restore the complete source tree if target creation or attachment fails. Visual Docking must put validation, leases, commit acknowledgement, and rollback around this primitive before enabling a drop.

Cross-process transfer remains unsupported. `ControlInteractivity`, `ControlCore`, the terminal buffer, renderer, and connection are process-local objects. The inherited drag path rejects a different process ID, and winTerm will not add a broker, duplicate ConPTY handles, restart the shell, or use command replay.

## Repository baseline

| Item | Recorded value |
| --- | --- |
| Branch before v0.5 work | `main` |
| Branch for v0.5 work | `codex/winterm-v0.5-visual-docking` |
| Current/v0.4 baseline commit | `9890c1c0419998b1c59c946810a4cb7e12a6787f` |
| v0.4 tag | None |
| Microsoft Terminal source baseline | `release-1.25@1cea42d433253d95c4487a3037db48197b5e72f4` |
| Remotes | `origin` exists; `upstream` is absent |
| Uncommitted files before v0.5 work | None observed |
| Package identity | `HelloThisWorld.winTerm`; alias `winterm.exe` |
| Application/package version before v0.5 | `0.4.0-dev` / `0.4.0.0` |
| Workspace schema before v0.5 | `1` |

The v0.1 through v0.4 acceptance documents exist. v0.4 is explicitly not accepted: its backend and source-level checks exist, but real capture/restore UI integration, builds, packages, launch, multi-window restore, and manual hardware scenarios remain open.

The pre-change Smoke suite passed on 2026-07-17. Debug x64 and Release x64 build attempts both stopped before compilation because PowerShell 7, Visual Studio, MSBuild, and the Windows SDK toolchain are unavailable in this environment. No compiled or runtime result is inferred from the source checks.

## Required architecture answers

### 1. Are multiple windows hosted in one process or separate processes?

Normal windows are hosted by one terminal process. `WindowEmperor` describes itself as the manager for the single Terminal process, stores all `AppHost` instances in `_windows`, and creates each new window from the shared `AppLogic`. A new client invocation may contact the existing remoting instance, but it does not make a normal destination window a separate owner of the transferred terminal content.

### 2. How does upstream implement tab tear-out?

`TerminalPage` handles WinUI `TabDragStarting`, puts the source window ID and current PID in the drag package, and stashes the dragged `Tab`. A target tab strip accepts only a matching PID and asks the source window to send content through the window manager. Dropping outside calls `_sendDraggedTabToWindow` for a new window.

The source converts the complete pane tree to startup actions using `BuildStartupKind::Content`. Each terminal action carries a process-local `ContentId`. `_DetachTabFromWindow` asks the shared `ContentManager` to detach every `TermControl`, after which the target parses the actions and `_MakePane` attaches new controls to the retained content. The original tab is then removed.

### 3. Can a live terminal session move between windows without restarting?

Yes, between windows in the same current process. The target reuses the `ControlInteractivity` identified by `ContentId`; it does not construct a new `ITerminalConnection`.

### 4. Can a live pane move between windows without restarting?

Yes, through the inherited `move-pane` path when both windows use the shared in-process `ContentManager`. The current implementation is not transactional and therefore is not sufficient for an enabled Visual Docking drop until v0.5 rollback and acknowledgement are added.

No, across unrelated processes. That capability is disabled rather than emulated.

### 5. Which object owns the ConPTY connection?

`ControlCore` holds the `ITerminalConnection`. For a local shell that interface is implemented by `ConptyConnection`, which owns the pseudoconsole and shell process resources. Closing `ControlCore` closes the connection.

### 6. Which object owns the terminal buffer and renderer state?

`ControlCore` owns the shared terminal buffer (`Terminal`), renderer, and Atlas render engine. `ControlInteractivity` owns the `ControlCore` and UIA engine. `ContentManager` retains `ControlInteractivity` independently of a particular `TermControl`.

### 7. Can a terminal view be detached and reattached?

Yes. `TermControl::Detach`, `ControlInteractivity::Detach`, `ControlCore::Detach`, `ContentManager::TryLookupCore`, and `TermControl::NewControlByAttachingContent` form the existing detach/reattach boundary. The XAML `TermControl` is recreated or rehosted while the retained core remains alive.

### 8. How are tab and pane IDs represented?

Upstream runtime window IDs are `uint64_t`. Runtime pane IDs are optional `uint32_t` values assigned in the pane tree. A `Tab` does not expose a durable workspace GUID in the inherited transfer path; the Tab object and TabView position identify it during runtime.

The v0.4 winTerm workspace model separately stores stable window, tab, and pane IDs as strings and validates them globally. v0.5 docking uses those stable strings in its model and maps them to upstream runtime objects only at the host adapter boundary. Empty slots receive their own stable string IDs.

### 9. How are layouts serialized?

Upstream serializes live pane trees as ordered `newTab`, `splitPane`, and `moveFocus` actions. Persisted state is coordinated by `ApplicationState`, `WindowLayout`, `TerminalPage::PersistState`, `Tab::BuildStartupActions`, and `Pane::BuildStartupActions`.

The winTerm v0.4 schema serializes a separate value model with `pane` and binary `split` nodes. Split orientation is `horizontal` or `vertical`, and each split contains a ratio and exactly two children. v0.5 extends this value model to schema version 2 with `emptySlot`; preview, validation, transaction planning, history, and Workspace persistence all consume the same model.

### 10. Which upstream components should remain untouched?

Visual Docking does not change ConPTY, the VT parser, text buffer, terminal renderer core, Unicode width engine, input protocol parser, or OpenConsole internals. The existing `ContentManager`/`ControlCore` detach-and-attach primitive is reused through a narrow adapter. Changes to `TerminalPage`, `Tab`, `Pane`, XAML, actions, or Settings are kept integration-focused and do not move upstream files into `src/winterm`.

## Current ownership graph

```text
WindowEmperor (one process)
├── AppLogic
│   └── ContentManager (shared by all windows)
│       └── contentId -> ControlInteractivity
│           ├── UIA engine
│           └── ControlCore
│               ├── Terminal buffer
│               ├── Renderer and render engine
│               └── ITerminalConnection
│                   └── ConptyConnection / other connection type
└── AppHost [one per window]
    └── TerminalWindow / TerminalPage
        └── Tab
            └── Pane tree
                └── TerminalPaneContent
                    └── TermControl (XAML view attached to retained content)
```

The current `Pane` owns an `IPaneContent`, and `TerminalPaneContent::Close` closes its `TermControl`. Transfer avoids that close by detaching the retained content first. This is an implicit lifetime convention, not an explicit lease, so v0.5 must make the transfer lifetime visible and testable.

## Existing capabilities

- WinUI tab reorder within one tab strip.
- Full-tab tear-out to a new same-process window.
- Full-tab movement to another same-process window and insertion index.
- Active-pane movement to another tab, a new tab, a named window, or a new window.
- Complete tab subtree reconstruction from existing `ContentId` values.
- DPI conversion and initial window positioning for tab tear-out.
- Framework-independent terminal content that outlives a detached XAML control.
- Upstream pane split tree, focus movement, detach, attach, normalization-by-sibling-promotion, and startup-action serialization.
- winTerm stable string IDs, strict Workspace validation, atomic persistence, dirty generations, restore planning, and import security boundaries.

## Gaps required for v0.5

- A model-first docking source, target, zone, operation, and capability layer.
- Schema version 2 and `EmptySlotNode`.
- Deterministic edge/corner transformation, geometry, normalization, and validation.
- Opaque expiring drag tokens; PID and window ID alone are not authority.
- A drag state machine, target priority, hysteresis, cancellation, and diagnostics.
- A two-sided layout transaction with target-ready acknowledgement.
- Explicit `SessionTransferLease` objects around retained `ControlInteractivity`.
- Atomic source/target model commit and visual commit adapters.
- Rollback and recovery layout paths.
- Layout history with live-session availability checks.
- Overlay and preview generated from the same proposed layout.
- Keyboard actions, UI Automation names/states, high-contrast styling, and announcements.
- Workspace capture/restore adapters for empty slots and docking metadata.

## Chosen transfer architecture

`TerminalSession` is a winTerm ownership record around the existing retained `ControlInteractivity` and its stable session identity. It does not replace `ControlCore` or `ITerminalConnection`.

`PaneHost` maps a stable pane ID to one session reference and one attachable view. A `SessionTransferLease` increments an in-memory lease count before any source view or layout object is removed. A `PaneViewAttachment` adapter detaches the current `TermControl` and creates or attaches the target control using the existing content manager.

The transaction coordinator follows this order:

1. Resolve the opaque drag token to an internal source record.
2. Snapshot source and target layout, focus, window, tab, and session ownership.
3. Build and validate proposed source and target layouts.
4. Acquire leases for every moved live session.
5. Ask the target host to prepare attachable views without mutating the live layout.
6. Commit target model and visuals.
7. Commit source model and visuals.
8. Validate exactly-one ownership, restore focus, mark Workspace dirty, and acknowledge success.
9. Invalidate the token and release leases.

Any failure before acknowledgement restores both snapshots while the leases retain the cores. If rollback cannot restore the original tree, every retained session is placed in a recovery tab; no live session is closed as a repair strategy.

## Cross-process constraints

- C++/WinRT object references, XAML controls, renderer pointers, and `ContentId` map entries are process-local.
- A raw process ID, pane ID, content ID, session ID, pointer, command line, working directory, environment, process handle, terminal output, or clipboard content is not a valid drag capability.
- The inherited Monarch/Peasant path coordinates process instances and command lines; it is not a terminal-core broker.
- A secure cross-process live transfer would require a supported external ownership service or broker. That is explicitly outside v0.5.
- A separate elevated process, another installed build, or an unrelated terminal process is not a supported target.

## Unsupported scenarios

- Transfer to a destination whose PID does not match the current winTerm process.
- Transfer from another application or from a forged/expired token.
- Duplicate, clone, mirror, or Ctrl-copy of a live shell.
- Live reattachment after winTerm exits or restarts.
- Session transfer that requires a new shell, command replay, handle injection, or ConPTY duplication.
- A connection type whose retained `ControlInteractivity` cannot be attached safely.
- Overlay or preview persistence.
- Arbitrary floating panes, overlapping layouts, or plugin-defined zones.

## Technical risks

| Risk | Mitigation |
| --- | --- |
| Source is removed before target succeeds | Prepare target first, retain snapshots and leases, require commit acknowledgement |
| `Pane` close paths terminate retained content | Route docking through a host adapter and explicit lease; never invoke close for a move |
| XAML attachment fails after model commit | Separate prepare/model/visual commit states and roll back both sides |
| Window closes during a move | Revalidate source and target generations before each commit transition |
| Content ID is guessed or reused | Resolve only an opaque random token in the in-memory registry and consume it once |
| Preview differs from final layout | Use the same `LayoutTransformer` result for geometry, preview, commit, and persistence |
| Pane IDs differ between runtime and Workspace | Keep stable docking IDs in the winTerm model and maintain an explicit runtime mapping |
| Mixed DPI changes during drag | Recompute target geometry in target-window DIPs and cancel safely on display invalidation |
| Existing v0.4 runtime integration is incomplete | Keep backend/model tests independent; mark end-to-end Workspace/UI gates not tested |
| Upstream source assumes best-effort moves | Put the inherited primitive behind a capability-checked transactional coordinator |

## Prototype acceptance and go/no-go

| Capability | Decision | Evidence |
| --- | --- | --- |
| Same-window pane move | Go | Existing `Pane::DetachPane`/`AttachPane` keeps the `IPaneContent` object |
| Same-process cross-window tab move | Go behind transaction | Existing complete-tree `BuildStartupKind::Content` and shared `ContentManager` |
| Same-process cross-window pane move | Go behind transaction | Existing `BuildStartupKind::MovePane`, content detach, and reattach path |
| Cross-process tab move | No-go | Drag explicitly rejects a mismatched PID; retained content registry is process-local |
| Cross-process pane move | No-go | No transferable owner for `ControlCore`, renderer state, buffer, or connection |

Implementation may advertise same-process transfers only after transaction and rollback tests pass. Cross-process targets remain disabled with the reason: `Live terminal transfer is available only between windows in this winTerm process.`
