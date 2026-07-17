# winTerm 1.0 security review

Review date: 2026-07-18. This review distinguishes source validation from runtime evidence. No open GitHub issue is labelled Critical or High, but missing runtime and signing evidence prevents a passing Stable decision.

| Area | Result | Evidence and remaining work |
| --- | --- | --- |
| Shell integration | Partial | Module loading, process-local behavior, executable precedence, local-only mappings, protocol size limits, and no policy bypass pass source tests. Packaged PowerShell 7, Windows PowerShell, CMD, WSL, SSH, and REPL boundaries remain unverified. |
| Import | Partial | Theme and Workspace inputs reject command fields, executables, remote includes, invalid paths, oversized files, malformed JSON, and excessive structures in fixtures. Live dialog and XML parser behavior require packaged tests. |
| Clipboard | Partial | Paste analysis is non-executing and source tests cover multiline and suspicious input. Packaged right-click and bracketed-paste behavior remains unverified. |
| Workspace | Partial | Command history, terminal output, credentials, and raw Workspace JSON are excluded from diagnostics; atomic persistence, backups, and sanitization have source coverage. Runtime recovery and migration need provisioned tests. |
| Docking | Partial | Opaque expiring tokens, single use, ownership, rollback, and cross-process rejection have model coverage. Runtime Docking remains disabled, so no live-session claim is made. |
| Updates | Disabled | winTerm performs no update request without explicit consent. Manifest, identity, version, hash, signature, and downgrade enforcement must pass before enabling the feature. |
| Telemetry and crash reporting | Passed (source) | winTerm-branded UI, settings, connection, control, and host binaries use distinct ETW provider identities and do not register Microsoft telemetry or fallback crash reporting. Runtime process inspection remains part of the clean-machine gate. |
| Diagnostics | Partial | Path, token, email, and connection redaction markers pass static validation; bundles are user-generated. End-to-end temporary-file and bundle inspection remains open. |
| Release supply chain | Partial | Exact-tag workflow, allowlisted assets, immutable Action pins, checksums, SBOM, Attestation, Draft re-download, and no-clobber rules are implemented. Production signing and clean-machine validation are unavailable. |

## Severity decision

- Known Critical issues: 0 reported.
- Known High issues: 0 reported.
- Security gate: **Not available**, because production signature, package installation, live Shell, and end-to-end diagnostics evidence are missing.

Any confirmed command execution through import, secret or clipboard exposure, package identity collision, forged Docking ownership, or release secret leakage blocks publication.
