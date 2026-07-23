# Pane resize undo and redo

Pane resize history stores up to 20 committed ratio changes per tab. Each entry
contains only the owning runtime split ID and exact before/after ratios.

One continuous pointer drag creates one record at release. Keyboard resize and
Balance Panes also create one record when they change a ratio. Pointer movement,
hover, snap indicator, pointer coordinates, and cancelled transactions are not
stored. A new resize after undo clears redo.

Undo and redo apply the exact ratio through the existing pane tree. If the
owning split no longer exists, the operation fails safely and cannot recreate
closed shells. History is runtime-only and is not serialized into workspaces;
normal workspace persistence stores the current final ratio.
