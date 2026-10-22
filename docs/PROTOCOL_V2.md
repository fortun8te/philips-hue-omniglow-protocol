# Philips Hue OmniGlow data protocol — verified specification (v2, 22 Oct 2026)

Independent reverse engineering of the Philips Hue OmniGlow lightstrip DATA line, from logic captures of a genuine Hue controller and from driving a cut strip with an ESP32. Supersedes the open questions in [PROTOCOL.md](PROTOCOL.md). Not an official Philips specification.

## 1. Summary

| Item | Value | Confidence |
|---|---|---|
| Line coding | NRZ pulse-width, idle low, MSB first | Measured (controller + own output) |
| Bit slot | 1.25 µs (800 kbit/s) | Measured: 1249 ns controller, 1250 ns ours |
| `1` | high ≈ 950 ns (measured 875–1000), then low | Measured |
| `0` | high ≈ 250 ns (measured 250–375), then low | Measured |
| Frame | N × 10-byte pixel units + 6-byte tail | Measured on controller, replay accepted |
| Pixel unit | 5 × big-endian uint16: **R, G, B, warm white, cool white** | Measured channel-by-channel |
| Frame rate | one frame every ~17–20 ms, continuous | Measured (17.4 ms gap after a 7-unit frame) |
| Addressing | one unit per 12.5 cm section, daisy-chained | Measured: 8 different units → 8 different sections on a 1 m cut piece |
| Tail | constant per controller, e.g. `03 75 D7 7B EC 08` or `03 75 D7 5E 78 08` | Measured; meaning unknown |

## 2. Electrical

- Logic levels: the controller output was read through a 74HC14 (two inverters, 1 kΩ series) into a 3.3 V ESP32 input; our output drives the strip through a 74HC14 on ~4.5 V with ~340 Ω series.
- Polarity: non-inverted at the strip DATA pad (high = active pulse).
- Idle between frames: low. Frames are separated by ≥ 9 ms of idle.
- Never connect strip supply (24 V) to a logic pin, and never drive DATA while the Hue controller is also connected to it.

## 3. Frame layout

```
| unit 0 (10 B) | unit 1 (10 B) | ... | unit N-1 (10 B) | tail (6 B) |
  ^ nearest the controller
unit = R_hi R_lo G_hi G_lo B_hi B_lo Ww_hi Ww_lo Cw_hi Cw_lo
```

- Each section (an IC driving the LED modules of one 12.5 cm cut segment) takes the first unit it receives and passes the rest on, so unit k lands on section k. Cutting the strip only reduces how many units are consumed; extra units are harmless.
- The Hue controller of a 10 m strip (`929004608201`) sends **7 units** (its gradient resolution, `pixel_count: 7` in the CLIP v2 API), all identical for a single colour and different for gradients.
- The tail is identical for every colour on a given controller (`03 75 D7 7B EC 08` on one, `03 75 D7 5E 78 08` on another). Replaying a frame with either tail works. It is not a CRC of the payload (constant across payloads); it may be a configuration or current-gain block. It is not a valid SM16825E gain block under the published layout (decodes to impossible currents).

## 4. Channel semantics and Hue's own values

Captured from the genuine controller at 100 % brightness:

| Setting (via Hue) | R | G | B | Warm | Cool | Σ / 65535 |
|---|---|---|---|---|---|---|
| Red (hs 0, 100) | FED9 | 00B1 | 0000 | 01AD | 0000 | 1.01 |
| 2200 K | 3DF8 | 0E7A | 0000 | BF52 | 0000 | 1.11 |
| 4000 K | 0000 | 204F | 0000 | 8562 | 7663 | 1.11 |
| 6500 K | 0000 | 0000 | 030B | 01D7 | FFA4 | 1.05 |

- Channel 4 carries the warm end, channel 5 the cool end.
- Hue mixes a little white into saturated colours (red +0.7 % warm, blue +1.6 % cool) and assists very warm whites with red/green.
- Hue keeps the per-section channel sum near one full channel (≤ ~1.11). Use the same budget when generating colours.
- 16-bit channels give smooth low-end dimming; Hue's dimmest observed unit (Nachtlampje) is ~0.45 % of full scale.

Gradient example (Hue scene "Lily", 7 units):
`a91919a5000001be0000 · c2fb1da100001bb90000 · 38290000028157490000 · …`

## 5. Why earlier captures disagreed

- **"One 8-byte word every 20 ms" (October):** the sniffer used ESP32 legacy RMT RX with one 64-item memory block. The classic ESP32 RMT receiver has no wrap mode, so every capture stopped at exactly 64 bits. Hue's red unit `FED9 00B1 0000 01AD 0000` begins with that "word" `fed900b1000001ad`.
- **"100 identical 8-byte blocks" (fork analyser):** fixed 6448-bit windows appended across frame boundaries.
- **80 × 10 B + tail (September):** correct layout; 80 units because that controller drove a longer strip.
- A correct capture needs a sampler that sees whole frames, e.g. SPI MISO + DMA at 8 MHz (used here).

## 6. Generating the signal

Reference implementation: ESP32, SPI at 20 MHz with DMA, each data bit encoded as 25 SPI bits (19 high + 6 low for `1`, 5 high + 20 low for `0`), so the waveform is exact and unaffected by Wi-Fi or flash activity. ~50 frames per second, continuous refresh.

An interrupt-refilled RMT transmitter works at rest but can stretch bits under load, which shows as flicker.

## 7. Hue bridge observations (CLIP v2)

- The 10 m OmniGlow exposes `gradient.points` (up to 5), `pixel_count: 7`, gradient modes `interpolated_palette`, `interpolated_palette_mirrored`, `random_pixelated`, `segmented_palette`, and effects candle, fire, prism, sparkle, opal, glisten, underwater, cosmos, sunbeam, enchant.
- The 3 m OmniGlow (`929004608001`, firmware 1.163.5, bridge API 1.78.0) is reported **without** any `gradient` object, so its gradient cannot be read through the bridge; only a DATA-line tap reveals it.
- The event stream (`/eventstream/clip/v2`) pushes partial updates within ~100–300 ms.

## 8. Evidence

Captures, firmware and tools: OmniGlow controller project (`firmware/omniglow_esp32`, `tools/capture_hue_scenes.py`, `docs/hue_scenes_keuken.json`). Live checks of 22 Oct 2026: loopback capture of our output (250/1000 ns, 1250 ns slot, bit-exact frame), controller captures above, and owner-confirmed visual tests (red, green, blue, dimming, off, 3-colour split across 8 sections).
