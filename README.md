# Philips Hue OmniGlow Lightstrip Protocol — Arduino RGB/CCT control

> **Current, verified specification: [docs/PROTOCOL_V2.md](docs/PROTOCOL_V2.md)** (22 Oct 2026): RGB + warm + cool 16-bit units, one per 12.5 cm section, Hue controller reference values. The "8-byte block" reading in `captures/protocol-analysis.md` came from a 64-bit capture limit and is superseded.

Reverse engineering notes and a small Arduino playground for a Philips Hue OmniGlow lightstrip. The project records a repeatable data waveform observed on one intact strip and provides cautious RGB/CCT experiments for an Arduino Uno.

This is an independent, unofficial project. It is not Philips documentation, and the controller IC, channel naming, electrical thresholds, checksum rules, and general product compatibility are still unconfirmed.

## What is currently known

- An Arduino Uno class board running an ATmega328P at 16 MHz can reproduce the measured waveform on D2.
- A frame contains 806 transmitted bytes: 800 bytes of repeating payload followed by a six-byte footer.
- The working payload model is 80 repeated units of ten bytes, with five 16-bit values per unit, sent most significant byte first. This grouping is an inference from captures and successful tests, not a confirmed Philips data structure.
- Captures indicate approximately800kbit/s. The accepted replay uses exactly20CPU cycles per symbol (1.25µs), with short/long high durations of4/15cycles (250/937.5ns). These exact values describe the generated waveform, not a calibrated measurement of the original transmitter.
- The first 6,400 symbols repeat an 80-symbol pattern 80 times. A distinct 48-symbol footer follows at approximately 8.00 ms. The footer bytes observed in the working representation are `03 75 D7 5E 78 08`.
- Full red, full blue, half red/half blue, and the reversed half-and-half arrangement have been user-confirmed on the tested strip.

The capture alignment leaves the first symbol inferred. The short/long pulse labels are a convenient `0`/`1` convention; they are not a decoded protocol definition.

## Hardware boundary

Keep the original Hue controller DATA connection physically isolated before the Arduino drives the strip. Keep strip power and Arduino ground common only where the wiring has been verified. The strip supply in the test setup is nominally 24 V; never put that supply on an Arduino signal pin.

Use Arduino D2 for DATA and a verified common GND. Wire colours are not a general pinout; identify supply, ground and data on your own hardware.

D2 is `INPUT` at boot, does not autostart, and returns to `INPUT` after a stop or completed effect. Use only a known compatible electrical interface for the DATA level; the strip threshold and any required level shifting have not been characterized here.

## Firmware and commands

The current playground firmware is [firmware/omniglow_playground/omniglow_playground.ino](firmware/omniglow_playground/omniglow_playground.ino). It targets an ATmega328P Uno at 16 MHz and opens serial control at 115200 baud.

- `D` — run bounded smooth random colour transitions.
- `W` — run the white-channel experiment.
- `P` — run the eight-effect playground.
- `F` — run the six-act particle-fireworks show: rockets, blossoms, crossing trails, golden willows, rainbow bursts and finale.
- `X` — stop between frames and return D2 to `INPUT`.

Send one command at a time after checking the wiring. Do not connect the original controller DATA line while using Arduino output.

## Documents

- [Measured and inferred protocol record](docs/PROTOCOL.md)
- [Nederlandse handleiding](docs/HANDLEIDING.md)
- [Complete protocol and full current software in one document](docs/COMPLETE_PROTOCOL_AND_SOFTWARE.md)

See the [red captures](evidence/red-aligned-captures.json), [blue captures](evidence/blue-aligned-captures.json), and [compiled timing checks](evidence/waveform-verification.json). The protocol record distinguishes captured observations, reconstruction assumptions and user-confirmed behavior.

## Next verification step

Treat this as a strip-specific working model. A future ESP32-C6 Zigbee prototype would translate Hue commands into this strip waveform. Pairing with a real Hue Bridge must be tested before building two independently controlled devices. Zigbee firmware and Hue pairing are not implemented in this repository.

## Build, validate and run

The shipped sketch and HEX target only an ATmega328P Uno at16MHz. Keep the original Hue DATA isolated. Opening the serial port normally resets the Uno; no output starts until a demo command is received.

```sh
python -m pip install -r requirements.txt
python tools/build.py --avr-bin /path/to/avr/bin --arduino-core /path/to/arduino/avr/1.8.8
python tools/verify_waveform.py
```

The build helper expects native `avr-gcc`, `avr-g++`, `avr-objcopy`, `avr-objdump`, and `avr-size`, plus the Arduino AVR core (`cores/` and `variants/`). It produces `build/omniglow_playground.hex`. The instruction checker is narrowly written for this serializer; it is not a full AVR emulator.

Upload using an installed AVRDUDE and your verified serial port, for example:

```sh
avrdude -p atmega328p -c arduino -P YOUR_PORT -b115200 -D -U flash:w:build/omniglow_playground.hex:i
python tools/control.py --port YOUR_PORT --command P
```

You can also upload the included [tested HEX](firmware/omniglow_playground/omniglow_playground.hex). Use the AVRDUDE configuration appropriate to your installation. Upload replaces the current sketch. A previous sketch can be preserved with a read-only flash backup before replacement. Only one program should own the serial port. Ctrl-C in the control tool requests a stop.

## Effects

`P` plays aurora curtains, twin comet dance, pond ripples, bouncing marbles, candy conveyor, firefly garden, sunrise wave and liquid plasma, then fades to a candidate warm-white channel. `W` combines white-channel transitions, a moving rainbow, a soft comet and candle glow. `D` gives independent smooth colors to the two40-unit halves. These are local demonstrations, not Hue-app effects or Hue Bridge integration.

## Frequently asked questions

**Is the OmniGlow protocol WS2812 or WS2811?** This project does not identify the driver IC or claim compatibility with either protocol. It documents an accepted waveform and packet reconstruction from one OmniGlow setup.

**Can a cut-off section become a second Hue lamp?** Not established here. Separate color zones on the intact strip work; electrical access to a cut section and independent Zigbee pairing remain untested.

**Does this require a laptop permanently?** The effects execute on the Arduino. This test firmware requires a serial start command after boot. A standalone startup policy and wireless control would be separate work.

The `F` fireworks show uses24 simulated particles with sub-unit motion, local additive light and brightness normalization. It finishes with a warm afterglow. Run it with `python tools/control.py --port YOUR_PORT --command F`.

## Play every show in sequence

```sh
python tools/play_all.py --port YOUR_PORT
```

This keeps one serial connection open and runs `D → W → P → F`, advancing only after each completion acknowledgement. Expect about three minutes. Ctrl-C cancels the playlist; alternatively create the file supplied by `--stop-file`. The [latest completed playlist log](evidence/all-shows-run.json) records every show's acknowledgement and duration.

## Capture tools

The separate [read-only acquisition sketch](firmware/omniglow_capture/omniglow_capture.ino) and [capture HEX](firmware/omniglow_capture/omniglow_capture.hex) preserve the measurement firmware used in this investigation. It replaces the effects sketch if uploaded. Its D2 pin only listens. It requires the original controller as the signal source and verified electrical levels; it does not drive the strip.

```sh
python tools/read_hue_capture.py --port YOUR_PORT --trigger --count 40 --output captures.json
python tools/read_hue_aligned.py --port YOUR_PORT --output aligned.json
```

Only use these readers with the capture sketch, not the effects firmware. The aligned reader scans offsets through multiple separate transmissions; keep the controller color and brightness fixed. The supplied80-unit model is specific to the tested strip.
