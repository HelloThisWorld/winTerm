# Contributing to winTerm

winTerm is an independent project based on the Microsoft Terminal open-source codebase. It is not a Microsoft product and is not endorsed by Microsoft.

## Before starting

Search existing winTerm Issues and open an Issue before a substantial change. Security vulnerabilities must follow `SECURITY.md`, never a public Issue.

The v1.0 release branch is feature-frozen. It accepts bug, security, accessibility, performance, compatibility, installation, upgrade, release-engineering, documentation, and diagnostics fixes only. Record new feature ideas in `docs/roadmap-post-1.0.md`.

## Build and test

Follow `AGENTS.md`, `docs/build.md`, and upstream Microsoft Terminal toolchain guidance. Keep protected areas unchanged unless an acceptance requirement cannot be met otherwise. Run the smallest relevant checks and record exactly what was and was not tested.

```powershell
.\scripts\winterm\test.ps1 -Suite Smoke
.\scripts\winterm\build.ps1 -Configuration Debug -Platform x64 -IncludeTests
.\scripts\winterm\test.ps1 -Suite Relevant -Configuration Debug -Platform x64
```

Do not claim a build, package, architecture, signature, installer, or runtime test passed unless it actually ran.

## Code and commits

- Use English for code, identifiers, comments, logs, tests, and error messages.
- Follow existing Microsoft Terminal C++, C++/WinRT, XAML, and PowerShell style.
- Add the appropriate MIT header to new source and scripts.
- Avoid large formatting-only changes, generated output, absolute paths, secrets, certificates, and unrelated changes.
- Preserve Microsoft Terminal copyright, licenses, and third-party notices.
- Keep commits focused and do not rewrite public history or force-push release work.
- Update the root `CHANGELOG.md` in every source or documentation commit. After the source commit exists, immediately add its final SHA, link, summary, and checkpoint/release to the Wiki `Development-Changes.md` ledger and push the Wiki commit. Amend, rebase, squash, and GitHub merge SHAs must be corrected or added in the Wiki; a source commit is not complete until the matching Wiki commit is published.
- Wiki-only synchronization commits do not require recursive Wiki entries. Report a Wiki push failure instead of claiming the workflow is complete, and do not add secret-bearing or expensive native-build CI to automate this policy.
- Reuse suitable sanitized application screenshots from `HelloThisWorld/winterm-site` before adding documentation imagery. Do not recapture or regenerate an equivalent image. See `docs/development/changelog-policy.md`.

## Pull requests

Describe scope, tests, security and privacy impact, accessibility impact, package impact, schema impact, and known limitations. Release pull requests must link the current release checklist and record signing, architecture, install, upgrade, uninstall, and coexistence status.

### Label-gated validation builds

Every pull request runs change classification, quick source validation, smoke tests, shell integration checks, layout validation, and the workspace benchmark. Ordinary development pull requests do not run a native C++ build unless a maintainer selects one of these labels:

- No label: use for normal development and intermediate review. Only the quick validation job runs.
- `build`: use for a roadmap milestone or an alpha/beta build that needs downloadable binaries. CI performs one package-capable x64 Release build, runs the three compiled test suites, reuses that build for the unpackaged stage, builds Setup and Portable distributions, runs their lifecycle tests, and uploads both files. It does not run Debug.
- `delivery`: use for release candidates and final release preparation. It runs everything selected by `build` and adds the x64 Debug build and compiled test suites in parallel with the Release delivery job.
- `ci:full`: backward-compatible alias for `delivery`.

Adding or removing a validation label retriggers the pull-request workflow. Apply an expensive label after the intended milestone changes are pushed; later pushes retrigger the selected jobs. Manual dispatch supports `quick`, `build`, and `delivery`; the legacy `fast` and `full` choices remain aliases for `build` and `delivery`.

Each compiled TAEF suite has its own 20-minute process timeout. On timeout, CI terminates the runner and its descendants, fails the job, and uploads suite stdout, stderr, and a diagnostic record. The tag-triggered Release workflow remains authoritative for publishing a public release.

## External contributions and protected review areas

Every change from an external contributor is accepted only through a pull request reviewed and approved by a maintainer listed in `CODE_SIGNING_POLICY.md`. Maintainer review must explicitly cover, in addition to the code itself:

- CI configuration and every file under `.github/workflows/`;
- build, packaging, installer, and release scripts under `scripts/winterm/` and `build/`;
- anything that downloads a dependency, tool, or other build input, including pinned URLs, versions, and hashes;
- signing configuration, release metadata, and version/branding sources such as `src/winterm/Branding/`;
- the policy documents `CODE_SIGNING_POLICY.md`, `PRIVACY.md`, and `SECURITY.md`.

Changes in these areas must never be merged on the strength of passing checks alone.
