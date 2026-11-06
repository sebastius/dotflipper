#!/usr/bin/env python3
"""Generates the KiCad schematics of the dotflipper v2 controller, see ONTWERP.md.

All 64 outputs of a panel are the same thing 64 times, so a script makes fewer mistakes than
drawing them. Every pin gets a net label: the schematic reads as a netlist, not as a drawing.
"""

import os
import re
import uuid

SYMBOLS = '/usr/share/kicad/symbols/'
PROJECT = 'dotflipper2'
HERE = os.path.dirname(os.path.abspath(__file__))
OUT = os.path.join(HERE, 'kicad')

R0603 = 'Resistor_SMD:R_0603_1608Metric'
C0603 = 'Capacitor_SMD:C_0603_1608Metric'
C0805 = 'Capacitor_SMD:C_0805_2012Metric'
C1206 = 'Capacitor_SMD:C_1206_3216Metric'
DRV_FOOTPRINT = 'Package_SO:HTSSOP-24-1EP_4.4x7.8mm_P0.65mm_EP3.4x7.8mm_Mask2.4x4.68mm_ThermalVias'

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


def custom_drv8912():
    left = [('VM', '16', 'power_in'), ('VM', '21', 'power_in'), ('VDD', '6', 'power_in'),
            ('SDI', '5', 'input'), ('SCLK', '20', 'input'), ('nSCS', '19', 'input'),
            ('nSLEEP', '8', 'input'), ('SDO', '7', 'output'), ('nFAULT', '12', 'open_collector'),
            ('GND', '1', 'power_in'), ('GND', '13', 'power_in'), ('GND', '24', 'power_in'), ('EP', '25', 'power_in')]
    right = [(f'OUT{n}', p, 'bidirectional') for n, p in
             ((1, '2'), (2, '23'), (3, '14'), (4, '11'), (5, '3'), (6, '10'), (7, '4'), (8, '22'), (9, '9'),
              (10, '15'), (11, '17'), (12, '18'))]
    height = 2.54 * (len(left) + 1)
    pins = ''
    for i, (name, number, kind) in enumerate(left):
        y = height / 2 - 2.54 * (i + 1)
        pins += (f'(pin {kind} line (at -15.24 {y:.2f} 0) (length 2.54) (name "{name}" (effects (font (size 1.27 1.27)))) '
                 f'(number "{number}" (effects (font (size 1.27 1.27)))))\n')
    for i, (name, number, kind) in enumerate(right):
        y = height / 2 - 2.54 * (i + 1)
        pins += (f'(pin {kind} line (at 15.24 {y:.2f} 180) (length 2.54) (name "{name}" (effects (font (size 1.27 1.27)))) '
                 f'(number "{number}" (effects (font (size 1.27 1.27)))))\n')
    prop = lambda name, value, y, hide='': (f'(property "{name}" "{value}" (at 0 {y} 0) (effects (font (size 1.27 1.27)){hide}))\n')
    raw = ('(symbol "DRV8912-Q1" (pin_names (offset 1.016)) (exclude_from_sim no) (in_bom yes) (on_board yes)\n'
           + prop('Reference', 'U', height / 2 + 2.54)
           + prop('Value', 'DRV8912-Q1', -height / 2 - 2.54)
           + prop('Footprint', DRV_FOOTPRINT, 0, ' (hide yes)')
           + prop('Datasheet', 'https://www.ti.com/lit/ds/symlink/drv8912-q1.pdf', 0, ' (hide yes)')
           + prop('Description', '12 half-bridge driver with SPI', 0, ' (hide yes)')
           + '(symbol "DRV8912-Q1_0_1" (rectangle (start -12.7 %.2f) (end 12.7 %.2f) '
             '(stroke (width 0.254) (type default)) (fill (type background))))\n' % (height / 2, -height / 2)
           + '(symbol "DRV8912-Q1_1_1"\n' + pins + ')\n)')
    return Symbol(PROJECT, 'DRV8912-Q1', raw), raw


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

    def passive(self, symbol, ref, value, net1, net2, footprint='', mpn=''):
        """Two pin parts go in a grid at the bottom of the sheet."""
        column, row = self.slot % 7, self.slot // 7
        self.slot += 1
        x, y = 40.64 + column * 50.8, self.grid_y + row * 25.4
        names = [p['number'] for p in symbol.pins]
        self.place(symbol, ref, value, x, y, {names[0]: net1, names[1]: net2}, footprint=footprint, mpn=mpn)


