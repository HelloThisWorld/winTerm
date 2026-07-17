# winTerm Stable release process

## 1. Select exact source

Start from reviewed `main`, create a release branch, and record:

- repository and branch;
- commit SHA and v0.6 baseline;
- Microsoft Terminal upstream revision;
- clean working tree and untracked files;
- CI, package, signing, architecture, and blocker status.

Run:

```powershell
.\scripts\winterm\verify-version.ps1
.\scripts\winterm\test.ps1 -Suite Smoke -Configuration Release -Platform x64
```

Do not tag, publish, force-push, or overwrite user work at this stage.

## 2. Run compiled and manual gates

Build and test x64 with PowerShell 7, Visual Studio, and Windows SDK 10.0.22621.0:

```powershell
.\scripts\winterm\build.ps1 -Configuration Release -Platform x64 -IncludeTests
.\scripts\winterm\test.ps1 -Suite Relevant -Configuration Release -Platform x64
.\scripts\winterm\package.ps1 -Platform x64
```

Then validate a production-signed package on a clean Windows 11 x64 machine: install, launch, PowerShell, CMD, tabs, panes, Workspace save/restore, restart, upgrade from 0.6, uninstall, reinstall, `winterm.exe`, Windows Terminal coexistence, and continued ownership of `wt.exe` by Windows Terminal.

Complete Narrator, keyboard, contrast, scaling, privacy, diagnostics, performance, and soak evidence. ARM64 is published only after separate native evidence.

## 3. Configure protected signing

The `winterm-stable-release` GitHub environment owns:

- `WINTERM_SIGNING_PFX_BASE64` secret;
- `WINTERM_SIGNING_PFX_PASSWORD` secret;
- `WINTERM_PACKAGE_PUBLISHER` protected variable;
- `WINTERM_TIMESTAMP_URL` protected variable;
- required reviewers.

The workflow validates certificate purpose, subject, expiry, private key, timestamp, package Publisher, package identity, alias, and signature. It deletes the ephemeral PFX. Never echo or commit signing material.

## 4. Merge, tag, and prepare Draft

After every pre-tag gate is reviewed:

1. merge the release PR to `main`;
2. record the exact merge commit;
3. create annotated tag `v1.0.0` on that commit;
4. push only the tag;
5. let `.github/workflows/release.yml` build from the clean tag checkout.

The prepare mode creates a Draft, uploads an allowlist, generates Attestations, re-downloads every asset, verifies hashes, package identity, architecture, alias, Publisher, signature when present, and Attestation. It never uses `--clobber`.

If production signing is unavailable, the Draft is explicitly blocked and remains unsigned. Do not promote it.

## 5. Publish deliberately

Run workflow dispatch from the `v1.0.0` tag in `publish` mode. Supply the exact commit that passed clean install, upgrade, uninstall, alias, and coexistence validation. The protected environment reviewers confirm all manual evidence.

The workflow re-downloads the Draft, requires a valid trusted signature and timestamp, verifies checksums and Attestation, then sets Draft false, Prerelease false, and Latest. It re-downloads the public assets into a new directory and verifies hashes again.

The phase remains open until a clean Windows 11 machine installs and launches the publicly downloaded package. If that smoke test fails, do not replace the `v1.0.0` asset; record the incident and prepare `v1.0.1`.

## 6. Post-release metadata

Only after the public URL and asset hash exist:

- generate the stable update manifest with `generate-stable-update-manifest.ps1`;
- run the WinGet workflow and `winget validate`;
- update the README to link `/releases/latest`;
- record Release ID, URL, workflow run, assets, hashes, signatures, installation results, and remaining issues.
