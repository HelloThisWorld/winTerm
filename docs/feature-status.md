# winTerm 1.0 feature status

This table records verified evidence, not intended capability. The only allowed classifications are **Stable**, **Experimental**, and **Disabled**. A feature is not Stable merely because its source or fixtures pass validation.

| Feature | Status | Default | Limitations |
| --- | --- | --- | --- |
| PowerShell 7 | Experimental | On when discovered | Source discovery is validated; packaged runtime launch is not yet recorded. |
| Windows PowerShell 5.1 | Experimental | On | Profile and shell assets are source-validated; packaged runtime launch is not yet recorded. |
| CMD | Experimental | On | Shell assets and conservative macros are validated; packaged runtime launch is not yet recorded. |
| WSL | Experimental | On when discovered | Dynamic discovery is validated; no distribution runtime matrix is complete. |
| Git Bash | Disabled | Off | No stable integration promise. |
| Linux Safe Compatibility | Experimental | On for local PowerShell and CMD; off elsewhere | Conservative mappings only; never a general Linux command translator. |
| Themes | Experimental | On | Asset hashes and licenses pass; packaged visual inspection remains open. |
| Theme Import | Experimental | On | Data-only import boundaries pass static validation; live dialogs remain unverified. |
| Fonts | Experimental | On | App-private bundled Cascadia fonts pass hash validation; runtime fallback remains unverified. |
| ANSI True Color | Experimental | On | Inherited upstream implementation; winTerm packaged runtime validation is open. |
| Color Emoji | Experimental | On where DirectWrite supports it | Complex sequences and cell width can vary. |
| Workspace Restore | Experimental | On | Schema, migration, fallback, and fixtures pass; repeated packaged restore is unverified. |
| Named Workspaces | Experimental | On | Data model and persistence foundations exist; packaged workflow validation is open. |
| Crash Recovery | Experimental | On | Snapshot and recovery foundations pass; crash-loop and packaged recovery tests are open. |
| Tab Dragging | Disabled | Off | Runtime Docking adapter is disabled. |
| Pane Dragging | Disabled | Off | Runtime Docking adapter is disabled. |
| Edge Docking | Disabled | Off | Model and transaction validation pass; live UI validation is open. |
| Corner Docking | Disabled | Off | Model and transaction validation pass; live UI validation is open. |
| Empty Slots | Experimental | On in the layout model | Model-only until runtime Docking is enabled. |
| Cross-window Tab Transfer | Experimental | Off | Same-process model only; no shell restart fallback. |
| Cross-window Pane Transfer | Disabled | Off | Cross-process transfer is rejected. |
| Layout Undo/Redo | Experimental | On in the layout model | Model validation exists; runtime UI remains disabled. |
| Update Check | Disabled | Off | winTerm makes no update request without a future explicit consent path. |
| Diagnostic Bundle | Experimental | User initiated | Redaction boundaries pass static checks; end-to-end bundle inspection remains open. |
| ARM64 | Disabled | Off | No native ARM64 install and launch evidence exists. |

## Stable-release consequence

The current candidate is not eligible for public Stable publication while core shell, installation, accessibility, and runtime Workspace evidence remain Experimental or unavailable. The Release workflow can prepare an explicitly blocked Draft without promoting it.
