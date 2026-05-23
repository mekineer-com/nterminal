# NTerminal Maintainer Notes

Last updated: 2026-05-23

Audience: maintainers and contributors.

If you are just trying to use nterminal, read `README.md` first.
This file is for "why does this code work this way?" and "where do I patch it safely?"

## What This File Covers

- Why compose mode behaves differently across Claude/Codex/Gemini.
- Why we keep resize behavior coalesced (next tick) instead of reacting on every callback.
- Which files are safe to change for app logic vs vendored widget behavior.
- Known edge cases so we do not reintroduce old regressions.

## Fast Mental Model

NTerminal is QTerminal plus a compose editor at the bottom.

- The compose editor is a local input buffer.
- Terminal apps still run in the PTY as usual.
- Compose growth should not spam PTY resizes.
- CLI-specific submit paths are intentional compatibility behavior, not accidental branching.

## File Map (Where To Edit)

- `src/compose.cpp`, `src/compose.h`
  - Compose editor UI, auto-grow, CLI detection, submit/transfer behavior.
- `src/mainwindow.cpp`
  - Small amount of compose wiring into the window lifecycle.
- `lib/qtermwidget/lib/Session.cpp`
  - Coalesced terminal resize scheduling (`onViewSizeChange` / `updateTerminalSize`).
- `lib/qtermwidget/lib/Screen.cpp`, `ScreenWindow.cpp`, `Emulation.cpp`
  - Bottom-anchored scroll behavior when shrinking.
- `lib/qtermwidget/lib/TerminalDisplay.cpp`
  - Cursor and image safety guards around resize transitions.

Rule of thumb:
- App behavior changes: patch `src/*` first.
- Terminal rendering/resize internals: patch vendored qtermwidget files.

## Compose Mode Contract

Enabled with `NTERMINAL_COMPOSE=1`.

What users expect:
- Enter inserts newline.
- Ctrl+Enter submits.
- Ctrl+Shift+Up transfers editor text to terminal without submit.
- Ctrl+Shift+Down pulls selected terminal text into editor.
- F6 toggles raw mode.

What maintainers must preserve:
- `m_submitInProgress` prevents accidental double submit races.
- CLI detection checks both foreground PID and shell PID (`/proc/<pid>/cmdline`).
- Unknown CLI path must stay safe for normal shells.

## CLI Compatibility (Intent)

| CLI | Why It Has Special Handling | Submit Path |
|-----|------------------------------|-------------|
| Claude Code | Fullscreen TUI + strict prompt handling | Ctrl+U -> 150ms -> text -> 120ms -> `\r` |
| Codex CLI | Reliable with direct text + Return key event | text -> 100ms -> Key_Return |
| Gemini CLI | `?` primer required to preserve literal behavior | `?` -> 100ms -> text -> 200ms -> `\r` |
| bash/ash/zsh | Safe generic shell path | text -> 100ms -> `\r` |

If you change timings, retest all four rows.

## Resize Strategy (Why It Exists)

### Coalesced resize in Session.cpp

`Session::onViewSizeChange()` schedules one `updateTerminalSize()` on the next event-loop tick.

Why:
- Compose and search-bar layout changes can fire many size callbacks quickly.
- Immediate resize on each callback created redraw churn and duplicated output symptoms.
- Coalescing keeps behavior deterministic and reduces churn.

### Compose growth vs PTY resize

Compose growth should primarily adjust offsets, not repeatedly resize the PTY.

Important detail:
- Reserve flips (`setRenderBottomReserve`) can still trigger a one-time PTY resize when switching between raw/compose reserve states.
- Ongoing compose growth should not cause repeated resize storms.

## Vendored QTermWidget Patch Rationale

### Bottom-anchored scroll on shrink

Problem:
- Shrink pushed lines into history but viewport could jump to top.

Fix:
- Track pushed-line count across batched resizes and apply it in `showBulk()`.

### Cursor/image hardening

Problem:
- Transient size mismatch could hit out-of-bounds cursor/image access.

Fix:
- Clamp cursor coordinates and early-return on zero-sized windows.

### Removed APIs

These were removed because they caused regressions or dead behavior:
- `setSuppressPtyResize(bool)`
- `sendCurrentSizeToPty()`
- compose `lastSelectedText` fallback cache

## Wrapped-Line Height Sync (High-Risk Area)

The wrapped-line bug family is timing + geometry sensitive.

Current stabilization stack (keep together):
1. Listen to `QAbstractTextDocumentLayout::documentSizeChanged`.
2. Queue `updateHeight()` using `Qt::QueuedConnection`.
3. Count visual rows from per-block `layout()->lineCount()`.
4. Keep internal editor scroll pinned to top while under the max-line cap.
5. Include document margin in height math.

Known edge case still seen sometimes:
- At exactly the cap boundary (line 12 default), a transient phantom row can appear in narrow wrapped-content sequences.
- This is most visible at exactly line 12 (not line 11 and not line 13), which points to a cap-boundary unit mismatch instead of a general wrapping bug.

Likely root cause:
- Row-count units and pixel geometry are still not perfectly aligned at the cap boundary.

Future root-fix direction:
- Keep row counts only for cap decisions.
- Compute final height from actual layout pixel extents.
- Remove magic vertical constants (for example fixed padding values) from row math and apply measured insets in one place.

## Known Limitations

- User edge-resizing still causes one redraw after coalesced update.
- Claude Code can clear its own scrollback (`ESC[3J`), so unlimited local history helps Codex/Gemini more than Claude.
- Gemini `?` timing values are empirical and may need tuning on slower systems.

## Build / Smoke Test

```sh
git clone --recurse-submodules https://github.com/mekineer-com/nterminal.git
cd nterminal
mkdir build && cd build
cmake ..
make -j$(nproc)
ctest --output-on-failure
```

Minimum manual checks after behavior changes:
- Claude submit works repeatedly (no dropped submits).
- Codex submit works (no extra newline/double submit).
- Gemini preserves literal `?` behavior.
- Ctrl+Shift+Down transfer still works in fullscreen TUI with Shift+drag.
