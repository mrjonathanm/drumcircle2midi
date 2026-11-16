# MIDI protocol: listener → FM-1

One direction only. The listener is the clock master and the song brain; the FM-1 firmware
is a clock-slaved groovebox that holds the patterns and sounds.

| Message | Bytes | Meaning |
|---|---|---|
| Clock | `F8` | 24 per beat, phase-locked to the circle |
| Start | `FA` | Song start, pattern position reset |
| Stop | `FC` | Song finished |
| Program Change, ch 16 | `CF pp` | Switch to pattern `pp`. Sent on a bar line; the FM-1 should switch immediately |
| Song Select (planned) | `F3 ss` | Load song `ss`'s pattern bank |

## Physical link

The FM-1 has a 3.5 mm MIDI in, plus USB-C and BLE MIDI. Start with the wire:

- ESP32 UART TX (3.3 V, 31250 baud) → TRS MIDI out, two resistors (10 Ω on the data line,
  33 Ω to 3.3 V). Check whether the FM-1's jack is TRS Type A or B before soldering.
- BLE MIDI from the ESP32 is possible later and removes the cable, but adds jitter to the
  clock. Wire first.

## With no listener attached

The FM-1 firmware runs from its internal clock and steps through the song's parts itself
(auto parts by bar count, cue parts by a button press). The listener only takes over when
external clock arrives.
