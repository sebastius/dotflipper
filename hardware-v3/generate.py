#!/usr/bin/env python3
"""Dotflipper v3: one panel on one board with 74HC595 shift registers, ULN2803A and XL62783, all SMD.

    python3 generate.py

Makes the schematic (root sheet plus drawn sheets groep_x1, groep_rij and besturing), the library for
the ESP32-S3-DevKitC-1 and the placed, unrouted board. An existing board is only brought up to date, its placement
and tracks stay; python3 generate.py --opnieuw throws the board away and places everything again.

The XL62783 (Xinluda) is a copy of the Toshiba TBD62783A, a DMOS source driver: same pinout and package,
so the TBD62783AFWG (C146353) fits the same footprint if the clone disappoints.

Driving: six groups of eight lines. Each group has an XL62783 (outputs to 20 V), a ULN2803A (outputs
to ground) and two 74HC595s side by side on the same data, one per driver. All logic is plain HC: every logic
input gets 5 V CMOS levels, only the 74AHCT541 with its TTL inputs sees the 3.3 V of the ESP32. The drivers always have
power and ground. DIR_X1 and DIR_ROW pick per kind of group which of the two 595s may switch its outputs
on (through their OE): plus and minus of one line or of one panel row can therefore never be on
together, also not after a software bug. Change DIR only while nOE is high.

    Chain: MOSI > G1 X1 1-8 > G2 X1 9-16 > G3 X1 17-24 > G4 X1 25-32 > G5 X3 rows 0-7 > G6 X2 rows 8-15
    In every group QA is the lowest pin or row: bit n of a group is line or row n of that group.
"""

import os
import re
import subprocess
import sys
import tempfile

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
import schematic  # noqa: E402  The schematic helpers

import pcbnew  # noqa: E402

NAME = 'v3'
WIDTH, HEIGHT = 226, 80
MM = pcbnew.FromMM

R0603 = 'Resistor_SMD:R_0603_1608Metric'
C0603 = 'Capacitor_SMD:C_0603_1608Metric'
C1206 = 'Capacitor_SMD:C_1206_3216Metric'
SO16 = 'Package_SO:SOIC-16_3.9x9.9mm_P1.27mm'
SO14 = 'Package_SO:SOIC-14_3.9x8.7mm_P1.27mm'
ARRAY = 'Resistor_SMD:R_Array_Convex_4x0603'
SO18W = 'Package_SO:SOIC-18W_7.5x11.6mm_P1.27mm'
IDC = 'Connector_IDC:IDC-Header_2x%02d_P2.54mm_Vertical'
TERMINAL = 'TerminalBlock_Phoenix:TerminalBlock_Phoenix_MKDS-1,5-2-5.08_1x02_P5.08mm_Horizontal'

GROUPS = [  # name, kind, header, first pin or row
    ('G1', 'X1', 'X1', 1), ('G2', 'X1', 'X1', 9), ('G3', 'X1', 'X1', 17), ('G4', 'X1', 'X1', 25),
    ('G5', 'ROW', 'X3', 0), ('G6', 'ROW', 'X2', 0),
]

# ESP32-S3-DevKitC-1 v1.1, header J1 (pads 1-22) and J3 (pads 23-44), from the Espressif user guide
J1 = ['3V3', '3V3', 'RST', 'IO4', 'IO5', 'IO6', 'IO7', 'IO15', 'IO16', 'IO17', 'IO18', 'IO8', 'IO3', 'IO46',
      'IO9', 'IO10', 'IO11', 'IO12', 'IO13', 'IO14', '5V', 'GND']
J3 = ['GND', 'TX', 'RX', 'IO1', 'IO2', 'IO42', 'IO41', 'IO40', 'IO39', 'IO38', 'IO37', 'IO36', 'IO35', 'IO0',
      'IO45', 'IO48', 'IO47', 'IO21', 'IO20', 'IO19', 'GND', 'GND']
# FSPI pins on J1 15-20 next to each other. No strapping, USB, UART or PSRAM pins
ESP = {'IO9': 'nOE_IN', 'IO10': 'LATCH_IN', 'IO11': 'DATA_IN', 'IO12': 'SCLK_IN', 'IO13': 'MISO', 'IO14': 'DIR_X1_IN',
       'IO8': 'DIR_ROW_IN', '5V': 'ESP_5V', 'GND': 'GND'}

LCSC = {}  # Filled per reference in build(): only the SMD parts, JLCPCB can place those
PARTS = {'100nF': 'C14663', '10uF 50V': 'C13585', '22uF 25V': 'C12891', '470uF 35V': 'C46550413',
         '10k': 'C25804', '20k': 'C4184', '100k': 'C25803', '4x10k': 'C29718', 'SS14': 'C2480', '3A': 'C182445',
         '74HC595': 'C5947', '74HC32': 'C5632', '74HC14': 'C5605', 'XL62783': 'C556260', 'ULN2803A': 'C845537',
         '74AHCT541': 'C2682058', 'AP63205WU': 'C2071056', '4.7uH': 'C57269', '74LVC1G79': 'C42878', '4x1k': 'C20197'}
SO20W = 'Package_SO:SOIC-20W_7.5x12.8mm_P1.27mm'
SOT235 = 'Package_TO_SOT_SMD:SOT-23-5'


def esp_symbol():
    pins = ''
    for i, name in enumerate(J1):
        pins += (f'(pin bidirectional line (at -17.78 {26.67 - 2.54 * i:.2f} 0) (length 2.54) (name "{name}" '
                 f'(effects (font (size 1.27 1.27)))) (number "{i + 1}" (effects (font (size 1.27 1.27)))))\n')
    for i, name in enumerate(J3):
        pins += (f'(pin bidirectional line (at 17.78 {26.67 - 2.54 * i:.2f} 180) (length 2.54) (name "{name}" '
                 f'(effects (font (size 1.27 1.27)))) (number "{i + 23}" (effects (font (size 1.27 1.27)))))\n')
    for kind, names in (('power_in', ('5V',)), ('passive', ('GND',))):
        pins = re.sub(r'\(pin bidirectional (line \(at [^)]*\) \(length 2.54\) \(name "(%s)")' % '|'.join(names),
                      rf'(pin {kind} \1', pins)
    prop = lambda name, value, y, hide='': f'(property "{name}" "{value}" (at 0 {y} 0) (effects (font (size 1.27 1.27)){hide}))\n'
    raw = ('(symbol "ESP32-S3-DevKitC-1" (pin_names (offset 1.016)) (exclude_from_sim no) (in_bom yes) (on_board yes)\n'
           + prop('Reference', 'A', 30.48) + prop('Value', 'ESP32-S3-DevKitC-1', -30.48)
           + prop('Footprint', f'{NAME}:ESP32-S3-DevKitC-1', 0, ' (hide yes)')
           + prop('Datasheet', 'https://docs.espressif.com/projects/esp-dev-kits/en/latest/esp32s3/esp32-s3-devkitc-1/user_guide_v1.1.html', 0, ' (hide yes)')
           + prop('Description', 'ESP32-S3-DevKitC-1 v1.1 on two 22 way headers', 0, ' (hide yes)')
           + '(symbol "ESP32-S3-DevKitC-1_0_1" (rectangle (start -15.24 29.21) (end 15.24 -29.21) '
             '(stroke (width 0.254) (type default)) (fill (type background))))\n'
           + '(symbol "ESP32-S3-DevKitC-1_1_1"\n' + pins + ')\n)')
    return schematic.Symbol(NAME, 'ESP32-S3-DevKitC-1', raw), raw


