#!/usr/bin/env python3
"""Checks the netlist of the schematic against what the design must do, see ONTWERP.md.

ERC only finds wiring that looks odd. This checks that the board does what it is meant to do:
    kicad-cli sch export netlist --format kicadsexpr -o netlist.net kicad/dotflipper2.kicad_sch
    ./check.py netlist.net
"""

import re
import sys

text = open(sys.argv[1]).read()
nets = {}
for match in re.finditer(r'\(net\s+\(code "?\d+"?\)\s+\(name "(.*?)"(.*?)(?=\n\s*\(net\s|\Z)', text, re.S):
    name = match.group(1).split('/')[-1]
    nets[name] = sorted(set(re.findall(r'\(ref "(.*?)"\)\s+\(pin "(.*?)"', match.group(2))))
pins = {node: name for name, nodes in nets.items() for node in nodes}
failures = 0


def check(text, ok):
    global failures
    print(f'  {text:68s} {"goed" if ok else "FOUT"}')
    failures += not ok


def connected(net, *nodes):
    return all(node in nets.get(net, []) for node in nodes)


OUT_PINS = ['2', '23', '14', '11', '3', '10', '4', '22', '9', '15', '17', '18']  # OUT1 to OUT12

print('Panelen')
for p, base in ((0, 100), (1, 200)):
    chips = [f'U{base + i}' for i in range(1, 7)]
    x1, x2, x3 = f'J{base + 1}', f'J{base + 2}', f'J{base + 3}'
    used = []
    for pin_list, connector, count in ((None, x1, 32), (None, x2, 16), (None, x3, 16)):
        for n in range(1, count + 1):
            nodes = [node for node in pins if pins[node] == pins.get((connector, str(n)))]
            drivers = [node for node in nodes if node[0] in chips]
            used.append(len(drivers) == 1 and len(nodes) == 2)
    check(f'Paneel {p}: alle 64 connectorpinnen aan precies 1 driveruitgang', all(used) and len(used) == 64)
    outputs = [(chip, pin) for chip in chips for pin in OUT_PINS]
    named = [pins.get(o, '') for o in outputs]
    wired = [n for n in named if n.startswith(f'P{p}_')]
    check(f'Paneel {p}: 64 uitgangen gebruikt, geen enkele dubbel', len(wired) == 64 and len(set(wired)) == 64)
    check(f'Paneel {p}: X1 pin 33 en 34 niet aangesloten', (x1, '33') not in pins and (x1, '34') not in pins or
          all(len(nets[pins[(x1, n)]]) == 1 for n in ('33', '34') if (x1, n) in pins))
    check(f'Paneel {p}: X1 en X2/X3 nooit op dezelfde driver', not {o[0] for o in outputs[:36] if pins.get(o, '').startswith(f'P{p}_')} &
          {o[0] for o in outputs[36:] if pins.get(o, '').startswith(f'P{p}_')})
    chain = pins.get((chips[0], '5')) == f'MOSI{p}' and pins.get((chips[5], '7')) == f'MISO{p}'
    for a, b in zip(chips, chips[1:]):
        chain &= pins.get((a, '7')) == pins.get((b, '5'))
    check(f'Paneel {p}: SPI-keten loopt door alle 6 drivers', chain)
    check(f'Paneel {p}: klok, select, nSLEEP en nFAULT aan elke driver',
          all(pins.get((c, '20')) == f'SCLK{p}' and pins.get((c, '19')) == f'CS{p}' and
              pins.get((c, '8')) == 'NSLEEP' and pins.get((c, '12')) == 'NFAULT' for c in chips))
    check(f'Paneel {p}: VM, VDD en massa aan elke driver',
          all(pins.get((c, '16')) == 'VM' and pins.get((c, '21')) == 'VM' and pins.get((c, '6')) == '+3V3' and
              all(pins.get((c, g)) == 'GND' for g in ('1', '13', '24', '25')) for c in chips))

print('Beveiliging')
check('Alleen de AND-poort stuurt NSLEEP', connected('NSLEEP', ('U6', '4')) and
      not any(node[0] in ('U7',) for node in nets['NSLEEP']))
check('nSLEEP heeft een pull-down', connected('NSLEEP', ('R13', '1')) or connected('NSLEEP', ('R13', '2')))
check('ENABLE heeft een pull-down', any(n[0] == 'R12' for n in nets['ENABLE']))
check('Poort: ENABLE en TRIP_N', connected('ENABLE', ('U6', '1')) and connected('TRIP_N', ('U6', '2')))
check('Latch: ~Q op TRIP_N, Q naar de MCU', connected('TRIP_N', ('U5', '3')) and connected('TRIPPED', ('U5', '5')))
check('Latch begint uitgeschakeld (PRE met RC)', connected('PRE_N', ('U5', '7')) and any(n[0] == 'C13' for n in nets['PRE_N']))
check('MCU kan CLR alleen via een condensator bereiken',
      not any(n[0] == 'U7' for n in nets['CLR_N']) and any(n[0] == 'C14' for n in nets['CLR_N']) and any(n[0] == 'C14' for n in nets['TRIP_CLEAR']))
check('Timer: comparator uit naar RC, naar tweede comparator', connected('TIMER', ('U4', '1'), ('U4', '5')))
check('Comparator 2 klokt de latch', connected('TRIP_CLK', ('U4', '7'), ('U5', '1')))
check('Stroommeting: shunt tussen VM_SW en VM', any(n[0] == 'R3' for n in nets['VM_SW']) and any(n[0] == 'R3' for n in nets['VM']))
check('INA180 over de shunt', connected('VM_SW', ('U3', '3')) and connected('VM', ('U3', '4')))
check('Buffer vóór de shunt, geen elco op VM',
      any(n[0] == 'C11' for n in nets['VM_SW']) and not any(n[0] == 'C11' for n in nets['VM']))
check('VM-schakelaar: source aan VIN, drain aan VM_SW', connected('VIN', ('Q1', '2')) and connected('VM_SW', ('Q1', '3')))
check('VM-schakelaar volgt NSLEEP', connected('NSLEEP', ('Q2', '1')) and connected('VM_GATE_N', ('Q2', '3')))

print('MCU en voeding')
check('Polariteit D2, D3: kathode op +5V', connected('+5V', ('D2', '1'), ('D3', '1')))
check('Polariteit D4: clamp naar +3V3', connected('+3V3', ('D4', '1')) and connected('CLR_N', ('D4', '2')))
check('Elco C11: plus aan VM_SW', connected('VM_SW', ('C11', '1')))
check('LED: kathode aan massa', connected('GND', ('D10', '1')))
check('Logica werkt zonder 20 V: LDO hangt aan +5V', connected('+5V', ('U2', '3')) and any(n[0] == 'D3' for n in nets['USB_VBUS']))
check('ISENSE naar ADC1 (IO1) en comparator', any(n[0] == 'U7' for n in nets['ISENSE']) and connected('ISENSE', ('U4', '3')))

print(f'\n{"MISLUKT: %d fout(en)" % failures if failures else "Alles goed"}')
sys.exit(failures != 0)
