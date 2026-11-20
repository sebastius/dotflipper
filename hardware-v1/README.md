# Controller v1

The first dotflipper controller: one board per 32x16 panel, six PCA9685 PWM chips on I2C driving
ULN2803 sinks and UDN2982 sources. Designed in Eagle: schematic, board, PDFs and Gerbers are in this folder.

**Tested and working**: it runs the flip dot display, with the firmware in `../software` (PlatformIO env
`pico32`) or the original firmware in `../software-original`.

**But dangerous and slow:**

- Nothing in hardware keeps plus and minus of a wire apart. On the 32-side the source and the sink of a
  line share a wire: when the software switches both on, 20 V is shorted to ground. Only the software
  prevents that.
- A coil only gets a short pulse because the software switches it off again. A hang, a crash or a bug in
  the middle of a pulse leaves the coil on, far above what the drivers can take. That has happened.
- Pin 10 (COM) of the ULN2803s is not connected, so their clamp diodes don't catch the voltage spike of
  the coils.
- Every pulse takes several I2C writes to set up and end: about 0.9 ms per pair of dots, about half a
  second for the whole 32x32 screen.

The v3 controller in `../hardware-v3` fixes all of this in hardware.
