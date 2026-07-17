# winTerm Public Beta compatibility matrix

This matrix records evidence, not aspirations. `Not tested` is never equivalent to `Supported`.

| Shell or platform | Launch | CWD | Marks | Safe commands | Completion | Workspace | Docking | Status | Evidence |
| --- | --- | --- | --- | --- | --- | --- | --- | --- | --- |
| PowerShell 7 | Not tested | Not tested | Source validated | Source validated | Not tested | Not tested | Disabled | Not tested | Requires a provisioned Windows machine with `pwsh.exe`. |
| Windows PowerShell 5.1 | Source validated | Source validated | Source validated | Source validated | Not tested | Not tested | Disabled | Partial | Smoke and shell-asset validation only. |
| Command Prompt | Source validated | Source validated | N/A | Source validated | N/A | Not tested | Disabled | Partial | Smoke and CMD helper validation only. |
| WSL Ubuntu | Not tested | Not tested | Not tested | Not tested | Not tested | Not tested | Disabled | Not tested | Dynamic discovery is source-validated. |
| WSL Debian | Not tested | Not tested | Not tested | Not tested | Not tested | Not tested | Disabled | Not tested | No installed distribution was available. |
| Git Bash | Not tested | Not tested | Not tested | Not supported | Not tested | Not tested | Disabled | Not tested | No compatibility promise is made. |
| SSH | Not tested | Not tested | Not tested | Disabled | Not tested | Not tested | Disabled | Not tested | Safe command translation is intentionally excluded. |
| Windows 11 x64 | CI build and package | Not tested | Not tested | Source validated | Not tested | Source validated | Disabled | Partial | GitHub Actions validates build and MSIX output. |
| Windows 11 ARM64 | Not tested | Not tested | Not tested | Not tested | Not tested | Not tested | Disabled | Not supported | No native ARM64 artifact has been verified. |

## Terminology

- **Source validated**: static, fixture, or script checks passed; it is not a runtime claim.
- **Partial**: a defined subset is verified and the gaps are stated above.
- **Disabled**: intentionally unavailable in the Public Beta default configuration.
