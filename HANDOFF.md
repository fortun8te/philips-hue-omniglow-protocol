# Philips Hue OmniGlow protocolanalyse — handoff

## Doel

Het elektrische dataprotocol van de Philips Hue OmniGlow-strip begrijpen en uiteindelijk onafhankelijk kunnen reproduceren voor kleur, helderheid, wit en kleurtemperatuur.

De originele Hue-controller blijft tijdens analyse aan de strip gekoppeld. De ESP32 is dan uitsluitend een hoogohmige DATA-monitor. Replay via de eigen uitgang gebeurt alleen nadat de originele DATA-uitgang fysiek is losgekoppeld.

## Hardware en pinnen

### ESP32

```text
GPIO35 = DATA IN vanuit ingangstrap U1
GPIO26 = DATA OUT naar uitgangstrap U2 voor replay
GND    = gemeenschappelijk met Hue-controller, U1, U2 en strip
```

GPIO35 is input-only en werkt in de huidige opstelling. De ESP32 gebruikt de RMT-peripheral voor nauwkeurige capture.

### Ingangstrap U1

U1 is een 74HC14 op 3,3 V:

```text
Hue DATA ── 1kΩ ── U1 pin 1
U1 pin 2 ───────── U1 pin 3
U1 pin 4 ───────── ESP32 GPIO35
```

Voeding:

- U1 pin 14 naar 3V3.
- U1 pin 7 naar GND.
- 100 nF direct tussen pin 14 en pin 7.
- Ongebruikte ingangen pin 5, 9, 11 en 13 naar GND.
- Ongebruikte uitgangen pin 6, 8, 10 en 12 open laten.

De eerder gebruikte 4k7-pull-up naar 3V3 is verwijderd omdat de Hue-controller met de pull-up een storing gaf. De controller levert in de werkende meetopstelling zelf de hoge en lage niveaus.

### Uitgangstrap U2

U2 is een tweede 74HC14:

```text
5V ── anode 1N4148; kathode ── U2 pin 14
U2 pin 7 ── GND
100 nF tussen pin 14 en pin 7

GPIO26 ── U2 pin 1
U2 pin 2 ── U2 pin 3
U2 pin 4 ── 2 × 680Ω parallel (ongeveer 340Ω) ── strip DATA
```

Ongebruikte U2-ingangen pin 5, 9, 11 en 13 naar GND; ongebruikte uitgangen pin 6, 8, 10 en 12 open.

Optioneel: 10kΩ van GPIO26 naar GND om de uitgang tijdens boot laag te houden.

De oorspronkelijke Hue-controller-DATA mag nooit tegelijk met GPIO26 als actieve uitgang op dezelfde datalijn aangesloten zijn.

## Vastgestelde timing

- ESP32 RMT-clock: 20 MHz.
- Eén RMT-tick: 50 ns.
- Eén dataslot: ongeveer 25 ticks = 1,25 µs.
- Lange HIGH in de huidige decoder: ongeveer 19 ticks, gevolgd door ongeveer 6 ticks LOW.
- De RMT-inputfilter moet uit staan; korte LOW-segmenten werden anders weggefilterd.
- De huidige decoder gebruikt korte HIGH als bit 0 en lange HIGH als bit 1.

## Huidige framebevindingen

De eerste gelabelde dataset bevat 19 captures. Iedere capture bevat 806 bytes.

De data toont:

```text
100 × 8-byte blokken = 800 bytes
+ 6 extra bytes
```

Bij stabiele captures zijn de 100 blokken identiek. De vier waarden in ieder 8-byte blok kunnen voorlopig worden gelezen als vier big-endian 16-bit waarden:

```text
V0 = byte 0..1
V1 = byte 2..3
V2 = byte 4..5
V3 = byte 6..7
```

De eerdere hypothese `80 × 10 bytes + 6-byte footer` past niet bij deze ESP32-dataset en moet voorlopig worden verworpen.

Voorbeelden van dominante blokwaarden:

