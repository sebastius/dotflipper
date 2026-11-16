#!/usr/bin/env python3
"""Makes the dotflipper logo: the word DOTFLIPPER on a flip dot panel.

    python3 docs/logo.py

Writes docs/logo.svg for the repository and hardware-v3/v3.pretty/Logo_Dotflipper.kicad_mod, the same
dots on the silkscreen: a large dot is a disc flipped to yellow, a small dot a disc that shows black.
"""

import os

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.dirname(HERE)
WORD = 'DOTFLIPPER'

FONT = {  # 5 x 7, one string per row
    'D': ['11110', '10001', '10001', '10001', '10001', '10001', '11110'],
    'O': ['01110', '10001', '10001', '10001', '10001', '10001', '01110'],
    'T': ['11111', '00100', '00100', '00100', '00100', '00100', '00100'],
    'F': ['11111', '10000', '10000', '11110', '10000', '10000', '10000'],
    'L': ['10000', '10000', '10000', '10000', '10000', '10000', '11111'],
    'I': ['01110', '00100', '00100', '00100', '00100', '00100', '01110'],
    'P': ['11110', '10001', '10001', '11110', '10000', '10000', '10000'],
    'E': ['11111', '10000', '10000', '11110', '10000', '10000', '11111'],
    'R': ['11110', '10001', '10001', '11110', '10100', '10010', '10001'],
}


def grid(lines=(WORD,)):
    """Rows of booleans: each line of text centred, one column between letters, one empty row between
    lines and a border of one dark disc all round."""
    width = max(len(line) for line in lines) * 6 - 1
    rows = [[False] * (width + 2)]
    for n, line in enumerate(lines):
        if n:
            rows.append([False] * (width + 2))
        pad = (width - (len(line) * 6 - 1)) // 2
        for r in range(7):
            row = [False] * (1 + pad)
            for i, letter in enumerate(line):
                row += [c == '1' for c in FONT[letter][r]]
                if i < len(line) - 1:
                    row.append(False)
            rows.append(row + [False] * (width + 2 - len(row)))
    return rows + [[False] * (width + 2)]


def svg():
    rows = grid()
    pitch, radius, margin = 14, 5.8, 18
    width = len(rows[0]) * pitch + 2 * margin
    height = len(rows) * pitch + 2 * margin
    flipping = {(3, len(rows[0]) - 1), (6, len(rows[0]) - 1)}  # Two discs in the right border, caught halfway
    out = [f'<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 {width} {height + 30}" width="{width}" height="{height + 30}" '
           'role="img" aria-label="dotflipper">',
           '<title>dotflipper</title>',
           '<defs>',
           '<radialGradient id="on" cx="38%" cy="32%" r="75%"><stop offset="0" stop-color="#fff3a0"/>'
           '<stop offset="0.45" stop-color="#f7c600"/><stop offset="1" stop-color="#b98a00"/></radialGradient>',
           '<radialGradient id="off" cx="38%" cy="32%" r="75%"><stop offset="0" stop-color="#3a3a3a"/>'
           '<stop offset="1" stop-color="#141414"/></radialGradient>',
           '<linearGradient id="panel" x1="0" y1="0" x2="0" y2="1"><stop offset="0" stop-color="#262626"/>'
           '<stop offset="1" stop-color="#121212"/></linearGradient>',
           '</defs>',
           f'<rect x="1" y="1" width="{width - 2}" height="{height - 2}" rx="10" fill="url(#panel)" stroke="#000" stroke-width="2"/>',
           f'<rect x="7" y="7" width="{width - 14}" height="{height - 14}" rx="6" fill="none" stroke="#333" stroke-width="1"/>']
    for r, row in enumerate(rows):
        for c, on in enumerate(row):
            x, y = margin + (c + 0.5) * pitch, margin + (r + 0.5) * pitch
            if (r, c) in flipping:
                # Seen edge on, halfway through its flip: yellow on one half, black on the other
                out.append(f'<ellipse cx="{x:.1f}" cy="{y:.1f}" rx="{radius * 0.35:.1f}" ry="{radius:.1f}" fill="url(#off)"/>')
                out.append(f'<path d="M{x:.1f} {y - radius:.1f} A{radius * 0.35:.1f} {radius:.1f} 0 0 1 {x:.1f} {y + radius:.1f} Z" fill="url(#on)"/>')
                continue
            out.append(f'<circle cx="{x:.1f}" cy="{y:.1f}" r="{radius}" fill="url(#{"on" if on else "off"})"/>')
            # The axle of the disc, just visible
            out.append(f'<line x1="{x - radius:.1f}" y1="{y:.1f}" x2="{x + radius:.1f}" y2="{y:.1f}" stroke="#000" '
                       f'stroke-opacity="{0.18 if on else 0.35}" stroke-width="0.8"/>')
    out.append(f'<text x="{width / 2:.0f}" y="{height + 22}" text-anchor="middle" font-family="DejaVu Sans Mono, Menlo, Consolas, monospace" '
               'font-size="15" fill="#8a8a8a" letter-spacing="2">flip dot controller</text>')
    out.append('</svg>')
    return '\n'.join(out) + '\n'


