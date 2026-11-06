# Dotflipper v2: ontwerpnotitie

Status: concept, nog niet nagekeken. Eerste schema staat in `kicad/`, nog geen PCB.

## Doel

Een nieuwe controller die sneller is dan de PCA9685-versie en waarbij een softwarefout of crash
geen spoel of driver kan beschadigen. De aanleiding: op v1 bleef bij een bug een spoel honderden
milliseconden aan staan, omdat niets in de hardware dat tegenhoudt.

## Wat we weten

**Gemeten of uit het v1-schema:**

- Paneel: 32 lijnen op X1, 16 lijnen op X2 + X3. Twee panelen.
- Connectors: pin headers 2,54 mm, 2 rijen. X1 34 polig (pin 33 en 34 ongebruikt op v1), X2 en X3 16 polig.
- X1: elke lijn wordt op v1 zowel naar plus als naar massa geschakeld.
- X2/X3: per lijn twee aparte draden: oneven pinnen naar plus, even pinnen naar massa.
- Per dot een BAV99 (opdruk A7). Middenpin naar de spoel, buitenpinnen naar de 16-kant.
- Spoel: ongeveer 20 ohm. Spanning: 20 V. Op v1 flipt alles goed bij een puls van ongeveer 224 us.

**Niet gemeten:** de zelfinductie van de spoel en dus de echte piekstroom.

**Aanname op basis van een ander paneel:** de BUSE DOT-LED-specificatie noemt 18 ohm, 6 mH, en
350-450 mA aanbevolen. Als onze spoel vergelijkbaar is:

| Spoel | 150 us | 224 us | 500 us | 1 ms |
|---|---|---|---|---|
| 3 mH | 506 mA | 620 mA | 771 mA | 799 mA |
| 6 mH | 315 mA | 421 mA | 649 mA | 771 mA |
| 10 mH | 207 mA | 289 mA | 506 mA | 692 mA |

Berekend met 16 V over 20 ohm, dus na de spanningsval van de v1-drivers en de BAV99. Bij 6 mH
komt 224 us precies in het aanbevolen gebied, en dat past bij wat we zagen. Ontwerpwaarde voor v2:
**1 A per spoel**, want met de lagere spanningsval van v2 is de eindwaarde ongeveer 0,9 A.

## Eisen

1. Plus en massa op dezelfde lijn kunnen in hardware niet tegelijk aan.
2. Een MCU die crasht, hangt of reset kan geen spoel aan laten staan.
3. Bij reset en opstarten zijn alle uitgangen vanzelf uit.
4. Spanningspieken van de spoelen hebben altijd een veilige weg.
5. Firmware kan de echte stroom zien.
6. Veel sneller dan v1: nu ongeveer 0,9 ms per dotpaar.
7. Twee panelen op één bord, te maken bij JLCPCB.

## Niet doen: de twee draden aan de 16-kant samenvoegen

Het lijkt 16 drivers per paneel te besparen, maar zonder aparte draden ontstaat een sluipweg via
een niet-aangestuurde lijn: spoel, diode, gezamenlijke draad, andere diode, spoel. Dan flippen er
dots die niet bedoeld zijn. Dit volgt uit mijn analyse van de BAV99-bedrading en is nog niet getest.
Per paneel zijn er dus 64 uitgangen nodig: 32 half-bruggen en 32 enkele schakelaars.

## Keuze drivers

**Voorstel: TI DRV8912-Q1**, 12 half-bruggen met SPI.

| Eigenschap | Waarde |
|---|---|
| Voedingsspanning | 4,5 tot 32 V, absoluut maximum 40 V |
| Stroom | 1 A RMS per uitgang |
| Weerstand aan | 0,75 ohm typisch per FET |
| Overstroombeveiliging | Trip 1,3 tot 2,3 A, instelbare deglitch 1 tot 60 us |
| Dode tijd bij half-brug | 2 tot 15 us bij snelle slew rate, dus eis 1 zit in de chip |
| Vertraging SPI naar uitgang | 3 tot 10 us bij snelle slew rate |
| SPI | 5 MHz, 16 bit, daisy chain |
| nSLEEP laag | Alles uit binnen 20 us, opnieuw starten kost 200 us |
| Behuizing | HTSSOP-24 met thermal pad, assembly aangeraden |

Per paneel 6 chips (72 uitgangen, 64 gebruikt), dus 12 op het bord.

De interne diodes vangen spanningspieken op naar VM en massa, dus eis 4 wordt grotendeels vanzelf
gehaald. Dat geldt alleen met voldoende buffercondensatoren op VM. De chip heeft ook
overspanningsbeveiliging; dat die niet aanspreekt bij de terugvloeiende energie moet nog worden berekend.

**Alternatief: TPIC6B595-achtige shiftregisters plus high-side schakelaars.** Goedkoper en dichter
bij het oorspronkelijke idee, maar de TPIC6B595 is 150 mA continu met een stroomlimiet rond 500 mA,
dat is krap. Eis 1 moet dan met extra logica, en die fout is precies wat we willen uitsluiten.

