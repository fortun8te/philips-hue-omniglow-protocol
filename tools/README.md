# Geautomatiseerde OmniGlow-tests

`hue_capture_test.py` bedient de originele Hue-controller via Home Assistant en laat daarna de ESP32 een capture maken. De WiFi-gegevens, Home Assistant-token en entity-id worden uitsluitend via omgevingsvariabelen gelezen.

## Eenmalige voorbereiding

```bash
cd tools
cp .env.example .env.local
```

Vul `.env.local` lokaal in en laad hem in de huidige shell. Deel dit bestand niet en commit het niet.

```bash
export HA_URL=http://homeassistant.local:8123
export HA_TOKEN='jouw-lokale-home-assistant-token'
export HA_ENTITY_ID=light.omniglow
export ESP32_URL=http://192.168.1.123
```

Controleer eerst de testreeks zonder hardware aan te sturen:

```bash
python3 hue_capture_test.py --dry-run
```

Voer daarna één test uit:

```bash
python3 hue_capture_test.py --only red_10
```

Of voer de volledige reeks uit:

```bash
python3 hue_capture_test.py
```

De resultaten worden opgeslagen in `captures/automated/<tijdstip>/` als losse `.hex`-bestanden, een `manifest.json` en een volledige `all-results.txt`.

Home Assistant moet de Hue-lamp als `light`-entity aanbieden. De service wordt met `transition: 0` aangeroepen, waarna het script nog wacht voordat het frame wordt vastgelegd. De originele Hue-controller blijft tijdens deze fase aan de strip gekoppeld; GPIO26 van de ESP32 blijft los.
