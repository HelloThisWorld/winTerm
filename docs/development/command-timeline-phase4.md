# Command Timeline Phase 4 — search, settings, and bounded history

Phase 4 completes the in-memory Command Timeline feature surface: pane-local
search and filtering, the two public settings, trustworthy shell degradation
states, and bounded history with oldest-first eviction.

Engineering checkpoint: `1.2.4` / `1.2.4.0`, tag `v1.2.4`. This is not a public
release; GitHub Latest stays on v1.2.0.

## Scope

| In scope | Out of scope |
| --- | --- |
| Pane-local literal search | Regex or fuzzy search |
| Filtered projection with stable identity | Output search or indexing |
| `commandTimeline.enabled` / `.historyLimit` | Persistent history |
| Settings UI under Appearance | Output cache |
| Four distinct empty states | Telemetry |
| Bounded history, oldest-first eviction | `v1.3.0-alpha` work |

## Search semantics

`CommandTimelineQueryMatches` is a case-insensitive literal substring search
implemented with `std::search` and `towlower`. It is deliberately not a regex
and deliberately not fuzzy, and `test-command-timeline.ps1` fails the build if
`std::regex`, `regex_search`, or a fuzzy matcher appears in the model.

Filtering reads `entry.cachedCommandText` only — the same bounded 4096-character
cache Phase 1 established. Output is never consulted, and no terminal-buffer
scan is triggered. `_rebuildFilter` walks the existing in-memory index; a new
command is reconciled through the same incremental path that already existed.

### Query bounds

`NormalizeCommandTimelineQuery` caps the query at
`MaxCommandTimelineQueryLength` (256 UTF-16 code units). If the cut would land
between a high and low surrogate, the orphaned lead unit is dropped, so the
result is never a lone surrogate. The XAML `TextBox` also carries
`MaxLength="256"`, and the control writes the normalized value back into the box
when truncation shortens it.

## Filtered projection

The navigation model keeps `_filtered`, a vector of indices into the caller's
entries span, and navigates over *positions within `_filtered`* rather than over
raw entry indices. An empty query fills `_filtered` with every index, so the
unfiltered case walks exactly the same code path.

Selection reconciliation:

| Situation | Result |
| --- | --- |
| Selected command still matches | Stays selected |
| Selected command stops matching | Nearest surviving match (`_nearestPosition`) |
| Following latest, new command matches | New command becomes the selection |
| Following latest, new command does not match | Selection unchanged |
| Browsing older history, new command arrives | Selection unchanged |
| No results | Selected `CommandId` retained, nothing projected |

Following-latest additionally requires that the newest command is itself in the
projection, which is what stops a non-matching new command from pulling the
selection anywhere.

Every action still resolves through `viewState.selectedCommandId`, so filtered
navigation, hover, click, wheel, copy, load, jump, and the context menu all act
on the same stable command.

Only `visibleCapacity` rows are ever materialized, whatever the size of the
history behind them.

## Settings

| Setting | Default | Range | Effect |
| --- | ---: | --- | --- |
| `commandTimeline.enabled` | `true` | — | Overlay and left-side handle |
| `commandTimeline.historyLimit` | `500` | 50–5000, integer | Per-pane history |

Plumbing, in order: `MTSMSettings.h` (`MTSM_GLOBAL_SETTINGS`) →
`GlobalAppSettings.idl` → `ControlProperties.h` → `IControlSettings.idl` →
`TerminalSettings.cpp` → `ControlCore`. Defaults live in `defaults.json`; the
JSON schema in `doc/cascadia/profiles.schema.json` carries type, default,
`minimum`, and `maximum`.

An absent setting is not written back on serialization, so an existing settings
file needs no migration. An out-of-range value is accepted by the parser and
clamped by `ClampCommandTimelineHistoryLimit`, so the runtime value is always
within 50–5000 rather than failing the whole settings load.

`UpdateSettings` applies the limit to panes that already exist, and the index
constructor applies it to new panes.

