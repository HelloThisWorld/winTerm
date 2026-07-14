# Shell completion

PowerShell retains its native command, parameter, and path completion. The Safe `ll`, `touch`, and `open` functions use normal path parameters, so they receive the same relative-path, quoted-path, directory, and path-with-space completion behavior as other PowerShell commands. `which` registers a command-name completer only when winTerm owns the function.

If PSReadLine is loaded, diagnostics reports it as available. winTerm does not set PSReadLine key bindings, edit mode, prediction source, history behavior, or any global option. If PSReadLine is unavailable, the module still loads and basic compatibility commands remain usable.

CMD keeps its native Tab path completion. DOSKEY does not provide a PowerShell-class completion protocol, and winTerm does not bundle or inject Clink. `winterm-shim.exe` supplies normal command-line help for its supported operations.
