# Install winTerm

Download winTerm only from the official
[GitHub Releases page](https://github.com/HelloThisWorld/winTerm/releases).
Choose the Setup EXE matching the Release version, for example
`winTerm-<version>-setup-x64.exe`, and verify it against `SHA256SUMS.txt` from
the same Release.

The Setup wizard supports:

- all-users installation to `%ProgramFiles%\winTerm` with administrator
  approval;
- current-user installation to `%LOCALAPPDATA%\Programs\winTerm`;
- a custom installation directory;
- a Start Menu shortcut by default and an optional desktop shortcut;
- the `winterm.exe` App Paths command by default;
- optional **Open winTerm here** File Explorer integration.

No MSIX certificate, Developer Mode, Visual Studio, Windows SDK,
`Add-AppxPackage`, manual DLL copy, manual registry edit, global font install,
PowerShell profile edit, or CMD AutoRun edit is required.

If the Release notes say the installer is unsigned, Windows may show Unknown
Publisher or a SmartScreen warning. Verify the repository URL and SHA-256; do
not disable SmartScreen or other security software.

## Portable ZIP

Download `winTerm-<version>-portable-x64.zip`, verify its hash, and extract the
entire archive to a writable directory. Run `winTerm.exe`. The included
`portable.marker` stores settings and workspaces in the adjacent `data`
directory. Portable mode requires no installation or administrator rights and
does not modify the registry or create shortcuts.

winTerm does not replace Microsoft Windows Terminal and does not register or
modify `wt.exe`.
