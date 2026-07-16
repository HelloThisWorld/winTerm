# Named workspaces

Named workspaces are durable templates stored under `workspaces/named`. Their generated ID, not their display name, forms the local file name. Names are bounded data and cannot affect paths.

The manager supports explicit Save As, Rename, Duplicate, Delete, and Set Default operations. Save As removes recovery state and creates user-owned metadata. Duplicate requires a new ID and timestamps and never inherits default status. Rename changes only display metadata. Setting a default clears the flag on other named workspaces.

Delete first creates a recovery snapshot of the current runtime state. It deletes only the named definition and does not close or delete running shell sessions. Named files are never updated by autosave; Last Session is the autosave target.

`index.json` contains ID, name, relative file name, update time, default state, and health. It is rebuilt from valid named files and is not authoritative. A damaged index therefore cannot permanently hide the workspace definitions.

The backend operations are implemented. Gallery cards, confirmation dialogs, and user interaction remain runtime UI work requiring a built application.
