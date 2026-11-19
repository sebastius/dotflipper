#!/bin/sh
# Builds the display code on this PC and checks it against the hardware model: v1 (simulate.cpp)
# and v3 (simulate595.cpp).
# With a git ref it also proves the I2C writes are the same as in that version, in the same order:
#   ./run.sh [ref]
set -e
cd "$(dirname "$0")"
out=$(mktemp -d)
trap 'rm -rf "$out"' EXIT

g++ -std=c++17 -O1 -DSIMULATE -DHAS_STATS -DHAS_FAULT -DHAS_BITBANG -I. -I../src simulate.cpp ../src/display.cpp ../src/pca9685.cpp -o "$out/new"
"$out/new" "$out/new.trace"

echo
for layout in "-DPANELS_X=1 -DPANELS_Y=2" "-DPANELS_X=2 -DPANELS_Y=4"; do
  g++ -std=c++17 -O1 -DSIMULATE -DHARDWARE_V3 $layout -I. -I../src simulate595.cpp ../src/display.cpp ../src/display595.cpp ../src/shiftchain.cpp -o "$out/v3"
  "$out/v3"
  echo
done

if [ -n "$1" ]; then
  mkdir "$out/old"
  for f in display.cpp display.h pca9685.cpp pca9685.h config.h i2cpins.h; do
    git show "$1:esp_flipdot/src/$f" > "$out/old/$f" 2>/dev/null || rm -f "$out/old/$f"
  done
  # Older versions lack the counters, the emergency stop or the bit-banged bus
  flags=""
  grep -q I2CStats "$out/old/pca9685.h" && flags="$flags -DHAS_STATS"
  grep -q i2cfault "$out/old/pca9685.h" && flags="$flags -DHAS_FAULT"
  [ -f "$out/old/i2cpins.h" ] && flags="$flags -DHAS_BITBANG"
  g++ -std=c++17 -O1 -DSIMULATE $flags -I. -I"$out/old" simulate.cpp "$out/old/display.cpp" "$out/old/pca9685.cpp" -o "$out/oldsim"
  "$out/oldsim" "$out/old.trace" > /dev/null
  echo
  grep '^write' "$out/old.trace" > "$out/old.writes"
  grep '^write' "$out/new.trace" > "$out/new.writes"
  echo "Vergelijking met $1: $(wc -l < "$out/old.writes") writes oud, $(wc -l < "$out/new.writes") writes nieuw"
  cmp "$out/old.writes" "$out/new.writes" && echo "De writes en hun volgorde zijn identiek"
fi
