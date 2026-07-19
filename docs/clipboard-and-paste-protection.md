# Clipboard and paste protection

The v0.3 source foundation contains a pure `PasteRiskAnalyzer`. It examines only the paste candidate in memory and returns decision data: line count, character count, final newline, unexpected control characters, command separators, and a small set of suspicious patterns. It does not modify text, save it, send it over the network, or execute it.

The intended default confirmation triggers are multiple non-empty lines, a final newline, configured large text, unexpected control characters, multiple command separators, and suspicious destructive patterns. Detection is advisory: it can have false positives and false negatives and must never claim to recognize every harmful command.

The runtime terminal handler offers confirmation for multiline and large clipboard input. The pure analyser provides decision data for broader control-character, separator, and suspicious-pattern checks, but that broader UI integration remains separate work. Paste previews are truncated safely, and cancelling a confirmation sends no input.

The default right-click behavior is integrated in `ControlInteractivity`: selected text is copied and the selection is cleared; with no active selection, right-click requests a paste. Therefore, selecting text and right-clicking once copies it, while the next right-click pastes. `copyOnSelect` and `rightClickContextMenu` can intentionally change this behavior. Terminal mouse-reporting mode takes precedence for full-screen programs, and holding Shift temporarily suppresses VT mouse reporting so local selection and right-click handling remain available. Paste confirmation runs before clipboard text is handed back for bracketed-paste wrapping, so the exact clipboard text stays unchanged.
