# Historical winTerm v1.0 Release Checklist

This checklist belongs to the completed v1.0.x MSIX-era release process. It is
retained as historical evidence and must not be used to publish a new release.

For current releases, use `docs/release-process.md`. The active workflow builds
an unpackaged x64 runtime, an Inno Setup installer, and a Portable ZIP; validates
the exact asset allowlist and hashes; tests installation and launch behavior;
creates a Draft; re-downloads and verifies it; and publishes only after every
required gate succeeds.

Existing v1.0.x tags and Release assets must remain unchanged.
