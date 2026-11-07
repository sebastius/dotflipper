#!/usr/bin/env python3
"""Places the footprints of a schematic on a new board, see ONTWERP.md.

    kicad-cli sch export netlist --format kicadsexpr -o netlist.net <schematic>
    python3 pcb.py proef|groot netlist.net

Only makes the board: footprints, nets, outline, planes and rules. Routing is done afterwards.
Run it before routing: it starts from an empty board.
"""

import json
import os
import re
import sys

import pcbnew

HERE = os.path.dirname(os.path.abspath(__file__))
SYSTEM = '/usr/share/kicad/footprints/'
LOCAL = os.path.join(HERE, 'footprints')
MM = pcbnew.FromMM

TEST_BOARD = sys.argv[1] == 'proef'
NAME = 'dotflipper2-proef' if TEST_BOARD else 'dotflipper2'
FOLDER = os.path.join(HERE, 'kicad-proef' if TEST_BOARD else 'kicad')
WIDTH, HEIGHT = (110, 100) if TEST_BOARD else (214, 134)
USB_ROTATION = 270

POWER_REFS = ['J1', 'F1', 'D1', 'C1', 'U1', 'C2', 'L1', 'C3', 'C4', 'D2', 'D3', 'C5', 'U2', 'C6']
SAFETY_REFS = ['C11', 'Q1', 'R1', 'R2', 'C10', 'Q2', 'R3', 'U3', 'C15', 'U4', 'C16', 'U5', 'C17', 'U6', 'C18',
               'R4', 'R5', 'R6', 'R7', 'R8', 'C12', 'R9', 'R10', 'C13', 'R11', 'C14', 'D4', 'R12', 'R13']
MCU_REFS = ['J2', 'U8', 'SW1', 'SW2', 'R20', 'C20', 'R21', 'R22', 'R23', 'R24', 'R25', 'D10', 'C21', 'C22']


def parse_netlist(path):
    text = open(path).read()
    parts = {}
    for match in re.finditer(r'\(comp\s+\(ref "(.+?)"\)(.*?)(?=\n\s*\(comp\s|\n\s*\(libparts|\Z)', text, re.S):
        body = match.group(2)
        footprint = re.search(r'\(footprint "(.*?)"\)', body)
        parts[match.group(1)] = {
            'value': re.search(r'\(value "(.*?)"\)', body).group(1),
            'footprint': footprint.group(1) if footprint else '',
            'sheet': re.search(r'\(sheetpath\s+\(names ".*?"\)\s+\(tstamps "(.*?)"\)', body).group(1),
        }
    nets = {}
    for match in re.finditer(r'\(net\s+\(code "?\d+"?\)\s+\(name "(.*?)"(.*?)(?=\n\s*\(net\s|\Z)', text, re.S):
        name = match.group(1).split('/')[-1]
        if name.startswith('unconnected-'):
            continue
        for ref, pin in re.findall(r'\(ref "(.*?)"\)\s+\(pin "(.*?)"', match.group(2)):
            nets[(ref, pin)] = name
    return parts, nets


def load(entry):
    library, name = entry.split(':')
    folder = os.path.join(LOCAL if library == 'dotflipper2' else SYSTEM, library + '.pretty')
    return pcbnew.FootprintLoad(folder, name)


