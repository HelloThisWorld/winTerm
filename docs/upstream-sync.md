# Synchronizing Microsoft Terminal upstream

## Remotes and baseline

Use the winTerm repository as `origin` and the official Microsoft repository as `upstream`:

```powershell
git remote add upstream https://github.com/microsoft/terminal.git
git remote -v
```

The v0.1 baseline is `upstream/release-1.25` at `1cea42d433253d95c4487a3037db48197b5e72f4`. Record the exact integrated commit in the sync commit message, this document, and `src/winterm/README.md` whenever the baseline changes.

## Prepare a sync

Start with a clean worktree. The helper fetches the selected official branch, resolves its commit, and prints the current and target revisions. It never merges, rebases, force-pushes, deletes a branch, or pushes.

```powershell
.\scripts\winterm\sync-upstream.ps1 -TargetBranch release-1.25
```

To create a review branch from the current winTerm commit:

```powershell
.\scripts\winterm\sync-upstream.ps1 `
    -TargetBranch release-1.25 `
    -CreateBranch `
    -BranchName sync/upstream-YYYYMMDD
```

`-SkipFetch` exists only for an offline dry run against an already-fetched remote-tracking reference.

For a future major baseline update, explicitly choose `main` or another official stable branch after reviewing upstream release policy. Do not silently change the helper default.

## Integrate and review

On the sync branch, choose either merge or rebase according to the repository policy. Never use a forced history rewrite merely to make the sync look clean.

```powershell
git merge --no-ff upstream/release-1.25
# or, when the branch policy explicitly permits rebasing:
git rebase upstream/release-1.25
```

Review conflicts rather than selecting all of either side. Branding conflicts commonly occur in:

- `common.openconsole.props` and `build/rules/Branding.targets`
- `src/cascadia/CascadiaPackage/CascadiaPackage.wapproj`
- Package manifests, assets, and COM registration headers
- `src/cascadia/TerminalSettingsModel/FileUtils.cpp`
- Window title, Settings, About, CLI, and resource-string integration points

Preserve upstream behavior in its existing brand branches. Reapply only the small `WinTerm` conditional, and confirm any new package extension or identity field does not collide with Microsoft packages.

## Keep ownership boundaries clear

Files under `src/winterm`, `assets/winterm`, `scripts/winterm`, and `docs` are winTerm-owned and should normally not be replaced by upstream versions. Generated package resources under `res/terminal/images-WinTerm` are also winTerm-owned. Avoid moving upstream core code into these directories or copying upstream features into winTerm wrappers.

Protected terminal-engine areas require a separate justification and review. A normal branding sync must not change ConPTY, VT parsing, the buffer, renderer, Unicode width, or input parsing.

## Validate before integration

On a fully provisioned Windows 11 x64 build machine:

```powershell
.\scripts\winterm\test.ps1 -Suite Smoke
.\scripts\winterm\build.ps1 -Configuration Debug -Platform x64
.\scripts\winterm\test.ps1 -Suite Relevant -Configuration Debug -Platform x64
.\scripts\winterm\build.ps1 -Configuration Release -Platform x64
.\scripts\winterm\test.ps1 -Suite Relevant -Configuration Release -Platform x64
.\scripts\winterm\package.ps1 -Platform x64
```

Then repeat the manual package, coexistence, launch, profiles, tabs, panes, settings, command palette, input, and rendering checklist in `docs/v0.1-acceptance.md`. Merge the reviewed sync branch into the development branch and then the release branch only after those results are recorded.
