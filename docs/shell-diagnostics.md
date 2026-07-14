# Shell integration diagnostics

PowerShell exposes `Get-WinTermShellDiagnostics` and `Test-WinTermShellIntegration`. They report:

- shell type and version;
- integration and protocol versions;
- session marker and prompt wrapper state;
- current-directory and command-mark providers;
- PSReadLine and completion availability;
- current compatibility mode; and
- a concise last integration failure category.

Diagnostics intentionally omit command history, command text, arguments, clipboard data, terminal output, environment dumps, tokens, passwords, and full user paths. CMD diagnostics are limited to its process markers, available helper, prompt configuration, and DOSKEY conflicts.

An integration failure must leave the shell usable. The usual resolution is to verify that the package shell assets exist, launch an explicit profile that sets the session marker, and import the installed module under the user's existing execution policy. Diagnostics must not write a profile or registry entry to repair an installation.
