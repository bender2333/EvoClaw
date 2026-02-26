#!/usr/bin/env bash
# EvoClaw Watchdog — 3s 自动重启
# 用法: watchdog.sh (后台运行)

set -euo pipefail

EVOCLAW_HOME="${EVOCLAW_HOME:-$HOME/.evoclaw}"
PID_DIR="$EVOCLAW_HOME"
BUILD_DIR="${BUILD_DIR:-./build}"
INTERVAL=3

PROCESSES=("evoclaw-engine" "evoclaw-compiler" "evoclaw-dashboard")

while true; do
  for proc in "${PROCESSES[@]}"; do
    if [ -f "$PID_DIR/$proc.pid" ]; then
      pid=$(cat "$PID_DIR/$proc.pid")
      if ! kill -0 "$pid" 2>/dev/null; then
        echo "[watchdog] $proc (PID $pid) died, restarting..."
        "$BUILD_DIR/src/${proc#evoclaw-}/$proc" &
        echo $! > "$PID_DIR/$proc.pid"
        echo "[watchdog] $proc restarted (PID $!)"
      fi
    fi
  done
  sleep "$INTERVAL"
done
