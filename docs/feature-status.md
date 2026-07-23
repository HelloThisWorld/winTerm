# winTerm 1.1 feature status

This table records current evidence. **Implemented** means the runtime path and
tests are present; **Experimental** means a real Release validation is still
open; **Removed** means the capability is intentionally unavailable.

| Feature | Status | Default | Limitations |
| --- | --- | --- | --- |
| PowerShell 7 | Experimental | On when discovered | Packaged 1.1 launch validation is open. |
| Windows PowerShell 5.1 | Experimental | On | Packaged 1.1 launch validation is open. |
| CMD | Experimental | On | Packaged 1.1 launch validation is open. |
| WSL | Experimental | On when discovered | Distribution runtime matrix is open. |
| Directed pane splitting | Implemented | On | Release UI/menu validation is open. |
| Pane border resizing | Experimental | On | Source/model coverage exists; native Release validation is open. |
| Common-ratio snapping | Experimental | On | Model coverage exists; pointer feel and visual validation are open. |
| Resize undo/redo | Experimental | On | Exact runtime model is implemented; native validation is open. |
| Pane repositioning | Removed | Off | No pane drag, docking target, overlay, or movement command is available. |
| Tab reordering | Inherited | On | Unchanged by 1.1. |
| Workspace restore | Experimental | On | Schema/fixtures pass; repeated packaged restore is open. |
| Themes and fonts | Experimental | On | Asset validation exists; packaged visual inspection is open. |
| Installer and Portable | Experimental | Primary | 1.1 artifacts and upgrade validation are open. |
| ARM64 | Disabled | Off | No ARM64 build and launch evidence exists. |

Public 1.1 publication remains blocked until the native Release, DPI,
accessibility, renderer, shell, installer, Portable, and upgrade gates in
`docs/v1.1-acceptance.md` pass.