Disabling the feature hides the handle and closes an open overlay
(`_applyCommandTimelineEnabledSetting`), and `ToggleCommandTimeline` refuses to
open while disabled.

## History limit and eviction

`CommandTimelineIndex::_applyHistoryLimit` erases from the front until the
history fits, and runs on entry creation, on bootstrap, and on
`SetHistoryLimit`. Consequences, all covered by tests:

- Lowering the limit evicts immediately, oldest first.
- Raising the limit never resurrects an evicted entry.
- `_nextSequence` only ever increases, so sequence IDs are never reused.
- `ReconcileLoadedInput` releases loaded-input state when the loaded command is
  evicted.

## Shell degradation

`CommandTimelineEmptyState` distinguishes four cases so an unsupported shell is
never presented as an empty history:

| State | Condition | Message |
| --- | --- | --- |
| `WaitingForShell` | Capability `Unknown`, no entries | Waiting for shell integration |
| `ShellUnsupported` | Capability `Limited` | Command timeline unavailable |
| `NoCommands` | Capability `Full`, no entries | No commands yet |
| `NoMatchingCommands` | Query non-empty, entries exist, no matches | No matching commands |

No prompt parser, no heuristic output detection, and no ConPTY, VT parser,
TextBuffer, renderer, or shell protocol change.

## Input isolation

| Key | Timeline focus | Filter-box focus |
| --- | --- | --- |
| `/` | Focus filter box | Types `/` |
| Tab | Focus filter box | Types/moves per text box |
| Up / Down | Move selection | Move selection |
| Left / Right | Page edges | Caret editing |
| Enter | Load, never execute | Load, never execute |
| Escape | Cancel confirmation → clear query → close | Clear query → close |
| Ctrl+C | Copy selected command | Text box copy |

`/` and Tab are consumed by `_tryHandleCommandTimelineKey`, so neither reaches
the PTY. Filter-box text never reaches the PTY because the `TextBox` owns the
input; `_CommandTimelineSearchKeyDown` claims only Up, Down, Enter, and Escape
and leaves everything else — including IME/TSF composition — to the text box.
`_commandTimelineConsumedKeys` still de-duplicates key-down/key-up so a consumed
key never leaks on release.

`Ctrl+Tab` and user-defined key bindings keep precedence because
`_TryHandleKeyBinding` runs before the Timeline handler.

## Accessibility

- The filter box has a localized accessible name and placeholder.
- List item `PositionInSet` and `SizeOfSet` use the **filtered** result count,
  so assistive technology announces a position within the matches.
- Empty states are localized resources, and status is never conveyed by color
  alone.
- The overlay uses `{ThemeResource}` brushes for High Contrast and adds no
  storyboard or continuous animation.
- Geometry is in device-independent pixels; the overlay changes no terminal
  rows/columns, pane size, PTY size, swap-chain size, or padding.

## Performance evidence

`SearchStressAtMaximumHistoryLimit` builds a full 5000-entry history, then:

- Filters with a query matching all 5000 — projection reports 5000 matches while
  materializing exactly `visibleCapacity` (20) rows.
- Runs 25 passes of narrow → no-result → broad filtering, asserting the
  projection and the materialized row count return to their expected values each
  pass, so repeated filtering does not accumulate.
- Asserts cached command text stays within
  `5000 * DefaultMaxCachedCommandText`.
- Closes and asserts the filtered projection is released.

Measured result is recorded in the pull request rather than described as
"performs well".

## Validation

- `scripts/winterm/test-command-timeline.ps1` — extended with Phase 4 guards for
  search literalness, query bounds, settings defaults/range/schema, Settings UI
  presence, and the filter reading only cached command text.
- `src/cascadia/UnitTests_Control/CommandTimelineTests.cpp` — the `Search*`,
  `HistoryLimit*`, and `ShellDegradation*` tests.
- `src/cascadia/UnitTests_SettingsModel/WinTermCommandTimelineTests.cpp` —
  settings defaults, JSON round-trip, runtime clamping, and out-of-range
  handling.