def esp_footprint():
    """Two 22 way rows, 22.86 mm apart, pin 1 of J1 top left with the antenna above it.

    From the Espressif dimension drawing: 25.40 mm wide, pins 1.27 mm from the long edges, the board
    ends 1.40 mm above pin 1 and 8.00 mm below pin 22 (the USB connectors). The antenna sticks out at the
    pin 1 end, so place the module with that end at the edge of the board.
    """
    out = [f'(footprint "ESP32-S3-DevKitC-1" (version 20241229) (generator "pcbnew") (generator_version "10.0") (layer "F.Cu")',
           '(descr "ESP32-S3-DevKitC-1 v1.1, through hole, on 1x22 sockets. Antenna at the pin 1 end")',
           '(property "Reference" "REF**" (at 11.43 -3 0) (layer "F.SilkS") (effects (font (size 1 1) (thickness 0.15))))',
           '(property "Value" "ESP32-S3-DevKitC-1" (at 11.43 64 0) (layer "F.Fab") (effects (font (size 1 1) (thickness 0.15))))',
           '(attr through_hole)']
    def rect(layer, left, top, right, bottom, width):
        return (f'(fp_rect (start {left} {top}) (end {right} {bottom}) (stroke (width {width}) (type solid)) '
                f'(fill no) (layer "{layer}"))')
    out.append(rect('F.Fab', -1.27, -1.40, 24.13, 61.34, 0.1))
    out.append(rect('F.SilkS', -1.39, -1.52, 24.25, 61.46, 0.12))
    out.append(rect('F.CrtYd', -1.65, -1.65, 24.4, 61.6, 0.05))
    out.append('(fp_text user "antenne" (at 11.43 1 0) (layer "F.Fab") (effects (font (size 1 1) (thickness 0.15))))')
    out.append('(fp_text user "USB" (at 11.43 59.5 0) (layer "F.Fab") (effects (font (size 1 1) (thickness 0.15))))')
    for i in range(22):
        for number, x in ((i + 1, 0), (i + 23, 22.86)):
            shape = 'rect' if i == 0 else 'circle'
            out.append(f'(pad "{number}" thru_hole {shape} (at {x} {2.54 * i:.2f}) (size 1.7 1.7) (drill 1) (layers "*.Cu" "*.Mask"))')
    out.append(f'(model "${{KICAD10_3DMODEL_DIR}}/Connector_PinSocket_2.54mm.3dshapes/PinSocket_1x22_P2.54mm_Vertical.step" '
               '(offset (xyz 0 0 0)) (scale (xyz 1 1 1)) (rotate (xyz 0 0 0)))')
    out.append(f'(model "${{KICAD10_3DMODEL_DIR}}/Connector_PinSocket_2.54mm.3dshapes/PinSocket_1x22_P2.54mm_Vertical.step" '
               '(offset (xyz 22.86 0 0)) (scale (xyz 1 1 1)) (rotate (xyz 0 0 0)))')
    return '\n'.join(out) + '\n)\n'


DRAWN = []


class Drawn:
    """A sheet drawn with real wires, used once or several times with other references per use."""

    def __init__(self, file, title, instances, root, version):
        self.file, self.title, self.instances, self.root, self.version = file, title, instances, root, version
        self.path = {i['name']: f'/{root}/{schematic.uid("blok", i["name"])}' for i in instances}
        self.symbols, self.body, self.count = {}, [], 0
        self.base = 9000 + 1000 * len(DRAWN)  # Power symbols need numbers unique over all sheets
        DRAWN.append(file)
        self.power_symbols = {n: schematic.load_symbol('power', n) for n in ('GND', '+5V')}

    def symbol(self, sym, role, value, x, y, footprint, unit=1, mirror=False):
        self.symbols[sym.id] = sym
        identifier = schematic.uid(self.file, role, unit)
        code = PARTS.get(value)
        refs = {i['name']: i['refs'][role] for i in self.instances}
        if code:
            LCSC.update({ref: code for ref in refs.values()})
        hidden = '(effects (font (size 1.27 1.27)) (hide yes))'
        first = next(iter(refs.values()))
        flags = '(in_bom yes) (on_board yes)' if code else '(in_bom no) (on_board yes) (in_pos_files no)'
        numbers = dict.fromkeys(p['number'] for p in sym.pins if p['unit'] in (0, unit))
        field = f'(property "LCSC" "{code}" (at {x:.2f} {y:.2f} 0) {hidden})\n' if code else ''
        instances = ''.join(f'(path "{self.path[name]}" (reference "{ref}") (unit {unit}))' for name, ref in refs.items())
        self.body.append(f'(symbol (lib_id "{sym.id}") (at {x:.2f} {y:.2f} 0){" (mirror x)" if mirror else ""} (unit {unit}) '
                         f'(exclude_from_sim no) {flags} (dnp no) (uuid "{identifier}")\n'
                         f'(property "Reference" "{first}" (at {x + 2.54:.2f} {y - 2.54:.2f} 0) (effects (font (size 1.27 1.27)) (justify left)))\n'
                         f'(property "Value" "{value}" (at {x + 2.54:.2f} {y + 2.54:.2f} 0) (effects (font (size 1.27 1.27)) (justify left)))\n'
                         f'(property "Footprint" "{footprint}" (at {x:.2f} {y:.2f} 0) {hidden})\n'
                         f'(property "Datasheet" "" (at {x:.2f} {y:.2f} 0) {hidden})\n' + field
                         + ''.join(f'(pin "{n}" (uuid "{schematic.uid(identifier, n)}"))' for n in numbers)
                         + f'\n(instances (project "{NAME}" {instances})))')

        def tip(name):
            pin = next(p for p in sym.pins if p['unit'] in (0, unit) and name in (p['number'], p['name']))
            return (round(x + pin['x'], 2), round(y + pin['y'] if mirror else y - pin['y'], 2))
        return tip

    def power(self, name, x, y, rotation=0):
        self.count += 1
        identifier = schematic.uid(self.file, 'pwr', self.count)
        hidden = '(effects (font (size 1.27 1.27)) (hide yes))'
        instances = ''.join(f'(path "{self.path[i["name"]]}" (reference "#PWR{self.base + 100 * n + self.count}") (unit 1))'
                            for n, i in enumerate(self.instances))
        self.body.append(f'(symbol (lib_id "power:{name}") (at {x:.2f} {y:.2f} {rotation}) (unit 1) (exclude_from_sim no) (in_bom yes) '
                         f'(on_board yes) (dnp no) (uuid "{identifier}")\n(property "Reference" "#PWR" (at {x:.2f} {y:.2f} 0) {hidden})\n'
                         f'(property "Value" "{name}" (at {x:.2f} {y:.2f} 0) {hidden})\n(property "Footprint" "" (at {x:.2f} {y:.2f} 0) {hidden})\n'
                         f'(property "Datasheet" "" (at {x:.2f} {y:.2f} 0) {hidden})\n(pin "1" (uuid "{schematic.uid(identifier, "1")}"))\n'
                         f'(instances (project "{NAME}" {instances})))')

    def wire(self, *points):
        for (x1, y1), (x2, y2) in zip(points, points[1:]):
            self.body.append(f'(wire (pts (xy {x1:.2f} {y1:.2f}) (xy {x2:.2f} {y2:.2f})) (stroke (width 0) (type default)) '
                             f'(uuid "{schematic.uid(self.file, "wire", x1, y1, x2, y2)}"))')

    def junction(self, x, y):
        self.body.append(f'(junction (at {x:.2f} {y:.2f}) (diameter 0) (color 0 0 0 0) (uuid "{schematic.uid(self.file, "junction", x, y)}"))')

    def rail(self, x, ys):
        """A vertical wire through the given heights, with a dot at every height between the ends."""
        ys = sorted(set(ys))
        self.wire(*[(x, y) for y in ys])
        for y in ys[1:-1]:
            self.junction(x, y)

    def port(self, name, x, y, side, again=False):
        angle, justify, shape = {'left': (180, 'right', 'input'), 'right': (0, 'left', 'output'), 'down': (270, 'right', 'output')}[side]
        key = (name, x, y) if again else (name,)
        self.body.append(f'(hierarchical_label "{name}" (shape {shape}) (at {x:.2f} {y:.2f} {angle}) '
                         f'(effects (font (size 1.27 1.27)) (justify {justify})) (uuid "{schematic.uid(self.file, "port", *key)}"))')

    def label(self, name, x, y, again=False):
        key = (name, x, y) if again else (name,)
        self.body.append(f'(label "{name}" (at {x:.2f} {y:.2f} 0) (effects (font (size 1.27 1.27)) (justify left bottom)) '
                         f'(uuid "{schematic.uid(self.file, "label", *key)}"))')

    def vm(self, x, y, angle):
        justify = 'right' if angle in (180, 270) else 'left'
        self.body.append(f'(global_label "VM" (shape input) (at {x:.2f} {y:.2f} {angle}) (effects (font (size 1.27 1.27)) (justify {justify})) '
                         f'(uuid "{schematic.uid(self.file, "vm", x, y)}"))')

    def no_connect(self, x, y):
        self.body.append(f'(no_connect (at {x:.2f} {y:.2f}) (uuid "{schematic.uid(self.file, "nc", x, y)}"))')

    def text(self, words, x, y):
        self.body.append(f'(text "{words}" (at {x:.2f} {y:.2f} 0) (effects (font (size 2 2)) (justify left bottom)) '
                         f'(uuid "{schematic.uid(self.file, "text", words)}"))')

    def write(self):
        out = [f'(kicad_sch (version {self.version}) (generator "dotflipper3") (generator_version "10.0")',
               f'(uuid "{schematic.uid("sheet", self.file)}")', '(paper "A3")',
               f'(title_block (title "{self.title}") (company "Dotflipper v3"))', '(lib_symbols']
        out += [s.raw for s in self.symbols.values()] + [s.raw for s in self.power_symbols.values()] + [')'] + self.body + [')']
        open(os.path.join(HERE, self.file + '.kicad_sch'), 'w').write('\n'.join(out))


