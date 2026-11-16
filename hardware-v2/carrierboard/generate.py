#!/usr/bin/env python3
"""Makes the schematic and the placed, unrouted board of the carrier board.

    python3 generate.py

Overwrites carrierboard.kicad_sch and carrierboard.kicad_pcb: edits made in KiCad are lost, so run
this only to start over. One flipdot panel: X1, X2, X3 and six DRV8912 breakouts in one SPI chain.
"""

import os
import re
import subprocess
import sys
import tempfile

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, os.path.dirname(HERE))
import generate as schematic  # noqa: E402  The helpers of the first design

import pcbnew  # noqa: E402

NAME = 'carrierboard'
WIDTH, HEIGHT = 170, 74
MM = pcbnew.FromMM

R0603 = 'Resistor_SMD:R_0603_1608Metric'
C0603 = 'Capacitor_SMD:C_0603_1608Metric'
C1206 = 'Capacitor_SMD:C_1206_3216Metric'
HEADER = 'Connector_IDC:IDC-Header_2x%02d_P2.54mm_Vertical'  # Box header: key on the odd pins, as on v1
MODULES = ['U1', 'U2', 'U3', 'U4', 'U5', 'U6']  # In order of the SPI chain

# Checked in the JLCPCB parts library. Basic parts where one exists
LCSC = {
    'C1': 'C46550413', 'C2': 'C15849', 'C3': 'C89632', 'C4': 'C14663', 'C5': 'C12891', 'C6': 'C12891',
    'D1': 'C2480', 'J1': 'C5246813', 'L1': 'C305174', 'R1': 'C25808', 'R2': 'C25804', 'R3': 'C25804',
    'U7': 'C42459160', 'U8': 'C2071056',
}
# Bought and soldered by hand: not in the BOM or the position file for assembly
BY_HAND = ['A1', 'X1', 'X2', 'X3', 'J2', 'J3', 'J4', 'J5', 'JP1'] + MODULES

# Pico GPIOs: SPI1 on the side that faces the drivers
PICO = {'GPIO12': 'MISO', 'GPIO13': 'nSCS', 'GPIO14': 'SCLK', 'GPIO15': 'MOSI',
        'GPIO11': 'nSLEEP', 'GPIO10': 'nFAULT', 'GPIO9': 'PD_nGOOD',
        'VSYS': 'VSYS', '3V3': '+3V3', 'GND': 'GND', 'AGND': 'GND'}


def outputs():
    """Panel wires per module, in the order of the chain: OUT1 of U1 is pin 1 of X1 and so on.

    After X1 the chain takes X3 and then X2, so wire n of those two is row (n - 1) / 2 of the panel.
    Plus and minus of a row are neighbours: 12 outputs per module keeps them on one driver.
    """
    x1 = [f'X1_{n}' for n in range(1, 33)]
    rows = [f'X3_{n}' for n in range(1, 17)] + [f'X2_{n}' for n in range(1, 17)]
    return [dict(enumerate(wires, start=1)) for wires in
            (x1[0:12], x1[12:24], x1[24:32], rows[0:12], rows[12:24], rows[24:32])]


def module_symbol():
    text = open(os.path.join(HERE, NAME + '.kicad_sym')).read()
    start = text.index('(symbol "DRV8912_Breakout"')
    return schematic.Symbol(NAME, 'DRV8912_Breakout', schematic.balanced(text, start))


