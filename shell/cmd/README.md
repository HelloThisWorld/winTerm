# winTerm Command Prompt integration

`winterm-init.cmd` is designed for an explicit winTerm profile command such as:

```cmd
cmd.exe /K "call \"<package-path>\ShellAssets\cmd\winterm-init.cmd\""
```

It sets only process-local environment variables, configures prompt marks using the inherited `OSC 9;9` and `OSC 133` handlers, and adds DOSKEY macros only when neither a user macro nor a real `.exe` has the same name. It never modifies the AutoRun registry value.

CMD supplies no dependable pre-execution hook. The integration therefore relies on the upstream `autoMarkPrompts` behavior for command execution marks and reports CWD and prompt boundaries on each prompt. It does not claim full command completion or reliable exit-code reporting.

The `touch` and `open` macros call `winterm-shim.exe`. If that helper is missing, CMD remains usable and the affected macro reports a normal command failure.