def group_refs(g):
    u = 4 * g
    return {'SRH': f'U{u + 1}', 'SRL': f'U{u + 2}', 'UDN': f'U{u + 3}', 'ULN': f'U{u + 4}', 'CH': f'C{2 * g + 1}', 'CL': f'C{2 * g + 2}',
            'RN1': f'RN{2 * g + 1}', 'RN2': f'RN{2 * g + 2}'}


def group_sheet(kind, instances, root, version):
    """Two 595s, each feeding one driver through straight wires. Bit QA is channel 8, so the drivers are
    drawn mirrored with channel 8 on top. The inputs of the XL62783 get a pull-down, in two packs: its 595 is
    off most of the time. The ULN needs none, its inputs have 2.7k and 7.2k + 3k to ground inside."""
    d = Drawn('groep_x1' if kind == 'X1' else 'groep_rij', 'Groep X1 (8 lijnen)' if kind == 'X1' else 'Groep rijen (8 rijen)', instances, root, version)
    load = schematic.load_symbol
    sr, udn, uln = load('74xx', '74HC595'), load('Transistor_Array', 'TBD62783A'), load('Transistor_Array', 'ULN2803A')
    pack, cap = load('Device', 'R_Pack04'), load('Device', 'C')
    letters = 'ABCDEFGH'
    x_sr, x_drv, y_h, y_l = 76.2, 152.4, 63.5, 165.1
    rails = {'SER': 50.8, 'SRCLK': 55.88, 'RCLK': 60.96}
    ends, drivers = {}, {}
    for side, y, role, drv, y_drv, packs in (('H', y_h, 'SRH', udn, y_h, ('RN1', 'RN2')), ('L', y_l, 'SRL', uln, y_l + 2.54, ())):
        chip = d.symbol(sr, role, '74HC595', x_sr, y, SO16)
        driver = d.symbol(drv, 'UDN' if side == 'H' else 'ULN', 'XL62783' if side == 'H' else 'ULN2803A', x_drv, y_drv, SO18W, mirror=True)
        d.text('595 voor de XL62783 (naar 20 V)' if side == 'H' else '595 voor de ULN (naar massa)', x_sr - 25.4, y - 25.4)
        d.power('+5V', x_sr, y - 15.24)
        d.power('GND', x_sr, y + 17.78)
        d.power('+5V', *chip('~{SRCLR}'), 90)
        for name, rail in rails.items():
            pin = chip(name)
            ends.setdefault(name, []).append(pin)
            if side == 'H':
                d.wire((30.48, pin[1]), (rail, pin[1]))
                d.port(name, 30.48, pin[1], 'left')
            d.wire((rail, pin[1]), pin)
        oe = chip('~{OE}')
        d.wire((30.48, oe[1]), oe)
        d.port(f'nOE_{side}', 30.48, oe[1], 'left')
        # Wires from bit to driver, with the channel number as the net name
        taps = {}
        for i, letter in enumerate(letters):
            q = chip(f'Q{letter}')
            channel = 8 - i
            inp = driver(f'I{channel}')
            assert q[1] == inp[1], (letter, q, inp)
            tap = x_sr + 22.86 + i * 2.54 + (0 if i < 4 else 10.16)
            if packs:
                d.wire(q, (tap, q[1]), inp)
                d.junction(tap, q[1])
            else:
                d.wire(q, inp)
            d.label(f'{side}{channel}', x_sr + 11.43, q[1])
            taps[i] = (tap, q[1])
        y_pack = y + 25.4
        for p, role_pack in enumerate(packs):
            x_pack = x_sr + 27.94 + 20.32 * p
            tip = d.symbol(pack, role_pack, '4x10k', x_pack, y_pack, ARRAY, mirror=True)
            for n in range(4):
                top = tip(str(n + 1))
                d.wire(taps[4 * p + n], top)
            bottoms = [tip(str(8 - n)) for n in range(4)]
            d.wire(*bottoms)
            d.power('GND', *bottoms[1])
        c = d.symbol(cap, 'CH' if side == 'H' else 'CL', '100nF', 40.64, y + 33.02, C0603)
        d.power('+5V', *c('1'))
        d.power('GND', *c('2'))
        d.vm(x_drv, y_drv + 12.7, 270) if side == 'H' else d.vm(x_drv + 10.16, y_drv + 7.62, 0)
        d.power('GND', x_drv, y_drv - (15.24 if side == 'H' else 17.78), 180)
        if side == 'H':
            qh = chip("QH'")
            d.wire(qh, (91.44, qh[1]), (91.44, 104.14))
            d.port('QH', 91.44, 104.14, 'down')
        else:
            d.no_connect(*chip("QH'"))
        drivers[side] = driver
    for name, rail in rails.items():
        d.rail(rail, [p[1] for p in ends[name]])
        d.junction(rail, ends[name][0][1])
    # Outputs
    high, low = drivers['H'], drivers['L']
    for i in range(8):
        channel = 8 - i
        a, b = high(f'O{channel}'), low(f'O{channel}')
        if kind == 'X1':
            x = 175.26 + i * 2.54
            d.wire(a, (x, a[1]), (x, b[1]), b)
            d.wire((x, a[1]), (210.82, a[1]))
            d.junction(x, a[1])
            d.port(f'OUT{channel}', 210.82, a[1], 'right')
        else:
            d.wire(a, (190.5, a[1]))
            d.wire(b, (190.5, b[1]))
            d.port(f'PLUS{channel}', 190.5, a[1], 'right')
            d.port(f'MIN{channel}', 190.5, b[1], 'right')
    d.write()
    return d.file


