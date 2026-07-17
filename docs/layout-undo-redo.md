# Layout undo and redo

Visual Docking history keeps up to 20 committed layout operations by default and can be configured from 1 through 100.

Each entry contains:

- a sequence and transaction ID;
- the move operation and user-facing description;
- cloned before and after source/target layout models;
- stable session IDs and generation-tagged owners;
- before and after focus state.

Terminal output, scrollback, commands, environment data, clipboard data, process handles, pointers, views, overlays, and drag tokens are never stored.

Undo applies the `before` snapshot through the same transaction host boundary. Redo applies `after`. A new committed operation clears the redo stack. Reducing the limit removes the oldest entries.

Undo and redo are disabled safely when any referenced session has exited or cannot be retained. History cannot resurrect a process, undo an explicit close after the process ended, survive an application restart as live history, or replace sessions removed by Workspace import.

Cross-window undo is supported only while all referenced sessions remain live in the current winTerm process. It never restarts a shell to approximate the previous layout.
