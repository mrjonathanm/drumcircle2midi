/* Host tests for the portable core. Build: cmake -S core -B build && cmake --build build && ./build/test_core */
#include <math.h>
#include <stdio.h>
#include <stdlib.h>

#include "dc2m.h"

static int failures;
#define CHECK(cond, ...) \
    do { if (!(cond)) { failures++; printf("FAIL %s:%d: ", __FILE__, __LINE__); printf(__VA_ARGS__); printf("\n"); } } while (0)

static double jitter(double ms) { return ((rand() % 2001) / 1000.0 - 1.0) * ms * 1000.0; }

/* Circle starts at the song's 120 bpm then settles at a different tempo, with sloppy timing. */
static void test_tempo_follows_drift(float actual_bpm) {
    dc2m_tempo_t t;
    dc2m_tempo_init(&t, 120, 0.2f);
    double period = 60e6 / actual_bpm, now = 1e6;
    for (int i = 0; i < 96; i++) {
        dc2m_tempo_onset(&t, (int64_t)(now + jitter(15)));
        if (i % 3 == 0) dc2m_tempo_onset(&t, (int64_t)(now + period / 2 + jitter(15))); /* off-beats */
        now += period;
    }
    float bpm = dc2m_tempo_bpm(&t);
    CHECK(fabsf(bpm - actual_bpm) < 1.5f, "tracking %.1f bpm, got %.2f", actual_bpm, bpm);
    CHECK(t.confidence > 0.6f, "confidence %.2f at %.1f bpm", t.confidence, actual_bpm);
}

/* Clock driven by the tracker should end up ticking on the circle's beat. */
static void test_clock_phase_locks(void) {
    dc2m_tempo_t t;
    dc2m_clock_t c;
    dc2m_tempo_init(&t, 100, 0.2f);
    dc2m_clock_init(&c, 100);
    double period = 60e6 / 104.0, next_hit = 1.2e6, last_beat = 0, last_hit = 0;
    int ticks = 0;
    dc2m_clock_start(&c, 1000000);
    for (int64_t now = 1000000; now < 61000000; now += 500) {
        if (now >= next_hit) {
            dc2m_tempo_onset(&t, now);
            dc2m_clock_sync(&c, &t);
            last_hit = next_hit;
            next_hit += period;
        }
        bool beat;
        while (dc2m_clock_poll(&c, now, &beat)) {
            ticks++;
            if (beat) last_beat = (double)now;
        }
    }
    double err = fmod(fabs(last_beat - last_hit), period);
    if (err > period / 2) err = period - err;
    CHECK(err < 15000, "clock beat is %.1f ms off the circle's beat", err / 1000);
    CHECK(abs(ticks - 104 * 24) < 30, "expected ~%d ticks in a minute, got %d", 104 * 24, ticks);
}

static void test_player(void) {
    static const dc2m_part_t parts[] = {
        {"intro", 0, 2, 4, 1, DC2M_ADVANCE_AUTO},
        {"groove", 1, 4, 4, 0, DC2M_ADVANCE_CUE},
        {"outro", 2, 2, 4, 1, DC2M_ADVANCE_AUTO},
    };
    static const dc2m_song_t song = {"test", 100, parts, 3};
    dc2m_player_t p;
    dc2m_player_init(&p, &song);

    int beats = 0;
    while (!dc2m_player_beat(&p)) beats++;
    CHECK(beats + 1 == 8 && p.part == 1, "intro should last 8 beats, lasted %d", beats + 1);

    for (int i = 0; i < 40; i++) CHECK(!dc2m_player_beat(&p), "groove must loop until cued");
    CHECK(p.part == 1, "still in groove");

    for (int i = 0; i < 3; i++) dc2m_player_beat(&p); /* cue arrives mid-bar... */
    dc2m_player_cue(&p);
    beats = 0;
    while (!dc2m_player_beat(&p)) beats++;
    CHECK(p.part == 2 && p.beat == 0 && p.bar == 0, "...and the change waits for the end of the 4-bar part");
    CHECK(beats + 1 == 5, "expected change 5 beats after cue, got %d", beats + 1);

    beats = 0;
    while (!dc2m_player_beat(&p)) beats++;
    CHECK(p.finished, "song should finish after outro");
}

static void test_whistle(void) {
    enum { SR = 16000, N = 256 };
    int16_t buf[N];
    dc2m_whistle_t w;
    dc2m_whistle_init(&w, SR);
    int fired = 0;
    for (int b = 0; b < 40; b++) { /* noise: must not fire */
        for (int i = 0; i < N; i++) buf[i] = (int16_t)(rand() % 8000 - 4000);
        fired += dc2m_whistle_process(&w, buf, N);
    }
    CHECK(fired == 0, "noise triggered the whistle detector %d times", fired);
    for (int b = 0; b < 40; b++) { /* 2.8 kHz tone: fires exactly once */
        for (int i = 0; i < N; i++) buf[i] = (int16_t)(8000 * sin(2 * M_PI * 2800 * (b * N + i) / SR));
        fired += dc2m_whistle_process(&w, buf, N);
    }
    CHECK(fired == 1, "whistle should fire once, fired %d", fired);
}

static void test_onset(void) {
    enum { N = 256 };
    int16_t quiet[N], loud[N];
    for (int i = 0; i < N; i++) quiet[i] = (int16_t)(rand() % 200 - 100), loud[i] = (int16_t)(rand() % 20000 - 10000);
    dc2m_onset_t o;
    dc2m_onset_init(&o, 9, 60);
    int onsets = 0;
    for (int b = 0; b < 200; b++) /* 16 ms blocks, a hit every 25 blocks */
        onsets += dc2m_onset_process(&o, b % 25 == 10 ? loud : quiet, N, (int64_t)b * 16000, NULL);
    CHECK(onsets == 8, "expected 8 onsets, got %d", onsets);
}

int main(void) {
    srand(1);
    test_tempo_follows_drift(112);
    test_tempo_follows_drift(120);
    test_tempo_follows_drift(131);
    test_clock_phase_locks();
    test_player();
    test_whistle();
    test_onset();
    printf(failures ? "%d FAILED\n" : "all passed\n", failures);
    return failures != 0;
}
