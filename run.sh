#!/usr/bin/env bash
set -euo pipefail
REPO="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
cd "$REPO"

make -s tetris

# Launch in its own session so a shell/tool timeout can't kill the game.
# PULSE_LATENCY_MSEC trims the PulseAudio client buffer (default ~19ms) for
# snappier sound-effect onset on the WSLg RDP sink.
export PULSE_LATENCY_MSEC=20
setsid ./tetris >tetris.log 2>&1 </dev/null &
echo "tetris started (pid $!)"