def control_sheet(root, version):
    """Per kind of group: the 595s for the UDNs only with DIR high, those for the ULNs only with DIR low."""
    refs = {'U25': 'U25', 'U26': 'U26', 'C13': 'C13', 'C14': 'C14'}
    d = Drawn('besturing', 'Besturing: plus en min nooit samen', [{'name': 'Besturing', 'refs': refs}], root, version)
    load = schematic.load_symbol
    gate, inverter, res, cap = load('74xx', '74LS32'), load('74xx', '74HC14'), load('Device', 'R'), load('Device', 'C')
    d.text('Uitgangen van de 595 voor een kant mogen alleen aan als nOE laag is en DIR die kant kiest.', 30.48, 25.4)
    d.text('Na reset: nOE hoog en DIR laag door R1-R3 op het blad Doorlus. Wissel DIR alleen terwijl nOE hoog is.', 30.48, 30.48)
    nOE_x, dir_x, gate_x, inv_x = 45.72, 55.88, 114.3, 76.2
    taps = []
    # (gate units), (inverter unit, pins)
    blocks = ((0, 'X1', (1, 2), 1), (63.5, 'ROW', (3, 4), 2))
    pins = {1: ('1', '2', '3'), 2: ('4', '5', '6'), 3: ('9', '10', '8'), 4: ('12', '13', '11')}
    inv_pins = {1: ('1', '2'), 2: ('3', '4')}
    for offset, kind, units, inv_unit in blocks:
        y_h, y_l = 60.96 + offset, 91.44 + offset
        inv_y = 76.2 + offset
        inv = d.symbol(inverter, 'U26', '74HC14', inv_x, inv_y, SO14, unit=inv_unit)
        gh, gl = (d.symbol(gate, 'U25', '74HC32', gate_x, y, SO14, unit=u) for y, u in ((y_h, units[0]), (y_l, units[1])))
        a, y_out = inv_pins[inv_unit]
        d.wire((30.48, inv_y), (dir_x, inv_y), inv(a))
        d.port(f'DIR_{kind}', 30.48, inv_y, 'left')
        d.junction(dir_x, inv_y)
        low = gl(pins[units[1]][1])
        d.wire(inv(y_out), (99.06, inv_y), (99.06, gh(pins[units[0]][1])[1]), gh(pins[units[0]][1]))
        d.wire((dir_x, low[1]), low)
        d.rail(dir_x, [inv_y, low[1]])
        for u, g, side in ((units[0], gh, 'H'), (units[1], gl, 'L')):
            first = g(pins[u][0])
            d.wire((nOE_x, first[1]), first)
            taps.append(first[1])
            out = g(pins[u][2])
            d.wire(out, (134.62, out[1]))
            d.port(f'nOE_{side}_{kind}', 134.62, out[1], 'right')
    # nOE: the port, then the four gates
    d.wire((30.48, 50.8), (nOE_x, 50.8))
    d.port('nOE', 30.48, 50.8, 'left')
    d.rail(nOE_x, [50.8] + taps)
    # Spare inverters: input to ground, output open (inverter 3 clocks the flip-flop on the sheet Doorlus).
    # Supply units and decoupling
    for u, (a, y_out) in zip((4, 5, 6), (('9', '8'), ('11', '10'), ('13', '12'))):
        y = 60.96 + (u - 4) * 15.24
        spare = d.symbol(inverter, 'U26', '74HC14', 200.66, y, SO14, unit=u)
        d.power('GND', *spare(a))
        d.no_connect(*spare(y_out))
    for ref, sym, unit, x, c_ref, x_cap in (('U25', gate, 5, 182.88, 'C13', 165.1), ('U26', inverter, 7, 226.06, 'C14', 208.28)):
        value = '74HC32' if ref == 'U25' else '74HC14'
        supply = d.symbol(sym, ref, value, x, 165.1, SO14, unit=unit)
        d.power('+5V', *supply('VCC'))
        d.power('GND', *supply('GND'))
        c = d.symbol(cap, c_ref, '100nF', x_cap, 165.1, C0603)
        d.power('+5V', *c('1'))
        d.power('GND', *c('2'))
    d.write()
    return d.file


