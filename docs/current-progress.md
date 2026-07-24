# Current development progress

Last updated: 2026-07-24

## Repository state

- Branch: `codex/fix-v1.1.2-ghost-divider-lines`
- Starting commit: `031f403a1bf7bd36a1df0e9ab1024e2aa78df262`
- Microsoft Terminal upstream revision:
  `1cea42d433253d95c4487a3037db48197b5e72f4`
- Application and module version: `1.1.2`
- Package version: `1.1.2.0`
- Intended tag: `v1.1.2`
- Supported release target: Windows 11 x64

## Implemented in the working tree

- Fixed the 1.1.1 ghost pane-divider rendering: the visible divider now uses
  the same owner-local, leading-edge alignment model as its pointer target
  (vertical dividers align Left, horizontal dividers align Top, offset by the
  split position minus half the visible thickness).
- Extended the pane-resize source validation with divider-placement
  invariants: shared leading-edge alignment across the visible line and
  pointer targets, half-thickness centering on the split position, a guard
  against reintroducing the primary-axis Center alignment, a wider pointer
  target than the visible line, and divider reattachment across the split,
  swap, close, restore, and workspace rebuild paths.
- Updated application, package, shell module, About, workspace-fallback, and
  release metadata to `1.1.2` with intended tag `v1.1.2`.
- Added the 1.1.2 changelog entry and release notes; the README points to the
  1.1.2 release notes while downloads keep the stable `/releases/latest` URL.

## Evidence

The pane-resize and Smoke source validations pass in Windows PowerShell on
this branch. Compiled native tests, Release application launch, the manual
divider acceptance matrix (split ratios, nesting, DPI scales, High Contrast),
and 1.1.2 installer/Portable creation run through the pull-request GitHub
Actions workflows and remain release gates until their results are recorded.
