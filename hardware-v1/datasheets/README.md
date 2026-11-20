# Datasheets for controller v1

| File | Part | On the board | Source |
|---|---|---|---|
| PCA9685.pdf | NXP PCA9685, 16-channel PWM over I2C | 6 per board, outputs 0-7 to the ULN2803, 8-15 to the UDN2982 | NXP |
| ULN2803A.pdf | 8 Darlington sinks, 500 mA | 6 per board, lines to ground. Pin 10 (COM) is not connected on this board | TI, revision G (copy: TI has discontinued the part, its link is gone) |
| UDN2981-UDN2982.pdf | Allegro 8-channel source drivers, 500 mA | 6 per board, lines to 20 V | Allegro data sheet 29310F (copy: discontinued, the successor is the A2982) |
| ESP32.pdf | Espressif ESP32 chip | the controller, an ESP32 PICO board (PlatformIO `pico32`) | Espressif |
| ESP32-PICO.pdf | Espressif ESP32-PICO series | idem | Espressif |
| BAV99.pdf | dual diode in series | not on this board: one per dot in the panels, it steers the coil current | onsemi |

The 3.3 V comes from a KIS-3R33S buck module; there is no datasheet from its maker.

Sources, as downloaded in November 2026:

- PCA9685: already in this repository since 2019, revision 4 of the NXP datasheet; NXP has it at
  https://www.nxp.com/docs/en/data-sheet/PCA9685.pdf (their site refuses automatic downloads)
- ULN2803A: https://www.fdi.ucm.es/profesor/mendias/DAS-ise-spartan/docs/uln2803a.pdf (TI SLRS049G)
- UDN2981/2982: https://rcscomponents.kiev.ua/datasheets/datasheet-2982.pdf (Allegro 29310F)
- ESP32: https://www.espressif.com/sites/default/files/documentation/esp32_datasheet_en.pdf
- ESP32-PICO: https://www.espressif.com/sites/default/files/documentation/esp32-pico-d4_datasheet_en.pdf
- BAV99: https://www.onsemi.com/pdf/datasheet/bav99lt1-d.pdf
