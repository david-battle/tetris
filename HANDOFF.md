# Handoff

State as of 2026-10-14. Working tree clean; latest commits:

- `21ab68a` Add handoff notes
- `1929d5f` Auto-repeat hard drop while SPACE held in easy mode
- `ce5369c` Add G key to toggle the hard-drop ghost piece

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
- `A` toggles autoplay (`autoPlay`): forces easy on + ghost off, then hard
  drops the easy-best placement every 0.125s (double the SPACE-hold rate)
  with no key held. Pressing `E` to turn easy off also clears autoplay;
  toggling autoplay off leaves easy/ghost as-is. EASY/GHOST badges always
  show ON/OFF; AUTOPLAY badge only shows while active.
- Layout caveat: the badges stack from `oy + 5*cell` downward (each
  `mfs + 8` tall) while CONTROLS starts at `oy + 7*cell`, so three badges
  just fit on normal aspect ratios. On a much wider/shorter render the
  AUTOPLAY badge can touch the CONTROLS header; nudge `my` or `cy` if that
  ever shows up.

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
