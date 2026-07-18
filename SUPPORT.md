# winTerm support policy

## Supported releases

The latest public Stable winTerm release is supported. A prerelease is supported for testing and feedback only. winTerm 1.0.1 is the current Stable release.

## Platforms and architectures

- Windows 11 x64 is the intended fully supported platform after clean installation, upgrade, uninstall, and runtime validation pass.
- Windows 11 ARM64 is unsupported unless an ARM64 asset is actually published with native build, install, and launch evidence.
- Windows 10 is unsupported for the winTerm 1.0 Stable commitment.

## Shells and feature levels

PowerShell 7, Windows PowerShell 5.1, Command Prompt, and WSL have different evidence levels recorded in `docs/compatibility-matrix.md`. Experimental and Disabled features are listed in `docs/feature-status.md`; they do not receive the same compatibility promise as Stable features.

## Getting help

Use the [winTerm GitHub Issue chooser](https://github.com/HelloThisWorld/winTerm/issues/new/choose) for reproducible bugs, crashes, compatibility reports, and feature requests. Remove credentials, private terminal output, commands, proprietary source, and unredacted Workspace files before posting.

## Security

Do not report a vulnerability in a public Issue. Follow `SECURITY.md` and use GitHub Private Vulnerability Reporting when available.

## Release cadence and data compatibility

No fixed release cadence is promised. The latest Stable release receives priority for correctness and security fixes. Data compatibility follows `docs/compatibility-policy.md` and `docs/schema-support-policy.md`; winTerm 1.x reads winTerm 1.0 settings and Workspace Schema version 2 unless an explicit deprecation and migration policy is published.
