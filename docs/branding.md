# winTerm branding and identity

## Public identity

| Surface | winTerm 1.0 value |
| --- | --- |
| Product and display name | `winTerm` |
| Package name | `HelloThisWorld.winTerm` |
| Application ID | `winTerm` |
| Execution alias | `winterm.exe` |
| Forbidden alias | `wt.exe` |
| Development publisher | `CN=winTerm Development` |
| Stable publisher | `CN=winTerm Development`, matching the public self-signed Release certificate |
| Package version | `1.0.1.0` |
| Application version | `1.0.1` |
| Release channel | `stable` |
| Package description | `Independent open-source terminal based on Microsoft Windows Terminal` |

The WinTerm build brand does not change Microsoft Terminal Stable, Preview, Canary, or Dev package identities. Unique winTerm package, application-data, COM handoff, and shell-extension identities prevent package registration from replacing the upstream application.

The terminal host may retain the internal filename `WindowsTerminal.exe`, and upstream project names, namespaces, API contracts, compatibility identifiers, and source paths remain unchanged. The package maps the launcher output to `winterm.exe`; it neither ships nor registers `wt.exe`.

## Publisher and signing

`CN=winTerm Development` is the v1.0.1 self-signed publisher. The Release workflow creates a temporary non-exportable key and exports only its public CER. The certificate subject must exactly match the package Publisher. A private key, PFX, password, token, or signing-service credential must never be committed or uploaded as a release asset.

An unsigned package may exist only as an intermediate CI build. The public installer must contain a cryptographically valid signature matching the CER attached to the same Release.

## Application data and coexistence

Packaged winTerm has its own package-local data. Unpackaged winTerm uses `%LOCALAPPDATA%\winTerm`. It does not import, overwrite, or remove Microsoft Windows Terminal settings. Uninstall and Reset operations may target only winTerm-owned data.

## Public contracts

The `winterm.exe` alias is frozen for winTerm 1.x. No `winterm:` URI protocol is registered because no reviewed activation contract exists. Shared profile-fragment identifiers retained from upstream are compatibility inputs, not winTerm settings roots or a claim of Microsoft endorsement.

## Artwork

`assets/winterm/icons/winterm.svg` is original placeholder geometry. It contains no Microsoft, Windows, Windows Terminal, or Apple logo. Generated PNG and ICO files are committed so image generation is not a build dependency.