def link_sheet(root, version):
    """Every board buffers the link signals: each cable runs from one buffer to the next, at most about 50 cm.

    The 74AHCT541 drives the signals on this board and, through 1k in series, the cable to the next
    board. Because SCLK reaches the next board later than here, the chain data leaves through a flip-flop
    on the falling edge of SCLK: the next board samples it on the rising edge, half a clock later. The chain
    returns through every board to the ESP32, also buffered; JP1 closes it on the last board.
    """
    refs = {r: r for r in ('U28', 'U29', 'U26', 'RN13', 'RN14', 'RN15', 'R1', 'R2', 'R3', 'C18', 'C19', 'JP1')}
    d = Drawn('doorlus', 'Doorlus: buffer naar het volgende bord', [{'name': 'Doorlus', 'refs': refs}], root, version)
    load = schematic.load_symbol
    buffer, flop, inverter = load('74xx', '74AHCT541'), load('74xGxx', '74LVC1G79'), load('74xx', '74HC14')
    pack, res, cap = load('Device', 'R_Pack04'), load('Device', 'R'), load('Device', 'C')
    d.text('Binnen van het vorige bord (J3) of van de ESP32 (bord 1). Lokaal gebufferd, en via 1k naar het volgende bord (J2).', 30.48, 25.4)
    d.text('De ketendata gaat via een flipflop op de dalende flank van SCLK de kabel op: geen last van de vertraging van de klok.', 30.48, 30.48)
    u = d.symbol(buffer, 'U28', '74AHCT541', 101.6, 88.9, SO20W)
    d.power('+5V', *u('VCC'))
    d.power('GND', *u('GND'))
    channels = [('DATA_IN', 'MOSI'), ('SCLK_IN', 'SCLK'), ('LATCH_IN', 'LATCH'), ('nOE_IN', 'nOE'), ('DIR_X1_IN', 'DIR_X1'),
                ('DIR_ROW_IN', 'DIR_ROW'), ('RET_IN', 'MISO_5V')]
    taps = {}
    for i, (inside, local) in enumerate(channels):
        a, y = u(f'A{i}'), u(f'Y{i}')
        d.wire((45.72, a[1]), a)
        d.port(inside, 45.72, a[1], 'left')
        d.wire(y, (165.1, y[1]))
        d.port(local, 165.1, y[1], 'right')
        taps[inside], taps[local] = a, y
    spare_in, spare_out = u('A7'), u('Y7')
    d.wire(spare_in, (83.82, spare_in[1]))
    d.power('GND', 83.82, spare_in[1])
    d.no_connect(*spare_out)
    g1, g2 = u('G1'), u('G2')
    d.wire(g1, (86.36, g1[1]), (86.36, g2[1]), g2)
    d.power('GND', 86.36, g2[1])
    # Inputs: nothing may float when no cable is in. nOE up, DIR down: all outputs off
    up = d.symbol(res, 'R1', '10k', 68.58, 71.12, R0603)
    d.power('+5V', *up('1'))
    d.wire(up('2'), (68.58, taps['nOE_IN'][1]))
    d.junction(68.58, taps['nOE_IN'][1])
    for ref, name, x in (('R2', 'DIR_X1_IN', 73.66), ('R3', 'DIR_ROW_IN', 78.74)):
        r = d.symbol(res, ref, '100k', x, 111.76, R0603)
        d.wire((x, taps[name][1]), r('1'))
        d.junction(x, taps[name][1])
        d.power('GND', *r('2'))
    pull = d.symbol(pack, 'RN15', '4x10k', 60.96, 111.76, ARRAY, mirror=True)
    for n, name in enumerate(('DATA_IN', 'SCLK_IN', 'LATCH_IN', 'RET_IN')):
        top = pull(str(n + 1))
        d.wire((top[0], taps[name][1]), top)
        d.junction(top[0], taps[name][1])
    bottoms = [pull(str(8 - n)) for n in range(4)]
    d.wire(*bottoms)
    d.power('GND', *bottoms[1])
    # Out to the cable: 1k in series (a basic part at JLCPCB). With 50 cm of cable and 10 pF of input that is
    # an edge of some 35 ns: fine up to a few MHz SCLK. Against the 10k pull-downs of the next board a high
    # arrives at 4.5 V, against its 10k pull-up on nOE a low at 0.45 V
    series = [('RN13', 127, ('SCLK', 'LATCH', 'nOE', 'DIR_X1'), ('SCLK_OUT', 'LATCH_OUT', 'nOE_OUT', 'DIR_X1_OUT')),
              ('RN14', 147.32, ('DIR_ROW', 'MISO_5V', None, None), ('DIR_ROW_OUT', 'RET_OUT', 'CHAIN_OUT', None))]
    for ref, x, inside, outside in series:
        p = d.symbol(pack, ref, '4x1k', x, 111.76, ARRAY, mirror=True)
        for n in range(4):
            top, bottom = p(str(n + 1)), p(str(8 - n))
            if inside[n]:
                d.wire((top[0], taps[inside[n]][1]), top)
                d.junction(top[0], taps[inside[n]][1])
            if outside[n] == 'CHAIN_OUT':
                d.wire(top, (top[0], top[1] - 5.08))
                d.label('CHAIN_Q', top[0], top[1] - 5.08, again=True)
            elif not outside[n]:
                d.no_connect(*top)
            if outside[n]:
                d.wire(bottom, (bottom[0], bottom[1] + 7.62))
                d.port(outside[n], bottom[0], bottom[1] + 7.62, 'down')
            else:
                d.no_connect(*bottom)
    # The flip-flop for the chain data, clocked by the inverted SCLK
    ff = d.symbol(flop, 'U29', '74LVC1G79', 127, 147.32, SOT235)
    d.power('+5V', *ff('VCC'))
    d.power('GND', *ff('GND'))
    inv = d.symbol(inverter, 'U26', '74HC14', 88.9, 152.4, SO14, unit=3)
    d.wire((71.12, 152.4), inv('5'))
    d.port('SCLK', 71.12, 152.4, 'left', again=True)
    d.wire(inv('6'), ff('C'))
    d.wire((45.72, ff('D')[1]), ff('D'))
    d.port('CHAIN_END', 45.72, ff('D')[1], 'left')
    jumper = d.symbol(load('Jumper', 'SolderJumper_2_Open'), 'JP1', 'Sluit bij laatste bord', 165.1, ff('Q')[1],
                      'Jumper:SolderJumper-2_P1.3mm_Open_TrianglePad1.0x1.5mm')
    d.wire(ff('Q'), jumper('1'))
    d.label('CHAIN_Q', 142.24, ff('Q')[1], again=True)
    d.wire(jumper('2'), (177.8, ff('Q')[1]))
    d.port('RET_IN', 177.8, ff('Q')[1], 'right', again=True)
    for ref, x in (('C18', 60.96), ('C19', 76.2)):
        c = d.symbol(cap, ref, '100nF', x, 172.72, C0603)
        d.power('+5V', *c('1'))
        d.power('GND', *c('2'))
    d.write()
    return d.file


def build():
    device = lambda name: schematic.load_symbol('Device', name)
    res, cap, cap_pol = device('R'), device('C'), device('C_Polarized')
    sheet = schematic.Sheet(NAME, 'Dotflipper v3')
    sheet.grid_y = 400

    def two(symbol, ref, value, x, y, net1, net2, footprint):
        names = [p['number'] for p in symbol.pins]
        sheet.place(symbol, ref, value, x, y, {names[0]: net1, names[1]: net2}, footprint=footprint)
        if value in PARTS:
            LCSC[ref] = PARTS[value]

    # The headers of the panel
    x1 = schematic.load_symbol('Connector_Generic', 'Conn_02x17_Odd_Even')
    x23 = schematic.load_symbol('Connector_Generic', 'Conn_02x08_Odd_Even')
    sheet.place(x1, 'X1', 'X1', 477.52, 76.2, {str(n): f'X1_{n}' for n in range(1, 33)}, footprint=IDC % 17)
    sheet.place(x23, 'X2', 'X2', 477.52, 177.8, {str(n): f'X2_{n}' for n in range(1, 17)}, footprint=IDC % 8)
    sheet.place(x23, 'X3', 'X3', 477.52, 248.92, {str(n): f'X3_{n}' for n in range(1, 17)}, footprint=IDC % 8)

    # ESP32 and what hangs on it
    esp, esp_raw = esp_symbol()
    sheet.place(esp, 'A1', 'ESP32-S3-DevKitC-1', 76.2, 101.6, ESP, footprint=f'{NAME}:ESP32-S3-DevKitC-1')
    two(device('D_Schottky'), 'D1', 'SS14', 76.2, 165.1, 'ESP_5V', '+5V', 'Diode_SMD:D_SMA')  # No 5 V from the USB of the ESP32 into the board
    # The chain comes back at 5 V, the ESP32 takes 3.3 V
    two(res, 'R4', '10k', 127, 165.1, 'MISO_5V', 'MISO', R0603)
    two(res, 'R5', '20k', 152.4, 165.1, 'MISO', 'GND', R0603)

    # Power: a ready made USB-C PD trigger on J4, J5 passes the 20 V on to a second board
    terminal = schematic.load_symbol('Connector_Generic', 'Conn_01x02')
    sheet.place(terminal, 'J4', '20V in', 50.8, 304.8, {'1': 'VM_IN', '2': 'GND'}, footprint=TERMINAL)
    sheet.place(terminal, 'J5', '20V door', 50.8, 330.2, {'1': 'VM_IN', '2': 'GND'}, footprint=TERMINAL)
    two(device('Fuse'), 'F1', '3A', 101.6, 317.5, 'VM_IN', 'VM', 'Fuse:Fuse_1206_3216Metric')
    two(cap_pol, 'C15', '470uF 35V', 139.7, 317.5, 'VM', 'GND', 'Capacitor_SMD:CP_Elec_10x10.5')
    two(cap, 'C16', '10uF 50V', 165.1, 317.5, 'VM', 'GND', C1206)
    # 5 V buck, the fixed 5 V AP63205 with the parts of table 3 in its datasheet (Diodes DS41326): 4.7 uH,
    # 10 uF in (C16), 2 x 22 uF out, 100 nF from SW to BST. FB senses the output itself, EN on VIN starts it
    buck = schematic.load_symbol('Regulator_Switching', 'AP63205WU')
    sheet.place(buck, 'U27', 'AP63205WU', 215.9, 317.5, {'IN': 'VM', 'EN': 'VM', 'GND': 'GND', 'FB': '+5V', 'BST': 'BST', 'SW': 'SW'},
                footprint='Package_TO_SOT_SMD:TSOT-23-6')
    LCSC['U27'] = PARTS['AP63205WU']
    two(cap, 'C20', '100nF', 241.3, 297.18, 'BST', 'SW', C0603)
    two(device('L'), 'L1', '4.7uH', 241.3, 330.2, 'SW', '+5V', 'Inductor_SMD:L_Sunlord_SWPA4030S')
    two(cap, 'C17', '22uF 25V', 266.7, 317.5, '+5V', 'GND', C1206)
    two(cap, 'C21', '22uF 25V', 279.4, 317.5, '+5V', 'GND', C1206)

    # Link to the next board: straight 10 way cable from J2 to J3 of the next board. Pin 1 carries the chain
    # data, pin 6 the chain back towards the ESP32. On the last board JP1 closes the chain
    link = schematic.load_symbol('Connector_Generic', 'Conn_02x05_Odd_Even')
    ground = {'2': 'GND', '4': 'GND', '10': 'GND'}
    sheet.place(link, 'J2', 'Naar volgend bord', 342.9, 342.9, {**ground, '1': 'CHAIN_OUT', '3': 'SCLK_OUT', '5': 'LATCH_OUT',
                '6': 'RET_IN', '7': 'nOE_OUT', '8': 'DIR_X1_OUT', '9': 'DIR_ROW_OUT'}, footprint=IDC % 5)
    sheet.place(link, 'J3', 'Van vorig bord', 419.1, 342.9, {**ground, '1': 'DATA_IN', '3': 'SCLK_IN', '5': 'LATCH_IN',
                '6': 'RET_OUT', '7': 'nOE_IN', '8': 'DIR_X1_IN', '9': 'DIR_ROW_IN'}, footprint=IDC % 5)
    return sheet, esp_raw


