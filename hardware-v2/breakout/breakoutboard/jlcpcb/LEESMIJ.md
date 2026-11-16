# Breakout bestellen bij JLCPCB

Gemaakt met `python3 ../../export_jlc.py`. Het script weigert bij DRC-fouten.

- `breakout-gerbers.zip`: Gerbers en boorbestanden, uploaden als PCB
- `breakout-bom.csv`: BOM voor de assembly
- `breakout-cpl.csv`: plaatsingsbestand voor de assembly

Instellingen:

- 2 lagen, 1,6 mm, 32 x 19 mm, 15 stuks
- Oppervlak: ENIG (vlakker bij 0,65 mm pitch) of HASL lead-free
- PCB Assembly, Economic, top side, 15 stuks, Confirm Parts Placement aan
- J1 en J2 (headers) staan niet in de BOM: niet monteren, zelf solderen aan de chipkant

Controleer in de preview of de pin-1-punt van U1 op het pin-1-merkteken staat. Rotatie 270 is
nagerekend tegen de JLCPCB-footprint van C1855817.
