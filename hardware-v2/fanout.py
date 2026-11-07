"""Fixed connections of the drivers, made before routing because the router leaves no room for them.

GND pins of a driver go straight to the thermal pad next to them, VM and VDD get a via just outside
the pad. The small chips get a via just outside their pads for every pin on a plane.
"""

import pcbnew

MM, TO = pcbnew.FromMM, pcbnew.ToMM
VIA_PINS = ('16', '21', '6')
GND_PINS = ('1', '13', '24')
VIA_OFFSET = 1.75
SMALL_CHIPS = ('U3', 'U5', 'U6', 'U8')
PLANE_NETS = ('GND', 'VM', '+3V3')


def points(board):
    """Returns (net, x, y, end x, end y, via) for every fixed connection, in mm."""
    result = []
    for footprint in board.GetFootprints():
        center = footprint.GetPosition()
        if footprint.GetReference() in SMALL_CHIPS:
            for pad in footprint.Pads():
                if pad.GetNetname() in PLANE_NETS:
                    p = pad.GetPosition()
                    x, y = TO(p.x), TO(p.y)
                    side = 1 if p.x > center.x else -1
                    result.append((pad.GetNetname(), x, y, x + side * 1.5, y, True))
            continue
        if footprint.GetValue() != 'DRV8912-Q1':
            continue
        for pad in footprint.Pads():
            p = pad.GetPosition()
            x, y = TO(p.x), TO(p.y)
            side = 1 if p.x > center.x else -1
            if pad.GetNumber() in VIA_PINS:
                result.append((pad.GetNetname(), x, y, x + side * VIA_OFFSET, y, True))
            elif pad.GetNumber() in GND_PINS:
                result.append((pad.GetNetname(), x, y, x - side * 1.2, y, False))
    return result


def add(board):
    for net, x, y, ex, ey, via in points(board):
        info = board.FindNet(net)
        track = pcbnew.PCB_TRACK(board)
        track.SetStart(pcbnew.VECTOR2I(MM(x), MM(y)))
        track.SetEnd(pcbnew.VECTOR2I(MM(ex), MM(ey)))
        track.SetWidth(MM(0.3))
        track.SetLayer(pcbnew.F_Cu)
        track.SetNet(info)
        board.Add(track)
        if via:
            hole = pcbnew.PCB_VIA(board)
            hole.SetPosition(pcbnew.VECTOR2I(MM(ex), MM(ey)))
            hole.SetDrill(MM(0.3))
            hole.SetWidth(MM(0.6))
            hole.SetNet(info)
            board.Add(hole)


def keepouts(board):
    """Specctra keepouts, so the router stays clear of what add() will put there after routing."""
    text = ''
    for net, x, y, ex, ey, via in points(board):
        if via:
            for layer in ('F.Cu', 'B.Cu'):
                text += f'    (keepout "" (circle {layer} 1000 {ex * 1000:.0f} {-ey * 1000:.0f}))\n'
            text += f'    (keepout "" (circle F.Cu 500 {(x + ex) * 500:.0f} {-(y + ey) * 500:.0f}))\n'
    return text
