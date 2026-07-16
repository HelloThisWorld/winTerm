# winTerm-owned source boundary

This directory contains winTerm-specific integration files that should remain clearly separated from Microsoft Terminal upstream code.

The current Microsoft Terminal baseline remains `release-1.25` at commit `1cea42d433253d95c4487a3037db48197b5e72f4`.

winTerm v0.1 reuses the upstream terminal engine, ConPTY integration, renderer, settings model, profiles, tabs, panes, and window management. It adds only the minimum package-isolation, branding, build, verification, and maintenance integration required for the foundation release.

winTerm v0.4 adds a workspace-owned schema and backend under `Workspaces`, settings data under `Settings/Workspaces`, and startup resolution under `Startup`. These modules describe windows, tabs, panes, profiles, working directories, appearance references, persistence, recovery, and safe restore plans. They do not serialize process memory, terminal output, command history, clipboard data, credentials, environment dumps, or arbitrary executable commands.

The inherited TerminalApp pane startup-action layout remains the runtime bridge boundary. Visual docking, grid or empty-slot nodes, cross-window live pane transfer, a terminal broker, process reattachment, and remote process persistence are outside v0.4.
