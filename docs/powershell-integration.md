# PowerShell integration

The packaged module is `ShellAssets\powershell\winTerm.Shell\winTerm.Shell.psd1`, version `1.0.0`. It supports PowerShell 7 and Windows PowerShell 5.1 with the same syntax.

An explicit winTerm profile launcher must set these process-local variables before importing the module:

```powershell
$env:WINTERM_SESSION_ID = '<opaque-session-id>'
$env:WINTERM_INTEGRATION_VERSION = '1'
Import-Module '<package-path>\ShellAssets\powershell\winTerm.Shell\winTerm.Shell.psd1'
```

The module does not add this block to `$PROFILE`. A launcher must preserve normal PowerShell execution policy; the module neither uses nor recommends `-ExecutionPolicy Bypass`. If policy prevents importing a module, PowerShell must still launch and diagnostics should report the failure and recommend a user-reviewed policy or installation remedy.

## Prompt and marks

On an eligible session, the module captures the current `prompt` function, sends prompt/CWD marks around its output, and calls the original script block. A second import detects its own wrapper instead of nesting it. Removing `winTerm.Shell` restores the captured prompt when the wrapper is still active. This preserves common profile customizations, including prompt frameworks loaded before the module.

The module emits `OSC 133;A`, `OSC 9;9`, `OSC 133;B`, and then `OSC 133;D;<exit>` on the next prompt. The inherited `autoMarkPrompts` behavior supplies the command-executed transition. Command duration is intentionally not guessed from prompt idle time.

## Compatibility and completion

Compatibility mode resolves in this order: session override, `WINTERM_PROFILE_COMPATIBILITY_MODE`, `WINTERM_COMPATIBILITY_MODE`, then `Safe`. `Off` disables winTerm compatibility functions; `Extended` is labelled experimental and currently provides only the Safe command set.

The module never exports a compatibility function over an existing user command. Each function additionally defers to a real application with the same name. Native PowerShell commands and aliases such as `cls`, `pwd`, `ls`, `cat`, `clear`, and `history` are not replaced.

Path parameters use PowerShell's native completion. `which` adds a command completer only when winTerm owns that function. PSReadLine is detected but its key bindings, edit mode, prediction source, and option values are not modified.

`Get-WinTermShellDiagnostics` reports the shell version, activation state, protocol, marker status, prompt wrapper, completion provider, PSReadLine status, compatibility mode, and a redacted failure category. It does not expose a full current path, command history, environment, or terminal output.
