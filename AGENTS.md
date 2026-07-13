# winTerm Repository Guidance

## Build Rules

- Use the toolchain recommended by the Microsoft Terminal upstream project.
- New PowerShell scripts must stop on errors and return a nonzero exit code on failure.
- Build wrappers must preserve compiler output and must not hide compiler errors.
- Do not commit local absolute paths or generated build output.

## Code Rules

- Use English for code, function names, variable names, comments, logs, and error messages.
- Follow the existing Microsoft Terminal C++, C++/WinRT, XAML, and PowerShell style.
- Do not add unnecessary third-party dependencies or duplicate existing upstream features.
- Do not apply large formatting changes to files that are otherwise unmodified.
- Keep each commit focused on one purpose.
- Add the appropriate MIT license header to new source and script files, following upstream conventions.

## Protected Areas

Do not modify the following areas unless a winTerm v0.1 acceptance requirement cannot be met without doing so:

- ConPTY
- VT parser
- Text buffer
- Terminal renderer core
- Unicode width engine
- Input protocol parser
- OpenConsole internals

## Branding Rules

- Preserve the Microsoft Terminal license, copyright notices, and third-party notices.
- Do not describe winTerm as a Microsoft product or imply Microsoft endorsement.
- Do not use Microsoft, Windows, or Windows Terminal logos.
- New winTerm icons must use original placeholder geometry or original artwork.
- Keep the winTerm package identity, executable alias, application data, and signing identity separate from Microsoft Terminal.

## Scope Rules

winTerm v0.1 must not implement:

- Linux command translation
- Custom command completion
- Clink bundling
- Theme importers or bundled third-party themes
- Bundled fonts
- Emoji renderer changes
- Session restoration extensions
- Named workspaces
- Tab docking overlays or corner docking
- AI command generation
- Remote process persistence

