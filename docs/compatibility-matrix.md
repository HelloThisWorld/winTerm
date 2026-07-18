# winTerm 1.0 compatibility matrix

This matrix records evidence, not aspirations. `Not tested` is never equivalent to `Supported`.

| Shell or platform | Launch | CWD | Marks | Safe commands | Completion | Workspace | Docking | Status | Evidence |
| --- | --- | --- | --- | --- | --- | --- | --- | --- | --- |
| PowerShell 7 | Not tested | Not tested | Source validated | Source validated | Not tested | Not tested | Disabled | Experimental | Requires a signed package and provisioned Windows 11 machine with `pwsh.exe`. |
| Windows PowerShell 5.1 | Source validated | Source validated | Source validated | Source validated | Not tested | Not tested | Disabled | Experimental | Profile, module, and shell-asset validation only. |
| Command Prompt | Source validated | Source validated | N/A | Source validated | N/A | Not tested | Disabled | Experimental | CMD helper, macro isolation, and precedence validation only. |
| WSL Ubuntu | Not tested | Not tested | Not tested | Disabled | Not tested | Not tested | Disabled | Experimental | Dynamic discovery is source-validated; Safe Compatibility is intentionally off. |
| WSL Debian | Not tested | Not tested | Not tested | Disabled | Not tested | Not tested | Disabled | Experimental | No installed distribution was available. |
| Git Bash | Not tested | Not tested | Not tested | Disabled | Not tested | Not tested | Disabled | Disabled | No 1.0 compatibility promise. |
| SSH | Not tested | Not tested | Not tested | Disabled | Not tested | Not tested | Disabled | Boundary only | Safe command translation is intentionally excluded. |
| Windows 11 x64 | v0.6 CI build only | Not tested | Not tested | Source validated | Not tested | Source validated | Disabled | Candidate | v1.0 signed package, install, and launch evidence is unavailable. |
| Windows 11 ARM64 | Not tested | Not tested | Not tested | Not tested | Not tested | Not tested | Disabled | Disabled | No native ARM64 artifact has been verified. |
| Windows 10 | Not tested | Not tested | Not tested | Not tested | Not tested | Not tested | Disabled | Unsupported | Outside the 1.0 Stable support commitment. |

## Terminology

- **Source validated** means static, fixture, or script checks passed; it is not a runtime claim.
- **Experimental** means a capability exists but lacks complete Stable evidence.
- **Disabled** means intentionally unavailable by default.
- **Candidate** means release engineering exists but installation and runtime gates are open.
