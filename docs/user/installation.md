# Install winTerm

There is no approved public Stable installer while `docs/release-checklist-v1.0.md` is blocked. Do not install an Actions artifact, unsigned Draft package, or package that asks you to import a development certificate as winTerm Stable.

After a public `v1.0.0` Release exists:

1. download `winTerm-1.0.0-x64.msix` and `SHA256SUMS.txt` from the official Release;
2. verify SHA-256;
3. run `Get-AuthenticodeSignature` and require `Status: Valid`, a trusted timestamp, and a signer matching the package Publisher;
4. open the MSIX or run `Add-AppxPackage`;
5. launch winTerm from Start or `winterm.exe`.

Installation must not require Visual Studio, Windows SDK, Git, PowerShell build modules, Developer Mode, manual copying, registry edits, execution-policy changes, global font installation, PowerShell profile changes, or CMD AutoRun changes.

winTerm uses `Kaname.winTerm` and `winterm.exe`. It does not replace Microsoft Windows Terminal or claim `wt.exe`.
