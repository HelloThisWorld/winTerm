# Workspace capture

Workspace capture begins with a value-only snapshot assembled from window, tab, and pane state. Live XAML, Pane, TermControl, connection, and process objects are never serialized. The capture service applies privacy settings before a snapshot reaches persistence.

For each pane, the working-directory selection order is fresh Shell Integration metadata, last known Shell Integration metadata, profile starting directory, process launch directory, and user home. The descriptor records both the namespace and source. Remote metadata is omitted by default. If working-directory saving is disabled, both the current directory and starting-directory fallback are removed.

Capture includes stable window, tab, and pane IDs, active references, the pane/split layout, profile references, theme and font overrides, and only `lastExitCode`, `wasCommandRunning`, and `endedUnexpectedly` session flags. It never reads or writes command text, command history, terminal output, clipboard content, environment variables, credentials, SSH state, or process memory.

Structural changes use a 750 ms debounce. Window move/resize and CWD changes use 1000 ms. Final exit capture is immediate. Periodic recovery snapshot policy defaults to 30 seconds. Mouse-move and resize events only mark state dirty; they do not write each event.

The dirty tracker assigns a generation to every mutation. Serialization and disk I/O may finish out of order, but completion is accepted only when it matches the newest generation. This prevents generation 42 from overwriting generation 43.

The current source contains the capture/privacy service and scheduler policy. Actual UI event wiring and performance validation require a built winTerm package and remain open in `v0.4-acceptance.md`.
