# tetris

Standalone fullscreen Tetris clone in C on raylib. One source file, own git
repo. Keep changes small.

## Build / run

- `make` — builds `./tetris`. Links statically against the sibling raylib
  clone at `~/raylib` (`src/raylib.h`, `src/libraylib.a`). That clone is
  upstream `raysan5/raylib`, not personal; do not push it.
- `./run.sh` — builds if needed, then launches the game detached via `setsid`
  and returns immediately. Run GUI programs this way: a plain foreground launch
  inherits the agent tool's stdout pipe, and the tool's timeout then kills the
  whole process group (this looked like the game "dying" but was the launcher).
- Compiled binary `tetris` and `tetris.log` are gitignored; commit source and
  resources only.

## Files

- `tetris.c` — the whole game. 10x20 well, all 7 tetrominoes, move/soft-drop/
  rotate/hard-drop, line clears, scoring, levels with rising gravity,
  next-piece preview, game over + ENTER restart, on-screen controls panel, and
  external-monitor fullscreen with cell size scaled to the render resolution.
- `resources/` — audio only: `buttonfx.wav` (move), `ping_send.wav` (rotate),
  `coin.wav` (lock), `hit_splat.wav` (line clear), `boom.wav` (game over), and
  `country.mp3` (looped BGM). Paths are relative, so run the binary from the
  repo root (the run script does).

## Conventions

- If the user sends any message after a program was started, assume they exited
  it — check with `pgrep` instead of assuming it is still running. If a fresh
  copy must run and the old one is alive, kill it silently first; never ask and
  never offer to kill it.

## Things not to "simplify"

- **Rotation grids.** Each piece has its own square grid centered on its
  occupied cells — 3x3 for T/L/J/S/Z, 2x2 for O, 4x4 for I with the bar on its
  middle row (`SIZES[]`), rotating only the `n x n` sub-grid about that grid's
  midpoint. A single shared 4x4 grid rotated about the midpoint drifts the piece
  across cells on every turn. The per-piece centered grid is drift-free across
  four rotations; don't collapse it back to one grid.
- **Window placement on WSLg.** Do not go back to `FLAG_FULLSCREEN_MODE` +
  `SetWindowMonitor()`. WSLg's RAIL shell ignores the requested monitor and
  pins fullscreen X11 windows to the main screen, so the game always came up on
  the built-in panel. Instead the window is undecorated and sized/positioned
  over the chosen monitor (borderless-windowed fullscreen), which RAIL honors.
  Monitor geometry is only valid after `InitWindow()`, so monitor selection runs
  after it. `SelectExternalMonitor()` picks by physical panel size (the laptop
  panel is the smaller one); on WSLg both monitors are named `rdp-N`, so names
  can't distinguish them.
- **Lock cue.** There is no separate lock-delay timer: a grounded piece stays
  live until the next gravity tick. The white-pulse fill and blinking
  white/amber halo are keyed off the same `collide(board, cur.s, cur.x,
  cur.y+1)` test, so they stay honest if the timing changes. Keep them keyed
  together.
