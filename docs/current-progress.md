# Current development progress

Last updated: 2026-08-13

## Repository state

- Branch: `release/v1.3.0` (maintenance branch), based on the
  `v1.3.0-beta3` tree at `feb2687e5`
- Microsoft Terminal upstream revision:
  `1cea42d433253d95c4487a3037db48197b5e72f4`
- Application version: `1.3.0`
- Package/file version: `1.3.0.7`
- PowerShell module version: `1.3.0` with no prerelease suffix
- Release channel: `stable`
- Release tag: `v1.3.0`
- Supported target: Windows 11 x64

`v1.3.0` promotes the field-tested v1.3.0-beta3 build — the Command
Timeline release with the Visual Progress launch-fallback fix — to the
stable channel. The release workflow publishes it with `--latest`, moving
GitHub Latest from v1.2.0 (the Visual Progress stable release) to v1.3.0.
Development has meanwhile continued on `main` in the 1.4 series
(Pane Search); this maintenance branch exists only to carry the stable
version metadata for the v1.3.0 tag.

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

## Beta2 findings

Field testing the published beta2 build with long-running processes surfaced
one defect, fixed through pull request #41 and carried by this release
branch:

1. The Shell Integration fallback animated forever for any command that
   intentionally stays running (Alternate Screen TUIs, development servers,
   `tail -f`). The fallback is now a bounded one-shot launch indication: the
   comet plays one 1,800 ms traversal driven by a one-shot compositor batch
   (no timer, no polling loop), then the overlay hides with a silent
   Hidden/Running snapshot. The expiration is command-generation scoped, so
   stale completions cannot affect a newer command, alternate-screen churn
   cannot replay a consumed launch, and an expired fallback cannot resurrect
   after a provider or explicit OSC 9;4 owner clears. Explicit progress,
   recognized providers, short commands, and the result presentations are
   unchanged.

## Beta1 findings

Producing the sanitized website screenshots against the published beta1
portable build surfaced one defect, fixed on `main` through pull request #39:

1. Every Command Timeline entry reported `✓ Succeeded`, including commands
   that failed. The prompt wrapper executed `Get-Module` before the prompt
   function read `$?`, so the finished mark always carried exit code 0. The
   wrapper now captures `$?` first and passes it through, and the shell
   integration suite drives the installed wrapper end to end (first prompt,
   success, cmdlet failure, native exit code, recovery).

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

1. Update the winTerm website: dual stable/beta download columns, refreshed
   sanitized screenshots, the Tools navigation dropdown, and the logo link.
2. Collect beta feedback.
3. Promote to a stable `v1.3.0` only after beta testing, which is the point at
   which Latest, WinGet, and the website stable slot move.

## Validation state

Publication is gated by the tag-triggered release workflow: exact tag/version
match, release absence, clean checkout, version and branding verification,
static/security/privacy/workflow gates, an x64 Release build with compiled
tests, artifact generation, Draft asset re-download testing, and only then
publication. Record results from those actual runs; do not treat this document
as evidence for a command that did not run.
