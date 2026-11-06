#!/bin/sh
# Builds the display code on this PC and checks it against the hardware model.
# With a git ref it also proves the I2C writes are the same as in that version, in the same order:
#   ./run.sh [ref]
set -e
cd "$(dirname "$0")"
out=$(mktemp -d)
trap 'rm -rf "$out"' EXIT

g++ -std=c++17 -O1 -DSIMULATE -DHAS_STATS -DHAS_FAULT -DHAS_BITBANG -I. -I../src simulate.cpp ../src/display.cpp ../src/pca9685.cpp -o "$out/new"
"$out/new" "$out/new.trace"

if [ -n "$1" ]; then
  mkdir "$out/old"
  for f in display.cpp display.h pca9685.cpp pca9685.h config.h; do
    git show "$1:esp_flipdot/src/$f" > "$out/old/$f"
  done
  g++ -std=c++17 -O1 -I. -I"$out/old" simulate.cpp "$out/old/display.cpp" "$out/old/pca9685.cpp" -o "$out/oldsim"
  "$out/oldsim" "$out/old.trace" > /dev/null
  echo
  grep '^write' "$out/old.trace" > "$out/old.writes"
  grep '^write' "$out/new.trace" > "$out/new.writes"
  echo "Vergelijking met $1: $(wc -l < "$out/old.writes") writes oud, $(wc -l < "$out/new.writes") writes nieuw"
  cmp "$out/old.writes" "$out/new.writes" && echo "De writes en hun volgorde zijn identiek"
fi
