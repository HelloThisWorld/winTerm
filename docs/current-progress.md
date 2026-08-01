# Current development progress

Last updated: 2026-08-01

## Repository state

- Branch: `codex/release-v1.2.0-visual-progress`
- Starting commit: `f4534bcfa49bc7328945279c8a392ba39fd5cf45`
- Microsoft Terminal upstream revision:
  `1cea42d433253d95c4487a3037db48197b5e72f4`
- Application and PowerShell module version: `1.2.0`
- Package version: `1.2.0.0`
- Intended tag: `v1.2.0`
- Release channel: stable
- Supported release target: Windows 11 x64

No tag or GitHub Release is created by this preparation branch. Publication
remains gated on review, merge, tagging the exact merged default-branch commit,
and the tag-triggered formal Release workflow.

## Implemented in the working tree

- Promoted Visual Progress from its Phase 2 preview to the stable 1.2.0 feature
  contract. Per-pane determinate, indeterminate, waiting, success, error, and
  cancelled state use the Rainbow Arc Weld overlay without consuming terminal
  rows, changing the viewport, or blocking PTY output.
- Preserved precedence as explicit OSC 9;4 progress, then a high-confidence
  built-in CLI provider, then generic OSC 133 shell lifecycle, then hidden.
- Enabled Visual Progress and bounded CLI recognition by default. Added the
  polished **Settings → Appearance → Visual progress** controls with Automatic,
  Full, Balanced, and Minimal performance modes. Recognized-output replacement
  remains off by default and keeps its conservative fail-open allowlist.
- Added a window-scoped, approximately one-second UI-dispatch sampler and an
  adaptive governor. Automatic mode caps effects by active progress count and
  degrades or recovers with consecutive latency samples and cooldown hysteresis;
  hidden, minimized, inactive, reduced-motion, High Contrast, remote,
  software-rendered, and constrained environments take cheaper safe tiers.
- Added ProgressBar UI Automation semantics with real 0–100 values only for
  determinate progress, localized nonnumeric state for indeterminate progress,
  and throttled start/milestone/waiting/terminal announcements for the active
  visible pane. Accessibility text contains no command, provider, or path data.
- Kept CLI recognition local, bounded, and in-memory. The authoritative
  `WINTERM_DISABLE_VISUAL_PROGRESS=1` override disables recognition, rendering,
  animation, and replacement while preserving all original output.
- Updated application, package, module, About, executable resource, Workspace
  fallback, release-note, changelog, and verification surfaces for 1.2.0. The
  README download entry continues to use GitHub's `/releases/latest` route.
- Kept workspace schema 2, docking model 1, shell protocol 1, theme schema 1,
  update-manifest schema 1, pinned upstream revision, dependencies, product
  identity, and signing policy unchanged.

## Validation state

The branch is prepared for source validation, feature tests, x64 Debug and
Release builds, relevant upstream tests, a bounded manual/soak matrix, and
unpackaged Setup/Portable lifecycle validation. Those results must be recorded
from actual runs; this document does not claim they passed merely because the
implementation and release metadata are present.

The formal Release workflow remains the publication gate. It must build and
attest the exact tagged commit, publish the eight-file asset allowlist, then
publicly re-download and verify the Setup EXE, Portable ZIP, checksums, notices,
both SBOM formats, release metadata, and release notes. The planned Setup EXE
is not Authenticode-signed, so its notes disclose possible Unknown Publisher or
SmartScreen warnings and direct users to `SHA256SUMS.txt`.
