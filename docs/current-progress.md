# Current development progress

Last updated: 2026-08-04

## Repository state

- Branch: `release/v1.3.0-alpha1`
- Base branch: `main` at `ac760eab` (Command Timeline Phase 4, pull request #30)
- Microsoft Terminal upstream revision:
  `1cea42d433253d95c4487a3037db48197b5e72f4`
- Application version: `1.3.0-alpha1`
- Package/file version: `1.3.0.0`
- PowerShell module version: `1.3.0` with prerelease suffix `alpha1`
- Release channel: `alpha`
- Release tag: `v1.3.0-alpha1`
- Current public Latest: `v1.2.0`, the stable Visual Progress release
- Supported target: Windows 11 x64

`v1.3.0-alpha1` is a published GitHub **prerelease**, not a stable release. The
release workflow marks any non-stable channel with `--prerelease` and
`--latest=false`, so `/releases/latest` keeps resolving to v1.2.0. The alpha is
deliberately not listed on the winTerm website and is skipped by the WinGet
workflow.

## Command Timeline status

The Command Timeline is feature-complete for its in-memory surface. Engineering
checkpoints v1.2.1 through v1.2.4 are all merged to `main`:

| Checkpoint | Scope |
| --- | --- |
| `v1.2.1` | Pane-owned OSC 133 index, stable command IDs, bounded command-text cache |
| `v1.2.2` | Overlay, deterministic navigation, wheel accumulation, accessibility |
| `v1.2.3` | Load without executing, copy command/output, jump to output, context menu |
| `v1.2.4` | Pane-local search, public settings, shell degradation, bounded history |

There is no persistent history, no output cache, no output search, and no
telemetry.

## Release channel handling

`verify-version.ps1` is now channel-aware rather than stable-only. It accepts
`stable`, `alpha`, and `beta`, and enforces that the channel, the module
prerelease suffix, and the application-version suffix agree with each other, so
a prerelease can never publish as Latest and a stable release can never carry a
prerelease suffix. The package version stays four-part numeric for MSIX and the
Win32 resource fields, and the PowerShell module version stays numeric with the
suffix carried in `PrivateData.PSData.Prerelease`.

## Next steps

1. Install `v1.3.0-alpha1` locally and exercise the Command Timeline.
2. Fix anything the alpha testing surfaces.
3. Cut `v1.3.0-beta1` on channel `beta`. The beta may be listed on the winTerm
   website alongside the stable v1.2.0 download.
4. Promote to a stable `v1.3.0` only after beta testing, which is the point at
   which Latest, WinGet, and the website stable slot move.

## Validation state

Publication is gated by the tag-triggered release workflow: exact tag/version
match, release absence, clean checkout, version and branding verification,
static/security/privacy/workflow gates, an x64 Release build with compiled
tests, artifact generation, Draft asset re-download testing, and only then
publication. Record results from those actual runs; do not treat this document
as evidence for a command that did not run.
