# NTerminal Compatibility And Handoff Coverage

Last updated: 2026-04-18

## Sources Covered
- `/home/marcos/gemini-cli/HANDOFF.md`
- `/home/marcos/gemini-cli/_archive/doc-versions/HANDOFF-260411-squash-071428.md`

## Legend
- `W` = works in live test
- `F` = fails in live test
- `RT` = re-test needed after latest code change

## Current Compatibility Matrix (Versions As Rows)

| CLI Version | Replace Prompt From Editor | Preserve `?` In Text | `Ctrl+Enter` Submits Message | Notes |
|---|---|---|---|---|
| Claude Code `2.1.112` | RT | W | W | Submit works (raw `\r`). Clear leaves a hard block at one cursor position after multi-line inject; this build adds Ctrl+E + Ctrl+U at end of clear sequence to neutralize it — needs re-test. |
| Codex CLI `0.118.0` | W | W | W | Uses shared fallback clear path (`Ctrl+K` x8, `Ctrl+U` x8). Historical note: raw `\r` submit was bad for Codex; Enter-key submit is the known-good path. |
| Gemini CLI `0.35.2` | RT | RT | W | Gemini-specific hard pin: clear uses `Down x8` then `Ctrl+E`, then (`Ctrl+U` + Backspace) x8; submit uses raw `\r` after 300ms (100ms Enter-event attempt failed: newline). User-confirmed submit working. |

Marcos notes: claude and codex never had issues with the question mark: only gemini.

## Behavior And Feature Coverage (From Handoffs)

### Method Policy (Do Not Drift)
- Use one unified method per function first (for example: one submit path for all CLIs).
- Split into CLI-specific methods only when the unified method is proven failing in live tests.
- Every approved CLI-specific split must be recorded in the compatibility table row and notes.
- Treat the compatibility table as source of truth for exceptions; do not add hidden per-CLI behavior outside what the table documents.

### Active Behavior
- Compose mode integrated in forked terminal (`nterminal`) with editor strip in-terminal.
- `Ctrl+Enter` sends composed content with CLI-specific delayed submit timing.
- Submit timing by CLI:
  - Claude: 100ms after clear before insert, then raw `\r` 100ms after insert.
  - Gemini: raw `\r` after 300ms (user-confirmed).
  - Codex: Enter key event after 100ms.
  - Unknown: Enter key event after 100ms.
- Submit path uses Enter keypress+release by default, with Claude and Gemini using raw `\r`.
- `F6` toggles compose/raw mode.
- Startup focus: compose panel is visible, but focus starts on terminal input.
- Bidirectional transfer shortcuts:
  - `Ctrl+Shift+Down`: terminal selection -> compose editor.
  - `Ctrl+Shift+Up`: compose editor -> terminal input.
- Terminal input is cleared best-effort before compose text is injected.
- Compose transfer normalization removes selection artifacts (trailing whitespace + soft-wrap indent cleanup).
- CLI-aware handling exists in `src/mainwindow.cpp`:
  - Claude has independent clear and submit paths.
  - Gemini has independent clear and submit paths.
  - Codex has independent clear and submit paths.
  - Unknown CLI has explicit fallback clear and submit paths.
  - Claude and Gemini submit use raw `\r`; Codex/Unknown submit paths use Enter key event.

### Supporting Files And Paths
- Repo: `/home/marcos/gemini-cli/nterminal`
- Launcher: `/home/marcos/.local/bin/nterminal`
- Desktop entry: `/home/marcos/.local/share/applications/nterminal.desktop`
- Built binary artifact path: `/home/marcos/gemini-cli/nterminal/build/qterminal`
- Main implementation files:
  - `src/mainwindow.h`
  - `src/mainwindow.cpp`

### Documented Commits Mentioned In Handoffs
- `762b4b6`:
  - added compose input mode and stabilized `Ctrl+Enter` submit with delayed Enter (`100ms`).
- `bd845c0`:
  - compose/terminal transfer shortcuts and robust overwrite/selection cleanup.
- `c28feed`:
  - README shortcut and intro updates.

### Historical Attempts Noted (Important Context)
- Tried direct keypress submit (press/release Enter) for Codex in an earlier stage.
- Tried raw `\r` submit path at short delay; bad for Gemini in that form. Gemini raw `\r` at 300ms is user-confirmed working.
- Tried duplicate/newline mitigation paths:
  - shortcut duplication cleanup,
  - event-filter interception for `Ctrl+Enter`,
  - later rollback to pre-interceptor path on request.
- Delay tuning happened (`200ms` -> `100ms`).
- `?` drop/preservation issue was investigated; Gemini remains an active regression watch item.

### Repo/Distribution History
- Fork repo created and wired:
  - `https://github.com/mekineer-com/nterminal`
  - `origin` set to fork remote.
- Fork visibility switched to public.
- Local backup artifacts were created during launcher/desktop rewiring:
  - `/home/marcos/.local/bin/nterminal.orig`
  - `/home/marcos/.local/share/applications/nterminal.desktop.orig`

## Bidirectional Transfer (Ctrl+Shift+Down / Ctrl+Shift+Up)

### Ctrl+Shift+Down (terminal → compose)
- In Claude Code's fullscreen TUI, ink/React enables X11 mouse reporting. Mouse drag events go to the app, not to QTermWidget's selection engine. `copyAvailable` never fires for plain drag; cache stays empty.
- **Workaround required in Claude Code: use Shift+drag instead of plain drag.** Shift+drag forces the terminal emulator to create a terminal-level selection, bypassing app mouse reporting.
- This build adds a second cache path: `TermWidget` now also listens to `QClipboard::selectionChanged` (X11 PRIMARY). Shift+drag sets PRIMARY → `selectionChanged` fires → cache updated → Ctrl+Shift+Down finds it.
- No clipboard manager pollution: we only read PRIMARY (which the drag already set), never write to CLIPBOARD.
- Plain shell / Codex / Gemini: plain drag works (no app mouse reporting); `copyAvailable` path handles it.

### Scrollbar in Claude Code — not an nterminal bug
- `tui: fullscreen` (active since 2026-04-17) puts Claude in alt-screen mode. Alt-screen has no scrollback buffer; QTermWidget shows no scrollbar thumb because there is nothing to scroll.
- Scrollbar works correctly in plain shell sessions.
- No fix possible in nterminal without reverting `tui: fullscreen` in `~/.claude/settings.json`.

## Outlier Note
- Current outliers:
  - Claude clear/replace: hard block at cursor position after multi-line inject — Ctrl+E+Ctrl+U at end of clear sequence added in this build, needs re-test. Root cause may be in how Claude Code renders injected multi-line text rather than in clear residue.
  - Gemini clear and `?` preservation remain RT; submit is working with 300ms raw `\r`.
  - Ctrl+Shift+Down in Claude Code: requires Shift+drag (not plain drag) due to app mouse reporting. See section above.
