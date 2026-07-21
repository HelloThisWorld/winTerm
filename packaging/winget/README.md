# WinGet manifest preparation

The package identifier is `HelloThisWorld.winTerm`, the publisher is
`helloThisWorld`, and the installer type is `inno`.

Final manifests are generated only after a GitHub Release is public. The
workflow downloads the real `winTerm-<version>-setup-x64.exe`, computes its
actual SHA-256, and constructs the exact public URL. Placeholders, Draft assets,
Actions artifact URLs, and precomputed fake hashes are rejected.

The generated multi-file manifest provides current-user and all-users scopes,
the `winterm` command, minimum Windows 11 version, Inno upgrade behavior, and
Installed Apps correlation. CI pins Windows Package Manager 1.29.280 and runs:

```powershell
winget validate --manifest <generated-version-directory> --disable-interactivity
```

The validated set is uploaded for manual review. This does not claim that a
`microsoft/winget-pkgs` pull request exists or has been accepted.