def sheet_blocks(root, blocks):
    """The sub sheets on the root sheet, with net labels on their pins."""
    out = []
    for page, block in enumerate(blocks, start=2):
        x, y = block['x'], block['y']
        identifier = schematic.uid('blok', block['name'])
        height = 2.54 * (max(len(block['in']), len(block['out'])) + 1)
        pins = ''
        for side, items in (('left', block['in']), ('right', block['out'])):
            for j, (name, net) in enumerate(items):
                px, angle, justify, kind, label = ((x, 180, 'left', 'input', 'right') if side == 'left' else
                                                   (x + block['w'], 0, 'right', 'output', 'left'))
                py = y + 2.54 * (j + 1)
                pins += (f'(pin "{name}" {kind} (at {px:.2f} {py:.2f} {angle}) (uuid "{schematic.uid(identifier, name)}") '
                         f'(effects (font (size 1.27 1.27)) (justify {justify})))\n')
                out.append(f'(label "{net}" (at {px:.2f} {py:.2f} {angle}) (effects (font (size 1.27 1.27)) (justify {label})) '
                           f'(uuid "{schematic.uid(identifier, "label", name)}"))')
        out.append(f'(sheet (at {x:.2f} {y:.2f}) (size {block["w"]:.2f} {height:.2f}) (exclude_from_sim no) (in_bom yes) (on_board yes) (dnp no) '
                   f'(stroke (width 0.1524) (type solid)) (fill (color 0 0 0 0.0000)) (uuid "{identifier}")\n'
                   f'(property "Sheetname" "{block["name"]}" (at {x:.2f} {y - 0.7:.2f} 0) (effects (font (size 1.27 1.27)) (justify left bottom)))\n'
                   f'(property "Sheetfile" "{block["file"]}.kicad_sch" (at {x:.2f} {y + height + 0.6:.2f} 0) '
                   '(effects (font (size 1.27 1.27)) (justify left top)))\n'
                   + pins + f'(instances (project "{NAME}" (path "/{root}" (page "{page}")))))')
    return '\n'.join(out) + '\n'


def write_schematic(sheet, esp_raw):
    schematic.PROJECT, schematic.OUT = NAME, HERE
    version = re.search(r'\(version (\d+)\)', schematic.library_text('power')).group(1)
    root = schematic.uid('sheet', NAME)
    schematic.write_sheet(sheet, {'VM', 'VM_IN', 'ESP_5V'}, f'/{root}', version, ['GND', 'VM_IN', 'VM', 'ESP_5V', '+5V'], {'power': 0, 'flag': 0})
    path = os.path.join(HERE, NAME + '.kicad_sch')
    text = open(path).read().replace('(paper "A3")', '(paper "A2")')

    groups = {kind: [{'name': g[0], 'refs': group_refs(n)} for n, g in enumerate(GROUPS) if g[1] == kind] for kind in ('X1', 'ROW')}
    files = {kind: group_sheet(kind, items, root, version) for kind, items in groups.items()}
    blocks = [{'name': 'Besturing', 'file': control_sheet(root, version), 'x': 165.1, 'y': 40.64, 'w': 50.8,
               'in': [('nOE', 'nOE'), ('DIR_X1', 'DIR_X1'), ('DIR_ROW', 'DIR_ROW')],
               'out': [(f'nOE_{s}_{k}', f'nOE_{s}_{k}') for k in ('X1', 'ROW') for s in 'HL']}]
    inputs = ['DATA_IN', 'SCLK_IN', 'LATCH_IN', 'nOE_IN', 'DIR_X1_IN', 'DIR_ROW_IN', 'RET_IN', 'CHAIN_END']
    outputs = ['MOSI', 'SCLK', 'LATCH', 'nOE', 'DIR_X1', 'DIR_ROW', 'MISO_5V',
               'CHAIN_OUT', 'SCLK_OUT', 'LATCH_OUT', 'nOE_OUT', 'DIR_X1_OUT', 'DIR_ROW_OUT', 'RET_OUT']
    blocks.append({'name': 'Doorlus', 'file': link_sheet(root, version), 'x': 165.1, 'y': 76.2, 'w': 50.8,
                   'in': [(n, n) for n in inputs], 'out': [(n, n) for n in outputs]})
    y = 40.64
    for n, (name, kind, header, first) in enumerate(GROUPS):
        inputs = [('SER', 'MOSI' if n == 0 else f'CHAIN{n}'), ('SRCLK', 'SCLK'), ('RCLK', 'LATCH'),
                  ('nOE_H', f'nOE_H_{kind}'), ('nOE_L', f'nOE_L_{kind}')]
        outputs = [('QH', 'CHAIN_END' if n == len(GROUPS) - 1 else f'CHAIN{n + 1}')]
        for channel in range(8, 0, -1):
            index = 8 - channel
            if kind == 'X1':
                outputs.append((f'OUT{channel}', f'X1_{first + index}'))
            else:
                outputs += [(f'PLUS{channel}', f'{header}_{2 * index + 1}'), (f'MIN{channel}', f'{header}_{2 * index + 2}')]
        blocks.append({'name': name, 'file': files[kind], 'x': 292.1, 'y': y, 'w': 63.5, 'in': inputs, 'out': outputs})
        y += 2.54 * (len(outputs) + 1) + 12.7
    words = [('ESP32-S3-DevKitC-1', 40.64, 58.42), ('Voeding: 20 V van de PD-module', 40.64, 289.56), ('Doorlus naar het volgende bord', 330.2, 320.04),
             ('Besturing', 165.1, 35.56), ('Zes groepen van 8 lijnen', 292.1, 35.56), ('Connectors naar het paneel', 457.2, 30.48)]
    notes = ''.join(f'(text "{w}" (at {x} {y} 0) (effects (font (size 2.54 2.54)) (justify left bottom)) (uuid "{schematic.uid("root", w)}"))\n'
                    for w, x, y in words)
    text = text.rstrip()[:-1] + sheet_blocks(root, blocks) + notes + '(sheet_instances (path "/" (page "1")))\n)\n'
    for ref, code in LCSC.items():
        if not re.search(r'\(reference "%s"\)' % ref, text):
            continue  # In a sub sheet, which carries the field itself
        # Every unit of a part with more units gets the field, otherwise the BOM splits them
        matches = list(re.finditer(r'\(property "Footprint" "[^"]*" \(at ([-\d.]+) ([-\d.]+) 0\)[^\n]*\n(?=\(property "Datasheet" "" [^\n]*\n'
                                   r'(?:\(pin [^\n]*\n)+\(instances \(project "%s" \(path "[^"]*" \(reference "%s"\))' % (NAME, ref), text))
        assert matches, ref
        for match in reversed(matches):
            text = (text[:match.end()] + f'(property "LCSC" "{code}" (at {match.group(1)} {match.group(2)} 0) '
                    '(effects (font (size 1.27 1.27)) (hide yes)))\n' + text[match.end():])
    # Through hole parts are soldered by hand: not in the BOM or the position file for assembly
    for ref in re.findall(r'\(reference "([^"]+)"\)', text):
        if ref.startswith('#') or ref in LCSC:
            continue
        text, count = re.subn(r'\(in_bom yes\) \(on_board yes\)( \(dnp no\) \(uuid "[^"]*"\)\n\(property "Reference" "%s")' % ref,
                              r'(in_bom no) (on_board yes) (in_pos_files no)\1', text)
    open(path, 'w').write(text)
    with open(os.path.join(HERE, NAME + '.kicad_sym'), 'w') as f:
        f.write(f'(kicad_symbol_lib (version {version}) (generator "dotflipper3") (generator_version "10.0")\n{esp_raw}\n)\n')
    os.makedirs(os.path.join(HERE, NAME + '.pretty'), exist_ok=True)
    with open(os.path.join(HERE, NAME + '.pretty', 'ESP32-S3-DevKitC-1.kicad_mod'), 'w') as f:
        f.write(esp_footprint())
    for table, kind, uri in (('sym-lib-table', 'sym_lib_table', f'{NAME}.kicad_sym'), ('fp-lib-table', 'fp_lib_table', f'{NAME}.pretty')):
        with open(os.path.join(HERE, table), 'w') as f:
            f.write(f'({kind} (version 7)\n(lib (name "{NAME}")(type "KiCad")(uri "${{KIPRJMOD}}/{uri}")(options "")(descr ""))\n)\n')
    project = os.path.join(HERE, NAME + '.kicad_pro')
    if not os.path.exists(project):
        open(project, 'w').write('{"meta": {"filename": "%s.kicad_pro", "version": 3}}\n' % NAME)


