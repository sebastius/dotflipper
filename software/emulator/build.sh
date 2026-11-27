#!/bin/sh
# Builds the emulator: the firmware for the v3 boards, compiled for the browser, for 64x64 and 32x32.
#   ./build.sh            WebAssembly into web/ (needs emcc from emsdk on the PATH): firmware.js and firmware32.js
#   ./build.sh native     a native test program that runs 10 virtual seconds and prints the screen
#                         (PANELS="1 2" ./build.sh native for 32x32)
# ArduinoJson: from ARDUINOJSON (a directory with ArduinoJson.h), else from the PlatformIO build.
set -e
cd "$(dirname "$0")"
json=${ARDUINOJSON:-../.pio/libdeps/v3/ArduinoJson/src}
sources="hardware.cpp wifisetup.cpp"
for f in ../src/*.cpp; do
  case "$f" in
    */wifisetup.cpp|*/udpdisplay.cpp|*/pca9685.cpp) ;;  # Network and v1 hardware
    *) sources="$sources $f" ;;
  esac
done
common="-std=c++17 -O2 -DSIMULATE -DHARDWARE_V3 -DARDUINOJSON_ENABLE_ARDUINO_STRING=1 -Iinclude -I../src -I$json"
if [ "$1" = native ]; then
  # PANELS="1 2" for 32x32 (two panels on top of each other), default 64x64
  set -- ${PANELS:-2 4}
  g++ $common -DPANELS_X=$1 -DPANELS_Y=$2 ${SANITIZE:+-g -fsanitize=address,undefined} $sources -o native
  echo "Klaar: ./native [seconden]"
else
  # Two screens: 64x64 (eight boards) as firmware.js, 32x32 (two boards, as the first v3 flipdot) as firmware32.js
  mkdir -p web
  for size in "2 4 firmware" "1 2 firmware32"; do
    set -- $size
    emcc $common -DPANELS_X=$1 -DPANELS_Y=$2 $sources -o web/$3.js \
      -sMODULARIZE -sEXPORT_NAME=Firmware -sALLOW_MEMORY_GROWTH -sENVIRONMENT=web \
      -sEXPORTED_FUNCTIONS=_malloc,_free \
      -sEXPORTED_RUNTIME_METHODS=ccall,cwrap,UTF8ToString,HEAPF64,HEAP32
  done
  echo "Klaar: web/firmware.js (64x64) en web/firmware32.js (32x32), met hun .wasm"
fi
