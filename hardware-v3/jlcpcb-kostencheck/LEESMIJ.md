# Kostencheck v3 bij JLCPCB

Gemaakt met `python3 ../export_jlc.py --kostencheck`. **Alleen voor een offerte, niet bestellen:**
het bord is nog niet gerouteerd en de plaatsingsrotaties zijn nog niet nagekeken.

- `v3-gerbers.zip`: Gerbers en boorbestanden, uploaden als PCB
- `v3-bom.csv`: BOM voor de assembly
- `v3-cpl.csv`: plaatsingsbestand voor de assembly

Instellingen:

- 2 lagen, 1,6 mm, 226 x 80 mm, aantal naar keuze (8 borden: bestel 10)
- PCB Assembly, Economic, top side, aantal te assembleren borden naar keuze
- Through-hole delen staan niet in de BOM: headers X1-X3, J2, J3, schroefklemmen J4, J5 en de
  sockets voor de ESP32 soldeer je zelf

De rotaties van alle IC's (U) en weerstandsarrays (RN) worden in `export_jlc.py` een kwart slag
rechtsom gecorrigeerd (`ROTATION_FIX`), nagekeken in de JLC-preview op 16-11-2026. Kijk na elke export
toch in de preview of de pin-1-markering van elk IC op de juiste plek staat.