def parse_netlist(path):
    text = open(path).read()
    parts = {}
    for match in re.finditer(r'\(comp\s+\(ref "(.+?)"\)(.*?)(?=\n\s*\(comp\s|\n\s*\(libparts|\Z)', text, re.S):
        body = match.group(2)
        stamps = re.findall(r'\(tstamps "(.*?)"\)', body)
        sheet = re.search(r'\(sheetpath\s+\(names "(.*?)"\)', body).group(1).strip('/') or 'Root'
        parts[match.group(1)] = (re.search(r'\(value "(.*?)"\)', body).group(1),
                                 re.search(r'\(footprint "(.*?)"\)', body).group(1),
                                 stamps[0] + stamps[-1], sheet)
    nets = {}
    for match in re.finditer(r'\(net\s+\(code "?\d+"?\)\s+\(name "(.*?)"(.*?)(?=\n\s*\(net\s|\Z)', text, re.S):
        for ref, pin in re.findall(r'\(ref "(.*?)"\)\s+\(pin "(.*?)"', match.group(2)):
            nets[(ref, pin)] = match.group(1)
    return parts, nets


def load(entry):
    library, name = entry.split(':')
    folder = os.path.join(HERE, NAME + '.pretty') if library == NAME else f'/usr/share/kicad/footprints/{library}.pretty'
    return pcbnew.FootprintLoad(folder, name)


# As on v1 the headers sit along the top edge, 82.55 and 33.02 mm apart, key to the edge, pin 1 on the right.
# Under every header its groups, one column per group, from the top: TBD, its 595, ULN, its 595. The outputs
# of a driver face the header, channel 1 on the left. The 595s are turned so QB-QH face their driver, every
# pad of them straight under its input at the same 1.27 mm pitch; QA comes around the right end. The outputs
# of the ULN go up past the TBD on the bottom layer. The ESP32 stands at the right edge; the antenna of its
# module sticks out past the top edge of the board.
C = 55
GROUP_X = {'G4': 15, 'G3': 41.5, 'G2': 68, 'G1': 94.5, 'G6': C + 82.55, 'G5': C + 115.57}
ROWS = {'UDN': 22.5, 'H': 33.5, 'ULN': 46.5, 'L': 57.5}
CENTRES = {
    'X1': (C, 6.2, 270), 'X2': (C + 82.55, 6.2, 270), 'X3': (C + 115.57, 6.2, 270),
    'A1': (203.1, 31.4, 0),
    'U25': (116.5, 30, 90), 'C13': (116.5, 22, 0), 'U26': (116.5, 47, 90), 'C14': (116.5, 39, 0),
    'R1': (116.5, 56, 0), 'R2': (116.5, 59, 0), 'R3': (116.5, 62, 0),
    'J4': (15, 71, 0), 'J5': (27, 71, 0), 'F1': (36.5, 68, 90), 'C15': (48, 70, 0), 'C16': (59, 66, 90),
    'U27': (64, 71, 0), 'C20': (66.8, 67.6, 90), 'L1': (70.5, 71, 0), 'C21': (75, 71, 90), 'C17': (77, 66, 90),
    'D1': (186, 74.5, 90),
    'J2': (128, 72.5, 270), 'J3': (155, 72.5, 270), 'R4': (175, 72.5, 0), 'R5': (175, 75.5, 0),
    # The link buffer between the power corner and J2
    'U28': (100, 71, 90), 'C18': (90, 75.5, 90), 'RN15': (89.5, 70, 0), 'RN13': (110, 68.5, 0), 'RN14': (110, 73, 0),
    'U29': (114.5, 76, 0), 'C19': (110, 77, 0), 'JP1': (175, 69, 0),
}
HOLES = ((3.5, 3.5), (3.5, HEIGHT - 3.5), (WIDTH - 3.5, HEIGHT - 3.5))


def places():
    result = dict(CENTRES)
    for g, (name, kind, header, first) in enumerate(GROUPS):
        x, refs = GROUP_X[name], group_refs(g)
        result[refs['UDN']] = (x, ROWS['UDN'], 90)
        result[refs['ULN']] = (x, ROWS['ULN'], 90)
        xc = x - 1.905  # QB-QH straight under I7-I1
        result[refs['SRH']] = (xc, ROWS['H'], 270)
        result[refs['SRL']] = (xc, ROWS['L'], 270)
        result[refs['CH']] = (xc + 4.45, ROWS['H'] + 5, 0)
        result[refs['CL']] = (xc + 4.45, ROWS['L'] + 5, 0)
        result[refs['RN1']] = (xc + 8.7, ROWS['H'], 0)    # QA-QD, pads 1-4 towards the 595
        result[refs['RN2']] = (xc - 8.7, ROWS['H'], 180)  # QE-QH, pads 1-4 towards the 595
    return result


def centre(footprint, x, y, rotation):
    footprint.SetOrientationDegrees(rotation)
    box = footprint.GetCourtyard(pcbnew.F_CrtYd).BBox()
    position = footprint.GetPosition()
    footprint.SetPosition(pcbnew.VECTOR2I(position.x + MM(x) - box.Centre().x, position.y + MM(y) - box.Centre().y))


