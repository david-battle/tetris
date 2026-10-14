# Handoff

State as of 2026-10-14. Working tree clean; latest commits:

- `ae01e17` Add hint key, cheat mode (Ctrl mirror), and paused badge
- `3a92db2` Record badge/CONTROLS layout caveat in handoff notes
- `7561f88` Add autoplay mode; gate ghost extras and add mode badges

Build/run: `make` (must stay warning-clean under `-Wall -Wextra`), `./run.sh`.

## What easy mode does now

- Two-piece lookahead. `easyBestDrop()` (tetris.c ~168) scans every orientation
  and column for the current piece, `tryPlace()`s each onto a scratch board, then
  `bestNext()` searches the preview piece (`nextType`) the same way from that
  result. A candidate is scored `400 * (clears by both pieces) + scoreBoard()`,
  minus a small `-|bx - (COLS/2 - 1)|` center bias (applied per piece).
- `scoreBoard()` is the shape alone; `clearLines()` does the mutation.
  Weights: clear 400, tallest 12, total stack 5, buried holes 8 (depth-weighted),
  bumpiness 4.
- `SPAWN()` passes `nextType` (already the re-rolled preview at that point).

## Other toggles/behaviour

- `G` toggles the hard-drop ghost (`ghost`, starts on): both the translucent
  ghost cells and the pending-line-clear row highlight are gated by it.
- Easy on: holding SPACE re-drops each newly spawned piece every 0.25s
  (`dropRepeat`, tetris.c ~330). Easy off: one hard drop per press, unchanged.
- `H` hint: snaps the live piece to the easy-mode best placement (orientation
  + column, reusing `easyBestDrop`) without dropping it; skipped if that
  would collide at the piece's current height. Works regardless of easy mode.
- `C` toggles cheat mode (`cheat`); its badge is a third switch-style one.
  While on, either Ctrl key runs `mirrorShape()` on the live piece: cells flip
  left-right within the piece's own (centered) grid, and the piece becomes its
  mirror partner — J<->L and S<->Z, with the partner's type/color (the
  function returns the new type). I/O are mirror-symmetric so skipped; T does
  a 180 turn instead (feedback on a tap). Skipped if the mirrored shape would
  collide at the current spot.
- `A` toggles autoplay (`autoPlay`): forces easy on + ghost off, then hard
  drops the easy-best placement every 0.125s (double the SPACE-hold rate)
  with no key held. Pressing `E` to turn easy off also clears autoplay;
  toggling autoplay off leaves easy/ghost as-is. EASY/GHOST/CHEAT badges always
  show ON/OFF; AUTOPLAY shows only while active and PAUSED only while halted.
- Layout: the EASY/GHOST/CHEAT badges stack from `oy + 5*cell` downward (each
  `mfs + 8` tall), with AUTOPLAY and PAUSED (shown while active/halted,
  amber) as fourth/fifth slots; CONTROLS starts at `oy + 9*cell` to clear
  all five.

## Caveats / don't re-break

- CONTROLS is 13 rows and the badge stack is now up to 5 slots tall. At a
  16:9-ish window everything fits with room to spare, but on a much
  shorter/wider render the last control row can crowd the bottom edge. If
  that shows up, shrink `mfs`/`cfs` or lower the badge count, not `cy`.
- Both `H` (hint) and `Ctrl` (cheat mirror) silently no-op when the target
  placement/mirror would collide at the piece's current position — that's
  intended, not a missed input. Hint doesn't require easy mode to be on; it
  just reuses `easyBestDrop`.

## Facts worth not re-deriving

- RNG is raylib's `rprand` (Xoshiro128** seeded by SplitMix64), seeded from
  `SetRandomSeed(time(NULL))` inside `InitWindow`. Our only call site is
  `GetRandomValue(0,6)` in `SPAWN`.
- A piece run (e.g. 7x S in a row) is genuine RNG, not a bug, and is
  reproducible: read `rprand_state`/`rprand_seed` from `/proc/<pid>/mem`
  (`nm tetris` gives the offsets; binary is a non-stripped PIE), then subtract
  `4 * 0x9e3779b97f4a7c15` from `rprand_seed` to get the original seed and
  replay the Xoshiro128** stream.
- Quirk: because the seed is `time(NULL)`, two launches in the same second
  replay the same piece sequence. Fix would be a higher-resolution seed; not a
  bug in our code.

## Possible next steps (none started)

- Tune auto-repeat intervals (0.25s hold, 0.125s autoplay).
- Try different easy-mode weights, or look further ahead.
- Optional RNG-seed change noted above.
