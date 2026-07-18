# winTerm 1.x compatibility policy

winTerm 1.x is intended to read settings created by winTerm 1.0. Unknown safe settings are preserved where the existing settings model can do so without weakening validation. Missing settings use documented defaults, and malformed settings must fall back without blocking startup.

This policy commits to:

- reading winTerm 1.0 settings throughout the supported 1.x line;
- reading Workspace Schema version 2 unless a future release explicitly deprecates it with migration guidance;
- retaining backward compatibility for Theme Schema version 1 imports;
- preserving unsupported newer Workspace files rather than overwriting them;
- creating a backup before a migration replaces persisted data;
- deterministic and idempotent migrations;
- preserving the `winterm.exe` alias and the `Kaname.winTerm` package boundary throughout 1.x;
- leaving Windows Terminal settings, package identity, and `wt.exe` untouched.

This is not a promise to read every future schema forever. A future incompatible schema must use a new schema version, publish a migration policy, and never silently downgrade or overwrite newer data.
