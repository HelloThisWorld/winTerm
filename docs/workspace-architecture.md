# Workspace architecture

winTerm v0.4 treats a workspace as a restore description, not a suspended terminal process. The description contains windows, tab order, the existing pane split tree, profile references, working-directory metadata, appearance references, focus, and recovery metadata. It never represents process memory, handles, sockets, terminal output, command history, clipboard data, credentials, or environment snapshots.

Three states are deliberately separate:

- A named workspace is a user-managed definition and changes only through an explicit save or update operation.
- Runtime state is the currently open set of windows, tabs, and panes. Opening a named workspace does not make later runtime changes overwrite that definition.
- Last Session is the autosave target for the latest runtime state and may include changes made after opening a named workspace.

The source boundary is under `src/winterm/Workspaces`. `Model` defines immutable snapshot data. `Capture` applies privacy policy to a UI-thread snapshot. `Persistence` performs strict JSON parsing, validation, migration, atomic replacement, backup, snapshots, indexing, and quarantine. `Restore` resolves monitors, profiles, directories, themes, and fonts into a progressive plan. `Runtime` owns dirty generations, debounce decisions, recovery inspection, and explicit named-workspace operations. `ImportExport` is the untrusted-file boundary. `Diagnostics` emits redacted health data.

The Microsoft Terminal `ApplicationState`, `WindowLayout`, and pane startup-action implementation remain the inherited runtime bridge. v0.4 does not change ConPTY, the VT parser, text buffer, renderer core, Unicode width engine, input protocol parser, or OpenConsole internals. The backend is included in the Settings Model library project so it can be shared by startup, settings, and tests without serializing live XAML or Pane objects; compilation is still an open acceptance gate on this machine.

For winTerm branding only, the inherited first-window preference defaults to persisted layout. Other upstream branding retains the original default-profile setting. On a first launch with no persisted layout this still opens a new default-profile session.

Capture must read the minimum UI state on the UI thread, produce a value snapshot, and perform validation, JSON serialization, and disk writes away from UI objects. Every save has a monotonically increasing generation; an older generation cannot replace a newer one.

The storage layout is a winTerm-only `workspaces` directory under the application state root. It contains `last-session.json`, its backup, `snapshots`, `named`, `imported`, `quarantine`, and a rebuildable `index.json`. This storage never uses Microsoft Terminal package identity or LocalState.

Workspace schema v2 shares its `pane`, `split`, and `emptySlot` tree with the winTerm Visual Docking planner and preview geometry. Docking hover state and drag tokens remain outside persistence. `GridNode`, cross-process live pane transfer, terminal broker support, post-exit ConPTY reattachment, command replay, and SSH reconnection remain outside this architecture.
