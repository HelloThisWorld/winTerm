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

Then validate the self-signed package on a clean Windows 11 x64 machine: verify hashes, import the attached CER into Trusted People, install, launch, exercise PowerShell, CMD, tabs, panes, and Workspace save/restore, restart, uninstall, reinstall, run `winterm.exe`, and confirm Windows Terminal retains `wt.exe`.

Complete Narrator, keyboard, contrast, scaling, privacy, diagnostics, performance, and soak evidence. ARM64 is published only after separate native evidence.

## 3. Self-sign without a public CA

The Release workflow:

- creates a one-year RSA 3072-bit code-signing certificate for `CN=winTerm Development`;
- marks the private key non-exportable;
- signs the exact-tag MSIX without a public timestamp;
- exports only the public CER;
- verifies the embedded PKCS#7 signature against that CER;
- removes the private key after signing.

The public certificate is uploaded beside the installer. Never commit or upload a private key, PFX, password, token, or signing-service credential.

## 4. Merge, tag, and prepare Draft

After every pre-tag gate is reviewed:

1. merge the release PR to `main`;
2. record the exact merge commit;
3. create annotated tag `v1.0.2` on that commit;
4. push only the tag;
5. let `.github/workflows/release.yml` build from the clean tag checkout.

The workflow creates a Draft, uploads only the allowlisted installer, public CER, installation instructions, checksums, notices, SBOMs, symbols, metadata, and notes, then generates Attestations. It re-downloads every asset and verifies hashes, package identity, architecture, alias, Publisher, self-signed signature, public certificate, and Attestation. It never uses `--clobber`.

## 5. Publish deliberately

After the Draft assets pass re-download verification, the same exact-tag workflow sets Draft false, Prerelease false, and Latest. It then downloads the public assets into a new directory and repeats package, certificate, signature, checksum, and Attestation verification.

The installer is self-signed and not publicly trusted. Users must verify the repository URL and hashes before importing the attached CER into Trusted People. If the public package fails verification or installation, never replace the `v1.0.2` asset; record the incident and prepare a new version and Tag.

## 6. Post-release metadata

Only after the public URL and asset hash exist:

- generate the stable update manifest with `generate-stable-update-manifest.ps1`;
- run the WinGet workflow and `winget validate`;
- update the README to link `/releases/latest`;
- record Release ID, URL, workflow run, assets, hashes, signatures, installation results, and remaining issues.
