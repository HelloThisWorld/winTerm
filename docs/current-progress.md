# Current development progress

Last updated: 2026-08-02

## Repository state

- Branch: `feature/command-timeline-v1.3.0`
- Starting commit: `59020ff093ce8061d228d3aa3248f57a3b3301ef`
- Microsoft Terminal upstream revision:
  `1cea42d433253d95c4487a3037db48197b5e72f4`
- Engineering application and PowerShell module version: `1.2.1`
- Engineering package version: `1.2.1.0`
- Intended checkpoint tag: `v1.2.1`
- Final Command Timeline release target: `v1.3.0`
- Current public Latest: `v1.2.0`
- Supported target: Windows 11 x64

`v1.2.1` is a development checkpoint, not a distributable release. The README
and GitHub Latest continue to identify v1.2.0 as the public Visual Progress
release. Checkpoint tags v1.2.1 through v1.2.4 run quick validation only and are
explicitly excluded from full build, installer packaging, asset publication,
and GitHub Release jobs.

## Implemented in the working tree

- Added the Command Timeline Phase 1 data layer with a pane-owned index, view
  state, bounded command-text cache, and stable IDs composed from the pane
  session ID plus a monotonically increasing sequence.
- Extended native scrollbar mark metadata with an internal-only identity. The
  identity follows existing TextBuffer row copying and reflow and is never
  emitted into terminal text, OSC payloads, settings, workspaces, telemetry, or
  persistent storage.
- Reused the existing OSC 133 `StartPrompt`, `StartCommand`, `StartOutput`, and
  `EndCurrentCommand` lifecycle. Updates are incremental and idempotent;
  trustworthy completion codes alone map to success or failure.
- Added cold bootstrap through the existing native mark extents API and a
  pane-scoped mark revision seam. Warm reads with an unchanged revision return
  the cached index without rescanning the TextBuffer.
- Added explicit pruning for clear, circular-buffer eviction, and reflow loss.
  Reflow reports surviving native identities during its existing traversal, so
  resize preserves command IDs without a second full mark scan.
- Added deterministic model tests and a real native OSC component test covering
  stable identity, pane isolation, lifecycle ordering, capability, pruning,
  reflow, cache privacy, view-state ownership, and repeatable cleanup.
- Kept Phase 1 data-only: no pane handle, Timeline overlay, settings surface,
  input behavior, installer, website, or public release change was added.

## Validation state

The focused x64 Debug Command Timeline target builds successfully, and all 12
deterministic unit/component tests pass. Repository Smoke/static validation,
version consistency, checkpoint guard checks, and YAML parsing also pass. The
annotated checkpoint tag must point to this verified commit; the tag-triggered
quick workflow remains the remote confirmation gate.

The existing public v1.2.0 Visual Progress feature and its release notes remain
the stable user-facing milestone. The Command Timeline work is not public
release documentation and does not change the `/releases/latest` route.
