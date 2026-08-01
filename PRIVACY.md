# winTerm privacy policy

winTerm does not collect general usage analytics. It does not collect or upload command text, terminal output, clipboard content, Workspace contents, working-directory paths, shell usage frequency, Theme selection, or feature-usage events.

## Network and optional features

- **Update checks are off.** Current winTerm releases contain no enabled update request path because explicit user consent is not yet implemented. A future opt-in request may contain only application version, architecture, release channel, and public release metadata.
- **Crash-report upload is off by default** and remains opt-in. Local crash metadata is not uploaded unless the user explicitly chooses a reviewed, redacted report.
- **Diagnostic bundles are created only when the user requests one.** They exclude terminal output, commands, clipboard data, environment variables, SSH configuration, full settings, raw Workspace files, full paths, tokens, email addresses, and connection strings by default.

winTerm does not upload Workspaces or silently contact a telemetry service on first launch.

## Visual Progress recognition

Visual Progress recognition runs locally and in memory.
Newly arriving output uses bounded inspection. It does not scan scrollback.
Terminal content is
not uploaded, not persisted, and not logged. It emits no Visual Progress
telemetry. Provider names, package and image names, paths, URLs, usernames,
hostnames, environment variables, credentials, and clipboard data are not
retained for the overlay. Disabling CLI recognition stops this inspection and
leaves terminal output unchanged.

Recognized-output replacement is off by default. When a user enables it, only
an unambiguous high-confidence transient frame from a small allowlist of
providers can be removed from the visible terminal stream; ordinary output,
warnings, errors, prompts, summaries, malformed or unsupported data, and all
uncertain records pass through unchanged. This processing remains local and
does not alter the privacy or network policy.

Visual Progress diagnostics, when explicitly requested through the existing
diagnostic flow, may contain bounded structural categories such as provider ID,
parser state, dropped-event count, overflow, provider disable, or renderer
tier. They must not contain commands, terminal output, package names, paths,
URLs, usernames, hostnames, environment variables, credentials, or clipboard
content. Set `WINTERM_DISABLE_VISUAL_PROGRESS=1` before launch to disable the
renderer, recognizer, animation, and replacement regardless of saved settings.

## Local data

winTerm may store settings, named Workspaces, recovery snapshots, imported Themes, app-private font metadata, local diagnostics, and logs inside its own application-data boundary. Logs must not include commands, terminal output, or clipboard content. winTerm does not modify Windows Terminal settings or data.

## User control

Diagnostic bundles are generated and shared by the user. Review a bundle before attaching it to a report. Crash upload, if introduced in a future release, must remain disabled until the user opts in and must be independently reversible.

## Removing local data

The Inno uninstaller removes winTerm application files and registrations while preserving `%LOCALAPPDATA%\winTerm` by default. See [uninstall guidance](docs/user/uninstall.md) before deleting winTerm-specific local data. Never delete Windows Terminal data, PowerShell profiles, WSL distributions, shell history, global fonts, or unrelated user folders to remove winTerm.
