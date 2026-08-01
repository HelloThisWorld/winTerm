# Visual Progress Phase 2

Phase 2 extends the [Phase 1 progress foundation](visual-progress-phase1.md) with the Rainbow Arc Weld renderer and bounded CLI progress recognition. This remains a developer preview: it is not a released or stable v1.2.0 feature, the application version remains 1.1.3, and this work does not create a tag or release.

## Architecture

Phase 2 extends the existing per-pane state machine instead of introducing a second progress system:

```text
OSC 9;4 taskbar progress ---------------------------+
bounded incremental CLI recognition -> provider ----+-> normalized pane state
OSC 133 command lifecycle --------------------------+          |
                                                               v
                                                    one-element UI mailbox
                                                               |
                                                               v
                                                    Rainbow Arc Weld renderer
```

The normalized state contains only presentation data: source, provider ID, mode, status, real value when known, confidence, transient eligibility, optional stage identity, and sequence. It does not retain command text, package names, paths, URLs, or complete output records, and it is not written to workspaces or snapshots. The one-element mailbox continues to coalesce UI work so newer accepted snapshots replace older pending snapshots and terminal output never waits for decorative rendering.

Progress ownership follows this order:

1. Explicit standard OSC 9;4 progress.
2. A high-confidence built-in CLI provider.
3. Generic OSC 133 shell running, success, or error lifecycle.
4. Hidden.

Generic heuristic recognition cannot override explicit progress or an owned built-in provider. Clearing explicit progress reveals the current valid provider or shell fallback. A new semantic prompt resets provider ownership and bounded parser state for that pane.

## Rainbow Arc Weld renderer

The WinTerm-owned renderer is under `src/winterm/VisualProgress/`. Its renderer-independent state and constants are separated from the small Pane integration boundary so timing, status transitions, degradation, and resource budgets can be tested without XAML.

The continuous rounded beam is layered over the existing pane content row. It does not consume a terminal row, alter viewport dimensions, participate in hit testing, or receive accessibility focus. Transparent drawing space accommodates localized bloom without changing layout. The visual uses Windows composition transforms, clipping, opacity, cached brushes, and compositor animations; it is not constructed from terminal characters or repeated cell glyphs.

The composition layers, from back to front, are:

1. **Track:** a six-DIP rounded foundation.
2. **Rainbow fill:** a continuous animated gradient clipped to the real filled width.
3. **Luminous trail:** a short highlight behind the boundary.
4. **Welding head:** a warm white center and 2.5-DIP white-hot core.
5. **Inner glow:** a localized 12-DIP colored glow.
6. **Outer bloom:** a localized 26-DIP soft bloom.
7. **Welding sparks:** a sparse pooled particle layer.

The centralized geometry uses a 10-DIP horizontal inset, an 8-DIP bottom inset, a three-DIP corner radius, a five-DIP warm core, and an eight-DIP trail. Logical XAML sizing keeps the geometry stable at 100%, 125%, 150%, and 200% DPI and in narrow, maximized, split, and zoomed panes.

`SizeChanged` recomputes the track and bloom drawing geometry. When the usable track width or vertical origin changes, the renderer stops the compositor animations that captured the old geometry and rebinds rainbow and comet movement through the current environment plan. The post-layout generation therefore uses the new endpoints without a CPU frame loop, stale absolute offsets, or a terminal viewport relayout.

The rainbow is a coherent red-to-orange-to-yellow-to-green-to-cyan-to-blue-to-violet-to-magenta gradient. Its cached brush moves on a 2,000-millisecond cycle and is never rebuilt per frame. Determinate updates normally interpolate for 220 milliseconds. A real regression uses an intentional 240-millisecond phase-reset transition; the renderer does not invent a monotonic value. Zero percent keeps the welding head inside the track, while the fill remains clipped and the bloom drawing space remains available at 100 percent.

Indeterminate progress uses a welding-head comet with a continuous tail covering 25 percent of the track. It traverses the track in 1,800 milliseconds, fades cleanly at the right edge, and reappears at the left without showing a fabricated percentage.

### Status presentations

- **Running:** rainbow motion and the welding head are active. Only an eligible active pane emits sparse sparks.
- **Waiting:** forward movement pauses, the last meaningful value remains visible, sparks stop, and the glow breathes on a 1,600-millisecond cycle.
- **Success:** progress advances to 100 percent when appropriate, the head intensifies, a roughly 320-millisecond white highlight sweeps left to right, one controlled final burst is permitted, the beam takes on a success tint, and it fades over roughly 650 milliseconds.
- **Error:** sparks and rainbow movement stop, the beam changes to the error treatment, one roughly 220-millisecond intensity pulse runs, and the state remains until a new prompt or safe reset. It never flashes continuously. If an error has no meaningful progress value, the renderer preserves the real zero fill but colors the track and parks a visible error head at the zero boundary for the one-shot pulse. Static and solid fallbacks color the track instead, so unknown progress remains visibly erroneous without inventing a percentage.
- **Cancelled:** sparks stop, brightness falls, and the presentation fades in roughly 180 milliseconds before releasing its resources.

