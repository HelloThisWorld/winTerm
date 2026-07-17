# winTerm 1.0 accessibility audit

Static checks cover accessible Docking descriptions, keyboard command descriptors, non-color labels, and privacy-safe diagnostics categories. They do not replace assistive-technology testing.

| Required flow or environment | Result | Evidence |
| --- | --- | --- |
| Launch, new tab, close tab, split, and pane focus | Not available | Requires a packaged Windows 11 test. |
| Settings, Theme, Font, save and open Workspace | Not available | Requires keyboard-only packaged validation. |
| Keyboard Docking, Dock Tab, Dock Pane | Skipped with reason | Runtime Docking is disabled. Model keyboard commands pass static checks. |
| Layout Undo and Redo | Skipped with reason | Runtime Docking is disabled. Model history passes static checks. |
| Diagnostics and update controls | Not available | Diagnostics requires packaged validation; update check is disabled pending consent UI. |
| Narrator | Not available | No recorded manual session. |
| High Contrast | Not available | No recorded manual session. |
| 100%, 150%, 200%, and 300% text scaling | Not available | No recorded manual session. |
| Keyboard-only usage | Not available | No recorded end-to-end session. |
| Pseudo-localization | Not available | No complete `en-XA` run is recorded. |

No core control may have invisible focus, communicate state only through color, expose an unlabelled Docking zone or import dialog, read internal GUIDs to a screen reader, or create a keyboard trap.

Accessibility gate: **Not available**. Public Stable publication is blocked until the core keyboard and Narrator paths pass.
