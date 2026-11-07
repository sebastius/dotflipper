#!/usr/bin/env python3
"""Routes a board placed by pcb.py with Freerouting: python3 route.py <board.kicad_pcb> [passes]

Needs ~/.local/share/freerouting/freerouting-2.5.0.jar and Java. Keeps the unrouted board as *.unrouted.
"""

import os
import re
import shutil
import subprocess
import sys

import pcbnew

import fanout

PLANE_NETS = ('GND', 'VM', '+3V3')  # These go to their plane, see stitch.py
JAR = os.path.expanduser('~/.local/share/freerouting/freerouting-2.5.0.jar')
path = sys.argv[1]
passes = sys.argv[2] if len(sys.argv) > 2 else '40'
base = os.path.splitext(path)[0]


def block_end(text, start):
    depth, i = 0, start
    while True:
        depth += {'(': 1, ')': -1}.get(text[i], 0)
        i += 1
        if depth == 0:
            return i


def without_plane_nets(dsn):
    for name in PLANE_NETS:
        match = re.search(r'\(net %s\s' % re.escape(name), dsn)
        if match:
            dsn = dsn[:match.start()] + f'(net {name})' + dsn[block_end(dsn, match.start()):]
    # Planes on the inner layers stay: they keep the router out. The pours on the outer layers go.
    for layer in ('F.Cu', 'B.Cu'):
        while True:
            match = re.search(r'\(plane \S+ \(polygon %s' % re.escape(layer), dsn)
            if not match:
                break
            dsn = dsn[:match.start()] + dsn[block_end(dsn, match.start()):]
    plane = dsn.find('(plane ')
    return dsn[:plane] + fanout.keepouts(board) + dsn[plane:]


board = pcbnew.LoadBoard(path)
shutil.copy(path, path + '.unrouted')
assert pcbnew.ExportSpecctraDSN(board, base + '.dsn')
dsn = without_plane_nets(open(base + '.dsn').read())
open(base + '.dsn', 'w').write(dsn)
subprocess.run(['java', '-jar', JAR, '--gui.enabled=false', '-de', base + '.dsn', '-do', base + '.ses', '-mp', passes], check=True,
               stdout=open(base + '.route.log', 'w'), stderr=subprocess.STDOUT)
assert pcbnew.ImportSpecctraSES(board, base + '.ses')
fanout.add(board)
pcbnew.ZONE_FILLER(board).Fill(board.Zones())
pcbnew.SaveBoard(path, board)
print('Klaar', path)
