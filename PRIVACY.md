# winTerm privacy policy

winTerm does not collect general usage analytics. It does not collect or upload command text, terminal output, clipboard content, Workspace contents, working-directory paths, shell usage frequency, Theme selection, or feature-usage events.

## Network and optional features

- **Update checks are off.** winTerm 1.0 contains no enabled update request path because explicit user consent is not yet implemented. A future opt-in request may contain only application version, architecture, release channel, and public release metadata.
- **Crash-report upload is off by default** and remains opt-in. Local crash metadata is not uploaded unless the user explicitly chooses a reviewed, redacted report.
- **Diagnostic bundles are created only when the user requests one.** They exclude terminal output, commands, clipboard data, environment variables, SSH configuration, full settings, raw Workspace files, full paths, tokens, email addresses, and connection strings by default.

winTerm does not upload Workspaces or silently contact a telemetry service on first launch.

## Local data

winTerm may store settings, named Workspaces, recovery snapshots, imported Themes, app-private font metadata, local diagnostics, and logs inside its own application-data boundary. Logs must not include commands, terminal output, or clipboard content. winTerm does not modify Windows Terminal settings or data.

## User control

Diagnostic bundles are generated and shared by the user. Review a bundle before attaching it to a report. Crash upload, if introduced after 1.0, must remain disabled until the user opts in and must be independently reversible.

## Removing local data

MSIX uninstall removes application binaries according to Windows package behavior. See [uninstall guidance](docs/user/uninstall.md) before deleting remaining winTerm-specific local data. Never delete Windows Terminal data, PowerShell profiles, WSL distributions, shell history, global fonts, or unrelated user folders to remove winTerm.
