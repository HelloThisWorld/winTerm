# Command Timeline Phase 3 — entry actions

Phase 3 turns the read-only Phase 2 overlay into something you can act on. It
adds loading a command onto the input line, copying a command or its output,
and jumping to a command's output, without introducing persistence, an output
cache, or any automatic execution.

Engineering checkpoint: `1.2.3` / `1.2.3.0`, tag `v1.2.3`. This is not a public
release; GitHub Latest stays on v1.2.0.

## Scope

| In scope | Out of scope |
| --- | --- |
| Load selected command into the pane input | Executing a command |
| Copy command text | Persisting history |
| Copy command output, resolved on demand | Caching or indexing output |
| Jump viewport to a command's native mark | Searching output |
| Per-entry context menu | Search and filtering (Phase 4) |
| Execution-generation race protection | Public settings (Phase 4) |

## Layering

Phase 3 keeps the same three-layer split as Phase 1 and 2.

- `src/winterm/CommandTimeline/CommandTimelineModel.h/.cpp` —
  `CommandTimelineActionModel` is pure C++ with no WinRT or buffer access. It
  decides whether an action is possible and returns a `CommandActionRequest`
  describing it. It never resolves output, never touches the clipboard, and
  never produces a payload containing a carriage return.
- `ControlCore` — owns the pane's action model instance, applies the request
  against the live terminal, and is the only place that reaches the connection,
  the clipboard event, or the buffer.
- `TermControl` — routes keyboard, click, and context-menu input into the core
  and renders the resulting status text.

## Load semantics

`ControlCore::LoadCommandTimelineCommand` is the only load path.

1. The payload is filtered with `FilterStringForPaste(text, ControlCodes)`.
   `CarriageReturnNewline` is deliberately **not** requested: converting a
   newline into a carriage return would submit the command rather than load it.
2. No carriage return is ever appended.
3. When the shell has bracketed paste enabled the payload is wrapped in
   `ESC [200~` / `ESC [201~` so the shell treats it as literal text.
4. The payload goes to `SendInput`, which writes to this pane's connection
   only. The Windows clipboard is never read, and input broadcast has no
   opportunity to forward the load to another pane.

### Multi-line and large loads

| Condition | Result |
| --- | --- |
| Command text is empty | `CommandTextUnavailable` |
| Multi-line, bracketed paste **off** | `MultilineUnsafe` — refused |
| Multi-line, bracketed paste **on** | Loads literally |
| Longer than 1024 characters | `ConfirmationRequired` — Enter again to confirm |

Refusing a multi-line load without bracketed paste is stricter than clipboard
paste, which only warns. The reasoning matches the upstream note on GH#13014:
without bracketing, embedded line breaks are indistinguishable from the user
pressing Enter, and the Timeline must never cause execution.

A pending confirmation is cleared by Escape, by moving the selection, and by
closing the overlay, so a stale confirmation can never apply to a different
command.

## Stable identity

Every action resolves through `viewState.selectedCommandId`, a stable
pane-scoped `CommandId`, never through the XAML row index. The context menu
re-selects the slot and then acts on whatever `CommandId` that slot resolves
to at that moment. `ActionCopyResolvesStableCommandIdNotRowIndex` covers the
case where an eviction shifts every row index by one between preparing and
re-preparing a request.

## Execution generation

`CommandTimelineViewState` carries `loadedCommandId`, `loadedIntoInput`, and
`executionGeneration`.

- `NotifyLoaded` records the loaded command and increments the generation.
- `NotifyExecutionStarted` fires on the OSC 133 `CommandStart` lifecycle event,
  clears the loaded state, and increments the generation again.
- `IsCurrentGeneration(viewState, generation)` reports whether work that began
  at an earlier generation is still relevant.

`ResolveCommandTimelineOutput` checks the generation before reading the buffer,
so a completion belonging to a retired command cannot deliver output attributed
to the current one.

`ReconcileLoadedInput` drops the loaded state when the loaded command has been
evicted, so loaded-input state never points at a removed command.

## Output resolution

Output is resolved only by an explicit copy-output action:

`Terminal::ResolveCommandTimelineOutput(identity)` locates the mark by identity
via `FindCommandTimelineMarkExtents`, then reads
`GetPlainText(commandEnd, outputEnd)`. Nothing is stored. Both output actions
require a live native range; a stale range reports `OutputUnavailable`.

The Timeline still holds only bounded command text (4096 characters per entry,
`DefaultMaxCachedCommandText`). There is no output field anywhere in the model,
and `test-command-timeline.ps1` fails the build if one appears.

## Keyboard

| Key | Behavior |
| --- | --- |
| Up / Down | Move selection by one entry |
| Left / Right | Select first / last entry on the current page |
| Enter | Load selected command; never executes |
| Space | Jump viewport to the selected command's output |
| Ctrl+C | Copy selected command text |
| Escape | Cancel a pending confirmation, otherwise close |

`Ctrl+Tab` still toggles the Timeline, and user-defined key bindings keep
precedence over these bare keys because `_TryHandleKeyBinding` runs first.
Key-down and key-up are tracked in `_commandTimelineConsumedKeys` so a consumed
key never reaches the shell on release.

## Validation

- `scripts/winterm/test-command-timeline.ps1` — source, input, action,
  accessibility, privacy, and lifecycle boundaries, including a guard that
  fails if the load path gains `CarriageReturnNewline`, a carriage-return
  append, or a clipboard read.
- `src/cascadia/UnitTests_Control/CommandTimelineTests.cpp` — the `Action*`
  tests cover load preparation, missing command text, multi-line refusal,
  large-load confirmation, generation tracking, late-completion detection,
  eviction release, stable-identity resolution, and output availability.
