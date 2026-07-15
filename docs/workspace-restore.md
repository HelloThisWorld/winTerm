# Workspace restore

Restore is planned in this order: parse and validate, migrate if supported, resolve monitors, create window plans, restore bounds, order tabs, rebuild the pane/split tree, resolve profiles and directories, resolve appearance, restore active tab and focused pane, and finally activate only the saved active window.

The active window is planned first. Within it, the active tab and active pane are planned first. Remaining tabs and secondary windows follow progressively. A pane failure is recorded without cancelling its tab, window, or workspace. Report states are `Restored`, `RestoredWithFallback`, `Skipped`, and `Failed`.

Profile resolution tries exact ID, stable source and profile ID, display name, shell type, the default profile, and finally the first enabled profile. It never creates a profile or accepts an imported command line.

Local directories use the saved path when it exists, the nearest existing parent, profile starting directory, and user home. UNC validation is deferred unless a background-capable caller explicitly allows it. WSL paths remain WSL paths and require the saved distribution when one was recorded. Remote paths are metadata only; restore starts a safe profile and does not execute SSH.

Theme selection is pane, tab, profile, global, then `winterm.midnight`. Font selection is pane, profile, global, `Cascadia Mono NF`, then system fallback. Missing appearance resources produce adjustments rather than startup failure.

The restore plan exposes saved layout and focus but never claims to reattach a process. If a command was running or the session ended unexpectedly, it requests a non-blocking notice. It does not replay the command.

The backend plan and resolvers are implemented. End-to-end creation of multiple real windows and settings UI reporting require a built package and are not marked accepted on the current machine.
