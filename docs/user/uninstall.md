# Uninstall and reset winTerm

Uninstall winTerm through Windows Apps settings. This removes winTerm application binaries, its Start entry, and `winterm.exe` registration according to MSIX behavior.

Uninstall must leave these items untouched:

- Microsoft Windows Terminal and `wt.exe`;
- WSL distributions;
- global system fonts;
- PowerShell profiles and execution policy;
- CMD AutoRun settings;
- terminal history and unrelated application data.

Local settings, named Workspaces, recovery snapshots, imported Themes, diagnostics, and logs are winTerm-owned data. A full **Settings → Reset → Delete all winTerm local data** operation must list those categories, require a second confirmation, and delete only winTerm’s package-local or `%LOCALAPPDATA%\winTerm` data. Until that reviewed UI is verified in a packaged build, users should not manually delete folders based on guessed paths.
