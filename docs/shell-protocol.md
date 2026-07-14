# winTerm Shell protocol

Protocol version: `1`.

winTerm v0.3 reuses the terminal parser inherited from the Microsoft Terminal `release-1.25` baseline. The shell assets emit only sequences that this checkout already handles. `src/winterm/Shell/Protocol` classifies payloads for diagnostics and tests; it never consumes terminal output or executes an action.

| Purpose | Sequence | Notes |
| --- | --- | --- |
| Current working directory | `OSC 9 ; 9 ; "C:\Path" ST` | The baseline accepts a quoted or unquoted Windows path. It does not yet implement OSC 7. |
| Prompt start | `OSC 133 ; A ST` | Starts the prompt portion of a command mark. |
| Command start | `OSC 133 ; B ST` | Ends the prompt portion. |
| Command executed | `OSC 133 ; C ST` | Provided by the inherited `autoMarkPrompts` path when available. |
| Command finished | `OSC 133 ; D ; exitCode ST` | PowerShell reports `0` for success and a nonzero value for failure. CMD sends `D` without an exit code. |

`ST` is `ESC \\`. The PowerShell module sends OSC 9;9 only for the FileSystem provider. It reports local drive and UNC paths, but does not represent a WSL, SSH, or other remote path as a local Windows filesystem path.

## Safety and compatibility

- A payload longer than 8,192 characters, containing a control character, or having an unknown shape is ignored by the winTerm diagnostic classifier.
- The inherited terminal dispatcher also ignores unsupported actions. No sequence can start a process, write a file, open a URL, change settings, or change shell compatibility mode.
- Command text is not included in a winTerm-specific protocol message and is not persisted by the v0.3 metadata types.
- The protocol version is independent from the application version. Version `1` remains compatible with a v0.3 module; future versions must preserve these messages or negotiate a different explicit capability.

For the baseline parser behavior, see `src/terminal/adapter/adaptDispatch.cpp` and the inherited mark specification in `doc/specs/#11000 - Marks/Shell-Integration-Marks.md`.
