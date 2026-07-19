# winTerm schema support policy

| Contract | Stable version | Read policy | Write policy |
| --- | --- | --- | --- |
| Workspace | 2 | Read version 2; migrate supported older fixtures; preserve newer unsupported files. | Write version 2 atomically. |
| Docking Model | 1 | Read version 1 only inside a supported Workspace. | Write version 1. |
| Shell Protocol | 1 | Accept only validated version 1 payloads within size and field limits. | Emit version 1. |
| Theme | 1 | Read validated version 1 themes and supported data-only imports. | Write version 1. |
| Update Manifest | 1 | Accept HTTPS stable-channel metadata only after consent and identity, version, and hash validation. | Generated only after a real public Release exists. |

## Failure behavior

- Corrupted settings or Workspace data must not block startup.
- A migration must create a backup before replacement.
- Running the same migration again must not alter the result.
- Newer unsupported schema files must not be rewritten.
- Unknown fields are preserved only when doing so is demonstrably safe.
- Imported data may not add commands, executables, environment variables, remote includes, or external URLs.

Application version 1.0.2 does not change any schema version.