class Board:
    def __init__(self):
        self.board = pcbnew.CreateEmptyBoard()
        self.board.SetCopperLayerCount(6)
        # Planes, so the router leaves them alone and connects to them with vias
        for layer in (pcbnew.In1_Cu, pcbnew.In2_Cu, pcbnew.In3_Cu, pcbnew.In4_Cu):
            self.board.SetLayerType(layer, pcbnew.LT_POWER)
        self.parts = {}
        self.netinfo = {}

    def net(self, name):
        if name not in self.netinfo:
            info = pcbnew.NETINFO_ITEM(self.board, name)
            self.board.Add(info)
            self.netinfo[name] = info
        return self.netinfo[name]

    def add(self, ref, part, nets, unmatched):
        footprint = load(part['footprint'])
        footprint.SetReference(ref)
        footprint.SetValue(part['value'])
        self.board.Add(footprint)
        names = {pad.GetNumber() for pad in footprint.Pads()}
        for pad in footprint.Pads():
            name = nets.get((ref, pad.GetNumber()))
            if name:
                pad.SetNet(self.net(name))
        for (other, pin) in nets:
            if other == ref and pin not in names:
                unmatched.append((ref, pin, nets[(other, pin)]))
        self.parts[ref] = footprint

    def box(self, ref):
        return self.parts[ref].GetCourtyard(pcbnew.F_CrtYd).BBox()

    def size(self, ref):
        box = self.box(ref)
        return pcbnew.ToMM(box.GetWidth()), pcbnew.ToMM(box.GetHeight())

    def center(self, ref, x, y, rotation=0):
        footprint = self.parts[ref]
        footprint.SetOrientationDegrees(rotation)
        box = self.box(ref)
        position = footprint.GetPosition()
        footprint.SetPosition(pcbnew.VECTOR2I(position.x + MM(x) - box.Centre().x, position.y + MM(y) - box.Centre().y))

    def pack(self, refs, x, y, width, gap=1.0):
        """Rows from left to right, returns the y below the last row."""
        cursor, row_height = x, 0
        for ref in refs:
            w, h = self.size(ref)
            if cursor + w > x + width and cursor > x:
                cursor, y, row_height = x, y + row_height + gap, 0
            self.center(ref, cursor + w / 2, y + h / 2)
            cursor += w + gap
            row_height = max(row_height, h)
        return y + row_height

    def outline(self):
        shape = pcbnew.PCB_SHAPE(self.board)
        shape.SetShape(pcbnew.SHAPE_T_RECT)
        shape.SetStart(pcbnew.VECTOR2I(0, 0))
        shape.SetEnd(pcbnew.VECTOR2I(MM(WIDTH), MM(HEIGHT)))
        shape.SetLayer(pcbnew.Edge_Cuts)
        shape.SetWidth(MM(0.1))
        self.board.Add(shape)

    def plane(self, net, layer):
        zone = pcbnew.ZONE(self.board)
        zone.SetLayer(layer)
        zone.SetNet(self.net(net))
        zone.SetZoneName(net)
        outline = zone.Outline()
        outline.NewOutline()
        for x, y in ((0.3, 0.3), (WIDTH - 0.3, 0.3), (WIDTH - 0.3, HEIGHT - 0.3), (0.3, HEIGHT - 0.3)):
            outline.Append(MM(x), MM(y))
        zone.SetPadConnection(pcbnew.ZONE_CONNECTION_FULL)
        self.board.Add(zone)

    def hole(self, x, y, index):
        footprint = pcbnew.FootprintLoad(SYSTEM + 'MountingHole.pretty', 'MountingHole_3.2mm_M3')
        footprint.SetReference(f'H{index}')
        footprint.SetPosition(pcbnew.VECTOR2I(MM(x), MM(y)))
        self.board.Add(footprint)