### Sparks and resource limits

Spark objects come from fixed pools. A normal burst emits one or two sparks; a stronger completion burst emits three to six. Typical particles are one to two DIPs, a rare bright particle is at most three DIPs, lifetimes are 120–260 milliseconds, and travel is 6–18 DIPs. Their color fades from white through warm yellow and orange to transparent, with emission biased forward and slightly upward or downward rather than in a 360-degree explosion.

The hard caps are **8 live sparks per active pane** and **24 live sparks globally**. The shared global budget is stricter than independent per-window allocation and prevents multiple windows from exceeding 24 live particles in total. Background panes do not emit sparks. Pooled particles are reused, and compositor animations perform movement and fading without per-frame heap allocation.

There is no CPU-driven frame loop, permanent per-pane timer, spark scheduler, per-frame brush construction, per-frame blur construction, terminal-buffer repaint, or viewport relayout. Two preallocated ambient spark slots use sparse compositor-managed iteration only while a visible, focused, active pane is eligible; all continuous motion remains compositor-managed.

## Lifecycle and degradation

Pane load, unload, close, split, detach, zoom, restore, focus, activation, and visibility changes update renderer eligibility without changing terminal state. In the XAML-Islands host, `CoreWindow` visibility and activation are not authoritative for the top-level Win32 window. `TerminalPage::WindowVisibilityChanged` and `TerminalPage::WindowActivated` instead publish the HWND-derived visible/focused pair through the root pane to every leaf renderer. New and restored tabs receive the current pair during tab-event registration, and newly split children inherit it immediately. A renderer starts unfocused and creates no continuous work or sparks until this authoritative state arrives.

Hidden or minimized windows pause animation. Inactive or background panes retain an inexpensive presentation but do not emit sparks. An unfocused window pauses all continuous motion and suppresses sparks. Completion, cancellation, pane destruction, settings disable, dispatcher shutdown, and device loss stop animation and release pooled resources.

Rendering is decorative and degrades independently for each affected pane:

1. Rainbow, localized glow, and sparks.
2. Rainbow and static glow, without sparks.
3. Static gradient.
4. Solid progress bar.
5. Overlay disabled for that pane.

Composition or GPU failure never changes PTY, input, output, selection, copy and paste, or pane-management behavior. Normalized UI publication is coalesced to approximately 10–20 updates per second even if a CLI writes more frequently.

### Reduced Motion and High Contrast

When Windows disables animation, the renderer uses the Phase 1 static behavior. Determinate values update directly, indeterminate state remains nonnumeric, and there is no moving rainbow, comet, interpolation, breathing, success sweep, or spark emission. Solid and static status treatments use separate dark- and light-surface palettes selected from the XAML host theme, with system background luminance as a fallback when the theme is default. This keeps running, waiting, success, and error states legible on light backgrounds as well as dark ones.

High Contrast uses a clear system-compatible solid track and fill. It does not depend on rainbow hue or glow to communicate progress. High Contrast also disables sparks and continuous decorative motion. These fallbacks take precedence over the full renderer tier.

## Bounded CLI recognition

The recognizer observes only newly arriving output. It never scans scrollback, reprocesses complete command output, or stores an unbounded record. Providers are built in; there is no native plug-in loading or arbitrary third-party parser execution.

Each terminal control owns one optional, isolated parser/provider state. Phase 2 performs the immediately available bounded decision synchronously before the terminal write; it creates no recognition worker, service, or queue. Incremental decoding accepts arbitrary chunk boundaries, including split ANSI and UTF-8 sequences. Current-line storage, recent provider history, active Docker layer/BuildKit step records, and the UI mailbox all have hard limits defined next to the implementation. Parsing uses bounded state machines and anchored token/numeric parsing with explicit ranges; it does not use regular expressions.

When a chunk, line, ANSI sequence, provider record, or active set exceeds its bound, recognition abandons that record and preserves the original output. Ingress uses try-lock-and-drop behavior: contention preserves the complete callback, invalidates partial recognition state, and resets before the next inspection. No asynchronous decision or queued work is allowed to delay rendering while deciding whether a record is transient.

