# Uninstall winTerm

Remove winTerm through **Settings → Apps → Installed apps** or the winTerm
Start Menu uninstall entry. The Inno uninstaller removes application binaries,
the Start Menu shortcut, an optional desktop shortcut, the App Paths command,
optional context-menu entries, and the winTerm uninstall record.

By default, silent and interactive uninstall preserve
`%LOCALAPPDATA%\winTerm`, including settings, themes, workspaces, snapshots,
logs, and updates. Interactive uninstall offers an optional data deletion path
with a separate second confirmation. Silent uninstall never deletes user data.

Uninstall does not remove or modify:

- Microsoft Windows Terminal or `wt.exe`;
- WSL distributions;
- global system fonts;
- PowerShell profiles or execution policy;
- CMD AutoRun settings;
- Clink, unrelated PATH entries, or other application data.

Portable mode has no uninstaller. Close winTerm and delete only the extracted
portable directory when it is no longer needed. Its data is inside that
directory while `portable.marker` exists.
