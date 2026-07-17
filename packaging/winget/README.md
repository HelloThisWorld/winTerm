# WinGet Public Beta manifest plan

The Public Beta package identifier is reserved as `Kaname.winTerm.Beta`. A WinGet manifest is not submitted or claimed valid until the draft GitHub Release has immutable x64 MSIX URLs and SHA-256 values.

Generate installer, default-locale, and version manifests from `release-metadata.json`, validate them with `winget validate`, and retain the final validation output with the release report. Do not publish a template with invented URLs or hashes.
