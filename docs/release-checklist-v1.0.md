# winTerm 1.0 release checklist

Recorded on 2026-07-18. `Passed` means evidence exists. `Not available` is never treated as passed.

| Gate | Status | Evidence or reason |
| --- | --- | --- |
| Feature freeze complete | Passed | `docs/feature-status.md` and `docs/roadmap-post-1.0.md`. |
| P0 count is zero | Passed | Connected GitHub repository returned no open Issues; no known P0 was found in source review. |
| P1 count is zero or explicitly approved | Failed | Production signing, clean install, runtime core flows, accessibility, and performance evidence are unresolved. |
| Version is 1.0.0 everywhere | Passed | `scripts/winterm/verify-version.ps1` after implementation validation. |
| Full CI passes | Not available | v1.0 branch has not been pushed or run in GitHub Actions. |
| Security review passes | Not available | Static review exists; runtime evidence is incomplete. |
| Privacy review passes | Passed | Static privacy and diagnostic checks exist; end-to-end bundle inspection remains separately open. |
| Accessibility review passes | Not available | Narrator, keyboard-only, contrast, scaling, and pseudo-localization are untested. |
| Performance review passes | Not available | Packaged benchmarks and soak tests are untested. |
| x64 package builds | Not available | v0.6 CI built an unsigned package; the v1.0 package has not been built. |
| x64 signature verifies | Not available | No production signing certificate or CI signing configuration is available. |
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
| Draft Release created | Not available | No tag or final package exists. |
| Assets uploaded | Not available | No Draft Release exists. |
| Assets downloaded again | Not available | No Draft Release exists. |
| Downloaded hashes verified | Not available | No Draft Release exists. |
| Draft published | Not available | Publication is blocked. |
| Release marked Latest | Not available | No public Release exists. |
| Release URL verified | Not available | No Release URL exists. |
| Public download smoke test passes | Not available | No public Release or clean Windows 11 environment exists. |
| README download link updated | Skipped with reason | A `/releases/latest` installer link would be misleading before a public stable Release exists. |
| Stable update manifest updated | Skipped with reason | It must contain the actual public URL, publication time, and downloaded hash. |
| Final report completed | Not available | Release phase remains blocked. |

## Publication decision

**Release blocked before publication.** The workflow may create an explicitly blocked Draft only after a tag is deliberately selected. It cannot publish an unsigned or untested installer.
