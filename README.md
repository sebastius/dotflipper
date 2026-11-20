# dotflipper

![dotflipper](docs/logo.svg)

My Flip Dot controller.

## Status (November 2026)

- **v1** (`hardware-v1`): the original controller, based on the PCA9685 i2c port expanders. Tested and
  working, but dangerous and slow.
- **v3** (`hardware-v3`): a new controller with 74HC595 shift registers, where the hardware keeps plus and
  minus of a wire apart. Designed and on order, not tested yet.
- **Software** (`software`): firmware for v1 and v3, being developed with the help of AI. Try the v3
  firmware in your browser: https://sebastius.github.io/dotflipper/
- **Original software** (`software-original`): the hand-written firmware from 2018-2020, heavily outdated.

## License

CC BY-SA 4.0, except the word clock font derived from Ithaca, which stays under the SIL Open Font License.
Fonts, authors and licenses: [software/tools/fonts](software/tools/fonts/README.md).

lol
