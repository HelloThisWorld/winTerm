# Third-party asset governance

## Principle

winTerm bundles only assets that have a clear license, immutable source identity, local license text, and a verified SHA-256. Normal builds and validation do not download or update themes or fonts.

The authoritative manifests are:

- `assets/winterm/themes/manifest.json` with `pathBase` set to `manifest`;
- `assets/winterm/fonts/manifest.json` with `pathBase` set to `repository`.

Theme paths are resolved relative to the theme manifest. Font paths are resolved relative to the repository root so existing files under `res/fonts` can be recorded without duplication. In both cases the resolved path must remain inside the repository. Absolute paths and `..` traversal outside the repository are rejected.

## Adding an asset

Before adding a theme or font:

1. Identify the original project and author.
2. Review the license for redistribution, modification, attribution, and naming conditions.
3. Choose an immutable tag or commit and record the exact source file.
4. Obtain the asset outside the normal build, review it, and place only the approved file in the repository.
5. Store the complete license text under `assets/winterm/licenses`, unless the original winTerm `LICENSE` applies.
6. Add all required manifest metadata and the lowercase SHA-256 of the exact bundled bytes.
7. Run asset validation and regenerate third-party notices.
8. Review visual fidelity, provenance, package inclusion, and runtime behavior separately.

A passing checksum confirms byte identity with the manifest; it does not replace a license or security review.

## License gate

`scripts/winterm/validate-assets.ps1` checks manifest schemas, required metadata, duplicate IDs, safe relative paths, allowed extensions, file and license existence, descriptor IDs and source metadata, canonical theme colors, palette lengths, opacity, and SHA-256 values.

Theme assets must be JSON. Font assets must be OpenType or TrueType files. License files must be text, Markdown, or the repository `LICENSE`. Unsupported referenced extensions fail validation.

`scripts/winterm/generate-third-party-notices.ps1` reads only the local manifests and local license files. It produces deterministic inventory and license sections. `-Check` compares the generated content with the root `THIRD_PARTY_NOTICES.md` and fails when it is absent or stale. Inherited source dependencies remain covered by the upstream `NOTICE.md`; the generated file focuses on appearance assets.

For winTerm-branded builds, the existing About dialog's third-party-notices link targets the packaged offline `AppearanceAssets\licenses\open-source-licenses.html` index. That index lists all 15 bundled themes and four fonts and links to their local license texts, the generated appearance notice, the repository license, and the complete inherited `NOTICE.md`. The branded resource list excludes the network-only upstream development placeholder. The package script verifies these files after unpacking; the link itself still requires runtime acceptance testing.

## Version and checksum policy

`sourceRevision` must identify an immutable upstream revision or a winTerm development revision for an original asset. `sourceFile` records the path inside that source. `sha256` is always 64 lowercase hexadecimal characters and is recomputed from the repository file by CI.

Do not use a floating branch, an unpinned release URL, an automatically changing archive, or a locally modified file without recording the modification and its license implications.

## Upgrade process

An asset upgrade is a reviewed change, not an automated build step:

1. verify the new upstream revision and license;
2. compare source and rendered behavior;
3. replace the asset and update version, revision, source path, and checksum;
4. update license text if upstream changed it;
5. regenerate notices;
6. run asset validation, importer tests where relevant, packaging checks, and manual rendering checks;
7. record known visual or glyph-coverage changes.

Keep an upgrade focused; do not mix unrelated theme, font, renderer, or shell features.

## Removal process

To remove an asset, delete its manifest entry and bundled file, then remove a license file only when no remaining entry references it. Regenerate notices and verify that settings migration safely handles the missing ID or family. Protected built-in fallback assets must not be removed without selecting and testing a replacement.

Removal must not delete user-imported files from application data, globally installed fonts, Microsoft Terminal assets, or notices required by inherited dependencies.

## Current exclusions

Cascadia Mono NF and other Nerd Fonts are not bundled. JetBrains Mono and Fira Code were not added because this change did not complete an exact binary-version, checksum, and font-naming-condition review for them. One Dark and One Light were also excluded because no canonical palette revision was completed through the license gate used for the bundled theme set. Exclusion does not imply that those projects are unlicensed; it means this repository does not yet have the required pinned redistribution record.

Apple fonts, themes with unclear redistribution terms, online theme downloads, font marketplaces, and automatic asset updates remain outside v0.2.
