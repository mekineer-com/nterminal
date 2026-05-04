# NTerminal Compatibility Notes

Last updated: 2026-05-01

Audience: maintainers and contributors. This is the technical companion to `README.md` (which is the quick operator guide).

## What NTerminal Adds

Fork of qterminal with a compose editor for AI CLIs (Claude Code, Codex, Gemini).

**Compose mode** (`NTERMINAL_COMPOSE=1`):
- Multi-line text editor below the terminal. Enter adds newlines; Ctrl+Enter sends.
- Auto-detects which CLI is running and adapts submit/clear behavior per CLI.
- F6 toggles raw input mode (bypasses editor, types directly into terminal).

**Keyboard shortcuts** (compose mode):
- **Ctrl+Enter** — send editor contents to terminal (only when editor is focused)
- **Ctrl+Shift+Down** — grab highlighted terminal text into editor (or just focus editor if nothing selected)
- **Ctrl+Shift+Up** — transfer editor contents into terminal input without submitting
- **F6** — toggle raw input mode

**Terminal improvements**:
- Unlimited scrollback history (benefits Codex and Gemini; Claude Code clears its own).
- Debounced resize — layout changes from the compose editor produce one clean terminal redraw instead of many.
- Bottom-anchored scroll — terminal stays at bottom when shrinking (upstream PR: lxqt/qtermwidget#638).

## Compatibility Matrix

| CLI | Replace Prompt | Preserve `?` | Submit | Notes |
|-----|----------------|--------------|--------|-------|
| Claude Code | Yes | Yes | Yes | Clear: Ctrl+U. Submit: 200ms delays. Transfer: bracketed paste. |
| Codex CLI | Yes | Yes | Yes | Clear: Ctrl+K + Ctrl+U x8. Submit: Enter key after 100ms. |
| Gemini CLI | Yes | Yes | Yes | Clear: Down x8 + Ctrl+E + Ctrl+U x8. `?` primer trick on both paths. |
| bash/ash/zsh | Yes | Yes | Yes | Clear: Ctrl+K + Ctrl+U x8. Submit: `\r` after 100ms (not Key_Return). |

## Architecture

All compose logic in `src/compose.cpp` / `src/compose.h`. `mainwindow.cpp` has ~50 lines of wiring.

CLI detection: reads `/proc/<pid>/cmdline` for foreground and shell processes. Matches "claude", "codex", "gemini". Falls back to Unknown (bash/zsh path).

## Submit Timing

| CLI | Step 1 | Wait | Step 2 | Wait | Step 3 |
|-----|--------|------|--------|------|--------|
| Claude | Ctrl+U | 200ms | text | 200ms | `\r` |
| Gemini | `?` | 100ms | text | 200ms | `\r` |
| Codex | text | 100ms | Key_Return | — | — |
| Unknown | text | 100ms | `\r` | — | — |

Double-submit guard (`m_submitInProgress`) prevents Ctrl+Enter race across all CLIs.

## Vendored QTermWidget Patches

Submodule at `lib/qtermwidget/` → [mekineer-com/qtermwidget](https://github.com/mekineer-com/qtermwidget).
Upstream PR: [lxqt/qtermwidget#638](https://github.com/lxqt/qtermwidget/pull/638).

### Bottom-anchored scroll (Screen.cpp, ScreenWindow.cpp, Emulation.cpp)
`Screen::resizeImage()` pushes lines to history when terminal shrinks, but `ScreenWindow::_currentLine` wasn't adjusted in the non-tracking path → viewport jumped to top. Fix: `_resizePushedLines` counter, accumulated across batched resizes, reset in `showBulk()`.

### Debounced resize (Session.cpp)
`Session::onViewSizeChange()` debounces `updateTerminalSize()` through a 150ms single-shot timer. Compose editor height changes and search bar toggling trigger many rapid layout events; the debounce coalesces them into one SIGWINCH at the correct final size. Replaced earlier suppress approach (`setSuppressPtyResize`) which caused status line disappearing and cursor OOB crashes without fixing the duplication.

### Cursor position clamping (TerminalDisplay.cpp)
`cursorPosition()` clamps Y to `_lines-1` and X to `_columns-1`. Safety net for transient size mismatches during resize debounce window. `updateImage()` also early-returns if windowLines/windowColumns are zero.

### CMAKE_CURRENT_BINARY_DIR fix (CMakeLists.txt)
Generated `qtermwidget_version.h` used `CMAKE_BINARY_DIR` which breaks when built as `add_subdirectory()`. Changed to `CMAKE_CURRENT_BINARY_DIR`.

### Removed APIs / Behavior
- `setSuppressPtyResize(bool)` removed (caused regressions and did not solve duplication).
- `sendCurrentSizeToPty()` removed (no callers after suppress path removal).
- `lastSelectedText` fallback cache removed from compose transfer path (caused stale ghost paste).

## Compose Editor Details

- `documentMargin(2)` for left/right padding (prevents first-character clipping).
- Unlimited scrollback history in compose mode (benefits Codex/Gemini long sessions; Claude Code clears its own scrollback).
- Height auto-grows up to `NTERMINAL_COMPOSE_MAX_LINES` (default 12).
- `QTimer::singleShot(0)` deferred height update so document layout runs before size query.

## Selection and Transfer

### Terminal → Editor (Ctrl+Shift+Down)
1. Try `selectedText(true)` (live selection)
2. If empty: just focus the editor (no transfer)
3. Normalize: strip `\r`, trailing whitespace, soft-wrap indent artifacts
4. Insert at cursor (not replace)

No fallback cache — `lastSelectedText` was removed (caused ghost paste from stale selections).

### Editor → Terminal (Ctrl+Shift+Up)
- Claude: 100ms delay + bracketed paste (`\x1b[200~`...`\x1b[201~`)
- Gemini: `?` primer + 100ms + text
- Others: direct `sendText`

### Claude Code selection quirk
In `tui: fullscreen`, plain drag creates visual highlight but Screen repaints clear it before release. **Shift+drag** works. See upstream root cause: `Screen::clearSelection()` during alt-screen repaints.

## Known Limitations

- **Window resize** (user drags edge): debounced to one SIGWINCH after layout settles. TUI app still redraws once; history duplication possible but reduced vs pre-debounce.
- **Claude Code scrollback**: Claude clears its own scrollback (ESC[3J). Unlimited history in nterminal doesn't help Claude specifically. Benefits Codex and Gemini.
- **Gemini `?` timing**: 100ms delay between primer and text is empirical. May need adjustment on slower systems.

## Build

```sh
git clone --recurse-submodules https://github.com/mekineer-com/nterminal.git
cd nterminal
mkdir build && cd build
cmake ..
make -j$(nproc)
```

Launcher: set `NTERMINAL_COMPOSE=1` to enable compose mode.
