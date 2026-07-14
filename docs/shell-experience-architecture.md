# Shell Experience architecture

winTerm keeps command interpretation at the shell boundary. It does not inspect keystrokes to replace text, infer a prompt from rendered cells, or translate input inside SSH, WSL, Vim, Python, or other interactive applications.

```text
winTerm profile with explicit session marker
    -> PowerShell module or CMD initialization asset
        -> inherited OSC 9;9 and OSC 133 handling
            -> upstream command marks, navigation, and working-directory state
```

The PowerShell module activates its prompt integration only when `WINTERM_SESSION_ID` is present and the host is an interactive ConsoleHost with unredirected input and output. It stores a captured prompt script block only for the current process and restores it when removed. No `$PROFILE`, registry AutoRun value, or execution policy is changed.

`ShellSessionMetadata` is an in-memory, per-session model with a session ID, profile ID, shell type, capabilities, current-directory kind, command state, last exit code, duration, and health. It deliberately excludes command history, terminal output, environment dumps, clipboard data, passwords, and tokens. Remote and WSL directories are tagged separately from trusted local paths.

The paste analyser is also pure and in-memory. It reports reasons such as multiline text, a final newline, text size, control characters, command separators, and suspicious patterns. It never changes text, records clipboard content, blocks all text containing a word, or executes a command. UI confirmation and terminal-control wiring remain acceptance items until a built application can be exercised.

## Upstream boundaries

The v0.3 foundation reuses the inherited TerminalCore parser and command-mark navigation. It does not modify ConPTY, the VT parser, text buffer, renderer core, Unicode width engine, input protocol parser, or OpenConsole internals. The new source under `src/winterm` is limited to diagnostic/session models and paste-risk analysis.
