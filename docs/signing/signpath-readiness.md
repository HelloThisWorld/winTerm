# SignPath Foundation readiness

Status of the winTerm application for free OSS code signing from SignPath
Foundation, audited 2026-07-28 against <https://signpath.org/terms.html>,
<https://signpath.org/apply>,
<https://docs.signpath.io/trusted-build-systems/github>, and
<https://docs.signpath.io/origin-verification/>.

The latest public release is winTerm 1.1.3 (`v1.1.3`,
`winTerm-1.1.3-setup-x64.exe`, `winTerm-1.1.3-portable-x64.zip`,
`SHA256SUMS.txt`, SBOMs, `release-metadata.json`, GitHub attestations). The
Setup EXE is **not Authenticode-signed**.

## Completed in this repository

- Canonical [CODE_SIGNING_POLICY.md](../../CODE_SIGNING_POLICY.md) with the
  exact SignPath attribution sentence, roles (Authors/Committers, Reviewers,
  Approvers), manual-approval statement, build provenance, artifact
  ownership boundaries, and the honest current unsigned status.
- README `Code signing policy` section visible on the repository homepage,
  linking the canonical policy and stating the current unsigned status.
- [CONTRIBUTING.md](../../CONTRIBUTING.md) explicit external-contribution
  review policy covering CI, workflows, build/packaging scripts, dependency
  downloads, and signing configuration.
- [.github/CODEOWNERS](../../.github/CODEOWNERS) covering workflows, release
  scripts, packaging, version/branding metadata, and policy documents.
- [PRIVACY.md](../../PRIVACY.md) version-neutral wording; validated by
  `scripts/winterm/test-privacy.ps1` (no analytics, no update requests, crash
  upload off/opt-in, diagnostics user-initiated, uninstall data behavior).
- Release workflow ([.github/workflows/release.yml](../../.github/workflows/release.yml))
  builds from an immutable tag that must match
  `src/winterm/Branding/version.json`, exclusively on GitHub-hosted
  runners, with a pinned and signature-verified Inno Setup toolchain, an
  exact release-asset allowlist, SHA-256 checksums, SPDX + CycloneDX SBOMs,
  GitHub artifact attestations, and re-download verification before and
  after publication.
- `winterm-shim.rc` version resources aligned with the canonical version
  metadata (1.1.3), and `scripts/winterm/verify-version.ps1` now fails on any
  future `winTerm.exe`/`winterm-shim.exe` version-resource drift.
- `scripts/winterm/generate-release-artifacts.ps1` no longer duplicates the
  Signing section (the guard now recognizes "not Authenticode-signed") and
  ensures every future Release body links the Code signing policy and the
  Privacy policy.
- Release notes source `docs/releases/1.1.3.md` carries the Signing section
  and policy links.
- Website <https://winterm.dev> (separate `winterm-site` repository):
  `Code signing policy` link on the homepage download area and footer, the
  policy page with the exact attribution sentence, roles, manual approval,
  current unsigned status, and privacy/installation/uninstall links.

## Verified externally (rechecked 2026-07-28)

- Latest public Release `v1.1.3` exists with the expected asset list and is
  explicitly unsigned in its notes.
- The published v1.1.3 Release body currently duplicates the Signing
  section; a corrected body (single Signing section plus policy links) is
  applied from `docs/releases/1.1.3.md` without touching the tag or assets.
- Repository metadata: `HelloThisWorld/winTerm` is public, MIT-licensed, and
  **not** marked as a GitHub fork (see Blocked).

## Known gaps in already-published artifacts

- The binaries inside the published v1.1.3 packages predate the shim
  version-resource fix: `winTerm.exe` and `winterm-shim.exe` report
  ProductVersion 1.0.0 / FileVersion 1.0.8.0, while `WindowsTerminal.exe`
  correctly reports 1.1.3. Published release assets are immutable and are
  not replaced; the next release will carry consistent metadata enforced by
  `verify-version.ps1`.
- `OpenConsole.exe` and `elevate-shim.exe` are upstream-derived binaries
  built without version resources. Decide before configuring SignPath
  artifacts whether they are in signing scope; if so, they need version
  resources in a future release.

## Manual prerequisites (cannot be completed from this repository)

- GitHub account MFA enabled for `HelloThisWorld`.
- SignPath account creation and MFA enabled.
- Submitting the SignPath Foundation application.
- Installing the SignPath GitHub App and granting it repository access.
- SignPath-side organization, project, artifact configuration, and signing
  policies; configuring the manual approver.
- `SIGNPATH_API_TOKEN` (or equivalent) as a repository secret. Do not add
  placeholder IDs, slugs, or tokens to workflows before the SignPath
  configuration exists; the release workflow must keep succeeding unsigned
  until then.
- GitHub server-side branch protection / rulesets requiring code-owner
  review for the protected paths in `.github/CODEOWNERS`.
- After certificate issuance: integrate the SignPath step into
  `release.yml` (artifacts uploaded via `actions/upload-artifact` before the
  signing request), then re-verify Authenticode signature, product/version
  metadata, hashes, and install/uninstall on the signed outputs before
  publication.

## Blocked / needs SignPath confirmation

- **Upstream fork visibility (high priority).** winTerm is modified
  Microsoft Terminal (pinned baseline
  `release-1.25@1cea42d433253d95c4487a3037db48197b5e72f4`, integration
  process in [docs/upstream-sync.md](../upstream-sync.md)), but the GitHub
  repository is not a GitHub-visible fork of `microsoft/terminal`
  (`fork: false`, no parent). SignPath's modified-upstream terms expect the
  project to "visibly fork" upstream. Recreating the repository as a GitHub
  fork or rewriting history is intentionally out of scope. Next step for the
  maintainer: ask SignPath whether the pinned-baseline provenance documented
  here and in `upstream-sync.md` satisfies the requirement, or whether a
  visible-fork migration is required before approval.
- SignPath Foundation's subjective review of project reputation and
  popularity.
