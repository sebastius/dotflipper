# Firmware

Firmware for both controllers, built with PlatformIO (see `platformio.ini`):

- v1 (`../hardware-v1`, PCA9685 over I2C, ESP32 PICO): `pio run -e pico32`
- v3 (`../hardware-v3`, chain of 74HC595 boards, ESP32-S3-DevKitC-1, 8 panels as 64x64): `pio run -e v3`

Clock faces, 26 animations and a web interface on http://flipdot.local, the same for both. WiFi is set up
from the phone through an access point on first boot. `src/main.cpp` lists what every file does.

- `simulate/run.sh`: checks the flip code of v1 and v3 against a model of the hardware, before anything
  goes onto a real board.
- `emulator/`: the v3 firmware in the browser, live at https://sebastius.github.io/dotflipper/
- `tools/wordfonts.py`: makes the word clock fonts, see `tools/fonts/README.md` for their licenses.

This firmware is being developed with the help of AI. The v3 part has not run on real hardware yet.
