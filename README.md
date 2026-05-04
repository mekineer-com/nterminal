# NTerminal

NTerminal is a QTerminal fork that adds a compose editor for drafting multi-line input before sending it to terminal apps. It is tuned for Claude Code, Codex CLI, Gemini CLI, and plain shells.

This README is the quick operator guide. For implementation details and edge-case behavior, see [NTERMINAL_COMPAT_NOTES.md](NTERMINAL_COMPAT_NOTES.md).

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

## Shortcuts

| Shortcut | Action |
|---|---|
| `Ctrl+Enter` | Send editor contents and execute (editor focus only) |
| `Ctrl+Numpad Enter` | Same as above |
| `Ctrl+Shift+Up` | Transfer editor contents to terminal input (no execute) |
| `Ctrl+Shift+Down` | Pull terminal selection into editor |
| `F6` | Toggle compose/raw mode |

### Selection Note (`Ctrl+Shift+Down`)

In Claude Code fullscreen TUI, use **Shift+drag** to create a terminal-level selection before `Ctrl+Shift+Down`. In Codex/Gemini/plain shells, normal drag works.

## CLI Compatibility

| CLI | Submit Path | Transfer Path | Notes |
|-----|-------------|---------------|-------|
| Claude Code | Ctrl+U, wait 200ms, text, wait 200ms, `\r` | Bracketed paste | Shift+drag selection in fullscreen TUI |
| Codex CLI | text, wait 100ms, Enter key | Direct `sendText` | |
| Gemini CLI | `?`, wait 100ms, text, wait 200ms, `\r` | `?`, wait 100ms, text | Preserves literal `?` |
| bash/ash/zsh | text, wait 100ms, `\r` | Direct `sendText` | Avoids double-submit from Key_Return |

## Vendored QTermWidget Patches

NTerminal vendors a patched [QTermWidget](https://github.com/mekineer-com/qtermwidget) (upstream PR: [lxqt/qtermwidget#638](https://github.com/lxqt/qtermwidget/pull/638)).

- Bottom-anchored scroll on shrink (prevents jump-to-top).
- Debounced resize in `Session::onViewSizeChange()` (single SIGWINCH after layout settles).
- Cursor position clamping and image bounds hardening during resize transitions.

## Architecture

| File | Purpose |
|------|---------|
| `src/compose.cpp` | Compose editor, CLI detection, submit/transfer/clear |
| `src/compose.h` | `ComposeInput` class |
| `src/updatecheck.cpp` | GitHub release update checker |
| `lib/qtermwidget/` | Vendored patched QTermWidget submodule |

## Known Limitations

- Window-edge resize still triggers one redraw after debounce.
- Claude Code can clear its own scrollback; unlimited local scrollback helps mainly Codex and Gemini.
- Gemini `?` primer delays are empirical and may need tuning on slower systems.

## Upstream Base

NTerminal is based on:

- [LXQt QTerminal](https://github.com/lxqt/qterminal)
- [QTermWidget](https://github.com/lxqt/qtermwidget)
