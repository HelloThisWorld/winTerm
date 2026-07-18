# winTerm 1.0 release checklist

Recorded on 2026-07-18. `Passed` means evidence exists. `Not available` is never treated as passed.

| Gate | Status | Evidence or reason |
| --- | --- | --- |
| Feature freeze complete | Passed | `docs/feature-status.md` and `docs/roadmap-post-1.0.md`. |
| P0 count is zero | Passed | Connected GitHub repository returned no open Issues; no known P0 was found in source review. |
| P1 count is zero or explicitly approved | Accepted limitation | v1.0.1 is intentionally self-signed; the Release notes require explicit certificate trust and do not claim public-CA signing. |
| Version is 1.0.1 everywhere | Required | Enforced by `scripts/winterm/verify-version.ps1`. |
| Full CI passes | Not available | v1.0 branch has not been pushed or run in GitHub Actions. |
| Security review passes | Not available | Static review exists; runtime evidence is incomplete. |
| Privacy review passes | Passed | Static privacy and diagnostic checks exist; end-to-end bundle inspection remains separately open. |
| Accessibility review passes | Not available | Narrator, keyboard-only, contrast, scaling, and pseudo-localization are untested. |
| Performance review passes | Not available | Packaged benchmarks and soak tests are untested. |
| x64 package builds | Required | Enforced by the full-build PR checks and exact-tag Release workflow. |
| x64 signature verifies | Required | The workflow verifies the embedded PKCS#7 signature against the attached public CER. |
| ARM64 status verified | Passed | Status is Disabled and no ARM64 artifact will be published without native evidence. |
| Clean install passes | Not available | No clean Windows 11 machine is provisioned. |
| Upgrade passes | Not available | 0.6-to-1.0 upgrade is untested. |
| Uninstall passes | Not available | Clean uninstall is untested. |
| Delete all winTerm local data passes | Not available | The required reviewed UI and packaged deletion boundary are not verified. Manual guessed-path deletion is not accepted as evidence. |
| Windows Terminal coexistence passes | Not available | Manifest isolation is source-validated; installed coexistence is untested. |
| winterm.exe works | Not available | Alias registration is source-validated; installed launch is untested. |
| wt.exe remains untouched | Partial | Manifest and payload validation prohibit `wt.exe`; installed coexistence is untested. |
| Schemas are compatible | Passed | Versions remain Workspace 2, Docking 1, Shell 1, Theme 1; policies are documented. |
| Workspace migration passes | Partial | Fixtures and source checks pass; upgrade runtime is untested. |
| SBOM generated | Not available | Generator exists; no final package input exists. |
| Checksums generated | Not available | Generator and verifier exist; no final package input exists. |
| Third-party notices current | Passed | Existing notice check passes. |
| Release notes complete | Passed | Draft notes exist and do not claim unavailable assets or tests. |
| Git tag created | Not available | Prohibited until the final release commit and all publication gates are selected. |
| Tag pushed | Not available | No tag exists. |
| Draft Release created | Automated | Exact-tag workflow refuses to overwrite an existing Release. |
| Assets uploaded | Automated | Allowlist includes the final MSIX, public CER, instructions, checksums, notices, SBOMs, symbols, metadata, and notes. |
| Assets downloaded again | Automated | Draft and public assets are downloaded into fresh directories. |
| Downloaded hashes verified | Automated | `verify-release-assets.ps1` and `verify-checksums.ps1` run after each download. |
| Draft published | Automated | Publication occurs only after Draft verification succeeds. |
| Release marked Latest | Automated | Workflow sets Latest, non-Draft, and non-Prerelease. |
| Release URL verified | Automated | Workflow reads back and records the public URL. |
| Public download smoke test passes | User validation | Installation requires importing the attached self-signed CER into Trusted People. |
| README download link updated | Passed | README links the v1.0.1 and Latest Release pages. |
| Stable update manifest updated | Skipped with reason | It must contain the actual public URL, publication time, and downloaded hash. |
| Final report completed | Not available | Release phase remains blocked. |

## Publication decision

**Publication boundary:** only the exact immutable `v1.0.1` Tag workflow may create the Release. It cannot publish an unsigned installer, replace existing assets, expose a private key, or skip re-download verification.
