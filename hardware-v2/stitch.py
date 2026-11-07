#!/usr/bin/env python3
"""Connects what the router leaves to the planes: python3 stitch.py <board.kicad_pcb>

The router skips the nets that have a plane. This adds a via to the plane for every open connection
of those nets, in a spot with room for it. Repeats until nothing changes.
"""

import json
import math
import os
import subprocess
import sys
import tempfile

import pcbnew

MM = pcbnew.FromMM
TO = pcbnew.ToMM
CLEARANCE = 0.17
VIA_SIZE, VIA_DRILL, STUB = 0.6, 0.3, 0.25
PLANE_NETS = ('GND', 'VM', '+3V3')

path = sys.argv[1]
keepouts = []


def segment_distance(px, py, ax, ay, bx, by):
    dx, dy = bx - ax, by - ay
    length = dx * dx + dy * dy
    t = 0 if length == 0 else max(0, min(1, ((px - ax) * dx + (py - ay) * dy) / length))
    return math.hypot(px - ax - t * dx, py - ay - t * dy)


class Obstacles:
    def __init__(self, board):
        self.tracks, self.pads = [], []
        for t in board.GetTracks():
            if t.Type() == pcbnew.PCB_TRACE_T:
                s, e = t.GetStart(), t.GetEnd()
                self.tracks.append((t.GetNetname(), t.GetLayer(), TO(s.x), TO(s.y), TO(e.x), TO(e.y), TO(t.GetWidth()) / 2))
            elif t.Type() == pcbnew.PCB_VIA_T:
                p = t.GetPosition()
                self.tracks.append((t.GetNetname(), None, TO(p.x), TO(p.y), TO(p.x), TO(p.y), TO(t.GetWidth(pcbnew.F_Cu)) / 2))
        for f in board.GetFootprints():
            for pad in f.Pads():
                box = pad.GetBoundingBox()
                self.pads.append((pad.GetNetname(), pad.IsOnLayer(pcbnew.F_Cu), pad.IsOnLayer(pcbnew.B_Cu),
                                  TO(box.GetLeft()), TO(box.GetTop()), TO(box.GetRight()), TO(box.GetBottom())))
            for zone in f.Zones():
                box = zone.GetBoundingBox()
                keepouts.append((TO(box.GetLeft()) - 1, TO(box.GetTop()) - 1, TO(box.GetRight()) + 1, TO(box.GetBottom()) + 1))

    def point_free(self, net, x, y, radius, layer=None):
        """radius includes the clearance. layer None means all layers, for a via."""
        for name, track_layer, ax, ay, bx, by, half in self.tracks:
            if name != net and (layer is None or track_layer is None or track_layer == layer):
                if segment_distance(x, y, ax, ay, bx, by) < radius + half:
                    return False
        for name, on_front, on_back, left, top, right, bottom in self.pads:
            if name == net:
                continue
            if layer == pcbnew.F_Cu and not on_front or layer == pcbnew.B_Cu and not on_back:
                continue
            dx, dy = max(left - x, 0, x - right), max(top - y, 0, y - bottom)
            if math.hypot(dx, dy) < radius:
                return False
        return True

    def find(self, net, x, y, layer, board_w, board_h):
        via_r = VIA_SIZE / 2 + CLEARANCE
        for r10 in range(5, 45, 2):
            r = r10 / 10
            for step in range(16):
                a = step * math.pi / 8
                cx, cy = x + r * math.cos(a), y + r * math.sin(a)
                if not (1 < cx < board_w - 1 and 1 < cy < board_h - 1):
                    continue
                if any(l < cx < rr and t < cy < b for l, t, rr, b in keepouts):
                    continue
                if not self.point_free(net, cx, cy, via_r):
                    continue
                if any(n == net and l is None and math.hypot(cx - ax, cy - ay) < 0.7 for n, l, ax, ay, _, _, _ in self.tracks):
                    continue
                steps = max(2, int(r / 0.1))
                if all(self.point_free(net, x + (cx - x) * i / steps, y + (cy - y) * i / steps, STUB / 2 + CLEARANCE, layer)
                       for i in range(steps + 1)):
                    return cx, cy
        return None


def add_via(board, net, x, y, cx, cy, layer):
    via = pcbnew.PCB_VIA(board)
    via.SetPosition(pcbnew.VECTOR2I(MM(cx), MM(cy)))
    via.SetDrill(MM(VIA_DRILL))
    via.SetWidth(MM(VIA_SIZE))
    via.SetNet(board.FindNet(net))
    board.Add(via)
    track = pcbnew.PCB_TRACK(board)
    track.SetStart(pcbnew.VECTOR2I(MM(x), MM(y)))
    track.SetEnd(pcbnew.VECTOR2I(MM(cx), MM(cy)))
    track.SetWidth(MM(STUB))
    track.SetLayer(layer)
    track.SetNet(board.FindNet(net))
    board.Add(track)


def unconnected():
    report = tempfile.mktemp(suffix='.json')
    subprocess.run(['kicad-cli', 'pcb', 'drc', '--format', 'json', '--severity-all', '-o', report, path],
                   capture_output=True, check=False)
    data = json.load(open(report))
    os.remove(report)
    return data['unconnected_items']


def main():
    board = pcbnew.LoadBoard(path)
    pcbnew.ZONE_FILLER(board).Fill(board.Zones())
    box = board.GetBoardEdgesBoundingBox()
    width, height = TO(box.GetWidth()), TO(box.GetHeight())
    pcbnew.SaveBoard(path, board)

    for attempt in range(6):
        board = pcbnew.LoadBoard(path)
        keepouts.clear()
        obstacles = Obstacles(board)
        items = unconnected()
        added, skipped = 0, 0
        done = []
        for violation in items:
            for item in violation['items']:
                text = item['description']
                net = next((n for n in PLANE_NETS if f'[{n}]' in text), None)
                # A through hole pad is on the plane already
                if net is None or not text.startswith('Pad'):
                    continue
                x, y = item['pos']['x'], item['pos']['y']
                if any(math.hypot(x - dx, y - dy) < 0.8 for dx, dy in done):
                    continue
                layer = pcbnew.B_Cu if 'B.Cu' in text else pcbnew.F_Cu
                spot = obstacles.find(net, x, y, layer, width, height)
                if spot is None:
                    skipped += 1
                    continue
                add_via(board, net, x, y, spot[0], spot[1], layer)
                obstacles.tracks.append((net, None, spot[0], spot[1], spot[0], spot[1], VIA_SIZE / 2))
                done.append((x, y))
                added += 1
        print(f'Ronde {attempt + 1}: {len(items)} open, {added} vias gezet, {skipped} geen plek')
        if not added:
            break
        pcbnew.ZONE_FILLER(board).Fill(board.Zones())
        pcbnew.SaveBoard(path, board)


main()