def build():
    device = lambda name: schematic.load_symbol('Device', name)
    res, cap, cap_pol = device('R'), device('C'), device('C_Polarized')
    sheet = schematic.Sheet(NAME, 'Carrierboard')
    sheet.grid_y = 330.2

    module = module_symbol()
    wires = outputs()
    for i, ref in enumerate(MODULES):
        conns = {'1': 'VM', '2': '+3V3', '3': 'MOSI' if i == 0 else f'CHAIN{i}',
                 '4': 'CHAIN_END' if i == 5 else f'CHAIN{i + 1}', '5': 'nSLEEP', '6': 'nFAULT', '7': 'nSCS', '8': 'SCLK',
                 '9': 'GND', '10': 'GND'}
        for n, net in wires[i].items():
            conns[str(10 + n)] = net
        sheet.place(module, ref, 'DRV8912_Breakout', 63.5 + (i % 3) * 101.6, 63.5 + (i // 3) * 88.9, conns,
                    footprint=f'{NAME}:DRV8912_Breakout_ChipDown')

    x1 = schematic.load_symbol('Connector_Generic', 'Conn_02x17_Odd_Even')
    x23 = schematic.load_symbol('Connector_Generic', 'Conn_02x08_Odd_Even')
    sheet.place(x1, 'X1', 'X1', 381, 76.2, {str(n): f'X1_{n}' for n in range(1, 33)}, footprint=HEADER % 17)
    sheet.place(x23, 'X2', 'X2', 381, 165.1, {str(n): f'X2_{n}' for n in range(1, 17)}, footprint=HEADER % 8)
    sheet.place(x23, 'X3', 'X3', 482.6, 165.1, {str(n): f'X3_{n}' for n in range(1, 17)}, footprint=HEADER % 8)

    pico = schematic.load_symbol('MCU_Module', 'RaspberryPi_Pico')
    sheet.place(pico, 'A1', 'Raspberry Pi Pico 2 W', 88.9, 241.3, PICO, footprint=f'{NAME}:RaspberryPi_Pico_W_THT')

    usb = schematic.load_symbol('Connector', 'USB_C_Receptacle_PowerOnly_6P')
    sheet.place(usb, 'J1', 'USB-C', 203.2, 228.6,
                {'A9': 'VM', 'B9': 'VM', 'A5': 'CC1', 'B5': 'CC2', 'A12': 'GND', 'B12': 'GND', 'SH': 'GND'},
                footprint='Connector_USB:USB_C_Receptacle_GCT_USB4125-xx-x-0190_6P_TopMnt_Horizontal')
    # CH224A on the CH224K symbol, same pins, but pin 1 is VHV (32 V) and takes VBUS directly.
    # 120k from CFG1 to GND asks for 20 V, CFG2 and CFG3 stay open
    pd = schematic.load_symbol('Interface_USB', 'CH224K')
    sheet.place(pd, 'U7', 'CH224A', 304.8, 228.6,
                {'VDD': 'VM', 'CFG1': 'PD_CFG1', 'CC1': 'CC1', 'CC2': 'CC2', 'VBUS': 'VM', 'PG': 'PD_nGOOD',
                 'GND': 'GND'},
                footprint='Package_SO:SSOP-10-1EP_3.9x4.9mm_P1mm_EP2.1x3.3mm')
    buck = schematic.load_symbol('Regulator_Switching', 'AP63205WU')
    sheet.place(buck, 'U8', 'AP63205WU', 406.4, 241.3,
                {'IN': 'VM', 'EN': 'VM', 'GND': 'GND', 'FB': '+5V', 'BST': 'BST', 'SW': 'SW'},
                footprint='Package_TO_SOT_SMD:TSOT-23-6')

    sheet.passive(cap_pol, 'C1', '470uF 35V', 'VM', 'GND', footprint='Capacitor_SMD:CP_Elec_10x10.5')
    sheet.passive(res, 'R1', '120k', 'PD_CFG1', 'GND', footprint=R0603)
    sheet.passive(cap, 'C2', '1uF 50V', 'VM', 'GND', footprint=C0603)
    sheet.passive(cap, 'C3', '10uF 50V', 'VM', 'GND', footprint=C1206)
    sheet.passive(cap, 'C4', '100nF', 'BST', 'SW', footprint=C0603)
    sheet.passive(device('L'), 'L1', '4.7uH', 'SW', '+5V', footprint='Inductor_SMD:L_Sunlord_SWPA5040S')
    sheet.passive(cap, 'C5', '22uF 25V', '+5V', 'GND', footprint=C1206)
    sheet.passive(cap, 'C6', '22uF 25V', '+5V', 'GND', footprint=C1206)
    # Keeps the 5 V from the Pico's own USB port out of the regulator, see the Pico datasheet
    sheet.passive(device('D_Schottky'), 'D1', 'SS14', 'VSYS', '+5V', footprint='Diode_SMD:D_SMA')
    sheet.passive(res, 'R2', '10k', '+3V3', 'nFAULT', footprint=R0603)
    sheet.passive(res, 'R3', '10k', '+3V3', 'PD_nGOOD', footprint=R0603)

    # Link to a second board without a Pico: the 20 V over J4 and J5, one in and one out, the signals
    # over a straight 10 way cable.
    # The chain leaves on pin 1 of J2 and comes back on pin 6. Without a second board JP1 closes the chain
    link = schematic.load_symbol('Connector_Generic', 'Conn_02x05_Odd_Even')
    shared = {'2': 'GND', '3': 'SCLK', '4': 'GND', '5': 'nSCS', '7': 'nSLEEP', '8': 'nFAULT', '9': '+3V3', '10': 'GND'}
    sheet.place(link, 'J2', 'Naar bord 2', 482.6, 228.6, {**shared, '1': 'CHAIN_END', '6': 'MISO'}, footprint=HEADER % 5)
    sheet.place(link, 'J3', 'Van bord 1', 482.6, 266.7, {**shared, '1': 'MOSI', '6': 'CHAIN_END'}, footprint=HEADER % 5)
    terminal = schematic.load_symbol('Connector_Generic', 'Conn_01x02')
    for ref, value in (('J4', '20V in'), ('J5', '20V door')):
        sheet.passive(terminal, ref, value, 'VM', 'GND',
                      footprint='TerminalBlock_Phoenix:TerminalBlock_Phoenix_MKDS-1,5-2-5.08_1x02_P5.08mm_Horizontal')
    sheet.passive(schematic.load_symbol('Jumper', 'SolderJumper_2_Open'), 'JP1', 'Sluit bij 1 bord', 'CHAIN_END', 'MISO',
                  footprint='Jumper:SolderJumper-2_P1.3mm_Open_TrianglePad1.0x1.5mm')
    return sheet


def write_schematic(sheet):
    schematic.PROJECT, schematic.OUT = NAME, HERE
    version = re.search(r'\(version (\d+)\)', schematic.library_text('power')).group(1)
    root = schematic.uid('sheet', NAME)
    counters = {'power': 0, 'flag': 0}
    schematic.write_sheet(sheet, {'VM', 'VSYS'}, f'/{root}', version, ['GND', 'VM', '+5V', 'VSYS'], counters)
    path = os.path.join(HERE, NAME + '.kicad_sch')
    text = open(path).read().replace('(paper "A3")', '(paper "A2")')
    text = text.rstrip()[:-1] + '(sheet_instances (path "/" (page "1")))\n)\n'
    for ref in BY_HAND:
        text, count = re.subn(r'\(in_bom yes\) \(on_board yes\)( \(dnp no\) \(uuid "[^"]*"\)\n\(property "Reference" "%s")' % ref,
                              r'(in_bom no) (on_board yes) (in_pos_files no)\1', text)
        assert count == 1, ref
    for ref in LCSC:
        match = re.search(r'\(property "Footprint" "[^"]*" \(at ([-\d.]+) ([-\d.]+) 0\)[^\n]*\n(?=\(property "Datasheet" "" [^\n]*\n(?:\(pin [^\n]*\n)+\(instances \(project "%s" \(path "[^"]*" \(reference "%s"\))' % (NAME, ref), text)
        assert match, ref
        extra = (f'(property "LCSC" "{LCSC[ref]}" (at {match.group(1)} {match.group(2)} 0) '
                 '(effects (font (size 1.27 1.27)) (hide yes)))\n')
        text = text[:match.end()] + extra + text[match.end():]
    open(path, 'w').write(text)


def parse_netlist(path):
    text = open(path).read()
    parts = {}
    for match in re.finditer(r'\(comp\s+\(ref "(.+?)"\)(.*?)(?=\n\s*\(comp\s|\n\s*\(libparts|\Z)', text, re.S):
        body = match.group(2)
        parts[match.group(1)] = (re.search(r'\(value "(.*?)"\)', body).group(1),
                                 re.search(r'\(footprint "(.*?)"\)', body).group(1),
                                 re.findall(r'\(tstamps "(.*?)"\)', body)[-1])
    nets = {}
    for match in re.finditer(r'\(net\s+\(code "?\d+"?\)\s+\(name "(.*?)"(.*?)(?=\n\s*\(net\s|\Z)', text, re.S):
        for ref, pin in re.findall(r'\(ref "(.*?)"\)\s+\(pin "(.*?)"', match.group(2)):
            nets[(ref, pin)] = match.group(1)
    return parts, nets


def load(entry):
    library, name = entry.split(':')
    folder = os.path.join(HERE, NAME + '.pretty') if library == NAME else f'/usr/share/kicad/footprints/{library}.pretty'
    return pcbnew.FootprintLoad(folder, name)


# As on v1 the headers sit along one edge, 82.55 and 33.02 mm apart, key to the edge and pin 1 on the
# right. The modules hang chip down under their part of a header, which mirrors them: their OUT1-12
# then run in the direction of the pins. The Pico sits between the two groups with the antenna at
# the same edge. All J1s end up in one row, so one bus along them feeds everything.
TOP = 13
MODULE_LEFT = {'U3': 3.5, 'U2': 24, 'U1': 44.5, 'U6': 103, 'U5': 123.5, 'U4': 144}
PLACES = {ref: (left + 16.33, TOP + 29.525, 270) for ref, left in MODULE_LEFT.items()}
# Centres of the courtyards
CENTRES = {
    'X1': (33, 6.2, 270), 'X2': (115.55, 6.2, 270), 'X3': (148.57, 6.2, 270),
    'A1': (82.5, 27.1, 180),
    'J1': (13.5, 69.8, 0), 'U7': (23, 62, 0), 'C2': (17.5, 62, 90), 'R1': (28.5, 58.5, 0),
    'C1': (38, 62, 0), 'U8': (51, 61, 0), 'C3': (51, 65.5, 0), 'C4': (51, 57, 0), 'L1': (59, 61.5, 0),
    'C5': (65.5, 58.5, 90), 'C6': (65.5, 64.5, 90), 'D1': (71, 60, 90),
    'R2': (97, 49, 0), 'R3': (97, 46, 0), 'JP1': (97, 53, 0),
    'J4': (96, 64, 0), 'J5': (108, 64, 0), 'J2': (126, 65, 270), 'J3': (150, 65, 270),
}
HOLES = ((3.5, 3.5), (3.5, HEIGHT - 3.5), (WIDTH - 3.5, 3.5), (WIDTH - 3.5, HEIGHT - 3.5))


def centre(footprint, x, y, rotation):
    footprint.SetOrientationDegrees(rotation)
    box = footprint.GetCourtyard(pcbnew.F_CrtYd).BBox()
    position = footprint.GetPosition()
    footprint.SetPosition(pcbnew.VECTOR2I(position.x + MM(x) - box.Centre().x, position.y + MM(y) - box.Centre().y))


def rectangle(board, layer, left, top, right, bottom):
    shape = pcbnew.PCB_SHAPE(board)
    shape.SetShape(pcbnew.SHAPE_T_RECT)
    shape.SetStart(pcbnew.VECTOR2I(MM(left), MM(top)))
    shape.SetEnd(pcbnew.VECTOR2I(MM(right), MM(bottom)))
    shape.SetLayer(layer)
    shape.SetWidth(MM(0.1))
    board.Add(shape)


def antenna_keepout(board):
    """No copper under the antenna, 14 x 9 mm in the datasheet, at the edge of the board."""
    zone = pcbnew.ZONE(board)
    zone.SetIsRuleArea(True)
    zone.SetDoNotAllowTracks(True)
    zone.SetDoNotAllowVias(True)
    zone.SetDoNotAllowPads(True)
    zone.SetDoNotAllowZoneFills(True)
    zone.SetDoNotAllowFootprints(False)
    zone.SetZoneName('Antenne')
    layers = pcbnew.LSET()
    layers.AddLayer(pcbnew.F_Cu)
    layers.AddLayer(pcbnew.B_Cu)
    zone.SetLayerSet(layers)
    outline = zone.Outline()
    outline.NewOutline()
    for x, y in ((75, 0), (90, 0), (90, 11), (75, 11)):
        outline.Append(MM(x), MM(y))
    board.Add(zone)


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
    for ref, (value, entry, identifier) in parts.items():
        footprint = load(entry)
        footprint.SetReference(ref)
        footprint.SetValue(value)
        footprint.SetFPID(pcbnew.LIB_ID(*entry.split(':')))
        footprint.SetPath(pcbnew.KIID_PATH('/' + identifier))
        footprint.SetSheetfile(NAME + '.kicad_sch')
        footprint.SetSheetname('Root')
        footprint.SetField('Datasheet', '')
        footprint.SetField('Description', '')
        if ref in LCSC:
            footprint.SetField('LCSC', LCSC[ref])
            footprint.GetField('LCSC').SetVisible(False)
        if ref in BY_HAND:
            footprint.SetExcludedFromBOM(True)
            footprint.SetExcludedFromPosFiles(True)
        board.Add(footprint)
        numbers = {pad.GetNumber() for pad in footprint.Pads()}
        for pad in footprint.Pads():
            if (ref, pad.GetNumber()) in nets:
                pad.SetNet(infos[nets[(ref, pad.GetNumber())]])
        missing = [pin for other, pin in nets if other == ref and pin not in numbers]
        assert not missing, (ref, missing)
        if ref in PLACES:
            x, y, rotation = PLACES[ref]
            footprint.SetOrientationDegrees(rotation)
            footprint.SetPosition(pcbnew.VECTOR2I(MM(x), MM(y)))
            # Inside the outline: the gap between two rows is too small for the text
            box = footprint.GetCourtyard(pcbnew.F_CrtYd).BBox()
            footprint.Reference().SetPosition(pcbnew.VECTOR2I(box.Centre().x, box.Centre().y))
        else:
            centre(footprint, *CENTRES[ref])
        if ref == 'A1':
            box = footprint.GetCourtyard(pcbnew.F_CrtYd).BBox()
            footprint.Reference().SetPosition(pcbnew.VECTOR2I(box.Centre().x, box.GetBottom() + MM(1.5)))
    for i, (x, y) in enumerate(HOLES, start=1):
        hole = load('MountingHole:MountingHole_3.2mm_M3')
        hole.SetReference(f'H{i}')
        hole.SetBoardOnly(True)
        hole.SetPosition(pcbnew.VECTOR2I(MM(x), MM(y)))
        board.Add(hole)
    rectangle(board, pcbnew.Edge_Cuts, 0, 0, WIDTH, HEIGHT)
    antenna_keepout(board)
    pcbnew.SaveBoard(os.path.join(HERE, NAME + '.kicad_pcb'), board, True)


if __name__ == '__main__':
    # Never write over a schematic or board that KiCad has open: its next save would undo this, or the other way round
    locks = [f for f in os.listdir(HERE) if f.startswith('~') and f.endswith(('.kicad_sch.lck', '.kicad_pcb.lck'))]
    if locks:
        sys.exit('KiCad heeft ' + ', '.join(locks) + ' open: sluit de schema- en PCB-editor eerst.')
    write_schematic(build())
    write_board()
    with open(os.path.join(HERE, 'uitgangen.csv'), 'w') as f:
        f.write('module,uitgang,draad\n')
        for ref, wires in zip(MODULES, outputs()):
            for n in sorted(wires):
                f.write(f'{ref},OUT{n},{wires[n]}\n')
    print('Klaar')
