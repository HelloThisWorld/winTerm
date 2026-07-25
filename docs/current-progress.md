# Current development progress

Last updated: 2026-07-26

## Repository state

- Branch: `codex/release-v1.1.3`
- Starting commit: `c5d3ddc61869e041f1b2ef8de743296525c309a0`
- Microsoft Terminal upstream revision:
  `1cea42d433253d95c4487a3037db48197b5e72f4`
- Application and module version: `1.1.3`
- Package version: `1.1.3.0`
- Intended tag: `v1.1.3`
- Supported release target: Windows 11 x64

## Implemented in the working tree

- Refreshed the native title bar, tab strip, pane headers, dividers, window
  controls, and terminal-shell palette to match the winterm.dev product
  visuals without changing terminal, pane, workspace, or privacy behavior.
- Regenerated Windows package artwork, tiles, ICOs, and High Contrast icons
  from the canonical `assets/winterm/icons/winterm.svg` source.
- Fixed the tab-row XAML content structure so the tab strip and bottom border
  share one `ContentPresenter` child and compile in both Debug and Release.
- Updated application, package, shell module, About, workspace-fallback, and
  release metadata to `1.1.3` with intended tag `v1.1.3`.
- Added the 1.1.3 changelog entry and release notes; the README points to the
  1.1.3 release notes while downloads keep the stable `/releases/latest` URL.

## Evidence

PR #19 passed Windows PowerShell Smoke validation, GitHub static validation,
x64 Debug and Release builds, relevant upstream tests, Workspace benchmarks,
unpackaged/Setup/Portable creation, current-user and all-users installation
lifecycles, and the exact distribution allowlist. The v1.1.3 release branch
must repeat the applicable validation before its pull request is merged; the
tag-triggered formal Release workflow remains the final publication gate.
