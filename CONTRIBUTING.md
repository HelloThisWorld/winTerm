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

## Pull requests

Describe scope, tests, security and privacy impact, accessibility impact, package impact, schema impact, and known limitations. Release pull requests must link the current release checklist and record signing, architecture, install, upgrade, uninstall, and coexistence status.
