# winTerm architecture

## Foundation baseline

winTerm v0.1 is based on Microsoft Terminal `release-1.25` commit `1cea42d433253d95c4487a3037db48197b5e72f4`. The project deliberately follows the upstream directory and build layout so reviewed upstream changes can continue to be integrated without a terminal-engine fork inside the fork.

## Reused upstream capabilities

The following remain upstream implementations:

- ConPTY and console-host integration
- VT parsing, terminal buffer, renderer, Unicode width, input protocol, and clipboard behavior
- Settings model and settings UI
- Dynamic profile generators, including PowerShell and WSL
- Tabs, panes, windows, command palette, search, scrollback, and keyboard actions
- XAML application shell, deployment plumbing, and tests

No v0.1 feature rewrites these areas.

## winTerm-owned boundary

winTerm owns a small integration layer:

- `src/winterm/Branding/winTerm.props` selects the `WinTerm` build brand.
- `src/cascadia/CascadiaPackage/Package-winTerm.appxmanifest` owns the independent MSIX identity and alias.
- `assets/winterm` contains original source artwork; `res/terminal/images-WinTerm` contains generated package resources.
- `scripts/winterm` contains thin wrappers around the upstream build and test modules.
- `docs` records identity, maintenance, release, and acceptance decisions.

Limited conditional changes in upstream integration files select the manifest, package assets, COM handoff identifiers, file metadata, application-data root, and visible strings when `WindowsTerminalBranding=WinTerm`. Existing Stable, Preview, Canary, and Dev brand branches remain intact.

## State isolation

| State | winTerm location or behavior |
| --- | --- |
| Packaged settings and state | Package-local data under `%LOCALAPPDATA%\Packages\<Kaname.winTerm package family>\LocalState` |
| Unpackaged settings and state | `%LOCALAPPDATA%\winTerm` |
| `settings.json` | The selected winTerm base settings directory |
| `state.json` and elevated state | The selected winTerm base settings directory |
| Logs | Existing upstream ETW diagnostics; no dedicated winTerm log directory in v0.1 |
| Cache | No new persistent winTerm cache is introduced |
| Profile source cache | Upstream profile generation is in memory; no separate persistent cache was found or added |
| Fragment discovery | Upstream compatibility discovery remains enabled, including the shared `Microsoft\Windows Terminal\Fragments` locations and package extensions |
| Stable-settings migration | Disabled for the WinTerm brand; it does not import the Windows Terminal Stable state directory |

Fragment locations are compatibility inputs supplied by profile publishers. They are not winTerm package-local settings and winTerm does not write Windows Terminal `LocalState` during first launch.

## Protected core

ConPTY, the VT parser, text buffer, renderer core, Unicode width engine, input protocol parser, and general OpenConsole behavior are protected. The only console-host-adjacent v0.1 edits choose independent COM registration identifiers for the WinTerm package; they do not change terminal semantics.

## Future module seams

Possible post-v0.1 modules are named here only to reserve architectural ownership:

- Shell Bridge: shell-specific compatibility and command adaptation
- Docking: IDE-style tab or pane placement interactions
- Workspace: named layouts and extended restoration

No interfaces, services, registrations, storage schemas, or user-visible behavior for these modules are implemented in v0.1. Their designs must be reviewed against upstream capabilities before code is added.
