"""Schematic helpers for generate.py: KiCad symbols from the standard libraries, and a sheet on which
every pin of a part gets a net label (write_sheet) instead of a drawn wire.

Taken from the generator of controller v2. uid() keeps its old namespace so the identifiers in the
schematic and board stay the same.
"""

import os
import re
import uuid

SYMBOLS = '/usr/share/kicad/symbols/'
PROJECT = 'v3'  # Set by generate.py
OUT = os.path.dirname(os.path.abspath(__file__))
POWER = {'GND': 'GND', '+3V3': '+3V3', '+5V': '+5V'}


def uid(*parts):
    return str(uuid.uuid5(uuid.NAMESPACE_URL, 'dotflipper2/' + '/'.join(str(p) for p in parts)))


def balanced(text, start):
    depth, quoted, i = 0, False, start
    while True:
        c = text[i]
        if c == '"' and text[i - 1] != '\\':
            quoted = not quoted
        elif not quoted:
            if c == '(':
                depth += 1
            elif c == ')':
                depth -= 1
                if depth == 0:
                    return text[start:i + 1]
        i += 1


class Symbol:
    def __init__(self, lib, name, raw):
        self.lib, self.name = lib, name
        self.id = f'{lib}:{name}'
        self.raw = raw.replace(f'(symbol "{name}"', f'(symbol "{self.id}"', 1)
        self.pins = []
        for unit_match in re.finditer(r'\(symbol "%s_(\d+)_(\d+)"' % re.escape(name), raw):
            unit = int(unit_match.group(1))
            body = balanced(raw, unit_match.start())
            for pin in re.finditer(r'\(pin\s+(\S+)\s+(\S+)\s+\(at ([-\d.]+) ([-\d.]+) (\d+)\)\s+\(length ([-\d.]+)\)',
                                   body):
                rest = balanced(body, pin.start())
                self.pins.append({
                    'unit': unit, 'type': pin.group(1),
                    'x': float(pin.group(3)), 'y': float(pin.group(4)), 'angle': int(pin.group(5)),
                    'name': re.search(r'\(name "(.*?)"', rest).group(1),
                    'number': re.search(r'\(number "(.*?)"', rest).group(1),
                })


_files = {}


def library_text(lib):
    if lib not in _files:
        _files[lib] = open(SYMBOLS + lib + '.kicad_sym').read()
    return _files[lib]


def load_symbol(lib, name):
    text = library_text(lib)
    start = text.find(f'(symbol "{name}"')
    assert start >= 0, (lib, name)
    raw = balanced(text, start)
    parent = re.search(r'\(extends "(.+?)"\)', raw)
    if parent:
        base = load_symbol(lib, parent.group(1)).raw.replace(f'"{lib}:{parent.group(1)}"', f'"{parent.group(1)}"', 1)
        raw = base.replace(f'(symbol "{parent.group(1)}"', f'(symbol "{name}"', 1)
        raw = raw.replace(f'"{parent.group(1)}_', f'"{name}_')
    return Symbol(lib, name, raw)


class Sheet:
    def __init__(self, name, title):
        self.name, self.title = name, title
        self.items = []  # (kind, ...)
        self.symbols = {}
        self.slot = 0

    def use(self, symbol):
        self.symbols[symbol.id] = symbol

    def place(self, symbol, ref, value, x, y, conns, unit=1, footprint='', mpn=''):
        """conns maps a pin name or number to a net, None is no connect. Other pins are no connect too."""
        self.use(symbol)
        tips = {}
        for pin in symbol.pins:
            if pin['unit'] not in (0, unit):
                continue
            tip = (x + pin['x'], y - pin['y'])
            net = conns.get(pin['number'], conns.get(pin['name'], None))
            if tip not in tips or net is not None:
                tips[tip] = (net, pin['angle'], pin['type'])
        used = set()
        for pin in symbol.pins:
            used.update((pin['number'], pin['name']))
        for key in conns:
            assert key in used, (ref, key, sorted(used))
        self.items.append(('symbol', symbol, ref, value, x, y, unit, footprint, mpn,
                           [p['number'] for p in symbol.pins if p['unit'] in (0, unit)]))
        for tip, (net, angle, kind) in tips.items():
            self.items.append(('net', net, tip[0], tip[1], angle, kind))

    def passive_single(self, symbol, ref, value, net, footprint=''):
        column, row = self.slot % 7, self.slot // 7
        self.slot += 1
        self.place(symbol, ref, value, 40.64 + column * 50.8, self.grid_y + row * 25.4, {symbol.pins[0]['number']: net}, footprint=footprint)

    def passive(self, symbol, ref, value, net1, net2, footprint='', mpn=''):
        """Two pin parts go in a grid at the bottom of the sheet."""
        column, row = self.slot % 7, self.slot // 7
        self.slot += 1
        x, y = 40.64 + column * 50.8, self.grid_y + row * 25.4
        names = [p['number'] for p in symbol.pins]
        self.place(symbol, ref, value, x, y, {names[0]: net1, names[1]: net2}, footprint=footprint, mpn=mpn)


def text_effects(justify=''):
    return f'(effects (font (size 1.27 1.27)){justify})'


