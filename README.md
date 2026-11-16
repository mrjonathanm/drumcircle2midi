# drumcircle2midi

A backing band that follows the drum circle instead of the other way round.

A listener module (AI-Thinker ESP32-A1S) hears the circle through its on-board mics, tracks
tempo and bars, and drives an M-VAVE FM-1 running custom groovebox firmware over MIDI clock.
Songs are a string of parts; parts change by bar count or when the facilitator signals with
a whistle or a call on djembe.

## Layout

| Path | What | State |
|---|---|---|
| `core/` | Portable C: onset detection, tempo PLL, MIDI clock, whistle cue, song player | Works, host-tested on synthetic input |
| `listener/` | PlatformIO project for the ESP32-A1S | Written, never built or flashed |
| `fm1-firmware/` | FM-1 groovebox firmware | Not started; base firmware to be chosen |
| `songs/` | Song format (JSON) | Example only; listener still uses compiled-in songs |
| `docs/` | [Architecture](docs/architecture.md), [MIDI protocol](docs/midi-protocol.md) | |

## Run the core tests

```sh
cmake -S core -B build && cmake --build build && ./build/test_core
```

## Build the listener

```sh
cd listener && pio run -t upload && pio device monitor
```
