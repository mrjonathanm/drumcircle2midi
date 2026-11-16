# Architecture

```
 drum circle ──mics──▶ LISTENER (ESP32-A1S) ──MIDI clock + part changes──▶ FM-1 (custom firmware) ──▶ PA
                        onset → tempo PLL → clock                          pads, bass, lead, perc, samples
                        whistle / fill → cue → song player
```

## Who owns what

- **Listener** owns time and song position. It tracks the circle, generates MIDI clock, counts
  bars, hears cues, and tells the FM-1 which pattern to play.
- **FM-1** owns sound. Each song is a bank of patterns; it plays the selected one, slaved to
  incoming clock. Stand-alone (no listener) it runs its own clock and steps the song itself.
- **core/** is the shared, hardware-free C library with host tests. All the listening logic
  lives here so it can be developed against recordings on a laptop, not at a session.

## Tempo following

The song gives a nominal tempo. The tracker is a phase-locked loop around it: each detected
onset is compared to the nearest predicted beat or half-beat and nudges phase and period,
clamped to ±20% of nominal. The MIDI clock then steers its tick spacing (max 10% per
correction) so its beat lands on the tracker's. Slow, smooth following suits the parts —
pads and bass tolerate it; nobody hears the synth "hunting".

Known limits of this first version, to fix against real recordings:

- Seeded from the song tempo, so it follows drift but won't find a tempo from cold.
- It can lock onto the off-beat (half-beat ambiguity). Needs downbeat/accent weighting.
- Onset detection is plain broadband energy. A room full of djembes will need band-split
  detection (bass tones carry the pulse) and probably a comb-filter/autocorrelation tempo
  estimate alongside the PLL.

## Cues

- **Whistle**: steady tone, 1.5–4.5 kHz, ~130 ms. Current detector is a zero-crossing
  placeholder that will not survive loud drumming; replace with FFT peak-to-band ratio.
- **Signal fills on djembe**: not started. Plan: record the facilitator's actual calls, then
  try template matching on onset patterns before reaching for a model.
- Changes always land on a bar line at the end of the current part.

## Where AI fits

- **Tempo and whistle**: classic DSP on the ESP32. Deterministic, fast, no network.
- **Fill recognition**: a small on-device classifier (TFLite Micro / ESP-DL on mel frames) is
  realistic on the ESP32 if template matching isn't enough. Needs recordings from real sessions.
- **Cloud / OpenAI-style API**: wrong tool for anything in the live loop (round-trip latency,
  and a hall with no reliable wifi). Good fit offline: turning a described arrangement into
  a song file, or labelling session recordings for training.

## Next steps

1. Record a session (phone is fine) and build `tools/` to run the core over WAV files.
2. Bring up the listener on the A1S: confirm mic input, onset logging over serial, MIDI out.
3. Choose the FM-1 base firmware (see `fm1-firmware/README.md`) and get a clock-slaved
   single pattern playing.
4. Song loading from JSON, song select, stand-alone mode on the FM-1.
