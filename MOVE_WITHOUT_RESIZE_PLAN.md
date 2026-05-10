# Move-Without-Resize Plan

## Goal
Keep terminal rows/columns stable while compose grows.

The terminal should move upward inside the window as compose grows at the bottom. The terminal should not be resized during that growth.

## Why This Plan
The core failure mode is not submit timing. The core failure mode is resize churn.

When terminal height changes, qtermwidget recomputes lines, PTY gets `setWindowSize`, and the CLI process receives size-change signals. That chain causes redraw churn and can lead to repeated headers/multiples. Timing tweaks can hide symptoms but do not remove this chain.

This plan removes the chain by preserving terminal size and changing only position.

## Why Earlier Approaches Were Insufficient
`Submit delay` helped command-send reliability, but did not address terminal resize side effects.

`Debounce` reduced how often resize updates fired, but resize still happened eventually.

`Suppress` blocked updates in risky ways and produced side effects (missing status lines, instability).

`Overlay editor over terminal` avoided some layout pressure but still treated editor behavior as the center of control instead of enforcing a strict terminal-size contract.

## Core Principle
Treat terminal geometry as a contract:

The terminal viewport size is fixed for the current window state.

Compose growth is handled by moving terminal position, not changing terminal size.

If size does not change, PTY rows/cols do not change, and the CLI does not see moving top/bottom boundaries.

## Implementation Strategy
Phase 1: Capture a stable terminal geometry baseline.
Why: We need one source of truth for width/height to prevent accidental relayout writes.

Phase 2: Anchor compose to bottom and allow vertical growth.
Why: Compose growth is desired behavior; growth itself is not a bug.

Phase 3: Translate terminal upward by compose height delta.
Why: This creates visual space for compose without touching terminal size.

Phase 4: Recompute only positions on resize, tab switch, split focus change, and show/hide.
Why: Position updates are required for correctness, but size updates must stay blocked.

Phase 5: Guard against hidden resize writes.
Why: Any path that calls terminal resize during compose growth breaks the contract.

## Non-Goals
Do not optimize submit timing in this pass unless it blocks correctness.

Do not add new debounce layers as a primary fix.

Do not change PTY protocol behavior.

## Acceptance Criteria
Compose can grow/shrink and terminal remains same width/height from the PTY perspective.

No repeated init/after-compact header blocks caused by compose growth interactions.

No statusline disappearance/regression caused by compose interactions.

`Ctrl+Shift+Down`, manual typing, and multiline compose editing all preserve stable terminal rows/cols.

## Risk Notes
If any code path still derives terminal size from layout row height, the bug can return.

If split/tab focus changes are not tracked, compose/terminal positioning can desync.

If terminal movement leaks into size writes by framework side effects, add explicit guards at the boundary where PTY resize is requested.

## Feasibility And Concrete Implementation
Yes, this is possible. You are not inventing impossible behavior.

Why it is possible:
Qt lets us move a child widget without changing its width/height. If width/height do not change, terminal row/column size does not need to change.

How to implement it in this codebase:
1. In `src/compose.cpp`, stop putting compose in layout row math as the thing that changes terminal height during typing.
2. Keep one fixed terminal widget size baseline per active terminal view (capture after normal window resize, not after compose growth).
3. On compose height change, move the terminal widget up by compose height delta (`move(x, baselineY - composeHeight)`), but keep `resize(width, height)` untouched.
4. Keep compose pinned to the bottom of the visible container and let compose grow upward.
5. Re-run only position updates (not size updates) on these events: window resize, tab switch, split focus change, show/hide.
6. In the resize bridge to PTY (`Session::updateTerminalSize` call path), add a guard flag for compose-driven movement so move-only operations cannot trigger PTY resize writes.
7. Validate with live checks: while typing multiline compose, terminal rows/cols stay constant and multiples do not appear.

When this would fail:
If some hidden path still recalculates terminal size from layout height during compose growth, PTY resize events will still leak through. That is why the guard in step 6 is important.

## 2026-05-10 Revised Plan (Layout Reservation)
### Why the previous approach failed
- Moving `TabWidget` with `move()` creates fragile geometry behavior.
- `TabWidget` has its own method named `move(Direction)`, which is a smell for this path.
- The editor can still overlay status lines if position and container math drift by even a little.
- Freezing PTY resize caused stale width/columns; unfreezing restored width but overlay remained.

### Better approach
- Keep terminal and tab widgets fully layout-managed.
- Reserve bottom space in the central layout equal to compose editor height.
- Keep editor as an overlay child pinned to bottom of the window.
- Never move terminal widgets manually.
- Never suspend PTY resize for normal compose editing.

### Implementation steps
1. Store the central `QGridLayout*` in `ComposeInput`.
2. Replace move-based offset logic with one function that updates layout bottom margin:
   - margin = editor height when compose is visible
   - margin = 0 when compose is hidden/raw mode
3. Keep editor `setGeometry(0, containerHeight - editorHeight, containerWidth, editorHeight)` and `raise()`.
4. Remove baseline and manual move state (`m_tabBaseline` and related math).
5. Keep `onHostLayoutChanged()` as the trigger point for resize/tab/split/show/hide, but make it call only:
   - `positionComposeEditor()`
   - `updateLayoutReservation()`
6. Validate behavior:
   - status line remains visible at startup
   - editor growth does not cover terminal content
   - terminal width/columns keep updating with window resize
