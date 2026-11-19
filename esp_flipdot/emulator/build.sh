#!/bin/sh
# Builds the emulator: the firmware for the v3 boards with 8 panels (64x64), compiled for the browser.
#   ./build.sh            WebAssembly into web/ (needs emcc from emsdk on the PATH)
#   ./build.sh native     a native test program that runs 10 virtual seconds and prints the screen
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
flags="-std=c++17 -O2 -DSIMULATE -DHARDWARE_V3 -DPANELS_X=2 -DPANELS_Y=4 -DARDUINOJSON_ENABLE_ARDUINO_STRING=1 -Iinclude -I../src -I$json"
if [ "$1" = native ]; then
  g++ $flags ${SANITIZE:+-g -fsanitize=address,undefined} $sources -o native
  echo "Klaar: ./native [seconden]"
else
  mkdir -p web
  emcc $flags $sources -o web/firmware.js \
    -sMODULARIZE -sEXPORT_NAME=Firmware -sALLOW_MEMORY_GROWTH -sENVIRONMENT=web \
    -sEXPORTED_FUNCTIONS=_malloc,_free \
    -sEXPORTED_RUNTIME_METHODS=ccall,cwrap,UTF8ToString,HEAPF64,HEAP32
  echo "Klaar: web/firmware.js en web/firmware.wasm"
fi
