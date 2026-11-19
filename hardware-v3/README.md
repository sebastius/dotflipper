# Controller v3

> **AI-generated and untested.** The schematic, board placement, scripts and part choices in this
> folder were made with an AI assistant (Claude) and have not been built or tested on real hardware
> yet. The board is routed and passes DRC (2026-11-19). Check everything yourself before ordering, and test the first
> board with a current-limited supply: these drivers switch 20 V coils, and a mistake can short them.

One flip dot panel (32x32, two 32x16 halves) per board.

- Six groups of eight lines. Each group: two 74HC595s on the same data, one feeding an XL62783 (lines
  to 20 V), one feeding a ULN2803A (lines to ground).
- DIR_X1, DIR_ROW and nOE gate the 595 outputs through a 74HC32 and 74HC14, so plus and minus of
  one line or row can never be on together, also not after a software bug. Change DIR only while nOE
  is high.
- Boards chain over a 10-way flat cable (J2 to J3 of the next board). Each board buffers the link with
  a 74AHCT541; the chain data leaves through a flip-flop on the falling SCLK edge. One ESP32-S3-DevKitC-1
  drives all boards. Close JP1 on the last board.
- 20 V from a USB-C PD module, 5 V from an AP63205 buck on the board.

Files:

- `generate.py`: makes the schematic and updates the board (placement and routing stay;
  `--opnieuw` starts the board over)
- `export_jlc.py`: Gerbers, BOM and CPL for JLCPCB (`--kostencheck` for a quote from an unrouted board)
- `bits.csv`: which bit of the chain drives which wire, for the firmware

Not yet verified on hardware: coil peak current (estimated 450-480 mA, the drivers are rated 390-400 mA
recommended, 500 mA absolute), the ESP32 footprint (print it 1:1), the buck regulator layout, and the
rotations in the CPL for anything but the JLC preview.
