# ESP32 OmniGlow analyzer

## Ingangscircuit

De Hue-DATA-lijn wordt niet rechtstreeks op de ESP32 aangesloten. De ingang gebruikt een 74HC14 op 3,3 V als spanningsbegrenzing, signaalconditionering en dubbele inverter:

```text
3V3
 │
[4k7]
 │
 ├───[1k]───► U1 pin 1
 │              │
Hue data       poort 1
                │
              U1 pin 2 ──► U1 pin 3
                              │
                           poort 2
                              │
                           U1 pin 4 ──► GPIO25 (ESP32)
```

### Voeding en massa van U1

- U1 pin 14 naar 3V3.
- U1 pin 7 naar GND.
- Plaats 100 nF tussen pin 14 en pin 7, zo dicht mogelijk bij de chip.
- Verbind de ongebruikte ingangen U1 pin 5, pin 9, pin 11 en pin 13 met GND.

### Massa

Verbind de GND van de Hue-controller, de ESP32 en U1 met elkaar. Zonder gemeenschappelijke massa is het logische niveau niet betrouwbaar.

### Werking

- De 4k7-pull-up zit aan de Hue-kant. Bij een open-collectoruitgang is het hoge niveau daardoor 3,3 V; de Hue-controller trekt de lijn actief naar 0 V.
- Bij een push-pulluitgang of een interne pull-up naar 5 V beperkt de 1k-serieweerstand de stroom naar de ingangsbescherming van U1 tot ongeveer 1 mA.
- De twee 74HC14-poorten inverteren het signaal tweemaal. GPIO25 ziet daardoor dezelfde logische polariteit als de Hue-DATA-lijn, maar op een schoon niveau van 0–3,3 V.

## ESP32-pinnen

```text
GPIO25 = DATA IN vanuit U1 pin 4
GPIO26 = DATA OUT voor toekomstig replay, via een geschikte levelshifter
```

GPIO25 is de RMT-ingang voor de protocolanalyse. GPIO26 wordt niet met de originele Hue-DATA-lijn verbonden zolang de replayfunctie niet bewust wordt gebruikt. Koppel de originele Hue-controller fysiek los voordat een eigen DATA-signaal wordt uitgezonden.

## Uitgangscircuit

De uitgang gebruikt een tweede 74HC14 (U2), gevoed vanuit ongeveer 4,5 V. De ESP32-uitgang op GPIO26 wordt daarmee naar een geschikt 5 V-datasignaal omgezet.

```text
5V ───────►|───────┬──────────── U2 pin 14 (Vcc, ca. 4,5V)
           1N4148  │
        (streep    │
         naar U2) [100nF]
                   │
GND ───────────────┴──────────── U2 pin 7 (GND)

GPIO26 ──────► U2 pin 1
                (poort 1)
                   │
                U2 pin 2 ──► U2 pin 3
                             (poort 2)
                                │
                             U2 pin 4
                                │
                  ┌─────────────┴─────────────┐
                [680Ω]                      [680Ω]
                  └─────────────┬─────────────┘
                                │
                           Data strip
```

### Aansluitingen U2

- Pin 14 (Vcc) gaat naar de kathode van de diode, dus de kant met de streep.
- De anode van de diode gaat naar 5 V.
- Pin 7 gaat naar GND.
- Plaats de 100 nF direct tussen pin 14 en pin 7, aan de chipkant van de diode.
- Pin 1 gaat naar GPIO26.
- Pin 2 gaat naar pin 3.
- Pin 4 gaat via twee parallelle 680 Ω-weerstanden naar de OmniGlow-datalijn. Samen vormen die ongeveer 340 Ω.
- Pin 5, 9, 11 en 13, de ongebruikte ingangen, gaan naar GND.
- Pin 6, 8, 10 en 12, de ongebruikte uitgangen, blijven open.

### Massa en isolatie

De GND van de ESP32, U2 en de strip moet met elkaar verbonden zijn. De datalijn van de strip gaat uitsluitend naar deze uitgangsweerstand van ongeveer 340 Ω. De originele Hue-controller-DATA moet tijdens replay volledig losgekoppeld zijn.

### Onderdelen

- 1× 74HC14 (U2)
- 1× 1N4148 of 1N4007
- 2× 680 Ω
- 1× 100 nF

Optioneel kan een 10 kΩ-weerstand van GPIO26 naar GND worden geplaatst. Daarmee blijft de uitgang tijdens het opstarten laag, zodat de strip niet kort flikkert voordat de firmware GPIO26 overneemt.

## WiFi-analyzer

De analyzer maakt verbinding met het lokale WiFi-netwerk en toont het IP-adres in de seriële monitor. Vul SSID en wachtwoord alleen lokaal in de sketch in. Publiceer nooit echte WiFi-gegevens in deze repository. Open daarna het gemelde IP-adres in een browser. De pagina toont captures, labels, de ruwe framebytes en de testresultaten.
