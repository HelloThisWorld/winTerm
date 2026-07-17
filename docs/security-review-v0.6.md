# winTerm v0.6 security review

## Confirmed boundaries

- Workspace and theme imports are data-only and reject embedded commands, executables, remote includes, invalid paths, oversized files, and malformed JSON fixtures.
- Docking drag tokens are opaque, expiring, and single-use. Cross-process transfer is rejected; there is no broker or shell-restart fallback.
- Paste-risk analysis is non-executing. winTerm does not log clipboard content.
- Diagnostics are restricted to redacted categories and must not include terminal output, commands, workspace JSON, credentials, clipboard, or full paths.
- Automatic update installation is not implemented. Any future update check must be user opt-in and open an official release after manifest, identity, version, and hash validation.

## Required Public Beta verification

| Area | Required evidence | Status |
| --- | --- | --- |
| Shell integration | PowerShell 7, Windows PowerShell, CMD, WSL, SSH, and REPL boundary tests | Partial |
| Import security | Fixture validation and size limits | Source validated |
| Package integrity | MSIX identity, alias, payload, and hash verification | CI validated for x64 development MSIX |
| Diagnostic redaction | Unit coverage for paths, tokens, email, and connection strings | Pending implementation |
| Upgrade and uninstall | Clean-machine installation exercises | Not tested |

Unresolved high-severity issues block a Public Beta release.
