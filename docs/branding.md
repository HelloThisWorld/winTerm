# winTerm branding and identity

| Surface | Value |
| --- | --- |
| Application and product name | `winTerm` |
| Publisher and company | `helloThisWorld` |
| Product ID / WinGet ID | `HelloThisWorld.winTerm` |
| Diagnostic product ID | `helloThisWorld.winTerm` |
| Executable | `winTerm.exe` |
| Command | `winterm.exe` |
| Forbidden command ownership | `wt.exe` |
| User data | `%LOCALAPPDATA%\winTerm` |
| All-users install | `%ProgramFiles%\winTerm` |
| Current-user install | `%LOCALAPPDATA%\Programs\winTerm` |

The terminal host retains internal upstream filenames, namespaces, API
contracts, compatibility identifiers, and source paths where required. Those
are not public winTerm publisher metadata. Microsoft and third-party component
publisher data must not be rewritten.

Windows filenames are case-insensitive, so the physical `winTerm.exe` also
satisfies a lowercase `winterm.exe` invocation. The installer registers that
command with App Paths and never takes ownership of Windows Terminal's `wt.exe`.

## Signing and installed metadata

`helloThisWorld` appears in winTerm executable version information, Inno Setup
metadata, Installed Apps, About, diagnostics, release metadata, SBOMs, and
WinGet manifests. This metadata does not make an unsigned executable a
Windows-verified publisher. Only a trusted Authenticode certificate can do so.

The release tooling supports trusted signing or an explicit unsigned state. A
PFX, private key, password, token, development certificate, or signing service
credential must never be committed or uploaded.

## Data and coexistence

Installed winTerm stores data under `%LOCALAPPDATA%\winTerm`. Portable mode
uses the adjacent `data` directory while `portable.marker` exists. winTerm does
not import, overwrite, or remove Microsoft Windows Terminal settings, packages,
or commands. Uninstall and reset operations target only winTerm-owned files.

## Artwork

`assets/winterm/icons/winterm.svg` is original placeholder geometry. It contains
no Microsoft, Windows, Windows Terminal, or Apple logo. Generated PNG and ICO
files are committed so image generation is not a build dependency.
