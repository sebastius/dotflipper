# Original firmware

The original dotflipper firmware for the v1 controller (`../hardware-v1`), written by hand in 2018-2020:
an Arduino sketch for an ESP32 with Game of Life, Snake and Tetris (with a joystick), and showing frames
received over UDP. This is the version of 31 March 2020.

It is kept for history and is **heavily outdated**: the current firmware is in `../software`. It has
fewer protections against shorts and stuck coils, its pulse length depends on the I2C writes, and the
WiFi network is set in the source code: fill in your own network in `esp_flipdot.ino` before using it.

Open `esp_flipdot/esp_flipdot.ino` in the Arduino IDE.
