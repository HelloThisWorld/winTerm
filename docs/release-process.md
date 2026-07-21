# winTerm release process

## 1. Select exact source

Start from reviewed `main`. Record the clean commit, application version,
Microsoft Terminal upstream revision, open blockers, and existing tags and
Releases. Never replace an existing tag or Release asset.

```powershell
.\scripts\winterm\verify-version.ps1
.\scripts\winterm\verify-branding.ps1 -ExpectedPublisher 'CN=helloThisWorld'
.\scripts\winterm\test.ps1 -Suite Smoke -Configuration Release -Platform x64
```

## 2. Complete build and runtime gates

```powershell
.\scripts\winterm\build.ps1 -Configuration Release -Platform x64 -IncludeTests
.\scripts\winterm\test.ps1 -Suite Relevant -Configuration Release -Platform x64
.\scripts\winterm\build-unpackaged.ps1 -Configuration Release -Platform x64
.\scripts\winterm\build-installer.ps1 -Version <version> -Platform x64
.\scripts\winterm\build-portable.ps1 -Version <version> -Platform x64 -SkipBuild
```

Run current-user and elevated all-users installer round trips. On a clean
Windows 11 x64 machine, verify PowerShell, CMD, WSL discovery, themes, fonts,
ANSI true color, Emoji, workspaces, directed splits, pane controls, session
preservation, Portable launch, and portable data mode. Confirm Microsoft
Windows Terminal and `wt.exe` are unchanged.

## 3. Signing

If a trusted Authenticode certificate is configured, Release CI imports it into
the ephemeral current-user store, signs the Setup EXE, applies a trusted
timestamp, and verifies the signature. The PFX is deleted immediately and is
never an artifact.

Without a trusted certificate, the workflow produces an explicitly unsigned
release. Release notes must state that Windows can show Unknown Publisher or a
SmartScreen warning and direct users to `SHA256SUMS.txt`. A shared test
certificate must never be presented as production signing.

## 4. Tag and Draft Release

After every pre-tag gate passes, create a new version tag matching
`src/winterm/Branding/version.json` and push only that tag. The generic release
workflow rejects mismatched tags, dirty source, and any tag that already has a
GitHub Release.

It builds the unpackaged stage, Setup EXE, and Portable ZIP; runs both installer
scopes; generates SBOMs and hashes; creates an allowlisted Draft Release; then
re-downloads and tests every Draft asset. It never uses `--clobber`.

## 5. Publish and verify

Only after all Draft gates pass does the workflow publish the Release. It then
downloads the public assets into a fresh directory and repeats layout, hash,
signature-state, current-user/all-users installation, upgrade, uninstall,
reinstall, and Windows Terminal isolation checks.

If any public verification fails, record an incident and prepare a new version;
never silently replace an already published asset.

## 6. WinGet

Only a public Release can trigger final WinGet manifest generation. The workflow
downloads the real Setup EXE, computes its SHA-256, creates `InstallerType:
inno` manifests for user and machine scope, runs the pinned `winget validate`,
and uploads the validated manifests for separate manual submission.
