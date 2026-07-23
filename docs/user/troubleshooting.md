# Troubleshooting

| Symptom | Likely cause | Safe resolution |
| --- | --- | --- |
| winTerm does not launch | Incomplete installation or blocked unsigned executable | Verify the official checksum, inspect Windows event logs, and reinstall from the official release. |
| Certificate or SmartScreen warning | The installer is unsigned or not publicly trusted | Download only from the official release and verify `SHA256SUMS.txt`. |
| PowerShell integration is unavailable | Profile or execution-policy boundary | Do not set a global unrestricted policy; inspect the module installation and selected profile. |
| Workspace cannot restore | Missing monitor, folder, profile, or invalid old state | Use the safe fallback and create a redacted diagnostic bundle. |
| Divider will not move farther | A child reached its terminal cell/header minimum size | Enlarge the window or close a pane; winTerm will not create an invalid layout. |
| A ratio does not snap | The point is disabled, outside the threshold, or invalid at the current size | Release Alt, enable snapping, choose Common ratios, or enlarge the owning split. |
| Resizing feels too sticky | Entry threshold is too large | Reset pane resizing or reduce the advanced logical-pixel threshold. |
| Escape restored the prior layout | Expected cancellation behavior | Release the pointer to commit; only committed ratios enter history and persistence. |
| An old pane movement binding does nothing | Pane repositioning was removed in 1.1 | Remove the legacy binding and use directed split, border resize, or Balance Panes. |
| Update check fails | Automatic updates are unavailable | Use the official GitHub Release manually. |
