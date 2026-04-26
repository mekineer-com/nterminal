# NTerminal

NTerminal adds a compose editor strip to QTerminal for drafting multi-line input before sending it to the terminal. Works with Claude Code, Codex CLI, Gemini CLI, and plain shells (bash/ash/zsh).

Startup: compose editor is visible at the bottom, keyboard focus starts on the terminal input field.
Compose editor auto-grows up to 12 lines by default (`NTERMINAL_COMPOSE_MAX_LINES` env var to override).

## Shortcuts

| Shortcut | Action |
|---|---|
| `Ctrl+Enter` | Send editor contents to terminal and execute |
| `Ctrl+Numpad Enter` | Same as above (numpad Enter key) |
| `Ctrl+Shift+Up` | Transfer editor contents to terminal input field (no execute) |
| `Ctrl+Shift+Down` | Grab terminal selection into editor |
| `F6` | Toggle compose editor visibility |

### Notes on `Ctrl+Shift+Down` (terminal → editor)

In Claude Code's fullscreen TUI, mouse events go to the app so plain drag does not create a terminal-level selection. Use **Shift+drag** to force a terminal-level selection, then `Ctrl+Shift+Down`.

In Codex, Gemini, and plain shells, normal drag works.

## CLI Compatibility

All three AI CLIs and plain shells are confirmed working for replace, `?` preservation, and submit.

| CLI | Submit | Transfer | Notes |
|-----|--------|----------|-------|
| Claude Code | 200ms delays | Bracketed paste | Shift+drag for selection in fullscreen TUI |
| Codex CLI | 100ms delay | Direct | |
| Gemini CLI | `?` primer + 100/200ms | `?` primer + 100ms | Preserves literal `?` characters |
| bash/ash/zsh | 100ms delay | Direct | Uses `\r` not Key_Return to avoid double-Enter |

## Vendored QTermWidget Patches

NTerminal vendors a [patched QTermWidget](https://github.com/mekineer-com/qtermwidget) with fixes that benefit all qtermwidget users (upstream PR: [lxqt/qtermwidget#638](https://github.com/lxqt/qtermwidget/pull/638)):

- **Bottom-anchored scroll on resize:** when the terminal shrinks, the viewport stays put instead of jumping to the top of scrollback. Stock qtermwidget/qterminal has this bug.
- **Suppress pty resize API:** `setSuppressPtyResize(bool)` lets the host app prevent SIGWINCH during internal layout changes (compose editor, search bar). Without this, TUI apps redraw on every resize, duplicating history.

## Architecture

All compose logic lives in `src/compose.cpp` / `src/compose.h`. `mainwindow.cpp` stays close to upstream qterminal with ~50 lines of wiring.

| File | Purpose |
|------|---------|
| `src/compose.cpp` | Compose editor, CLI detection, submit/transfer, clear |
| `src/compose.h` | ComposeInput class |
| `src/updatecheck.cpp` | GitHub release update checker |
| `lib/qtermwidget/` | Vendored patched QTermWidget (git submodule) |

## Other Features

- **Double-submit guard:** rapid Ctrl+Enter presses won't send twice.
- **Unlimited scrollback** in compose mode — useful for Codex and Gemini sessions.
- **Update checker:** checks GitHub releases on startup, logs a notice if outdated.

---

# QTerminal

## Overview

![Screenshot of QTerminal with bookmarks pane and 3 tabs open](qterminal.png)

QTerminal is a lightweight Qt terminal emulator based on [QTermWidget](https://github.com/lxqt/qtermwidget).

It is maintained by the LXQt project but can be used independently from this desktop environment. The only bonds are [lxqt-build-tools](https://github.com/lxqt/lxqt-build-tools) representing a build dependency.

This project is licensed under the terms of the [GPLv2](https://www.gnu.org/licenses/gpl-2.0.en.html) or any later version. See the LICENSE file for the full text of the license.

## Installation

### Cloning

NTerminal vendors a patched [QTermWidget](https://github.com/mekineer-com/qtermwidget) as a git submodule. You must clone with `--recurse-submodules`:

```
git clone --recurse-submodules https://github.com/mekineer-com/nterminal.git
cd nterminal
```

If you already cloned without it:

```
git submodule update --init
```

### Building

Dependencies: Qt ≥ 6.6.0, [lxqt-build-tools](https://github.com/lxqt/lxqt-build-tools), CMake ≥ 3.18.0. Optional: [libcanberra](https://0pointer.net/lennart/projects/libcanberra/) for bell sounds.

No separate QTermWidget install is needed — nterminal builds it from the vendored submodule.

```
mkdir build && cd build
cmake ..
make -j$(nproc)
```

To install: `make install` (accepts `DESTDIR` as usual).

### Binary packages

Official binary packages are provided by all major Linux and BSD distributions.
Just use your package manager to search for string `qterminal`.

### Translation

Translations can be done in [LXQt-Weblate](https://translate.lxqt-project.org/projects/lxqt-desktop/qterminal/)

<a href="https://translate.lxqt-project.org/projects/lxqt-desktop/qterminal/">
<img src="https://translate.lxqt-project.org/widgets/lxqt-desktop/-/qterminal/multi-auto.svg" alt="Translation status" />
</a>
