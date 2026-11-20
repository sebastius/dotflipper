# Datasheets for controller v3

| File | Part | On the board | Source |
|---|---|---|---|
| 74HC595.pdf | 8-bit shift register with output latch | U1, U2, U5, U6, U9, U10, U13, U14, U17, U18, U21, U22: two per group of eight lines | TI |
| XL62783.pdf | Xinluda XL62783, 8 DMOS sources to 50 V, 500 mA | U3, U7, U11, U15, U19, U23: lines to 20 V | Xinluda revision 2.4, via LCSC (part C556260) |
| ULN2803A.pdf | 8 Darlington sinks, 500 mA | U4, U8, U12, U16, U20, U24: lines to ground, COM to 20 V | TI, revision G (copy: TI has discontinued the part, its link is gone) |
| ULN2803C.pdf | the successor TI names for the ULN2803A | not fitted, for reference | TI |
| 74HC32.pdf | quad OR gate | U25: nOE and DIR to the OE of the 595s | TI |
| 74HC14.pdf | hex Schmitt trigger inverter | U26: DIR inverted, SCLK inverted for the flip-flop | TI |
| 74AHCT541.pdf | octal buffer with TTL inputs | U28: buffers the chain from the ESP32 or the previous board | TI |
| 74LVC1G79.pdf | single D flip-flop | U29: chain data to the next board on the falling edge of SCLK | TI |
| INA180.pdf | current sense amplifier (A2: gain 50) | U30: coil current over the 0.1 ohm shunt R8, to IO4 | TI |
| AP63205.pdf | Diodes AP63205, 2 A buck converter, 5 V | U27: 5 V from 20 V | Diodes |
| SS14.pdf | 1 A, 40 V Schottky diode | D1: 5 V to the ESP32, not back. The fitted part is the MDD SS14 (LCSC C2480), this is the Vishay datasheet of the same part | Vishay |
| BAV99.pdf | dual diode in series | not on this board: one per dot in the panels, it steers the coil current | onsemi |
| ESP32-S3.pdf | Espressif ESP32-S3 chip | A1, the controller | Espressif |
| ESP32-S3-WROOM-1.pdf | module on the DevKitC | A1 | Espressif |
| ESP32-S3-DevKitC-1_schematic_v1.1.pdf | schematic of the development board | A1, its pins and 5 V input | Espressif |

Not here: the Toshiba TBD62783A, of which the XL62783 is a copy (same pinout and package, a drop-in
alternative), because the Toshiba site refuses automatic downloads:
https://toshiba.semicon-storage.com/info/docget.jsp?did=30523

Sources, as downloaded in November 2026:

- TI: https://www.ti.com/lit/ds/symlink/ followed by sn74hc595.pdf, sn74hc32.pdf, sn74hc14.pdf, sn74ahct541.pdf,
  sn74lvc1g79.pdf, ina180.pdf and uln2803c.pdf
- ULN2803A: https://www.fdi.ucm.es/profesor/mendias/DAS-ise-spartan/docs/uln2803a.pdf (TI SLRS049G)
- XL62783: https://atta.szlcsc.com/upload/public/pdf/source/20240812/A9F992BBF5E33DA25A38A292376B91D5.pdf
- AP63205: https://www.diodes.com/assets/Datasheets/AP63200-AP63201-AP63203-AP63205.pdf
- SS14: https://www.vishay.com/docs/88746/ss12.pdf
- BAV99: https://www.onsemi.com/pdf/datasheet/bav99lt1-d.pdf
- ESP32-S3: https://www.espressif.com/sites/default/files/documentation/esp32-s3_datasheet_en.pdf
- ESP32-S3-WROOM-1: https://www.espressif.com/sites/default/files/documentation/esp32-s3-wroom-1_wroom-1u_datasheet_en.pdf
- DevKitC-1: https://dl.espressif.com/dl/schematics/SCH_ESP32-S3-DevKitC-1_V1.1_20220413.pdf
