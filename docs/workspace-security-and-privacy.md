# Workspace security and privacy

Workspace files can reveal project, company, share, user-home, and WSL distribution names. Local working-directory capture defaults on; remote directory capture defaults off; export home redaction defaults on. Users can disable directory and custom-title capture.

The schema cannot hold command history, command text, terminal output, clipboard data, credentials, tokens, passwords, private keys, environment dumps, SSH agent state, or process state. Logs and copied diagnostics never include full workspace JSON or custom titles. Diagnostic paths replace the user component below `C:\Users` with `<user>`.

External JSON is an untrusted boundary. Strict parsing rejects malformed, oversized, duplicate-key, deeply nested, and special-float input. Validation bounds every collection and checks references and namespaces. Import inspection rejects fields that could name a command, executable, script, registry setting, URL, include, plugin, credential, environment override, or private key. Import cannot run code or modify settings.

Managed corrupted or unsafe workspace files can be moved to `workspaces/quarantine` under a generated name. A separate sanitized metadata file records only the reason and source type. Raw JSON is not intended for direct rendering. Quarantined files are not retried on every startup and are not automatically deleted.

Atomic storage validates the model before serialization, writes through the inherited atomic file primitive, updates a backup only from validated prior content, and never truncates the current valid file first. Save generation checks prevent stale background work from replacing newer state.

Workspace content stays local. No workspace code performs upload, network download, network mount, credential prompt, SSH reconnect, profile creation, or arbitrary command execution.
