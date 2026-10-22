# Eerste analyse van de OmniGlow-captures

Bron: 19 gelabelde captures uit de ESP32-RMT-analyzer.

## Directe observaties

- Iedere capture bevat 806 bytes.
- De eerste 800 bytes kunnen exact worden gegroepeerd als 100 blokken van 8 bytes.
- Bij veel stabiele captures zijn alle 100 blokken identiek.
- De laatste 6 bytes lijken in deze export niet op een vaste footer: bij stabiele frames zijn zij de eerste 6 bytes van hetzelfde 8-byte patroon.
- De eerdere hypothese `80 × 10 bytes + 6-byte footer` past niet bij deze nieuwe dataset en moet worden herzien.

## Stabiele 8-byte blokken

De vier big-endian 16-bit waarden van het dominante blok zijn:

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
| TEMPERATURE_2700K | 1AB5 | 0000 | C5AD | 0000 |
| TEMPERATURE_4000K | DC55 | 0000 | 03F3 | 0000 |
| TEMPERATURE_6500K | 539C | FFFF | 2E1B | 0000 |

## Voorlopige interpretatie

De vier waarden gedragen zich niet als eenvoudige 8-bit RGB-kanalen. Eén gebruikersinstelling verandert vaak meerdere 16-bit waarden en sommige waarden bereiken `FFFF`. Dit past beter bij een interne kleurrepresentatie, bijvoorbeeld kleurcoördinaten plus intensiteits- of mengwaarden, dan bij direct RGB+CCT.

De 10/20/50/75/100%-reeks voor rood is bruikbaar voor het bepalen van de helderheidsfunctie, maar de waarden zijn niet monotone afzonderlijke kanaalwaarden. De controller voert waarschijnlijk kleurtransformatie, gamma, normalisatie of begrenzing uit.

## Niet-stabiele captures

`TEST`, `COOLWHITE_10`, `COOLWHITE_100` en `EXTRA_CAPTURE` bevatten meerdere 8-byte patronen. Dat kan wijzen op een overgang, animatie, kleurtemperatuur- of helderheidsramp tijdens de capture. Voor protocolanalyse moet de lamp na iedere wijziging minstens enkele seconden stabiel staan voordat CAPTURE wordt ingedrukt.

## Volgende analyse

1. Vergelijk alle 100 blokken per capture en bepaal of de laatste 6 bytes werkelijk bij het volgende frame horen.
2. Neem per instelling drie captures met dezelfde waarde.
3. Test helderheid met een vaste kleur en zonder overgangseffect.
4. Test kleurtemperatuur met vaste helderheid.
5. Zoek naar een checksum alleen nadat de echte framegrens en frame-lengte zijn vastgesteld.

De huidige dataset bewijst dus een herhaald 8-byte datablok, maar bewijst nog niet de betekenis van V0–V3.
