# NTerminal

NTerminal is a QTerminal fork that adds a compose editor for drafting multi-line input before sending it to terminal apps. It is tuned for Claude Code, Codex CLI, Gemini CLI, Grok Build, and plain shells.

This README is the quick operator guide. For implementation details, design rationale, and edge-case behavior, see [NTERMINAL_MAINTAINER_NOTES.md](NTERMINAL_MAINTAINER_NOTES.md).

Questions and design ideas: [Discussions](https://github.com/mekineer-com/nterminal/discussions). Bugs: [Issues](https://github.com/mekineer-com/nterminal/issues).

Who should read what:
- `README.md` (this file): how to install, run, and use compose mode.
- `NTERMINAL_MAINTAINER_NOTES.md`: why behavior is implemented this way and where to patch safely.

## Quick Start

Clone with submodules:

```sh
git clone --recurse-submodules https://github.com/mekineer-com/nterminal.git
cd nterminal
```

If already cloned without submodules:

```sh
git submodule update --init
```

Build:

```sh
mkdir build && cd build
cmake ..
make -j$(nproc)
```

Enable compose mode at launch with `NTERMINAL_COMPOSE=1`.

## Compose Mode

- Startup: compose editor is visible at bottom and focus starts in terminal.
- Auto-grow limit: 12 lines by default (`NTERMINAL_COMPOSE_MAX_LINES` to override).
- Raw input toggle: `F6` (hide editor and type directly into terminal).
- Editor right-click: **Clean up spacing** applies the terminal-selection
  cleanup to highlighted text, or the whole editor if nothing is selected.
  Undo restores the original text.

### Editor Reflex Safety (`Ctrl+Z`)

If you are used to editor shortcuts, `Ctrl+Z` in a terminal can suspend the app by mistake.  
Remap terminal control keys in your shell startup files (for example `~/.profile` and `~/.bashrc`):

```sh
stty intr ^X
stty susp ^]
```

- `Ctrl+X` becomes interrupt (`SIGINT`) instead of `Ctrl+C`.
- `Ctrl+]` becomes suspend (`SIGTSTP`) instead of `Ctrl+Z`.
- Open a new terminal tab/window after changing these lines.

## Shortcuts

| Shortcut | Action |
|---|---|
| `Ctrl+Enter` | Send editor contents and execute (editor focus only) |
| `Ctrl+Numpad Enter` | Same as above |
| `Ctrl+Shift+Up` | Transfer editor selection, or all contents if none, to terminal input (no execute) |
| `Ctrl+Shift+Down` | Pull terminal selection into editor |
| `F6` | Toggle compose/raw mode |

When Grok Build has terminal focus, `Esc` sends the `Ctrl+C` event Grok uses to
cancel its current turn. Other CLIs and the compose editor keep normal `Esc` behavior.

### Selection Note (`Ctrl+Shift+Down`)

In Claude Code fullscreen TUI, use **Shift+drag** to create a terminal-level selection before `Ctrl+Shift+Down`. In Gemini CLI and plain shells, normal drag works.

For Codex CLI, **Scrollback** mode supports NTerminal's normal selection and
drag-to-editor workflows. Codex's newer **Fullscreen** mode owns mouse selection:
**Shift+drag** creates a terminal-level selection for `Ctrl+Shift+Down`, but
dragging selected text into the compose editor remains unresolved (tested with
Codex 0.159.3). Disabling Codex auto-copy does not restore terminal-owned selection.
Choose Scrollback with Codex's `/tui` command, then exit and resume Codex.

## CLI Compatibility

NTerminal has internal compatibility paths for Claude Code, Codex CLI, Gemini CLI, Grok Build, and normal shells.
Users should only need the compose shortcuts above.

Implementation details live in [NTERMINAL_MAINTAINER_NOTES.md](NTERMINAL_MAINTAINER_NOTES.md).

## Vendored QTermWidget Patches

NTerminal vendors a patched [QTermWidget](https://github.com/mekineer-com/qtermwidget) (upstream PR: [lxqt/qtermwidget#638](https://github.com/lxqt/qtermwidget/pull/638)).

- Bottom-anchored scroll on shrink (prevents jump-to-top).
- Coalesced resize scheduling in `Session::onViewSizeChange()` (next-tick update; one SIGWINCH per settled geometry change).
- Cursor position clamping and image bounds hardening during resize transitions.

## Architecture

| File | Purpose |
|------|---------|
| `src/compose.cpp` | Compose editor, CLI detection, submit/transfer/clear |
| `src/compose.h` | `ComposeInput` class |
| `src/updatecheck.cpp` | GitHub release update checker |
| `lib/qtermwidget/` | Vendored patched QTermWidget submodule |

## Known Limitations

- Window-edge resize still triggers one redraw after coalesced resize scheduling.
- Claude Code can clear its own scrollback; unlimited local scrollback helps mainly Codex and Gemini.
- Gemini `?` primer delays are empirical and may need tuning on slower systems.

## Upstream Base

NTerminal is based on:

- [LXQt QTerminal](https://github.com/lxqt/qterminal)
- [QTermWidget](https://github.com/lxqt/qtermwidget)
