/* drumcircle2midi portable core: no hardware dependencies, builds on host and ESP32. */
#ifndef DC2M_H
#define DC2M_H

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ---------- Onset detector: block energy rise over a slow average ---------- */
typedef struct {
    float avg_db;        /* slow-moving level */
    float threshold_db;  /* rise needed to call an onset */
    int64_t last_onset_us;
    int64_t refractory_us;
    bool primed;
} dc2m_onset_t;

void dc2m_onset_init(dc2m_onset_t *o, float threshold_db, int refractory_ms);
/* Feed one block of mono samples. Returns true if the block contains an onset;
 * *strength_db is how far it rose above the running level. */
bool dc2m_onset_process(dc2m_onset_t *o, const int16_t *pcm, int n, int64_t now_us,
                        float *strength_db);

/* ---------- Tempo tracker: PLL that follows the circle around a nominal tempo ---------- */
typedef struct {
    double period_us;     /* current beat period */
    double next_beat_us;  /* predicted time of the next beat */
    double min_period_us, max_period_us;
    float kp, ki;         /* phase and period gains */
    float confidence;     /* 0..1, how well onsets are landing on the grid */
    bool locked;          /* false until the first onset sets the phase */
} dc2m_tempo_t;

/* max_dev is the allowed drift from nominal, e.g. 0.2 = +/-20%. */
void dc2m_tempo_init(dc2m_tempo_t *t, float nominal_bpm, float max_dev);
void dc2m_tempo_onset(dc2m_tempo_t *t, int64_t onset_us);
float dc2m_tempo_bpm(const dc2m_tempo_t *t);

/* ---------- MIDI clock: 24 ppqn, phase-locked to the tracker's beat ---------- */
#define DC2M_PPQN 24
typedef struct {
    double interval_us;   /* current tick spacing */
    double next_tick_us;
    int tick_in_beat;     /* 0..23 */
    float max_correction; /* per-sync limit on tick spacing change vs period/24 */
    bool running;
} dc2m_clock_t;

void dc2m_clock_init(dc2m_clock_t *c, float bpm);
void dc2m_clock_start(dc2m_clock_t *c, int64_t now_us);
/* Steer tick spacing so the next beat boundary lands on the tracker's prediction. */
void dc2m_clock_sync(dc2m_clock_t *c, const dc2m_tempo_t *t);
/* Call often. Returns true if a tick (0xF8) is due; call again until false.
 * *beat is set when the tick is the first of a beat. */
bool dc2m_clock_poll(dc2m_clock_t *c, int64_t now_us, bool *beat);

/* ---------- Cue detector: facilitator's whistle (placeholder algorithm) ---------- */
typedef struct {
    int sample_rate;
    float min_hz, max_hz;
    float last_hz;
    int stable_blocks, need_blocks;
    bool active;
} dc2m_whistle_t;

void dc2m_whistle_init(dc2m_whistle_t *w, int sample_rate);
/* Returns true once per whistle, when a steady tone in band has lasted long enough. */
bool dc2m_whistle_process(dc2m_whistle_t *w, const int16_t *pcm, int n);

/* ---------- Song: a string of parts ---------- */
typedef enum { DC2M_ADVANCE_AUTO = 0, DC2M_ADVANCE_CUE = 1 } dc2m_advance_t;

typedef struct {
    const char *name;
    uint8_t pattern;        /* pattern number on the FM-1 (sent as Program Change) */
    uint8_t bars;           /* length; AUTO parts advance after this many bars */
    uint8_t beats_per_bar;
    uint8_t repeats;        /* AUTO: times to play before advancing (0 = 1) */
    dc2m_advance_t advance; /* CUE: loop until the facilitator signals */
} dc2m_part_t;

typedef struct {
    const char *name;
    float bpm;
    const dc2m_part_t *parts;
    int n_parts;
} dc2m_song_t;

typedef struct {
    const dc2m_song_t *song;
    int part, beat, bar, repeat;
    bool cue_pending, finished;
} dc2m_player_t;

void dc2m_player_init(dc2m_player_t *p, const dc2m_song_t *song);
void dc2m_player_cue(dc2m_player_t *p); /* facilitator signal heard */
/* Call on every beat. Returns true if the part changed (or the song ended);
 * changes only ever happen on a bar line. */
bool dc2m_player_beat(dc2m_player_t *p);

#ifdef __cplusplus
}
#endif
#endif
