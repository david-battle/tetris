#!/usr/bin/env bash
set -euo pipefail
REPO="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
cd "$REPO"

make -s tetris

# Launch in its own session so a shell/tool timeout can't kill the game.
setsid ./tetris >tetris.log 2>&1 </dev/null &
echo "tetris started (pid $!)"
