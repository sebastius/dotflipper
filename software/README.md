# Firmware

Firmware for both controllers, built with PlatformIO (see `platformio.ini`):

- v1 (`../hardware-v1`, PCA9685 over I2C, ESP32 PICO): `pio run -e pico32`
- v3 (`../hardware-v3`, chain of 74HC595 boards, ESP32-S3-DevKitC-1, 8 panels as 64x64): `pio run -e v3`

Clock faces, 26 animations, 5 games and a web interface on http://flipdot.local, the same for both. WiFi is
set up from the phone through an access point on first boot. `src/main.cpp` lists what every file does.

Games: Snake, Tron (up to 4 riders on 64x64, 2 on 32x32), Pong, Breakout and Tetris. Start one under
Spelletjes in the web interface; that opens a controller with buttons on the screen. The keyboard (arrows,
space, Z; a second player on WASD, F, G) and gamepads on the computer work too, through the browser.
The page sends the buttons to `POST /api/input?p=<player 0-3>&b=<held>&d=<pressed since last time>`,
bits: up 1, down 2, left 4, right 8, A 16, B 32. Players without anyone on the buttons are played by the
computer. After 90 seconds without a button the flipdot goes back to the clock.

- `simulate/run.sh`: checks the flip code of v1 and v3 against a model of the hardware, before anything
  goes onto a real board.
- `emulator/`: the v3 firmware in the browser, live at https://sebastius.github.io/dotflipper/.
  `./native 60 game:snake` plays a game with random buttons.
- `tools/wordfonts.py`: makes the word clock fonts, see `tools/fonts/README.md` for their licenses.

This firmware is being developed with the help of AI. The v3 part has not run on real hardware yet.
