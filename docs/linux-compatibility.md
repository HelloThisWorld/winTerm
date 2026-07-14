# Linux and macOS command compatibility

winTerm v0.3 is not a Bash translator or GNU utility bundle. It offers a small, shell-aware Safe mode for local PowerShell and CMD sessions only. WSL, Git Bash, SSH, and other Unix-like profiles should default to `Off`, because those shells already provide their own command semantics.

| Command | PowerShell Safe mode | CMD Safe mode | Notes |
| --- | --- | --- | --- |
| `ll`, `la` | `Get-ChildItem -Force` | `dir /a` | `-a` and `--all` accepted in PowerShell. Other GNU flags fail clearly. |
| `which name` | Friendly `Get-Command` result | `where` | Reports name, command type, and source/path in PowerShell. |
| `touch path...` | Create or update `LastWriteTime` | `winterm-shim.exe touch` | Does not create missing parent directories or truncate content. |
| `open target` | `Invoke-Item` for existing files or HTTP(S) URLs | `winterm-shim.exe open` | Uses Windows file/URL associations; no download occurs. |
| `clear`, `cls`, `pwd`, `ls`, `cat`, `history` | Native behavior retained | Safe DOSKEY mapping where appropriate | No PowerShell native alias is overwritten. |

The precedence rule is: an explicit user command, a native shell command or real executable, then a winTerm mapping, then normal command-not-found behavior. CMD checks existing DOSKEY macros and `.exe` files before adding a mapping. PowerShell does not export a mapping when a command already exists and also defers to a real application found later.

The following are intentionally not translated: `rm`, `rm -rf`, `sudo`, `chmod`, `chown`, `grep`, `sed`, `awk`, `find`, `xargs`, arbitrary `tar` flags, shell redirection, command substitution, Bash variables, loops, and conditionals. A missing command keeps the normal shell error; winTerm does not generate a destructive alternative.

`Off` disables winTerm mappings. `Safe` is the default. `Extended` is an experimental label for future explicitly-reviewed additions; it currently behaves like Safe and does not promise broader compatibility.
