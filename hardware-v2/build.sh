#!/bin/sh
# Whole board from the schematic: ./build.sh proef|groot [router passes]
# Starts from an empty board, so it throws away any routing done by hand.
set -e
cd "$(dirname "$0")"
if [ "$1" = proef ]; then folder=kicad-proef; name=dotflipper2-proef; else folder=kicad; name=dotflipper2; fi
net=$(mktemp)
kicad-cli sch export netlist --format kicadsexpr -o "$net" "$folder/$name.kicad_sch" > /dev/null
python3 pcb.py "$1" "$net" 2>/dev/null | tail -1
python3 route.py "$folder/$name.kicad_pcb" "${2:-40}" 2>/dev/null | tail -1
grep -E "Auto-routing stage completed" "$folder/$name.route.log" | sed 's/.*stage completed: //' | cut -c1-110
python3 stitch.py "$folder/$name.kicad_pcb" 2>/dev/null | grep -E "Ronde"
rm -f "$net"
