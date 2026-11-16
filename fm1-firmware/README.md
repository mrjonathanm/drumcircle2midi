# FM-1 firmware (not started)

Custom firmware for the M-VAVE FM-1: a clock-slaved groovebox with pad, bass, lead
(flute / panpipe), world percussion and sample tracks. Stereo out, or mono music on one
channel with click on the other.

## Decision needed first: what to build on

The FM-1 runs a JieLi AC791N. Candidates, none evaluated yet:

- [m-vave/fm1_open](https://github.com/m-vave/fm1_open) — M-VAVE's official open firmware.
- [hugelton/Felucca](https://github.com/hugelton/Felucca) — open-source multitimbral
  sequencer firmware; other firmwares (e.g. Hortator) are already built on it. Closest to
  what this needs.

Check for each: licence, build toolchain, whether external MIDI clock sync already works on
the 3.5 mm input, sample playback support, and the route back to stock firmware.

## What it must do

1. Follow external MIDI clock, Start, Stop (see `../docs/midi-protocol.md`).
2. Switch pattern on Program Change, channel 16.
3. Hold several songs, each a bank of patterns.
4. Stand-alone mode: internal clock, steps through parts itself, button for cue parts.
5. Optional click on one output channel.
