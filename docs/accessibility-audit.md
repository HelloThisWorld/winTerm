# winTerm v0.6 accessibility audit

## Automated evidence

The local docking UI validation checks accessible descriptions, keyboard docking commands, privacy-safe diagnostics labels, and manifest references. It is source-level evidence only.

## Manual audit required before RC

| Area | Required verification | Status |
| --- | --- | --- |
| Keyboard navigation | Tabs, panes, Settings, Workspace actions, diagnostics, and exit | Not tested |
| Focus visibility and order | Light, dark, high-contrast, and scaled displays | Not tested |
| Narrator | Main window, tabs, panes, Settings, docking overlay, paste confirmation, recovery, update, and diagnostic dialogs | Not tested |
| High contrast | Overlay, selection, errors, success states, galleries, and update state | Not tested |
| Text and display scaling | 100 through 200 percent plus mixed DPI | Not tested |
| Pseudo-localization | `en-XA` expansion, non-ASCII text, and RTL layout resilience | Not implemented |

No core state may be conveyed by color alone. New public controls must expose a localized name, role, state, action, and keyboard path.
