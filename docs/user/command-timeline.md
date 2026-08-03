# Command Timeline

The Command Timeline is a per-pane list of the commands that pane has run. Open
it with `Ctrl+Tab`, or with the small handle on the left edge of the terminal.
Use it to find an earlier command and put it back on your input line without
retyping it.

The Timeline is an in-development feature. It ships in engineering checkpoint
builds; the current public release is v1.2.0.

## What it shows

Each row is one command, with a status glyph and a status word:

| Status | Meaning |
| --- | --- |
| Running | The command is still running |
| Succeeded | The shell reported exit code 0 |
| Failed | The shell reported a non-zero exit code |
| Cancelled | The command was interrupted |
| Unknown | The shell did not report a trustworthy result |

Status is never conveyed by color alone, and a result is only shown as
succeeded or failed when the shell actually reported it. If the shell's command
boundaries were incomplete, the row reads Unknown rather than guessing.

## Shell requirements

The Timeline reads command boundaries from OSC 133 shell integration. It never
guesses where a prompt or output begins.

| Situation | What you see |
| --- | --- |
| winTerm has not determined the shell's capability yet | `Waiting for shell integration` |
| Shell does not report complete command boundaries | `Command timeline unavailable` |
| Shell integration is working, no commands yet | `No commands yet` |
| A filter is active and nothing matches | `No matching commands` |

PowerShell with winTerm shell integration reports full boundaries. `cmd.exe`
does not, and winTerm does not add a prompt parser for it, so the Timeline
stays unavailable there rather than showing untrustworthy results.

## Keyboard

| Key | Action |
| --- | --- |
| `Ctrl+Tab` | Toggle the Timeline for the focused pane |
| `/` or `Tab` | Move focus to the filter box |
| `Up` / `Down` | Move the selection by one command |
| `Left` / `Right` | Select the first / last command on the current page |
| `Enter` | Load the selected command onto the input line |
| `Space` | Scroll the terminal to that command's output |
| `Ctrl+C` | Copy the selected command text |
| `Escape` | Clear the filter, or close the Timeline if it is already empty |

Your own key bindings take precedence over these. If you have bound one of
these keys yourself, your binding wins.

Nothing you type while the Timeline has focus reaches the shell. `/` and `Tab`
move focus to the filter box instead of being sent, and the text you type into
the filter box stays in the filter box.

## Filtering

Press `/` or `Tab` and start typing to narrow the list to commands containing
what you typed.

- Matching is a plain, case-insensitive substring match. `git` matches
  `GIT status`. There are no wildcards, no regular expressions, and no fuzzy
  matching — `g.*s` matches only the literal text `g.*s`.
- Only command text is searched. Command output is never searched.
- Queries are limited to 256 characters.
- The filter is not remembered. Closing the Timeline clears it.

While filtering, `Up` and `Down` move between matches, and `Enter` still loads
the selected command without running it. If the command you had selected stops
matching, the closest remaining match is selected instead. If nothing matches,
you see `No matching commands` and your selection is left alone.

## Settings

Settings → Appearance → Command timeline:

| Setting | Default | Range |
| --- | ---: | --- |
| Show command timeline (`commandTimeline.enabled`) | On | — |
| Commands remembered per pane (`commandTimeline.historyLimit`) | 500 | 50–5000 |

Turning the Timeline off hides the handle and closes it if it is open; the
`Ctrl+Tab` shortcut stops opening it.

Lowering the history limit discards the oldest commands immediately. Raising it
again does not bring them back. Each pane counts its own history separately.

You do not need to edit your settings file for these to apply — existing panes
pick up the change straight away.

## Loading a command

Press `Enter`, or click a row once, to put the selected command on the input
line. Hovering a row only moves the selection; it does not load anything.

**A load never runs the command.** winTerm places the text on the input line
and stops there. You review it and press Enter yourself. No carriage return is
ever sent as part of the load.

A load also does not use the Windows clipboard, and it only ever reaches the
pane you loaded it from — broadcast input does not forward it to other panes.

Two cases are handled specially:

- **Multi-line commands.** If your shell has bracketed paste enabled, the whole
  command loads literally. If it does not, winTerm refuses the load and says
  so, because without bracketing the embedded line breaks would be read as you
  pressing Enter and the command would run.
- **Long commands.** Above 1024 characters, winTerm asks you to press `Enter`
  once more before loading. `Escape` cancels that confirmation without closing
  the Timeline.

## Copying and jumping

Right-click a row for:

- **Copy command** — copies the command text.
- **Copy output** — reads that command's output from the terminal buffer at
  that moment and copies it.
- **Jump to output** — scrolls the terminal to where that command's output
  begins.

Output is only ever read when you explicitly ask for it. winTerm does not keep
a copy of command output.

## Privacy and limits

- Command text lives only in memory, only for the pane that ran it, and only
  while that pane is open. Closing the pane discards it.
- Nothing is written to disk. There is no command history file or database.
- Command output is never cached, indexed, or searched.
- Filter text is never saved and never leaves the pane.
- Nothing is sent anywhere. The Timeline writes no telemetry and logs no
  commands, output, paths, or filter text.
- The clipboard is only written when you explicitly choose a copy action, and
  is never read.
- Each command's stored text is capped at 4096 characters.
- Each pane keeps at most `commandTimeline.historyLimit` commands, and each
  pane has its own independent Timeline.

Commands scrolled out of the terminal's scrollback are dropped from the
Timeline too, so the list never outlives the buffer it describes.