def build():
    device = lambda name: load_symbol('Device', name)
    res, cap, cap_pol = device('R'), device('C'), device('C_Polarized')
    schottky, led, fuse = device('D_Schottky'), device('LED'), device('Fuse')
    tvs = device('D_TVS')
    inductor = device('L')
    button = load_symbol('Switch', 'SW_Push')
    drv, drv_raw = custom_drv8912()

    power = Sheet('power', 'Voeding')
    safety = Sheet('safety', 'Hardware-beveiliging')
    mcu = Sheet('mcu', 'ESP32-S3')
    panels = [Sheet('panel0', 'Paneel 0'), Sheet('panel1', 'Paneel 1')]

    # Power: 20 V in, 5 V from a buck or USB, so the logic also runs with the 20 V off
    power.grid_y = 139.7
    term = load_symbol('Connector_Generic', 'Conn_01x02')
    power.place(term, 'J1', '20V IN', 50.8, 50.8, {'1': 'VIN_RAW', '2': 'GND'},
                footprint='TerminalBlock_Phoenix:TerminalBlock_Phoenix_MKDS-1,5-2-5.08_1x02_P5.08mm_Horizontal')
    buck = load_symbol('Regulator_Switching', 'AP63205WU')
    power.place(buck, 'U1', 'AP63205WU', 127, 50.8,
                {'IN': 'VIN', 'EN': 'VIN', 'GND': 'GND', 'FB': '5V_BUCK', 'BST': 'BST', 'SW': 'SW'},
                footprint='Package_TO_SOT_SMD:TSOT-23-6')
    ldo = load_symbol('Regulator_Linear', 'AMS1117-3.3')
    power.place(ldo, 'U2', 'AMS1117-3.3', 228.6, 50.8, {'VI': '+5V', 'VO': '+3V3', 'GND': 'GND'},
                footprint='Package_TO_SOT_SMD:SOT-223-3_TabPin2')
    power.passive(fuse, 'F1', '3A', 'VIN_RAW', 'VIN', footprint='Fuse:Fuse_1812_4532Metric')
    power.passive(tvs, 'D1', 'SMBJ24A', 'VIN', 'GND', footprint='Diode_SMD:D_SMB')
    power.passive(cap, 'C1', '10uF 50V', 'VIN', 'GND', footprint=C1206)
    power.passive(cap, 'C2', '100nF', 'BST', 'SW', footprint=C0603)
    power.passive(inductor, 'L1', '4.7uH', 'SW', '5V_BUCK', footprint='Inductor_SMD:L_Bourns_SRP7028A_7.3x6.6mm')
    power.passive(cap, 'C3', '22uF', '5V_BUCK', 'GND', footprint=C1206)
    power.passive(cap, 'C4', '22uF', '5V_BUCK', 'GND', footprint=C1206)
    power.passive(schottky, 'D2', 'SS14', '+5V', '5V_BUCK', footprint='Diode_SMD:D_SMA')
    power.passive(schottky, 'D3', 'SS14', '+5V', 'USB_VBUS', footprint='Diode_SMD:D_SMA')
    power.passive(cap, 'C5', '10uF', '+5V', 'GND', footprint=C0805)
    power.passive(cap, 'C6', '22uF', '+3V3', 'GND', footprint=C1206)

    # Safety: nothing here depends on the MCU doing the right thing
    safety.grid_y = 165.1
    pfet = load_symbol('Transistor_FET', 'AO3401A')
    nfet = load_symbol('Transistor_FET', '2N7002')
    safety.place(pfet, 'Q1', 'AO3401A', 50.8, 50.8, {'G': 'VM_GATE', 'S': 'VIN', 'D': 'VM_SW'},
                 footprint='Package_TO_SOT_SMD:SOT-23')
    safety.place(nfet, 'Q2', '2N7002', 50.8, 88.9, {'G': 'NSLEEP', 'S': 'GND', 'D': 'VM_GATE_N'},
                 footprint='Package_TO_SOT_SMD:SOT-23')
    ina = load_symbol('Amplifier_Current', 'INA180A1')
    safety.place(ina, 'U3', 'INA180A1', 114.3, 50.8, {'+': 'VM_SW', '-': 'VM', 'V+': '+3V3', 'GND': 'GND', '1': 'ISENSE'},
                 footprint='Package_TO_SOT_SMD:SOT-23-5')
    comparator = load_symbol('Comparator', 'LM393')
    footprint_so8 = 'Package_SO:SOIC-8_3.9x4.9mm_P1.27mm'
    safety.place(comparator, 'U4', 'LM393', 177.8, 50.8, {'+': 'ISENSE', '-': 'REF1', '1': 'TIMER'}, unit=1, footprint=footprint_so8)
    safety.place(comparator, 'U4', 'LM393', 177.8, 88.9, {'+': 'TIMER', '-': 'REF2', '7': 'TRIP_CLK'}, unit=2, footprint=footprint_so8)
    safety.place(comparator, 'U4', 'LM393', 177.8, 127, {'V+': '+3V3', 'V-': 'GND'}, unit=3, footprint=footprint_so8)
    latch = load_symbol('74xGxx', '74LVC2G74')
    safety.place(latch, 'U5', '74LVC2G74', 254, 50.8,
                 {'C': 'TRIP_CLK', 'D': '+3V3', 'Q': 'TRIPPED', '~{Q}': 'TRIP_N', '~{CLR}': 'CLR_N', '~{PRE}': 'PRE_N',
                  'GND': 'GND', 'VCC': '+3V3'}, footprint='Package_SO:VSSOP-8_2.3x2mm_P0.5mm')
    gate = load_symbol('74xGxx', '74LVC1G08')
    safety.place(gate, 'U6', '74LVC1G08', 254, 101.6,
                 {'1': 'ENABLE', '2': 'TRIP_N', '4': 'NSLEEP', 'GND': 'GND', 'VCC': '+3V3'},
                 footprint='Package_TO_SOT_SMD:SOT-23-5')
    safety.passive(res, 'R1', '47k', 'VIN', 'VM_GATE', footprint=R0603)
    safety.passive(res, 'R2', '47k', 'VM_GATE', 'VM_GATE_N', footprint=R0603)
    safety.passive(cap, 'C10', '100nF 50V', 'VIN', 'VM_GATE', footprint=C0603)
    safety.passive(cap_pol, 'C11', '470uF 35V', 'VM_SW', 'GND', footprint='Capacitor_SMD:CP_Elec_10x10.5')
    safety.passive(res, 'R3', '0.05 1W', 'VM_SW', 'VM', footprint='Resistor_SMD:R_2512_6332Metric')
    safety.passive(res, 'R4', '30k', '+3V3', 'REF1', footprint=R0603)
    safety.passive(res, 'R5', '1k', 'REF1', 'GND', footprint=R0603)
    safety.passive(res, 'R6', '22k', '+3V3', 'REF2', footprint=R0603)
    safety.passive(res, 'R7', '10k', 'REF2', 'GND', footprint=R0603)
    safety.passive(res, 'R8', '27k', '+3V3', 'TIMER', footprint=R0603)
    safety.passive(cap, 'C12', '100nF', 'TIMER', 'GND', footprint=C0603)
    safety.passive(res, 'R9', '10k', '+3V3', 'TRIP_CLK', footprint=R0603)
    safety.passive(res, 'R10', '100k', '+3V3', 'PRE_N', footprint=R0603)
    safety.passive(cap, 'C13', '100nF', 'PRE_N', 'GND', footprint=C0603)
    safety.passive(res, 'R11', '100k', '+3V3', 'CLR_N', footprint=R0603)
    safety.passive(cap, 'C14', '1nF', 'CLR_N', 'TRIP_CLEAR', footprint=C0603)
    safety.passive(schottky, 'D4', 'BAT54', '+3V3', 'CLR_N', footprint='Diode_SMD:D_SOD-323')
    safety.passive(res, 'R12', '100k', 'ENABLE', 'GND', footprint=R0603)
    safety.passive(res, 'R13', '100k', 'NSLEEP', 'GND', footprint=R0603)
    safety.passive(cap, 'C15', '100nF', '+3V3', 'GND', footprint=C0603)
    safety.passive(cap, 'C16', '100nF', '+3V3', 'GND', footprint=C0603)
    safety.passive(cap, 'C17', '100nF', '+3V3', 'GND', footprint=C0603)
    safety.passive(cap, 'C18', '100nF', '+3V3', 'GND', footprint=C0603)

    # MCU
    mcu.grid_y = 165.1
    esp = load_symbol('RF_Module', 'ESP32-S3-WROOM-1')
    mcu.place(esp, 'U7', 'ESP32-S3-WROOM-1-N8', 101.6, 76.2, {
        '3V3': '+3V3', 'GND': 'GND', 'EN': 'ESP_EN', 'IO0': 'BOOT',
        'IO12': 'SCLK0', 'IO11': 'MOSI0', 'IO13': 'MISO0', 'IO10': 'CS0',
        'IO4': 'SCLK1', 'IO5': 'MOSI1', 'IO6': 'MISO1', 'IO7': 'CS1',
        'IO15': 'ENABLE', 'IO16': 'TRIP_CLEAR', 'IO17': 'NFAULT', 'IO18': 'TRIPPED',
        'IO1': 'ISENSE', 'IO2': 'LED',
        'USB_D-': 'USB_DN', 'USB_D+': 'USB_DP',
    }, footprint='RF_Module:ESP32-S3-WROOM-1')
    usb = load_symbol('Connector', 'USB_C_Receptacle_USB2.0_16P')
    mcu.place(usb, 'J2', 'USB-C', 215.9, 76.2, {
        'VBUS': 'USB_VBUS', 'GND': 'GND', 'SHIELD': 'GND', 'CC1': 'CC1', 'CC2': 'CC2', 'D+': 'USB_DP', 'D-': 'USB_DN',
    }, footprint='Connector_USB:USB_C_Receptacle_HRO_TYPE-C-31-M-12')
    esd = load_symbol('Power_Protection', 'USBLC6-2SC6')
    mcu.place(esd, 'U8', 'USBLC6-2SC6', 292.1, 76.2,
              {'1': 'USB_DP', '6': 'USB_DP', '3': 'USB_DN', '4': 'USB_DN', 'VBUS': 'USB_VBUS', 'GND': 'GND'},
              footprint='Package_TO_SOT_SMD:SOT-23-6')
    mcu.passive(res, 'R20', '10k', '+3V3', 'ESP_EN', footprint=R0603)
    mcu.passive(cap, 'C20', '1uF', 'ESP_EN', 'GND', footprint=C0603)
    mcu.passive(button, 'SW1', 'RESET', 'ESP_EN', 'GND', footprint='Button_Switch_SMD:SW_SPST_TL3342')
    mcu.passive(button, 'SW2', 'BOOT', 'BOOT', 'GND', footprint='Button_Switch_SMD:SW_SPST_TL3342')
    mcu.passive(res, 'R21', '10k', '+3V3', 'BOOT', footprint=R0603)
    mcu.passive(res, 'R22', '5.1k', 'CC1', 'GND', footprint=R0603)
    mcu.passive(res, 'R23', '5.1k', 'CC2', 'GND', footprint=R0603)
    mcu.passive(res, 'R24', '10k', '+3V3', 'NFAULT', footprint=R0603)
    mcu.passive(res, 'R25', '1k', 'LED', 'LED_A', footprint=R0603)
    mcu.passive(led, 'D10', 'LED', 'GND', 'LED_A', footprint='LED_SMD:LED_0603_1608Metric')
    mcu.passive(cap, 'C21', '10uF', '+3V3', 'GND', footprint=C0805)
    mcu.passive(cap, 'C22', '100nF', '+3V3', 'GND', footprint=C0603)

    # Panels: 6 drivers each. Every driver output goes to exactly one connector pin
    x1 = load_symbol('Connector_Generic', 'Conn_02x17_Odd_Even')
    x23 = load_symbol('Connector_Generic', 'Conn_02x08_Odd_Even')
    header34 = 'Connector_PinHeader_2.54mm:PinHeader_2x17_P2.54mm_Vertical'
    header16 = 'Connector_PinHeader_2.54mm:PinHeader_2x08_P2.54mm_Vertical'
    for p, sheet in enumerate(panels):
        base = 100 * (p + 1)
        sheet.grid_y = 330.2
        sides = [[f'P{p}_X1_{n}' for n in range(1, 33)],
                 [f'P{p}_X2_{n}' for n in range(1, 17)] + [f'P{p}_X3_{n}' for n in range(1, 17)]]
        chips = [sides[0][0:12], sides[0][12:24], sides[0][24:32], sides[1][0:12], sides[1][12:24], sides[1][24:32]]
        for i, outputs in enumerate(chips):
            x, y = 76.2 + (i % 3) * 114.3, 63.5 + (i // 3) * 76.2
            conns = {'VM': 'VM', 'VDD': '+3V3', 'GND': 'GND', 'EP': 'GND', 'SCLK': f'SCLK{p}', 'nSCS': f'CS{p}',
                     'nSLEEP': 'NSLEEP', 'nFAULT': 'NFAULT',
                     'SDI': f'MOSI{p}' if i == 0 else f'CHAIN{p}_{i}',
                     'SDO': f'MISO{p}' if i == 5 else f'CHAIN{p}_{i + 1}'}
            for n, net in enumerate(outputs):
                conns[f'OUT{n + 1}'] = net
            sheet.place(drv, f'U{base + i + 1}', 'DRV8912-Q1', x, y, conns, footprint=DRV_FOOTPRINT)
        pins = {str(n): f'P{p}_X1_{n}' for n in range(1, 33)}
        sheet.place(x1, f'J{base + 1}', 'X1', 76.2, 241.3, pins, footprint=header34)
        sheet.place(x23, f'J{base + 2}', 'X2', 190.5, 241.3, {str(n): f'P{p}_X2_{n}' for n in range(1, 17)}, footprint=header16)
        sheet.place(x23, f'J{base + 3}', 'X3', 304.8, 241.3, {str(n): f'P{p}_X3_{n}' for n in range(1, 17)}, footprint=header16)
        for i in range(6):
            n = base + i * 4
            sheet.passive(cap, f'C{n + 1}', '100nF 50V', 'VM', 'GND', footprint=C0603)
            sheet.passive(cap, f'C{n + 2}', '100nF 50V', 'VM', 'GND', footprint=C0603)
            sheet.passive(cap, f'C{n + 3}', '100nF', '+3V3', 'GND', footprint=C0603)
            sheet.passive(cap, f'C{n + 4}', '1uF', '+3V3', 'GND', footprint=C0603)

    return [power, safety, mcu] + panels, drv_raw


def text_effects(justify=''):
    return f'(effects (font (size 1.27 1.27)){justify})'


def write_sheet(sheet, nets_elsewhere, sheet_path, library_version, flags, counters):
    out = [f'(kicad_sch (version {library_version}) (generator "dotflipper2") (generator_version "10.0")',
           f'(uuid "{uid("sheet", sheet.name)}")', '(paper "A2")' if sheet.name.startswith('panel') else '(paper "A3")',
           f'(title_block (title "{sheet.title}") (company "Dotflipper v2"))', '(lib_symbols']
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


def main():
    os.makedirs(OUT, exist_ok=True)
    version = re.search(r'\(version (\d+)\)', library_text('power')).group(1)
    sheets, drv_raw = build()

    # Nets on more than one sheet need global labels
    used = {}
    for sheet in sheets:
        for item in sheet.items:
            if item[0] == 'net' and item[1] is not None:
                used.setdefault(item[1], set()).add(sheet.name)
    shared = {net for net, names in used.items() if len(names) > 1}
    flags = {'power': ['GND', '+5V', 'VIN', 'USB_VBUS'], 'safety': ['VM']}
    shared.update(('VIN', 'USB_VBUS', 'VM'))

    root = uid('sheet', 'root')
    counters = {'power': 0, 'flag': 0}
    blocks = []
    for page, sheet in enumerate(sheets, start=2):
        block = uid('block', sheet.name)
        write_sheet(sheet, shared, f'/{root}/{block}', version, flags.get(sheet.name, []), counters)
        x, y = 50.8 + (page - 2) * 63.5, 50.8
        blocks.append(f'(sheet (at {x} {y}) (size 50.8 25.4) (exclude_from_sim no) (in_bom yes) (on_board yes) (dnp no) '
                      f'(stroke (width 0.1524) (type solid)) (fill (color 0 0 0 0.0)) (uuid "{block}")\n'
                      f'(property "Sheetname" "{sheet.title}" (at {x} {y - 1.27} 0) {text_effects(" (justify left bottom)")})\n'
                      f'(property "Sheetfile" "{sheet.name}.kicad_sch" (at {x} {y + 26.67} 0) {text_effects(" (justify left top)")})\n'
                      f'(instances (project "{PROJECT}" (path "/{root}" (page "{page}")))))')
    with open(os.path.join(OUT, PROJECT + '.kicad_sch'), 'w') as f:
        f.write(f'(kicad_sch (version {version}) (generator "dotflipper2") (generator_version "10.0")\n'
                f'(uuid "{root}") (paper "A3") (title_block (title "Dotflipper v2"))\n(lib_symbols)\n'
                + '\n'.join(blocks) + '\n(sheet_instances (path "/" (page "1")))\n)\n')
    with open(os.path.join(OUT, PROJECT + '.kicad_sym'), 'w') as f:
        f.write(f'(kicad_symbol_lib (version {version}) (generator "dotflipper2") (generator_version "10.0")\n{drv_raw}\n)\n')
    with open(os.path.join(OUT, 'sym-lib-table'), 'w') as f:
        f.write(f'(sym_lib_table (version 7)\n(lib (name "{PROJECT}")(type "KiCad")(uri "${{KIPRJMOD}}/{PROJECT}.kicad_sym")(options "")(descr ""))\n)\n')
    with open(os.path.join(OUT, PROJECT + '.kicad_pro'), 'w') as f:
        f.write('{"meta": {"filename": "%s.kicad_pro", "version": 3}}\n' % PROJECT)
    print('Klaar:', ', '.join(s.name for s in sheets))


if __name__ == '__main__':
    main()
