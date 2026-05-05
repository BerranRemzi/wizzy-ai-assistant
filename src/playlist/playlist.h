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

static const uint8_t PLAYLIST_MAX_BUTTON_SEQUENCE_STEPS = 6;
extern uint8_t g_button_sequence_step_count;
extern uint8_t g_button_sequence_section_count[PLAYLIST_MAX_BUTTON_SEQUENCE_STEPS];
extern uint8_t g_button_sequence_sections[PLAYLIST_MAX_BUTTON_SEQUENCE_STEPS][PLAYLIST_NUM_SECTIONS];

extern char    g_clock_time_files[24][PLAYLIST_MAX_ENTRIES][PLAYLIST_MAX_FNAME];
extern uint8_t g_clock_time_count[24];
extern uint8_t g_clock_compose_count[24];
extern uint8_t g_clock_compose_sections[24][PLAYLIST_NUM_SECTIONS];

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
bool     playlist_pick_button_sequence_files(char out_files[][PLAYLIST_MAX_FNAME], uint8_t max_files, uint8_t *out_count);
const char* playlist_pick_random_clock_time(uint8_t hour);
const char* playlist_pick_random_clock_compose(uint8_t hour);
