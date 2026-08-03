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
| Shell integration is working, no commands yet | `No commands yet` |
| Shell does not report complete command boundaries | `Command timeline unavailable` |

PowerShell with winTerm shell integration reports full boundaries. `cmd.exe`
does not, and winTerm does not add a prompt parser for it, so the Timeline
stays unavailable there rather than showing untrustworthy results.

## Keyboard

| Key | Action |
| --- | --- |
| `Ctrl+Tab` | Toggle the Timeline for the focused pane |
| `Up` / `Down` | Move the selection by one command |
| `Left` / `Right` | Select the first / last command on the current page |
| `Enter` | Load the selected command onto the input line |
| `Space` | Scroll the terminal to that command's output |
| `Ctrl+C` | Copy the selected command text |
| `Escape` | Close the Timeline |

Your own key bindings take precedence over these. If you have bound one of
these keys yourself, your binding wins.

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
- Nothing is sent anywhere. The Timeline writes no telemetry and logs no
  commands, output, or paths.
- The clipboard is only written when you explicitly choose a copy action, and
  is never read.
- Each command's stored text is capped at 4096 characters.
- Each pane has its own independent Timeline.

Commands scrolled out of the terminal's scrollback are dropped from the
Timeline too, so the list never outlives the buffer it describes.
