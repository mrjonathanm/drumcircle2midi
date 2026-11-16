// drumcircle2midi listener: hears the circle, sends MIDI clock + part changes to the FM-1.
// NOT YET BUILT OR FLASHED. The audio board calls follow pschatzmann's arduino-audio-tools
// API from memory; everything else is plain Arduino + the tested portable core.
#include <Arduino.h>

#include "AudioTools.h"
#include "AudioTools/AudioLibs/AudioBoardStream.h"
#include "dc2m.h"
#include "songs.h"

static const int BLOCK = 256; // 16 ms at 16 kHz

static AudioBoardStream board(AudioKitEs8388V1);
static HardwareSerial &midi = Serial1;

static dc2m_onset_t onset;
static dc2m_tempo_t tempo;
static dc2m_clock_t clk;
static dc2m_whistle_t whistle;
static dc2m_player_t player;

static void sendPart() {
  const dc2m_part_t &part = player.song->parts[player.part];
  midi.write(0xCF); // Program Change, channel 16 = part select (see docs/midi-protocol.md)
  midi.write(part.pattern & 0x7F);
  Serial.printf("part -> %s\n", part.name);
}

static void startSong(const dc2m_song_t *song) {
  dc2m_player_init(&player, song);
  dc2m_tempo_init(&tempo, song->bpm, 0.2f);
  dc2m_clock_init(&clk, song->bpm);
  sendPart();
  midi.write(0xFA); // Start
  dc2m_clock_start(&clk, esp_timer_get_time());
}

void setup() {
  Serial.begin(115200);
  midi.begin(31250, SERIAL_8N1, -1, DC2M_MIDI_TX_PIN);

  auto cfg = board.defaultConfig(RX_MODE);
  cfg.input_device = ADC_INPUT_LINE2; // on-board mics on most A1S revisions; try LINE1 if silent
  cfg.sample_rate = DC2M_SAMPLE_RATE;
  cfg.channels = 1;
  cfg.bits_per_sample = 16;
  board.begin(cfg);

  dc2m_onset_init(&onset, 9.0f, 60);
  dc2m_whistle_init(&whistle, DC2M_SAMPLE_RATE);
  startSong(&dc2m_songs[0]);
}

void loop() {
  static int16_t pcm[BLOCK];
  static size_t filled = 0;

  // Non-blocking read so the clock below keeps its timing.
  if (board.available() > 0) {
    filled += board.readBytes((uint8_t *)pcm + filled, sizeof(pcm) - filled);
  }
  if (filled == sizeof(pcm)) {
    filled = 0;
    int64_t now = esp_timer_get_time();
    if (dc2m_onset_process(&onset, pcm, BLOCK, now, nullptr)) {
      dc2m_tempo_onset(&tempo, now);
      dc2m_clock_sync(&clk, &tempo);
    }
    if (dc2m_whistle_process(&whistle, pcm, BLOCK)) {
      dc2m_player_cue(&player);
      Serial.println("cue: whistle");
    }
  }

  bool beat;
  while (dc2m_clock_poll(&clk, esp_timer_get_time(), &beat)) {
    midi.write(0xF8);
    if (beat && dc2m_player_beat(&player)) {
      if (player.finished) {
        midi.write(0xFC); // Stop
        clk.running = false;
      } else {
        sendPart();
      }
    }
  }
}
