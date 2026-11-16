#include "dc2m.h"

#include <math.h>
#include <string.h>

/* ---------------- onset ---------------- */

static float block_db(const int16_t *pcm, int n) {
    double sum = 0;
    for (int i = 0; i < n; i++) sum += (double)pcm[i] * pcm[i];
    return 10.0f * log10f((float)(sum / (n > 0 ? n : 1)) + 1.0f);
}

void dc2m_onset_init(dc2m_onset_t *o, float threshold_db, int refractory_ms) {
    memset(o, 0, sizeof *o);
    o->threshold_db = threshold_db;
    o->refractory_us = (int64_t)refractory_ms * 1000;
}

bool dc2m_onset_process(dc2m_onset_t *o, const int16_t *pcm, int n, int64_t now_us,
                        float *strength_db) {
    float db = block_db(pcm, n);
    if (!o->primed) {
        o->avg_db = db;
        o->primed = true;
        o->last_onset_us = now_us - o->refractory_us;
        return false;
    }
    float rise = db - o->avg_db;
    o->avg_db += 0.1f * (db - o->avg_db);
    if (rise > o->threshold_db && now_us - o->last_onset_us >= o->refractory_us) {
        o->last_onset_us = now_us;
        if (strength_db) *strength_db = rise;
        return true;
    }
    return false;
}

/* ---------------- tempo ---------------- */

void dc2m_tempo_init(dc2m_tempo_t *t, float nominal_bpm, float max_dev) {
    memset(t, 0, sizeof *t);
    t->period_us = 60e6 / nominal_bpm;
    t->min_period_us = t->period_us * (1.0 - max_dev);
    t->max_period_us = t->period_us * (1.0 + max_dev);
    t->kp = 0.25f;
    t->ki = 0.05f;
}

void dc2m_tempo_onset(dc2m_tempo_t *t, int64_t onset_us) {
    double now = (double)onset_us;
    if (!t->locked) {
        t->next_beat_us = now + t->period_us;
        t->locked = true;
        return;
    }
    /* Skip over beats nobody played. */
    while (now > t->next_beat_us + t->period_us / 2) t->next_beat_us += t->period_us;

    /* Error against the nearest beat or half-beat, so off-beat hits still steer. */
    double e = now - t->next_beat_us;
    double quarter = t->period_us / 4;
    if (e > quarter) e -= t->period_us / 2;
    else if (e < -quarter) e += t->period_us / 2;

    t->next_beat_us += t->kp * e;
    t->period_us += t->ki * e;
    if (t->period_us < t->min_period_us) t->period_us = t->min_period_us;
    if (t->period_us > t->max_period_us) t->period_us = t->max_period_us;

    float fit = 1.0f - (float)(fabs(e) / quarter);
    t->confidence += 0.1f * (fit - t->confidence);
}

float dc2m_tempo_bpm(const dc2m_tempo_t *t) { return (float)(60e6 / t->period_us); }

/* ---------------- clock ---------------- */

void dc2m_clock_init(dc2m_clock_t *c, float bpm) {
    memset(c, 0, sizeof *c);
    c->interval_us = 60e6 / bpm / DC2M_PPQN;
    c->max_correction = 0.10f;
}

void dc2m_clock_start(dc2m_clock_t *c, int64_t now_us) {
    c->next_tick_us = (double)now_us;
    c->tick_in_beat = 0;
    c->running = true;
}

void dc2m_clock_sync(dc2m_clock_t *c, const dc2m_tempo_t *t) {
    if (!c->running || !t->locked) return;
    double base = t->period_us / DC2M_PPQN;
    /* Ticks left until our next beat boundary, and when the circle's beat lands. */
    int remaining = (DC2M_PPQN - c->tick_in_beat) % DC2M_PPQN;
    double target = t->next_beat_us;
    double ours = c->next_tick_us + remaining * base;
    /* Pick the circle beat nearest to where ours would fall. */
    while (target < ours - t->period_us / 2) target += t->period_us;
    while (target > ours + t->period_us / 2) target -= t->period_us;
    if (remaining == 0) remaining = DC2M_PPQN, target += (target <= c->next_tick_us) ? t->period_us : 0;
    double want = (target - c->next_tick_us) / remaining;
    double lo = base * (1.0 - c->max_correction), hi = base * (1.0 + c->max_correction);
    c->interval_us = want < lo ? lo : want > hi ? hi : want;
}

bool dc2m_clock_poll(dc2m_clock_t *c, int64_t now_us, bool *beat) {
    if (!c->running || (double)now_us < c->next_tick_us) return false;
    if (beat) *beat = (c->tick_in_beat == 0);
    c->tick_in_beat = (c->tick_in_beat + 1) % DC2M_PPQN;
    c->next_tick_us += c->interval_us;
    return true;
}

/* ---------------- whistle (placeholder: steady zero-crossing pitch) ---------------- */

void dc2m_whistle_init(dc2m_whistle_t *w, int sample_rate) {
    memset(w, 0, sizeof *w);
    w->sample_rate = sample_rate;
    w->min_hz = 1500;
    w->max_hz = 4500;
    w->need_blocks = 8;
}

bool dc2m_whistle_process(dc2m_whistle_t *w, const int16_t *pcm, int n) {
    int crossings = 0;
    for (int i = 1; i < n; i++)
        if ((pcm[i - 1] < 0) != (pcm[i] < 0)) crossings++;
    float hz = (float)crossings * w->sample_rate / (2.0f * n);
    bool loud = block_db(pcm, n) > 50.0f;
    bool in_band = hz >= w->min_hz && hz <= w->max_hz;
    bool steady = fabsf(hz - w->last_hz) < 0.08f * hz;
    w->last_hz = hz;
    if (loud && in_band && steady) {
        if (++w->stable_blocks >= w->need_blocks && !w->active) {
            w->active = true;
            return true;
        }
    } else {
        w->stable_blocks = 0;
        w->active = false;
    }
    return false;
}

/* ---------------- song player ---------------- */

void dc2m_player_init(dc2m_player_t *p, const dc2m_song_t *song) {
    memset(p, 0, sizeof *p);
    p->song = song;
    p->finished = song->n_parts == 0;
}

void dc2m_player_cue(dc2m_player_t *p) { p->cue_pending = true; }

bool dc2m_player_beat(dc2m_player_t *p) {
    if (p->finished) return false;
    const dc2m_part_t *part = &p->song->parts[p->part];
    if (++p->beat < part->beats_per_bar) return false;
    p->beat = 0;
    if (++p->bar < part->bars) return false;
    p->bar = 0;

    bool advance;
    if (part->advance == DC2M_ADVANCE_CUE) {
        advance = p->cue_pending;
    } else {
        int repeats = part->repeats ? part->repeats : 1;
        advance = ++p->repeat >= repeats;
    }
    if (!advance) return false;

    p->cue_pending = false;
    p->repeat = 0;
    if (++p->part >= p->song->n_parts) {
        p->part = p->song->n_parts - 1;
        p->finished = true;
    }
    return true;
}
