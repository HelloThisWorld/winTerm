# Docking security

## Opaque drag token

The operating-system drag package carries only a random 256-bit hexadecimal token and a winTerm format marker. The token is generated with the Windows system-preferred cryptographic random generator.

The in-memory `DragPayloadRegistry` maps that token to:

- source application instance ID;
- source window, tab, and pane or subtree IDs;
- capability type;
- expiration time;
- internal source model reference.

The token expires after 30 seconds by default and is removed when consumed, cancelled, failed, or completed. Resolution requires the expected source instance and capability. Consumption is single-use.

## Data not placed in drag payloads

- command line or shell arguments;
- working directory;
- environment variables;
- profile executable;
- terminal output or scrollback;
- clipboard content;
- process ID or process handle;
- session or content ID;
- C++ pointer or XAML object.

Those values are not logged as token metadata either.

## External and cross-process rejection

A missing, forged, expired, reused, wrong-capability, or wrong-instance token resolves to no source and cannot acquire a session lease. Another process cannot use an internal token to obtain `ControlInteractivity`, and cross-process live transfer remains disabled.

The inherited `PID + windowId` drag properties are not sufficient authorization for Visual Docking. The v0.5 host adapter must resolve and consume the opaque registry token before it can start a transaction.

## Ownership validation

Every moved live session must:

- exist in the ownership registry;
- be live and marked transferable;
- belong to the current source instance;
- have the expected generation and owner;
- hold an active transfer lease;
- receive one non-empty target window/tab/pane owner;
- not collide with another live pane owner.

Failed ownership validation does not detach or restart a shell.

## Diagnostics privacy

Diagnostics record operation type, zone, target kind, durations, state transitions, counts, and redacted stable IDs. They do not record token values, commands, environment variables, terminal output, clipboard content, credentials, or raw process handles.