Provider buffers clear on command completion, a new prompt, pane or tab close, connection restart, settings disable, emergency override, or provider error. Malformed ANSI, malformed UTF-8, oversized lines, and unexpected state transitions fail open.

### Provider support

| Provider | Recognized progress | Phase 2 output policy |
| --- | --- | --- |
| Docker Pull | Layer states; real byte totals where present; otherwise bounded stage or indeterminate progress | Overlay only; preserve cursor-addressed and daemon output |
| Docker BuildKit | Bounded active steps and reliable current/total step fractions | Overlay only; preserve all build logs and errors |
| pip | Modern Rich two-record downloads, legacy transfers, real byte totals, speed/ETA signatures, or real percentages | High-confidence single-line transient transfer frames may be replaced |
| Git | Counting, compressing, receiving, resolving, and updating phases using Git's real percentage or current/total values | High-confidence carriage-return percentage frames may be replaced |
| curl and wget | Standard transfer meters using real percentage and byte totals; indeterminate when length is unknown | High-confidence single-line carriage-return transfer frames may be replaced |
| npm, pnpm, and yarn | Resolve, fetch, link, build, postinstall, completion, and failure stages; determinate only for explicit counts or percentages | Overlay only; preserve warnings, audit findings, scripts, errors, and summaries |
| nvm | Download, extract, install, switch, completion, and failure stages; delegated transfers reuse bounded transfer parsing | Overlay only |
| Maven | Tagged or untagged resolver transfers plus dependency/build stages | Overlay only; never estimate overall build percentage |
| Gradle | Task state, dependency transfer, and actual emitted percentage or counts | Overlay only; never estimate overall build percentage |
| Generic fallback | Anchored percentages, current/total, transfer speed with ETA, and repeated transient/spinner-like lines | Overlay only and never suppressible |

Maven and Gradle use determinate mode only for a real transfer or explicit execution value. Stage names alone select indeterminate mode. Git phase regression is preserved as a real phase reset rather than converted into a fabricated overall percentage.

Modern Rich pip may emit a sized archive announcement followed by an otherwise untagged meter such as a shared-unit `current/total MB` value with transfer rate and ETA. A bounded wheel announcement can establish pip context directly; generic source-archive extensions require an existing pip claim from an explicit signature such as `Collecting`. The following meter supplies the real value. Archive names are inspected only as bounded signatures and are not retained. Summaries remain ordinary terminal output.

Maven Resolver output is not always prefixed with `[INFO]`. Anchored `Downloading from ...`, `Downloaded from ...`, and `Progress (n): current/total unit` records can bootstrap the Maven provider without a prior tagged line. Repository URLs and artifact names are not retained, and every Maven record remains overlay-only.

