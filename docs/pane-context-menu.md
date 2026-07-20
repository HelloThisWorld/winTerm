# Pane Context Menu

Right-clicking a pane handle or pressing the overflow button opens the same command model:

```text
Pane
├── Split
│   ├── Top
│   ├── Bottom
│   ├── Left
│   └── Right
├── Move
│   ├── Move to New Tab
│   └── Move to New Window
├── Close Pane
└── More
    ├── Focus Pane
    ├── Zoom Pane
    └── Pane Settings
```

The menu uses explicit labels. It does not use ambiguous terms such as Pop or Release.

## Capability state

- Move to New Tab is disabled when the pane is already the only pane in the tab.
- Move to New Window is disabled when the current window host cannot reattach a live pane in the same process.
- Split directions are disabled when the focused pane is too small for the requested axis.
- Close and layout-changing commands are disabled while a transaction protects the pane.

Every disabled command exposes a reason as tooltip/secondary text and in its accessible name.

## Safety

Move operations retain the existing pane node and session ownership. The source is removed only as part of a validated transaction after the target is prepared. Close Pane delegates to the existing safe close path and does not bypass close confirmation.

Terminal-content right-click remains governed by terminal copy/paste settings. Pane-header right-click always requests the pane menu.
