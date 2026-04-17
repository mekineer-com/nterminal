# NTerminal Compatibility And Handoff Coverage

Last updated: 2026-04-14

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
| Claude Code `2.1.91` | F | W | F | User-confirmed: Claude-specific clear path still does not clear prefix lines in real session. Decision note: next build should remove Claude-specific clear path and return Claude to shared fallback clear. Submit path still uses Claude-specific raw `\r`. |
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

## Outlier Note
- Current outliers:
  - Claude replace/clear still fails in real user session. Next build decision: revert Claude clear to shared fallback clear (remove Claude-specific clear path).
  - Claude submit still failing in latest user report despite Claude-specific raw `\r` path.
  - Gemini clear and `?` preservation remain RT; submit is working with 300ms raw `\r`.
