# Security policy

## Supported versions

The latest public winTerm Stable release receives security support. winTerm 1.0.1 is the current supported Stable release. Its installer is self-signed; verify the GitHub Release URL, SHA-256 hashes, attached public certificate, and GitHub provenance before importing certificate trust or installing it.

## Reporting a vulnerability

Do not open a public Issue for a suspected vulnerability. Do not publish credentials, tokens, private terminal output, commands, Workspace files, memory dumps, certificate material, or exploit payloads.

Use GitHub Private Vulnerability Reporting from this repository’s Security tab. If it is unavailable, contact the repository owner through GitHub and request a private channel before sharing technical details.

## Scope

Relevant reports include arbitrary command execution, unsafe imports, secret or clipboard exposure, package identity confusion, unsafe update behavior, diagnostic-redaction failure, Workspace corruption, Docking ownership or live-session loss, and vulnerabilities in winTerm-maintained release automation.

## Response

Maintainers assess impact and affected versions through the private channel, coordinate a fix, and publish disclosure only after users have a reasonable opportunity to update. No response-time or bounty promise is made.

Microsoft Terminal vulnerabilities that are not introduced by winTerm should also be checked against the upstream Microsoft Terminal security process; do not imply that winTerm can coordinate a Microsoft response.
