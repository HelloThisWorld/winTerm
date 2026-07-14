# Clipboard and paste protection

The v0.3 source foundation contains a pure `PasteRiskAnalyzer`. It examines only the paste candidate in memory and returns decision data: line count, character count, final newline, unexpected control characters, command separators, and a small set of suspicious patterns. It does not modify text, save it, send it over the network, or execute it.

The intended default confirmation triggers are multiple non-empty lines, a final newline, configured large text, unexpected control characters, multiple command separators, and suspicious destructive patterns. Detection is advisory: it can have false positives and false negatives and must never claim to recognize every harmful command.

The pending terminal-control UI will offer Paste, Paste without final newline, Copy to editor or clipboard, and Cancel. A preview must be truncated safely and must not expose a complete large or secret-containing paste. Cancel must send no input.

Right-click behavior remains an application integration acceptance item: selected text should copy and clear selection, no selection should paste, Shift+right-click should open the context menu, and terminal mouse-reporting mode must take precedence for full-screen programs. The analyser is designed to be invoked before bracketed-paste wrapping so the exact clipboard text stays unchanged.
