# Fonts of the word clock

`src/wordfonts.h` holds bitmaps of the capitals the word clock uses (A C D E F G H I J K L N O R S T U V W Z),
made with `tools/wordfonts.py` from the fonts below. Only these bitmaps are in this repository, not the font
files. The change to each font is the same: its glyphs are rendered at their own pixel grid and stored as
bitmaps, other characters are left out.

| In the web interface | Font | Author | License | Source |
|---|---|---|---|---|
| Groot | 14px Pixel | a FontStruct user (no name given) | [CC BY-SA 3.0](https://creativecommons.org/licenses/by-sa/3.0/) | https://fontstruct.com/fontstructions/show/1029993 |
| Vet | derived from Ithaca | GGBotNet, https://ggbot.net | [SIL Open Font License 1.1](OFL-Ithaca.txt) | https://www.fontspace.com/ithaca-font-f144503 |
| Schreef | Pixelated Elegance | GGBotNet, https://ggbot.net | [CC0 1.0](https://creativecommons.org/publicdomain/zero/1.0/) | https://www.fontspace.com/pixelated-elegance-font-f126145 |
| Dun | Minecraft | unknown, published as public domain | public domain | https://www.fontspace.com/minecraft-font-f28180 |

What that means for the bitmaps in `src/wordfonts.h`:

- **14px Pixel** (`pixel14Glyphs`): an adaptation under the share-alike terms of CC BY-SA, licensed under
  CC BY-SA 4.0 like the rest of this repository.
- **Ithaca** (`bold9Glyphs`): a Modified Version under the SIL Open Font License 1.1, see `OFL-Ithaca.txt`:
  Copyright © 2025 GGBotNet, with Reserved Font Name "Ithaca". It stays under the OFL, not CC BY-SA, and as
  required for a Modified Version it does not carry the reserved name: it is called "Vet".
- **Pixelated Elegance** (`serif7Glyphs`) and **Minecraft** (`thin7Glyphs`): no conditions. The Minecraft
  font is a fan font; this project has nothing to do with Mojang or Minecraft.

To add or change a font: download it from its source, add it to `FONTS` in `tools/wordfonts.py` with its
license, run `python3 tools/wordfonts.py <folder with the font files>`, and add it to the table above.
