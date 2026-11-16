#!/usr/bin/env python3
"""Makes everything JLCPCB needs for the v3 board: python3 export_jlc.py [--kostencheck]

Writes to jlcpcb/: a zip with Gerbers and drill files, the BOM and the placement file. Parts that are
excluded from the BOM and position file in KiCad, the through hole parts, stay out of both.

With --kostencheck it skips the DRC check and writes to jlcpcb-kostencheck/ instead: only for a quote,
not for an order, the board may not be routed yet.
"""

import csv
import glob
import json
import os
import re
import shutil
import subprocess
import tempfile
import sys
import zipfile

HERE = os.path.dirname(os.path.abspath(__file__))
BOARD = os.path.join(HERE, 'v3.kicad_pcb')
QUOTE = '--kostencheck' in sys.argv
OUT = os.path.join(HERE, 'jlcpcb-kostencheck' if QUOTE else 'jlcpcb')
LAYERS = 'F.Cu,B.Cu,F.Paste,B.Paste,F.SilkS,B.SilkS,F.Mask,B.Mask,Edge.Cuts'


def cli(*args):
    subprocess.run(['kicad-cli', 'pcb', 'export', *args, BOARD], check=True, capture_output=True)


def gerbers():
    folder = tempfile.mkdtemp()
    cli('gerbers', '--layers', LAYERS, '--subtract-soldermask', '--no-x2', '--output', folder + '/')
    cli('drill', '--format', 'excellon', '--excellon-units', 'mm', '--excellon-separate-th',
        '--excellon-oval-format', 'alternate', '--output', folder + '/')
    with zipfile.ZipFile(os.path.join(OUT, 'v3-gerbers.zip'), 'w', zipfile.ZIP_DEFLATED) as archive:
        for path in sorted(glob.glob(folder + '/*')):
            archive.write(path, os.path.basename(path))
    shutil.rmtree(folder)


def parts():
    """Reference, value, footprint and LCSC number from the board, without the excluded parts."""
    text = open(BOARD).read()
    found = {}
    for block in re.split(r'\n\t\(footprint ', text)[1:]:
        head = block[:block.find('(pad ')] if '(pad ' in block else block
        if 'exclude_from_bom' in head or 'board_only' in head:
            continue
        ref = re.search(r'\(property "Reference" "(.*?)"', block).group(1)
        value = re.search(r'\(property "Value" "(.*?)"', block).group(1)
        code = re.search(r'\(property "LCSC" "(.*?)"', block)
        footprint = re.match(r'"(?:.*?:)?(.*?)"', block).group(1)
        assert code, f'{ref} heeft geen LCSC-nummer'
        found[ref] = (value, footprint, code.group(1))
    return found


def bom(found):
    groups = {}
    for ref, (value, footprint, code) in found.items():
        groups.setdefault((value, footprint, code), []).append(ref)
    with open(os.path.join(OUT, 'v3-bom.csv'), 'w', newline='') as f:
        writer = csv.writer(f)
        writer.writerow(['Comment', 'Designator', 'Footprint', 'LCSC Part #'])
        for (value, footprint, code), refs in sorted(groups.items(), key=lambda item: item[1]):
            writer.writerow([value, ','.join(sorted(refs)), footprint, code])


# JLCPCB draws some packages a quarter turn off from KiCad. Checked in the JLCPCB preview on 2026-11-16:
# every IC and resistor array had to turn 90 degrees clockwise. Both count rotation counterclockwise, so
# clockwise is minus 90. U29, the 74LVC1G79 in SOT-23-5, needed a half turn: one more quarter clockwise.
ROTATION_FIX = ((re.compile(r'U29$'), -180), (re.compile(r'(U|RN)\d+$'), -90))


def placement(found):
    path = tempfile.mktemp(suffix='.csv')
    cli('pos', '--format', 'csv', '--units', 'mm', '--side', 'both', '--output', path)
    with open(path) as source, open(os.path.join(OUT, 'v3-cpl.csv'), 'w', newline='') as f:
        writer = csv.writer(f)
        writer.writerow(['Designator', 'Mid X', 'Mid Y', 'Layer', 'Rotation'])
        rows = list(csv.DictReader(source))
        rows = [row for row in rows if row['Ref'] in found]
        assert {row['Ref'] for row in rows} == set(found), 'BOM en positiebestand verschillen'
        for row in rows:
            writer.writerow([row['Ref'], f"{float(row['PosX']):.4f}mm", f"{float(row['PosY']):.4f}mm",
                             row['Side'].capitalize(), f"{rotation(row['Ref'], float(row['Rot'])) % 360:.0f}"])
    os.remove(path)


def rotation(ref, angle):
    for pattern, fix in ROTATION_FIX:
        if pattern.match(ref):
            return angle + fix
    return angle


def checked():
    """Refuses to make production files from a board with DRC errors or open connections."""
    report = tempfile.mktemp(suffix='.json')
    subprocess.run(['kicad-cli', 'pcb', 'drc', '--severity-error', '--schematic-parity', '--format', 'json', '-o', report, BOARD],
                   check=True, capture_output=True)
    data = json.load(open(report))
    os.remove(report)
    problems = data['violations'] + data['unconnected_items'] + data.get('schematic_parity', [])
    for problem in problems:
        print(problem['type'], [item['description'] for item in problem['items']])
    assert not problems, f'{len(problems)} fouten: geen productiebestanden gemaakt'


if __name__ == '__main__':
    if not QUOTE:
        checked()
    os.makedirs(OUT, exist_ok=True)
    gerbers()
    found = parts()
    bom(found)
    placement(found)
    print('Klaar:', ', '.join(sorted(os.listdir(OUT))))
