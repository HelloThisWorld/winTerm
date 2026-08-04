# PowerShell integration

The packaged module is `ShellAssets\powershell\winTerm.Shell\winTerm.Shell.psd1`; its module version tracks the winTerm release. It supports PowerShell 7 and Windows PowerShell 5.1 with the same syntax.

A winTerm launcher must set these process-local variables before importing the module:

```powershell
$env:WINTERM_SESSION_ID = '<opaque-session-id>'
$env:WINTERM_INTEGRATION_VERSION = '1'
Import-Module '<package-path>\ShellAssets\powershell\winTerm.Shell\winTerm.Shell.psd1'
```

The module does not add this block to `$PROFILE`. A launcher must preserve normal PowerShell execution policy; the module neither uses nor recommends `-ExecutionPolicy Bypass`. If policy prevents importing a module, PowerShell must still launch and diagnostics should report the failure and recommend a user-reviewed policy or installation remedy.

## Automatic integration for bare PowerShell profiles

winTerm performs the launcher steps automatically when a profile's commandline
is a bare PowerShell invocation, so the Command Timeline and shell-lifecycle
progress work out of the box for the stock Windows PowerShell profile. The
rules are deliberately narrow and are implemented in
`src/winterm/Shell/AutoIntegration.h`:

- Only `powershell.exe` and `pwsh.exe` are recognized, by executable basename.
- The only arguments tolerated on the original commandline are `-NoLogo` and
  `-NoExit`. Any other argument — including `-Command`, `-File`,
  `-EncodedCommand`, `-NoProfile`, or `-ExecutionPolicy` — means the user has
  customized the invocation, and it launches unchanged.
- The rewrite appends `-NoExit -Command` with a fragment that sets the two
  session variables and imports the packaged module with
  `-ErrorAction SilentlyContinue`. Execution policy is never altered, and an
  import failure leaves a working shell without integration.
- A commandline that already mentions the module is not rewritten again, so a
  restarted connection stays stable.

The per-profile setting `"shellIntegration.autoInject"` (default `true`)
disables the rewrite when set to `false`. Because `-Command` is present on the
rewritten invocation, PowerShell suppresses its startup banner; this is the
standard behavior of every launcher-based shell integration.

## Prompt and marks

On an eligible session, the module captures the current `prompt` function and wraps it. A second import detects its own wrapper instead of nesting it. Removing `winTerm.Shell` restores the captured prompt when the wrapper is still active. This preserves common profile customizations, including prompt frameworks loaded before the module.

The wrapper returns a single string with the marks embedded around the original prompt text: `OSC 133;D;<exit>` (from the second prompt on), `OSC 133;A`, `OSC 9;9`, the original prompt output, then `OSC 133;B`. Embedding matters: the console host writes a prompt function's console output before it writes the returned text, so side-effect writes would place the command-start mark before the visible prompt and the recorded command region would include the prompt itself. Each sequence is terminated by ESC `\`; in PowerShell single quotes that is `'\'` — one character, since backslash is not an escape character in PowerShell strings. The inherited `autoMarkPrompts` behavior supplies the command-executed transition. Command duration is intentionally not guessed from prompt idle time.

## Component resilience

Some antivirus engines block individual script files at parse time. A component file that fails to dot-source is skipped and recorded — `Get-WinTermShellDiagnostics` reports a redacted failure category — instead of spilling a parse error into the user's session. Shell integration needs only the `Private` components; a blocked `Compatibility.ps1` costs the compatibility commands and nothing else. Only functions that actually loaded are exported.

## Compatibility and completion

Compatibility mode resolves in this order: session override, `WINTERM_PROFILE_COMPATIBILITY_MODE`, `WINTERM_COMPATIBILITY_MODE`, then `Safe`. `Off` disables winTerm compatibility functions; `Extended` is labelled experimental and currently provides only the Safe command set.

The module never exports a compatibility function over an existing user command. Each function additionally defers to a real application with the same name. Native PowerShell commands and aliases such as `cls`, `pwd`, `ls`, `cat`, `clear`, and `history` are not replaced.

Path parameters use PowerShell's native completion. `which` adds a command completer only when winTerm owns that function. PSReadLine is detected but its key bindings, edit mode, prediction source, and option values are not modified.

`Get-WinTermShellDiagnostics` reports the shell version, activation state, protocol, marker status, prompt wrapper, completion provider, PSReadLine status, compatibility mode, and a redacted failure category. It does not expose a full current path, command history, environment, or terminal output.
