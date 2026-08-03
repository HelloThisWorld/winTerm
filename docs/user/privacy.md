# Privacy

See the repository [privacy policy](../../PRIVACY.md). winTerm does not collect
command text, terminal output, clipboard content, Workspace content, or general
usage analytics. Visual Progress recognition is bounded, local, and in-memory;
it does not upload or persist terminal content, and recognized-output
replacement is off by default.

## Command Timeline

The [Command Timeline](command-timeline.md) keeps a per-pane list of commands
entirely in memory:

- Command text lives only in the pane that ran it and only while that pane is
  open. Nothing is written to disk; there is no history file or database.
- Command output is never cached, indexed, or searched. Output is read from the
  terminal buffer only when you explicitly choose to copy it, and is not
  retained afterwards.
- Filter text is never persisted and never leaves the pane. Closing the Timeline
  releases it.
- Each command's stored text is capped at 4096 characters, and each pane keeps
  at most `commandTimeline.historyLimit` commands (default 500, maximum 5000).
- The clipboard is written only by an explicit copy action and is never read.
  Loading a command onto the input line does not use the clipboard.
- No telemetry is written, and no command, output, path, or filter text is
  logged.
