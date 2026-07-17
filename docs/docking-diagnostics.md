# Docking diagnostics

## Safe trace data

`DockingDiagnostics` keeps a bounded in-memory ring of operation metadata:

- Source and target type.
- Selected zone.
- Transaction result.
- Duration in milliseconds.
- Layout node count.
- An allow-listed error category.

The trace does not contain terminal output, command text, clipboard content, environment variables, working directories, drag payloads, tokens, raw addresses, or persistent session identifiers. Unknown error text is reduced to the category `other`. Trace history is not serialized into a Workspace.

## Layout health

The C++ health inspector and `validate-layout.ps1` report:

- Node, pane, and empty-slot counts.
- Duplicate pane or slot IDs.
- Layout panes without declared sessions.
- Declared sessions absent from the layout.
- Invalid split ratios.
- Cycles in the in-memory C++ graph.
- Maximum tree depth.
- Overall result.

The script accepts a Workspace file, named Workspace, Last Session, test fixture, or a runtime layout snapshot. Runtime inspection expects the application adapter to publish a redacted `current-layout.json`; that adapter is not enabled yet.

Validation is read-only by default. `-Normalize` also requires a distinct `-OutputPath`, never overwrites the source, and only repairs structural single-child splits and supported ratio bounds. Duplicate ownership or unknown nodes remain errors.

## UI actions

The planned Settings diagnostics page can bind these core operations:

- Validate current layout.
- Normalize to a user-selected output file.
- Copy redacted diagnostics.
- Clear in-memory layout history.
- Reset docking settings.

The current checkout provides the core and command-line validation. The XAML diagnostics page is not implemented or accepted.
