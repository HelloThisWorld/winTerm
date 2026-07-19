# Install winTerm

Install winTerm only from the official GitHub Release. Do not install an Actions artifact, an unsigned Draft package, or a package from a mirror.

For `v1.0.2`:

1. download `winTerm-1.0.2-x64.msix`, `winTerm-1.0.2.cer`, `INSTALL.txt`, and `SHA256SUMS.txt` from the official Release;
2. verify SHA-256;
3. open the CER, select **Install Certificate**, choose **Local Machine**, and place it in **Trusted People**;
4. open the MSIX or run `Add-AppxPackage`;
5. launch winTerm from Start or `winterm.exe`.

The certificate is self-signed and is not publicly trusted or timestamped. Verify that the files came from `github.com/HelloThisWorld/winTerm` before importing it. Installation must not require Visual Studio, Windows SDK, Git, PowerShell build modules, Developer Mode, manual copying, registry edits, execution-policy changes, global font installation, PowerShell profile changes, or CMD AutoRun changes.

winTerm uses `HelloThisWorld.winTerm` and `winterm.exe`. It does not replace Microsoft Windows Terminal or claim `wt.exe`.
