#!/usr/bin/env python3
"""Makes src/wordfonts.h: the letters of the word clock from pixel fonts, as bitmaps.

    python3 tools/wordfonts.py <folder with the font files>

The font files are not in this repository, only the bitmaps made from them. Get them from the links
below. Every font is rendered large, its pixel grid is measured from the strokes, and each grid cell
is sampled in the middle: that gives the font at its own resolution, dot for dot.
"""

import os
import sys

from PIL import Image, ImageDraw, ImageFont

LETTERS = 'ACDEFGHIJKLNORSTUVWZ'  # All the word clock needs
BIG = 240

# id, name in the web interface, file, source and license
FONTS = [
    ('pixel14', 'Groot', '14px-pixel.otf',
     '"14px Pixel", https://fontstruct.com/fontstructions/show/1029993, CC BY-SA 3.0'),
    ('bold9', 'Vet', 'Ithaca-LVB75.ttf',
     'derived from "Ithaca" (c) 2025 GGBotNet, https://ggbot.net, SIL Open Font License 1.1'),
    ('serif7', 'Schreef', 'PixelatedEleganceRegular-ovawB.ttf',
     '"Pixelated Elegance" by GGBotNet, https://www.fontspace.com/pixelated-elegance-font-f126145, CC0'),
    ('thin7', 'Dun', 'MinecraftRegular-Bmg3.otf',
     '"Minecraft" font, https://www.fontspace.com/minecraft-font-f28180, public domain'),
]


def render(path, text):
    font = ImageFont.truetype(path, BIG)
    img = Image.new('1', (int(BIG * len(text) * 1.5) + 2 * BIG, BIG * 3), 0)
    draw = ImageDraw.Draw(img)
    draw.fontmode = '1'
    draw.text((BIG, BIG), text, font=font, fill=1)
    return img


def runs(img):
    px = img.load()
    left, top, right, bottom = img.getbbox()
    found = []
    for y in range(top, bottom):
        r = 1
        for x in range(left + 1, right):
            if px[x, y] == px[x - 1, y]:
                r += 1
            else:
                found.append(r)
                r = 1
    for x in range(left, right):
        r = 1
        for y in range(top + 1, bottom):
            if px[x, y] == px[x, y - 1]:
                r += 1
            else:
                found.append(r)
                r = 1
    return [r for r in found if r >= 3]


def gridstep(path):
    """The largest step that every run of dots or gaps is a whole number of."""
    found = runs(render(path, LETTERS))
    step = None
    for tenth in range(30, 1200):
        u = tenth / 10
        if sum(abs(r - max(1, round(r / u)) * u) for r in found) / len(found) < 0.6:
            step = u
    if not step:
        sys.exit(f'{path}: no pixel grid found')
    return step


def glyph(path, letter, step):
    img = render(path, letter)
    left, top, right, bottom = img.getbbox()
    px = img.load()
    cols, rows = round((right - left) / step), round((bottom - top) / step)
    cells = [[px[int(left + (c + 0.5) * step), int(top + (r + 0.5) * step)] for c in range(cols)] for r in range(rows)]
    return round((top - BIG) / step), cells


def main():
    folder = sys.argv[1]
    files = {name: os.path.join(root, name) for root, _, names in os.walk(folder) for name in names}
    out = ['#pragma once', '',
           '// Made by tools/wordfonts.py, do not edit. The letters of the word clock from pixel fonts, at their own',
           '// resolution: rows[] has the leftmost dot in the most significant used bit, like fonts.h.', '',
           '#include <stdint.h>', '',
           'struct BitmapGlyph {', '  char letter;', '  uint8_t width;', '  uint16_t rows[16];', '};', '',
           'struct BitmapFont {', '  const char *id;', '  const char *name;  // In the web interface',
           '  uint8_t height;', '  const BitmapGlyph *glyphs;', '};', '']
    table = []
    for ident, name, filename, source in FONTS:
        path = files[filename]
        step = gridstep(path)
        glyphs = {letter: glyph(path, letter, step) for letter in LETTERS}
        top = min(t for t, _ in glyphs.values())
        height = max(t + len(cells) for t, cells in glyphs.values()) - top
        assert height <= 16, f'{filename}: {height} rows'
        out.append(f'// {source}')
        out.append(f'const BitmapGlyph {ident}Glyphs[] = {{')
        for letter, (t, cells) in glyphs.items():
            width = len(cells[0])
            rows = [0] * height
            for r, row in enumerate(cells):
                rows[t - top + r] = sum(bit << (width - 1 - c) for c, bit in enumerate(row))
            out.append(f"  {{ '{letter}', {width}, {{ {', '.join(f'0x{v:04X}' for v in rows)} }} }},")
        out.append('  { 0, 0, { 0 } },')
        out.append('};')
        out.append('')
        table.append(f'  {{ "{ident}", "{name}", {height}, {ident}Glyphs }},')
    out.append(f'#define BITMAP_FONTS {len(FONTS)}')
    out.append('const BitmapFont bitmapFonts[BITMAP_FONTS] = {')
    out += table
    out.append('};')
    target = os.path.join(os.path.dirname(os.path.abspath(__file__)), '..', 'src', 'wordfonts.h')
    with open(target, 'w') as f:
        f.write('\n'.join(out) + '\n')
    print('Klaar:', os.path.normpath(target))


if __name__ == '__main__':
    main()
