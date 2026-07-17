# winTerm privacy policy

winTerm does not collect general usage analytics. It does not collect or upload command text, terminal output, clipboard content, Workspace contents, directory paths, shell usage frequency, theme selection, or feature-usage events.

## Optional features

- **Update checks** are off by default. A future enabled check may request only the current application version, architecture, selected release channel, and public release metadata.
- **Crash-report upload** is off by default. Crash metadata remains local unless the user explicitly chooses to share a redacted report.
- **Diagnostic bundles** are created only when the user requests one. They exclude terminal output, commands, clipboard data, environment variables, SSH configuration, full settings, and raw Workspace files by default.

## Local data

winTerm may store settings, named Workspaces, recovery snapshots, imported themes, app-private font metadata, local diagnostics, and logs in its own application-data boundary. It does not modify Windows Terminal settings or data.

## Removing local data

MSIX uninstall follows Windows platform behavior for application binaries. See [uninstall guidance](docs/user/uninstall.md) before deleting any remaining winTerm-specific local data. Never delete Windows Terminal data, PowerShell profiles, WSL distributions, shell history, or unrelated user folders to remove winTerm.
