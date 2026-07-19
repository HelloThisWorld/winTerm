# WinGet manifest preparation

The Stable package identifier is `HelloThisWorld.winTerm`.

No 1.0.2 manifest is committed before the public GitHub Release exists. The installer URL and SHA-256 must come from the actually published `winTerm-1.0.2-x64.msix`; placeholders and Actions artifact URLs are forbidden.

After publication, `.github/workflows/winget.yml`:

1. confirms `v1.0.2` is public and not a Prerelease;
2. downloads the x64 installer from the Release;
3. computes its real SHA-256;
4. generates installer, default-locale, and version manifests;
5. runs `winget validate`;
6. uploads the validated set as a workflow artifact for manual review.

This does not claim that an official `microsoft/winget-pkgs` pull request exists or has been approved. Record a real PR URL only after a separate reviewed submission succeeds.