The Generic fallback recognizes anchored real percentages or counts, transfer speed plus ETA even when no total is available, transient single-character `|`, `/`, `-`, or `\` spinners, and repeated carriage-return records with the same structural shape. A repeated transient shape must be observed again before it produces indeterminate progress. Generic recognition is always overlay-only, never suppresses output, and cannot replace an active high-confidence built-in provider. When the next complete record no longer resembles progress, a structural hidden update clears the Generic overlay immediately without retaining or logging that record.

## Safe transient replacement preview

The JSON-only preview setting is:

```json
{
  "visualProgress.enabled": true,
  "visualProgress.replaceRecognizedOutput": false
}
```

`visualProgress.replaceRecognizedOutput` defaults to `false`. While it is false, every provider is overlay-only and the original output is unchanged.

`visualProgress.enabled` also retains its Phase 1 developer-preview default of `false`. Phase 2 adds no polished Settings page for either option.

When it is true, only high-confidence, immediately recognized, single-line transient progress from **pip, Git, curl, or wget** is eligible for replacement. The known provider must own the record; the record must be a carriage-return, erase-line, spinner, or equivalent temporary frame; parser state must be healthy and within bounds; the pane must be in a compatible normal terminal mode; and the progress system and renderer must be enabled. Replacement is disallowed if removing a frame could disturb later cursor addressing.

Docker Pull, BuildKit, npm/pnpm/yarn, nvm, Maven, Gradle, and generic recognition remain overlay-only in Phase 2. In particular, generic output is never suppressible, and Docker's multi-line terminal-control behavior is not removed.

Ordinary newline-terminated logs, warnings, errors, stack traces, test or compilation failures, authentication/password/confirmation prompts, remote messages, package and build summaries, install summaries, Docker daemon errors, Maven `BUILD SUCCESS` or `BUILD FAILURE`, Gradle summaries, alternate-screen output, unknown output, and malformed or incomplete ANSI are always preserved.

Late or contended recognition, low confidence, parser overflow, renderer unavailability or failure, settings disable, lifecycle-generation changes, and incompatible terminal modes all preserve the original bytes. Suppression is deliberately limited to an unambiguous record wholly contained in the current output callback; fragmented or trailing-carriage-return callbacks remain visible even if their completed record can update the overlay. Only ephemeral progress frames are candidates for removal from the visible buffer; the terminal never waits for recognition and ordinary output is never discarded to protect the effect.

## Privacy and emergency override

Recognition is local and in-memory. WinTerm does not upload or persist terminal content and adds no progress telemetry. Diagnostics may contain only structural categories such as provider ID, parser state, dropped-event count, overflow, provider disable, or renderer tier. They must not contain command lines, package or image names, paths, URLs, usernames, hostnames, environment variables, credentials, clipboard data, or terminal output.

`WINTERM_DISABLE_VISUAL_PROGRESS=1` is authoritative. Set it before launching winTerm to prevent renderer, recognizer, compositor animation, and suppression initialization regardless of JSON settings. With the override active, all original output passes through unchanged.

## Manual demo

Enable `visualProgress.enabled`, restart winTerm, and run the synthetic demo in a winTerm pane:

```powershell
.\scripts\winterm\invoke-visual-progress-smoke.ps1
```

The script uses only PowerShell output and OSC sequences. It requires no Docker, Node, Python, Maven, Gradle, provider CLI, network access, or download. It exercises determinate values at 0, 1, 50, 99, and 100 percent; a real regression; indeterminate, waiting, success, error, cancellation, and clear; and sanitized carriage-return samples for every built-in provider and the generic fallback.

For replacement preview testing, first run with `visualProgress.replaceRecognizedOutput` false and confirm every synthetic line remains visible. Then set it to true, restart, and confirm only eligible pip, Git, curl, and wget transient frames can disappear. All summaries and every overlay-only provider sample must remain visible.

For pane eligibility, split the window, run the demo or bounded soak in both panes, and move focus between them. Confirm that only the active pane emits sparks, inactive panes retain a simplified presentation, and both panes keep independent values. Move to another tab and minimize or deactivate the window to confirm hidden and unfocused animation pauses or simplifies.

For Reduced Motion, disable animation in Windows accessibility settings, restart winTerm, and rerun the demo. Values should update directly with no moving gradient, comet, breathing, sweep, or sparks. Enable a Windows contrast theme and rerun to verify the solid High Contrast treatment. Restore the system settings after testing.

The optional soak is manually bounded to at most 10,000 iterations and is not part of ordinary CI:

```powershell
.\scripts\winterm\invoke-visual-progress-smoke.ps1 -DelayMilliseconds 0 -SoakIterations 1000
```

The soak repeatedly publishes real 0–100 determinate values and periodic provider frames, then clears progress. It writes no screenshots, recordings, binaries, or build artifacts.

Where a corresponding CLI is already installed, optional real-world checks may be run in a disposable directory. Suitable command shapes include `docker pull <public-image>`, `python -m pip download <public-package>`, `git clone --progress <public-repository-url>`, `curl.exe -L --output <temporary-file> <public-url>`, `wget.exe --output-document=<temporary-file> <public-url>`, a package-manager install in a disposable project, `nvm install <version>`, `mvn package`, and `gradlew build --console=rich`. These commands may download content and are not required for the base demo.

## Validation

Run source validation first:

```powershell
.\scripts\winterm\test-visual-progress.ps1 -SourceOnly
```

After a Release build with tests, run the compiled coverage:

```powershell
.\scripts\winterm\test-visual-progress.ps1 -Configuration Release -Platform x64 -RequireCompiled
```

The ordinary test suite uses deterministic or injected timing for animation-state tests. It does not depend on wall-clock sleeps, and the longer bounded soak remains opt-in.

## Known limitations and Phase 3 hand-off

Phase 2 has no polished Settings UI, labels, speed or ETA text, command names, notifications, history, persistence, or arbitrary external providers. Recognition intentionally rejects unsupported variants and preserves their output. Docker and BuildKit replacement remains disabled. npm/pnpm/yarn, nvm, Maven, and Gradle are overlay-only. Maven and Gradle do not expose a fabricated overall percentage.

Visual appearance, DPI, theme transitions, device-loss degradation, multi-window budget behavior, and active/background sparks still require packaged-application manual QA. The default shared 24-spark budget is global; a later host may inject a narrower per-window service without weakening the global cap.

Phase 3 owns the complete adaptive performance-governor tuning, polished settings, final defaults, accessibility polish, long soak validation, release documentation, version bump, installer and portable validation, and v1.2.0 release preparation. Until then, the current source version and public release version remain 1.1.3.
