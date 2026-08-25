# Current development progress

Last updated: 2026-08-25

## Repository state

- Working branch: `codex/release-v1.4.2`
- Branch base: `09dc76796725d9b7bd7b0d86a28196bce14698c7` (`origin/main` at
  release preparation start)
- Application version: `1.4.2`
- Package/file version: `1.4.2.0`
- PowerShell module version: `1.4.2` with an empty prerelease suffix
- Release channel: `stable`
- Intended tag: `v1.4.2`
- Supported target: Windows 11 x64

The source remains based on the repository's pinned Microsoft Terminal
baseline `release-1.25@1cea42d433253d95c4487a3037db48197b5e72f4`.
Microsoft Terminal `upstream/main` was separately fetched through
`86d15aef08e500be34497ff4e3a6f0d099ffb067` (2026-08-24) for a targeted
cursor-positioning audit; no broad upstream merge was performed.

## 1.4.2 release scope

winTerm 1.4.2 promotes the complete Pane Search development line and adds
Click to position cursor as a stable, default-enabled winTerm feature. It is
the combined application delta since stable v1.3.0. The immutable v1.4.1 tag
reached packaging but did not publish a GitHub Release because its release-note
signing heading did not match the exact verifier contract; 1.4.2 corrects that
release-only defect. The earlier 1.4.0 alpha and beta entries remain historical
records only.

### Pane Search

- `Ctrl+F` opens the focused pane's search overlay; `Ctrl+Shift+F` remains a
  compatibility alias.
- Full-scrollback live matching, all-match highlighting, forward/backward
  navigation, case-sensitive and regular-expression modes, and the compact
  `current / total` counter are complete.
- The pane-local scrollbar overview is complete and remains independent of
  the generic `ShowMarks` setting.
- Typing coalescing, sustained-output convergence, repaint signatures,
  reflow, scrollback eviction, alternate-buffer changes, wide characters,
  and invalid regex handling have regression coverage.

### Click to position cursor

- A plain single click is recorded on mouse-down and acted on only at
  mouse-up. Crossing the existing drag threshold cancels positioning and
  preserves normal selection without first moving the shell cursor.
- Ctrl+Click hyperlinks, VT mouse applications, double/triple click,
  Shift+Click, drag selection, and copy-on-select retain their established
  precedence.
- Positioning requires the unfinished final OSC 133 shell mark and accepts
  only the current editable command. Historical commands, output, scrollback,
  and untrusted locations safely do nothing.
- Coordinate translation validates viewport, buffer, inclusive TextBuffer
  bounds, padding, overflow, resize-era points, and malformed input before any
  buffer iterator is obtained.
- Full-width glyph trailing cells are excluded from LEFT/RIGHT event counts,
  and each split pane sends input only through its own connection.
- The winTerm default is enabled; an explicit profile value of `false` is
  preserved. The compatible internal JSON key remains
  `experimental.repositionCursorWithMouse`, while localized UI describes the
  stable feature as **Click to position cursor**.

### Visual Progress and Command Timeline

- Visual Progress remains stable in 1.4.2, including its determinate and
  indeterminate renderer, per-pane state, accessibility path, and local-only
  recognition controls.
- Command Timeline remains stable and pane-local, with OSC 133-backed command
  boundaries, filtering, load-without-executing, copy, jump, and status.
- The 1.4.2 cursor work does not change either feature's settings, protocol,
  persistence, or privacy boundaries.

## Upstream cursor audit

- Inspected Microsoft Terminal PR #20442 and merged commit
  `de3fc87d186e5da1d5ccd8731412905f5e2aba30`, which clamps click coordinates
  before TextBuffer access.
- Searched subsequent upstream history for changes involving
  `RepositionCursorWithMouse`, `_repositionCursorWithMouse`, and related click
  behavior. No later directly applicable cursor-safety fix was found.
- Also reviewed earlier related cursor-selection commits
  `4995af3dc1cc600b57fcdd953734a13b9b1a425f` and
  `d14ff939dc418fa04401304fdef539424dbb5bd5`; their relevant behavior was
  already present in winTerm.
- The backport follows current inclusive coordinate contracts rather than
  copying the upstream clamp literally, and adds stricter editable-mark,
  overflow, vertical-padding, glyph, and release-time interaction guards.

## Release channel and artifacts

The guarded tag workflow must confirm an exact tag/version match, absence of
an existing Release, a clean checkout, version/branding/security/privacy
gates, an x64 Release build with compiled tests, artifact generation, and a
public asset re-download before publication is considered complete.

The expected public Release assets are:

- `winTerm-1.4.2-setup-x64.exe`
- `winTerm-1.4.2-portable-x64.zip`
- `SHA256SUMS.txt`
- `THIRD_PARTY_NOTICES.md`
- `SBOM.spdx.json`
- `SBOM.cyclonedx.json`
- `release-metadata.json`
- `winTerm-1.4.2-release-notes.md`

The installer is currently not Authenticode-signed, so Unknown Publisher or
SmartScreen warnings remain possible. The Release notes disclose this and
direct users to verify `SHA256SUMS.txt`.

## Validation state

Local stable-candidate validation completed on 2026-08-25:

- Debug x64 package and all three unit-test projects built successfully.
- Smoke and Relevant suites passed.
- Compiled Settings Model, Terminal App, and Control suites passed with
  381/381, 51/51, and 93/93 tests respectively.
- Version, branding, privacy, release-workflow, and PowerShell syntax gates
  passed as part of those suites.

The formal Release workflow and public asset verification remain authoritative
for the published installer and Portable ZIP.

## Next steps

1. Merge through the application pull request, synchronize the Wiki ledger,
   tag the exact merged `main` commit, and monitor the formal Release workflow
   through public asset validation.
2. Update and deploy winterm.dev from the real `published_at` timestamp and
   public v1.4.2 asset URLs, then verify English/Japanese production pages.
3. Only after both the stable Release and website are verified, remove the
   published v1.4.0-beta GitHub prerelease entry while retaining its historical
   git tag.
