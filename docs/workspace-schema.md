# Workspace schema

`WorkspaceSchemaVersion` is `2`, `DockingModelVersion` is `1`, and both remain independent from application version `1.0.2`. A document is UTF-8 JSON with a 5 MB maximum size. The strict parser rejects comments, duplicate keys, special floating-point values, trailing data, excessive strings, and excessive nesting.

The top-level object contains `schemaVersion`, `id`, `name`, `description`, `createdAt`, `updatedAt`, `source`, `applicationVersion`, `protocolVersion`, `startupBehavior`, `activeWindowId`, and `windows`. Optional metadata includes tags, default state, last-opened time, capture reason, and recovery generation. IDs are globally unique and are not user-visible file names.

A window contains monitor identity, saved work area and DPI, absolute bounds, normalized monitor-relative bounds, state, active tab, and ordered tabs. Supported states are `normal`, `maximized`, `fullscreen`, and `minimized`.

A tab contains its title policy, theme, color, active and zoomed pane references, read-only state, pane descriptors, and one layout root. Layout nodes are `pane`, `split`, and `emptySlot`. A split has `horizontal` or `vertical` orientation, two children, and a finite ratio. Ratios outside `0.05` through `0.95` are repaired to `0.5`. An empty slot has a stable slot ID and optional preferred profile reference but no pane, command, process, or session. Grid, floating, external-session, plugin, script, command, and URL nodes are rejected.

A pane contains a stable ID, profile ID and safe fallback metadata, shell type, current working-directory descriptor, starting-directory fallback, title, optional theme and font overrides, and limited session-end flags. Working-directory kinds are `windows`, `unc`, `wsl`, `remote`, and `unknown`; their source is recorded separately. A WSL path stays in the WSL namespace and can include a distribution ID.

The schema has no field for command text, command history, output, clipboard content, arbitrary executable paths, shell arguments, environment variables, credentials, private keys, URLs, includes, plugins, process IDs for reattachment, or remote connection instructions.

Validation enforces maximums of 32 windows, 128 tabs per window, 64 panes per tab, 512 panes total, and layout depth 32. It verifies IDs, active references, layout integrity, finite geometry, positive sizes, DPI, supported enums, fonts, and directory namespaces. Missing active references and invalid ratios have explicit repair paths; duplicate IDs, cycles, missing panes, unsupported nodes, and excessive data are errors.

Migration is sequential and idempotent. The current migration helper accepts the internal schema-0 fixture, produces the version-1 envelope, and then upgrades version 1 to version 2 without changing pane or split semantics or adding empty slots. Stored files receive a separate pre-migration backup. A newer schema is preserved without modification and is not loaded.
