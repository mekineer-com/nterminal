# NTerminal Compatibility Notes

Last updated: 2026-04-26

## Compatibility Matrix

| CLI | Replace Prompt | Preserve `?` | Submit | Notes |
|-----|---------------|-------------|--------|-------|
| Claude Code | W | W | W | Clear: Ctrl+U. Submit: 200ms delays. Transfer: bracketed paste. |
| Codex CLI | W | W | W | Clear: Ctrl+K + Ctrl+U ×8. Submit: Enter key after 100ms. |
| Gemini CLI | W | W | W | Clear: Down×8 + Ctrl+E + Ctrl+U×8. `?` primer trick on both paths. |
| bash/ash/zsh | W | W | W | Clear: Ctrl+K + Ctrl+U ×8. Submit: `\r` after 100ms (not Key_Return). |

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

### Suppress pty resize API (Session.h, qtermwidget.h)
`setSuppressPtyResize(bool)` skips `setWindowSize()` (TIOCSWINSZ/SIGWINCH) while set. Used by nterminal during compose resize and Ctrl+F search bar toggle. Without this, TUI apps receive SIGWINCH and redraw, duplicating history content.

### CMAKE_CURRENT_BINARY_DIR fix (CMakeLists.txt)
Generated `qtermwidget_version.h` used `CMAKE_BINARY_DIR` which breaks when built as `add_subdirectory()`. Changed to `CMAKE_CURRENT_BINARY_DIR`.

## Compose Editor Details

- `documentMargin(2)` for left/right padding (prevents first-character clipping).
- Unlimited scrollback history in compose mode (benefits Codex/Gemini long sessions; Claude Code clears its own scrollback).
- Height auto-grows up to `NTERMINAL_COMPOSE_MAX_LINES` (default 12).
- `QTimer::singleShot(0)` deferred height update so document layout runs before size query.

## Selection and Transfer

### Terminal → Editor (Ctrl+Shift+Down)
1. Try `selectedText(true)` (live selection)
2. Fall back to `lastSelectedText()` cache
3. Normalize: strip `\r`, trailing whitespace, soft-wrap indent artifacts
4. Insert at cursor (not replace)
5. Clear cache after transfer

Cache populated via `copyAvailable(true)` and X11 PRIMARY `selectionChanged` (focus-gated to prevent leaks from other apps).

### Editor → Terminal (Ctrl+Shift+Up)
- Claude: 100ms delay + bracketed paste (`\x1b[200~`...`\x1b[201~`)
- Gemini: `?` primer + 100ms + text
- Others: direct `sendText`

### Claude Code selection quirk
In `tui: fullscreen`, plain drag creates visual highlight but Screen repaints clear it before release. **Shift+drag** works. See upstream root cause: `Screen::clearSelection()` during alt-screen repaints.

## Known Limitations

- **Window resize** (user drags edge): SIGWINCH fires normally → TUI app redraws → possible history duplication. Rare and intentional; not suppressed.
- **Claude Code scrollback**: Claude clears its own scrollback (ESC[3J). Unlimited history in nterminal doesn't help Claude specifically. Benefits Codex and Gemini.
- **Gemini `?` timing**: 100ms delay between primer and text is empirical. May need adjustment on slower systems.

## Build

```
git clone --recurse-submodules https://github.com/mekineer-com/nterminal.git
cd nterminal && mkdir build && cd build && cmake .. && make -j$(nproc)
```

Launcher: set `NTERMINAL_COMPOSE=1` to enable compose mode.