def place_panel(board, base, x, y, drivers):
    """Drivers in a grid with their capacitors above them, connectors along the bottom edge."""
    for i in range(drivers):
        cx, cy = x + 16 + (i % 3) * 33, y + 9 + (i // 3) * 26
        board.center(f'U{base + i + 1}', cx, cy + 5)
        caps = [f'C{base + i * 4 + n}' for n in (1, 2, 3, 4)]
        board.pack(caps, cx - 7.5, cy - 3.2, 18)


def layout(board):
    """The antenna of the module needs a wide keepout, so the module goes at the top edge."""
    mcu_small = [ref for ref in MCU_REFS if ref not in ('J2', 'U8')]
    if TEST_BOARD:
        board.center('U7', WIDTH / 2, 6.5)
        board.pack(POWER_REFS, 7, 18, 29, gap=1.5)
        board.pack(SAFETY_REFS, 72, 18, 34, gap=2.2)
        board.pack(mcu_small, 41, 30, 28, gap=1.5)
        board.center('J2', 4.5, 56, USB_ROTATION)
        board.center('U8', 13, 56)
        board.center('U101', WIDTH / 2, 66)
        board.pack(['C101', 'C102', 'C103', 'C104', 'C190'], 44, 58, 26)
        board.pack([f'TP{n}' for n in range(1, 9)], 72, 60, 30)
        board.center('J101', 30, 90, 90)
        board.center('J103', 80, 90, 90)
    else:
        board.center('U7', WIDTH / 2, 6.5)
        board.pack(POWER_REFS, 9, 7, 72, gap=1.5)
        board.pack(SAFETY_REFS, 134, 7, 72, gap=2.2)
        board.pack(mcu_small, 86, 29, 42, gap=1.5)
        board.center('J2', 4.5, 44, USB_ROTATION)
        board.center('U8', 13, 44)
        for p, base in enumerate((100, 200)):
            x = 4 + p * 106
            place_panel(board, base, x, 56, 6)
            board.center(f'J{base + 1}', x + 28, 122, 90)
            board.center(f'J{base + 2}', x + 62, 122, 90)
            board.center(f'J{base + 3}', x + 87, 122, 90)


def project_rules():
    path = os.path.join(FOLDER, NAME + '.kicad_pro')
    project = json.load(open(path))
    clas = lambda name, track, via, drill: {
        'name': name, 'clearance': 0.15, 'track_width': track, 'via_diameter': via, 'via_drill': drill,
        'microvia_diameter': 0.3, 'microvia_drill': 0.1, 'diff_pair_gap': 0.25, 'diff_pair_width': 0.2,
        'diff_pair_via_gap': 0.25, 'wire_width': 6, 'bus_width': 12, 'line_style': 0, 'priority': 0 if name != 'Default' else 2147483647,
        'schematic_color': 'rgba(0, 0, 0, 0.000)', 'pcb_color': 'rgba(0, 0, 0, 0.000)'}
    project['net_settings'] = {
        'meta': {'version': 4},
        'classes': [clas('Default', 0.25, 0.6, 0.3), clas('Spoel', 0.3, 0.6, 0.3), clas('Voeding', 0.5, 0.8, 0.4),
                    clas('Vlak', 0.25, 0.6, 0.3)],
        'netclass_patterns': [{'netclass': 'Spoel', 'pattern': 'P?_X*'}, {'netclass': 'Vlak', 'pattern': 'GND'},
                              {'netclass': 'Vlak', 'pattern': 'VM'}, {'netclass': 'Vlak', 'pattern': '+3V3'},
                              {'netclass': 'Voeding', 'pattern': 'VIN*'},
                              {'netclass': 'Voeding', 'pattern': 'VM_SW'}, {'netclass': 'Voeding', 'pattern': '+5V'},
                              {'netclass': 'Voeding', 'pattern': '5V_BUCK'},
                              {'netclass': 'Voeding', 'pattern': 'SW'}],
    }
    rules = project.setdefault('board', {}).setdefault('design_settings', {}).setdefault('rules', {})
    rules.update({'min_through_hole_diameter': 0.2, 'min_clearance': 0.15, 'min_track_width': 0.1,
                  'min_via_diameter': 0.45, 'min_via_annular_width': 0.1, 'min_copper_edge_clearance': 0.3})
    json.dump(project, open(path, 'w'), indent=2)


def main():
    parts, nets = parse_netlist(sys.argv[2])
    board = Board()
    unmatched = []
    for ref, part in parts.items():
        if not part['footprint']:
            print('Geen footprint:', ref)
            continue
        board.add(ref, part, nets, unmatched)
    print('Niet gevonden pads:', unmatched)
    layout(board)
    board.outline()
    # No pours on the outer layers: signals would cut them into islands
    board.plane('GND', pcbnew.In1_Cu)
    board.plane('+3V3', pcbnew.In2_Cu)
    board.plane('VM', pcbnew.In3_Cu)
    board.plane('GND', pcbnew.In4_Cu)
    margin = 4
    for i, (x, y) in enumerate(((margin, margin), (WIDTH - margin, margin), (margin, HEIGHT - margin), (WIDTH - margin, HEIGHT - margin))):
        board.hole(x, y, i + 1)
    filler = pcbnew.ZONE_FILLER(board.board)
    filler.Fill(board.board.Zones())
    path = os.path.join(FOLDER, NAME + '.kicad_pcb')
    pcbnew.SaveBoard(path, board.board)
    project_rules()
    print(f'{NAME}: {len(board.parts)} onderdelen, {WIDTH} x {HEIGHT} mm, opgeslagen')


main()
