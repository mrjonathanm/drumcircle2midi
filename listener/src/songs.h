// Songs compiled in for now. Next step: load songs/*.json from SD or flash (see docs/architecture.md).
#pragma once
#include "dc2m.h"

static const dc2m_part_t welcome_parts[] = {
    // name      pattern bars beats repeats advance
    {"intro",    0,      4,   4,    1,      DC2M_ADVANCE_AUTO},
    {"groove A", 1,      4,   4,    0,      DC2M_ADVANCE_CUE},
    {"break",    2,      2,   4,    1,      DC2M_ADVANCE_AUTO},
    {"groove B", 3,      4,   4,    0,      DC2M_ADVANCE_CUE},
    {"outro",    4,      4,   4,    1,      DC2M_ADVANCE_AUTO},
};

static const dc2m_song_t dc2m_songs[] = {
    {"Welcome Groove", 96.0f, welcome_parts, sizeof(welcome_parts) / sizeof(welcome_parts[0])},
};
