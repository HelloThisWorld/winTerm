# Current development progress

Last updated: 2026-08-13

## Repository state

- Branch: `release/v1.4.0-beta`, based on `main` at
  `45cf23975` (1.4.0-alpha release metadata, pull request #47)
- Microsoft Terminal upstream revision:
  `1cea42d433253d95c4487a3037db48197b5e72f4`
- Application version: `1.4.0-beta`
- Package/file version: `1.4.0.1`
- PowerShell module version: `1.4.0` with prerelease suffix `beta`
- Release channel: `beta` (published as a GitHub prerelease)
- Release tag: `v1.4.0-beta`, running the full guarded release pipeline;
  the checkpoint-tag allowlist is unchanged.
- Current public Latest: `v1.3.0`, the stable Command Timeline release
  (moved from v1.2.0, the Visual Progress stable, on 2026-08-13)
- Newest published prerelease: `v1.4.0-beta` (replacing `v1.4.0-alpha`)
- Supported target: Windows 11 x64

`v1.4.0-beta` is the beta of the Pane Search release. Manual validation of
`v1.4.0-alpha` passed — live search under sustained Kafka/Spring Boot log
output, navigation, the counter, and the scrollbar overview all behaved —
so the alpha content is promoted unchanged with beta metadata.
`/releases/latest` resolves to v1.3.0, and WinGet is not updated for
prereleases.

## Pane Search status (v1.4 roadmap)

- Phase 1 — active-pane search — complete at `1.3.1`.
- Phase 2 — search UX and scrollbar overview — complete at `1.3.2`.
- Phase 3 — performance and edge-case hardening — complete at `1.3.3`.
- Final integration — `1.4.0-alpha` published 2026-08-13; manual user
  validation **passed**.
- Beta — `1.4.0-beta` published for wider testing. Beta feedback decides
  the stable `1.4.0` promotion; only the user authorizes it.

Phase 1 made `Ctrl+F` (with the retained `Ctrl+Shift+F` alias) open the
existing Microsoft Terminal search box inside the focused pane only,
reusing the mature upstream pipeline end to end — `SearchBoxControl`,
`ControlCore::Search`, `Search`/`TextBuffer::SearchText`, and renderer
search highlights — with no second search engine, no index, and no buffer
duplication.

Phase 2 turned that functional search into the winTerm search experience.
The search box now follows the winTerm compact overlay language (search
glyph, input, `current / total` counter, case/regex toggles, previous/next,
close, at chrome density with theme-aware system brushes) and degrades
gracefully in narrow panes through width-driven layout states that always
keep the input and close button usable. The scrollbar shows a search
overview while search is open: one right-aligned marker per matching buffer
row across the full scrollback, deduplicating same-row occurrences, with
the current match's row drawn at double width. Search overview markers are
independent of the `ShowMarks` setting (which keeps gating generic shell
marks, default off), coexist with generic marks when both are enabled,
disappear on close, respect a deliberately hidden scrollbar, and refresh
only through the existing throttled scrollbar update path — no timers, no
polling. Split panes keep fully isolated search state, including their
overview markers.

Phase 3 hardened that experience for real terminal workloads without adding
any new search engine, index, or persistent state. Live typing is coalesced
per pane (50 ms, leading immediate + trailing latest, reading the search
box's state at fire time so the latest query always wins and navigation can
never act on a stale query). An open search now converges during sustained
output: the debounced OutputIdle refresh is complemented by a non-debounced
500 ms cap armed only while a search is active, so `tail -f`-style streams
no longer freeze the counter, highlights, or overview — and search closed
still means zero recurring search work. The scrollbar mark bitmap repaints
only when its inputs change, so plain scrolling with large result sets no
longer re-enumerates occurrences. Edge cases were fixed deterministically:
pane resize no longer converts pre-reflow spans into a stray selection,
main/alt buffer switches drop the other buffer's highlight spans immediately
(search keeps following the active buffer), and closing search releases the
terminal-side span copy. Regression tests cover mutation invalidation,
match-anchor stability during appended output, scrollback eviction, reflow,
alternate-screen transitions, generation/arming semantics, repaint-signature
contracts, wide-character spans (CJK, Korean, accented Latin, emoji), and a
log-only scan benchmark (`WINTERM_SEARCH_BENCH_LINES` scales it locally).

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

1. Collect Pane Search feedback on the published `v1.4.0-beta`
   prerelease; promote a stable `1.4.0` only after beta testing. Only the
   user authorizes the promotion, which is the point at which Latest,
   WinGet, and the website stable slot move to 1.4.0.
2. Keep the winTerm website's stable slot (v1.3.0) and prerelease slot
   (v1.4.0-beta) in sync with GitHub Releases.

## Validation state

Publication is gated by the tag-triggered release workflow: exact tag/version
match, release absence, clean checkout, version and branding verification,
static/security/privacy/workflow gates, an x64 Release build with compiled
tests, artifact generation, Draft asset re-download testing, and only then
publication. Record results from those actual runs; do not treat this document
as evidence for a command that did not run.
