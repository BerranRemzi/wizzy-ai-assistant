#pragma once
#include <stdint.h>
#include <ArduinoJson.h>
#include "config.h"

enum PlaylistSection {
    SEC_OBRASHENIJA = 0,
    SEC_WAKE_UP,
    SEC_SCHOOL,
    SEC_FUN,
    SEC_THREAT,
    SEC_ADVENTURE,
    SEC_SLEEP,
    SEC_EVENING,
    SEC_SYSTEM
};

extern char    g_section_files[PLAYLIST_NUM_SECTIONS][PLAYLIST_MAX_ENTRIES][PLAYLIST_MAX_FNAME];
extern uint8_t g_section_count[PLAYLIST_NUM_SECTIONS];

extern const uint8_t POOL_MORNING[];
extern const uint8_t POOL_DAY[];
extern const uint8_t POOL_NIGHT[];
extern const uint8_t POOL_SURPRISE[];
extern const uint8_t MODE_SECTIONS[];
extern const size_t  POOL_MORNING_LEN;
extern const size_t  POOL_DAY_LEN;
extern const size_t  POOL_NIGHT_LEN;
extern const size_t  POOL_SURPRISE_LEN;
extern const size_t  MODE_SECTIONS_LEN;

bool     playlist_ensure_loaded();
void     playlist_clear_data();
void     playlist_unload_if_loaded();
void     playlist_release_for_playback();
const char* playlist_pick_random_from_sections(const uint8_t *secs, uint8_t n);
