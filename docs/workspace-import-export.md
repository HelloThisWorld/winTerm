# Workspace import and export

Import accepts `.winterm-workspace.json` and JSON content as untrusted data. Inspection reads at most 5 MB, uses the strict parser, applies schema and count limits, recursively examines every key and string, validates the complete model, sanitizes runtime-only metadata, produces a summary, and returns a value that is ready for explicit user confirmation. Inspection itself does not store or open the workspace.

The importer rejects command and command-line fields, startup commands, scripts, shell arguments, environment overrides, registry data, arbitrary executables, URLs, external files, includes, plugins, credentials, passwords, tokens, and private-key paths. It cannot download data, expand arbitrary environment variables, create profiles, modify global settings, or execute anything. A newer schema is reported and preserved without loading or rewriting it.

Import sanitization clears default status, recovery state, previous process flags, and remote directory values. Profiles remain references to profiles that the local resolver already knows.

Export options control working directories, user-home redaction, monitor identity, descriptions and tags, and custom titles. User-home paths are replaced only when they match the caller-provided known home prefix and become `<USER_HOME>`. Export never includes command history, clipboard, terminal output, credentials, environment dumps, tokens, or process state. File output uses atomic replacement.

The current backend returns the import summary and confirmation-ready model. File picker, summary dialog, explicit confirmation, and gallery integration remain UI work.
