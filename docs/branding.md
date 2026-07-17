# winTerm branding and identity

## Public identity

| Surface | winTerm 1.0 value |
| --- | --- |
| Product and display name | `winTerm` |
| Package name | `Kaname.winTerm` |
| Application ID | `winTerm` |
| Execution alias | `winterm.exe` |
| Forbidden alias | `wt.exe` |
| Development publisher | `CN=winTerm Development` |
| Stable publisher | Injected only from the protected signing environment and required to match the production certificate subject |
| Package version | `1.0.0.0` |
| Application version | `1.0.0` |
| Release channel | `stable` |
| Package description | `Independent open-source terminal based on Microsoft Windows Terminal` |

The WinTerm build brand does not change Microsoft Terminal Stable, Preview, Canary, or Dev package identities. Unique winTerm package, application-data, COM handoff, and shell-extension identities prevent package registration from replacing the upstream application.

The terminal host may retain the internal filename `WindowsTerminal.exe`, and upstream project names, namespaces, API contracts, compatibility identifiers, and source paths remain unchanged. The package maps the launcher output to `winterm.exe`; it neither ships nor registers `wt.exe`.

## Publisher and signing

`CN=winTerm Development` is a source-controlled development placeholder, not a production publisher. The protected Release workflow may replace it only in the clean tag checkout immediately before packaging. The certificate subject must exactly match the injected Publisher. A private key, PFX, password, token, or signing-service credential must never be committed or uploaded as a release asset.

An unsigned package can exist only in an explicitly blocked Draft Release. It cannot be described or published as a Stable installer.

## Application data and coexistence

Packaged winTerm has its own package-local data. Unpackaged winTerm uses `%LOCALAPPDATA%\winTerm`. It does not import, overwrite, or remove Microsoft Windows Terminal settings. Uninstall and Reset operations may target only winTerm-owned data.

## Public contracts

The `winterm.exe` alias is frozen for winTerm 1.x. No `winterm:` URI protocol is registered because no reviewed activation contract exists. Shared profile-fragment identifiers retained from upstream are compatibility inputs, not winTerm settings roots or a claim of Microsoft endorsement.

## Artwork

`assets/winterm/icons/winterm.svg` is original placeholder geometry. It contains no Microsoft, Windows, Windows Terminal, or Apple logo. Generated PNG and ICO files are committed so image generation is not a build dependency.
