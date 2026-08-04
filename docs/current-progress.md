# Current development progress

Last updated: 2026-08-04

## Repository state

- Branch: `release/v1.3.0-alpha4`
- Base branch: `main` at `49ee5eb3` (alpha3 field-report fixes, pull
  request #36)
- Microsoft Terminal upstream revision:
  `1cea42d433253d95c4487a3037db48197b5e72f4`
- Application version: `1.3.0-alpha4`
- Package/file version: `1.3.0.3`
- PowerShell module version: `1.3.0` with prerelease suffix `alpha4`
- Release channel: `alpha`
- Release tag: `v1.3.0-alpha4`
- Current public Latest: `v1.2.0`, the stable Visual Progress release
- Supported target: Windows 11 x64

`v1.3.0-alpha4` follows `v1.3.0-alpha3` as a GitHub **prerelease** for local
testing, carrying the three alpha3 field-report fixes. The release workflow marks any
non-stable channel with `--prerelease` and `--latest=false`, so
`/releases/latest` keeps resolving to v1.2.0. The alpha is deliberately not
listed on the winTerm website and is skipped by the WinGet workflow.

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

## Alpha3 field reports

Local testing of `v1.3.0-alpha3` surfaced three issues, all fixed on `main`
through pull request #36:

1. An antivirus alert on every new PowerShell tab: the `touch` command's raw
   `File::Open` write plus same-file command dispatch read as a
   write-then-execute pattern; creation now uses `New-Item` and the module
   loads clean.
2. Every completed command displayed `? Unknown`: the Enter keypress
   heuristic never notified the lifecycle, so the capability chain never read
   Full. Enter now reports the executed transition for shell-established
   marks with non-empty input, restoring Running and the ✓/✕ results.
3. Long commands were unreadable: rows now carry a full-command tooltip, and
   the Unknown status explains itself on hover.

## Alpha2 field reports

Local testing of `v1.3.0-alpha2` surfaced five issues, all fixed on `main`
through pull request #34:

1. Stray `\` characters before every prompt: the module's OSC terminator was
   a two-character PowerShell string; it is now a single backslash.
2. The Timeline captured the whole prompt line and Load inserted the prompt
   path: the marks were console side effects; they are now embedded in the
   returned prompt string in FinalTerm order.
3. The Visual Progress bar animated at an idle integrated prompt: `133;B`
   (composing input) no longer shows a bar; only `133;C` does.
4. A phantom `Command text unavailable / Running` row for the active prompt:
   a `133;B`-only mark no longer creates a Timeline entry.
5. Rows jumped on arrow keys and hover: selection-only updates now reuse the
   existing rows.

An antivirus-blocked module component (seen with `Compatibility.ps1`) is now
skipped silently and reported through diagnostics.

## Alpha1 field reports

Local testing of `v1.3.0-alpha1` surfaced four issues, all fixed on `main`
through pull request #32:

1. The Timeline handle covered terminal content. It is now a thin auto-hiding
   strip on the terminal's left edge that widens on hover, focus, or while the
   overlay is open.
2. Clicking the terminal area did not close an open Timeline. The overlay now
   light-dismisses on a terminal press, which still reaches the terminal.
3. A `dir` listing left the Visual Progress bar animating indefinitely. The
   recognition engine no longer claims ownership from a bare product-name
   mention, no longer rematches arbitrary records under an established claim,
   structurally clears a still-running bar after two consecutive ordinary
   records, and no longer reads slashed dates as meters.
4. Typed commands never appeared in the Timeline because nothing imported the
   packaged `winTerm.Shell` module. Bare PowerShell profile commandlines are
   now rewritten at connection creation to import it, gated by the new
   per-profile setting `"shellIntegration.autoInject"` (default `true`).

## Next steps

1. Install `v1.3.0-alpha4` locally and re-test the three fixes.
2. Cut `v1.3.0-beta1` on channel `beta` once alpha4 passes local testing. The
   beta may be listed on the winTerm website alongside the stable v1.2.0
   download.
3. Promote to a stable `v1.3.0` only after beta testing, which is the point at
   which Latest, WinGet, and the website stable slot move.

## Validation state

Publication is gated by the tag-triggered release workflow: exact tag/version
match, release absence, clean checkout, version and branding verification,
static/security/privacy/workflow gates, an x64 Release build with compiled
tests, artifact generation, Draft asset re-download testing, and only then
publication. Record results from those actual runs; do not treat this document
as evidence for a command that did not run.
