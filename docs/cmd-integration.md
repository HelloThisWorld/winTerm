# Command Prompt integration

The package contains `ShellAssets\cmd\winterm-init.cmd`. A profile may explicitly launch it with:

```cmd
cmd.exe /K "call \"<package-path>\ShellAssets\cmd\winterm-init.cmd\""
```

Initialization is process-local and idempotent. It sets `WINTERM_SESSION_ID`, `WINTERM_INTEGRATION_VERSION`, and a Safe compatibility default when those variables are absent. It does not require elevation, modify the AutoRun registry value, or write a global setting.

The script wraps the existing `PROMPT` template with inherited OSC 9;9 and OSC 133 marks. CMD has no safe pre-execution hook, so it relies on the inherited `autoMarkPrompts` behavior for the command-executed transition and cannot claim PowerShell-equivalent exit-code reporting or completion.

`winterm-doskey.cmd` adds `ll`, `la`, `clear`, `pwd`, `which`, `cat`, `ls`, `touch`, and `open` only when no same-named DOSKEY macro and no same-named `.exe` already exists. It does not overwrite user macros. `touch` and `open` use `winterm-shim.exe`, which accepts Unicode arguments and uses Windows APIs rather than calling PowerShell or composing `cmd /c` strings.

The helper supports `touch <path...>`, `open <target>`, `version`, and `doctor`. It returns `0` on success, `1` on a general error, `2` for invalid arguments, `3` for a missing target, `4` for access denied, and `5` for an unsupported operation.
