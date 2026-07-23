# Current development progress

Last updated: 2026-07-24

## Repository state

- Branch: `codex/release-v1.1.1`
- Starting commit: `a503b8dfaee05c533ba5981d83d22e92e31b3e20`
- Microsoft Terminal upstream revision:
  `1cea42d433253d95c4487a3037db48197b5e72f4`
- Application and module version: `1.1.1`
- Package version: `1.1.1.0`
- Intended tag: `v1.1.1`
- Supported release target: Windows 11 x64

## Implemented in the working tree

- Removed winTerm pane repositioning sources, overlay/target/session models,
  settings, menus, command-line entry point, keyboard mode, and tests.
- Added branch-local border resize transactions, minimum-size filtering,
  common-ratio snapping, 8/14-pixel hysteresis, Alt bypass, rollback,
  one-entry history, undo/redo, and Balance Panes.
- Added native Settings controls, centralized design tokens, and website-aligned
  titlebar, tab, pane-header, divider, and terminal-shell resources.
- Updated version metadata, source validation, migration guidance, user
  documentation, acceptance matrix, release notes, and website handoff.

## Evidence

The directed-split/pane-control and pane-resize source validations pass in
Windows PowerShell. Compiled native tests, Release application launch, visual
inspection, manual DPI/accessibility/rendering/shell scenarios, and 1.1
installer/Portable creation remain release gates until their results are
recorded.
