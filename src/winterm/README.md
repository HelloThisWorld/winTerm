# winTerm-owned source boundary

This directory contains winTerm-specific integration files that should remain clearly separated from Microsoft Terminal upstream code.

The v0.1 baseline is Microsoft Terminal `release-1.25` at commit `1cea42d433253d95c4487a3037db48197b5e72f4`.

winTerm v0.1 reuses the upstream terminal engine, ConPTY integration, renderer, settings model, profiles, tabs, panes, and window management. It adds only the minimum package-isolation, branding, build, verification, and maintenance integration required for the foundation release.

Future modules may be documented here, but Shell Bridge, docking, workspace, and other post-v0.1 features must not be implemented in this release.