def write_sheet(sheet, nets_elsewhere, sheet_path, library_version, flags, counters):
    out = [f'(kicad_sch (version {library_version}) (generator "dotflipper3") (generator_version "10.0")',
           f'(uuid "{uid("sheet", sheet.name)}")', '(paper "A3")',
           f'(title_block (title "{sheet.title}") (company "Dotflipper v3"))', '(lib_symbols']
    power_symbols = {name: load_symbol('power', name) for name in ('GND', '+3V3', '+5V', 'PWR_FLAG')}
    for symbol in list(sheet.symbols.values()) + list(power_symbols.values()):
        out.append(symbol.raw)
    out.append(')')

    def power_symbol(name, x, y, rotation, ref):
        symbol = power_symbols[name]
        identifier = uid(sheet.name, ref)
        out.append(f'(symbol (lib_id "{symbol.id}") (at {x:.2f} {y:.2f} {rotation}) (unit 1) (exclude_from_sim no) '
                   f'(in_bom yes) (on_board yes) (dnp no) (uuid "{identifier}")\n'
                   f'(property "Reference" "{ref}" (at {x:.2f} {y:.2f} 0) (effects (font (size 1.27 1.27)) (hide yes)))\n'
                   f'(property "Value" "{name}" (at {x:.2f} {y:.2f} 0) (effects (font (size 1.27 1.27)) (hide yes)))\n'
                   f'(property "Footprint" "" (at {x:.2f} {y:.2f} 0) (effects (font (size 1.27 1.27)) (hide yes)))\n'
                   f'(property "Datasheet" "" (at {x:.2f} {y:.2f} 0) (effects (font (size 1.27 1.27)) (hide yes)))\n'
                   f'(pin "1" (uuid "{uid(identifier, "pin")}"))\n'
                   f'(instances (project "{PROJECT}" (path "{sheet_path}" (reference "{ref}") (unit 1)))))')

    for item in sheet.items:
        if item[0] == 'symbol':
            _, symbol, ref, value, x, y, unit, footprint, mpn, numbers = item
            identifier = uid(sheet.name, ref, unit)
            out.append(f'(symbol (lib_id "{symbol.id}") (at {x:.2f} {y:.2f} 0) (unit {unit}) (exclude_from_sim no) '
                       f'(in_bom yes) (on_board yes) (dnp no) (uuid "{identifier}")\n'
                       f'(property "Reference" "{ref}" (at {x + 2.54:.2f} {y - 5.08:.2f} 0) {text_effects(" (justify left)")})\n'
                       f'(property "Value" "{value}" (at {x + 2.54:.2f} {y + 5.08:.2f} 0) {text_effects(" (justify left)")})\n'
                       f'(property "Footprint" "{footprint}" (at {x:.2f} {y:.2f} 0) (effects (font (size 1.27 1.27)) (hide yes)))\n'
                       f'(property "Datasheet" "" (at {x:.2f} {y:.2f} 0) (effects (font (size 1.27 1.27)) (hide yes)))')
            for number in dict.fromkeys(numbers):
                out.append(f'(pin "{number}" (uuid "{uid(identifier, number)}"))')
            out.append(f'(instances (project "{PROJECT}" (path "{sheet_path}" (reference "{ref}") (unit {unit})))))')
        else:
            _, net, x, y, angle, kind = item
            if net is None:
                if kind != 'no_connect':
                    out.append(f'(no_connect (at {x:.2f} {y:.2f}) (uuid "{uid(sheet.name, "nc", x, y)}"))')
                continue
            direction = {0: 'left', 180: 'right', 90: 'down', 270: 'up'}[angle]
            if net in POWER:
                rotation = ({'down': 0, 'right': 90, 'up': 180, 'left': 270} if net == 'GND' else
                            {'up': 0, 'left': 90, 'down': 180, 'right': 270})[direction]
                counters['power'] += 1
                power_symbol(POWER[net], x, y, rotation, f'#PWR{counters["power"]:04d}')
                continue
            rotation = {'left': 180, 'right': 0, 'down': 270, 'up': 90}[direction]
            justify = ' (justify right)' if direction in ('left', 'down') else ' (justify left)'
            kind_label = 'global_label' if net in nets_elsewhere else 'label'
            shape = ' (shape bidirectional)' if kind_label == 'global_label' else ''
            out.append(f'({kind_label} "{net}"{shape} (at {x:.2f} {y:.2f} {rotation}) {text_effects(justify)} '
                       f'(uuid "{uid(sheet.name, "label", net, x, y)}"))')

    for i, net in enumerate(flags):
        x, y = 355.6 + 25.4 * i, 25.4
        counters['flag'] += 1
        ref = f'#FLG{counters["flag"]:03d}'
        power_symbol('PWR_FLAG', x, y, 0, ref)
        if net in POWER:
            counters['power'] += 1
            power_symbol(POWER[net], x, y, 180 if net == 'GND' else 0, f'#PWR{counters["power"]:04d}')
        else:
            out.append(f'(global_label "{net}" (shape bidirectional) (at {x:.2f} {y:.2f} 270) {text_effects(" (justify right)")} '
                       f'(uuid "{uid(sheet.name, "flag", net)}"))')

    out.append(')')
    with open(os.path.join(OUT, sheet.name + '.kicad_sch'), 'w') as f:
        f.write('\n'.join(out))
