# Changelog and documentation asset policy

The root `CHANGELOG.md` is the canonical version history for winTerm. Every
source or documentation commit created by a developer or automation agent must
update `CHANGELOG.md` in that same source commit. A source commit is not
complete until a matching commit has also been pushed to the separate
`HelloThisWorld/winTerm.wiki` repository.

Each `Development-Changes.md` Wiki entry must record the date, final source
commit SHA and link, a concise summary, and its related checkpoint or release.
If amend, rebase, squash, or another operation changes the source SHA, update
the Wiki entry to the final SHA. GitHub-generated merge or squash commits must
also be added once their final SHA exists. Commits that only synchronize the
Wiki do not recursively require another Wiki ledger entry.

If the Wiki cannot be pushed, report that explicitly. Do not describe the
source commit workflow as complete until the corresponding Wiki commit is
published. Public CI must not use a secret-bearing job to write to the Wiki,
and changelog validation must not cause every pull request to run an expensive
native build.

When application documentation benefits from a screenshot, inspect the
existing sanitized assets in `HelloThisWorld/winterm-site` first and reuse an
appropriate original image. Do not recapture, regenerate, duplicate, crop,
round, mask, or place an equivalent screenshot in a simulated browser frame.
If no suitable existing asset is available, keep the documentation text-only.
