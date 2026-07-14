# winTerm release process

## 1. Establish the release input

- Start from a clean, reviewed branch.
- Record the exact Microsoft Terminal upstream commit.
- Confirm `git diff` contains no generated build output, certificate, key, credential, unmanifested font or theme, or downloaded logo.
- Confirm the release contains no post-v0.2 or out-of-scope feature work.

Run source validation first:

```powershell
.\scripts\winterm\test.ps1 -Suite Smoke
```

## 2. Set release identity

For a local development package, the v0.3 manifest uses version `0.3.0.0` and publisher `CN=winTerm Development`.

For a public release:

1. Update the package version according to the chosen release channel.
2. Replace the manifest publisher with the real publisher identity.
3. Use a code-signing certificate with a subject that exactly matches that publisher.
4. Recheck package family, upgrade, uninstall, and side-by-side behavior.

Do not commit a PFX, private key, password, certificate export, Store credential, or CI secret.

## 3. Build and test x64

Use a provisioned Windows 11 x64 machine and PowerShell 7:

```powershell
.\scripts\winterm\build.ps1 -Configuration Debug -Platform x64 -IncludeTests
.\scripts\winterm\test.ps1 -Suite Relevant -Configuration Debug -Platform x64
.\scripts\winterm\build.ps1 -Configuration Release -Platform x64 -IncludeTests
.\scripts\winterm\test.ps1 -Suite Relevant -Configuration Release -Platform x64
```

Run `-Suite Full` for release candidates when runner time permits. Record all failures, skips, warnings, and test binaries used.

## 4. Create and inspect the package

```powershell
.\scripts\winterm\package.ps1 -Platform x64
.\scripts\winterm\verify-branding.ps1 -PackageOutput <path-to-winTerm.msix>
```

The wrapper intentionally generates an unsigned MSIX. It reports the artifact under `src/cascadia/CascadiaPackage/AppPackages`, validates the embedded identity and alias, and reports signing status.

Sign only in a controlled release environment. A typical Windows SDK operation is:

```powershell
signtool.exe sign /fd SHA256 /f <path-to-certificate.pfx> <path-to-winTerm.msix>
```

Supply passwords through an approved secret mechanism, not a command committed to this repository. Verify the result:

```powershell
Get-AuthenticodeSignature <path-to-winTerm.msix>
```

## 5. Install and perform acceptance

Trust only the intended development or release certificate, then install the signed package:

```powershell
Add-AppxPackage -Path <path-to-winTerm.msix>
```

If MSBuild emits an `Add-AppDevPackage.ps1` helper, inspect its certificate and script before running it. The package script never installs a certificate automatically.

Complete every manual item in `docs/v0.2-acceptance.md`, including side-by-side installation with Microsoft Windows Terminal, aliases, first-launch state, profiles, tabs, panes, settings, Theme import and export, app-private fonts, command palette, clipboard, CJK input, ANSI colors, emoji, Powerline, upgrade, and uninstall behavior.

## 6. Publish deliberately

- Publish only reviewed, signed artifacts and their checksums.
- State architecture, version, upstream baseline, publisher, signing status, and known limitations.
- Keep CI free of Store publishing and real signing credentials.
- Do not overwrite an existing release or automatically publish to a Store.
- Tag and release only after the acceptance checklist is complete.

ARM64 must not be advertised until its build, tests, package, install, and manual acceptance have been run independently.