| Label | V0 | V1 | V2 | V3 |
|---|---:|---:|---:|---:|
| ON_WHITE_100 | 20D7 | 4567 | 0000 | 0000 |
| RED_10 | 0007 | 0387 | 0006 | 0000 |
| RED_20 | 0A34 | 0012 | 0000 | 0014 |
| RED_50 | 0071 | 3A28 | 0069 | 0000 |
| RED_75 | 0000 | 00F9 | 871F | 00FF |
| RED_100 | 0000 | 01B2 | FFFF | 01FE |
| GREEN_100 | 1E02 | 0000 | BF31 | 0000 |
| BLUE_100 | 0000 | 030C | FFFF | 0000 |
| WARMWHITE_10 | 0394 | 0031 | 0000 | 0008 |
| WARMWHITE_50 | 3E55 | 033E | 0000 | 0082 |
| WARMWHITE_100 | 01D2 | FFFF | 0C5D | 0000 |
| COOLWHITE_50 | 082C | 118A | 0000 | 0000 |
| 2700K | 1AB5 | 0000 | C5AD | 0000 |
| 4000K | DC55 | 0000 | 03F3 | 0000 |
| 6500K | 539C | FFFF | 2E1B | 0000 |

Deze waarden zijn geen eenvoudige directe RGB- of RGB+CCT-bytes. De OmniGlow/Hue-controller lijkt een interne kleurtransformatie te gebruiken. De strip werkt met drie hoge-niveau kleurparameters in de app, maar dat betekent niet dat het wire-protocol drie eenvoudige bytes gebruikt.

## Overgangen en onstabiele captures

De Hue-controller verstuurt meerdere tussenframes bij kleur-, helderheids- of temperatuurwijzigingen. Captures met meerdere verschillende 8-byte blokken zijn daarom waarschijnlijk overgangsframes of verkeerd uitgelijnde frames.

Onstabiele voorbeelden in de eerste dataset:

- TEST
- COOLWHITE_10
- COOLWHITE_100
- EXTRA_CAPTURE

Voor een betrouwbare capture:

1. Stel de lamp in via de Hue-app/Home Assistant.
2. Gebruik overgangstijd 0 waar mogelijk.
3. Wacht minimaal 3 seconden.
4. Maak drie captures met dezelfde instelling.
5. Gebruik alleen waarden die in alle drie stabiel gelijk zijn.

## Software

### ESP32-analyzer

Bestand:

```text
esp32_capture_analyzer/esp32_capture_analyzer.ino
```

Functies:

- RMT capture op GPIO35.
- Webserver op lokaal WiFi.
- Capture-labels via de website.
- Testreeks voor rood, groen, blauw, warmwit, koudwit en temperatuur.
- Opslag van maximaal 20 captures in RAM.
- `/results` levert alle resultaten als tekst.
- GPIO26 is voorbereid voor replay via U2.

WiFi-gegevens zijn placeholders en staan niet in deze repository.

### Geautomatiseerde Mac-test

Bestand:

```text
tools/hue_capture_test.py
```

Het script kan:

1. Home Assistant lokaal aansturen via de REST API.
2. Een Hue-light-entity instellen met RGB, brightness of color temperature.
3. Op stabilisatie wachten.
4. De ESP32-capture starten.
5. Het gelabelde frame ophalen.
6. `.hex`, `manifest.json` en `all-results.txt` opslaan.

Configuratie gebeurt uitsluitend via omgevingsvariabelen:

```text
HA_URL
HA_TOKEN
HA_ENTITY_ID
ESP32_URL
```

Nooit tokens of WiFi-wachtwoorden committen.

## Volgende protocolstappen

1. Pas de analyzer aan om vijf tot tien opeenvolgende volledige frames per capture te bewaren.
2. Bepaal met framevergelijking of de echte framebody 800 bytes is en of de extra 6 bytes uit het volgende frame komen.
3. Maak drie stabiele captures voor elke roodhelderheid: 10, 20, 30, 40, 50, 60, 70, 80, 90 en 100%.
4. Herhaal dit voor warmwit en koudwit.
5. Test 2700K, 3000K, 4000K, 5000K en 6500K bij vaste helderheid.
6. Analyseer per 8-byte blok welke V0–V3-waarden veranderen.
7. Zoek pas daarna naar checksum/footerlogica.
8. Gebruik de ESP32-CAM pas nadat de digitale mapping bekend is; de camera is voor optische kalibratie, niet voor de eerste protocoldecodering.

## Veiligheid

- 24 V van de strip nooit op de ESP32 aansluiten.
- GPIO35 alleen als ingang gebruiken.
- GPIO26 nooit parallel met de actieve Hue-controller als uitgang aansluiten.
- Tijdens capture blijft de originele controller aan de strip; tijdens replay wordt de originele DATA fysiek losgenomen.
- Controleer 3,3 V-logica aan GPIO35 en geschikte 5 V-levelshifting aan GPIO26.
