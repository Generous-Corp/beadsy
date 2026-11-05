#!/bin/zsh
# Run every in-process UI proof step against one standalone binary, one fresh
# process per step. Usage: ui_proof.sh <binary> <out-dir> [steps...]
set -u
BIN=$1; OUT=$2; shift 2
mkdir -p "$OUT"
STEPS=("$@"); [ ${#STEPS[@]} -eq 0 ] && STEPS=(layout click-freeze click-seed click-quality click-assign menu-freeze menu-seed menu-quality menu-assign grip-drag grip-drag-vertical)
for step in "${STEPS[@]}"; do
  pkill -x "$(basename "$BIN")" 2>/dev/null; sleep 0.3
  rm -f "$OUT/$step.png"
  PULP_SCREENSHOT="$OUT/$step.png" PULP_FRAMES=200 BEADSY_UI_PROOF="$step" timeout 40 "$BIN" > "$OUT/$step.log" 2>&1
  echo "== $step rc=$? png=$( [ -f "$OUT/$step.png" ] && sips -g pixelWidth -g pixelHeight "$OUT/$step.png" | awk '/pixel/{printf "%s ", $2}' )"
  grep -h 'beadsy-ui-proof' "$OUT/$step.log" | sed 's/^\[beadsy-ui-proof\] //'
done
