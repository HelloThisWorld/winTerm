# winTerm release checklist

1. Freeze the release branch and confirm a clean worktree.
2. Verify application, package, tag, artifact-file, and build-metadata versions match.
3. Run full CI, security checks, compatibility review, and accessibility audit.
4. Build and inspect x64 and ARM64 artifacts only on supported architectures.
5. Generate notices, SBOM files, and SHA-256 checksums from the final artifacts.
6. Test clean install, upgrade, downgrade policy, uninstall, and coexistence with Windows Terminal.
7. Create a draft release; review its notes, assets, hashes, signing status, and known issues.
8. Publish only after an authorized human review. Verify download hashes and update-manifest references afterwards.

Never insert a certificate, private key, signing password, token, local path, terminal output, or user Workspace into release assets.
