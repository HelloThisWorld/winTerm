# Empty layout slots

An Empty Layout Slot reserves a rectangular place in a tab without starting or retaining a shell. It is a schema-v2 layout node, participates in geometry and accessibility, and does not count toward session or pane limits.

The default presentation is:

```text
Drop a tab or pane here
Open shell
Close slot
```

Dropping a pane or complete tab subtree replaces the slot directly. No extra split is created, and every moved session keeps its identity. `Open shell` is a separate explicit user action: it opens profile selection and replaces the slot only after the user chooses a profile. Restore never interprets a preferred profile as permission to launch a shell automatically.

Closing a slot removes it and normalizes its parent. When the final tab in a window would otherwise have no root, the default policy keeps one empty tab with one new stable slot.

Slot IDs are opaque stable strings. Import accepts only the slot ID, an optional preferred profile reference, and safe appearance metadata. Commands, executable paths, arguments, environment variables, process or content IDs, URLs, plugin data, output, and clipboard data are rejected.

An Empty Slot is keyboard focusable and exposes its name, role, enabled state, available actions, and current drop state to UI Automation. Its high-contrast presentation includes a border, icon, and text so state is not communicated by color alone.
