# winTerm branding integration

`winTerm.props` selects the dedicated `WinTerm` build branding when no upstream branding was explicitly requested. Command-line global properties still take precedence, so upstream Release, Preview, Canary, and Dev builds remain available for comparison and merge validation.

The winTerm-specific build branding selects:

- `Package-winTerm.appxmanifest`
- the `winterm.exe` application execution alias
- winTerm-owned image assets
- isolated package, COM, and shell-extension identities
- the `WT_BRANDING_WINTERM` compile-time token

Internal upstream project names and namespaces intentionally remain unchanged.
