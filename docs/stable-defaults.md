# winTerm 1.0 default behavior

This table records the current candidate behavior. It does not turn an unverified capability into Stable status.

| Setting or behavior | Default | Notes |
| --- | --- | --- |
| Theme | Inherited terminal default | `winTerm Midnight` is bundled but is not claimed as the runtime default without packaged validation. |
| Font | Cascadia family | Bundled Cascadia Code and Cascadia Mono are app-private. No Nerd Font is bundled. |
| Linux Safe Compatibility | On for opted-in local PowerShell and CMD integration | Off for WSL, Git Bash, SSH, and REPL contexts. Real executables take precedence. |
| Restore last session | On in the winTerm Workspace settings model | Packaged restore remains Experimental. |
| Crash recovery | On with a user prompt | Automatic silent recovery is off. |
| Right-click copy/paste | Inherited terminal behavior | Packaged interaction validation remains open. |
| Multiline and suspicious paste warning | On where the winTerm paste analyzer is integrated | Clipboard contents are never logged. |
| Update check | Off | No request is sent until an explicit consent path is implemented. |
| Crash upload | Off | Future upload remains opt-in. |
| Usage telemetry | Off | No command, terminal, clipboard, Workspace, directory, or usage analytics are collected. |
| Edge and corner Docking | Off at runtime | The model defaults exist, but the runtime adapter is Disabled. |
| Layout Undo | Off at runtime | Model history exists; runtime Docking is Disabled. |
| Cross-process Pane transfer | Off | Rejected; Shells are never restarted to imitate transfer. |

First launch does not upload data, modify a PowerShell profile, modify CMD AutoRun, change execution policy, install global fonts, download Themes, or enable crash upload.
