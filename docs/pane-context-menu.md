# Pane context menu

The pane icon, header right-click, and overflow button use one command model:

```text
Pane
├── Split
│   ├── Top
│   ├── Bottom
│   ├── Left
│   └── Right
├── Zoom Pane
├── Balance Panes
├── Pane Settings
└── Close Pane
```

**Balance Panes** sets the immediate split containing the focused pane to
50/50 when both children satisfy minimum sizes. It preserves both sessions and
creates one pane-resize history entry. Split directions are disabled when the
focused pane is too small on the required axis. Close uses the existing safe
close path.

Terminal-content right-click remains governed by terminal copy/paste settings.
The pane menu contains no reposition, docking-target, new-tab transfer, or
new-window transfer operations.