## Hardware-beveiliging tegen een te lange puls

Los van de MCU en de drivers (blad "Hardware-beveiliging"):

1. Shunt van 50 mohm met INA180A1 (gain 20): 1 V per ampere. Ook naar een ADC, voor eis 5.
2. Comparator: boven ongeveer 100 mA start de timer.
3. RC-timer: 27k en 100 nF, drempel 1,03 V: na ongeveer 1 ms klokt de tweede comparator een latch.
4. De latch zet via een AND-poort nSLEEP van alle drivers laag, en schakelt ook de P-FET in de 20 V-lijn uit.
5. Na power-on staat de latch aan (veilige stand). De MCU kan hem alleen via een condensator
   wissen: een flank, geen niveau. Een hangende pin kan de beveiliging dus niet uitzetten.
6. ENABLE en nSLEEP hebben pull-downs, dus bij reset is alles uit.

**De shunt moet tussen de buffer en de drivers zitten.** Staat hij vóór de buffer, dan ziet hij bij
snel achter elkaar flippen continu stroom, en schakelt de timer onterecht uit. Daarom zit de 470 uF
vóór de shunt, en staat er per driver alleen 2 x 100 nF aan VM. **Dit wijkt af van de datasheet**
(die vraagt 10 uF of meer bij VM). De shunt is maar 50 mohm, dus de buffer doet nog steeds zijn werk,
maar de layout moet kort zijn, en dit moet op de proefopstelling getest worden.

De P-FET (AO3401A) krijgt via 47k/47k maximaal 10 V gate-source bij 20 V. Bij 24 V is dat 12 V, de
grens van dit onderdeel: niet hoger gaan zonder aanpassing.

## Voeding

Logica: 20 V naar 5 V (AP63205), of USB, via Schottky-diodes samen op +5V, dan AMS1117 naar 3,3 V.
De logica werkt dus ook met de 20 V uit, zodat flashen via USB veilig is.

## MCU

**Voorstel: ESP32-S3-module.** De firmware (Arduino, WiFiManager, OTA, webinterface) gaat vrijwel
mee, en hardware-SPI met DMA is ruim genoeg. De Pico 2 W heeft met PIO betere timing, maar die is
hier niet meer nodig omdat de hardware de veiligheid regelt. Het zou de hele netwerkkant opnieuw maken.

## Snelheid (geschat, niet gemeten)

Twee SPI-ketens, één per paneel, zodat beide tegelijk schakelen. Per commando 6 x 16 bit op 5 MHz
is ongeveer 20 us, plus 3 tot 10 us vertraging. Per dotpaar dus ongeveer de puls plus 60 us.
Bij 224 us is dat 0,28 ms, ongeveer 3 keer sneller dan v1. De puls kan pas korter na meting.

## Eerste test vóór we een bord bestellen

Een DRV8912-evaluatiebord of los proefbordje op één lijn van een paneel, om te meten:
piekstroom, minimale puls voor een betrouwbare flip, gedrag van de spanningspiek, en of de
overstroombeveiliging niet onterecht aanspreekt.

## Bestanden

- `generate.py` maakt het KiCad-project in `kicad/`.
- `check.py` controleert het netlist op de eisen in dit document. ERC alleen is niet genoeg.

    python3 generate.py
    kicad-cli sch export netlist --format kicadsexpr -o /tmp/netlist.net kicad/dotflipper2.kicad_sch
    python3 check.py /tmp/netlist.net

Wijzig het schema in `generate.py`, niet in KiCad: de bestanden in `kicad/` worden overschreven.

## Open punten

- Footprint DRV8912: de maat van de thermal pad van TI's PWP-behuizing nog vergelijken met de KiCad-footprint.
- Onderdeelnummers (MPN, LCSC) zijn nog niet toegewezen, ook de footprints van L1, F1 en J1 zijn een eerste keuze.
- De timer en comparatoren zijn berekend, niet gesimuleerd of getest.

- Inductie van de spoel meten.
- Klopt de ongebruikte status van pin 33/34, en de plaatsing van X1, X2, X3 op het paneel?
- Prijs en voorraad van de DRV8912-Q1 bij JLCPCB. Nog niet gecontroleerd.
- Energie van de spoel terug in VM, grootte van de buffercondensator.
- Hoe de ESP32-S3 de chips configureert vóór gebruik, en wat er gebeurt na een fout (nFAULT).

## Bronnen

- [BUSE DOT-LED specificatie](https://www.mikrocontroller.net/attachment/652902/dot_ledstrips_spec_en.pdf), via zoekresultaat, het PDF zelf kon ik niet downloaden
- [DRV8912-Q1 datasheet](https://www.ti.com/lit/ds/symlink/drv8912-q1.pdf)
- [TPIC6B595](https://ti.com/product/TPIC6B595)
