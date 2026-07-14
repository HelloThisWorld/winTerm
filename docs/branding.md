# winTerm branding and identity

## Public identity

| Surface | v0.1 value |
| --- | --- |
| Product and display name | `winTerm` |
| Package name | `Kaname.winTerm` |
| Application ID | `winTerm` |
| Execution alias | `winterm.exe` |
| Forbidden alias | `wt.exe` |
| Development publisher | `CN=winTerm Development` |
| Package version | `0.2.0.0` |
| Release status | `0.2.0-dev` until v0.2 acceptance is complete |
| Package description | `Independent open-source terminal based on Microsoft Windows Terminal` |

The WinTerm build brand selects these values without changing Microsoft Terminal Stable, Preview, Canary, or Dev package identities. Unique WinTerm COM handoff and shell-extension identifiers are used so package registration does not replace the upstream application.

The terminal host may retain the internal filename `WindowsTerminal.exe`, and upstream project names, namespaces, API contracts, compatibility identifiers, and source paths remain unchanged. Its WinTerm build metadata uses product name `winTerm` and description `winTerm Terminal Host`. The package maps the upstream launcher output to `winterm.exe`; it does not ship or register `wt.exe` for the WinTerm brand.

## URI protocol

No `winterm:` URI protocol is registered in v0.1. The upstream package does not provide a reusable URI activation handler that can be renamed safely as a packaging-only change. Registering a protocol without an activation contract would expose a broken public interface. A protocol can be designed in a later version only with an explicit parser, security review, tests, and compatibility policy.

## Signing identity

`CN=winTerm Development` is a local-development placeholder, not a production publisher. The subject of any signing certificate must match the manifest Publisher value. Before a public release, replace both values with the publisher's real identity, validate upgrades and side-by-side behavior, and keep the certificate, private key, password, and secret-provider configuration outside Git.

## Application data

The unique package name gives packaged winTerm its own package-local application data. Unpackaged WinTerm builds use `%LOCALAPPDATA%\winTerm`. The WinTerm branch bypasses the upstream Stable settings migration path, so first launch does not import or overwrite Microsoft Windows Terminal settings.

Shared fragment discovery paths are retained as a compatibility input for third-party profile fragments. Those paths are not used as winTerm's settings or state root. See [Architecture](architecture.md) for the complete state table.

## User-visible changes

Conditional WinTerm values cover the package and Start menu display name, description, application fallback title, settings tab title, command-line help heading, tray tooltip, About feedback destination, executable file metadata, package resources, and selected English resource strings. Branding verification intentionally checks these owned surfaces rather than rejecting every technical occurrence of `Windows Terminal`.

## Retained upstream strings

Microsoft Terminal names remain where they describe:

- Copyright and license attribution
- Third-party notices and historical comments
- Internal namespaces, project files, types, and executable host filename
- API, protocol, policy, fragment, and compatibility identifiers
- Alternative upstream build-brand branches and their test fixtures
- Documentation that explains the upstream relationship

This avoids a high-risk repository-wide rename and keeps upstream integrations reviewable.

## Artwork

`assets/winterm/icons/winterm.svg` is the canonical original placeholder. It uses a dark rounded rectangle, an accent rail, and original `>_` geometry. It contains no Microsoft, Windows, Windows Terminal, or Apple artwork. Generated PNG and ICO files are committed so Pillow is not a build dependency. Artwork source, sizes, regeneration, review, and licensing are documented in `assets/winterm/icons/README.md`.