def footprint():
    """Silkscreen version in two lines, DOT over FLIPPER, at 0.6 mm pitch: about 27 x 13 mm with the
    text under it."""
    rows = grid(('DOT', 'FLIPPER'))
    pitch, radius, stroke = 0.6, 0.16, 0.15
    cols = len(rows[0])
    x0, y0 = -(cols - 1) * pitch / 2, -(len(rows) - 1) * pitch / 2
    out = ['(footprint "Logo_Dotflipper" (version 20241229) (generator "pcbnew") (generator_version "10.0") (layer "F.Cu")',
           '(descr "dotflipper logo: DOTFLIPPER on a flip dot panel, silkscreen only")',
           '(property "Reference" "LOGO1" (at 0 -4 0) (layer "F.Fab") (hide yes) (effects (font (size 1 1) (thickness 0.15))))',
           '(property "Value" "Logo_Dotflipper" (at 0 5 0) (layer "F.Fab") (hide yes) (effects (font (size 1 1) (thickness 0.15))))',
           '(attr board_only exclude_from_pos_files exclude_from_bom)']
    for r, row in enumerate(rows):
        for c, on in enumerate(row):
            x, y = x0 + c * pitch, y0 + r * pitch
            r_dot = radius if on else 0.03  # A disc showing black: a small dot, so the letters stand out
            out.append(f'(fp_circle (center {x:.3f} {y:.3f}) (end {x + r_dot:.3f} {y:.3f}) (stroke (width {stroke}) (type solid)) '
                       f'(fill yes) (layer "F.SilkS"))')
    half_w, half_h = (cols - 1) * pitch / 2 + 0.55, (len(rows) - 1) * pitch / 2 + 0.55
    out.append(f'(fp_rect (start {-half_w:.3f} {-half_h:.3f}) (end {half_w:.3f} {half_h:.3f}) (stroke (width 0.15) (type solid)) '
               '(fill no) (layer "F.SilkS"))')
    out.append(f'(fp_text user "flip dot controller v3" (at 0 {half_h + 1.1:.3f} 0) (layer "F.SilkS") '
               '(effects (font (size 1 1) (thickness 0.15))))')
    out.append(f'(fp_rect (start {-half_w - 0.25:.3f} {-half_h - 0.25:.3f}) (end {half_w + 0.25:.3f} {half_h + 1.95:.3f}) '
               '(stroke (width 0.05) (type solid)) (fill no) (layer "F.CrtYd"))')
    return '\n'.join(out) + '\n)\n'


if __name__ == '__main__':
    with open(os.path.join(HERE, 'logo.svg'), 'w') as f:
        f.write(svg())
    with open(os.path.join(ROOT, 'hardware-v3', 'v3.pretty', 'Logo_Dotflipper.kicad_mod'), 'w') as f:
        f.write(footprint())
    print('Klaar: docs/logo.svg, hardware-v3/v3.pretty/Logo_Dotflipper.kicad_mod')
