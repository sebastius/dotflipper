# Emulator

The firmware for the v3 boards, compiled to WebAssembly and running on a model of eight boards with
32x16 panels (64x64 dots). Live: https://sebastius.github.io/dotflipper/

- The real code runs: `shiftchain.cpp` and `display595.cpp` drive the chain, the player, clock,
  animations and web interface are the same as on the ESP32-S3. Only WiFi, OTA and the network are left out.
- `hardware.cpp` is the hardware: the 595s, flip-flops, latch, DIR and nOE of eight boards and the RMT that
  makes the pulse on nOE, the same model as `simulate/simulate595.cpp`. A dot only flips when it gets a real pulse. The page shows the pulse
  length and counts rule breaks (LATCH or DIR with nOE low, more than one coil per board, wrong pulse length).
- Time is virtual: the cycle counter of a 240 MHz ESP32-S3, kept in step with the clock of the browser.
  The flips show at the moment the firmware makes them, so the speed on screen is the real speed.
- The web interface runs in a frame; its requests go to the web server of the firmware instead of the
  network. Settings are kept in the localStorage of the browser.

Build:

- `./build.sh` needs `emcc` from [emsdk](https://emscripten.org) 3.1.74 and writes `web/firmware.js` and `.wasm`.
  Serve `web/` with any web server, for example `python3 -m http.server -d web`.
- `./build.sh native` makes a test program for this PC: `./native 30 maze` runs 30 virtual seconds,
  plays the maze and prints the screen. With `SANITIZE=1` it checks every memory access.

The workflow `.github/workflows/emulator.yml` runs the simulations, builds the emulator and puts it on
GitHub Pages after every push to master that changes `esp_flipdot/`.
