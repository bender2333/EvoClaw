#!/usr/bin/env bash
# EvoClaw 进程管理脚本 — Story 1.6 实现
# 用法: evoclaw.sh {start|stop|status}

set -euo pipefail

EVOCLAW_HOME="${EVOCLAW_HOME:-$HOME/.evoclaw}"
PID_DIR="$EVOCLAW_HOME"
BUILD_DIR="${BUILD_DIR:-./build}"

PROCESSES=("evoclaw-engine" "evoclaw-compiler" "evoclaw-dashboard")

start() {
  mkdir -p "$PID_DIR" "$EVOCLAW_HOME/data/events/engine" \
    "$EVOCLAW_HOME/data/events/compiler" "$EVOCLAW_HOME/data/events/canonical" \
    "$EVOCLAW_HOME/data/memory/working_memory" "$EVOCLAW_HOME/data/memory/user_model" \
    "$EVOCLAW_HOME/data/memory/episodic" "$EVOCLAW_HOME/data/memory/challenge_set" \
    "$EVOCLAW_HOME/logs"

  for proc in "${PROCESSES[@]}"; do
    if [ -f "$PID_DIR/$proc.pid" ] && kill -0 "$(cat "$PID_DIR/$proc.pid")" 2>/dev/null; then
      echo "$proc already running (PID $(cat "$PID_DIR/$proc.pid"))"
      continue
    fi
    echo "Starting $proc..."
    "$BUILD_DIR/src/${proc#evoclaw-}/$proc" &
    echo $! > "$PID_DIR/$proc.pid"
    echo "$proc started (PID $!)"
  done
}

stop() {
  for proc in "${PROCESSES[@]}"; do
    if [ -f "$PID_DIR/$proc.pid" ]; then
      local pid
      pid=$(cat "$PID_DIR/$proc.pid")
      if kill -0 "$pid" 2>/dev/null; then
        echo "Stopping $proc (PID $pid)..."
        kill "$pid"
        rm -f "$PID_DIR/$proc.pid"
      else
        echo "$proc not running, cleaning PID file"
        rm -f "$PID_DIR/$proc.pid"
      fi
    else
      echo "$proc not running"
    fi
  done
}

status() {
  for proc in "${PROCESSES[@]}"; do
    if [ -f "$PID_DIR/$proc.pid" ] && kill -0 "$(cat "$PID_DIR/$proc.pid")" 2>/dev/null; then
      echo "$proc: running (PID $(cat "$PID_DIR/$proc.pid"))"
    else
      echo "$proc: stopped"
    fi
  done
}

case "${1:-}" in
  start)  start ;;
  stop)   stop ;;
  status) status ;;
  *)      echo "Usage: $0 {start|stop|status}"; exit 1 ;;
esac
