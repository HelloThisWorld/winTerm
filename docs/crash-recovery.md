# Crash recovery

winTerm workspace recovery uses a running marker, a clean-shutdown marker, the last valid session file and backup, and retained snapshots. Startup must inspect the previous markers before writing the new running marker.

A normal shutdown stops accepting workspace mutations, captures the final generation, atomically saves Last Session, flushes it, writes the clean marker, removes the running marker, and only then completes window shutdown. A crash leaves the running marker and therefore cannot be described as a normal exit.

Snapshots are written on the periodic policy and before destructive or migration operations. The default retention is the newest three valid files. Snapshot names contain a fixed-width generation so selection is deterministic. If the newest snapshot is corrupted, recovery examines older snapshots. Loading a damaged Last Session tries the last validated backup; damaged managed files can be moved to quarantine with sanitized reason metadata.

Recovery inspection recommends the latest valid snapshot after an abnormal shutdown. Following a clean shutdown, it recommends Last Session; when neither is available, it recommends a new session. It never overwrites normal state without a later explicit recovery choice. A recovered pane can report that a command was running, but no command is stored or replayed and no process is reattached.

Marker, backup, retention, latest-valid selection, and recommendation logic are implemented and source-tested. The recovery prompt and forced-termination manual scenario require a built application.
