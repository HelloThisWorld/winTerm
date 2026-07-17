# winTerm 1.0 performance and resource validation

The source tree includes non-runtime Workspace and Docking model benchmarks. No packaged Windows 11 performance or soak run is available for this candidate.

| Scenario group | Result | Evidence |
| --- | --- | --- |
| Workspace and Docking model scripts | Passed | Non-runtime scripts execute in CI; they do not measure packaged UI or shell processes. |
| Cold and warm startup; PowerShell and CMD startup | Not available | Requires a signed packaged build on named hardware. |
| Single and large Workspace restore | Not available | Requires packaged runtime instrumentation. |
| Theme Gallery and Font Picker | Not available | Requires packaged UI measurement. |
| ANSI, Emoji, scrollback, Tab, Pane, and Docking load | Not available | Requires runtime and renderer resource measurement. |
| Shutdown | Not available | Requires process and handle tracking. |
| 500 Tab, Pane, and Docking operations | Not available | Runtime Docking is disabled and no soak environment is provisioned. |
| 100 Workspace restores, Settings opens, Theme imports, and diagnostic bundles | Not available | No provisioned soak environment. |

The release must check shell processes, ConPTY objects, file handles, drag tokens, session leases, Workspace timers, XAML objects, renderer resources, and temporary files for persistent unbounded growth. A median startup regression above 15 percent or Workspace/Docking model regression above 20 percent requires investigation against a recorded baseline.

Performance gate: **Not available**. No claim of leak-free long-duration behavior is made.