def quiet(footprint, ref):
    """Small parts packed too close for text on the silkscreen: their reference only on the fab layer."""
    if re.match(r'C([1-9]|1[0-2]|18|19|20|21)$|RN1[345]$', ref):
        footprint.Reference().SetLayer(pcbnew.F_Fab)


def sheet_file(sheet):
    kind = next((g[1] for g in GROUPS if g[0] == sheet), None)
    if kind:
        return f'groep_{"x1" if kind == "X1" else "rij"}.kicad_sch'
    return {'Besturing': 'besturing.kicad_sch', 'Doorlus': 'doorlus.kicad_sch'}.get(sheet, NAME + '.kicad_sch')


def describe(footprint, ref, value, entry, identifier, sheet):
    """Fields, path and BOM flags of a footprint from the netlist."""
    footprint.SetReference(ref)
    footprint.SetValue(value)
    footprint.SetFPID(pcbnew.LIB_ID(*entry.split(':')))
    footprint.SetPath(pcbnew.KIID_PATH(identifier))
    footprint.SetSheetfile(sheet_file(sheet))
    footprint.SetSheetname(sheet)
    footprint.SetField('Datasheet', '')
    footprint.SetField('Description', '')
    if ref in LCSC:
        footprint.SetField('LCSC', LCSC[ref])
        footprint.GetField('LCSC').SetVisible(False)
    footprint.SetExcludedFromBOM(ref not in LCSC)
    footprint.SetExcludedFromPosFiles(ref not in LCSC)


def update_board():
    """Brings the board in line with the schematic and keeps everything done by hand: placement, tracks,
    vias and zones stay. New parts go to their place from places(), parts that are gone are removed. A
    track keeps its net when that net still exists; tracks on a net that disappeared are listed."""
    netlist = tempfile.mktemp(suffix='.net')
    subprocess.run(['kicad-cli', 'sch', 'export', 'netlist', '--format', 'kicadsexpr', '-o', netlist,
                    os.path.join(HERE, NAME + '.kicad_sch')], check=True, capture_output=True)
    parts, nets = parse_netlist(netlist)
    os.remove(netlist)
    path = os.path.join(HERE, NAME + '.kicad_pcb')
    board = pcbnew.LoadBoard(path)
    infos = {}
    for name in sorted(set(nets.values())):
        info = board.FindNet(name)
        if not info:
            info = pcbnew.NETINFO_ITEM(board, name)
            board.Add(info)
        infos[name] = info
    where = places()
    present = {f.GetReference(): f for f in board.GetFootprints()}
    added, removed = [], []
    for ref, (value, entry, identifier, sheet) in parts.items():
        footprint = present.get(ref)
        if footprint is None or footprint.GetFPID().GetUniStringLibItemName() != entry.split(':')[1]:
            if footprint is not None:
                board.Remove(footprint)
            footprint = load(entry)
            board.Add(footprint)
            centre(footprint, *where[ref])
            added.append(ref)
        quiet(footprint, ref)
        describe(footprint, ref, value, entry, identifier, sheet)
        numbers = {pad.GetNumber() for pad in footprint.Pads()}
        for pad in footprint.Pads():
            pad.SetNet(infos[nets[(ref, pad.GetNumber())]] if (ref, pad.GetNumber()) in nets else board.FindNet(''))
        missing = [pin for other, pin in nets if other == ref and pin not in numbers]
        assert not missing, (ref, missing)
    for ref, footprint in present.items():
        # Mounting holes and board only items such as the logo are not in the schematic: they stay
        if ref not in parts and not ref.startswith('H') and not footprint.IsBoardOnly():
            board.Remove(footprint)
            removed.append(ref)
    orphans = sorted({t.GetNetname() for t in board.GetTracks() if t.GetNetname() and t.GetNetname() not in infos})
    pcbnew.SaveBoard(path, board, True)
    print('PCB bijgewerkt. Nieuw:', ', '.join(added) or '-', '| weg:', ', '.join(removed) or '-',
          '| sporen op verdwenen netten:', ', '.join(orphans) or '-')


def write_board():
    netlist = tempfile.mktemp(suffix='.net')
    subprocess.run(['kicad-cli', 'sch', 'export', 'netlist', '--format', 'kicadsexpr', '-o', netlist,
                    os.path.join(HERE, NAME + '.kicad_sch')], check=True, capture_output=True)
    parts, nets = parse_netlist(netlist)
    os.remove(netlist)
    board = pcbnew.CreateEmptyBoard()
    infos = {}
    for name in sorted(set(nets.values())):
        infos[name] = pcbnew.NETINFO_ITEM(board, name)
        board.Add(infos[name])
    where = places()
    for ref, (value, entry, identifier, sheet) in parts.items():
        footprint = load(entry)
        describe(footprint, ref, value, entry, identifier, sheet)
        board.Add(footprint)
        numbers = {pad.GetNumber() for pad in footprint.Pads()}
        for pad in footprint.Pads():
            if (ref, pad.GetNumber()) in nets:
                pad.SetNet(infos[nets[(ref, pad.GetNumber())]])
        missing = [pin for other, pin in nets if other == ref and pin not in numbers]
        assert not missing, (ref, missing)
        centre(footprint, *where[ref])
        quiet(footprint, ref)
    for i, (x, y) in enumerate(HOLES, start=1):
        hole = load('MountingHole:MountingHole_3.2mm_M3')
        hole.SetReference(f'H{i}')
        hole.SetBoardOnly(True)
        hole.SetPosition(pcbnew.VECTOR2I(MM(x), MM(y)))
        board.Add(hole)
    shape = pcbnew.PCB_SHAPE(board)
    shape.SetShape(pcbnew.SHAPE_T_RECT)
    shape.SetStart(pcbnew.VECTOR2I(0, 0))
    shape.SetEnd(pcbnew.VECTOR2I(MM(WIDTH), MM(HEIGHT)))
    shape.SetLayer(pcbnew.Edge_Cuts)
    shape.SetWidth(MM(0.1))
    board.Add(shape)
    pcbnew.SaveBoard(os.path.join(HERE, NAME + '.kicad_pcb'), board, True)



def write_outputs():
    """Which bit of the chain drives which wire, for the firmware."""
    with open(os.path.join(HERE, 'bits.csv'), 'w') as f:
        f.write('groep,bit,595-uitgang,draad hoog (XL62783),draad laag (ULN)\n')
        for name, kind, header, first in GROUPS:
            for index in range(8):
                pin = f'Q{"ABCDEFGH"[index]}'
                if kind == 'X1':
                    f.write(f'{name},{index},{pin},X1_{first + index},X1_{first + index}\n')
                else:
                    row = first + index
                    f.write(f'{name},{index},{pin},{header}_{2 * row + 1},{header}_{2 * row + 2}\n')


if __name__ == '__main__':
    # Never write over a schematic or board that KiCad has open: its next save would undo this, or the other way round
    locks = [f for f in os.listdir(HERE) if f.startswith('~') and f.endswith(('.kicad_sch.lck', '.kicad_pcb.lck'))]
    if locks:
        sys.exit('KiCad heeft ' + ', '.join(locks) + ' open: sluit de schema- en PCB-editor eerst.')
    sheet, esp_raw = build()
    write_schematic(sheet, esp_raw)
    # An existing board holds work done by hand: only bring it up to date, unless asked to start over
    if os.path.exists(os.path.join(HERE, NAME + '.kicad_pcb')) and '--opnieuw' not in sys.argv:
        update_board()
    else:
        write_board()
    write_outputs()
    print('Klaar')

